#pragma once
#include <unordered_map>
#include <vector>
#include "cocos/game/CocosGame.h"
#ifdef USE_TINYUSDZ
    #include "cocos/game/USDLoader.h"
#endif

// Driver input for one simulation step, values already normalized.
struct CarInput {
    float throttle{0.0f}; // 0..1
    float brake{0.0f};    // 0..1
    float steer{0.0f};    // -1 (left) .. 1 (right)
    bool reset{false};
};

// Racing capability test — circular track + CC0 car (Kenney Car Kit, converted
// GLB -> USDA) + arcade car kinematics + lap timing. Engine physics stays off;
// the point is to exercise asset loading, per-frame transforms and input.
class RaceWorld {
public:
    void build(cc::scene::RenderScene *rs, cc::Root *root);
    void update(float dt, const CarInput &input);
    void destroy();

    const cc::Vec3 &carPosition() const { return _pos; }
    float carYawDeg() const { return _yawDeg; }
    float speed() const { return _speed; }
    int lapCount() const { return _lap; }
    float bestLap() const { return _bestLap; }

    static constexpr float TRACK_RADIUS = 30.0f;

private:
    cc::Node *box(const cc::Vec3 &center, const cc::Vec3 &size,
                  const cc::Color &color, float roughness = 0.9f,
                  cc::Node *parent = nullptr);
    cc::Material *material(const cc::Color &color, float roughness);

    void buildTrack();
    void buildCar();
    void buildFallbackCar(cc::Node *pivot);
    void resetCar();
    void stepKinematics(float dt, const CarInput &input);
    void updateLap(float dt);

    cc::scene::RenderScene *_rs{nullptr};
    cc::Root *_root{nullptr};
    cc::Mesh *_boxMesh{nullptr};
    std::unordered_map<uint64_t, cc::Material *> _matCache;
    std::vector<cc::Node *> _nodes;
    std::vector<cc::game::MeshRenderer *> _renderers;

#ifdef USE_TINYUSDZ
    cc::game::USDLoadResult _usd;
#endif
    cc::Node *_carPivot{nullptr};
    cc::Node *_wheelFL{nullptr};
    cc::Node *_wheelFR{nullptr};
    cc::Node *_wheelBL{nullptr};
    cc::Node *_wheelBR{nullptr};

    // car state
    cc::Vec3 _pos;
    float _yawDeg{0.0f};
    float _speed{0.0f};      // signed, m/s, + = forward
    float _steerVisDeg{0.0f};
    float _wheelSpinDeg{0.0f};

    // lap state
    float _lapTime{0.0f};
    float _bestLap{0.0f};
    float _angleAccum{0.0f};
    float _prevAngle{0.0f};
    int _lap{0};
    bool _lapStarted{false};
};
