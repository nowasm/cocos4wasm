#include "cocos/ui/components/ScrollBar.h"

#include <algorithm>

#include "cocos/2d/components/Sprite.h"
#include "cocos/2d/framework/UITransform.h"
#include "cocos/ui/components/ScrollView.h"
#include "core/scene-graph/Node.h"
#include "math/Vec3.h"

namespace cc {

CC_IMPLEMENT_CLASS(ScrollBar, "cc.ScrollBar", Component)
    .property("_scrollView",     &ScrollBar::_scrollView)
    .property("_handle",         &ScrollBar::_handle)
    .property("_direction",      &ScrollBar::_direction,      Direction::HORIZONTAL)
    .property("_enableAutoHide", &ScrollBar::_enableAutoHide, false)
    .property("_autoHideTime",   &ScrollBar::_autoHideTime,   1.f)
CC_END_CLASS(ScrollBar);

int ScrollBar::forceLink() { return 0; }

void ScrollBar::onEnable() {
    if (_enableAutoHide) {
        _opacity = 0.f;
        _visible = false;
    } else {
        _opacity = 1.f;
        _visible = true;
    }
}

void ScrollBar::update(float dt) {
    if (!_enableAutoHide) return;
    if (_autoHideRemaining > 0.f) {
        _autoHideRemaining -= dt;
        if (_autoHideRemaining <= 0.f) {
            _autoHideRemaining = 0.f;
            _visible = false;
        }
    }
    // Fade the opacity toward its target (visible=1, hidden=0) over
    // ~0.3 s so transitions feel smooth.
    const float target = _visible ? 1.f : 0.f;
    const float step = dt * 3.f;
    if (_opacity < target) _opacity = std::min(target, _opacity + step);
    else if (_opacity > target) _opacity = std::max(target, _opacity - step);

    if (_handle) {
        Color c = _handle->getColor();
        c.a = static_cast<uint8_t>(_opacity * 255.f);
        _handle->setColor(c);
    }
}

void ScrollBar::onScroll(const Vec2 & /*outOfBoundary*/) {
    if (!_handle || !_scrollView) return;

    auto *barNode = getNode();
    auto *barUI = barNode ? barNode->getComponent<UITransform>() : nullptr;
    if (!barUI) return;

    auto *view = _scrollView->getView();
    auto *content = _scrollView->getContent();
    auto *contentUI = content ? content->getComponent<UITransform>() : nullptr;
    if (!view || !contentUI) return;

    const Vec2 &vSize = view->getContentSize();
    const Vec2 &cSize = contentUI->getContentSize();
    // Anchor-neutral [0..1] position along each axis — 0 at top/left
    // extreme, 1 at bottom/right — so the handle matches the visible
    // content stripe regardless of the content node's anchor point.
    const Vec2 progress = _scrollView->getScrollProgress();

    const Vec2 &trackSize   = barUI->getContentSize();
    const Vec2 &trackAnchor = barUI->getAnchorPoint();
    auto *handleUI = _handle->getNode()->getComponent<UITransform>();
    // Handle rect in track-local space, independent of the handle node's
    // own anchor. We then convert back to node-position using that anchor
    // so handles authored with any anchor (prefab default is (0, 0) for
    // the vertical scrollbar, (1, 1) for some skins) land correctly.
    const Vec2 handleAnchor = handleUI ? handleUI->getAnchorPoint()
                                       : Vec2{0.5f, 0.5f};
    const float trackLeft   = -trackAnchor.x * trackSize.x;
    const float trackRight  = (1.f - trackAnchor.x) * trackSize.x;
    const float trackBottom = -trackAnchor.y * trackSize.y;
    const float trackTop    = (1.f - trackAnchor.y) * trackSize.y;

    if (_direction == Direction::HORIZONTAL) {
        const float ratio  = (cSize.x > 0.f) ? std::min(1.f, vSize.x / cSize.x) : 1.f;
        const float handleW = std::max(10.f, trackSize.x * ratio);
        const float handleH = trackSize.y;  // full-height stripe
        _handle->setSize(handleW, handleH);
        if (handleUI) handleUI->setContentSize(handleW, handleH);

        const float t = progress.x;

        // Visual rect of the handle inside the track: left slides from
        // trackLeft to (trackRight - handleW).
        const float visualLeft = trackLeft + t * (trackSize.x - handleW);
        const float visualBottom = trackBottom;  // stripes full track height
        const Vec3 newPos{visualLeft + handleAnchor.x * handleW,
                          visualBottom + handleAnchor.y * handleH,
                          _handle->getNode()->getPosition().z};
        _handle->getNode()->setPosition(newPos);
    } else {
        const float ratio  = (cSize.y > 0.f) ? std::min(1.f, vSize.y / cSize.y) : 1.f;
        const float handleH = std::max(10.f, trackSize.y * ratio);
        const float handleW = trackSize.x;  // full-width stripe
        _handle->setSize(handleW, handleH);
        if (handleUI) handleUI->setContentSize(handleW, handleH);

        const float t = progress.y;

        // Handle's TOP slides from trackTop down to trackTop - (trackH - handleH).
        const float visualTop    = trackTop - t * (trackSize.y - handleH);
        const float visualLeft   = trackLeft;
        const float visualBottom = visualTop - handleH;
        const Vec3 newPos{visualLeft + handleAnchor.x * handleW,
                          visualBottom + handleAnchor.y * handleH,
                          _handle->getNode()->getPosition().z};
        _handle->getNode()->setPosition(newPos);
    }

    show();
}

void ScrollBar::hide() { _autoHideRemaining = 0.f; _visible = false; }
void ScrollBar::show() {
    _visible = true;
    if (_enableAutoHide) _autoHideRemaining = _autoHideTime;
}

void ScrollBar::onTouchBegan() { show(); _autoHideRemaining = 0.f; }
void ScrollBar::onTouchEnded() { show(); }  // re-arms via show()

}  // namespace cc
