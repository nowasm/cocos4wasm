#pragma once
#include "cocos/game/CocosGame.h"
#include "RaceWorld.h"

// Racing capability test — WASD arcade driving with a smooth chase camera.
class Game : public cc::BaseGame {
public:
    int init() override;
    void onPause() override {}
    void onResume() override {}
    void onClose() override { BaseGame::onClose(); }

private:
    bool initEngine();
    void tick(float dt);
    void updateCamera(float dt);

    cc::events::Tick::Listener _tickListener;
    cc::events::Keyboard::Listener _keyboardListener;

    cc::Node *_camNode{nullptr};
    cc::scene::Camera *_camera{nullptr};
    cc::scene::RenderScene *_renderScene{nullptr};
    cc::Node *_lightNode{nullptr};
    cc::scene::DirectionalLight *_light{nullptr};

    RaceWorld _world;

    // held-key state (events are edge-triggered PRESS/RELEASE)
    bool _keyW{false}, _keyA{false}, _keyS{false}, _keyD{false};
    bool _resetQueued{false};

    cc::Vec3 _camPos;
    bool _camInit{false};

    // COCOS_AUTODRIVE=1: scripted self-driving for unattended verification
    bool _autopilot{false};
    float _autoTime{0.0f};
    float _autoLogTimer{0.0f};
};
