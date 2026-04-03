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
#include "cocos/bindings/jswrapper/SeApi.h"
#include "cocos/bindings/manual/jsb_global.h"
#include "base/Log.h"

#include <emscripten.h>

namespace cc {

bool EditBox::_isShown = false;

static se::Value g_textInputCallback;

static void getTextInputCallback() {
    if (!g_textInputCallback.isUndefined())
        return;

    auto global = se::ScriptEngine::getInstance()->getGlobalObject();
    se::Value jsbVal;
    if (global->getProperty("jsb", &jsbVal) && jsbVal.isObject()) {
        jsbVal.toObject()->getProperty("onTextInput", &g_textInputCallback);
        // Free global se::Value before ScriptEngine cleanup
        se::ScriptEngine::getInstance()->addBeforeCleanupHook([]() {
            g_textInputCallback.setUndefined();
        });
    }
}

static void callJSFunc(const ccstd::string &type, const ccstd::string &text) {
    getTextInputCallback();
    if (g_textInputCallback.isUndefined()) return;

    se::AutoHandleScope scope;
    se::ValueArray args;
    args.push_back(se::Value(type));
    args.push_back(se::Value(text));
    g_textInputCallback.toObject()->call(args, nullptr);
}

void EditBox::show(const ShowInfo &showInfo) {
    if (_isShown) {
        hide();
    }
    _isShown = true;

    const char *inputType = showInfo.inputType.c_str();
    const char *defaultValue = showInfo.defaultValue.c_str();
    bool isMultiline = showInfo.isMultiline;
    int maxLength = showInfo.maxLength;
    int x = showInfo.x;
    int y = showInfo.y;
    int width = showInfo.width;
    int height = showInfo.height;
    uint32_t fontSize = showInfo.fontSize;

    // Create an HTML input element overlaying the canvas
    EM_ASM({
        // Remove existing edit box if any
        var existing = document.getElementById('cocos-editbox');
        if (existing) existing.remove();

        var isMultiline = $0;
        var elem;
        if (isMultiline) {
            elem = document.createElement('textarea');
        } else {
            elem = document.createElement('input');
            var inputType = UTF8ToString($1);
            if (inputType === 'password') {
                elem.type = 'password';
            } else if (inputType === 'email') {
                elem.type = 'email';
            } else if (inputType === 'number') {
                elem.type = 'number';
            } else {
                elem.type = 'text';
            }
        }

        elem.id = 'cocos-editbox';
        elem.value = UTF8ToString($2);

        var maxLen = $3;
        if (maxLen > 0) {
            elem.maxLength = maxLen;
        }

        // Position over the canvas
        var canvas = document.querySelector('#canvas') || document.querySelector('canvas');
        var canvasRect = canvas ? canvas.getBoundingClientRect() : { left: 0, top: 0 };

        elem.style.position = 'absolute';
        elem.style.left = (canvasRect.left + $4) + 'px';
        elem.style.top = (canvasRect.top + $5) + 'px';
        elem.style.width = $6 + 'px';
        elem.style.height = $7 + 'px';
        elem.style.fontSize = $8 + 'px';
        elem.style.border = '1px solid #ccc';
        elem.style.outline = 'none';
        elem.style.padding = '2px';
        elem.style.boxSizing = 'border-box';
        elem.style.zIndex = '9999';
        elem.style.background = 'white';
        elem.style.color = 'black';

        document.body.appendChild(elem);
        elem.focus();

        // Input event -> send text back to engine
        elem.addEventListener('input', function() {
            var text = elem.value;
            // Call the native callback
            if (Module._editboxInputCallback) {
                Module._editboxInputCallback(text);
            }
        });

        // Enter/confirm handling
        elem.addEventListener('keydown', function(e) {
            if (e.key === 'Enter' && !isMultiline) {
                e.preventDefault();
                var text = elem.value;
                if (Module._editboxConfirmCallback) {
                    Module._editboxConfirmCallback(text);
                }
            }
        });

        // Blur (lost focus) -> complete
        elem.addEventListener('blur', function() {
            var text = elem.value;
            if (Module._editboxCompleteCallback) {
                Module._editboxCompleteCallback(text);
            }
        });

        // Store callbacks from C++
        Module._editboxInputCallback = function(text) {
            ccall('wasmEditBoxOnInput', null, ['string'], [text]);
        };
        Module._editboxConfirmCallback = function(text) {
            ccall('wasmEditBoxOnConfirm', null, ['string'], [text]);
        };
        Module._editboxCompleteCallback = function(text) {
            ccall('wasmEditBoxOnComplete', null, ['string'], [text]);
        };

    }, isMultiline, inputType, defaultValue, maxLength, x, y, width, height, fontSize);
}

void EditBox::hide() {
    if (!_isShown) return;
    _isShown = false;

    EM_ASM({
        var existing = document.getElementById('cocos-editbox');
        if (existing) {
            existing.remove();
        }
        Module._editboxInputCallback = null;
        Module._editboxConfirmCallback = null;
        Module._editboxCompleteCallback = null;
    });
}

bool EditBox::complete() {
    if (!_isShown) return true;

    // Get the current text and send complete event
    EM_ASM({
        var elem = document.getElementById('cocos-editbox');
        if (elem) {
            var text = elem.value;
            ccall('wasmEditBoxOnComplete', null, ['string'], [text]);
        }
    });

    hide();
    return true;
}

} // namespace cc

// C-linkage callbacks called from JavaScript
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
