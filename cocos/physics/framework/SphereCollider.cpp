#include "physics/framework/SphereCollider.h"

#include "base/memory/Memory.h"

#if CC_USE_PHYSICS_PHYSX
    #include "physics/sdk/Shape.h"
#endif

namespace cc {

CC_IMPLEMENT_CLASS(SphereCollider, "cc.SphereCollider", Collider)
    .property("_radius", &SphereCollider::_radius, 0.5F)
CC_END_CLASS(SphereCollider);

int SphereCollider::forceLink() {
    return getStaticClass() != nullptr ? 1 : 0;
}

physics::IBaseShape *SphereCollider::createShape() {
#if CC_USE_PHYSICS_PHYSX
    _sphereShape = ccnew physics::SphereShape();
    return _sphereShape;
#else
    return nullptr;
#endif
}

void SphereCollider::applyShapeProps() {
#if CC_USE_PHYSICS_PHYSX
    if (_shape != nullptr && _sphereShape != nullptr) {
        _sphereShape->setRadius(_radius);
    }
#endif
}

void SphereCollider::setRadius(float radius) {
    _radius = radius;
    applyShapeProps();
}

} // namespace cc
