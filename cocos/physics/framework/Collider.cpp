#include "physics/framework/Collider.h"

#include "core/scene-graph/Node.h"
#include "physics/framework/PhysicsSystem.h"
#include "physics/framework/RigidBody.h"

#if CC_USE_PHYSICS_PHYSX
    #include "physics/spec/IShape.h"
#endif

namespace cc {

CC_IMPLEMENT_CLASS(Collider, "cc.Collider", Component)
    .property("_center",    &Collider::_center,    Vec3(0.F, 0.F, 0.F))
    .property("_isTrigger", &Collider::_isTrigger, false)
CC_END_CLASS(Collider);

int Collider::forceLink() {
    return getStaticClass() != nullptr ? 1 : 0;
}

Collider::~Collider() {
    releaseShape();
}

RigidBody *Collider::getAttachedRigidBody() const {
    return _node != nullptr ? _node->getComponent<RigidBody>() : nullptr;
}

void Collider::ensureShape() {
#if CC_USE_PHYSICS_PHYSX
    if (_shape != nullptr || _node == nullptr) return;
    if (!PhysicsSystem::getInstance().init()) return;

    // Make sure a sibling RigidBody has its backend body first, so the
    // node's shared body carries the right type before shapes attach.
    RigidBody *body = getAttachedRigidBody();
    if (body != nullptr) body->ensureBody();

    _shape = createShape();
    if (_shape == nullptr) return;
    _shape->initialize(_node);
    applyShapeProps();
    _shape->setCenter(_center.x, _center.y, _center.z);
    if (_isTrigger) _shape->setAsTrigger(true);
#else
    PhysicsSystem::getInstance().init(); // logs the disabled notice once
#endif
}

void Collider::releaseShape() {
#if CC_USE_PHYSICS_PHYSX
    if (_shape == nullptr) return;
    if (_backendEnabled) {
        _shape->onDisable();
        _backendEnabled = false;
    }
    _shape->onDestroy();
    delete _shape;
    _shape = nullptr;
#endif
}

void Collider::onLoad() {
    ensureShape();
}

void Collider::onEnable() {
    ensureShape();
#if CC_USE_PHYSICS_PHYSX
    if (_shape != nullptr && !_backendEnabled) {
        _shape->onEnable();
        _backendEnabled = true;
    }
#endif
}

void Collider::onDisable() {
#if CC_USE_PHYSICS_PHYSX
    if (_shape != nullptr && _backendEnabled) {
        _shape->onDisable();
        _backendEnabled = false;
    }
#endif
}

void Collider::onDestroy() {
    releaseShape();
}

void Collider::setCenter(const Vec3 &center) {
    _center = center;
#if CC_USE_PHYSICS_PHYSX
    if (_shape != nullptr) _shape->setCenter(center.x, center.y, center.z);
#endif
}

void Collider::setIsTrigger(bool value) {
    _isTrigger = value;
#if CC_USE_PHYSICS_PHYSX
    if (_shape != nullptr) _shape->setAsTrigger(value);
#endif
}

} // namespace cc
