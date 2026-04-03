/****************************************************************************
 Emscripten: uses emscripten_fetch API for async file downloads.
****************************************************************************/

#include "network/Downloader-wasm.h"

#include "network/Downloader.h"
#include "base/memory/Memory.h"
#include "base/Log.h"

#include <emscripten/fetch.h>

namespace cc {
namespace network {

namespace {

class DownloadTaskWasm final : public IDownloadTask {
public:
    emscripten_fetch_t *fetch{nullptr};
    std::shared_ptr<const DownloadTask> ownerTask;
};

struct FetchDownloadData {
    DownloaderWasm *downloader{nullptr};
    DownloadTaskWasm *coTask{nullptr};
    std::shared_ptr<const DownloadTask> task;
};

} // namespace

DownloaderWasm::DownloaderWasm(const DownloaderHints &hints) {
    CC_UNUSED_PARAM(hints);
}

DownloaderWasm::~DownloaderWasm() = default;

IDownloadTask *DownloaderWasm::createCoTask(std::shared_ptr<const DownloadTask> &task) {
    auto *coTask = ccnew DownloadTaskWasm();
    coTask->ownerTask = task;

    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "GET");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;

    auto *cbData = new FetchDownloadData();
    cbData->downloader = this;
    cbData->coTask = coTask;
    cbData->task = task;
    attr.userData = cbData;

    attr.onprogress = [](emscripten_fetch_t *fetch) {
        auto *data = reinterpret_cast<FetchDownloadData *>(fetch->userData);
        if (!data || !data->downloader) return;

        if (data->downloader->onTaskProgress) {
            uint32_t bytesReceived = static_cast<uint32_t>(fetch->numBytes);
            uint32_t totalBytesReceived = static_cast<uint32_t>(fetch->dataOffset + fetch->numBytes);
            uint32_t totalBytesExpected = 0;
            if (fetch->totalBytes > 0) {
                totalBytesExpected = static_cast<uint32_t>(fetch->totalBytes);
            }
            std::function<uint32_t(void *buffer, uint32_t len)> nullTransfer;
            data->downloader->onTaskProgress(*data->task, bytesReceived, totalBytesReceived, totalBytesExpected, nullTransfer);
        }
    };

    attr.onsuccess = [](emscripten_fetch_t *fetch) {
        auto *data = reinterpret_cast<FetchDownloadData *>(fetch->userData);
        if (!data || !data->downloader) {
            delete data;
            emscripten_fetch_close(fetch);
            return;
        }

        // Check if this is a file download (storagePath not empty)
        const auto &storagePath = data->task->storagePath;
        if (!storagePath.empty()) {
            // Write to virtual filesystem
            FILE *fp = fopen(storagePath.c_str(), "wb");
            if (fp) {
                fwrite(fetch->data, 1, static_cast<size_t>(fetch->numBytes), fp);
                fclose(fp);
                if (data->downloader->onTaskFinish) {
                    data->downloader->onTaskFinish(*data->task, 0, 0, "", {});
                }
            } else {
                if (data->downloader->onTaskFinish) {
                    ccstd::string errMsg = "Failed to write file: " + storagePath;
                    data->downloader->onTaskFinish(*data->task,
                                                    DownloadTask::ERROR_FILE_OP_FAILED,
                                                    0,
                                                    errMsg,
                                                    {});
                }
            }
        } else {
            // In-memory download
            ccstd::vector<unsigned char> buf;
            if (fetch->numBytes > 0 && fetch->data) {
                buf.assign(reinterpret_cast<const unsigned char *>(fetch->data),
                           reinterpret_cast<const unsigned char *>(fetch->data) + fetch->numBytes);
            }
            if (data->downloader->onTaskFinish) {
                data->downloader->onTaskFinish(*data->task, 0, 0, "", buf);
            }
        }

        delete data;
        emscripten_fetch_close(fetch);
    };

    attr.onerror = [](emscripten_fetch_t *fetch) {
        auto *data = reinterpret_cast<FetchDownloadData *>(fetch->userData);
        if (!data || !data->downloader) {
            delete data;
            emscripten_fetch_close(fetch);
            return;
        }

        char errBuf[256];
        snprintf(errBuf, sizeof(errBuf), "Download failed with HTTP status %d", fetch->status);

        if (data->downloader->onTaskFinish) {
            data->downloader->onTaskFinish(*data->task,
                                            DownloadTask::ERROR_IMPL_INTERNAL,
                                            fetch->status,
                                            errBuf,
                                            {});
        }

        delete data;
        emscripten_fetch_close(fetch);
    };

    coTask->fetch = emscripten_fetch(&attr, task->requestURL.c_str());
    return coTask;
}

void DownloaderWasm::abort(const std::unique_ptr<IDownloadTask> &task) {
    auto *wasmTask = static_cast<DownloadTaskWasm *>(task.get());
    if (wasmTask && wasmTask->fetch) {
        // emscripten_fetch doesn't have a direct abort API,
        // but closing the fetch handle will effectively cancel it.
        // The callbacks may still fire but we can guard against that.
    }
}

} // namespace network
} // namespace cc
