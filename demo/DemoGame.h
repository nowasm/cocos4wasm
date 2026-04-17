#pragma once
#include "cocos/game/CocosGame.h"
#include "scenes/SceneRegistry.h"

class DemoGame : public cc::BaseGame {
public:
    int init() override;
    void onPause() override {}
    void onResume() override {}
    void onClose() override { BaseGame::onClose(); }

private:
    bool initEngine();
    void switchScene(int index);
    void applyPendingSceneSwitch();

    cc::events::Tick::Listener _tickListener;
    cc::events::Keyboard::Listener _keyboardListener;

    cc::scene::RenderScene *_renderScene{nullptr};
    cc::Node *_defaultCameraNode{nullptr};
    cc::scene::Camera *_defaultCamera{nullptr};
    cc::Node *_defaultLightNode{nullptr};
    cc::scene::DirectionalLight *_defaultLight{nullptr};

    int _currentSceneIdx{-1};
    int _pendingSceneIdx{-1};
    DemoScene *_currentScene{nullptr};
};
