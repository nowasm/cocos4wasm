#include "ScriptEngine.h"
#include "Class.h"
#include "Object.h"
#include "Utils.h"
#include "../MappingUtils.h"
#include "base/memory/Memory.h"

#include <emscripten.h>
#include <emscripten/val.h>

#if SCRIPT_ENGINE_TYPE == SCRIPT_ENGINE_BROWSER

namespace se {

ScriptEngine *ScriptEngine::_instance = nullptr;
ScriptEngine::DebuggerInfo ScriptEngine::sDebuggerInfo;

ScriptEngine *ScriptEngine::getInstance() {
    if (_instance == nullptr) {
        _instance = ccnew ScriptEngine();
    }
    return _instance;
}

void ScriptEngine::destroyInstance() {
    delete _instance;
    _instance = nullptr;
}

ScriptEngine::ScriptEngine() {
    _startTime = std::chrono::steady_clock::now();
}

ScriptEngine::~ScriptEngine() {
    cleanup();
}

bool ScriptEngine::init() {
    for (auto &hook : _beforeInitHookArray) hook();
    _beforeInitHookArray.clear();

    NativePtrToObjectMap::init();

    // globalThis IS the browser's global object
    _globalObj = Object::_createJSObject(nullptr, emscripten::val::global("globalThis"));
    if (_globalObj != nullptr) {
        _globalObj->incRef();
        _globalObj->root();
        // Ensure window === globalThis
        _globalObj->setProperty("window", se::Value(_globalObj));
    }

    _isValid = true;

    // Run register callbacks
    for (auto cb : _permanentRegisterCallbackArray) {
        cb(_globalObj);
    }
    for (auto cb : _registerCallbackArray) {
        cb(_globalObj);
    }
    _registerCallbackArray.clear();

    for (auto &hook : _afterInitHookArray) hook();
    _afterInitHookArray.clear();

    return true;
}

bool ScriptEngine::start() {
    return init();
}

void ScriptEngine::cleanup() {
    if (!_isValid) return;
    _isInCleanup = true;

    for (auto &hook : _beforeCleanupHookArray) hook();
    _beforeCleanupHookArray.clear();

    Class::cleanup();

    if (_globalObj != nullptr) {
        _globalObj->unroot();
        _globalObj->decRef();
        _globalObj = nullptr;
    }

    NativePtrToObjectMap::destroy();

    for (auto &hook : _afterCleanupHookArray) hook();
    _afterCleanupHookArray.clear();

    _isValid = false;
    _isInCleanup = false;
}

Object *ScriptEngine::getGlobalObject() const {
    return _globalObj;
}

void ScriptEngine::addRegisterCallback(RegisterCallback cb) {
    _registerCallbackArray.push_back(cb);
}

void ScriptEngine::addPermanentRegisterCallback(RegisterCallback cb) {
    _permanentRegisterCallbackArray.push_back(cb);
}

void ScriptEngine::addBeforeInitHook(const std::function<void()> &hook) { _beforeInitHookArray.push_back(hook); }
void ScriptEngine::addAfterInitHook(const std::function<void()> &hook) { _afterInitHookArray.push_back(hook); }
void ScriptEngine::addBeforeCleanupHook(const std::function<void()> &hook) { _beforeCleanupHookArray.push_back(hook); }
void ScriptEngine::addAfterCleanupHook(const std::function<void()> &hook) { _afterCleanupHookArray.push_back(hook); }

bool ScriptEngine::evalString(const char *script, uint32_t length, Value *ret, const char *fileName) {
    if (script == nullptr) return false;

    std::string code(script, length > 0 ? length : strlen(script));
    std::string fn(fileName ? fileName : "<eval>");

    emscripten::val result = emscripten::val::undefined();
    bool ok = true;

    // Use EM_ASM to evaluate with try/catch in browser JS
    // We pass the code as a string and evaluate it
    EM_ASM({
        try {
            var code = UTF8ToString($0);
            var result = eval(code);
            // Store result for C++ to pick up
            Module.__browser_se_eval_result = result;
            Module.__browser_se_eval_ok = true;
        } catch(e) {
            Module.__browser_se_eval_ok = false;
            Module.__browser_se_eval_error = e.message || String(e);
            Module.__browser_se_eval_stack = e.stack || '';
            console.error('ScriptEngine::evalString exception:', e.message, '\nstack:', e.stack);
        }
    }, code.c_str());

    // Check result
    emscripten::val module = emscripten::val::module_property("__browser_se_eval_ok");
    ok = module.as<bool>();

    if (ok && ret != nullptr) {
        emscripten::val evalResult = emscripten::val::module_property("__browser_se_eval_result");
        internal::jsToSeValue(evalResult, ret);
    }

    if (!ok) {
        emscripten::val errMsg = emscripten::val::module_property("__browser_se_eval_error");
        emscripten::val errStack = emscripten::val::module_property("__browser_se_eval_stack");
        std::string msg = errMsg.as<std::string>();
        std::string stack = errStack.as<std::string>();

        if (_exceptionCallback) {
            _exceptionCallback(fn.c_str(), msg.c_str(), stack.c_str());
        }
        _lastException = std::make_tuple(fn, msg, stack);
    }

    return ok;
}

bool ScriptEngine::runScript(const ccstd::string &path, Value *ret) {
    const auto &delegate = _fileOperationDelegate;
    if (!delegate.isValid()) return false;

    ccstd::string scriptContent = delegate.onGetStringFromFile(path);
    if (scriptContent.empty()) {
        SE_LOGE("ScriptEngine::runScript: file not found or empty: %s\n", path.c_str());
        return false;
    }

    return evalString(scriptContent.c_str(), static_cast<uint32_t>(scriptContent.size()), ret, path.c_str());
}

bool ScriptEngine::callFunction(Object *targetObj, const char *funcName, uint32_t argc, Value *args, Value *rval) {
    if (targetObj == nullptr) return false;

    emscripten::val target = targetObj->_getJSObject();
    emscripten::val func = target[funcName];

    if (func.typeOf().as<std::string>() != "function") {
        return false;
    }

    // Build args array
    emscripten::val argsArray = emscripten::val::array();
    for (uint32_t i = 0; i < argc; ++i) {
        argsArray.call<void>("push", internal::seToJsValue(args[i]));
    }

    emscripten::val result = func.call<emscripten::val>("apply", target, argsArray);

    if (rval != nullptr) {
        internal::jsToSeValue(result, rval);
    }
    return true;
}

ccstd::string ScriptEngine::getCurrentStackTrace() {
    return EM_ASM_INT({
        try { throw new Error(); } catch(e) { return e.stack || ''; }
    }) ? "" : "";
}

bool ScriptEngine::saveByteCodeToFile(const ccstd::string & /*path*/, const ccstd::string & /*pathBc*/) {
    return false; // Not supported in browser
}

void ScriptEngine::setFileOperationDelegate(const FileOperationDelegate &delegate) {
    _fileOperationDelegate = delegate;
}

const ScriptEngine::FileOperationDelegate &ScriptEngine::getFileOperationDelegate() const {
    return _fileOperationDelegate;
}

bool ScriptEngine::FileOperationDelegate::isValid() const {
    return onGetStringFromFile != nullptr;
}

bool ScriptEngine::isGarbageCollecting() const { return _isGarbageCollecting; }
bool ScriptEngine::isInCleanup() const { return _isInCleanup; }
bool ScriptEngine::isValid() const { return _isValid; }

void ScriptEngine::clearException() {
    _lastException = ExceptionInfo();
}

void ScriptEngine::throwException(const ccstd::string &errorMessage) {
    EM_ASM({
        throw new Error(UTF8ToString($0));
    }, errorMessage.c_str());
}

const ScriptEngine::ExceptionInfo &ScriptEngine::getLastException() const {
    return _lastException;
}

void ScriptEngine::setExceptionCallback(const ExceptionCallback &cb) { _exceptionCallback = cb; }
void ScriptEngine::setJSExceptionCallback(const ExceptionCallback &cb) { _jsExceptionCallback = cb; }

void ScriptEngine::handlePromiseExceptions() {
    // Browser handles this natively
}

void ScriptEngine::enableDebugger(const ccstd::string & /*serverAddr*/, uint32_t /*port*/, bool /*isWait*/) {
    // Browser has built-in DevTools
}

bool ScriptEngine::isDebuggerEnabled() const { return false; }

void ScriptEngine::mainLoopUpdate() {
    // Browser handles microtasks/promises natively — nothing to do
}

void ScriptEngine::garbageCollect() {
    // Cannot trigger GC in browser
}

const std::chrono::steady_clock::time_point &ScriptEngine::getStartTime() const {
    return _startTime;
}

uint32_t ScriptEngine::getVMId() const { return 0; }

void ScriptEngine::_setGarbageCollecting(bool v) { _isGarbageCollecting = v; }
void ScriptEngine::_setDebuggerInfo(const DebuggerInfo &info) { sDebuggerInfo = info; }

} // namespace se

#endif
