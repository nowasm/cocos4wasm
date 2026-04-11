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

#include "EditBox.h"
#include "base/Log.h"

#include <emscripten.h>
#include <emscripten/html5.h>

// Use EM_JS instead of EM_ASM to avoid C-preprocessor issues with commas
// and single quotes inside JavaScript code.

EM_JS(void, js_editbox_show, (int isMultiline, const char *inputTypePtr, const char *defaultValuePtr,
                               int maxLen, int x, int y, int w, int h, int fontSize), {
    var existing = document.getElementById("cocos-editbox");
    if (existing) existing.remove();

    var elem;
    if (isMultiline) {
        elem = document.createElement("textarea");
    } else {
        elem = document.createElement("input");
        var inputType = UTF8ToString(inputTypePtr);
        if (inputType === "password") {
            elem.type = "password";
        } else if (inputType === "email") {
            elem.type = "email";
        } else if (inputType === "number") {
            elem.type = "number";
        } else {
            elem.type = "text";
        }
    }

    elem.id = "cocos-editbox";
    elem.value = UTF8ToString(defaultValuePtr);

    if (maxLen > 0) {
        elem.maxLength = maxLen;
    }

    // Position over the canvas
    var canvas = document.querySelector("#canvas") || document.querySelector("canvas");
    var canvasRect = canvas ? canvas.getBoundingClientRect() : {left: 0, top: 0};

    elem.style.position = "absolute";
    elem.style.left = (canvasRect.left + x) + "px";
    elem.style.top = (canvasRect.top + y) + "px";
    elem.style.width = w + "px";
    elem.style.height = h + "px";
    elem.style.fontSize = fontSize + "px";
    elem.style.border = "1px solid #ccc";
    elem.style.outline = "none";
    elem.style.padding = "2px";
    elem.style.boxSizing = "border-box";
    elem.style.zIndex = "9999";
    elem.style.background = "white";
    elem.style.color = "black";

    document.body.appendChild(elem);
    elem.focus();

    // Input event -> send text back to engine
    elem.addEventListener("input", function() {
        ccall("wasmEditBoxOnInput", null, ["string"], [elem.value]);
    });

    // Enter/confirm handling
    elem.addEventListener("keydown", function(e) {
        if (e.key === "Enter" && !isMultiline) {
            e.preventDefault();
            ccall("wasmEditBoxOnConfirm", null, ["string"], [elem.value]);
        }
    });

    // Blur (lost focus) -> complete
    elem.addEventListener("blur", function() {
        ccall("wasmEditBoxOnComplete", null, ["string"], [elem.value]);
    });
});

EM_JS(void, js_editbox_hide, (), {
    var existing = document.getElementById("cocos-editbox");
    if (existing) {
        existing.remove();
    }
});

EM_JS(void, js_editbox_complete, (), {
    var elem = document.getElementById("cocos-editbox");
    if (elem) {
        ccall("wasmEditBoxOnComplete", null, ["string"], [elem.value]);
    }
});

namespace cc {

bool EditBox::_isShown = false;

static void callJSFunc(const ccstd::string &type, const ccstd::string &text) {
    // EditBox JS callback removed — bindings layer no longer available.
    CC_UNUSED_PARAM(type);
    CC_UNUSED_PARAM(text);
}

void EditBox::show(const ShowInfo &showInfo) {
    if (_isShown) {
        hide();
    }
    _isShown = true;

    js_editbox_show(
        showInfo.isMultiline ? 1 : 0,
        showInfo.inputType.c_str(),
        showInfo.defaultValue.c_str(),
        showInfo.maxLength,
        showInfo.x,
        showInfo.y,
        showInfo.width,
        showInfo.height,
        static_cast<int>(showInfo.fontSize));
}

void EditBox::hide() {
    if (!_isShown) return;
    _isShown = false;
    js_editbox_hide();
}

bool EditBox::complete() {
    if (!_isShown) return true;
    js_editbox_complete();
    hide();
    return true;
}

} // namespace cc

// C-linkage callbacks called from JavaScript via ccall
extern "C" {

EMSCRIPTEN_KEEPALIVE
void wasmEditBoxOnInput(const char *text) {
    cc::callJSFunc("input", text ? text : "");
}

EMSCRIPTEN_KEEPALIVE
void wasmEditBoxOnConfirm(const char *text) {
    cc::callJSFunc("confirm", text ? text : "");
}

EMSCRIPTEN_KEEPALIVE
void wasmEditBoxOnComplete(const char *text) {
    cc::callJSFunc("complete", text ? text : "");
}

}
