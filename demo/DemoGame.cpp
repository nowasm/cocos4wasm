#include "DemoGame.h"
#include "base/Log.h"
#include "cocos/2d/renderer/UIBatcher2d.h"
#include "core/Root.h"
#include "core/builtin/BuiltinResMgr.h"
#include "core/builtin/BuiltinEffectLoader.h"
#include "core/component/ComponentScheduler.h"
#include "core/component/NodeActivator.h"
#include "renderer/pipeline/forward/ForwardPipeline.h"
#include "renderer/GFXDeviceManager.h"
#include "platform/FileUtils.h"

using namespace cc;
using namespace cc::scene;

bool DemoGame::initEngine() {
    auto *device = gfx::Device::getInstance();
    if (!device) return false;

    auto *root = ccnew Root(device);
    root->initialize(nullptr);
    BuiltinResMgr::getInstance()->initBuiltinRes();

    auto *fu = FileUtils::getInstance();
    for (auto &p : {"builtin-effects.json",
                     "Resources/builtin-effects.json",
                     "../../../templates/wasm32/builtin-effects.json"}) {
        ccstd::string path(p);
        if (fu->isFileExist(path)) { loadBuiltinEffectsFromJson(path); break; }
        auto full = fu->fullPathForFilename(path);
        if (!full.empty() && fu->isFileExist(full)) { loadBuiltinEffectsFromJson(full); break; }
    }

    auto *pipeline = ccnew pipeline::ForwardPipeline();
    pipeline->initialize({});
    root->setRenderPipeline(pipeline);

    IRenderSceneInfo si;
    si.name = "DemoMain";
    _renderScene = root->createScene(si);

    _defaultCameraNode = ccnew Node("DemoCam");
    _defaultCameraNode->setPosition(Vec3(0, 0, 10));
    _defaultCameraNode->lookAt(Vec3::ZERO);
    _defaultCamera = root->createCamera();
    ICameraInfo ci;
    ci.name = "DemoCam";
    ci.node = _defaultCameraNode;
    ci.projection = CameraProjection::PERSPECTIVE;
    ci.window = root->getMainWindow();
    _defaultCamera->initialize(ci);
    _defaultCamera->setFov(45);
    _defaultCamera->setNearClip(0.1f);
    _defaultCamera->setFarClip(1000);
    _defaultCamera->setVisibility(0xFFFFFFFF);
    _defaultCamera->setClearFlag(gfx::ClearFlagBit::ALL);
    _defaultCamera->setClearColor(gfx::Color{0.12f, 0.12f, 0.18f, 1});
    _renderScene->addCamera(_defaultCamera);

    _defaultLightNode = ccnew Node("DemoLight");
    _defaultLightNode->setRotationFromEuler(-45, 45, 0);
    _defaultLight = root->createLight<DirectionalLight>();
    _defaultLight->setNode(_defaultLightNode);
    _defaultLight->setColor(Vec3(1, 1, 1));
    _defaultLight->setIlluminance(80000);
    _renderScene->setMainLight(_defaultLight);

    return true;
}

void DemoGame::switchScene(int index) {
    auto &reg = DemoSceneRegistry::get().list();
    if (reg.empty()) return;
    index = ((index % (int)reg.size()) + (int)reg.size()) % (int)reg.size();
    _pendingSceneIdx = index;
}

void DemoGame::applyPendingSceneSwitch() {
    if (_pendingSceneIdx < 0 || _pendingSceneIdx == _currentSceneIdx) {
        _pendingSceneIdx = -1;
        return;
    }
    auto &reg = DemoSceneRegistry::get().list();

    if (_currentScene) {
        _currentScene->onExit();
        delete _currentScene;
        _currentScene = nullptr;
    }

    _currentSceneIdx = _pendingSceneIdx;
    _pendingSceneIdx = -1;
    _currentScene = reg[_currentSceneIdx].factory();
    _currentScene->onEnter(_renderScene, Root::getInstance());
    CC_LOG_INFO("[demo] entered scene [%d/%zu] %s",
                _currentSceneIdx + 1, reg.size(), reg[_currentSceneIdx].name.c_str());
}

int DemoGame::init() {
    _windowInfo.title = "Cocos Demo";
    _windowInfo.width = 1280;
    _windowInfo.height = 720;

    int ret = BaseGame::init();
    if (ret != 0) return ret;

    if (!initEngine()) {
        CC_LOG_ERROR("[demo] engine init failed");
        return -1;
    }

    auto &reg = DemoSceneRegistry::get().list();
    CC_LOG_INFO("[demo] %zu scene(s) registered:", reg.size());
    for (size_t i = 0; i < reg.size(); ++i) {
        CC_LOG_INFO("  [%zu] %s", i + 1, reg[i].name.c_str());
    }

    // Start on the last-registered scene — that's usually the newest milestone
    // we care about verifying. Arrow keys cycle to earlier scenes.
    if (!reg.empty()) _pendingSceneIdx = static_cast<int>(reg.size()) - 1;

    _tickListener.bind([this](float dt) {
        applyPendingSceneSwitch();
        // Canonical Cocos tick order:
        //   queued start() → component update → scene logic → component
        //   lateUpdate → UI batcher (collects all registered UIRenderers
        //   and emits the minimum number of scene::Models)
        NodeActivator::get().invokePendingStarts();
        ComponentScheduler::get().update(dt);
        if (_currentScene) _currentScene->onUpdate(dt);
        ComponentScheduler::get().lateUpdate(dt);
        UIBatcher2d::get().tick();
    });

    _keyboardListener.bind([this](const KeyboardEvent &ev) {
        if (ev.action != KeyboardEvent::Action::PRESS) return;
        if (ev.key == static_cast<int>(KeyCode::ARROW_RIGHT)) {
            switchScene(_currentSceneIdx + 1);
        } else if (ev.key == static_cast<int>(KeyCode::ARROW_LEFT)) {
            switchScene(_currentSceneIdx - 1);
        } else if (ev.key == static_cast<int>(KeyCode::ESCAPE)) {
            onClose();
        }
    });

    return 0;
}

CC_REGISTER_APPLICATION(DemoGame);
