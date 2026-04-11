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

#include "platform/wasm/modules/Screen.h"
#include "base/Macros.h"

#include <emscripten.h>

namespace cc {

int Screen::getDPI() const {
    // Use devicePixelRatio to estimate DPI (96 is the standard CSS DPI)
    double dpr = emscripten_get_device_pixel_ratio();
    return static_cast<int>(96.0 * dpr);
}

float Screen::getDevicePixelRatio() const {
    return static_cast<float>(emscripten_get_device_pixel_ratio());
}

void Screen::setKeepScreenOn(bool value) {
    CC_UNUSED_PARAM(value);
    // Not applicable for web browsers
}

Screen::Orientation Screen::getDeviceOrientation() const {
    int orientation = EM_ASM_INT({
        // 0 = portrait, 90/-90 = landscape
        if (window.screen && window.screen.orientation) {
            var angle = window.screen.orientation.angle || 0;
            return angle;
        }
        return window.orientation || 0;
    });

    if (orientation == 0 || orientation == 180) {
        return Orientation::PORTRAIT;
    }
    return Orientation::LANDSCAPE_RIGHT;
}

Vec4 Screen::getSafeAreaEdge() const {
    return cc::Vec4();
}

bool Screen::isDisplayStats() {
    se::AutoHandleScope hs;
    se::Value ret;
    char commandBuf[100] = "cc.profiler.isShowingStats();";
    se::ScriptEngine::getInstance()->evalString(commandBuf, 100, &ret);
    return ret.toBoolean();
}

void Screen::setDisplayStats(bool isShow) {
    se::AutoHandleScope hs;
    char commandBuf[100] = {0};
    sprintf(commandBuf, isShow ? "cc.profiler.showStats();" : "cc.profiler.hideStats();");
    se::ScriptEngine::getInstance()->evalString(commandBuf);
}

} // namespace cc
