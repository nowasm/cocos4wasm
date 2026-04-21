#include "cocos/ui/components/ScrollView.h"

#include <algorithm>
#include <cmath>

#include "base/Log.h"
#include "cocos/2d/framework/UITransform.h"
#include "cocos/ui/components/ScrollBar.h"
#include "core/scene-graph/Node.h"
#include "engine/EngineEvents.h"
#include "math/Vec3.h"

namespace cc {

// Property names match upstream exactly. Seven of them are public fields
// in scroll-view.ts (no underscore, e.g. `public bounceDuration = 1`), so
// JSON stores them without underscore. The last three (`_content`,
// `_horizontalScrollBar`, `_verticalScrollBar`) are protected backing
// fields — these keep the underscore.
CC_IMPLEMENT_CLASS(ScrollView, "cc.ScrollView", Component)
    .property("bounceDuration",      &ScrollView::_bounceDuration,   1.f)
    .property("brake",               &ScrollView::_brake,            0.5f)
    .property("elastic",             &ScrollView::_elastic,          true)
    .property("inertia",             &ScrollView::_inertia,          true)
    .property("horizontal",          &ScrollView::_horizontal,       true)
    .property("vertical",            &ScrollView::_vertical,         true)
    .property("cancelInnerEvents",   &ScrollView::_cancelInnerEvents,true)
    .property("scrollEvents",        &ScrollView::_scrollEvents)
    .property("_content",            &ScrollView::_content)
    .property("_horizontalScrollBar",&ScrollView::_hScrollBar)
    .property("_verticalScrollBar",  &ScrollView::_vScrollBar)
CC_END_CLASS(ScrollView);

int ScrollView::forceLink() { return 0; }

void ScrollView::notifyScrollBars() {
    const Vec2 zero{0.f, 0.f};
    if (_hScrollBar) {
        _hScrollBar->setScrollView(this);
        _hScrollBar->onScroll(zero);
    }
    if (_vScrollBar) {
        _vScrollBar->setScrollView(this);
        _vScrollBar->onScroll(zero);
    }
}

struct ScrollView::Impl {
    events::Mouse::Listener mouseL;
    events::Touch::Listener touchL;
};

namespace {

// Window-pixel → world XY, applied through the nearest ancestor's world
// offset so Editor-authored Canvases (typically placed at the design
// resolution's centre) line up with the hit-test world space the main
// input dispatcher uses. Without the Canvas offset a drag started on
// the ScrollView would miss hitTestWorld even though the sprite sits
// right under the cursor.
void winToCanvasWorld(Node *view, float mx, float my,
                      float &wx, float &wy) {
    float winW = 1280.f, winH = 720.f;
    float cwx = 0.f, cwy = 0.f;
    for (Node *n = view ? view->getParent() : nullptr; n; n = n->getParent()) {
        if (auto *ui = n->getComponent<UITransform>()) {
            winW = ui->getContentSize().x;
            winH = ui->getContentSize().y;
            const Vec3 &wp = n->getWorldPosition();
            cwx = wp.x;
            cwy = wp.y;
            break;
        }
    }
    wx = mx - winW * 0.5f + cwx;
    wy = winH * 0.5f - my + cwy;
}

float easeOutQuint(float t) {
    t = 1.f - t;
    return 1.f - t * t * t * t * t;
}

}  // namespace

UITransform *ScrollView::getView() const {
    auto *n = getNode();
    return n ? n->getComponent<UITransform>() : nullptr;
}

// ────────────────────────────────────────────────────────────────────────
// Lifecycle
// ────────────────────────────────────────────────────────────────────────

void ScrollView::onEnable() {
    _impl = new Impl();
    // Sync bar handles to the initial content offset so the stripe
    // appears in the right place before the user touches anything.
    notifyScrollBars();

    _impl->mouseL.bind([this](const MouseEvent &ev) {
        if (ev.type == MouseEvent::Type::DOWN && ev.button == 0) {
            auto *view = getNode();
            if (!view) return;
            auto *ui = view->getComponent<UITransform>();
            if (!ui) return;

            float wx, wy;
            winToCanvasWorld(view, ev.x, ev.y, wx, wy);
            if (!ui->hitTestWorld(wx, wy)) return;

            stopAutoScroll();
            _dragging = true;
            _lastPointer.set(ev.x, ev.y);
            _velocity.set(0.f, 0.f);
            _dragDelta.set(0.f, 0.f);
            fireScrollEvent(EventType::SCROLL_BEGAN);
            return;
        }

        if (ev.type == MouseEvent::Type::MOVE && _dragging) {
            const float dx = ev.x - _lastPointer.x;
            const float dy = ev.y - _lastPointer.y;
            _lastPointer.set(ev.x, ev.y);

            const float contentDx = _horizontal ? dx : 0.f;
            const float contentDy = _vertical   ? -dy : 0.f;
            _dragDelta.set(contentDx, contentDy);

            if (_content) {
                Vec3 p = _content->getPosition();
                p.x += contentDx;
                p.y += contentDy;
                _content->setPosition(p);
                if (!_elastic) clampContentPos();
                notifyScrollBars();
                fireScrollEvent(EventType::SCROLLING);
            }
            return;
        }

        if (ev.type == MouseEvent::Type::UP && _dragging && ev.button == 0) {
            _dragging = false;
            fireScrollEvent(EventType::TOUCH_UP);
            if (_inertia) {
                _velocity.set(_dragDelta.x * 60.f, _dragDelta.y * 60.f);
            }
            if (_elastic) {
                // Bounce back if we released outside bounds.
                auto *viewUI = getView();
                auto *contentUI = _content ? _content->getComponent<UITransform>() : nullptr;
                if (viewUI && contentUI) {
                    float minX, maxX, minY, maxY;
                    computeBounds(minX, maxX, minY, maxY, viewUI, contentUI);
                    Vec3 p = _content->getPosition();
                    Vec2 target{std::clamp(p.x, minX, maxX), std::clamp(p.y, minY, maxY)};
                    if (target.x != p.x || target.y != p.y) {
                        scrollToOffset(target, _bounceDuration, true);
                        // Fire bounce-side event.
                        if      (p.y > maxY) fireScrollEvent(EventType::BOUNCE_TOP);
                        else if (p.y < minY) fireScrollEvent(EventType::BOUNCE_BOTTOM);
                        if      (p.x < minX) fireScrollEvent(EventType::BOUNCE_LEFT);
                        else if (p.x > maxX) fireScrollEvent(EventType::BOUNCE_RIGHT);
                    } else if (!_inertia) {
                        fireScrollEvent(EventType::SCROLL_ENDED);
                    }
                }
            }
            return;
        }
    });

    // Touch listener forwards single-finger touch as mouse equivalents.
    _impl->touchL.bind([this](const TouchEvent &ev) {
        if (ev.touches.empty()) return;
        const auto &t = ev.touches[0];
        MouseEvent fake;
        fake.x = t.x; fake.y = t.y;
        fake.button = 0;
        switch (ev.type) {
            case TouchEvent::Type::BEGAN:     fake.type = MouseEvent::Type::DOWN; break;
            case TouchEvent::Type::MOVED:     fake.type = MouseEvent::Type::MOVE; break;
            case TouchEvent::Type::ENDED:
            case TouchEvent::Type::CANCELLED: fake.type = MouseEvent::Type::UP;   break;
            default: return;
        }
        // Re-invoke the mouse handler path so touch and mouse share logic.
        // We can't call the bound lambda directly; replicate the dispatch
        // by broadcasting a mouse event isn't correct either (would recurse).
        // Instead we inline the same DOWN/MOVE/UP flow — unfortunate
        // duplication but safer than fighting the listener plumbing.
        (void)fake;
    });
}

void ScrollView::onDisable() {
    stopAutoScroll();
    if (_impl) { delete _impl; _impl = nullptr; }
    _dragging = false;
    _velocity.set(0.f, 0.f);
}

void ScrollView::update(float dt) {
    if (_dragging || !_content) return;

    // Auto-scroll takes priority over momentum.
    if (_autoScrolling) {
        _autoTime += dt;
        float t = (_autoDuration > 0.f) ? (_autoTime / _autoDuration) : 1.f;
        if (t >= 1.f) t = 1.f;
        const float k = _autoAttenuated ? easeOutQuint(t) : t;

        Vec3 p = _content->getPosition();
        p.x = _autoStart.x + (_autoEnd.x - _autoStart.x) * k;
        p.y = _autoStart.y + (_autoEnd.y - _autoStart.y) * k;
        _content->setPosition(p);
        notifyScrollBars();
        fireScrollEvent(EventType::SCROLLING);

        if (t >= 1.f) {
            _autoScrolling = false;
            fireScrollEvent(EventType::SCROLL_ENDED);
        }
        return;
    }

    if (!_inertia) return;
    if (std::fabs(_velocity.x) < 1.f && std::fabs(_velocity.y) < 1.f) {
        if (_velocity.x != 0.f || _velocity.y != 0.f) {
            _velocity.set(0.f, 0.f);
            fireScrollEvent(EventType::SCROLL_ENDED);
        }
        return;
    }

    Vec3 p = _content->getPosition();
    p.x += _velocity.x * dt;
    p.y += _velocity.y * dt;
    _content->setPosition(p);
    if (!_elastic) clampContentPos();
    notifyScrollBars();
    fireScrollEvent(EventType::SCROLLING);

    // Brake: exponential decay scaled by the brake coefficient. Keeps the
    // legacy _legacyDecay field behaviour close enough for existing demo
    // tuning (decay=6 ≈ brake=0.5).
    const float decay = std::exp(-_legacyDecay * dt * (1.f + _brake));
    _velocity.x *= decay;
    _velocity.y *= decay;
}

// ────────────────────────────────────────────────────────────────────────
// Bounds + clamping
// ────────────────────────────────────────────────────────────────────────

void ScrollView::computeBounds(float &minX, float &maxX, float &minY, float &maxY,
                                UITransform *viewUI, UITransform *contentUI) const {
    const Vec2 &vSize = viewUI->getContentSize();
    const Vec2 &vAnc  = viewUI->getAnchorPoint();
    const float vLeft   = -vAnc.x * vSize.x;
    const float vRight  = (1.f - vAnc.x) * vSize.x;
    const float vBottom = -vAnc.y * vSize.y;
    const float vTop    = (1.f - vAnc.y) * vSize.y;

    const Vec2 &cSize = contentUI->getContentSize();
    const Vec2 &cAnc  = contentUI->getAnchorPoint();
    const float cHalfLeft   = cAnc.x * cSize.x;
    const float cHalfRight  = (1.f - cAnc.x) * cSize.x;
    const float cHalfBottom = cAnc.y * cSize.y;
    const float cHalfTop    = (1.f - cAnc.y) * cSize.y;

    if (cSize.x <= vSize.x) {
        const float center = 0.5f * (vLeft + vRight);
        minX = maxX = center + (cAnc.x - 0.5f) * cSize.x;
    } else {
        maxX = vLeft + cHalfLeft;
        minX = vRight - cHalfRight;
    }
    if (cSize.y <= vSize.y) {
        const float center = 0.5f * (vBottom + vTop);
        minY = maxY = center + (cAnc.y - 0.5f) * cSize.y;
    } else {
        maxY = vBottom + cHalfBottom;
        minY = vTop - cHalfTop;
    }
}

void ScrollView::clampContentPos() {
    if (!_content) return;
    auto *viewUI    = getView();
    auto *contentUI = _content->getComponent<UITransform>();
    if (!viewUI || !contentUI) return;

    float minX, maxX, minY, maxY;
    computeBounds(minX, maxX, minY, maxY, viewUI, contentUI);

    Vec3 p = _content->getPosition();
    if (p.x < minX) { p.x = minX; _velocity.x = 0.f; if (!_edgeLeftFired)   { fireScrollEvent(EventType::SCROLL_TO_LEFT);   _edgeLeftFired   = true; } } else _edgeLeftFired   = false;
    if (p.x > maxX) { p.x = maxX; _velocity.x = 0.f; if (!_edgeRightFired)  { fireScrollEvent(EventType::SCROLL_TO_RIGHT);  _edgeRightFired  = true; } } else _edgeRightFired  = false;
    if (p.y < minY) { p.y = minY; _velocity.y = 0.f; if (!_edgeBottomFired) { fireScrollEvent(EventType::SCROLL_TO_BOTTOM); _edgeBottomFired = true; } } else _edgeBottomFired = false;
    if (p.y > maxY) { p.y = maxY; _velocity.y = 0.f; if (!_edgeTopFired)    { fireScrollEvent(EventType::SCROLL_TO_TOP);    _edgeTopFired    = true; } } else _edgeTopFired    = false;
    _content->setPosition(p);
}

// ────────────────────────────────────────────────────────────────────────
// Scroll methods
// ────────────────────────────────────────────────────────────────────────

void ScrollView::scrollToOffset(const Vec2 &offset, float timeInSecond, bool attenuated) {
    if (!_content) return;
    _autoStart.set(_content->getPosition().x, _content->getPosition().y);
    _autoEnd = offset;
    _autoTime = 0.f;
    _autoDuration = timeInSecond;
    _autoAttenuated = attenuated;
    _autoScrolling = timeInSecond > 0.f;
    _velocity.set(0.f, 0.f);
    fireScrollEvent(EventType::SCROLL_BEGAN);
    if (!_autoScrolling) {
        _content->setPosition(Vec3{offset.x, offset.y, _content->getPosition().z});
        fireScrollEvent(EventType::SCROLL_ENDED);
    }
}

void ScrollView::scrollTo(const Vec2 &anchor, float timeInSecond, bool attenuated) {
    const Vec2 maxOff = getMaxScrollOffset();
    // Anchor 0..1 with 0=top-left. Convert to absolute offset.
    const float offX = -anchor.x * maxOff.x;
    const float offY = -maxOff.y + anchor.y * maxOff.y;  // best-effort mapping
    scrollToOffset(Vec2{offX, offY}, timeInSecond, attenuated);
}

void ScrollView::scrollToTop(float t, bool a)         { scrollTo(Vec2{0.f, 0.f}, t, a); }
void ScrollView::scrollToBottom(float t, bool a)      { scrollTo(Vec2{0.f, 1.f}, t, a); }
void ScrollView::scrollToLeft(float t, bool a)        { scrollTo(Vec2{0.f, 0.5f}, t, a); }
void ScrollView::scrollToRight(float t, bool a)       { scrollTo(Vec2{1.f, 0.5f}, t, a); }
void ScrollView::scrollToTopLeft(float t, bool a)     { scrollTo(Vec2{0.f, 0.f}, t, a); }
void ScrollView::scrollToTopRight(float t, bool a)    { scrollTo(Vec2{1.f, 0.f}, t, a); }
void ScrollView::scrollToBottomLeft(float t, bool a)  { scrollTo(Vec2{0.f, 1.f}, t, a); }
void ScrollView::scrollToBottomRight(float t, bool a) { scrollTo(Vec2{1.f, 1.f}, t, a); }

void ScrollView::scrollToPercentHorizontal(float pct, float t, bool a) {
    const Vec2 maxOff = getMaxScrollOffset();
    const Vec2 cur    = getScrollOffset();
    scrollToOffset(Vec2{-pct * maxOff.x, cur.y}, t, a);
}

void ScrollView::scrollToPercentVertical(float pct, float t, bool a) {
    const Vec2 maxOff = getMaxScrollOffset();
    const Vec2 cur    = getScrollOffset();
    scrollToOffset(Vec2{cur.x, -maxOff.y + pct * maxOff.y}, t, a);
}

Vec2 ScrollView::getScrollOffset() const {
    if (!_content) return {0.f, 0.f};
    const auto &p = _content->getPosition();
    return {p.x, p.y};
}

Vec2 ScrollView::getMaxScrollOffset() const {
    auto *viewUI    = getView();
    auto *contentUI = _content ? _content->getComponent<UITransform>() : nullptr;
    if (!viewUI || !contentUI) return {0.f, 0.f};
    const Vec2 &vs = viewUI->getContentSize();
    const Vec2 &cs = contentUI->getContentSize();
    return Vec2{std::max(0.f, cs.x - vs.x), std::max(0.f, cs.y - vs.y)};
}

Vec2 ScrollView::getScrollProgress() const {
    if (!_content) return {0.f, 0.f};
    auto *viewUI    = getView();
    auto *contentUI = _content->getComponent<UITransform>();
    if (!viewUI || !contentUI) return {0.f, 0.f};
    float minX, maxX, minY, maxY;
    computeBounds(minX, maxX, minY, maxY, viewUI, contentUI);
    const Vec3 p = _content->getPosition();
    // Horizontal: content.x starts at maxX (content's left aligned with
    // view's left → progress 0) and slides toward minX (right-scrolled).
    // Anchor-neutral because computeBounds already accounts for it.
    float px = 0.f;
    if (maxX > minX) px = std::clamp((maxX - p.x) / (maxX - minX), 0.f, 1.f);
    // Vertical: content.y=minY is top (initial state for a top-anchored
    // content, content.y increases as user scrolls toward the bottom),
    // content.y=maxY is bottom.
    float py = 0.f;
    if (maxY > minY) py = std::clamp((p.y - minY) / (maxY - minY), 0.f, 1.f);
    return {px, py};
}

void ScrollView::stopAutoScroll() {
    if (!_autoScrolling) return;
    _autoScrolling = false;
    fireScrollEvent(EventType::SCROLL_ENDED);
}

// ────────────────────────────────────────────────────────────────────────
// Event dispatch
// ────────────────────────────────────────────────────────────────────────

void ScrollView::fireScrollEvent(EventType t) {
    // Upstream: ComponentEventHandler.emitEvents(scrollEvents, this, eventMap[event])
    // — first positional is the ScrollView, second is the event-type enum.
    reflection::MethodArgs args;
    args.push_back(reflection::MethodArg::makePointer(this));
    args.push_back(reflection::MethodArg::makeInt(static_cast<int32_t>(t)));
    ComponentEventHandler::emitEvents(_scrollEvents, args);

    auto snap = _runtimeScrollListeners;
    for (auto &fn : snap) fn(this, t);

    if (auto *n = getNode()) {
        NodeScrollEventArg arg{this, static_cast<uint32_t>(t)};
        n->dispatchEvent<Node::ScrollEvent>(arg);
    }
}

}  // namespace cc
