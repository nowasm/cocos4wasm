/****************************************************************************
 Copyright (c) 2012 greathqy
 Copyright (c) 2012 cocos2d-x.org
 Copyright (c) 2013-2016 Chukong Technologies Inc.
 Copyright (c) 2017-2023 Xiamen Yaji Software Co., Ltd.

 Emscripten: no bundled libcurl / USE_LIBCURL in this toolchain — minimal
 HttpClient that completes requests as failures so the engine and bindings link.
****************************************************************************/

#include "network/HttpClient.h"

#include <cstring>

#include "application/ApplicationManager.h"
#include "base/Log.h"
#include "base/ThreadPool.h"
#include "base/memory/Memory.h"
#include "platform/FileUtils.h"
#include "platform/StdC.h"

namespace cc {

namespace network {

static HttpClient *_httpClient = nullptr;
static LegacyThreadPool *gThreadPool = nullptr;

static void stubProcessResponse(HttpResponse *response, char *responseMessage) {
    static const char kMsg[] = "HttpClient stub on Emscripten (no libcurl in build)";
    const int cap = HttpClient::RESPONSE_BUFFER_SIZE;
    strncpy(responseMessage, kMsg, static_cast<size_t>(cap - 1));
    responseMessage[cap - 1] = '\0';
    response->setResponseCode(-1);
    response->setSucceed(false);
    response->setErrorBuffer(responseMessage);
}

void HttpClient::networkThread() {
    increaseThreadCount();

    while (true) {
        HttpRequest *request;

        {
            std::lock_guard<std::mutex> lock(_requestQueueMutex);
            while (_requestQueue.empty()) {
                _sleepCondition.wait(_requestQueueMutex);
            }
            request = _requestQueue.at(0);
            _requestQueue.erase(0);
        }

        if (request == _requestSentinel) {
            break;
        }

        HttpResponse *response = ccnew HttpResponse(request);
        response->addRef();

        stubProcessResponse(response, _responseMessage);

        _responseQueueMutex.lock();
        _responseQueue.pushBack(response);
        _responseQueueMutex.unlock();

        _schedulerMutex.lock();
        if (auto sche = _scheduler.lock()) {
            sche->performFunctionInCocosThread(CC_CALLBACK_0(HttpClient::dispatchResponseCallbacks, this));
        }
        _schedulerMutex.unlock();
    }

    _requestQueueMutex.lock();
    _requestQueue.clear();
    _requestQueueMutex.unlock();

    _responseQueueMutex.lock();
    _responseQueue.clear();
    _responseQueueMutex.unlock();

    decreaseThreadCountAndMayDeleteThis();
}

void HttpClient::networkThreadAlone(HttpRequest *request, HttpResponse *response) {
    char responseMessage[HttpClient::RESPONSE_BUFFER_SIZE] = {0};
    stubProcessResponse(response, responseMessage);

    _schedulerMutex.lock();
    if (auto sche = _scheduler.lock()) {
        sche->performFunctionInCocosThread([this, response, request] {
            const ccHttpRequestCallback &callback = request->getResponseCallback();

            if (callback != nullptr) {
                callback(this, response);
            }
            response->release();
            request->release();
        });
    }
    _schedulerMutex.unlock();

    decreaseThreadCountAndMayDeleteThis();
}

HttpClient *HttpClient::getInstance() {
    if (_httpClient == nullptr) {
        _httpClient = ccnew HttpClient();
    }

    return _httpClient;
}

void HttpClient::destroyInstance() {
    if (nullptr == _httpClient) {
        CC_LOG_DEBUG("HttpClient singleton is nullptr");
        return;
    }

    CC_LOG_DEBUG("HttpClient::destroyInstance begin");
    auto thiz = _httpClient;
    _httpClient = nullptr;

    if (auto sche = thiz->_scheduler.lock()) {
        sche->unscheduleAllForTarget(thiz);
    }

    thiz->_schedulerMutex.lock();
    thiz->_scheduler.reset();
    thiz->_schedulerMutex.unlock();

    thiz->_requestQueueMutex.lock();
    thiz->_requestQueue.pushBack(thiz->_requestSentinel);
    thiz->_requestQueueMutex.unlock();

    thiz->_sleepCondition.notify_one();
    thiz->decreaseThreadCountAndMayDeleteThis();

    CC_LOG_DEBUG("HttpClient::destroyInstance() finished!");
}

void HttpClient::enableCookies(const char *cookieFile) {
    std::lock_guard<std::mutex> lock(_cookieFileMutex);
    if (cookieFile) {
        _cookieFilename = ccstd::string(cookieFile);
    } else {
        _cookieFilename = (FileUtils::getInstance()->getWritablePath() + "cookieFile.txt");
    }
}

void HttpClient::setSSLVerification(const ccstd::string &caFile) {
    std::lock_guard<std::mutex> lock(_sslCaFileMutex);
    _sslCaFilename = caFile;
}

HttpClient::HttpClient()
: _isInited(false),
  _timeoutForConnect(30),
  _timeoutForRead(60),
  _threadCount(0),
  _cookie(nullptr),
  _requestSentinel(ccnew HttpRequest()) {
    CC_LOG_DEBUG("In the constructor of HttpClient!");
    _requestSentinel->addRef();
    if (gThreadPool == nullptr) {
        gThreadPool = LegacyThreadPool::newFixedThreadPool(4);
    }
    memset(_responseMessage, 0, RESPONSE_BUFFER_SIZE * sizeof(char));
    _scheduler = CC_CURRENT_ENGINE()->getScheduler();
    increaseThreadCount();
}

HttpClient::~HttpClient() {
    CC_SAFE_RELEASE(_requestSentinel);
    CC_LOG_DEBUG("HttpClient destructor");
}

bool HttpClient::lazyInitThreadSemaphore() {
    if (_isInited) {
        return true;
    }
    auto t = std::thread(CC_CALLBACK_0(HttpClient::networkThread, this));
    t.detach();
    _isInited = true;

    return true;
}

void HttpClient::send(HttpRequest *request) {
    if (false == lazyInitThreadSemaphore()) {
        return;
    }

    if (!request) {
        return;
    }

    request->addRef();

    _requestQueueMutex.lock();
    _requestQueue.pushBack(request);
    _requestQueueMutex.unlock();

    _sleepCondition.notify_one();
}

void HttpClient::sendImmediate(HttpRequest *request) {
    if (!request) {
        return;
    }

    request->addRef();
    HttpResponse *response = ccnew HttpResponse(request);
    response->addRef();

    increaseThreadCount();
    gThreadPool->pushTask([this, request, response](int /*tid*/) { HttpClient::networkThreadAlone(request, response); });
}

void HttpClient::dispatchResponseCallbacks() {
    HttpResponse *response = nullptr;

    _responseQueueMutex.lock();
    if (!_responseQueue.empty()) {
        response = _responseQueue.at(0);
        _responseQueue.erase(0);
    }
    _responseQueueMutex.unlock();

    if (response) {
        HttpRequest *request = response->getHttpRequest();
        const ccHttpRequestCallback &callback = request->getResponseCallback();

        if (callback != nullptr) {
            callback(this, response);
        }

        response->release();
        request->release();
    }
}

void HttpClient::increaseThreadCount() {
    _threadCountMutex.lock();
    ++_threadCount;
    _threadCountMutex.unlock();
}

void HttpClient::decreaseThreadCountAndMayDeleteThis() {
    bool needDeleteThis = false;
    if (_threadCount == 0) {
        return;
    }
    _threadCountMutex.lock();
    --_threadCount;
    if (0 == _threadCount) {
        needDeleteThis = true;
    }

    _threadCountMutex.unlock();
    if (needDeleteThis) {
        delete this;
    }
}

const ccstd::string &HttpClient::getCookieFilename() {
    std::lock_guard<std::mutex> lock(_cookieFileMutex);
    return _cookieFilename;
}

const ccstd::string &HttpClient::getSSLVerification() {
    std::lock_guard<std::mutex> lock(_sslCaFileMutex);
    return _sslCaFilename;
}

} // namespace network

} // namespace cc
