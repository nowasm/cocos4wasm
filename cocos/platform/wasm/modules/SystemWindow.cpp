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

#include "platform/wasm/modules/SystemWindow.h"
#include "base/Log.h"
#include "base/Macros.h"
#include "engine/EngineEvents.h"
#include <emscripten/html5.h>

namespace cc {

namespace {

bool tryGetCanvasSize(const char *target, int *width, int *height) {
    return emscripten_get_canvas_element_size(target, width, height) == EMSCRIPTEN_RESULT_SUCCESS;
}

// The window ID used by the single WASM window.
static uint32_t sWindowId = 0;

// ---- Keyboard key mapping: DOM keyCode -> Cocos KeyCode ----
static int domKeyCodeToCocosKeyCode(int domKeyCode, const char *keyStr) {
    // DOM key codes mostly match Cocos KeyCode values for common keys.
    // Handle special mappings for numpad and right-side modifier keys.
    if (keyStr) {
        // Right-side modifiers (DOM uses same keyCode as left side)
        // We check the "code" string from the DOM to disambiguate.
        if (strcmp(keyStr, "ShiftRight") == 0) return 20016;
        if (strcmp(keyStr, "ControlRight") == 0) return 20017;
        if (strcmp(keyStr, "AltRight") == 0) return 20018;
        if (strcmp(keyStr, "MetaRight") == 0) return 93;
        if (strcmp(keyStr, "ContextMenu") == 0) return 20093;
        if (strcmp(keyStr, "PrintScreen") == 0) return 20094;
        if (strcmp(keyStr, "NumpadEnter") == 0) return 20013;
        // Numpad digits
        if (strcmp(keyStr, "Numpad0") == 0) return 10048;
        if (strcmp(keyStr, "Numpad1") == 0) return 10049;
        if (strcmp(keyStr, "Numpad2") == 0) return 10050;
        if (strcmp(keyStr, "Numpad3") == 0) return 10051;
        if (strcmp(keyStr, "Numpad4") == 0) return 10052;
        if (strcmp(keyStr, "Numpad5") == 0) return 10053;
        if (strcmp(keyStr, "Numpad6") == 0) return 10054;
        if (strcmp(keyStr, "Numpad7") == 0) return 10055;
        if (strcmp(keyStr, "Numpad8") == 0) return 10056;
        if (strcmp(keyStr, "Numpad9") == 0) return 10057;
    }
    // For most keys, DOM keyCode matches Cocos KeyCode directly
    // (A-Z = 65-90, 0-9 = 48-57, F1-F12 = 112-123, arrows, etc.)
    return domKeyCode;
}

// ---- Emscripten keyboard callbacks ----
static EM_BOOL onKeyCallback(int eventType, const EmscriptenKeyboardEvent *e, void * /*userData*/) {
    KeyboardEvent event;
    event.windowId = sWindowId;
    event.key = domKeyCodeToCocosKeyCode(static_cast<int>(e->keyCode), e->code);
    event.altKeyActive = e->altKey;
    event.ctrlKeyActive = e->ctrlKey;
    event.metaKeyActive = e->metaKey;
    event.shiftKeyActive = e->shiftKey;
    event.code = e->code;

    if (eventType == EMSCRIPTEN_EVENT_KEYDOWN) {
        event.action = KeyboardEvent::Action::PRESS;
    } else if (eventType == EMSCRIPTEN_EVENT_KEYUP) {
        event.action = KeyboardEvent::Action::RELEASE;
    } else {
        event.action = KeyboardEvent::Action::UNKNOWN;
    }

    events::Keyboard::broadcast(event);
    return EM_FALSE; // Don't consume the event (allow default browser behavior for some keys)
}

// ---- Emscripten mouse callbacks ----
static EM_BOOL onMouseCallback(int eventType, const EmscriptenMouseEvent *e, void * /*userData*/) {
    MouseEvent event;
    event.windowId = sWindowId;
    event.x = static_cast<float>(e->targetX);
    event.y = static_cast<float>(e->targetY);
    event.xDelta = static_cast<float>(e->movementX);
    event.yDelta = static_cast<float>(e->movementY);
    event.button = static_cast<uint16_t>(e->button);

    switch (eventType) {
        case EMSCRIPTEN_EVENT_MOUSEDOWN:
            event.type = MouseEvent::Type::DOWN;
            break;
        case EMSCRIPTEN_EVENT_MOUSEUP:
            event.type = MouseEvent::Type::UP;
            break;
        case EMSCRIPTEN_EVENT_MOUSEMOVE:
            event.type = MouseEvent::Type::MOVE;
            break;
        default:
            event.type = MouseEvent::Type::UNKNOWN;
            break;
    }

    events::Mouse::broadcast(event);
    return EM_TRUE;
}

static EM_BOOL onWheelCallback(int /*eventType*/, const EmscriptenWheelEvent *e, void * /*userData*/) {
    MouseEvent event;
    event.windowId = sWindowId;
    event.x = static_cast<float>(e->mouse.targetX);
    event.y = static_cast<float>(e->mouse.targetY);
    event.xDelta = static_cast<float>(e->deltaX);
    event.yDelta = static_cast<float>(-e->deltaY); // Negate to match engine convention (scroll up = positive)
    event.button = static_cast<uint16_t>(e->mouse.button);
    event.type = MouseEvent::Type::WHEEL;

    events::Mouse::broadcast(event);
    return EM_TRUE;
}

// ---- Emscripten touch callbacks ----
static EM_BOOL onTouchCallback(int eventType, const EmscriptenTouchEvent *e, void * /*userData*/) {
    TouchEvent event;
    event.windowId = sWindowId;

    for (int i = 0; i < e->numTouches; ++i) {
        const EmscriptenTouchPoint &tp = e->touches[i];
        if (!tp.isChanged) continue;
        event.touches.emplace_back(
            static_cast<float>(tp.targetX),
            static_cast<float>(tp.targetY),
            tp.identifier);
    }

    switch (eventType) {
        case EMSCRIPTEN_EVENT_TOUCHSTART:
            event.type = TouchEvent::Type::BEGAN;
            break;
        case EMSCRIPTEN_EVENT_TOUCHMOVE:
            event.type = TouchEvent::Type::MOVED;
            break;
        case EMSCRIPTEN_EVENT_TOUCHEND:
            event.type = TouchEvent::Type::ENDED;
            break;
        case EMSCRIPTEN_EVENT_TOUCHCANCEL:
            event.type = TouchEvent::Type::CANCELLED;
            break;
        default:
            event.type = TouchEvent::Type::UNKNOWN;
            break;
    }

    events::Touch::broadcast(event);
    return EM_TRUE;
}

// ---- Emscripten resize callback ----
static EM_BOOL onResizeCallback(int /*eventType*/, const EmscriptenUiEvent * /*e*/, void * /*userData*/) {
    int w = 0, h = 0;
    if (tryGetCanvasSize("#canvas", &w, &h) ||
        tryGetCanvasSize("canvas", &w, &h) ||
        tryGetCanvasSize("#GameCanvas", &w, &h) ||
        tryGetCanvasSize("GameCanvas", &w, &h)) {
        cc::WindowEvent ev;
        ev.type = cc::WindowEvent::Type::SIZE_CHANGED;
        ev.width = w;
        ev.height = h;
        ev.windowId = sWindowId;
        events::WindowEvent::broadcast(ev);
        events::Resize::broadcast(w, h, sWindowId);
    }
    return EM_TRUE;
}

} // namespace

SystemWindow::SystemWindow(uint32_t windowId, void* externalHandle)
: _windowId(windowId) {
    if (externalHandle) {
        _windowHandle = reinterpret_cast<uintptr_t>(externalHandle);
    }
}

SystemWindow::~SystemWindow() = default;

uintptr_t SystemWindow::getWindowHandle() const {
    return _windowHandle;
}

uint32_t SystemWindow::getWindowId() const {
    return _windowId;
}

void SystemWindow::setCursorEnabled(bool value) {
    if (!value) {
        emscripten_request_pointerlock("#canvas", EM_TRUE);
    } else {
        emscripten_exit_pointerlock();
    }
}

int SystemWindow::init() {
    sWindowId = _windowId;

    const char *canvasId = "#canvas";

    // Register keyboard events on the document (not canvas) so they work even without focus
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr, EM_TRUE, onKeyCallback);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr, EM_TRUE, onKeyCallback);

    // Register mouse events on the canvas
    emscripten_set_mousedown_callback(canvasId, nullptr, EM_TRUE, onMouseCallback);
    emscripten_set_mouseup_callback(canvasId, nullptr, EM_TRUE, onMouseCallback);
    emscripten_set_mousemove_callback(canvasId, nullptr, EM_TRUE, onMouseCallback);
    emscripten_set_wheel_callback(canvasId, nullptr, EM_TRUE, onWheelCallback);

    // Register touch events on the canvas
    emscripten_set_touchstart_callback(canvasId, nullptr, EM_TRUE, onTouchCallback);
    emscripten_set_touchmove_callback(canvasId, nullptr, EM_TRUE, onTouchCallback);
    emscripten_set_touchend_callback(canvasId, nullptr, EM_TRUE, onTouchCallback);
    emscripten_set_touchcancel_callback(canvasId, nullptr, EM_TRUE, onTouchCallback);

    // Register resize event
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, onResizeCallback);

    return 0;
}

void SystemWindow::pollEvent(bool* quit) {
    // Emscripten events are dispatched asynchronously via registered callbacks.
    // No manual polling needed.
}

SystemWindow::Size SystemWindow::getViewSize() const {
    int width = _width;
    int height = _height;
    if (!tryGetCanvasSize("#canvas", &width, &height) &&
        !tryGetCanvasSize("canvas", &width, &height) &&
        !tryGetCanvasSize("#GameCanvas", &width, &height) &&
        !tryGetCanvasSize("GameCanvas", &width, &height)) {
        width = _width;
        height = _height;
    }
    return Size{static_cast<float>(width), static_cast<float>(height)};
}

bool SystemWindow::createWindow(const char *title, int x, int y, int w, int h, int flags) {
    CC_UNUSED_PARAM(title);
    CC_UNUSED_PARAM(x);
    CC_UNUSED_PARAM(y);
    CC_UNUSED_PARAM(flags);
    _width = w;
    _height = h;

    init();

    return true;
}

bool SystemWindow::createWindow(const char *title, int w, int h, int flags) {
    return createWindow(title, 0, 0, w, h, flags);
}

} // namespace cc
