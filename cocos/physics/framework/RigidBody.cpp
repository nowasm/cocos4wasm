#include "physics/framework/RigidBody.h"

#include "base/Log.h"
#include "base/memory/Memory.h"
#include "core/scene-graph/Node.h"
#include "physics/framework/PhysicsSystem.h"

#if CC_USE_PHYSICS_PHYSX
    #include "physics/sdk/RigidBody.h"
    #include "physics/spec/IBody.h"
#endif

namespace cc {

CC_IMPLEMENT_CLASS(RigidBody, "cc.RigidBody", Component)
    .property("_type",           &RigidBody::_type,           RigidBody::BodyType::DYNAMIC)
    .property("_mass",           &RigidBody::_mass,           1.F)
    .property("_useGravity",     &RigidBody::_useGravity,     true)
    .property("_linearDamping",  &RigidBody::_linearDamping,  0.1F)
    .property("_angularDamping", &RigidBody::_angularDamping, 0.1F)
CC_END_CLASS(RigidBody);

int RigidBody::forceLink() {
    return getStaticClass() != nullptr ? 1 : 0;
}

RigidBody::~RigidBody() {
    releaseBody();
}

void RigidBody::ensureBody() {
#if CC_USE_PHYSICS_PHYSX
    if (_body != nullptr || _node == nullptr) return;
    if (!PhysicsSystem::getInstance().init()) return;
    _body = ccnew physics::RigidBody();
    _body->initialize(_node, static_cast<physics::ERigidBodyType>(_type), 1 /*default group*/);
    _body->setMass(_mass);
    _body->useGravity(_useGravity);
    _body->setLinearDamping(_linearDamping);
    _body->setAngularDamping(_angularDamping);
#else
    PhysicsSystem::getInstance().init(); // logs the disabled notice once
#endif
}

void RigidBody::releaseBody() {
#if CC_USE_PHYSICS_PHYSX
    if (_body == nullptr) return;
    if (_backendEnabled) {
        _body->onDisable();
        _backendEnabled = false;
    }
    _body->onDestroy();
    delete _body;
    _body = nullptr;
#endif
}

void RigidBody::onLoad() {
    ensureBody();
}

void RigidBody::onEnable() {
    ensureBody();
#if CC_USE_PHYSICS_PHYSX
    if (_body != nullptr && !_backendEnabled) {
        _body->onEnable();
        _backendEnabled = true;
    }
#endif
}

void RigidBody::onDisable() {
#if CC_USE_PHYSICS_PHYSX
    if (_body != nullptr && _backendEnabled) {
        _body->onDisable();
        _backendEnabled = false;
    }
#endif
}

void RigidBody::onDestroy() {
    releaseBody();
}

void RigidBody::setBodyType(BodyType type) {
    _type = type;
#if CC_USE_PHYSICS_PHYSX
    if (_body != nullptr) _body->setType(static_cast<physics::ERigidBodyType>(type));
#endif
}

void RigidBody::setMass(float mass) {
    _mass = mass;
#if CC_USE_PHYSICS_PHYSX
    if (_body != nullptr) _body->setMass(mass);
#endif
}

void RigidBody::setUseGravity(bool value) {
    _useGravity = value;
#if CC_USE_PHYSICS_PHYSX
    if (_body != nullptr) _body->useGravity(value);
#endif
}

void RigidBody::setLinearDamping(float value) {
    _linearDamping = value;
#if CC_USE_PHYSICS_PHYSX
    if (_body != nullptr) _body->setLinearDamping(value);
#endif
}

void RigidBody::setAngularDamping(float value) {
    _angularDamping = value;
#if CC_USE_PHYSICS_PHYSX
    if (_body != nullptr) _body->setAngularDamping(value);
#endif
}

void RigidBody::applyForce(const Vec3 &force, const Vec3 &relativePoint) {
#if CC_USE_PHYSICS_PHYSX
    if (_body != nullptr) {
        _body->applyForce(force.x, force.y, force.z,
                          relativePoint.x, relativePoint.y, relativePoint.z);
    }
#else
    (void)force;
    (void)relativePoint;
#endif
}

void RigidBody::applyImpulse(const Vec3 &impulse, const Vec3 &relativePoint) {
#if CC_USE_PHYSICS_PHYSX
    if (_body != nullptr) {
        _body->applyImpulse(impulse.x, impulse.y, impulse.z,
                            relativePoint.x, relativePoint.y, relativePoint.z);
    }
#else
    (void)impulse;
    (void)relativePoint;
#endif
}

Vec3 RigidBody::getLinearVelocity() const {
#if CC_USE_PHYSICS_PHYSX
    if (_body != nullptr) return _body->getLinearVelocity();
#endif
    return Vec3::ZERO;
}

void RigidBody::setLinearVelocity(const Vec3 &velocity) {
#if CC_USE_PHYSICS_PHYSX
    if (_body != nullptr) _body->setLinearVelocity(velocity.x, velocity.y, velocity.z);
#else
    (void)velocity;
#endif
}

bool RigidBody::isAwake() const {
#if CC_USE_PHYSICS_PHYSX
    if (_body != nullptr) return _body->isAwake();
#endif
    return false;
}

void RigidBody::wakeUp() {
#if CC_USE_PHYSICS_PHYSX
    if (_body != nullptr) _body->wakeUp();
#endif
}

void RigidBody::sleep() {
#if CC_USE_PHYSICS_PHYSX
    if (_body != nullptr) _body->sleep();
#endif
}

} // namespace cc
