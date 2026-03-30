/****************************************************************************
 Copyright (c) 2021-2023 Xiamen Yaji Software Co., Ltd.

 Emscripten MEMFS/IDBFS friendly paths; no /proc or native home dirs.
****************************************************************************/

#include "platform/wasm/FileUtils-wasm.h"
#include <sys/stat.h>
#include <unistd.h>
#include "base/memory/Memory.h"

namespace cc {

FileUtils *createFileUtils() {
    return ccnew FileUtilsWasm();
}

FileUtilsWasm::FileUtilsWasm() {
    init();
}

bool FileUtilsWasm::init() {
    _defaultResRootPath = "/";
    _writablePath = "/data/";
    return FileUtils::init();
}

bool FileUtilsWasm::isFileExistInternal(const ccstd::string &filename) const {
    if (filename.empty()) {
        return false;
    }
    ccstd::string path = filename;
    if (!isAbsolutePath(path)) {
        path.insert(0, _defaultResRootPath);
    }
    struct stat sts {};
    return (stat(path.c_str(), &sts) == 0) && S_ISREG(sts.st_mode);
}

ccstd::string FileUtilsWasm::getWritablePath() const {
    if (mkdir(_writablePath.c_str(), 0755) != 0) {
        struct stat st {};
        if (stat(_writablePath.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
            // Still return path; caller may use MEMFS root
        }
    }
    return _writablePath;
}

} // namespace cc
