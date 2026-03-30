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

#include "platform/wasm/WasmPlatform.h"
#include "base/memory/Memory.h"
#include "modules/Accelerometer.h"
#include "modules/Battery.h"
#include "modules/Network.h"
#include "modules/Screen.h"
#include "modules/System.h"
#include "modules/SystemWindowManager.h"
#include "modules/SystemWindow.h"
#include "modules/Vibrator.h"
#include "platform/interfaces/OSInterface.h"

#include <emscripten.h>

namespace cc {

WasmPlatform::WasmPlatform() = default;

WasmPlatform::~WasmPlatform() = default;

int32_t WasmPlatform::init() {
    registerInterface(std::make_shared<Accelerometer>());
    registerInterface(std::make_shared<Battery>());
    registerInterface(std::make_shared<Network>());
    registerInterface(std::make_shared<Screen>());
    registerInterface(std::make_shared<System>());
    registerInterface(std::make_shared<SystemWindowManager>());
    registerInterface(std::make_shared<Vibrator>());
    return 0;
}

int32_t WasmPlatform::loop() {
    // fps=0: rAF; simulate_infinite_loop=1: keep runtime alive after main returns (browser HTML build).
    emscripten_set_main_loop([]() {
        static_cast<UniversalPlatform *>(cc::BasePlatform::getPlatform())->runTask();
    }, 0, 1);
    return 0;
}

void WasmPlatform::exit() {
    emscripten_cancel_main_loop();
    onDestroy();
}

ISystemWindow *WasmPlatform::createNativeWindow(uint32_t windowId, void *externalHandle) {
    return ccnew SystemWindow(windowId, externalHandle);
}

} // namespace cc
