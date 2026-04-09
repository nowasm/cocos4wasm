#pragma once

#include "../Define.h"
#include "../Value.h"
#include "Base.h"

#include <chrono>
#include <functional>
#include <string>
#include <tuple>
#include <vector>

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
        FileOperationDelegate() = default;
        bool isValid() const;
        std::function<void(const ccstd::string &, const std::function<void(const uint8_t *, size_t)> &)> onGetDataFromFile;
        std::function<ccstd::string(const ccstd::string &)> onGetStringFromFile;
        std::function<bool(const ccstd::string &)> onCheckFileExist;
        std::function<ccstd::string(const ccstd::string &)> onGetFullPath;
    };

    using ExceptionCallback = std::function<void(const char *, const char *, const char *)>;
    using RegisterCallback = bool (*)(Object *);
    using ExceptionInfo = std::tuple<ccstd::string, ccstd::string, ccstd::string>;

    struct DebuggerInfo {
        ccstd::string serverAddr;
        uint32_t port{0};
        bool isWait{false};
    };

    static ScriptEngine *getInstance();
    static void destroyInstance();

    ScriptEngine();
    ~ScriptEngine();

    Object *getGlobalObject() const;

    void addRegisterCallback(RegisterCallback cb);
    void addPermanentRegisterCallback(RegisterCallback cb);
    bool start();
    bool init();

    void addBeforeInitHook(const std::function<void()> &hook);
    void addAfterInitHook(const std::function<void()> &hook);
    void addBeforeCleanupHook(const std::function<void()> &hook);
    void addAfterCleanupHook(const std::function<void()> &hook);
    void cleanup();

    bool evalString(const char *script, uint32_t length = 0, Value *ret = nullptr, const char *fileName = nullptr);
    bool runScript(const ccstd::string &path, Value *ret = nullptr);
    bool callFunction(Object *targetObj, const char *funcName, uint32_t argc, Value *args, Value *rval = nullptr);

    ccstd::string getCurrentStackTrace();
    bool saveByteCodeToFile(const ccstd::string &path, const ccstd::string &pathBc);

    void setFileOperationDelegate(const FileOperationDelegate &delegate);
    const FileOperationDelegate &getFileOperationDelegate() const;

    bool isGarbageCollecting() const;
    bool isInCleanup() const;
    bool isValid() const;

    void clearException();
    void throwException(const ccstd::string &errorMessage);
    const ExceptionInfo &getLastException() const;
    void setExceptionCallback(const ExceptionCallback &cb);
    void setJSExceptionCallback(const ExceptionCallback &cb);
    void handlePromiseExceptions();

    void enableDebugger(const ccstd::string &serverAddr, uint32_t port, bool isWait = false);
    bool isDebuggerEnabled() const;
    void mainLoopUpdate();

    void garbageCollect();
    const std::chrono::steady_clock::time_point &getStartTime() const;
    uint32_t getVMId() const;

    void _setGarbageCollecting(bool v);
    static void _setDebuggerInfo(const DebuggerInfo &info);
    static DebuggerInfo sDebuggerInfo;

private:
    Object *_globalObj{nullptr};
    FileOperationDelegate _fileOperationDelegate;
    ExceptionCallback _exceptionCallback;
    ExceptionCallback _jsExceptionCallback;
    ExceptionInfo _lastException;
    ccstd::vector<RegisterCallback> _registerCallbackArray;
    ccstd::vector<RegisterCallback> _permanentRegisterCallbackArray;
    ccstd::vector<std::function<void()>> _beforeInitHookArray;
    ccstd::vector<std::function<void()>> _afterInitHookArray;
    ccstd::vector<std::function<void()>> _beforeCleanupHookArray;
    ccstd::vector<std::function<void()>> _afterCleanupHookArray;
    std::chrono::steady_clock::time_point _startTime;
    bool _isValid{false};
    bool _isGarbageCollecting{false};
    bool _isInCleanup{false};
    bool _debuggerEnabled{false};

    static ScriptEngine *_instance;
};

} // namespace se
