#pragma once

#include "../Value.h"
#include "../config.h"
#include "base/std/container/string.h"

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

    static ScriptEngine *getInstance();
    static void destroyInstance();

    ScriptEngine();
    ~ScriptEngine();

    Object *getGlobalObject() const;

    using RegisterCallback = bool (*)(Object *);

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
    bool isValid() const;

    void throwException(const ccstd::string &errorMessage);
    void clearException();

    using ExceptionCallback = std::function<void(const char *, const char *, const char *)>;
    void setExceptionCallback(const ExceptionCallback &cb);
    void setJSExceptionCallback(const ExceptionCallback &cb);

    const std::chrono::steady_clock::time_point &getStartTime() const { return _startTime; }

    void enableDebugger(const ccstd::string &serverAddr, uint32_t port, bool isWait = false);
    bool isDebuggerEnabled() const;

    void mainLoopUpdate();
    void handlePromiseExceptions();

    bool callFunction(Object *targetObj, const char *funcName, uint32_t argc, Value *args, Value *rval = nullptr);

    void garbageCollect();

    uint32_t getVMId() const { return _vmId; }

    void _setGarbageCollecting(bool v); // NOLINT

    struct DebuggerInfo {
        ccstd::string serverAddr;
        uint32_t port{0};
        bool isWait{false};
        bool isValid() const { return !serverAddr.empty() && port != 0; }
        void reset() {
            serverAddr.clear();
            port = 0;
            isWait = false;
        }
    };
    static void _setDebuggerInfo(const DebuggerInfo &info); // NOLINT

private:
    bool callRegisteredCallback();

    static ScriptEngine *sInstance;

    Object *_globalObj{nullptr};
    bool _isValid{false};
    bool _isInCleanup{false};
    bool _isGarbageCollecting{false};
    uint32_t _vmId{1};
    std::chrono::steady_clock::time_point _startTime{};

    std::vector<RegisterCallback> _registerCallbackArray;
    std::vector<RegisterCallback> _permRegisterCallbackArray;
    std::vector<std::function<void()>> _beforeInitHookArray;
    std::vector<std::function<void()>> _afterInitHookArray;
    std::vector<std::function<void()>> _beforeCleanupHookArray;
    std::vector<std::function<void()>> _afterCleanupHookArray;

    FileOperationDelegate _fileOperationDelegate;
    ExceptionCallback _exceptionCallback;
    ExceptionCallback _jsExceptionCallback;

    static DebuggerInfo sDebuggerInfo;
    bool _debuggerEnabled{false};
};

} // namespace se
