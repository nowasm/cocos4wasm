#include "cocos/ui/components/ScrollView.h"

#include <cmath>

#include "base/Log.h"
#include "cocos/2d/framework/UITransform.h"
#include "core/scene-graph/Node.h"
#include "engine/EngineEvents.h"
#include "math/Vec3.h"

namespace cc {

CC_IMPLEMENT_CLASS(ScrollView, "cc.ScrollView", Component)
    .property("horizontal", &ScrollView::_enabledH, true)
    .property("vertical",   &ScrollView::_enabledV, true)
    .property("decayPerSec",&ScrollView::_decayPerSec, 6.0f)
CC_END_CLASS(ScrollView);

// Engine-bus subscriptions. Declared out-of-line so ScrollView.h doesn't
// need to pull in the entire EngineEvents.h surface. Heap-allocated on
// onEnable, freed on onDisable.
struct ScrollView::Impl {
    events::Mouse::Listener mouseL;
    events::Touch::Listener touchL;
    // Last mouse-move delta (window space). On MouseUp we turn the most
    // recent delta into the initial inertia velocity — crude but works
    // for standard drag gestures. A ring buffer of samples would be more
    // robust against jittery last-frame pointer motion.
    Vec2  lastDelta{0.f, 0.f};
};

namespace {

// Map window-space (mouseX, mouseY) → Cocos UI world coords. Mirrors the
// transform InputEventDispatcher uses; kept local because ScrollView owns
// its own hit-test path (drag continues once armed even when the pointer
// leaves the view bounds).
void winToWorld(float mx, float my, float winW, float winH,
                float &wx, float &wy) {
    wx = mx - winW * 0.5f;
    wy = winH * 0.5f - my;
}

}  // namespace

void ScrollView::onEnable() {
    _impl = new Impl();

    _impl->mouseL.bind([this](const MouseEvent &ev) {
        // Hit-test: only arm drag when the down-press lands within the
        // view bounds. Subsequent MouseMove/Up use the armed flag; we
        // don't re-hit-test, so drag continues past the view edge.
        if (ev.type == MouseEvent::Type::DOWN && ev.button == 0) {
            auto *view = getNode();
            if (!view) return;
            auto *ui = view->getComponent<UITransform>();
            if (!ui) return;

            // Approximate the window size from the nearest ancestor
            // UITransform — the demo puts a 1280×720 rect on the Canvas
            // node, which matches the window. Avoids a dependency on
            // ISystemWindowManager here and is accurate for our MVP.
            float winW = 1280.f, winH = 720.f;
            for (Node *n = view->getParent(); n; n = n->getParent()) {
                if (auto *pui = n->getComponent<UITransform>()) {
                    winW = pui->getContentSize().x;
                    winH = pui->getContentSize().y;
                    break;
                }
            }
            float wx, wy;
            winToWorld(ev.x, ev.y, winW, winH, wx, wy);
            if (!ui->hitTestWorld(wx, wy)) return;

            _dragging = true;
            _lastPointer.set(ev.x, ev.y);
            _velocity.set(0.f, 0.f);
            _impl->lastDelta.set(0.f, 0.f);
            return;
        }

        if (ev.type == MouseEvent::Type::MOVE && _dragging) {
            const float dx = ev.x - _lastPointer.x;
            const float dy = ev.y - _lastPointer.y;
            _lastPointer.set(ev.x, ev.y);

            // Window Y grows downward; content uses Y-up. Invert to get
            // natural drag — mouse moving down pulls content down, so
            // content.y should decrease.
            const float contentDx = _enabledH ? dx : 0.f;
            const float contentDy = _enabledV ? -dy : 0.f;
            _impl->lastDelta.set(contentDx, contentDy);

            if (_content) {
                Vec3 p = _content->getPosition();
                p.x += contentDx;
                p.y += contentDy;
                _content->setPosition(p);
                clampContentPos();
            }
            return;
        }

        if (ev.type == MouseEvent::Type::UP && _dragging && ev.button == 0) {
            _dragging = false;
            // Promote the last frame-ish delta into a velocity in px/sec
            // assuming ~60 fps. Good enough for MVP feel; replace with a
            // sample-averaged estimate if jitter becomes visible.
            _velocity.set(_impl->lastDelta.x * 60.f,
                           _impl->lastDelta.y * 60.f);
            return;
        }
    });
}

void ScrollView::onDisable() {
    if (_impl) { delete _impl; _impl = nullptr; }
    _dragging = false;
    _velocity.set(0.f, 0.f);
}

void ScrollView::update(float dt) {
    if (_dragging || !_content) return;
    if (std::fabs(_velocity.x) < 1.f && std::fabs(_velocity.y) < 1.f) return;

    Vec3 p = _content->getPosition();
    p.x += _velocity.x * dt;
    p.y += _velocity.y * dt;
    _content->setPosition(p);
    clampContentPos();

    const float decay = std::exp(-_decayPerSec * dt);
    _velocity.x *= decay;
    _velocity.y *= decay;
}

void ScrollView::computeBounds(float &minX, float &maxX, float &minY, float &maxY,
                                UITransform *viewUI, UITransform *contentUI) const {
    // Content is a child of the view. We want to prevent the content's
    // edges from revealing the empty area inside the view's rect:
    //   maxX → content's left edge aligns with view's left edge
    //   minX → content's right edge aligns with view's right edge
    // Expressed in terms of content's local position (child-space of view):
    const Vec2 &vSize = viewUI->getContentSize();
    const Vec2 &vAnc  = viewUI->getAnchorPoint();
    const float vLeft   = -vAnc.x * vSize.x;
    const float vRight  = (1.f - vAnc.x) * vSize.x;
    const float vBottom = -vAnc.y * vSize.y;
    const float vTop    = (1.f - vAnc.y) * vSize.y;

    const Vec2 &cSize = contentUI->getContentSize();
    const Vec2 &cAnc  = contentUI->getAnchorPoint();
    const float cHalfLeft   = cAnc.x * cSize.x;       // distance from origin to left edge
    const float cHalfRight  = (1.f - cAnc.x) * cSize.x; // origin → right edge
    const float cHalfBottom = cAnc.y * cSize.y;
    const float cHalfTop    = (1.f - cAnc.y) * cSize.y;

    // content origin x such that content's left edge touches view's right
    // edge → origin x = vRight + cHalfLeft. That's the maximum.
    if (cSize.x <= vSize.x) {
        // Content narrower than view: clamp to centred-on-view.
        const float center = 0.5f * (vLeft + vRight);
        minX = maxX = center + (cAnc.x - 0.5f) * cSize.x;
    } else {
        maxX = vLeft + cHalfLeft;      // content left edge flush with view left
        minX = vRight - cHalfRight;    // content right edge flush with view right
    }
    if (cSize.y <= vSize.y) {
        const float center = 0.5f * (vBottom + vTop);
        minY = maxY = center + (cAnc.y - 0.5f) * cSize.y;
    } else {
        maxY = vBottom + cHalfBottom;
        minY = vTop    - cHalfTop;
    }
}

void ScrollView::clampContentPos() {
    if (!_content) return;
    auto *viewUI    = getNode()->getComponent<UITransform>();
    auto *contentUI = _content->getComponent<UITransform>();
    if (!viewUI || !contentUI) return;

    float minX, maxX, minY, maxY;
    computeBounds(minX, maxX, minY, maxY, viewUI, contentUI);

    Vec3 p = _content->getPosition();
    if (p.x < minX) { p.x = minX; _velocity.x = 0.f; }
    if (p.x > maxX) { p.x = maxX; _velocity.x = 0.f; }
    if (p.y < minY) { p.y = minY; _velocity.y = 0.f; }
    if (p.y > maxY) { p.y = maxY; _velocity.y = 0.f; }
    _content->setPosition(p);
}

void ScrollView::scrollTo(float offsetX, float offsetY) {
    if (!_content) return;
    Vec3 p = _content->getPosition();
    p.x = offsetX;
    p.y = offsetY;
    _content->setPosition(p);
    clampContentPos();
    _velocity.set(0.f, 0.f);
}

Vec2 ScrollView::getScrollOffset() const {
    if (!_content) return {0.f, 0.f};
    const auto &p = _content->getPosition();
    return {p.x, p.y};
}

}  // namespace cc
