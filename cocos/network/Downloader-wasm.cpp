#include "network/Downloader-wasm.h"

#include "network/Downloader.h"
#include "base/memory/Memory.h"

namespace cc {
namespace network {

namespace {
class DownloadTaskWasm final : public IDownloadTask {
};
} // namespace

DownloaderWasm::DownloaderWasm(const DownloaderHints &hints) {
    CC_UNUSED_PARAM(hints);
}

DownloaderWasm::~DownloaderWasm() = default;

IDownloadTask *DownloaderWasm::createCoTask(std::shared_ptr<const DownloadTask> &task) {
    auto *coTask = ccnew DownloadTaskWasm();
    if (onTaskFinish) {
        onTaskFinish(*task,
                     DownloadTask::ERROR_IMPL_INTERNAL,
                     DownloadTask::ERROR_IMPL_INTERNAL,
                     "Downloader is not implemented for Emscripten.",
                     {});
    }
    return coTask;
}

void DownloaderWasm::abort(const std::unique_ptr<IDownloadTask> & /*task*/) {
}

} // namespace network
} // namespace cc
