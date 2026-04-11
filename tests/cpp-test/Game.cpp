#include "Game.h"
#include "base/Log.h"

int Game::init() {
    _windowInfo.title = "Cocos C++ Test";
    _windowInfo.width = 800;
    _windowInfo.height = 600;

    int ret = BaseGame::init();
    if (ret != 0) {
        return ret;
    }

    CC_LOG_INFO("=== Pure C++ Engine initialized successfully! ===");

    // Subscribe to input events using the native C++ event bus.
    // No JS EventDispatcher needed - events come directly from SDL/platform.
    _touchListener.bind([](const cc::TouchEvent &ev) {
        const char *typeStr = "unknown";
        switch (ev.type) {
            case cc::TouchEvent::Type::BEGAN: typeStr = "BEGAN"; break;
            case cc::TouchEvent::Type::MOVED: typeStr = "MOVED"; break;
            case cc::TouchEvent::Type::ENDED: typeStr = "ENDED"; break;
            case cc::TouchEvent::Type::CANCELLED: typeStr = "CANCELLED"; break;
            default: break;
        }
        if (!ev.touches.empty()) {
            CC_LOG_INFO("[Touch] %s at (%.0f, %.0f)", typeStr, ev.touches[0].x, ev.touches[0].y);
        }
    });

    _mouseListener.bind([](const cc::MouseEvent &ev) {
        if (ev.type == cc::MouseEvent::Type::DOWN) {
            CC_LOG_INFO("[Mouse] Button %d DOWN at (%.0f, %.0f)", ev.button, ev.x, ev.y);
        } else if (ev.type == cc::MouseEvent::Type::UP) {
            CC_LOG_INFO("[Mouse] Button %d UP at (%.0f, %.0f)", ev.button, ev.x, ev.y);
        }
    });

    _keyboardListener.bind([](const cc::KeyboardEvent &ev) {
        if (ev.action == cc::KeyboardEvent::Action::PRESS) {
            CC_LOG_INFO("[Keyboard] Key %d PRESSED", ev.key);
        } else if (ev.action == cc::KeyboardEvent::Action::RELEASE) {
            CC_LOG_INFO("[Keyboard] Key %d RELEASED", ev.key);
        }
    });

    return 0;
}

CC_REGISTER_APPLICATION(Game);
