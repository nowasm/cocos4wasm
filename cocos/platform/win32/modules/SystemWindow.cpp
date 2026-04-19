/****************************************************************************
 Copyright (c) 2021-2023 Xiamen Yaji Software Co., Ltd.

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

#include "platform/win32/modules/SystemWindow.h"
#include <Windows.h>
#include <imm.h>
#include <functional>
#include "base/Log.h"
#include "engine/EngineEvents.h"
#include "platform/SDLHelper.h"
#include "platform/win32/RawInputHook.h"
#include "platform/win32/WindowsPlatform.h"

// Linked here rather than through CMake so the imm32 dependency stays
// local to the win32 platform module.
#pragma comment(lib, "imm32.lib")

namespace cc {

namespace {

// SDL on Windows associates the window with the IME context by default.
// When a Chinese IME is active (even without SDL_StartTextInput), the
// IME pre-filters WM_KEYDOWN for printable characters — shortcut keys
// like `1`, `A`, `T` never reach our Keyboard bus. Disassociating the
// IME context makes printable keys flow through unchanged. EditBox can
// call reattachIMEContext() later when it wants Chinese input; we hold
// on to the original HIMC so the restore is exact.
//
// GLFW-based engines (axmol, cocos2d-x desktop) don't hit this because
// GLFW never attaches the main window to an IME context; the IME layer
// only sees a dedicated invisible input window. Our SDL-based build has
// to opt out manually.
void detachIMEContextFromWindow(HWND hwnd) {
    if (!hwnd) {
        CC_LOG_WARNING("[win32] IME detach skipped: null HWND");
        return;
    }
    HIMC prev = ImmAssociateContext(hwnd, nullptr);
    CC_LOG_INFO("[win32] IME detached from HWND=%p (prev HIMC=%p)",
                hwnd, prev);
}

}  // namespace

SystemWindow::SystemWindow(uint32_t windowId, void *externalHandle)
: _windowId(windowId) {
    if (externalHandle) {
        _windowHandle = reinterpret_cast<uintptr_t>(externalHandle);
    }
}

SystemWindow::~SystemWindow() {
    _windowHandle = 0;
    _windowId = 0;
}

bool SystemWindow::createWindow(const char *title,
                                int w, int h, int flags) {
    _window = SDLHelper::createWindow(title, w, h, flags);
    if (!_window) {
        return false;
    }

    _width = w;
    _height = h;
    _windowHandle = SDLHelper::getWindowHandle(_window);
    detachIMEContextFromWindow(reinterpret_cast<HWND>(_windowHandle));
    // Install the Raw Input bypass — detaching the IME context handles
    // well-behaved IMEs, Raw Input covers the rest (Sogou, QQ, etc. that
    // install a global WH_KEYBOARD_LL hook).
    RawInputHook::install(_windowHandle);
    return true;
}

bool SystemWindow::createWindow(const char *title,
                                int x, int y, int w,
                                int h, int flags) {
    _window = SDLHelper::createWindow(title, x, y, w, h, flags);
    if (!_window) {
        return false;
    }

    _width = w;
    _height = h;
    _windowHandle = SDLHelper::getWindowHandle(_window);
    detachIMEContextFromWindow(reinterpret_cast<HWND>(_windowHandle));
    // Install the Raw Input bypass — detaching the IME context handles
    // well-behaved IMEs, Raw Input covers the rest (Sogou, QQ, etc. that
    // install a global WH_KEYBOARD_LL hook).
    RawInputHook::install(_windowHandle);
    return true;
}

void SystemWindow::closeWindow() {
    HWND windowHandle = reinterpret_cast<HWND>(getWindowHandle());
    if (windowHandle != 0) {
        ::SendMessageA(windowHandle, WM_CLOSE, 0, 0);
    }
}

uint32_t SystemWindow::getWindowId() const {
    return _windowId;
}

uintptr_t SystemWindow::getWindowHandle() const {
    return _windowHandle;
}

void SystemWindow::setCursorEnabled(bool value) {
    SDLHelper::setCursorEnabled(value);
}

SystemWindow::Size SystemWindow::getViewSize() const {
    return Size{static_cast<float>(_width), static_cast<float>(_height)};
}
} // namespace cc
