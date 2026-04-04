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
#include "bindings/jswrapper/SeApi.h"
#include "platform/FileUtils.h"
#include "platform/interfaces/modules/ISystemWindowManager.h"
#include "renderer/pipeline/GlobalDescriptorSetManager.h"

#if CC_PLATFORM == CC_PLATFORM_EMSCRIPTEN
    #include "2d/renderer/Batcher2d.h"
    #include "core/Root.h"
    #include "core/builtin/BuiltinResMgr.h"
    #include "core/builtin/BuiltinEffectLoader.h"
    #include "bindings/manual/jsb_classtype.h"
    #include "bindings/manual/jsb_conversions.h"
    #include "renderer/GFXDeviceManager.h"
    #include "renderer/pipeline/forward/ForwardPipeline.h"

// JSB function: jsb.__syncBatcher2DRootNodes(node)
// Called from the JS facade to sync root nodes for 2D rendering.
// The node argument is passed directly so its se::Object has valid privateData.
static bool js_syncBatcher2DRootNodes(se::State &s) {
    const auto &args = s.args();
    if (args.empty() || !args[0].isObject()) {
        CC_LOG_WARNING("__syncBatcher2DRootNodes: expected a Node argument");
        return false;
    }
    auto *node = static_cast<cc::Node *>(args[0].toObject()->getPrivateData());
    if (!node) {
        // Try PrivateObject path
        auto *privObj = args[0].toObject()->getPrivateObject();
        if (privObj) node = static_cast<cc::Node *>(privObj->getRaw());
    }
    if (!node) {
        CC_LOG_WARNING("__syncBatcher2DRootNodes: node has no native pointer");
        return false;
    }
    auto *root = cc::Root::getInstance();
    if (!root || !root->getBatcher2D()) {
        CC_LOG_WARNING("__syncBatcher2DRootNodes: Root or Batcher2D not available");
        return false;
    }
    ccstd::vector<cc::Node *> rootNodes;
    rootNodes.push_back(node);
    root->getBatcher2D()->syncRootNodesToNative(std::move(rootNodes));
    CC_LOG_INFO("__syncBatcher2DRootNodes: synced node '%s' to Batcher2D", node->getName().c_str());
    return true;
}
SE_BIND_FUNC(js_syncBatcher2DRootNodes)

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
    // Standalone WASM mode: initialise Root + render pipeline.
    {
        // 1. Load builtin resources and effects (C++ only, no JSB needed)
        BuiltinResMgr::getInstance()->initBuiltinRes();
        int effectCount = loadBuiltinEffectsFromJson("builtin-effects.json");
        CC_LOG_INFO("Loaded %d builtin effects for standalone WASM mode", effectCount);

        // 2. Create Root + Pipeline from C++ (needed for engine internals)
        auto *device = gfx::Device::getInstance();
        CC_ASSERT(device);
        auto *root = ccnew Root(device);
        root->initialize(nullptr);
        auto *pipeline = ccnew pipeline::ForwardPipeline();
        pipeline->initialize({});
        root->setRenderPipeline(pipeline);

        // 3. Wrap the C++ Root in a JS object and expose as jsb.__rt so the
        //    facade can access Root's JSB methods (getBatcher2D, mainWindow etc.)
        //    This creates the NativePtrToObjectMap entry that nativevalue_to_se needs.
        auto *seEngine = se::ScriptEngine::getInstance();
        se::Value rootVal;
        nativevalue_to_se(root, rootVal);
        if (!rootVal.isObject()) {
            // nativevalue_to_se failed (no class mapping).  Create wrapper manually.
            auto *rootCls = JSBClassType::findClass(root);
            if (rootCls) {
                auto *rootSeObj = se::Object::createObjectWithClass(rootCls);
                rootSeObj->setPrivateData(root);
                rootVal.setObject(rootSeObj);
            }
        }
        if (rootVal.isObject()) {
            se::Value jsbVal;
            seEngine->getGlobalObject()->getProperty("jsb", &jsbVal);
            if (jsbVal.isObject()) {
                jsbVal.toObject()->setProperty("__rt", rootVal);
                // Register native function for syncing Batcher2D root nodes
                jsbVal.toObject()->defineFunction("__syncBatcher2DRootNodes", _SE(js_syncBatcher2DRootNodes));
            }
            CC_LOG_INFO("BaseGame: Root JS wrapper + __syncBatcher2DRootNodes registered");
        }
    }
#endif
    runScript("main.js");
    return 0;
}
} // namespace cc
