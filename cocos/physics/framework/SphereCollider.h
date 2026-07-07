/****************************************************************************
 SphereCollider — the authoring-layer `cc.SphereCollider` component (P8).

 Sphere shape in node-local space: `radius` before node scale (the
 backend multiplies in the node's world scale), `center` (from Collider)
 offsets the sphere from the node origin.
****************************************************************************/

#pragma once

#include "physics/framework/Collider.h"

namespace cc {

namespace physics {
class SphereShape;
}

class SphereCollider : public Collider {
    CC_CLASS_DECL(SphereCollider, Collider)
public:
    SphereCollider() = default;
    ~SphereCollider() override = default;

    float getRadius() const { return _radius; }
    void setRadius(float radius);

    static int forceLink();

protected:
    physics::IBaseShape *createShape() override;
    void applyShapeProps() override;

private:
    // Concrete alias of Collider::_shape (IBaseShape is a virtual base of
    // the facade classes, so downcasting is not possible — keep the typed
    // pointer alongside). Only dereferenced while `_shape` is non-null.
    physics::SphereShape *_sphereShape{nullptr};
    float _radius{0.5F};
};

} // namespace cc
