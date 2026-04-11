#pragma once
#include "TestScene.h"

class Game : public cc::BaseGame {
public:
    int init() override;
    void onPause() override {}
    void onResume() override {}
    void onClose() override { BaseGame::onClose(); }

private:
    bool initEngine();
    void switchTest(int index);
    void doSwitchTest();
    void updateTitle();

    cc::events::Tick::Listener _tickListener;
    cc::events::Mouse::Listener _mouseListener;
    cc::events::Keyboard::Listener _keyboardListener;

    // orbit camera
    cc::Node *_camNode{nullptr};
    cc::scene::Camera *_camera{nullptr};
    cc::scene::RenderScene *_renderScene{nullptr};
    float _orbitYaw{45.0f};
    float _orbitPitch{-25.0f};
    float _orbitDist{15.0f};
    bool _dragging{false};
    float _lastMX{0}, _lastMY{0};

    // test management
    int _currentTest{-1};
    int _pendingTest{-1};  // deferred switch
    TestScene *_activeTest{nullptr};

    // directional light (shared across tests)
    cc::Node *_defaultLightNode{nullptr};
    cc::scene::DirectionalLight *_defaultLight{nullptr};
};
