/****************************************************************************
 BoxCollider — the authoring-layer `cc.BoxCollider` component (P8).

 Axis-aligned box shape in node-local space: `size` is the full extent
 (before node scale — the backend multiplies in the node's world scale),
 `center` (from Collider) offsets the box from the node origin.
****************************************************************************/

#pragma once

#include "physics/framework/Collider.h"

namespace cc {

namespace physics {
class BoxShape;
}

class BoxCollider : public Collider {
    CC_CLASS_DECL(BoxCollider, Collider)
public:
    BoxCollider() = default;
    ~BoxCollider() override = default;

    const Vec3 &getSize() const { return _size; }
    void setSize(const Vec3 &size);

    static int forceLink();

protected:
    physics::IBaseShape *createShape() override;
    void applyShapeProps() override;

private:
    // Concrete alias of Collider::_shape (IBaseShape is a virtual base of
    // the facade classes, so downcasting is not possible — keep the typed
    // pointer alongside). Only dereferenced while `_shape` is non-null.
    physics::BoxShape *_boxShape{nullptr};
    Vec3 _size{1.F, 1.F, 1.F};
};

} // namespace cc
