#pragma once

#include "core/component/Component.h"
#include "math/Vec2.h"

namespace cc {

// 2D UI transform. Describes an axis-aligned rectangle in the parent
// Canvas's coordinate space:
//
//   anchorPoint  — normalised [0..1] point within the rect that aligns with
//                  the node's local position. (0.5, 0.5) centres it.
//   contentSize  — width/height in Canvas units (pixels for P2 Windows-
//                  first, to match the system window resolution).
//   priority     — sibling draw order within the owning Canvas. Higher
//                  values render on top.
//
// Geometric helpers (world AABB, hit-test, etc.) come in P4; the P2a
// responsibility is just to hold the data so Sprite/Label can read it.
class UITransform : public Component {
    CC_CLASS_DECL(UITransform, Component)
public:
    UITransform() = default;
    ~UITransform() override = default;

    const Vec2 &getAnchorPoint() const { return _anchorPoint; }
    void setAnchorPoint(const Vec2 &v) { _anchorPoint = v; }
    void setAnchorPoint(float x, float y) { _anchorPoint.set(x, y); }

    const Vec2 &getContentSize() const { return _contentSize; }
    void setContentSize(const Vec2 &v) { _contentSize = v; }
    void setContentSize(float w, float h) { _contentSize.set(w, h); }

    int32_t getPriority() const { return _priority; }
    void setPriority(int32_t v) { _priority = v; }

private:
    Vec2    _anchorPoint{0.5f, 0.5f};
    Vec2    _contentSize{100.0f, 100.0f};
    int32_t _priority{0};
};

}  // namespace cc
