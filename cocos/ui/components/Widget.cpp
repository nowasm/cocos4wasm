#include "cocos/ui/components/Widget.h"

#include "base/Log.h"
#include "cocos/2d/framework/UITransform.h"
#include "core/scene-graph/Node.h"
#include "math/Vec3.h"

namespace cc {

CC_IMPLEMENT_CLASS(Widget, "cc.Widget", Component)
    .property("_alignFlags", &Widget::_alignFlags, static_cast<uint32_t>(0))
    .property("_alignMode",  &Widget::_alignMode,  AlignMode::ON_WINDOW_RESIZE)
    .property("_top",        &Widget::_top)
    .property("_bottom",     &Widget::_bottom)
    .property("_left",       &Widget::_left)
    .property("_right",      &Widget::_right)
    .property("_hCenter",    &Widget::_hCenter)
    .property("_vCenter",    &Widget::_vCenter)
    .property("_absTop",     &Widget::_absTop,     true)
    .property("_absBottom",  &Widget::_absBottom,  true)
    .property("_absLeft",    &Widget::_absLeft,    true)
    .property("_absRight",   &Widget::_absRight,   true)
    .property("_absHCenter", &Widget::_absHCenter, true)
    .property("_absVCenter", &Widget::_absVCenter, true)
CC_END_CLASS(Widget);

struct Widget::TargetHook {
    Node *target{nullptr};
    cc::event::TargetEventID<Node::SizeChanged>   sizeId;
    cc::event::TargetEventID<Node::AnchorChanged> anchorId;
};

// ────────────────────────────────────────────────────────────────────────
// Lifecycle
// ────────────────────────────────────────────────────────────────────────

void Widget::onEnable() {
    bindTargetListener();
    _didAlignOnce = false;
    updateAlignment();
    _didAlignOnce = true;
}

void Widget::onDisable() {
    unbindTargetListener();
}

// ────────────────────────────────────────────────────────────────────────
// target / flags / mode setters
// ────────────────────────────────────────────────────────────────────────

Node *Widget::effectiveTarget() const {
    if (_target) return _target;
    return getNode() ? getNode()->getParent() : nullptr;
}

void Widget::setTarget(Node *n) {
    if (_target == n) return;
    _target = n;
    // Rewire the listener to the new target.
    if (_hook) {
        unbindTargetListener();
        bindTargetListener();
    }
    scheduleLayout();
}

void Widget::setAlignFlags(uint32_t v) {
    if (_alignFlags == v) return;
    _alignFlags = v;
    scheduleLayout();
}

void Widget::setAlignMode(AlignMode m) {
    _alignMode = m;
}

void Widget::setAlignTop(bool v) {
    if (v) {
        _alignFlags = (_alignFlags | TOP) & ~MID;  // enabling an edge clears centre
    } else {
        _alignFlags &= ~TOP;
    }
    scheduleLayout();
}
void Widget::setAlignBottom(bool v) {
    if (v) _alignFlags = (_alignFlags | BOT) & ~MID;
    else   _alignFlags &= ~BOT;
    scheduleLayout();
}
void Widget::setAlignLeft(bool v) {
    if (v) _alignFlags = (_alignFlags | LEFT) & ~CENTER;
    else   _alignFlags &= ~LEFT;
    scheduleLayout();
}
void Widget::setAlignRight(bool v) {
    if (v) _alignFlags = (_alignFlags | RIGHT) & ~CENTER;
    else   _alignFlags &= ~RIGHT;
    scheduleLayout();
}
void Widget::setAlignVerticalCenter(bool v) {
    if (v) _alignFlags = (_alignFlags | MID) & ~(TOP | BOT);
    else   _alignFlags &= ~MID;
    scheduleLayout();
}
void Widget::setAlignHorizontalCenter(bool v) {
    if (v) _alignFlags = (_alignFlags | CENTER) & ~(LEFT | RIGHT);
    else   _alignFlags &= ~CENTER;
    scheduleLayout();
}

void Widget::setPadding(float v) {
    _top = v; _bottom = v; _left = v; _right = v;
    scheduleLayout();
}

// ────────────────────────────────────────────────────────────────────────
// Target listener
// ────────────────────────────────────────────────────────────────────────

void Widget::bindTargetListener() {
    Node *t = effectiveTarget();
    if (!t) return;
    if (_hook && _hook->target == t) return;
    _hook = new TargetHook();
    _hook->target = t;
    _hook->sizeId = t->on<Node::SizeChanged>([this](Node *) {
        if (_alignMode != AlignMode::ONCE) scheduleLayout();
    });
    _hook->anchorId = t->on<Node::AnchorChanged>([this](Node *) {
        if (_alignMode != AlignMode::ONCE) scheduleLayout();
    });
}

void Widget::unbindTargetListener() {
    if (!_hook) return;
    if (_hook->target) {
        _hook->target->off<Node::SizeChanged>  (_hook->sizeId);
        _hook->target->off<Node::AnchorChanged>(_hook->anchorId);
    }
    delete _hook;
    _hook = nullptr;
}

void Widget::scheduleLayout() {
    auto *n = getNode();
    if (!n || !n->isActiveInHierarchy()) return;
    if (_alignMode == AlignMode::ONCE && _didAlignOnce) return;
    updateAlignment();
}

// ────────────────────────────────────────────────────────────────────────
// Alignment math
// ────────────────────────────────────────────────────────────────────────

void Widget::updateAlignment() {
    if (_alignFlags == 0) return;

    auto *node = getNode();
    if (!node) return;
    auto *ui = node->getComponent<UITransform>();
    if (!ui) return;

    Node *parent = effectiveTarget();
    if (!parent) return;
    auto *parentUI = parent->getComponent<UITransform>();
    if (!parentUI) return;

    const Vec2 &pSize   = parentUI->getContentSize();
    const Vec2 &pAnchor = parentUI->getAnchorPoint();
    const float pLeft   = -pAnchor.x * pSize.x;
    const float pRight  = (1.0f - pAnchor.x) * pSize.x;
    const float pBottom = -pAnchor.y * pSize.y;
    const float pTop    = (1.0f - pAnchor.y) * pSize.y;

    // Resolve pixel vs ratio margins against the parent dimensions.
    const float topPx  = _absTop     ? _top     : _top     * pSize.y;
    const float botPx  = _absBottom  ? _bottom  : _bottom  * pSize.y;
    const float leftPx = _absLeft    ? _left    : _left    * pSize.x;
    const float rightPx= _absRight   ? _right   : _right   * pSize.x;
    const float hcPx   = _absHCenter ? _hCenter : _hCenter * pSize.x;
    const float vcPx   = _absVCenter ? _vCenter : _vCenter * pSize.y;

    Vec2 selfSize = ui->getContentSize();
    const Vec2 &selfAnchor = ui->getAnchorPoint();
    Vec3 pos = node->getPosition();

    // ── Horizontal axis ──────────────────────────────────────────────
    const bool hasL = (_alignFlags & LEFT)   != 0;
    const bool hasR = (_alignFlags & RIGHT)  != 0;
    const bool hasC = (_alignFlags & CENTER) != 0;
    if (hasL && hasR) {
        selfSize.x = (pRight - rightPx) - (pLeft + leftPx);
        if (selfSize.x < 0.f) selfSize.x = 0.f;
        ui->setContentSize(selfSize);
        pos.x = (pLeft + leftPx) + selfAnchor.x * selfSize.x;
    } else if (hasL) {
        pos.x = (pLeft + leftPx) + selfAnchor.x * selfSize.x;
    } else if (hasR) {
        pos.x = (pRight - rightPx) - (1.f - selfAnchor.x) * selfSize.x;
    } else if (hasC) {
        const float pCenterX = 0.5f * (pLeft + pRight);
        pos.x = pCenterX + hcPx + (selfAnchor.x - 0.5f) * selfSize.x;
    }

    // ── Vertical axis ────────────────────────────────────────────────
    const bool hasT = (_alignFlags & TOP) != 0;
    const bool hasB = (_alignFlags & BOT) != 0;
    const bool hasM = (_alignFlags & MID) != 0;
    if (hasT && hasB) {
        selfSize.y = (pTop - topPx) - (pBottom + botPx);
        if (selfSize.y < 0.f) selfSize.y = 0.f;
        ui->setContentSize(selfSize);
        pos.y = (pBottom + botPx) + selfAnchor.y * selfSize.y;
    } else if (hasT) {
        pos.y = (pTop - topPx) - (1.f - selfAnchor.y) * selfSize.y;
    } else if (hasB) {
        pos.y = (pBottom + botPx) + selfAnchor.y * selfSize.y;
    } else if (hasM) {
        const float pCenterY = 0.5f * (pTop + pBottom);
        pos.y = pCenterY + vcPx + (selfAnchor.y - 0.5f) * selfSize.y;
    }

    node->setPosition(pos);
    _didAlignOnce = true;
}

}  // namespace cc
