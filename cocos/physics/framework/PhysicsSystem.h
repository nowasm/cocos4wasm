/****************************************************************************
 PhysicsSystem — the pure-C++ physics framework driver (P8).

 Owns the backend physics world (PhysX via the physics/sdk facades) and
 steps it on a fixed-timestep accumulator. The world is created lazily —
 either on the first RigidBody / Collider enable or on an explicit init()
 call — so apps that never touch physics pay nothing.

 Per engine tick, update(dt) accumulates dt and runs up to `maxSubSteps`
 fixed substeps. Each substep follows the upstream flow:

   syncSceneToPhysics()  — push node transforms authored by game code
   step(fixedTimeStep)   — simulate; the PhysX backend pulls resulting
                           body transforms back into the nodes itself
                           (syncPhysicsToScene runs inside step)
   emitEvents()          — refresh trigger/contact pair lists

 Built even when USE_PHYSICS_PHYSX is OFF: every backend call is guarded
 by CC_USE_PHYSICS_PHYSX and the system degrades to an inert no-op that
 logs a one-time "backend disabled" notice.
****************************************************************************/

#pragma once

#include <cstdint>
#include "math/Vec3.h"

namespace cc {

namespace physics {
class World;
}

class PhysicsSystem final {
public:
    static PhysicsSystem &getInstance();

    // Creates the backend world (idempotent). Returns true when a physics
    // backend is available and the world exists.
    bool init();
    bool isInitialized() const { return _initialized; }

    // Fixed-timestep accumulator driver — call once per engine tick.
    void update(float dt);

    void setGravity(const Vec3 &gravity);
    const Vec3 &getGravity() const { return _gravity; }

    void setFixedTimeStep(float step);
    float getFixedTimeStep() const { return _fixedTimeStep; }

    void setMaxSubSteps(uint32_t steps) { _maxSubSteps = steps; }
    uint32_t getMaxSubSteps() const { return _maxSubSteps; }

    // Backend world facade; null until init() succeeds (always null when
    // the backend is compiled out).
    physics::World *getWorld() const { return _world; }

    // Tears the world down. Only safe once every physics component has
    // been destroyed (their backend objects reference the world).
    void destroy();

private:
    PhysicsSystem() = default;
    ~PhysicsSystem();
    PhysicsSystem(const PhysicsSystem &) = delete;
    PhysicsSystem &operator=(const PhysicsSystem &) = delete;

    physics::World *_world{nullptr};
    Vec3 _gravity{0.F, -9.81F, 0.F};
    float _fixedTimeStep{1.F / 60.F};
    uint32_t _maxSubSteps{4};
    float _accumulator{0.F};
    bool _initialized{false};
};

} // namespace cc
