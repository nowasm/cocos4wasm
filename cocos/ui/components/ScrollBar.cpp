#include "cocos/ui/components/ScrollBar.h"

#include <algorithm>

#include "cocos/2d/components/Sprite.h"
#include "cocos/2d/framework/UITransform.h"
#include "cocos/ui/components/ScrollView.h"
#include "core/scene-graph/Node.h"
#include "math/Vec3.h"

namespace cc {

CC_IMPLEMENT_CLASS(ScrollBar, "cc.ScrollBar", Component)
    .property("_direction",      &ScrollBar::_direction,      Direction::HORIZONTAL)
    .property("_enableAutoHide", &ScrollBar::_enableAutoHide, false)
    .property("_autoHideTime",   &ScrollBar::_autoHideTime,   1.f)
CC_END_CLASS(ScrollBar);

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
    const Vec2 scrollOffset = _scrollView->getScrollOffset();
    const Vec2 maxOffset = _scrollView->getMaxScrollOffset();

    const Vec2 &trackSize = barUI->getContentSize();
    const Vec2 &trackAnchor = barUI->getAnchorPoint();
    auto *handleUI = _handle->getNode()->getComponent<UITransform>();
    Vec3 handlePos = _handle->getNode()->getPosition();

    if (_direction == Direction::HORIZONTAL) {
        // Handle width ∝ view/content ratio, clamped so we always show
        // at least 10 px.
        const float ratio = (cSize.x > 0.f) ? std::min(1.f, vSize.x / cSize.x) : 1.f;
        const float handleW = std::max(10.f, trackSize.x * ratio);
        _handle->setSize(handleW, trackSize.y);
        if (handleUI) handleUI->setContentSize(handleW, trackSize.y);
        // Position along the track. scrollOffset.x goes from 0 (leftmost)
        // to -maxOffset.x (rightmost); normalise.
        const float t = (maxOffset.x > 0.f)
            ? std::clamp(-scrollOffset.x / maxOffset.x, 0.f, 1.f)
            : 0.f;
        const float left = -trackAnchor.x * trackSize.x;
        handlePos.x = left + t * (trackSize.x - handleW) + handleW * 0.5f;
        handlePos.y = 0.f;
    } else {
        const float ratio = (cSize.y > 0.f) ? std::min(1.f, vSize.y / cSize.y) : 1.f;
        const float handleH = std::max(10.f, trackSize.y * ratio);
        _handle->setSize(trackSize.x, handleH);
        if (handleUI) handleUI->setContentSize(trackSize.x, handleH);
        const float t = (maxOffset.y > 0.f)
            ? std::clamp(scrollOffset.y / maxOffset.y, 0.f, 1.f)
            : 0.f;
        const float top = (1.f - trackAnchor.y) * trackSize.y;
        handlePos.y = top - handleH * 0.5f - t * (trackSize.y - handleH);
        handlePos.x = 0.f;
    }
    _handle->getNode()->setPosition(handlePos);

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
