#include "RaceWorld.h"
#include <algorithm>
#include <cmath>
#include "base/Log.h"

using namespace cc;
using namespace cc::game;

namespace {

constexpr float PI = 3.14159265f;
constexpr float D2R = PI / 180.0f;
constexpr float R2D = 180.0f / PI;

// --- track layout ---
constexpr float TRACK_R = RaceWorld::TRACK_RADIUS; // centerline radius
constexpr float ROAD_HALF_W = 4.5f;
constexpr int ROAD_SEGS = 96;
constexpr int KERB_SEGS = 128;

// --- car tuning (arcade) ---
constexpr float CAR_SCALE = 1.6f;             // Kenney model is ~2.56m long -> ~4.1m
constexpr float WHEELBASE = 1.52f * CAR_SCALE;
constexpr float WHEEL_RADIUS = 0.3f * CAR_SCALE;
constexpr float MAX_SPEED = 33.0f;            // ~120 km/h
constexpr float MAX_REVERSE = 8.0f;
constexpr float ENGINE_ACCEL = 14.0f;
constexpr float BRAKE_DECEL = 24.0f;
constexpr float REVERSE_ACCEL = 7.0f;
constexpr float DRAG = 0.45f;                 // v * DRAG
constexpr float ROLLING = 1.8f;
constexpr float OFFROAD_DRAG_MULT = 5.0f;

inline Color rgb(int r, int g, int b) {
    return Color(static_cast<uint8_t>(r), static_cast<uint8_t>(g),
                 static_cast<uint8_t>(b), 255);
}

const Color GRASS = rgb(96, 158, 62);
const Color ROAD = rgb(72, 74, 80);
const Color KERB_RED = rgb(214, 48, 42);
const Color KERB_WHITE = rgb(240, 240, 238);
const Color LINE_WHITE = rgb(250, 250, 250);
const Color CONE_ORANGE = rgb(255, 130, 20);
const Color TREE_GREEN = rgb(48, 116, 46);
const Color TREE_TRUNK = rgb(112, 78, 46);

Node *findNodeByName(Node *node, const char *substr) {
    if (!node) return nullptr;
    if (node->getName().find(substr) != ccstd::string::npos) return node;
    for (const auto &child : node->getChildren()) {
        if (Node *hit = findNodeByName(child.get(), substr)) return hit;
    }
    return nullptr;
}

} // namespace

// ---------------------------------------------------------------------------

void RaceWorld::build(scene::RenderScene *rs, Root *root) {
    _rs = rs;
    _root = root;
    _boxMesh = PrimitiveFactory::createBox(1.0f, 1.0f, 1.0f);

    buildTrack();
    buildCar();
    resetCar();
}

Material *RaceWorld::material(const Color &c, float roughness) {
    uint64_t key = (static_cast<uint64_t>(c.r) << 32) |
                   (static_cast<uint64_t>(c.g) << 24) |
                   (static_cast<uint64_t>(c.b) << 16) |
                   static_cast<uint64_t>(roughness * 100.0f);
    auto it = _matCache.find(key);
    if (it != _matCache.end()) return it->second;

    PBRParams params;
    params.albedo = c;
    params.roughness = roughness;
    params.metallic = 0.0f;
    params.specularIntensity = 0.35f;
    auto *mat = MaterialFactory::createStandard(params);
    _matCache[key] = mat;
    return mat;
}

Node *RaceWorld::box(const Vec3 &center, const Vec3 &size, const Color &color,
                     float roughness, Node *parent) {
    auto *n = ccnew Node("b");
    if (parent) n->setParent(parent);
    n->setPosition(center);
    n->setScale(size);
    auto *r = ccnew MeshRenderer(n);
    r->setMesh(_boxMesh);
    r->setMaterial(material(color, roughness));
    _nodes.push_back(n);
    _renderers.push_back(r);
    return n;
}

void RaceWorld::buildTrack() {
    // grass ground — road surface sits at y = 0
    box(Vec3(0.0f, -0.35f, 0.0f), Vec3(240.0f, 0.5f, 240.0f), GRASS, 0.98f);

    // ring road out of overlapping tangential slabs
    const float segLen = 2.0f * PI * TRACK_R / ROAD_SEGS * 1.25f; // 25% overlap hides seams
    for (int i = 0; i < ROAD_SEGS; ++i) {
        float a = (i + 0.5f) * 2.0f * PI / ROAD_SEGS;
        Vec3 c(TRACK_R * std::cos(a), -0.05f, TRACK_R * std::sin(a));
        Node *n = box(c, Vec3(ROAD_HALF_W * 2.0f, 0.1f, segLen), ROAD, 0.96f);
        // slab's long axis (Z) must follow the tangent (-sin a, 0, cos a)
        n->setRotationFromEuler(0.0f, -a * R2D, 0.0f);
    }

    // red/white kerbs on both edges
    for (int side = 0; side < 2; ++side) {
        float r = side == 0 ? TRACK_R - ROAD_HALF_W - 0.35f
                            : TRACK_R + ROAD_HALF_W + 0.35f;
        for (int i = 0; i < KERB_SEGS; ++i) {
            float a = (i + 0.5f) * 2.0f * PI / KERB_SEGS;
            Vec3 c(r * std::cos(a), 0.02f, r * std::sin(a));
            Node *n = box(c, Vec3(0.7f, 0.08f, 2.0f),
                          (i % 2 == 0) ? KERB_RED : KERB_WHITE, 0.85f);
            n->setRotationFromEuler(0.0f, -a * R2D, 0.0f);
        }
    }

    // start/finish line across the road at angle 0 (world +X axis)
    box(Vec3(TRACK_R, 0.011f, 0.0f), Vec3(ROAD_HALF_W * 2.0f, 0.02f, 0.9f), LINE_WHITE, 0.9f);

    // cones marking the start
    box(Vec3(TRACK_R - ROAD_HALF_W - 1.2f, 0.25f, 0.0f), Vec3(0.4f, 0.5f, 0.4f), CONE_ORANGE, 0.8f);
    box(Vec3(TRACK_R + ROAD_HALF_W + 1.2f, 0.25f, 0.0f), Vec3(0.4f, 0.5f, 0.4f), CONE_ORANGE, 0.8f);

    // a few blocky trees inside/outside the ring for a sense of speed
    const float treeR[] = {14.0f, 18.0f, 46.0f, 52.0f, 44.0f, 16.0f, 50.0f, 12.0f};
    for (int i = 0; i < 8; ++i) {
        float a = i * (2.0f * PI / 8.0f) + 0.4f;
        float r = treeR[i];
        float x = r * std::cos(a), z = r * std::sin(a);
        box(Vec3(x, 1.0f, z), Vec3(0.7f, 2.0f, 0.7f), TREE_TRUNK, 0.95f);
        box(Vec3(x, 3.0f, z), Vec3(2.6f, 2.6f, 2.6f), TREE_GREEN, 0.95f);
    }
}

void RaceWorld::buildCar() {
    _carPivot = ccnew Node("Car");
    _nodes.push_back(_carPivot);

#ifdef USE_TINYUSDZ
    _usd = USDLoader::load("assets/race.usda", _carPivot);
    if (_usd.success && !_usd.renderers.empty()) {
        _usd.rootNode->setScale(CAR_SCALE, CAR_SCALE, CAR_SCALE);
        _wheelFL = findNodeByName(_usd.rootNode, "wheel_front_left");
        _wheelFR = findNodeByName(_usd.rootNode, "wheel_front_right");
        _wheelBL = findNodeByName(_usd.rootNode, "wheel_back_left");
        _wheelBR = findNodeByName(_usd.rootNode, "wheel_back_right");
        CC_LOG_INFO("RaceWorld: car loaded — %zu renderers, wheels %s",
                    _usd.renderers.size(),
                    (_wheelFL && _wheelFR && _wheelBL && _wheelBR) ? "found" : "MISSING");
        return;
    }
    CC_LOG_ERROR("RaceWorld: USD car load failed (%s) — using fallback box car",
                 _usd.error.c_str());
#endif
    buildFallbackCar(_carPivot);
}

void RaceWorld::buildFallbackCar(Node *pivot) {
    const Color BODY = rgb(214, 40, 40);
    const Color DARK = rgb(30, 30, 34);
    box(Vec3(0.0f, 0.55f, 0.0f), Vec3(1.9f, 0.55f, 4.0f), BODY, 0.5f, pivot);
    box(Vec3(0.0f, 1.05f, -0.35f), Vec3(1.5f, 0.5f, 1.9f), DARK, 0.4f, pivot);
    const float wx = 0.95f, wzF = 1.25f, wzB = -1.35f;
    for (float sx : {-wx, wx}) {
        for (float z : {wzF, wzB}) {
            box(Vec3(sx, 0.35f, z), Vec3(0.35f, 0.7f, 0.7f), DARK, 0.85f, pivot);
        }
    }
}

void RaceWorld::resetCar() {
    _pos = Vec3(TRACK_R, 0.0f, 0.0f);
    _yawDeg = 0.0f; // facing +Z = the tangent at angle 0, counter-clockwise
    _speed = 0.0f;
    _steerVisDeg = 0.0f;
    _lapTime = 0.0f;
    _angleAccum = 0.0f;
    _prevAngle = std::atan2(_pos.z, _pos.x);
    _lapStarted = false;
    if (_carPivot) {
        _carPivot->setPosition(_pos);
        _carPivot->setRotationFromEuler(0.0f, _yawDeg, 0.0f);
    }
}

void RaceWorld::stepKinematics(float dt, const CarInput &input) {
    // longitudinal
    float accel = 0.0f;
    if (input.throttle > 0.0f) {
        accel += input.throttle * ENGINE_ACCEL;
    }
    if (input.brake > 0.0f) {
        if (_speed > 0.5f) {
            accel -= input.brake * BRAKE_DECEL;
        } else {
            accel -= input.brake * REVERSE_ACCEL; // roll into reverse
        }
    }

    // resistances (sign-aware)
    float dragMult = 1.0f;
    float distToCenter = std::sqrt(_pos.x * _pos.x + _pos.z * _pos.z);
    bool offroad = std::fabs(distToCenter - TRACK_R) > ROAD_HALF_W + 0.3f;
    if (offroad) dragMult = OFFROAD_DRAG_MULT;
    accel -= _speed * DRAG * dragMult;
    if (std::fabs(_speed) > 0.05f) {
        accel -= (_speed > 0.0f ? ROLLING : -ROLLING) * dragMult * 0.5f;
    }

    _speed += accel * dt;
    _speed = std::max(-MAX_REVERSE, std::min(MAX_SPEED, _speed));
    if (input.throttle == 0.0f && input.brake == 0.0f && std::fabs(_speed) < 0.15f) {
        _speed = 0.0f;
    }

    // steering — tighter at low speed, stable at top speed
    float speedT = std::min(1.0f, std::fabs(_speed) / MAX_SPEED);
    float maxSteer = 32.0f - 22.0f * speedT;
    float steerDeg = input.steer * maxSteer;
    _steerVisDeg += (steerDeg - _steerVisDeg) * std::min(1.0f, dt * 10.0f);
    if (std::fabs(_speed) > 0.05f) {
        float yawRate = _speed / WHEELBASE * std::tan(steerDeg * D2R); // rad/s
        // right-handed Y-up: positive yaw turns +Z toward +X (left); flip so
        // steer=+1 (D key) turns right
        _yawDeg -= yawRate * R2D * dt;
    }

    float yr = _yawDeg * D2R;
    Vec3 forward(std::sin(yr), 0.0f, std::cos(yr));
    _pos += forward * (_speed * dt);

    _carPivot->setPosition(_pos);
    _carPivot->setRotationFromEuler(0.0f, _yawDeg, 0.0f);

    // wheel dressing: spin with speed, front pair mirrors the steering angle
    _wheelSpinDeg += _speed / WHEEL_RADIUS * R2D * dt;
    if (_wheelSpinDeg > 360.0f) _wheelSpinDeg -= 360.0f;
    if (_wheelSpinDeg < -360.0f) _wheelSpinDeg += 360.0f;
    if (_wheelFL) _wheelFL->setRotationFromEuler(_wheelSpinDeg, -_steerVisDeg, 0.0f);
    if (_wheelFR) _wheelFR->setRotationFromEuler(_wheelSpinDeg, -_steerVisDeg, 0.0f);
    if (_wheelBL) _wheelBL->setRotationFromEuler(_wheelSpinDeg, 0.0f, 0.0f);
    if (_wheelBR) _wheelBR->setRotationFromEuler(_wheelSpinDeg, 0.0f, 0.0f);
}

void RaceWorld::updateLap(float dt) {
    if (std::fabs(_speed) > 0.3f) _lapStarted = true;
    if (!_lapStarted) return;

    _lapTime += dt;
    float angle = std::atan2(_pos.z, _pos.x);
    float d = angle - _prevAngle;
    if (d > PI) d -= 2.0f * PI;
    if (d < -PI) d += 2.0f * PI;
    _prevAngle = angle;
    _angleAccum += d;

    // driving counter-clockwise (+Z at start) accumulates positive angle
    if (_angleAccum >= 2.0f * PI) {
        _lap++;
        bool best = _bestLap <= 0.0f || _lapTime < _bestLap;
        if (best) _bestLap = _lapTime;
        CC_LOG_INFO("RaceWorld: LAP %d done in %.2fs%s (best %.2fs)",
                    _lap, _lapTime, best ? " — NEW BEST" : "", _bestLap);
        _lapTime = 0.0f;
        _angleAccum -= 2.0f * PI;
    } else if (_angleAccum <= -2.0f * PI) {
        CC_LOG_INFO("RaceWorld: lap ignored — wrong way around the track");
        _angleAccum += 2.0f * PI;
        _lapTime = 0.0f;
    }
}

void RaceWorld::update(float dt, const CarInput &input) {
    if (!_carPivot) return;
    if (input.reset) {
        resetCar();
        CC_LOG_INFO("RaceWorld: car reset to start line");
        return;
    }
    dt = std::min(dt, 0.05f); // clamp huge hitches so integration stays sane
    stepKinematics(dt, input);
    updateLap(dt);
}

void RaceWorld::destroy() {
    for (auto *r : _renderers) delete r;
    _renderers.clear();
#ifdef USE_TINYUSDZ
    _usd.destroy();
#endif
    _nodes.clear();
    _carPivot = nullptr;
    _wheelFL = _wheelFR = _wheelBL = _wheelBR = nullptr;
}
