#include "physics/framework/PhysicsSystem.h"

#include "base/Log.h"
#include "base/memory/Memory.h"

#if CC_USE_PHYSICS_PHYSX
    #include "physics/sdk/World.h"
#endif

namespace cc {

namespace {
#if !CC_USE_PHYSICS_PHYSX
void logBackendDisabledOnce() {
    static bool logged = false;
    if (!logged) {
        logged = true;
        CC_LOG_INFO("[Physics] physics backend disabled (USE_PHYSICS_PHYSX=OFF) — physics components are inert");
    }
}
#endif
} // namespace

PhysicsSystem &PhysicsSystem::getInstance() {
    static PhysicsSystem instance;
    return instance;
}

PhysicsSystem::~PhysicsSystem() {
    destroy();
}

bool PhysicsSystem::init() {
#if CC_USE_PHYSICS_PHYSX
    if (_initialized) return true;
    _world = ccnew physics::World();
    _world->setGravity(_gravity.x, _gravity.y, _gravity.z);
    _world->setFixedTimeStep(_fixedTimeStep);
    _initialized = true;
    CC_LOG_INFO("[Physics] world created — gravity (%.2f, %.2f, %.2f), fixed step %.4fs, max %u substeps",
                _gravity.x, _gravity.y, _gravity.z, _fixedTimeStep, _maxSubSteps);
    return true;
#else
    logBackendDisabledOnce();
    return false;
#endif
}

void PhysicsSystem::update(float dt) {
#if CC_USE_PHYSICS_PHYSX
    if (!_initialized || _world == nullptr) return;

    _accumulator += dt;
    uint32_t substeps = 0;
    while (_accumulator >= _fixedTimeStep && substeps < _maxSubSteps) {
        // Push game-code transform writes into physics, simulate, and let
        // the backend write resulting dynamic-body poses back to the nodes
        // (PhysXWorld::step runs syncPhysicsToScene internally).
        _world->syncSceneToPhysics();
        _world->step(_fixedTimeStep);
        _world->emitEvents();
        _accumulator -= _fixedTimeStep;
        ++substeps;
    }
    // Cap the banked debt at one substep so a long hitch cannot trigger a
    // catch-up spiral on the following frames.
    if (_accumulator > _fixedTimeStep) _accumulator = _fixedTimeStep;
#else
    (void)dt;
#endif
}

void PhysicsSystem::setGravity(const Vec3 &gravity) {
    _gravity = gravity;
#if CC_USE_PHYSICS_PHYSX
    if (_world != nullptr) {
        _world->setGravity(gravity.x, gravity.y, gravity.z);
    }
#endif
}

void PhysicsSystem::setFixedTimeStep(float step) {
    if (step <= 0.F) return;
    _fixedTimeStep = step;
#if CC_USE_PHYSICS_PHYSX
    if (_world != nullptr) {
        _world->setFixedTimeStep(step);
    }
#endif
}

void PhysicsSystem::destroy() {
#if CC_USE_PHYSICS_PHYSX
    if (_world != nullptr) {
        delete _world;
        _world = nullptr;
    }
#endif
    _initialized = false;
    _accumulator = 0.F;
}

} // namespace cc
