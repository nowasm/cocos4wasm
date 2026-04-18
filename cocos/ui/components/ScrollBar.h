#pragma once

#include "core/component/Component.h"
#include "core/reflection/Reflection.h"
#include "math/Vec2.h"

namespace cc {

class Node;
class ScrollView;
class Sprite;

// Mirrors cocos/ui/scroll-bar.ts. A visual companion to ScrollView —
// the handle Sprite slides along the track as the content scrolls,
// sized proportionally to the visible-to-content ratio. Auto-hide
// fades it out after `autoHideTime` seconds of inactivity.
class ScrollBar : public Component {
    CC_CLASS_DECL(ScrollBar, Component)
public:
    // cc.ScrollBar.Direction.
    enum class Direction : uint32_t {
        HORIZONTAL = 0,
        VERTICAL   = 1,
    };

    ScrollBar() { _wantsUpdate = true; }
    ~ScrollBar() override = default;

    void onEnable() override;
    void update(float dt) override;

    Sprite *getHandle() const { return _handle; }
    void    setHandle(Sprite *h) { _handle = h; }

    Direction getDirection() const { return _direction; }
    void      setDirection(Direction d) { _direction = d; }

    bool isAutoHideEnabled() const { return _enableAutoHide; }
    void setAutoHideEnabled(bool v) { _enableAutoHide = v; }

    float getAutoHideTime() const { return _autoHideTime; }
    void  setAutoHideTime(float v) { _autoHideTime = v; }

    // Called by ScrollView when content position changes. Repositions
    // the handle along the track and resizes it based on the overlap
    // ratio. outOfBoundary is how far past the legal scroll range the
    // content currently is (unused in the simple MVP implementation).
    void onScroll(const Vec2 &outOfBoundary);

    // Hook wire-up — ScrollView calls this to announce itself so the
    // bar can measure content-vs-viewport ratio on demand.
    void setScrollView(ScrollView *sv) { _scrollView = sv; }

    // Immediately hide or show (no fade).
    void hide();
    void show();

    // Touch begin/end — ScrollBar reveals on touch and re-arms hide.
    void onTouchBegan();
    void onTouchEnded();

private:
    Sprite     *_handle{nullptr};
    ScrollView *_scrollView{nullptr};
    Direction   _direction{Direction::HORIZONTAL};
    bool        _enableAutoHide{false};
    float       _autoHideTime{1.f};

    float _autoHideRemaining{0.f};
    bool  _visible{true};
    float _opacity{1.f};  // 0..1; applied to handle's colour alpha each frame
};

}  // namespace cc
