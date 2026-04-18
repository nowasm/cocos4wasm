#pragma once

#include <functional>

#include "base/Ptr.h"
#include "base/std/container/vector.h"
#include "core/component/Component.h"
#include "core/reflection/Reflection.h"
#include "math/Vec2.h"

namespace cc {

class Node;
class UITransform;
class ScrollBar;

// Mirrors cocos/ui/scrollView/scroll-view.ts. Drag / inertia / elastic
// bounce-back semantics match Creator; the SDL engine-bus path replaces
// the TS touch-manager hooks but the observable behaviour is the same.
//
// Required structure (matches TS):
//   scrollViewNode  (Sprite[optional bg] + Mask + ScrollView + UITransform)
//     └─ content    (UITransform — the panned child)
//
// Event channels (both fire together on every scroll-state change):
//   • scrollEvents — programmatic vector of Handler callbacks (Creator's
//     inspector-editable array).
//   • Node::ScrollEvent — typed event carrying (ScrollView*, EventType*)
//     for users who prefer on-node listeners.
class ScrollView : public Component {
    CC_CLASS_DECL(ScrollView, Component)
public:
    // cc.ScrollView.EventType string values — surfaced via the component
    // handler callback (EventType arg) and as the Node::ScrollEvent arg.
    enum class EventType : uint32_t {
        NONE = 0,
        SCROLL_TO_TOP,
        SCROLL_TO_BOTTOM,
        SCROLL_TO_LEFT,
        SCROLL_TO_RIGHT,
        SCROLL_BEGAN,
        SCROLL_ENDED,
        BOUNCE_TOP,
        BOUNCE_BOTTOM,
        BOUNCE_LEFT,
        BOUNCE_RIGHT,
        SCROLLING,
        SCROLL_ENG_WITH_THRESHOLD,
        TOUCH_UP,
    };

    using Handler = std::function<void(ScrollView *, EventType)>;

    ScrollView() { _wantsUpdate = true; }
    ~ScrollView() override = default;

    // ── Component lifecycle ──────────────────────────────────────────────
    void onEnable() override;
    void onDisable() override;
    void update(float dt) override;

    // ── Serialised properties (TS parity) ────────────────────────────────
    // Bounce-back duration in seconds (elastic mode).
    float getBounceDuration() const { return _bounceDuration; }
    void  setBounceDuration(float v) { _bounceDuration = v; }

    // Friction 0..1. 0 = frictionless (content glides forever), 1 = full
    // stop immediately. Matches Creator's `brake`.
    float getBrake() const { return _brake; }
    void  setBrake(float v) { _brake = v; }

    // Allow the content to be dragged past the boundary with a bounce-back.
    bool isElastic() const { return _elastic; }
    void setElastic(bool v) { _elastic = v; }

    // Momentum after release.
    bool hasInertia() const { return _inertia; }
    void setInertia(bool v) { _inertia = v; }

    // Panned child.
    void  setContent(Node *n) { _content = n; }
    Node *getContent() const { return _content; }

    bool isHorizontal() const { return _horizontal; }
    bool isVertical()   const { return _vertical; }
    void setHorizontal(bool v) { _horizontal = v; }
    void setVertical(bool v)   { _vertical = v; }

    // Paired scrollbars (visual feedback).
    ScrollBar *getHorizontalScrollBar() const { return _hScrollBar; }
    ScrollBar *getVerticalScrollBar()   const { return _vScrollBar; }
    void setHorizontalScrollBar(ScrollBar *b) { _hScrollBar = b; }
    void setVerticalScrollBar  (ScrollBar *b) { _vScrollBar = b; }

    bool getCancelInnerEvents() const { return _cancelInnerEvents; }
    void setCancelInnerEvents(bool v) { _cancelInnerEvents = v; }

    // View rect (read-only) — resolves to the ScrollView node's own
    // UITransform, which defines the clipping rectangle Mask uses.
    UITransform *getView() const;

    // ── scrollEvents handler vector ──────────────────────────────────────
    void addScrollListener(Handler fn) { _scrollEvents.push_back(std::move(fn)); }
    void clearScrollListeners() { _scrollEvents.clear(); }

    // ── Scroll methods (match TS signatures) ─────────────────────────────
    void scrollToBottom(float timeInSecond = 0.f, bool attenuated = true);
    void scrollToTop   (float timeInSecond = 0.f, bool attenuated = true);
    void scrollToLeft  (float timeInSecond = 0.f, bool attenuated = true);
    void scrollToRight (float timeInSecond = 0.f, bool attenuated = true);
    void scrollToTopLeft    (float timeInSecond = 0.f, bool attenuated = true);
    void scrollToTopRight   (float timeInSecond = 0.f, bool attenuated = true);
    void scrollToBottomLeft (float timeInSecond = 0.f, bool attenuated = true);
    void scrollToBottomRight(float timeInSecond = 0.f, bool attenuated = true);
    // Anchor is in [0..1] on each axis, top-left origin (matches TS).
    void scrollTo(const Vec2 &anchor, float timeInSecond = 0.f, bool attenuated = true);
    // Absolute content-local offset from the top-left.
    void scrollToOffset(const Vec2 &offset, float timeInSecond = 0.f, bool attenuated = true);
    void scrollToPercentHorizontal(float percent, float timeInSecond = 0.f, bool attenuated = true);
    void scrollToPercentVertical  (float percent, float timeInSecond = 0.f, bool attenuated = true);

    // Query helpers.
    Vec2 getScrollOffset() const;
    Vec2 getMaxScrollOffset() const;
    void stopAutoScroll();
    bool isScrolling() const { return _dragging; }
    bool isAutoScrolling() const { return _autoScrolling; }

    float getScrollEndedEventTiming() const { return _scrollEndedThreshold; }

    // Legacy convenience kept for earlier demos — Creator doesn't expose
    // this but our ScrollViewScene uses it.
    float getInertiaDecayPerSecond() const { return _legacyDecay; }
    void  setInertiaDecayPerSecond(float v) { _legacyDecay = v; }
    void setHorizontalEnabled(bool v) { setHorizontal(v); }
    void setVerticalEnabled(bool v)   { setVertical(v); }

private:
    void fireScrollEvent(EventType t);
    void clampContentPos();
    void computeBounds(float &minX, float &maxX, float &minY, float &maxY,
                        UITransform *viewUI, UITransform *contentUI) const;

    struct Impl;
    Impl *_impl{nullptr};

    // Serialised.
    Node      *_content{nullptr};
    ScrollBar *_hScrollBar{nullptr};
    ScrollBar *_vScrollBar{nullptr};
    float _bounceDuration{1.f};
    float _brake{0.5f};
    bool  _elastic{true};
    bool  _inertia{true};
    bool  _horizontal{true};
    bool  _vertical{true};
    bool  _cancelInnerEvents{true};

    float _legacyDecay{6.f};

    // Runtime.
    bool  _dragging{false};
    bool  _autoScrolling{false};
    Vec2  _lastPointer{0.f, 0.f};
    Vec2  _velocity{0.f, 0.f};
    Vec2  _dragDelta{0.f, 0.f};
    // Auto-scroll target state.
    Vec2  _autoStart{0.f, 0.f};
    Vec2  _autoEnd{0.f, 0.f};
    float _autoTime{0.f};
    float _autoDuration{0.f};
    bool  _autoAttenuated{true};
    // Fired-once edges to avoid spamming boundary events during inertia.
    bool  _edgeTopFired{false};
    bool  _edgeBottomFired{false};
    bool  _edgeLeftFired{false};
    bool  _edgeRightFired{false};
    float _scrollEndedThreshold{50.f};

    ccstd::vector<Handler> _scrollEvents;
};

}  // namespace cc
