#pragma once

#include "../Value.h"
#include "../config.h"
#include "base/std/container/string.h"

extern "C" {
#include "quickjs.h"
}

#include <chrono>
#include <functional>
#include <vector>

namespace v8 {
class Isolate;
}

namespace se {

class Object;

class AutoHandleScope {
public:
    AutoHandleScope() = default;
    ~AutoHandleScope() = default;
};

class ScriptEngine final {
public:
    class FileOperationDelegate {
    public:
        FileOperationDelegate()
        : onGetDataFromFile(nullptr),
          onGetStringFromFile(nullptr),
          onCheckFileExist(nullptr),
          onGetFullPath(nullptr) {}

        bool isValid() const {
            return onGetDataFromFile != nullptr && onGetStringFromFile != nullptr && onCheckFileExist != nullptr && onGetFullPath != nullptr;
        }

        std::function<void(const ccstd::string &, const std::function<void(const uint8_t *, size_t)> &)> onGetDataFromFile;
        std::function<ccstd::string(const ccstd::string &)> onGetStringFromFile;
        std::function<bool(const ccstd::string &)> onCheckFileExist;
        std::function<ccstd::string(const ccstd::string &)> onGetFullPath;
    };

    using ExceptionCallback = std::function<void(const char *, const char *, const char *)>;
    using RegisterCallback = bool (*)(Object *);

    static ScriptEngine *getInstance();
    static void destroyInstance();

    ScriptEngine();
    ~ScriptEngine();

    Object *getGlobalObject() const;

    void addRegisterCallback(RegisterCallback cb);
    void addPermanentRegisterCallback(RegisterCallback cb);

    bool start();
    bool start(v8::Isolate *isolate);

    bool init();
    bool init(v8::Isolate *isolate);

    void addBeforeInitHook(const std::function<void()> &hook);
    void addAfterInitHook(const std::function<void()> &hook);
    void addBeforeCleanupHook(const std::function<void()> &hook);
    void addAfterCleanupHook(const std::function<void()> &hook);

    void cleanup();

    bool evalString(const char *script, uint32_t length = 0, Value *ret = nullptr, const char *fileName = nullptr);

    bool saveByteCodeToFile(const ccstd::string &path, const ccstd::string &pathBc);

    ccstd::string getCurrentStackTrace();

    void setFileOperationDelegate(const FileOperationDelegate &delegate);
    const FileOperationDelegate &getFileOperationDelegate() const;

    bool runScript(const ccstd::string &path, Value *ret = nullptr);

    bool isGarbageCollecting() const;
    bool isInCleanup() const { return _isInCleanup; }

    void garbageCollect();

    bool isValid() const;

    void clearException();

    void throwException(const ccstd::string &errorMessage);

    using ExceptionInfo = std::tuple<ccstd::string, ccstd::string, ccstd::string>;
    const ExceptionInfo &getLastException() const { return _lastException; }

    void setExceptionCallback(const ExceptionCallback &cb);
    void setJSExceptionCallback(const ExceptionCallback &cb);

    const std::chrono::steady_clock::time_point &getStartTime() const { return _startTime; }

    void enableDebugger(const ccstd::string &serverAddr, uint32_t port, bool isWait = false);
    bool isDebuggerEnabled() const;
    void mainLoopUpdate();

    bool callFunction(Object *targetObj, const char *funcName, uint32_t argc, Value *args, Value *rval = nullptr);

    void handlePromiseExceptions();

    uint32_t getVMId() const { return _vmId; }

    JSContext *getContext() const { return _ctx; }
    JSRuntime *getRuntime() const { return _rt; }

    void _setGarbageCollecting(bool v) { _isGarbageCollecting = v; }

    struct DebuggerInfo {
        ccstd::string serverAddr;
        uint32_t port{0};
        bool isWait{false};
    };
    static void _setDebuggerInfo(const DebuggerInfo &info); // NOLINT
    static DebuggerInfo sDebuggerInfo;

private:
    bool callRegisteredCallback();

    static ScriptEngine *sInstance;

    JSRuntime *_rt{nullptr};
    JSContext *_ctx{nullptr};
    Object *_globalObj{nullptr};

    bool _isValid{false};
    bool _isGarbageCollecting{false};
    bool _isInCleanup{false};
    bool _debuggerEnabled{false};
    uint32_t _vmId{0};

    FileOperationDelegate _fileOperationDelegate;
    ExceptionCallback _exceptionCallback{nullptr};
    ExceptionCallback _jsExceptionCallback{nullptr};

    std::vector<RegisterCallback> _registerCallbackArray;
    std::vector<RegisterCallback> _permRegisterCallbackArray;

    std::vector<std::function<void()>> _beforeInitHookArray;
    std::vector<std::function<void()>> _afterInitHookArray;
    std::vector<std::function<void()>> _beforeCleanupHookArray;
    std::vector<std::function<void()>> _afterCleanupHookArray;

    ExceptionInfo _lastException;
    std::chrono::steady_clock::time_point _startTime;
};

} // namespace se
