#include "Game.h"
#include <cmath>
#include "base/Log.h"
#include "core/Root.h"
#include "core/builtin/BuiltinResMgr.h"
#include "core/builtin/BuiltinEffectLoader.h"
#include "core/assets/EffectAsset.h"
#include "profiler/DebugRenderer.h"
#include "renderer/pipeline/forward/ForwardPipeline.h"
#include "renderer/pipeline/PipelineSceneData.h"
#include "renderer/GFXDeviceManager.h"
#include "platform/FileUtils.h"
#include "platform/interfaces/modules/ISystemWindow.h"
#include "platform/interfaces/modules/ISystemWindowManager.h"

using namespace cc;
using namespace cc::game;
using namespace cc::scene;

static void setOrbitCamera(Node *n, float yaw, float pitch, float dist) {
    constexpr float D2R = 3.14159265f / 180.0f;
    float yr = yaw * D2R, pr = pitch * D2R;
    float cp = cosf(pr);
    n->setPosition(Vec3(dist * cosf(yr) * cp, dist * -sinf(pr), dist * sinf(yr) * cp));
    n->lookAt(Vec3::ZERO);
}

// -------- engine bootstrap --------
bool Game::initEngine() {
    auto *device = gfx::Device::getInstance();
    if (!device) return false;

    auto *root = ccnew Root(device);
    root->initialize(nullptr);
    BuiltinResMgr::getInstance()->initBuiltinRes();

    // load effects
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

    // scene
    IRenderSceneInfo si; si.name = "Main";
    _renderScene = root->createScene(si);

    // camera
    _camNode = ccnew Node("Cam");
    setOrbitCamera(_camNode, _orbitYaw, _orbitPitch, _orbitDist);
    _camera = root->createCamera();
    ICameraInfo ci;
    ci.name = "Cam"; ci.node = _camNode;
    ci.projection = CameraProjection::PERSPECTIVE;
    ci.window = root->getMainWindow();
    _camera->initialize(ci);
    _camera->setFov(45); _camera->setNearClip(0.1f); _camera->setFarClip(1000);
    _camera->setVisibility(0xFFFFFFFF);
    _camera->setClearFlag(gfx::ClearFlagBit::ALL);
    _camera->setClearColor(gfx::Color{0.12f, 0.12f, 0.18f, 1});
    _renderScene->addCamera(_camera);

    // default directional light
    _defaultLightNode = ccnew Node("DefLight");
    _defaultLightNode->setRotationFromEuler(-45, 45, 0);
    _defaultLight = root->createLight<DirectionalLight>();
    _defaultLight->setNode(_defaultLightNode);
    _defaultLight->setColor(Vec3(1, 1, 1));
    _defaultLight->setIlluminance(80000);
    _renderScene->setMainLight(_defaultLight);

    return true;
}

// -------- test switching --------
void Game::switchTest(int index) {
    auto &reg = getTestRegistry();
    if (reg.empty()) return;
    index = ((index % (int)reg.size()) + (int)reg.size()) % (int)reg.size();
    // Defer to next frame start — GPU may still reference current frame's resources
    _pendingTest = index;
}

void Game::doSwitchTest() {
    if (_pendingTest < 0 || _pendingTest == _currentTest) return;
    auto &reg = getTestRegistry();

    // cleanup old test — renderers detach themselves from scene
    if (_activeTest) {
        _activeTest->cleanup();
        delete _activeTest;
        _activeTest = nullptr;
    }

    // clear any leftover lights that tests added directly to the scene
    if (_renderScene) {
        _renderScene->removeSphereLights();
        _renderScene->removeSpotLights();
        _renderScene->setMainLight(_defaultLight);
    }

    _currentTest = _pendingTest;
    _pendingTest = -1;
    _activeTest = reg[_currentTest].factory();
    _activeTest->setup(_renderScene, Root::getInstance());
    updateTitle();
    CC_LOG_INFO("Switched to [%s] %s", reg[_currentTest].category.c_str(), reg[_currentTest].name.c_str());
}

void Game::updateTitle() {
    auto &reg = getTestRegistry();
    std::string title = "Cocos C++ Tests | [" +
        std::to_string(_currentTest + 1) + "/" + std::to_string(reg.size()) +
        "] " + reg[_currentTest].category + " - " + reg[_currentTest].name +
        " | Arrow keys to switch";
    auto *wm = CC_GET_PLATFORM_INTERFACE(ISystemWindowManager);
    // title update not directly exposed, log it
    CC_LOG_INFO("%s", title.c_str());
}

// -------- init --------
int Game::init() {
    _windowInfo.title = "Cocos C++ Tests";
    _windowInfo.width = 1280;
    _windowInfo.height = 720;

    int ret = BaseGame::init();
    if (ret != 0) return ret;

    if (!initEngine()) {
        CC_LOG_ERROR("Engine init failed");
        return -1;
    }

    CC_LOG_INFO("=== %d tests registered ===", (int)getTestRegistry().size());
    for (size_t i = 0; i < getTestRegistry().size(); ++i) {
        auto &e = getTestRegistry()[i];
        CC_LOG_INFO("  [%zu] %s / %s", i + 1, e.category.c_str(), e.name.c_str());
    }

    // First test loaded immediately (no deferred needed — nothing to clean up)
    _pendingTest = 0;

    // tick — deferred test switch happens at frame start, before update/render
    _tickListener.bind([this](float dt) {
        doSwitchTest();
        if (_activeTest) _activeTest->update(dt);
    });

    // mouse
    _mouseListener.bind([this](const MouseEvent &ev) {
        if (ev.type == MouseEvent::Type::DOWN && ev.button == 0) {
            _dragging = true; _lastMX = ev.x; _lastMY = ev.y;
        } else if (ev.type == MouseEvent::Type::UP && ev.button == 0) {
            _dragging = false;
        } else if (ev.type == MouseEvent::Type::MOVE && _dragging) {
            _orbitYaw += (ev.x - _lastMX) * 0.3f;
            _orbitPitch -= (ev.y - _lastMY) * 0.3f;
            if (_orbitPitch > -5) _orbitPitch = -5;
            if (_orbitPitch < -85) _orbitPitch = -85;
            _lastMX = ev.x; _lastMY = ev.y;
            setOrbitCamera(_camNode, _orbitYaw, _orbitPitch, _orbitDist);
        } else if (ev.type == MouseEvent::Type::WHEEL) {
            _orbitDist -= ev.yDelta * 0.01f;
            if (_orbitDist < 3) _orbitDist = 3;
            if (_orbitDist > 60) _orbitDist = 60;
            setOrbitCamera(_camNode, _orbitYaw, _orbitPitch, _orbitDist);
        }
    });

    // keyboard
    _keyboardListener.bind([this](const KeyboardEvent &ev) {
        if (ev.action != KeyboardEvent::Action::PRESS) return;
        if (ev.key == static_cast<int>(KeyCode::ARROW_RIGHT)) {
            switchTest(_currentTest + 1);
        } else if (ev.key == static_cast<int>(KeyCode::ARROW_LEFT)) {
            switchTest(_currentTest - 1);
        } else if (ev.key == static_cast<int>(KeyCode::ESCAPE)) {
            onClose();
        }
    });

    return 0;
}

CC_REGISTER_APPLICATION(Game);
