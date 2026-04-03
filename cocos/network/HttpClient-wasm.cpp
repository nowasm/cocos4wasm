/****************************************************************************
 Copyright (c) 2012 greathqy
 Copyright (c) 2012 cocos2d-x.org
 Copyright (c) 2013-2016 Chukong Technologies Inc.
 Copyright (c) 2017-2023 Xiamen Yaji Software Co., Ltd.

 Emscripten: uses emscripten_fetch API for async HTTP requests.
****************************************************************************/

#include "network/HttpClient.h"

#include <cstring>

#include "application/ApplicationManager.h"
#include "base/Log.h"
#include "base/ThreadPool.h"
#include "base/memory/Memory.h"
#include "platform/FileUtils.h"
#include "platform/StdC.h"

#include <emscripten/fetch.h>

namespace cc {

namespace network {

static HttpClient *_httpClient = nullptr;

// ---- Fetch callback context ----
struct FetchCallbackData {
    HttpClient *client{nullptr};
    HttpRequest *request{nullptr};
    HttpResponse *response{nullptr};
};

static const char *httpMethodStr(HttpRequest::Type type) {
    switch (type) {
        case HttpRequest::Type::GET: return "GET";
        case HttpRequest::Type::POST: return "POST";
        case HttpRequest::Type::PUT: return "PUT";
        case HttpRequest::Type::DELETE_: return "DELETE";
        case HttpRequest::Type::PATCH: return "PATCH";
        default: return "GET";
    }
}

static void fetchOnLoad(emscripten_fetch_t *fetch) {
    auto *cbData = reinterpret_cast<FetchCallbackData *>(fetch->userData);
    if (!cbData) {
        emscripten_fetch_close(fetch);
        return;
    }

    HttpResponse *response = cbData->response;
    HttpRequest *request = cbData->request;
    HttpClient *client = cbData->client;

    response->setResponseCode(static_cast<long>(fetch->status));
    response->setSucceed(true);

    // Copy response data
    auto *responseData = response->getResponseData();
    if (fetch->numBytes > 0 && fetch->data) {
        responseData->insert(responseData->end(), fetch->data, fetch->data + fetch->numBytes);
    }

    // Copy response headers
    auto *responseHeaders = response->getResponseHeader();
    size_t headersLength = emscripten_fetch_get_response_headers_length(fetch);
    if (headersLength > 0) {
        char *headersBuf = new char[headersLength + 1];
        emscripten_fetch_get_response_headers(fetch, headersBuf, headersLength + 1);
        responseHeaders->insert(responseHeaders->end(), headersBuf, headersBuf + headersLength);
        delete[] headersBuf;
    }

    // Dispatch callback on main thread
    auto *mgr = cc::ApplicationManager::getInstance();
    if (mgr != nullptr) {
        auto app = mgr->getCurrentAppSafe();
        if (app != nullptr && app->getEngine() != nullptr) {
            app->getEngine()->getScheduler()->performFunctionInCocosThread([client, request, response]() {
                const ccHttpRequestCallback &callback = request->getResponseCallback();
                if (callback != nullptr) {
                    callback(client, response);
                }
                response->release();
                request->release();
            });
        } else {
            const ccHttpRequestCallback &callback = request->getResponseCallback();
            if (callback != nullptr) {
                callback(client, response);
            }
            response->release();
            request->release();
        }
    }

    delete cbData;
    emscripten_fetch_close(fetch);
}

static void fetchOnError(emscripten_fetch_t *fetch) {
    auto *cbData = reinterpret_cast<FetchCallbackData *>(fetch->userData);
    if (!cbData) {
        emscripten_fetch_close(fetch);
        return;
    }

    HttpResponse *response = cbData->response;
    HttpRequest *request = cbData->request;
    HttpClient *client = cbData->client;

    response->setResponseCode(static_cast<long>(fetch->status));
    response->setSucceed(false);

    char errBuf[256];
    snprintf(errBuf, sizeof(errBuf), "HTTP fetch failed with status %d for %s", fetch->status, fetch->url);
    response->setErrorBuffer(errBuf);

    auto *mgr = cc::ApplicationManager::getInstance();
    if (mgr != nullptr) {
        auto app = mgr->getCurrentAppSafe();
        if (app != nullptr && app->getEngine() != nullptr) {
            app->getEngine()->getScheduler()->performFunctionInCocosThread([client, request, response]() {
                const ccHttpRequestCallback &callback = request->getResponseCallback();
                if (callback != nullptr) {
                    callback(client, response);
                }
                response->release();
                request->release();
            });
        } else {
            const ccHttpRequestCallback &callback = request->getResponseCallback();
            if (callback != nullptr) {
                callback(client, response);
            }
            response->release();
            request->release();
        }
    }

    delete cbData;
    emscripten_fetch_close(fetch);
}

// ---- HttpClient implementation ----

void HttpClient::networkThread() {
    // Not used on Emscripten - fetch is async
}

void HttpClient::networkThreadAlone(HttpRequest *request, HttpResponse *response) {
    // Not used on Emscripten - fetch is async
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

    delete thiz;

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
    memset(_responseMessage, 0, RESPONSE_BUFFER_SIZE * sizeof(char));
    _scheduler = CC_CURRENT_ENGINE()->getScheduler();
}

HttpClient::~HttpClient() {
    CC_SAFE_RELEASE(_requestSentinel);
    CC_LOG_DEBUG("HttpClient destructor");
}

bool HttpClient::lazyInitThreadSemaphore() {
    _isInited = true;
    return true;
}

void HttpClient::send(HttpRequest *request) {
    if (!request) {
        return;
    }
    sendImmediate(request);
}

void HttpClient::sendImmediate(HttpRequest *request) {
    if (!request) {
        return;
    }

    request->addRef();
    HttpResponse *response = ccnew HttpResponse(request);
    response->addRef();

    // Prepare fetch attributes
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);

    strncpy(attr.requestMethod, httpMethodStr(request->getRequestType()), sizeof(attr.requestMethod) - 1);
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.onsuccess = fetchOnLoad;
    attr.onerror = fetchOnError;
    attr.timeoutMSecs = static_cast<unsigned long>(_timeoutForRead * 1000);

    // Set request body for POST/PUT/PATCH
    const char *requestData = nullptr;
    size_t requestDataSize = 0;
    if (request->getRequestType() == HttpRequest::Type::POST ||
        request->getRequestType() == HttpRequest::Type::PUT ||
        request->getRequestType() == HttpRequest::Type::PATCH) {
        auto *reqData = request->getRequestData();
        if (reqData && !reqData->empty()) {
            requestData = reqData->data();
            requestDataSize = reqData->size();
            attr.requestData = requestData;
            attr.requestDataSize = requestDataSize;
        }
    }

    // Set custom headers
    // emscripten_fetch expects headers as alternating key-value pairs: [key, value, key, value, ..., NULL]
    const auto headers = request->getHeaders();
    ccstd::vector<ccstd::string> headerParts; // Keep strings alive
    ccstd::vector<const char *> headerPtrs;
    if (!headers.empty()) {
        for (const auto &header : headers) {
            auto colonPos = header.find(':');
            if (colonPos != ccstd::string::npos) {
                ccstd::string key = header.substr(0, colonPos);
                ccstd::string value = header.substr(colonPos + 1);
                // Trim leading whitespace from value
                size_t start = value.find_first_not_of(' ');
                if (start != ccstd::string::npos) {
                    value = value.substr(start);
                }
                headerParts.push_back(std::move(key));
                headerParts.push_back(std::move(value));
            }
        }
        for (const auto &part : headerParts) {
            headerPtrs.push_back(part.c_str());
        }
        headerPtrs.push_back(nullptr);
        attr.requestHeaders = headerPtrs.data();
    }

    // Create callback context
    auto *cbData = new FetchCallbackData();
    cbData->client = this;
    cbData->request = request;
    cbData->response = response;
    attr.userData = cbData;

    // Perform the fetch
    emscripten_fetch(&attr, request->getUrl());
}

void HttpClient::dispatchResponseCallbacks() {
    // Not used on Emscripten - callbacks are dispatched via fetch callbacks
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
