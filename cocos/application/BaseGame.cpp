/****************************************************************************
 Copyright (c) 2017-2023 Xiamen Yaji Software Co., Ltd.

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

#include "BaseGame.h"
#include <string>
#include "ApplicationManager.h"
#include "platform/FileUtils.h"
#include "platform/interfaces/modules/ISystemWindowManager.h"
#include "renderer/pipeline/GlobalDescriptorSetManager.h"

#if CC_PLATFORM == CC_PLATFORM_EMSCRIPTEN
    #include "core/Root.h"
    #include "core/builtin/BuiltinResMgr.h"
    #include "core/builtin/BuiltinEffectLoader.h"
    #include "renderer/GFXDeviceManager.h"
    #include "renderer/pipeline/forward/ForwardPipeline.h"
#endif

#if CC_PLATFORM == CC_PLATFORM_ANDROID
    #include "platform/android/adpf_manager.h"
#endif
extern "C" void cc_load_all_plugins(); // NOLINT

namespace cc {

BaseGame::~BaseGame() { // NOLINT
#if (CC_PLATFORM == CC_PLATFORM_ANDROID) && CC_SUPPORT_ADPF
    ADPFManager::getInstance().destroy();
#endif
}

int BaseGame::init() {
    cc::pipeline::GlobalDSManager::setDescriptorSetLayout();

    cc_load_all_plugins();

#if (CC_PLATFORM == CC_PLATFORM_ANDROID) && CC_SUPPORT_ADPF
    ADPFManager::getInstance().initialize();
#endif

#if CC_PLATFORM == CC_PLATFORM_WINDOWS || CC_PLATFORM == CC_PLATFORM_LINUX || CC_PLATFORM == CC_PLATFORM_QNX || CC_PLATFORM == CC_PLATFORM_MACOS
    // override default value
    //_windowInfo.x      = _windowInfo.x == -1 ? 0 : _windowInfo.x;
    //_windowInfo.y      = _windowInfo.y == -1 ? 0 : _windowInfo.y;
    _windowInfo.width = _windowInfo.width == -1 ? 800 : _windowInfo.width;
    _windowInfo.height = _windowInfo.height == -1 ? 600 : _windowInfo.height;
    _windowInfo.flags = _windowInfo.flags == -1 ? cc::ISystemWindow::CC_WINDOW_SHOWN |
                                                      cc::ISystemWindow::CC_WINDOW_RESIZABLE |
                                                      cc::ISystemWindow::CC_WINDOW_INPUT_FOCUS
                                                : _windowInfo.flags;
    std::call_once(_windowCreateFlag, [&]() {
        ISystemWindowInfo info;
        info.title = _windowInfo.title;
    #if CC_PLATFORM == CC_PLATFORM_WINDOWS
        info.x = _windowInfo.x == -1 ? 50 : _windowInfo.x; // 50 meams move window a little for now
        info.y = _windowInfo.y == -1 ? 50 : _windowInfo.y; // same above
    #else
        info.x = _windowInfo.x == -1 ? 0 : _windowInfo.x;
        info.y = _windowInfo.y == -1 ? 0 : _windowInfo.y;
    #endif
        info.width = _windowInfo.width;
        info.height = _windowInfo.height;
        info.flags = _windowInfo.flags;

        ISystemWindowManager* windowMgr = CC_GET_PLATFORM_INTERFACE(ISystemWindowManager);
        windowMgr->createWindow(info);
    });

#endif

    if (_debuggerInfo.enabled) {
        setDebugIpAndPort(_debuggerInfo.address, _debuggerInfo.port, _debuggerInfo.pauseOnStart);
    }

    int ret = cc::CocosApplication::init();
    if (ret != 0) {
        return ret;
    }

    setXXTeaKey(_xxteaKey);
#if CC_PLATFORM != CC_PLATFORM_EMSCRIPTEN
    runScript("jsb-adapter/web-adapter.js");
#else
    // Standalone WASM mode: initialise Root + render pipeline from C++ since
    // there is no Creator JS bootstrap (web-adapter.js / application.js) to do it.
    {
        auto *device = gfx::Device::getInstance();
        CC_ASSERT(device);
        BuiltinResMgr::getInstance()->initBuiltinRes();
        // Load precompiled builtin effects (shaders + materials) so materials
        // can resolve their passes.  Must happen before pipeline activation.
        int effectCount = loadBuiltinEffectsFromJson("builtin-effects.json");
        CC_LOG_INFO("Loaded %d builtin effects for standalone WASM mode", effectCount);
        auto *root = ccnew Root(device);
        root->initialize(nullptr);
        // Use the legacy ForwardPipeline — NativePipeline (nullptr) requires a
        // fully populated program library which only Creator builds provide.
        auto *pipeline = ccnew pipeline::ForwardPipeline();
        pipeline->initialize({});
        root->setRenderPipeline(pipeline);
    }
#endif
    // Quick diagnostic: check JS globals before running main.js
    {
        auto *se = se::ScriptEngine::getInstance();
        se::Value ret;
        se->evalString("(typeof globalThis.cc !== 'undefined') ? 'cc exists, facade=' + !!globalThis.cc.__wasm32Facade : 'cc MISSING'", 0, &ret);
        CC_LOG_INFO("BaseGame: JS diagnostic: %s", ret.isString() ? ret.toString().c_str() : "(not string)");
        se->evalString("console.log('[BaseGame-JS] hello from evalString')", 0, nullptr);
    }
    CC_LOG_INFO("BaseGame: about to runScript main.js");
    bool scriptOk = false;
    auto *fileUtils = FileUtils::getInstance();
    if (fileUtils) {
        ccstd::string fullPath = fileUtils->fullPathForFilename("main.js");
        CC_LOG_INFO("BaseGame: main.js resolved to: %s (exists=%d)", fullPath.c_str(), fileUtils->isFileExist(fullPath));
        // Also check builtin-effects.json resolution
        ccstd::string effectsPath = fileUtils->fullPathForFilename("builtin-effects.json");
        CC_LOG_INFO("BaseGame: builtin-effects.json resolved to: %s (exists=%d)", effectsPath.c_str(), fileUtils->isFileExist(effectsPath));
    }
    runScript("main.js");
    CC_LOG_INFO("BaseGame: runScript main.js finished");
    return 0;
}
} // namespace cc
