/****************************************************************************
 Copyright (c) 2021-2023 Xiamen Yaji Software Co., Ltd.
****************************************************************************/

#pragma once

#include "platform/FileUtils.h"

namespace cc {

class CC_DLL FileUtilsWasm : public FileUtils {
public:
    FileUtilsWasm();
    ~FileUtilsWasm() override = default;

    bool isFileExistInternal(const ccstd::string &filename) const override;
    ccstd::string getWritablePath() const override;
    bool init() override;

private:
    ccstd::string _writablePath;
};

} // namespace cc
