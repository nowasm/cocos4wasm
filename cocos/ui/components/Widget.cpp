#include "cocos/ui/components/Widget.h"

#include "base/Log.h"
#include "cocos/2d/framework/UITransform.h"
#include "core/scene-graph/Node.h"
#include "math/Vec3.h"

namespace cc {

CC_IMPLEMENT_CLASS(Widget, "cc.Widget", Component)
    .property("alignFlags", &Widget::_align,   static_cast<uint32_t>(0))
    .property("top",        &Widget::_top)
    .property("bottom",     &Widget::_bottom)
    .property("left",       &Widget::_left)
    .property("right",      &Widget::_right)
    .property("hCenter",    &Widget::_hCenter)
    .property("vCenter",    &Widget::_vCenter)
CC_END_CLASS(Widget);

// Retained so we can detach the parent SizeChanged subscription cleanly.
// Stored out of the header because a forward-declared Node doesn't carry
// the event-template helpers.
struct Widget::ParentHook {
    Node *parent{nullptr};
    cc::event::TargetEventID<Node::SizeChanged> sizeId;
};

void Widget::onEnable() {
    bindParentListener();
    updateLayout();
}

void Widget::onDisable() {
    unbindParentListener();
}

void Widget::setAlign(uint32_t flags) {
    if (_align == flags) return;
    _align = flags;
    scheduleLayout();
}

void Widget::setAlignMode(AlignMode m) {
    _alignMode = m;
}

void Widget::scheduleLayout() {
    // Only re-apply automatically when the component is active; calling
    // updateLayout() pre-activation would read stale UITransform state.
    if (getNode() && getNode()->isActiveInHierarchy()) {
        updateLayout();
    }
}

void Widget::bindParentListener() {
    auto *node = getNode();
    if (!node) return;
    Node *parent = node->getParent();
    if (!parent) return;
    if (_hook && _hook->parent == parent) return;
    unbindParentListener();

    _hook = new ParentHook();
    _hook->parent = parent;
    _hook->sizeId = parent->on<Node::SizeChanged>(
        [this](Node * /*self*/) {
            if (_alignMode == AlignMode::ALWAYS) {
                updateLayout();
            }
        });
}

void Widget::unbindParentListener() {
    if (!_hook) return;
    if (_hook->parent) {
        _hook->parent->off<Node::SizeChanged>(_hook->sizeId);
    }
    delete _hook;
    _hook = nullptr;
}

void Widget::updateLayout() {
    if (_align == 0) return;

    auto *node = getNode();
    if (!node) return;
    auto *ui = node->getComponent<UITransform>();
    if (!ui) return;

    auto *parent = node->getParent();
    if (!parent) return;
    auto *parentUI = parent->getComponent<UITransform>();
    if (!parentUI) return;

    // Parent-local coords of parent's edges. Parent's origin is at (0,0)
    // in its own local frame; with anchor (ax, ay) and size (W, H), the
    // rectangle covers x ∈ [-ax·W, (1-ax)·W], y ∈ [-ay·H, (1-ay)·H].
    const Vec2 &pSize   = parentUI->getContentSize();
    const Vec2 &pAnchor = parentUI->getAnchorPoint();
    const float pLeft   = -pAnchor.x * pSize.x;
    const float pRight  = (1.0f - pAnchor.x) * pSize.x;
    const float pBottom = -pAnchor.y * pSize.y;
    const float pTop    = (1.0f - pAnchor.y) * pSize.y;

    // Our node's contentSize; may be stretched below if both edges aligned.
    Vec2 selfSize = ui->getContentSize();
    const Vec2 &selfAnchor = ui->getAnchorPoint();

    Vec3 pos = node->getPosition();

    // ── Horizontal ────────────────────────────────────────────────────
    const bool hasL = (_align & LEFT)  != 0;
    const bool hasR = (_align & RIGHT) != 0;
    if (hasL && hasR) {
        // Stretch: resize so that both margins are honoured, then centre.
        selfSize.x = (pRight - _right) - (pLeft + _left);
        if (selfSize.x < 0.f) selfSize.x = 0.f;
        ui->setContentSize(selfSize);
        // Node origin sits at anchor·size from the left edge; place it.
        pos.x = (pLeft + _left) + selfAnchor.x * selfSize.x;
    } else if (hasL) {
        pos.x = (pLeft + _left) + selfAnchor.x * selfSize.x;
    } else if (hasR) {
        pos.x = (pRight - _right) - (1.f - selfAnchor.x) * selfSize.x;
    } else if (_align & HORIZONTAL_CENTER) {
        // Centre the node's origin line at parent's horizontal centre,
        // offset by _hCenter. Works regardless of selfAnchor because the
        // user's _hCenter is applied to the origin point, not an edge.
        const float pCenterX = 0.5f * (pLeft + pRight);
        pos.x = pCenterX + _hCenter + (selfAnchor.x - 0.5f) * selfSize.x;
    }

    // ── Vertical ──────────────────────────────────────────────────────
    const bool hasT = (_align & TOP)    != 0;
    const bool hasB = (_align & BOTTOM) != 0;
    if (hasT && hasB) {
        selfSize.y = (pTop - _top) - (pBottom + _bottom);
        if (selfSize.y < 0.f) selfSize.y = 0.f;
        ui->setContentSize(selfSize);
        pos.y = (pBottom + _bottom) + selfAnchor.y * selfSize.y;
    } else if (hasT) {
        pos.y = (pTop - _top) - (1.f - selfAnchor.y) * selfSize.y;
    } else if (hasB) {
        pos.y = (pBottom + _bottom) + selfAnchor.y * selfSize.y;
    } else if (_align & VERTICAL_CENTER) {
        const float pCenterY = 0.5f * (pTop + pBottom);
        pos.y = pCenterY + _vCenter + (selfAnchor.y - 0.5f) * selfSize.y;
    }

    node->setPosition(pos);
}

}  // namespace cc
