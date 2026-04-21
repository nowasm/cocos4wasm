#include "DemoGame.h"
#include "base/Log.h"
#include "cocos/2d/renderer/UIBatcher2d.h"
#include "cocos/asset/AssetManager.h"
#include "cocos/asset/EffectLoader.h"
#include "cocos/asset/MaterialLoader.h"
#include "cocos/asset/PrefabLoader.h"
#include "cocos/asset/SpriteFrameLoader.h"
#include "cocos/misc/CameraComponent.h"
#include "cocos/input/InputEventDispatcher.h"
#include "cocos/ui/components/ProgressBar.h"
#include "cocos/ui/editBox/EditBox.h"
#include "core/Root.h"
#include "core/assets/Asset.h"
#include "core/builtin/BuiltinResMgr.h"
#include "core/builtin/BuiltinEffectLoader.h"
#include "core/component/ComponentScheduler.h"
#include "core/component/NodeActivator.h"
#include "game/TextureLoader.h"
#include "renderer/pipeline/forward/ForwardPipeline.h"
#include "renderer/GFXDeviceManager.h"
#include "platform/FileUtils.h"

using namespace cc;
using namespace cc::scene;

static void registerAssetLoaders() {
    // PNG → Texture2D loader. Reuses the existing TextureLoader path.
    auto pngLoader = [](const ccstd::string &absPath, const ccstd::string & /*uuid*/)
        -> IntrusivePtr<Asset> {
        auto *tex = game::TextureLoader::loadFromFile(absPath);
        return IntrusivePtr<Asset>(tex);
    };
    AssetManager::get().registerLoader(".png",  pngLoader);
    AssetManager::get().registerLoader(".jpg",  pngLoader);
    AssetManager::get().registerLoader(".jpeg", pngLoader);
    AssetManager::get().registerLoader(".webp", pngLoader);

    // .mtl → Material (Cocos Creator 3.x material asset format).
    AssetManager::get().registerLoader(".mtl",
        [](const ccstd::string &absPath, const ccstd::string & /*uuid*/)
            -> IntrusivePtr<Asset> {
            auto *mat = MaterialLoader::loadFromFile(absPath);
            return IntrusivePtr<Asset>(mat);
        });

    // .prefab → Prefab asset (JSON wrapper; instantiate() per-use).
    AssetManager::get().registerLoader(".prefab",
        [](const ccstd::string &absPath, const ccstd::string & /*uuid*/)
            -> IntrusivePtr<Asset> {
            auto *pre = PrefabLoader::loadFromFile(absPath);
            return IntrusivePtr<Asset>(pre);
        });

    // .effect → EffectAsset. MVP looks up the builtin registry by name;
    // full shader-blob parsing is deferred (see EffectLoader.h).
    AssetManager::get().registerLoader(".effect",
        [](const ccstd::string &absPath, const ccstd::string & /*uuid*/)
            -> IntrusivePtr<Asset> {
            auto *fx = EffectLoader::loadFromFile(absPath);
            return IntrusivePtr<Asset>(fx);
        });

    // .json (SpriteFrame) — Editor exports sprite-frame metadata as a plain
    // .json file sidecar to the texture. Treat .json as SpriteFrame for now;
    // if other .json asset types appear (label-atlas, spine data) we'll
    // sniff the `__type__` before dispatching.
    auto sfLoader = [](const ccstd::string &absPath, const ccstd::string & /*uuid*/)
        -> IntrusivePtr<Asset> {
        auto *sf = SpriteFrameLoader::loadFromFile(absPath);
        return IntrusivePtr<Asset>(sf);
    };
    AssetManager::get().registerLoader(".json", sfLoader);
    AssetManager::get().registerLoader(".sf",   sfLoader);
}

bool DemoGame::initEngine() {
    auto *device = gfx::Device::getInstance();
    if (!device) return false;

    auto *root = ccnew Root(device);
    root->initialize(nullptr);
    BuiltinResMgr::getInstance()->initBuiltinRes();

    // Force-include authoring-layer components that aren't referenced by
    // any demo code — their CC_END_CLASS auto-registration would
    // otherwise be stripped by MSVC's static-lib dead-code elimination.
    (void)CameraComponent::forceLink();
    (void)ProgressBar::forceLink();

    // Asset pipeline: register loaders + load the uuid-map for the
    // editor-sample fixture bundle.
    registerAssetLoaders();
    AssetManager::get().setAssetRoot("assets/editor-sample/");
    AssetManager::get().loadUuidMap("assets/editor-sample/uuid-map.json");

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

    // Start the P4 input dispatcher — it subscribes to the engine bus and
    // begins routing mouse/touch events into the active-Canvas subtrees.
    InputEventDispatcher::get().start();

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
    // Preference list — the first scene present wins.
    int startIdx = static_cast<int>(reg.size()) - 1;
    bool found = false;
    for (const char *prefer : {"PrefabLoadScene", "LabelTestScene", "EditBoxScene",
                                 "ScrollViewScene", "WidgetScene",
                                 "ClickableScene"}) {
        for (size_t i = 0; i < reg.size(); ++i) {
            if (reg[i].name == prefer) {
                startIdx = static_cast<int>(i);
                found = true;
                break;
            }
        }
        if (found) break;
    }
    if (!reg.empty()) _pendingSceneIdx = startIdx;

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
        // Don't steal arrow / escape keys while a text box is focused —
        // they move the caret / close the text input inside the EditBox.
        if (EditBox::isAnyEditing()) return;
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
