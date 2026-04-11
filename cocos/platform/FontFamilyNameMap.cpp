#include "platform/FontFamilyNameMap.h"

static ccstd::unordered_map<ccstd::string, ccstd::string> fontFamilyNameMap;

const ccstd::unordered_map<ccstd::string, ccstd::string> &getFontFamilyNameMap() {
    return fontFamilyNameMap;
}
