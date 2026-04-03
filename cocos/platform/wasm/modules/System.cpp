/****************************************************************************
 Copyright (c) 2022-2023 Xiamen Yaji Software Co., Ltd.

 http://www.cocos.com

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights to
 use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 of the Software, and to permit persons to whom the Software is furnished to do so,
 subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
****************************************************************************/

#include "platform/wasm/modules/System.h"
#include <string.h>
#include <emscripten.h>
#include <emscripten/html5.h>

namespace cc {
using OSType = System::OSType;

OSType System::getOSType() const {
    return OSType::LINUX;
}

ccstd::string System::getDeviceModel() const {
    return "Wasm";
}

System::LanguageType System::getCurrentLanguage() const {
    // Get browser language via JavaScript
    char lang[16] = {0};
    EM_ASM({
        var lang = (navigator.language || navigator.userLanguage || "en").substring(0, 2).toLowerCase();
        stringToUTF8(lang, $0, 16);
    }, lang);
    return getLanguageTypeByISO2(lang);
}

ccstd::string System::getCurrentLanguageCode() const {
    char langCode[16] = {0};
    EM_ASM({
        var lang = (navigator.language || navigator.userLanguage || "en").substring(0, 5);
        stringToUTF8(lang, $0, 16);
    }, langCode);
    return ccstd::string(langCode);
}

ccstd::string System::getSystemVersion() const {
    return "wasm";
}

bool System::openURL(const ccstd::string &url) {
    EM_ASM({
        window.open(UTF8ToString($0), '_blank');
    }, url.c_str());
    return true;
}

System::LanguageType System::getLanguageTypeByISO2(const char *code) const {
    LanguageType ret = LanguageType::ENGLISH;
    if (strcmp(code, "zh") == 0) {
        ret = LanguageType::CHINESE;
    } else if (strcmp(code, "en") == 0) {
        ret = LanguageType::ENGLISH;
    } else if (strcmp(code, "fr") == 0) {
        ret = LanguageType::FRENCH;
    } else if (strcmp(code, "it") == 0) {
        ret = LanguageType::ITALIAN;
    } else if (strcmp(code, "de") == 0) {
        ret = LanguageType::GERMAN;
    } else if (strcmp(code, "es") == 0) {
        ret = LanguageType::SPANISH;
    } else if (strcmp(code, "nl") == 0) {
        ret = LanguageType::DUTCH;
    } else if (strcmp(code, "ru") == 0) {
        ret = LanguageType::RUSSIAN;
    } else if (strcmp(code, "ko") == 0) {
        ret = LanguageType::KOREAN;
    } else if (strcmp(code, "ja") == 0) {
        ret = LanguageType::JAPANESE;
    } else if (strcmp(code, "hu") == 0) {
        ret = LanguageType::HUNGARIAN;
    } else if (strcmp(code, "pt") == 0) {
        ret = LanguageType::PORTUGUESE;
    } else if (strcmp(code, "ar") == 0) {
        ret = LanguageType::ARABIC;
    } else if (strcmp(code, "nb") == 0) {
        ret = LanguageType::NORWEGIAN;
    } else if (strcmp(code, "pl") == 0) {
        ret = LanguageType::POLISH;
    } else if (strcmp(code, "tr") == 0) {
        ret = LanguageType::TURKISH;
    } else if (strcmp(code, "uk") == 0) {
        ret = LanguageType::UKRAINIAN;
    } else if (strcmp(code, "ro") == 0) {
        ret = LanguageType::ROMANIAN;
    } else if (strcmp(code, "bg") == 0) {
        ret = LanguageType::BULGARIAN;
    } else if (strcmp(code, "hi") == 0) {
        ret = LanguageType::HINDI;
    }
    return ret;
}

void System::copyTextToClipboard(const ccstd::string &text) {
    EM_ASM({
        var text = UTF8ToString($0);
        if (navigator.clipboard && navigator.clipboard.writeText) {
            navigator.clipboard.writeText(text);
        } else {
            // Fallback: create a temporary textarea element
            var textarea = document.createElement('textarea');
            textarea.value = text;
            textarea.style.position = 'fixed';
            textarea.style.opacity = '0';
            document.body.appendChild(textarea);
            textarea.select();
            document.execCommand('copy');
            document.body.removeChild(textarea);
        }
    }, text.c_str());
}

} // namespace cc
