#pragma once

#include "base/Ptr.h"
#include "core/component/Component.h"
#include "core/reflection/Reflection.h"
#include "math/Vec2.h"

namespace cc {

class Node;
class UITransform;

// Draggable viewport that pans an inner content node bigger than itself.
//
// Required structure:
//   scrollView    (has ScrollView + UITransform; this is the "view")
//     └─ content  (has UITransform; usually has a Mask at the view level
//                   so anything outside view bounds is clipped)
//
// MVP scope:
//   • Mouse drag pans the content in horizontal + vertical (honours
//     enabledH / enabledV flags — off disables drag on that axis).
//   • Momentum / inertia with exponential decay on release.
//   • Hard clamp at content bounds (no rubber-band bounce).
//   • Mouse + touch; primary-button mouse only.
//   • Listens to the engine bus directly so drag continues after the
//     pointer leaves the view bounds (Node events would stop firing the
//     moment hit-test lands elsewhere).
//
// Deferred until a demo demands them:
//   • Rubber-band / elastic edges
//   • Scroll bars
//   • Snap-to-cell
//   • Mouse-wheel scroll (trivial to add once needed)
class ScrollView : public Component {
    CC_CLASS_DECL(ScrollView, Component)
public:
    ScrollView() { _wantsUpdate = true; }
    ~ScrollView() override = default;

    void onEnable() override;
    void onDisable() override;
    void update(float dt) override;

    // Child node whose position is panned on drag. Usually set once at
    // setup; the ScrollView doesn't take ownership.
    void  setContent(Node *n) { _content = n; }
    Node *getContent() const  { return _content; }

    bool isHorizontalEnabled() const { return _enabledH; }
    bool isVerticalEnabled()   const { return _enabledV; }
    void setHorizontalEnabled(bool v) { _enabledH = v; }
    void setVerticalEnabled(bool v)   { _enabledV = v; }

    float getInertiaDecayPerSecond() const { return _decayPerSec; }
    void  setInertiaDecayPerSecond(float v) { _decayPerSec = v; }

    // Programmatic scroll. Offset is in content-local units; positive
    // offsets move the content *content-relative*, i.e. scrolling right
    // reveals the right side. Clamped to legal range.
    void scrollTo(float offsetX, float offsetY);
    Vec2 getScrollOffset() const;

private:
    void clampContentPos();
    void computeBounds(float &minX, float &maxX, float &minY, float &maxY,
                       UITransform *viewUI, UITransform *contentUI) const;

    struct Impl;
    Impl *_impl{nullptr};

    Node *_content{nullptr};

    bool _enabledH{true};
    bool _enabledV{true};

    // Exponential decay: velocity *= exp(-_decayPerSec * dt). ~6 kills
    // visible motion in ~half a second — feels like typical mobile UI.
    float _decayPerSec{6.0f};

    // Runtime state (not serialised).
    bool  _dragging{false};
    Vec2  _lastPointer{0.f, 0.f};  // window-space from the last MouseMove
    Vec2  _velocity{0.f, 0.f};      // px/sec in content-local axes
    double _lastMoveTime{0.0};
};

}  // namespace cc
