#include "ScriptEngine.h"
#include "Object.h"
#include "Class.h"
#include "Utils.h"
#include "../MappingUtils.h"
#include "base/Macros.h"
#include "engine/EngineEvents.h"

#if USE_PLUGINS
    #include "plugins/bus/EventBus.h"
    #include "plugins/bus/BusTypes.h"
#endif

namespace se {

namespace {
enum class ConsoleLogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR,
};

void logConsoleMessage(ConsoleLogLevel level, const char *prefix, const std::string &msg) {
    switch (level) {
        case ConsoleLogLevel::DEBUG:
            CC_LOG_INFO("JS: %s%s", prefix, msg.c_str());
            break;
        case ConsoleLogLevel::INFO:
            CC_LOG_INFO("JS: %s%s", prefix, msg.c_str());
            break;
        case ConsoleLogLevel::WARN:
            CC_LOG_WARNING("JS: %s%s", prefix, msg.c_str());
            break;
        case ConsoleLogLevel::ERROR:
            SE_LOGE("JS: %s%s\n", prefix, msg.c_str());
            break;
    }
}

bool JSB_console_format_log(State &s, ConsoleLogLevel level, const char *prefix, int msgIndex = 0) {
    if (msgIndex < 0) {
        return false;
    }

    const auto &args = s.args();
    const int argc = static_cast<int>(args.size());
    if ((argc - msgIndex) == 1) {
        const std::string msg = args[msgIndex].toStringForce();
        logConsoleMessage(level, prefix, msg);
    } else if ((argc - msgIndex) > 1) {
        std::string msg = args[msgIndex].toStringForce();
        for (int i = msgIndex + 1; i < argc; ++i) {
            const size_t pos = msg.find("%");
            if (pos != std::string::npos && pos != (msg.length() - 1) &&
                (msg[pos + 1] == 'd' || msg[pos + 1] == 's' || msg[pos + 1] == 'f')) {
                msg.replace(pos, 2, args[i].toStringForce());
            } else {
                msg += " " + args[i].toStringForce();
            }
        }
        logConsoleMessage(level, prefix, msg);
    }

    return true;
}

bool JSB_console_log(State &s) {
    JSB_console_format_log(s, ConsoleLogLevel::DEBUG, "");
    return true;
}
SE_BIND_FUNC(JSB_console_log)

bool JSB_console_debug(State &s) {
    JSB_console_format_log(s, ConsoleLogLevel::DEBUG, "[DEBUG]: ");
    return true;
}
SE_BIND_FUNC(JSB_console_debug)

bool JSB_console_info(State &s) {
    JSB_console_format_log(s, ConsoleLogLevel::INFO, "[INFO]: ");
    return true;
}
SE_BIND_FUNC(JSB_console_info)

bool JSB_console_warn(State &s) {
    JSB_console_format_log(s, ConsoleLogLevel::WARN, "[WARN]: ");
    return true;
}
SE_BIND_FUNC(JSB_console_warn)

bool JSB_console_error(State &s) {
    JSB_console_format_log(s, ConsoleLogLevel::ERROR, "[ERROR]: ");
    return true;
}
SE_BIND_FUNC(JSB_console_error)

bool JSB_console_assert(State &s) {
    const auto &args = s.args();
    if (!args.empty() && args[0].isBoolean() && !args[0].toBoolean()) {
        JSB_console_format_log(s, ConsoleLogLevel::ERROR, "[ASSERT]: ", 1);
    }
    return true;
}
SE_BIND_FUNC(JSB_console_assert)

bool JSB_console_time(State &s) {
    CC_UNUSED_PARAM(s);
    return true;
}
SE_BIND_FUNC(JSB_console_time)

bool JSB_console_timeEnd(State &s) {
    CC_UNUSED_PARAM(s);
    return true;
}
SE_BIND_FUNC(JSB_console_timeEnd)

} // namespace

ScriptEngine *ScriptEngine::sInstance = nullptr;
ScriptEngine::DebuggerInfo ScriptEngine::sDebuggerInfo;

ScriptEngine *ScriptEngine::getInstance() {
    return sInstance;
}

void ScriptEngine::destroyInstance() {
    if (sInstance != nullptr) {
        delete sInstance;
    }
}

ScriptEngine::ScriptEngine() {
    sInstance = this;
}

ScriptEngine::~ScriptEngine() {
    cleanup();
    if (sInstance == this) {
        sInstance = nullptr;
    }
}

Object *ScriptEngine::getGlobalObject() const {
    return _globalObj;
}

void ScriptEngine::addRegisterCallback(RegisterCallback cb) {
    if (cb == nullptr) return;
    _registerCallbackArray.push_back(cb);
}

void ScriptEngine::addPermanentRegisterCallback(RegisterCallback cb) {
    if (cb == nullptr) return;
    _permRegisterCallbackArray.push_back(cb);
}

bool ScriptEngine::callRegisteredCallback() {
    if (_globalObj == nullptr) return false;
    for (auto cb : _permRegisterCallbackArray) {
        if (!cb(_globalObj)) return false;
    }
    for (auto cb : _registerCallbackArray) {
        if (!cb(_globalObj)) return false;
    }
    _registerCallbackArray.clear();
    return true;
}

bool ScriptEngine::init() {
    cleanup();
    ++_vmId;

    cc::events::ScriptEngine::broadcast(cc::ScriptEngineEvent::BEFORE_INIT);

    for (const auto &hook : _beforeInitHookArray) { hook(); }
    _beforeInitHookArray.clear();

    _rt = JS_NewRuntime();
    _ctx = JS_NewContext(_rt);

    NativePtrToObjectMap::init();

    JSValue globalVal = JS_GetGlobalObject(_ctx);
    _globalObj = Object::_createJSObject(nullptr, globalVal);
    if (_globalObj != nullptr) {
        _globalObj->incRef();
        _globalObj->root();
        _globalObj->setProperty("window", se::Value(_globalObj));

        se::Value consoleVal;
        const bool hasConsole = _globalObj->getProperty("console", &consoleVal) && consoleVal.isObject();
        if (!hasConsole) {
            const HandleObject consoleObj(Object::createPlainObject());
            consoleObj->defineFunction("log", _SE(JSB_console_log));
            consoleObj->defineFunction("debug", _SE(JSB_console_debug));
            consoleObj->defineFunction("info", _SE(JSB_console_info));
            consoleObj->defineFunction("warn", _SE(JSB_console_warn));
            consoleObj->defineFunction("error", _SE(JSB_console_error));
            consoleObj->defineFunction("assert", _SE(JSB_console_assert));
            consoleObj->defineFunction("time", _SE(JSB_console_time));
            consoleObj->defineFunction("timeEnd", _SE(JSB_console_timeEnd));
            _globalObj->setProperty("console", se::Value(consoleObj));
        }

        _globalObj->setProperty("scriptEngineType", se::Value(ccstd::string("QuickJS")));
    }

    _isValid = (_globalObj != nullptr);

#if USE_PLUGINS
    cc::plugin::send(cc::plugin::BusType::SCRIPT_ENGINE, cc::plugin::ScriptEngineEvent::POST_INIT);
#endif

    cc::events::ScriptEngine::broadcast(cc::ScriptEngineEvent::AFTER_INIT);

    for (const auto &hook : _afterInitHookArray) { hook(); }
    _afterInitHookArray.clear();

    return _isValid;
}

bool ScriptEngine::init(v8::Isolate * /*isolate*/) {
    return init();
}

bool ScriptEngine::start() {
    if (!init()) return false;
    _startTime = std::chrono::steady_clock::now();
    return callRegisteredCallback();
}

bool ScriptEngine::start(v8::Isolate *isolate) {
    CC_UNUSED_PARAM(isolate);
    return start();
}

void ScriptEngine::addBeforeInitHook(const std::function<void()> &hook) {
    _beforeInitHookArray.push_back(hook);
}

void ScriptEngine::addAfterInitHook(const std::function<void()> &hook) {
    _afterInitHookArray.push_back(hook);
}

void ScriptEngine::addBeforeCleanupHook(const std::function<void()> &hook) {
    _beforeCleanupHookArray.push_back(hook);
}

void ScriptEngine::addAfterCleanupHook(const std::function<void()> &hook) {
    _afterCleanupHookArray.push_back(hook);
}

void ScriptEngine::cleanup() {
    if (!_isValid) return;
    if (_isInCleanup) return;
    _isInCleanup = true;

    cc::events::ScriptEngine::broadcast(cc::ScriptEngineEvent::BEFORE_CLEANUP);

    for (const auto &hook : _beforeCleanupHookArray) { hook(); }
    _beforeCleanupHookArray.clear();

    if (_globalObj != nullptr) {
        _globalObj->unroot();
        _globalObj->decRef();
        _globalObj = nullptr;
    }

    Class::cleanup();
    NativePtrToObjectMap::destroy();

    _isValid = false;
    _registerCallbackArray.clear();
    _permRegisterCallbackArray.clear();

    for (const auto &hook : _afterCleanupHookArray) { hook(); }
    _afterCleanupHookArray.clear();

    if (_ctx) {
        JS_FreeContext(_ctx);
        _ctx = nullptr;
    }
    if (_rt) {
        JS_FreeRuntime(_rt);
        _rt = nullptr;
    }

    _isInCleanup = false;

    cc::events::ScriptEngine::broadcast(cc::ScriptEngineEvent::AFTER_CLEANUP);
}

bool ScriptEngine::evalString(const char *script, uint32_t length, Value *ret, const char *fileName) {
    if (_ctx == nullptr || script == nullptr) return false;

    const char *fn = (fileName != nullptr) ? fileName : "<eval>";
    size_t len = (length > 0) ? length : strlen(script);

    JSValue val = JS_Eval(_ctx, script, len, fn, JS_EVAL_TYPE_GLOBAL);
    bool ok = !JS_IsException(val);

    if (!ok) {
        JSValue exc = JS_GetException(_ctx);
        const char *msg = JS_ToCString(_ctx, exc);
        JSValue stack = JS_GetPropertyStr(_ctx, exc, "stack");
        const char *stackStr = JS_ToCString(_ctx, stack);
        SE_LOGE("ScriptEngine::evalString exception: %s\nstack:\n%s\n", msg ? msg : "", stackStr ? stackStr : "");

        if (_exceptionCallback) {
            _exceptionCallback(fn, msg ? msg : "", stackStr ? stackStr : "");
        }
        std::get<0>(_lastException) = fn;
        std::get<1>(_lastException) = msg ? msg : "";
        std::get<2>(_lastException) = stackStr ? stackStr : "";

        if (stackStr) JS_FreeCString(_ctx, stackStr);
        if (msg) JS_FreeCString(_ctx, msg);
        JS_FreeValue(_ctx, stack);
        JS_FreeValue(_ctx, exc);
    } else if (ret != nullptr) {
        internal::jsToSeValue(_ctx, val, ret);
    }

    JS_FreeValue(_ctx, val);
    return ok;
}

bool ScriptEngine::saveByteCodeToFile(const ccstd::string & /*path*/, const ccstd::string & /*pathBc*/) {
    return false;
}

ccstd::string ScriptEngine::getCurrentStackTrace() {
    return {};
}

void ScriptEngine::setFileOperationDelegate(const FileOperationDelegate &delegate) {
    _fileOperationDelegate = delegate;
}

const ScriptEngine::FileOperationDelegate &ScriptEngine::getFileOperationDelegate() const {
    return _fileOperationDelegate;
}

bool ScriptEngine::runScript(const ccstd::string &path, Value *ret) {
    if (!_fileOperationDelegate.isValid()) return false;
    ccstd::string fullPath = _fileOperationDelegate.onGetFullPath(path);
    if (!_fileOperationDelegate.onCheckFileExist(fullPath)) {
        SE_LOGE("ScriptEngine::runScript file not found: %s\n", fullPath.c_str());
        return false;
    }
    ccstd::string content = _fileOperationDelegate.onGetStringFromFile(fullPath);
    if (content.empty()) {
        SE_LOGE("ScriptEngine::runScript file is empty: %s\n", fullPath.c_str());
        return false;
    }
    return evalString(content.c_str(), static_cast<uint32_t>(content.size()), ret, fullPath.c_str());
}

bool ScriptEngine::isGarbageCollecting() const {
    return _isGarbageCollecting;
}

void ScriptEngine::garbageCollect() {
    if (_rt) {
        JS_RunGC(_rt);
    }
}

bool ScriptEngine::isValid() const {
    return _isValid;
}

void ScriptEngine::throwException(const ccstd::string &errorMessage) {
    if (_ctx) {
        JS_ThrowInternalError(_ctx, "%s", errorMessage.c_str());
    }
}

void ScriptEngine::clearException() {}

void ScriptEngine::setExceptionCallback(const ExceptionCallback &cb) {
    _exceptionCallback = cb;
}

void ScriptEngine::setJSExceptionCallback(const ExceptionCallback &cb) {
    _jsExceptionCallback = cb;
}

void ScriptEngine::enableDebugger(const ccstd::string & /*serverAddr*/, uint32_t /*port*/, bool /*isWait*/) {
    _debuggerEnabled = false;
}

bool ScriptEngine::isDebuggerEnabled() const {
    return _debuggerEnabled;
}

void ScriptEngine::mainLoopUpdate() {
    if (_ctx) {
        JSContext *ctx1 = nullptr;
        while (JS_ExecutePendingJob(_rt, &ctx1) > 0) {}
    }
}

bool ScriptEngine::callFunction(Object *targetObj, const char *funcName, uint32_t argc, Value *args, Value *rval) {
    if (_ctx == nullptr || targetObj == nullptr) return false;

    JSValue jsTarget = targetObj->_getJSObject();
    JSValue func = JS_GetPropertyStr(_ctx, jsTarget, funcName);
    if (!JS_IsFunction(_ctx, func)) {
        JS_FreeValue(_ctx, func);
        return false;
    }

    auto *jsArgs = reinterpret_cast<JSValue *>(alloca(sizeof(JSValue) * (argc > 0 ? argc : 1)));
    for (uint32_t i = 0; i < argc; ++i) {
        jsArgs[i] = internal::seToJsValue(_ctx, args[i]);
    }

    JSValue ret = JS_Call(_ctx, func, jsTarget, static_cast<int>(argc), jsArgs);

    for (uint32_t i = 0; i < argc; ++i) {
        JS_FreeValue(_ctx, jsArgs[i]);
    }
    JS_FreeValue(_ctx, func);

    bool ok = !JS_IsException(ret);
    if (ok && rval != nullptr) {
        internal::jsToSeValue(_ctx, ret, rval);
    }
    JS_FreeValue(_ctx, ret);
    return ok;
}

void ScriptEngine::handlePromiseExceptions() {
    if (_ctx) {
        JSContext *ctx1 = nullptr;
        while (JS_ExecutePendingJob(_rt, &ctx1) > 0) {}
    }
}

void ScriptEngine::_setDebuggerInfo(const DebuggerInfo &info) {
    sDebuggerInfo = info;
}

} // namespace se
