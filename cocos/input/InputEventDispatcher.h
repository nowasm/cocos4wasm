#pragma once

#include "base/std/container/vector.h"

namespace cc {

class Canvas;
class Node;

// Central listener that wires the Engine-bus mouse/touch events into the
// scene graph via per-Canvas hit-testing and Node typed event dispatch.
//
// Lifecycle:
//   - Dispatcher is a Meyers singleton; lazy-init on first access.
//   - Canvas::onEnable/onDisable push/pop themselves via register/unregister
//     so the hit-test set matches the active scene.
//   - start() binds engine-bus listeners; stop() unbinds. Called from
//     DemoGame (or any game app) at engine init / shutdown.
//
// Hit-test flow per mouse event:
//   1. Map window-space (mouseX, mouseY) into Cocos UI world space. MVP
//      assumes a canonical centered ortho camera matching the main window.
//   2. Walk registered canvases in descending priority order.
//   3. DFS each canvas's node subtree, visiting children in reverse sibling
//      order so topmost-drawn nodes are tested first.
//   4. For each node with a UITransform, call hitTestWorld(wx, wy).
//   5. On first hit, dispatch the matching Node typed event (MouseDown /
//      MouseMove / MouseUp / MouseWheel); the Node's parent chain receives
//      the bubble phase via the existing EventTarget infrastructure.
//
// Design intent: keep the dispatcher purely an adapter between two existing
// systems (engine bus ↔ Node events). No new state machine, no gesture
// recognition — P4 scope is "click works". Drag / focus / enter/leave /
// gestures land in later phases.
class InputEventDispatcher {
public:
    static InputEventDispatcher &get();

    void start();
    void stop();

    // Canvas registers itself so the dispatcher knows which subtrees are
    // eligible for hit-test. Order follows registration; dispatch sorts
    // by Canvas priority each event.
    void registerCanvas(Canvas *c);
    void unregisterCanvas(Canvas *c);

private:
    InputEventDispatcher() = default;

    // Bus subscription handles. Empty until start() is called.
    struct Impl;
    Impl *_impl{nullptr};

    ccstd::vector<Canvas *> _canvases;

    bool _running{false};
};

}  // namespace cc
