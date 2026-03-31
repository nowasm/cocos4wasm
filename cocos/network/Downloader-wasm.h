#pragma once

#include "network/DownloaderImpl.h"

namespace cc {
namespace network {

struct DownloaderHints;

class DownloaderWasm : public IDownloaderImpl {
public:
    explicit DownloaderWasm(const DownloaderHints &hints);
    ~DownloaderWasm() override;

    IDownloadTask *createCoTask(std::shared_ptr<const DownloadTask> &task) override;
    void abort(const std::unique_ptr<IDownloadTask> &task) override;

private:
};

} // namespace network
} // namespace cc
