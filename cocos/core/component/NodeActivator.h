#pragma once

#include "base/std/container/vector.h"

namespace cc {

class Node;
class Component;

// Drives component-lifecycle callbacks in response to scene-graph activation
// changes. Single shared singleton; invoked by Node on setActive / reparent,
// and by Engine each tick to drain the deferred start() queue.
//
// Ordering (match TS engine):
//   activation → onLoad (once) → onEnable → schedule update/lateUpdate
//   queued start → invoked on next frame before first update
//   deactivation → unschedule → onDisable
//   destroy → (deactivate if needed) → onDestroy
class NodeActivator {
public:
    static NodeActivator &get();

    // Toggle a node's effective active state. Recurses into children whose
    // local _active is true — they inherit the new parent state.
    void activateNode(Node *node, bool shouldActive);

    // Added to an already-active node: run onLoad+onEnable+schedule.
    void activateComp(Component *comp);

    // Becoming disabled (node deactivated or setEnabled(false)).
    void deactivateComp(Component *comp);

    // Destroyed: runs onDestroy after any final deactivate.
    void destroyComp(Component *comp);

    // Drained by Engine each tick before ComponentScheduler::update — invokes
    // queued start() callbacks and clears.
    void invokePendingStarts();

private:
    NodeActivator() = default;

    void activateNodeRecursive(Node *node);
    void deactivateNodeRecursive(Node *node);

    ccstd::vector<Component *> _startQueue;
};

}  // namespace cc
