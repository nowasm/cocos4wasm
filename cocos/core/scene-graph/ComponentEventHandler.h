/****************************************************************************
 Cocos4wasm — pure-C++ port of Cocos Creator's EventHandler.

 Mirrors `cocos-engine/cocos/scene-graph/component-event-handler.ts`:
 a serializable descriptor that binds a target node + component class name +
 handler method name, invoked via reflection at dispatch time.

 Registered as `cc.ClickEvent` so Editor-exported .prefab / .scene JSON that
 references click / toggle / scroll / slide / page event handlers loads with
 no manual bridging — the serialized arrays on Button, Toggle, ScrollView etc.
 are `vector<IntrusivePtr<ComponentEventHandler>>` and their contents call
 user-facing component methods through the reflection `invoke` we added in
 MethodMeta.
****************************************************************************/

#pragma once

#include "base/Ptr.h"
#include "base/std/container/string.h"
#include "base/std/container/vector.h"
#include "core/data/Object.h"
#include "core/reflection/Reflection.h"

namespace cc {

class Node;

// Inherits CCObject for refcounting (so IntrusivePtr<ComponentEventHandler>
// works in the vector<> reflection path), but is a *reflection root* — upstream
// `cc.ClickEvent` has no reflected parent either.
class ComponentEventHandler : public CCObject {
    CC_CLASS_DECL(ComponentEventHandler, void)
public:
    ComponentEventHandler() = default;
    ~ComponentEventHandler() override = default;

    // Editor-serialized fields. Names match upstream `cc.ClickEvent` exactly;
    // do not rename — the deserializer looks them up by string.
    Node          *target{nullptr};        // the node hosting the target component
    ccstd::string  component;              // class name, e.g. "cc.Button" or a user script name
    ccstd::string  _componentId;           // legacy numeric/UUID-style id, used when `component` is empty
    ccstd::string  handler;                // method name reflected on the component
    ccstd::string  customEventData;        // appended as the last argument on dispatch

    // Resolve target → component → method via reflection and invoke it.
    // Args are forwarded positionally; `customEventData` is appended as a
    // trailing string arg when non-empty, matching upstream JS behaviour.
    // Safe to call when the handler is partially configured — missing pieces
    // simply skip the dispatch.
    void emit(const reflection::MethodArgs &args) const;

    // Static fan-out: emits every non-null handler in the vector. Components
    // call this from their dispatch paths (Button::onClicked, Toggle::onCheck,
    // etc.) instead of writing the loop by hand.
    static void emitEvents(const ccstd::vector<IntrusivePtr<ComponentEventHandler>> &events,
                           const reflection::MethodArgs &args);
};

}  // namespace cc
