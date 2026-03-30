/****************************************************************************
 Copyright (c) 2010-2012 cocos2d-x.org
 Copyright (c) 2013-2016 Chukong Technologies Inc.
 Copyright (c) 2016-2023 Xiamen Yaji Software Co., Ltd.

 WebAssembly stub: no libwebsockets / libuv. init() reports CONNECTION_FAILURE on the engine thread.
****************************************************************************/

#include "base/memory/Memory.h"
#include "application/ApplicationManager.h"
#include "engine/Engine.h"
#include "network/WebSocket.h"

#include <algorithm>
#include <mutex>
#include "base/std/container/string.h"
#include "base/std/container/vector.h"

namespace {

ccstd::vector<WebSocketImpl *> *websocketInstances{nullptr};
std::mutex instanceMutex;

} // namespace

class WebSocketImpl {
public:
    static void closeAllConnections();
    explicit WebSocketImpl(cc::network::WebSocket *ws);
    ~WebSocketImpl();

    bool init(const cc::network::WebSocket::Delegate &delegate,
              const ccstd::string &url,
              const ccstd::vector<ccstd::string> *protocols = nullptr,
              const ccstd::string &caFilePath = "");

    void send(const ccstd::string &message);
    void send(const unsigned char *binaryMsg, unsigned int len);
    void close();
    void closeAsync();
    void closeAsync(int code, const ccstd::string &reason);
    cc::network::WebSocket::State getReadyState() const;
    const ccstd::string &getUrl() const;
    const ccstd::string &getProtocol() const;
    cc::network::WebSocket::Delegate *getDelegate() const;
    size_t getBufferedAmount() const;
    ccstd::string getExtensions() const;

private:
    void failConnect();

    cc::network::WebSocket *_ws{nullptr};
    cc::network::WebSocket::State _readyState{cc::network::WebSocket::State::CLOSED};
    mutable std::mutex _readyStateMutex;
    ccstd::string _url;
    ccstd::string _selectedProtocol;
    cc::network::WebSocket::Delegate *_delegate{nullptr};
};

void WebSocketImpl::failConnect() {
    auto *del = _delegate;
    auto *ws = _ws;
    auto report = [del, ws]() {
        if (del != nullptr && ws != nullptr) {
            del->onError(ws, cc::network::WebSocket::ErrorCode::CONNECTION_FAILURE);
        }
    };
    auto *mgr = cc::ApplicationManager::getInstance();
    if (mgr == nullptr) {
        report();
        return;
    }
    auto app = mgr->getCurrentAppSafe();
    if (app != nullptr && app->getEngine() != nullptr) {
        app->getEngine()->getScheduler()->performFunctionInCocosThread(report);
    } else {
        report();
    }
}

void WebSocketImpl::closeAllConnections() {
    std::lock_guard<std::mutex> lk(instanceMutex);
    if (websocketInstances == nullptr) {
        return;
    }
    ccstd::vector<WebSocketImpl *> copy = *websocketInstances;
    for (auto *inst : copy) {
        if (inst != nullptr) {
            inst->closeAsync();
        }
    }
    websocketInstances->clear();
    delete websocketInstances;
    websocketInstances = nullptr;
}

WebSocketImpl::WebSocketImpl(cc::network::WebSocket *ws)
: _ws(ws),
  _readyState(cc::network::WebSocket::State::CONNECTING) {
    std::lock_guard<std::mutex> lk(instanceMutex);
    if (websocketInstances == nullptr) {
        websocketInstances = ccnew ccstd::vector<WebSocketImpl *>();
    }
    websocketInstances->push_back(this);
}

WebSocketImpl::~WebSocketImpl() {
    std::lock_guard<std::mutex> lk(instanceMutex);
    if (websocketInstances != nullptr) {
        auto iter = std::find(websocketInstances->begin(), websocketInstances->end(), this);
        if (iter != websocketInstances->end()) {
            websocketInstances->erase(iter);
        }
    }
}

bool WebSocketImpl::init(const cc::network::WebSocket::Delegate &delegate,
                         const ccstd::string &url,
                         const ccstd::vector<ccstd::string> * /*protocols*/,
                         const ccstd::string & /*caFilePath*/) {
    _delegate = const_cast<cc::network::WebSocket::Delegate *>(&delegate);
    _url = url;
    if (_url.empty()) {
        return false;
    }
    {
        std::lock_guard<std::mutex> lk(_readyStateMutex);
        _readyState = cc::network::WebSocket::State::CONNECTING;
    }
    failConnect();
    {
        std::lock_guard<std::mutex> lk(_readyStateMutex);
        _readyState = cc::network::WebSocket::State::CLOSED;
    }
    return true;
}

void WebSocketImpl::send(const ccstd::string & /*message*/) {}
void WebSocketImpl::send(const unsigned char * /*binaryMsg*/, unsigned int /*len*/) {}

void WebSocketImpl::close() {
    std::lock_guard<std::mutex> lk(_readyStateMutex);
    _readyState = cc::network::WebSocket::State::CLOSED;
}

void WebSocketImpl::closeAsync() {
    close();
}

void WebSocketImpl::closeAsync(int /*code*/, const ccstd::string & /*reason*/) {
    close();
}

cc::network::WebSocket::State WebSocketImpl::getReadyState() const {
    std::lock_guard<std::mutex> lk(_readyStateMutex);
    return _readyState;
}

const ccstd::string &WebSocketImpl::getUrl() const {
    return _url;
}

const ccstd::string &WebSocketImpl::getProtocol() const {
    return _selectedProtocol;
}

cc::network::WebSocket::Delegate *WebSocketImpl::getDelegate() const {
    return _delegate;
}

size_t WebSocketImpl::getBufferedAmount() const {
    return 0;
}

ccstd::string WebSocketImpl::getExtensions() const {
    return {};
}

namespace cc {
namespace network {

void WebSocket::closeAllConnections() {
    WebSocketImpl::closeAllConnections();
}

WebSocket::WebSocket() {
    _impl = ccnew WebSocketImpl(this);
}

WebSocket::~WebSocket() {
    delete _impl;
}

bool WebSocket::init(const Delegate &delegate,
                     const ccstd::string &url,
                     const ccstd::vector<ccstd::string> *protocols,
                     const ccstd::string &caFilePath) {
    return _impl->init(delegate, url, protocols, caFilePath);
}

void WebSocket::send(const ccstd::string &message) {
    _impl->send(message);
}

void WebSocket::send(const unsigned char *binaryMsg, unsigned int len) {
    _impl->send(binaryMsg, len);
}

void WebSocket::close() {
    _impl->close();
}

void WebSocket::closeAsync() {
    _impl->closeAsync();
}

void WebSocket::closeAsync(int code, const ccstd::string &reason) {
    _impl->closeAsync(code, reason);
}

WebSocket::State WebSocket::getReadyState() const {
    return _impl->getReadyState();
}

ccstd::string WebSocket::getExtensions() const {
    return _impl->getExtensions();
}

size_t WebSocket::getBufferedAmount() const {
    return _impl->getBufferedAmount();
}

const ccstd::string &WebSocket::getUrl() const {
    return _impl->getUrl();
}

const ccstd::string &WebSocket::getProtocol() const {
    return _impl->getProtocol();
}

WebSocket::Delegate *WebSocket::getDelegate() const {
    return _impl->getDelegate();
}

} // namespace network
} // namespace cc
