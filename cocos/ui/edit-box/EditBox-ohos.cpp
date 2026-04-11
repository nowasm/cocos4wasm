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

#include "EditBox.h"
#include "engine/EngineEvents.h"
//#include "platform/Application.h"
#include "platform/java/jni/JniHelper.h"

#ifndef JCLS_EDITBOX
    #define JCLS_EDITBOX "com/cocos/lib/CocosEditBoxAbility"
#endif

#ifndef ORG_EDITBOX_CLASS_NAME
    #define ORG_EDITBOX_CLASS_NAME com_cocos_lib_CocosEditBoxAbility
#endif
#define JNI_EDITBOX(FUNC) JNI_METHOD1(ORG_EDITBOX_CLASS_NAME, FUNC)

namespace {

void callJSFunc(const ccstd::string &type, const ccstd::string &text) {
    // EditBox JS callback removed — bindings layer no longer available.
    CC_UNUSED_PARAM(type);
    CC_UNUSED_PARAM(text);
}
} // namespace

namespace cc {

bool EditBox::_isShown = false; //NOLINT

void EditBox::show(const cc::EditBox::ShowInfo &showInfo) {
    JniHelper::callStaticVoidMethod(JCLS_EDITBOX,
                                    "showNative",
                                    showInfo.defaultValue,
                                    showInfo.maxLength,
                                    showInfo.isMultiline,
                                    showInfo.confirmHold,
                                    showInfo.confirmType,
                                    showInfo.inputType);
    _isShown = true;
}

void EditBox::hide() {
    JniHelper::callStaticVoidMethod(JCLS_EDITBOX, "hideNative");
    _isShown = false;
}

bool EditBox::complete() {
    if (!_isShown) {
        return false;
    }

    EditBox::hide();

    return true;
}

} // namespace cc

extern "C" {
JNIEXPORT void JNICALL JNI_EDITBOX(onKeyboardInputNative)(JNIEnv * /*env*/, jclass /*unused*/, jstring text) {
    auto textStr = cc::JniHelper::jstring2string(text);
    callJSFunc("input", textStr);
}

JNIEXPORT void JNICALL JNI_EDITBOX(onKeyboardCompleteNative)(JNIEnv * /*env*/, jclass /*unused*/, jstring text) {
    auto textStr = cc::JniHelper::jstring2string(text);
    callJSFunc("complete", textStr);
}

JNIEXPORT void JNICALL JNI_EDITBOX(onKeyboardConfirmNative)(JNIEnv * /*env*/, jclass /*unused*/, jstring text) {
    auto textStr = cc::JniHelper::jstring2string(text);
    callJSFunc("confirm", textStr);
}
}
