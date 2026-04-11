#pragma once
#include "cocos/cocos.h"
#include "engine/EngineEvents.h"

class Game : public cc::BaseGame {
public:
    int init() override;
    void onPause() override {}
    void onResume() override {}
    void onClose() override { BaseGame::onClose(); }

private:
    cc::events::Touch::Listener _touchListener;
    cc::events::Mouse::Listener _mouseListener;
    cc::events::Keyboard::Listener _keyboardListener;
};
