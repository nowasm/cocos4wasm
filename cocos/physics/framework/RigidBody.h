/****************************************************************************
 RigidBody — the authoring-layer `cc.RigidBody` component (P8).

 Mirrors upstream rigid-body.ts: a node with a RigidBody participates in
 the dynamics simulation with the given body type / mass / damping, and
 the colliders on the same node become the body's shapes (colliders on a
 node WITHOUT a RigidBody act as static geometry instead).

 The backend object (physics::RigidBody facade over PhysXRigidBody) is
 created at onLoad and destroyed at onDestroy; enable state maps 1:1 to
 the backend's onEnable/onDisable. Colliders ask ensureBody() before
 creating their shape so either add order (collider-before-rigidbody or
 the reverse within one activation) resolves to the same shared body.

 With USE_PHYSICS_PHYSX off the component is inert — every backend call
 is compiled out and the runtime API degrades to plain field storage.
****************************************************************************/

#pragma once

#include <cstdint>
#include "core/component/Component.h"
#include "core/reflection/Reflection.h"
#include "math/Vec3.h"

namespace cc {

namespace physics {
class RigidBody;
}

class RigidBody : public Component {
    CC_CLASS_DECL(RigidBody, Component)
public:
    // Mirrors physics::ERigidBodyType (and upstream ERigidBodyType) values.
    enum class BodyType : int32_t {
        DYNAMIC = 1,
        STATIC = 2,
        KINEMATIC = 4,
    };

    RigidBody() = default;
    ~RigidBody() override;

    BodyType getBodyType() const { return _type; }
    void setBodyType(BodyType type);

    float getMass() const { return _mass; }
    void setMass(float mass);

    bool isUsingGravity() const { return _useGravity; }
    void setUseGravity(bool value);

    float getLinearDamping() const { return _linearDamping; }
    void setLinearDamping(float value);

    float getAngularDamping() const { return _angularDamping; }
    void setAngularDamping(float value);

    // Forces are expressed in world space; `relativePoint` is the offset
    // from the body's origin (world axes) the force is applied at.
    void applyForce(const Vec3 &force, const Vec3 &relativePoint = Vec3::ZERO);
    void applyImpulse(const Vec3 &impulse, const Vec3 &relativePoint = Vec3::ZERO);

    Vec3 getLinearVelocity() const;
    void setLinearVelocity(const Vec3 &velocity);

    bool isAwake() const;
    void wakeUp();
    void sleep();

    // Creates + initializes the backend body (idempotent). Called from the
    // component lifecycle and from Collider::onLoad so a collider that
    // activates first still attaches to a correctly-typed body.
    void ensureBody();

    void onLoad() override;
    void onEnable() override;
    void onDisable() override;
    void onDestroy() override;

    static int forceLink();

private:
    void releaseBody();

    physics::RigidBody *_body{nullptr}; // backend facade; null when disabled

    BodyType _type{BodyType::DYNAMIC};
    float _mass{1.F};
    bool _useGravity{true};
    float _linearDamping{0.1F};
    float _angularDamping{0.1F};
    bool _backendEnabled{false};
};

} // namespace cc
