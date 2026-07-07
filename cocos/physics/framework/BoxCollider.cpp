#include "physics/framework/BoxCollider.h"

#include "base/memory/Memory.h"

#if CC_USE_PHYSICS_PHYSX
    #include "physics/sdk/Shape.h"
#endif

namespace cc {

CC_IMPLEMENT_CLASS(BoxCollider, "cc.BoxCollider", Collider)
    .property("_size", &BoxCollider::_size, Vec3(1.F, 1.F, 1.F))
CC_END_CLASS(BoxCollider);

int BoxCollider::forceLink() {
    return getStaticClass() != nullptr ? 1 : 0;
}

physics::IBaseShape *BoxCollider::createShape() {
#if CC_USE_PHYSICS_PHYSX
    _boxShape = ccnew physics::BoxShape();
    return _boxShape;
#else
    return nullptr;
#endif
}

void BoxCollider::applyShapeProps() {
#if CC_USE_PHYSICS_PHYSX
    if (_shape != nullptr && _boxShape != nullptr) {
        _boxShape->setSize(_size.x, _size.y, _size.z);
    }
#endif
}

void BoxCollider::setSize(const Vec3 &size) {
    _size = size;
    applyShapeProps();
}

} // namespace cc
