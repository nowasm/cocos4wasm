#include "cocos/2d/framework/UITransform.h"

#include "core/scene-graph/Node.h"
#include "math/Mat4.h"

namespace cc {

// Editor JSON stores these under `_anchorPoint` / `_contentSize` /
// `_priority` — all protected @serializable fields on the TS side, so
// the leading underscore is part of the JSON key.
CC_IMPLEMENT_CLASS(UITransform, "cc.UITransform", Component)
    .property("_anchorPoint", &UITransform::_anchorPoint, Vec2{0.5f, 0.5f})
    .property("_contentSize", &UITransform::_contentSize, Vec2{100.0f, 100.0f})
    .property("_priority",    &UITransform::_priority)
CC_END_CLASS(UITransform);

void UITransform::setAnchorPoint(const Vec2 &v) {
    if (_anchorPoint.x == v.x && _anchorPoint.y == v.y) return;
    _anchorPoint = v;
    if (auto *n = getNode()) {
        n->dispatchEvent<Node::AnchorChanged>();
    }
}

void UITransform::setContentSize(const Vec2 &v) {
    if (_contentSize.x == v.x && _contentSize.y == v.y) return;
    _contentSize = v;
    if (auto *n = getNode()) {
        n->dispatchEvent<Node::SizeChanged>();
    }
}

bool UITransform::hitTestWorld(float worldX, float worldY, Vec2 *outLocal) const {
    auto *node = getNode();
    if (!node) return false;

    // Invert the node's world matrix and transform the world point into
    // the UITransform's local coordinate frame. With anchor (0.5, 0.5) and
    // contentSize W×H, the rectangle is [-W/2, W/2] × [-H/2, H/2]; for a
    // general anchor (ax, ay) the bounds shift so the rectangle occupies
    // x ∈ [-ax·W, (1-ax)·W], y ∈ [-ay·H, (1-ay)·H].
    const Mat4 inv = node->getWorldMatrix().getInversed();

    // Only 2D coords matter — feed z=0 in, drop z on the way out.
    const float inX = worldX;
    const float inY = worldY;
    const float *m = inv.m;
    const float lx = m[0] * inX + m[4] * inY + m[12];
    const float ly = m[1] * inX + m[5] * inY + m[13];

    if (outLocal) outLocal->set(lx, ly);

    const float w = _contentSize.x;
    const float h = _contentSize.y;
    const float minX = -_anchorPoint.x * w;
    const float maxX = (1.0f - _anchorPoint.x) * w;
    const float minY = -_anchorPoint.y * h;
    const float maxY = (1.0f - _anchorPoint.y) * h;
    return lx >= minX && lx <= maxX && ly >= minY && ly <= maxY;
}

}  // namespace cc
