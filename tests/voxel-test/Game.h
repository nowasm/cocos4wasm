#pragma once
#include "cocos/game/CocosGame.h"
#include "VoxelWorld.h"

// Voxel village demo — isometric orthographic recreation of example1.jpg.
class Game : public cc::BaseGame {
public:
    int init() override;
    void onPause() override {}
    void onResume() override {}
    void onClose() override { BaseGame::onClose(); }

private:
    bool initEngine();
    void updateCamera();

    cc::events::Tick::Listener _tickListener;
    cc::events::Mouse::Listener _mouseListener;
    cc::events::Keyboard::Listener _keyboardListener;

    cc::Node *_camNode{nullptr};
    cc::scene::Camera *_camera{nullptr};
    cc::scene::RenderScene *_renderScene{nullptr};
    cc::Node *_lightNode{nullptr};
    cc::scene::DirectionalLight *_light{nullptr};

    VoxelWorld _world;

    // isometric orbit (pitch stays fixed for the iso look)
    float _yaw{45.0f};
    float _pitch{32.0f};
    float _orthoHeight{17.0f};
    bool _dragging{false};
    float _lastMX{0}, _lastMY{0};
};
