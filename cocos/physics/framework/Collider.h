/****************************************************************************
 Collider — shared base of the authoring-layer collider components (P8).

 Mirrors upstream collider.ts: owns the backend shape, the local center
 offset and the trigger flag. A collider on a node WITHOUT a RigidBody
 becomes static world geometry (the PhysX shared body defaults to a
 static actor); with a RigidBody on the same node the shape attaches to
 that body instead.

 Add-order handling: onLoad first ensures any sibling RigidBody has its
 backend body created (RigidBody::ensureBody), THEN creates the shape —
 so the shared body is typed correctly regardless of component order
 within one activation pass.

 Subclasses (BoxCollider / SphereCollider) implement createShape() to
 build the concrete physics::*Shape facade and applyShapeProps() to push
 their geometry fields after initialization.
****************************************************************************/

#pragma once

#include "core/component/Component.h"
#include "core/reflection/Reflection.h"
#include "math/Vec3.h"

namespace cc {

namespace physics {
class IBaseShape;
}

class RigidBody;

class Collider : public Component {
    CC_CLASS_DECL(Collider, Component)
public:
    Collider() = default;
    ~Collider() override;

    const Vec3 &getCenter() const { return _center; }
    void setCenter(const Vec3 &center);

    bool isTrigger() const { return _isTrigger; }
    void setIsTrigger(bool value);

    // The RigidBody on the same node, if any (null ⇒ static collider).
    RigidBody *getAttachedRigidBody() const;

    void onLoad() override;
    void onEnable() override;
    void onDisable() override;
    void onDestroy() override;

    static int forceLink();

protected:
    // Build the concrete backend shape facade. Null ⇒ backend unavailable.
    virtual physics::IBaseShape *createShape() { return nullptr; }
    // Push subclass geometry (size / radius) to the freshly created shape.
    virtual void applyShapeProps() {}

    void ensureShape();
    void releaseShape();

    physics::IBaseShape *_shape{nullptr}; // backend facade; null when disabled

    Vec3 _center{0.F, 0.F, 0.F};
    bool _isTrigger{false};
    bool _backendEnabled{false};
};

} // namespace cc
