#include "Game.h"
#include <algorithm>
#include <cmath>
#include "base/Log.h"
#include "core/Root.h"
#include "core/builtin/BuiltinResMgr.h"
#include "core/builtin/BuiltinEffectLoader.h"
#include "platform/FileUtils.h"
#include "renderer/GFXDeviceManager.h"
#include "renderer/pipeline/forward/ForwardPipeline.h"
#include "renderer/pipeline/PipelineSceneData.h"
#include "scene/Ambient.h"

using namespace cc;
using namespace cc::scene;

namespace {
constexpr float D2R = 3.14159265f / 180.0f;
const Vec3 CAM_TARGET{8.0f, -1.5f, -8.0f};
constexpr float CAM_DIST = 140.0f;
} // namespace

void Game::updateCamera() {
    float yr = _yaw * D2R, pr = _pitch * D2R;
    float cp = std::cos(pr);
    Vec3 dir(std::cos(yr) * cp, std::sin(pr), std::sin(yr) * cp);
    _camNode->setPosition(CAM_TARGET + dir * CAM_DIST);
    _camNode->lookAt(CAM_TARGET);
    if (_camera) _camera->setOrthoHeight(_orthoHeight);
}

bool Game::initEngine() {
    auto *device = gfx::Device::getInstance();
    if (!device) return false;

    auto *root = ccnew Root(device);
    root->initialize(nullptr);
    BuiltinResMgr::getInstance()->initBuiltinRes();

    // load compiled builtin effects (shaders)
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
    si.name = "Voxel";
    _renderScene = root->createScene(si);

    // --- orthographic isometric camera ---
    _camNode = ccnew Node("Cam");
    _camera = root->createCamera();
    ICameraInfo ci;
    ci.name = "Cam";
    ci.node = _camNode;
    ci.projection = CameraProjection::ORTHO;
    ci.window = root->getMainWindow();
    _camera->initialize(ci);
    _camera->setProjectionType(CameraProjection::ORTHO);
    _camera->setOrthoHeight(_orthoHeight);
    _camera->setNearClip(0.1f);
    _camera->setFarClip(600.0f);
    _camera->setVisibility(0xFFFFFFFF);
    _camera->setClearFlag(gfx::ClearFlagBit::ALL);
    _camera->setClearColor(gfx::Color{0.85f, 0.83f, 0.88f, 1.0f}); // soft lavender sky
    _renderScene->addCamera(_camera);
    updateCamera();

    // --- sun ---
    _lightNode = ccnew Node("Sun");
    _lightNode->setRotationFromEuler(-52.0f, 132.0f, 0.0f);
    _light = root->createLight<DirectionalLight>();
    _light->setNode(_lightNode);
    _light->setColor(Vec3(1.0f, 0.96f, 0.88f));
    _light->setIlluminance(65000.0f);
    _renderScene->setMainLight(_light);

    // bright sky bounce — reference is mid-day with strong indirect fill
    if (auto *psd = pipeline->getPipelineSceneData()) {
        if (auto *amb = psd->getAmbient()) {
            amb->setSkyColor(Vec4(0.82f, 0.88f, 0.98f, 1.0f));
            amb->setGroundAlbedo(Vec4(0.60f, 0.55f, 0.44f, 1.0f));
            amb->setSkyIllum(20000.0f);
        }
    }
    return true;
}

int Game::init() {
    _windowInfo.title = "Cocos Voxel Village";
    _windowInfo.width = 1280;
    _windowInfo.height = 720;

    int ret = BaseGame::init();
    if (ret != 0) return ret;

    if (!initEngine()) {
        CC_LOG_ERROR("Engine init failed");
        return -1;
    }

    _world.build(_renderScene, Root::getInstance());
    CC_LOG_INFO("Voxel village built. Drag = rotate, wheel = zoom, ESC = quit.");

    _tickListener.bind([this](float dt) { _world.update(dt); });

    _mouseListener.bind([this](const MouseEvent &ev) {
        if (ev.type == MouseEvent::Type::DOWN && ev.button == 0) {
            _dragging = true;
            _lastMX = ev.x;
            _lastMY = ev.y;
        } else if (ev.type == MouseEvent::Type::UP && ev.button == 0) {
            _dragging = false;
        } else if (ev.type == MouseEvent::Type::MOVE && _dragging) {
            _yaw += (ev.x - _lastMX) * 0.3f;
            _pitch -= (ev.y - _lastMY) * 0.2f;
            _pitch = std::max(12.0f, std::min(78.0f, _pitch));
            _lastMX = ev.x;
            _lastMY = ev.y;
            updateCamera();
        } else if (ev.type == MouseEvent::Type::WHEEL) {
            _orthoHeight -= ev.yDelta * 0.6f;
            _orthoHeight = std::max(6.0f, std::min(40.0f, _orthoHeight));
            updateCamera();
        }
    });

    _keyboardListener.bind([this](const KeyboardEvent &ev) {
        if (ev.action != KeyboardEvent::Action::PRESS) return;
        if (ev.key == static_cast<int>(KeyCode::ESCAPE)) onClose();
    });

    return 0;
}

CC_REGISTER_APPLICATION(Game);
