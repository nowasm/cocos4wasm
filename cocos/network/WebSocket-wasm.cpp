/****************************************************************************
 Copyright (c) 2010-2012 cocos2d-x.org
 Copyright (c) 2013-2016 Chukong Technologies Inc.
 Copyright (c) 2016-2023 Xiamen Yaji Software Co., Ltd.

 Emscripten: uses browser WebSocket API via Emscripten's websocket.h.
****************************************************************************/

#include "base/memory/Memory.h"
#include "application/ApplicationManager.h"
#include "engine/Engine.h"
#include "network/WebSocket.h"
#include "base/Log.h"

#include <algorithm>
#include <mutex>
#include "base/std/container/string.h"
#include "base/std/container/vector.h"

#include <emscripten/websocket.h>

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
    static EM_BOOL onOpen(int eventType, const EmscriptenWebSocketOpenEvent *event, void *userData);
    static EM_BOOL onMessage(int eventType, const EmscriptenWebSocketMessageEvent *event, void *userData);
    static EM_BOOL onError(int eventType, const EmscriptenWebSocketErrorEvent *event, void *userData);
    static EM_BOOL onClose(int eventType, const EmscriptenWebSocketCloseEvent *event, void *userData);

    void dispatchOnMainThread(std::function<void()> func);

    cc::network::WebSocket *_ws{nullptr};
    cc::network::WebSocket::State _readyState{cc::network::WebSocket::State::CLOSED};
    mutable std::mutex _readyStateMutex;
    ccstd::string _url;
    ccstd::string _selectedProtocol;
    cc::network::WebSocket::Delegate *_delegate{nullptr};
    EMSCRIPTEN_WEBSOCKET_T _socket{0};
};

// ---- Static callbacks ----

EM_BOOL WebSocketImpl::onOpen(int /*eventType*/, const EmscriptenWebSocketOpenEvent *event, void *userData) {
    auto *impl = reinterpret_cast<WebSocketImpl *>(userData);
    if (!impl) return EM_FALSE;

    {
        std::lock_guard<std::mutex> lk(impl->_readyStateMutex);
        impl->_readyState = cc::network::WebSocket::State::OPEN;
    }

    impl->dispatchOnMainThread([impl]() {
        if (impl->_delegate) {
            impl->_delegate->onOpen(impl->_ws);
        }
    });

    return EM_TRUE;
}

EM_BOOL WebSocketImpl::onMessage(int /*eventType*/, const EmscriptenWebSocketMessageEvent *event, void *userData) {
    auto *impl = reinterpret_cast<WebSocketImpl *>(userData);
    if (!impl) return EM_FALSE;

    cc::network::WebSocket::Data wsData;
    wsData.isBinary = event->isText == 0;
    wsData.len = static_cast<uint32_t>(event->numBytes);

    // Copy the data since the event pointer may be invalidated
    char *dataCopy = new char[event->numBytes];
    memcpy(dataCopy, event->data, event->numBytes);
    wsData.bytes = dataCopy;

    impl->dispatchOnMainThread([impl, wsData]() {
        if (impl->_delegate) {
            impl->_delegate->onMessage(impl->_ws, wsData);
        }
        delete[] wsData.bytes;
    });

    return EM_TRUE;
}

EM_BOOL WebSocketImpl::onError(int /*eventType*/, const EmscriptenWebSocketErrorEvent *event, void *userData) {
    auto *impl = reinterpret_cast<WebSocketImpl *>(userData);
    if (!impl) return EM_FALSE;

    impl->dispatchOnMainThread([impl]() {
        if (impl->_delegate) {
            impl->_delegate->onError(impl->_ws, cc::network::WebSocket::ErrorCode::CONNECTION_FAILURE);
        }
    });

    return EM_TRUE;
}

EM_BOOL WebSocketImpl::onClose(int /*eventType*/, const EmscriptenWebSocketCloseEvent *event, void *userData) {
    auto *impl = reinterpret_cast<WebSocketImpl *>(userData);
    if (!impl) return EM_FALSE;

    {
        std::lock_guard<std::mutex> lk(impl->_readyStateMutex);
        impl->_readyState = cc::network::WebSocket::State::CLOSED;
    }

    uint16_t code = static_cast<uint16_t>(event->code);
    ccstd::string reason = event->reason ? event->reason : "";
    bool wasClean = event->wasClean;

    impl->dispatchOnMainThread([impl, code, reason, wasClean]() {
        if (impl->_delegate) {
            impl->_delegate->onClose(impl->_ws, code, reason, wasClean);
        }
    });

    return EM_TRUE;
}

void WebSocketImpl::dispatchOnMainThread(std::function<void()> func) {
    auto *mgr = cc::ApplicationManager::getInstance();
    if (mgr == nullptr) {
        func();
        return;
    }
    auto app = mgr->getCurrentAppSafe();
    if (app != nullptr && app->getEngine() != nullptr) {
        app->getEngine()->getScheduler()->performFunctionInCocosThread(std::move(func));
    } else {
        func();
    }
}

// ---- Instance lifecycle ----

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
    if (_socket) {
        emscripten_websocket_close(_socket, 1000, "destructor");
        emscripten_websocket_delete(_socket);
        _socket = 0;
    }

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
                         const ccstd::vector<ccstd::string> *protocols,
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

    // Create WebSocket
    EmscriptenWebSocketCreateAttributes attrs;
    emscripten_websocket_init_create_attributes(&attrs);
    attrs.url = _url.c_str();

    // Build protocol string (comma-separated)
    ccstd::string protocolStr;
    if (protocols && !protocols->empty()) {
        for (size_t i = 0; i < protocols->size(); ++i) {
            if (i > 0) protocolStr += ",";
            protocolStr += (*protocols)[i];
        }
        attrs.protocols = protocolStr.c_str();
    }

    _socket = emscripten_websocket_new(&attrs);
    if (_socket <= 0) {
        CC_LOG_ERROR("WebSocket: Failed to create WebSocket for URL: %s", _url.c_str());
        {
            std::lock_guard<std::mutex> lk(_readyStateMutex);
            _readyState = cc::network::WebSocket::State::CLOSED;
        }
        dispatchOnMainThread([this]() {
            if (_delegate) {
                _delegate->onError(_ws, cc::network::WebSocket::ErrorCode::CONNECTION_FAILURE);
            }
        });
        return false;
    }

    // Set callbacks
    emscripten_websocket_set_onopen_callback(_socket, this, onOpen);
    emscripten_websocket_set_onmessage_callback(_socket, this, onMessage);
    emscripten_websocket_set_onerror_callback(_socket, this, onError);
    emscripten_websocket_set_onclose_callback(_socket, this, onClose);

    return true;
}

void WebSocketImpl::send(const ccstd::string &message) {
    if (_socket) {
        emscripten_websocket_send_utf8_text(_socket, message.c_str());
    }
}

void WebSocketImpl::send(const unsigned char *binaryMsg, unsigned int len) {
    if (_socket) {
        emscripten_websocket_send_binary(_socket, const_cast<void *>(static_cast<const void *>(binaryMsg)), len);
    }
}

void WebSocketImpl::close() {
    if (_socket) {
        {
            std::lock_guard<std::mutex> lk(_readyStateMutex);
            _readyState = cc::network::WebSocket::State::CLOSING;
        }
        emscripten_websocket_close(_socket, 1000, "normal closure");
    }
}

void WebSocketImpl::closeAsync() {
    close();
}

void WebSocketImpl::closeAsync(int code, const ccstd::string &reason) {
    if (_socket) {
        {
            std::lock_guard<std::mutex> lk(_readyStateMutex);
            _readyState = cc::network::WebSocket::State::CLOSING;
        }
        emscripten_websocket_close(_socket, static_cast<unsigned short>(code), reason.c_str());
    }
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
    if (_socket) {
        unsigned long long amount = 0;
        emscripten_websocket_get_buffered_amount(_socket, &amount);
        return static_cast<size_t>(amount);
    }
    return 0;
}

ccstd::string WebSocketImpl::getExtensions() const {
    return {};
}

// ---- WebSocket public API ----

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
