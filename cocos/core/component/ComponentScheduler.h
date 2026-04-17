#pragma once

#include "base/std/container/vector.h"

namespace cc {

class Component;

// Per-frame driver of Component update() and lateUpdate(). Components opt in
// by setting _wantsUpdate / _wantsLateUpdate in their constructor; the
// scheduler tracks them in two flat vectors.
//
// Mutation during iteration is supported by a simple guard that defers adds
// and removes until after the current pass finishes.
class ComponentScheduler {
public:
    static ComponentScheduler &get();

    void scheduleUpdate(Component *c);
    void unscheduleUpdate(Component *c);
    void scheduleLateUpdate(Component *c);
    void unscheduleLateUpdate(Component *c);

    void update(float dt);
    void lateUpdate(float dt);

private:
    ComponentScheduler() = default;

    static void addUnique(ccstd::vector<Component *> &dst, Component *c);
    static void removeFrom(ccstd::vector<Component *> &dst, Component *c);

    void applyPending();

    ccstd::vector<Component *> _updatable;
    ccstd::vector<Component *> _lateUpdatable;

    ccstd::vector<Component *> _pendingAdd;
    ccstd::vector<Component *> _pendingRemove;
    ccstd::vector<Component *> _pendingAddLate;
    ccstd::vector<Component *> _pendingRemoveLate;

    int _iterDepth{0};
};

}  // namespace cc
