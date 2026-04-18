#include "cocos/ui/components/Slider.h"

#include <algorithm>

#include "cocos/2d/components/Sprite.h"
#include "cocos/2d/framework/UITransform.h"
#include "core/scene-graph/Node.h"
#include "math/Vec3.h"

namespace cc {

CC_IMPLEMENT_CLASS(Slider, "cc.Slider", Component)
    .property("_direction", &Slider::_direction, Direction::Horizontal)
    .property("_progress",  &Slider::_progress,  0.1f)
CC_END_CLASS(Slider);

struct Slider::Hooks {
    cc::event::TargetEventID<Node::MouseDown> downId;
    cc::event::TargetEventID<Node::MouseMove> moveId;
    cc::event::TargetEventID<Node::MouseUp>   upId;
};

namespace {

// Map a local-space click to a 0..1 position along the slider track.
float localToProgress(const Vec2 &localPt, UITransform *trackUI,
                       Slider::Direction dir) {
    const Vec2 &sz = trackUI->getContentSize();
    const Vec2 &an = trackUI->getAnchorPoint();
    if (dir == Slider::Direction::Horizontal) {
        if (sz.x <= 0.f) return 0.f;
        const float left = -an.x * sz.x;
        return std::clamp((localPt.x - left) / sz.x, 0.f, 1.f);
    }
    if (sz.y <= 0.f) return 0.f;
    const float bottom = -an.y * sz.y;
    return std::clamp((localPt.y - bottom) / sz.y, 0.f, 1.f);
}

}  // namespace

void Slider::onEnable() {
    _applyProgress();
    auto *node = getNode();
    if (!node || _hooks) return;
    _hooks = new Hooks();

    _hooks->downId = node->on<Node::MouseDown>(
        [this](Node *, const NodeMouseEventArg &arg) {
            if (arg.button != 0) return;
            _dragging = true;
            auto *ui = getNode()->getComponent<UITransform>();
            if (!ui) return;
            _progress = localToProgress(Vec2{arg.localX, arg.localY}, ui, _direction);
            _applyProgress();
            _emitSlide();
        });

    _hooks->moveId = node->on<Node::MouseMove>(
        [this](Node *, const NodeMouseEventArg &arg) {
            if (!_dragging) return;
            auto *ui = getNode()->getComponent<UITransform>();
            if (!ui) return;
            _progress = localToProgress(Vec2{arg.localX, arg.localY}, ui, _direction);
            _applyProgress();
            _emitSlide();
        });

    _hooks->upId = node->on<Node::MouseUp>(
        [this](Node *, const NodeMouseEventArg &) { _dragging = false; });
}

void Slider::onDisable() {
    if (!_hooks) return;
    auto *node = getNode();
    if (node) {
        node->off<Node::MouseDown>(_hooks->downId);
        node->off<Node::MouseMove>(_hooks->moveId);
        node->off<Node::MouseUp>  (_hooks->upId);
    }
    delete _hooks;
    _hooks = nullptr;
}

void Slider::setHandle(Sprite *h) {
    _handle = h;
    _applyProgress();
}

void Slider::setProgress(float v) {
    v = std::clamp(v, 0.f, 1.f);
    if (_progress == v) return;
    _progress = v;
    _applyProgress();
    _emitSlide();
}

void Slider::_applyProgress() {
    if (!_handle) return;
    auto *handleNode = _handle->getNode();
    auto *trackUI = getNode() ? getNode()->getComponent<UITransform>() : nullptr;
    if (!handleNode || !trackUI) return;

    const Vec2 &sz = trackUI->getContentSize();
    const Vec2 &an = trackUI->getAnchorPoint();
    Vec3 p = handleNode->getPosition();
    if (_direction == Direction::Horizontal) {
        const float left = -an.x * sz.x;
        p.x = left + _progress * sz.x;
        p.y = 0.f;
    } else {
        const float bottom = -an.y * sz.y;
        p.x = 0.f;
        p.y = bottom + _progress * sz.y;
    }
    handleNode->setPosition(p);
}

void Slider::_emitSlide() {
    auto snap = _slideEvents;
    for (auto &fn : snap) fn(this);
}

}  // namespace cc
