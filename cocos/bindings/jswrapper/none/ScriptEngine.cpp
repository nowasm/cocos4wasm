#include "ScriptEngine.h"
#include "Object.h"
#include "../MappingUtils.h"
#include "base/Macros.h"
#include "engine/EngineEvents.h"

#if USE_PLUGINS
    #include "plugins/bus/EventBus.h"
    #include "plugins/bus/BusTypes.h"
#endif

namespace se {

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
    if (cb == nullptr) {
        return;
    }
    _registerCallbackArray.push_back(cb);
}

void ScriptEngine::addPermanentRegisterCallback(RegisterCallback cb) {
    if (cb == nullptr) {
        return;
    }
    _permRegisterCallbackArray.push_back(cb);
}

bool ScriptEngine::callRegisteredCallback() {
    if (_globalObj == nullptr) {
        return false;
    }
    for (auto *cb : _permRegisterCallbackArray) {
        if (!cb(_globalObj)) {
            return false;
        }
    }
    for (auto *cb : _registerCallbackArray) {
        if (!cb(_globalObj)) {
            return false;
        }
    }
    _registerCallbackArray.clear();
    return true;
}

bool ScriptEngine::init() {
    cleanup();

    ++_vmId;

    cc::events::ScriptEngine::broadcast(cc::ScriptEngineEvent::BEFORE_INIT);

    for (const auto &hook : _beforeInitHookArray) {
        hook();
    }
    _beforeInitHookArray.clear();

    NativePtrToObjectMap::init();

    if (_globalObj == nullptr) {
        _globalObj = Object::createPlainObject();
        if (_globalObj != nullptr) {
            _globalObj->incRef();
            _globalObj->setProperty("scriptEngineType", se::Value(ccstd::string("None")));
        }
    }

    _isValid = _globalObj != nullptr;

#if USE_PLUGINS
    cc::plugin::send(cc::plugin::BusType::SCRIPT_ENGINE, cc::plugin::ScriptEngineEvent::POST_INIT);
#endif

    cc::events::ScriptEngine::broadcast(cc::ScriptEngineEvent::AFTER_INIT);

    for (const auto &hook : _afterInitHookArray) {
        hook();
    }
    _afterInitHookArray.clear();

    return _isValid;
}

bool ScriptEngine::init(v8::Isolate * /*isolate*/) {
    return init();
}

bool ScriptEngine::start() {
    if (!init()) {
        return false;
    }
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
    if (!_isValid) {
        return;
    }
    if (_isInCleanup) {
        return;
    }
    _isInCleanup = true;

    cc::events::ScriptEngine::broadcast(cc::ScriptEngineEvent::BEFORE_CLEANUP);

    for (const auto &hook : _beforeCleanupHookArray) {
        hook();
    }
    _beforeCleanupHookArray.clear();

    if (_globalObj != nullptr) {
        _globalObj->decRef();
        _globalObj = nullptr;
    }

    NativePtrToObjectMap::destroy();
    _isValid = false;
    _registerCallbackArray.clear();
    _permRegisterCallbackArray.clear();

    for (const auto &hook : _afterCleanupHookArray) {
        hook();
    }
    _afterCleanupHookArray.clear();

    _isInCleanup = false;

    cc::events::ScriptEngine::broadcast(cc::ScriptEngineEvent::AFTER_CLEANUP);
}

bool ScriptEngine::evalString(const char * /*script*/, uint32_t /*length*/, Value *ret, const char * /*fileName*/) {
    if (ret != nullptr) {
        ret->setUndefined();
    }
    return false;
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

bool ScriptEngine::runScript(const ccstd::string & /*path*/, Value *ret) {
    if (ret != nullptr) {
        ret->setUndefined();
    }
    return false;
}

bool ScriptEngine::isGarbageCollecting() const {
    return _isGarbageCollecting;
}

bool ScriptEngine::isValid() const {
    return _isValid;
}

void ScriptEngine::throwException(const ccstd::string & /*errorMessage*/) {}

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

void ScriptEngine::mainLoopUpdate() {}

void ScriptEngine::handlePromiseExceptions() {}

bool ScriptEngine::callFunction(Object * /*targetObj*/, const char * /*funcName*/, uint32_t /*argc*/, Value * /*args*/, Value * /*rval*/) {
    return false;
}

void ScriptEngine::garbageCollect() {}

void ScriptEngine::_setGarbageCollecting(bool v) {
    _isGarbageCollecting = v;
}

void ScriptEngine::_setDebuggerInfo(const DebuggerInfo &info) {
    sDebuggerInfo = info;
}

} // namespace se
