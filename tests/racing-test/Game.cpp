#include "Game.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
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

// chase camera tuning
constexpr float CAM_BACK = 9.0f;
constexpr float CAM_UP = 3.6f;
constexpr float CAM_LOOK_UP = 1.2f;
constexpr float CAM_LOOK_AHEAD = 4.0f;
constexpr float CAM_SMOOTH = 4.5f;

// Win32 VK codes — the engine KeyCode enum matches VK values for letters
constexpr int KEY_W = 'W', KEY_A = 'A', KEY_S = 'S', KEY_D = 'D', KEY_R = 'R';
} // namespace

void Game::updateCamera(float dt) {
    float yr = _world.carYawDeg() * D2R;
    Vec3 forward(std::sin(yr), 0.0f, std::cos(yr));
    const Vec3 &carPos = _world.carPosition();

    Vec3 desired = carPos - forward * CAM_BACK + Vec3(0.0f, CAM_UP, 0.0f);
    if (!_camInit) {
        _camPos = desired;
        _camInit = true;
    } else {
        float t = std::min(1.0f, dt * CAM_SMOOTH);
        _camPos += (desired - _camPos) * t;
    }
    _camNode->setPosition(_camPos);
    _camNode->lookAt(carPos + Vec3(0.0f, CAM_LOOK_UP, 0.0f) + forward * CAM_LOOK_AHEAD);
}

bool Game::initEngine() {
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
    si.name = "Race";
    _renderScene = root->createScene(si);

    // --- perspective chase camera ---
    _camNode = ccnew Node("Cam");
    _camera = root->createCamera();
    ICameraInfo ci;
    ci.name = "Cam";
    ci.node = _camNode;
    ci.projection = CameraProjection::PERSPECTIVE;
    ci.window = root->getMainWindow();
    _camera->initialize(ci);
    _camera->setProjectionType(CameraProjection::PERSPECTIVE);
    _camera->setFovAxis(CameraFOVAxis::VERTICAL);
    _camera->setFov(45.0f * D2R);
    _camera->setNearClip(0.3f);
    _camera->setFarClip(600.0f);
    _camera->setVisibility(0xFFFFFFFF);
    _camera->setClearFlag(gfx::ClearFlagBit::ALL);
    _camera->setClearColor(gfx::Color{0.55f, 0.75f, 0.95f, 1.0f}); // day sky
    _renderScene->addCamera(_camera);

    // --- sun ---
    _lightNode = ccnew Node("Sun");
    _lightNode->setRotationFromEuler(-58.0f, 118.0f, 0.0f);
    _light = root->createLight<DirectionalLight>();
    _light->setNode(_lightNode);
    _light->setColor(Vec3(1.0f, 0.97f, 0.9f));
    _light->setIlluminance(70000.0f);
    _renderScene->setMainLight(_light);

    if (auto *psd = pipeline->getPipelineSceneData()) {
        if (auto *amb = psd->getAmbient()) {
            amb->setSkyColor(Vec4(0.62f, 0.78f, 0.97f, 1.0f));
            amb->setGroundAlbedo(Vec4(0.35f, 0.45f, 0.30f, 1.0f));
            amb->setSkyIllum(22000.0f);
        }
    }
    return true;
}

void Game::tick(float dt) {
    CarInput in;
    if (_autopilot) {
        // P-controller lapping the ring counter-clockwise: follow the tangent,
        // correct toward the centerline. Exercises kinematics + lap timing
        // without OS input (the raw-input hook needs a foreground window).
        _autoTime += dt;
        const Vec3 &p = _world.carPosition();
        float a = std::atan2(p.z, p.x);
        float dist = std::sqrt(p.x * p.x + p.z * p.z);
        float crossTrack = dist - RaceWorld::TRACK_RADIUS; // + = outside
        float desiredYaw = -a / D2R - 2.5f * crossTrack;
        float headingErr = desiredYaw - _world.carYawDeg();
        while (headingErr > 180.0f) headingErr -= 360.0f;
        while (headingErr < -180.0f) headingErr += 360.0f;
        in.steer = std::max(-1.0f, std::min(1.0f, -0.08f * headingErr));
        in.throttle = std::fabs(headingErr) > 40.0f ? 0.3f : 0.85f;

        _autoLogTimer += dt;
        if (_autoLogTimer >= 2.0f) {
            _autoLogTimer = 0.0f;
            CC_LOG_INFO("AUTO: t=%.1fs pos(%.1f, %.1f) v=%.1fm/s yaw=%.0f xtrack=%.2f lap=%d",
                        _autoTime, p.x, p.z, _world.speed(), _world.carYawDeg(),
                        crossTrack, _world.lapCount());
        }
    } else {
        in.throttle = _keyW ? 1.0f : 0.0f;
        in.brake = _keyS ? 1.0f : 0.0f;
        in.steer = (_keyD ? 1.0f : 0.0f) - (_keyA ? 1.0f : 0.0f);
    }
    in.reset = _resetQueued;
    _resetQueued = false;

    _world.update(dt, in);
    updateCamera(dt);
}

int Game::init() {
    _windowInfo.title = "Cocos Racing Test";
    _windowInfo.width = 1280;
    _windowInfo.height = 720;

    int ret = BaseGame::init();
    if (ret != 0) return ret;

    if (!initEngine()) {
        CC_LOG_ERROR("Engine init failed");
        return -1;
    }

    const char *autodrive = ::getenv("COCOS_AUTODRIVE");
    _autopilot = autodrive && autodrive[0] == '1';

    _world.build(_renderScene, Root::getInstance());
    updateCamera(0.0f);
    if (_autopilot) CC_LOG_INFO("AUTO: autopilot enabled");
    CC_LOG_INFO("Racing test ready. W/S = throttle/brake, A/D = steer, R = reset, ESC = quit.");

    _tickListener.bind([this](float dt) { tick(dt); });

    _keyboardListener.bind([this](const KeyboardEvent &ev) {
        bool down = ev.action == KeyboardEvent::Action::PRESS ||
                    ev.action == KeyboardEvent::Action::REPEAT;
        switch (ev.key) {
            case KEY_W: case static_cast<int>(KeyCode::ARROW_UP): _keyW = down; break;
            case KEY_S: case static_cast<int>(KeyCode::ARROW_DOWN): _keyS = down; break;
            case KEY_A: case static_cast<int>(KeyCode::ARROW_LEFT): _keyA = down; break;
            case KEY_D: case static_cast<int>(KeyCode::ARROW_RIGHT): _keyD = down; break;
            case KEY_R:
                if (ev.action == KeyboardEvent::Action::PRESS) _resetQueued = true;
                break;
            case static_cast<int>(KeyCode::ESCAPE):
                if (ev.action == KeyboardEvent::Action::PRESS) onClose();
                break;
            default: break;
        }
    });

    return 0;
}

CC_REGISTER_APPLICATION(Game);
