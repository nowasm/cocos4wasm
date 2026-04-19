#include "core/scene-graph/ComponentEventHandler.h"

#include "core/component/Component.h"
#include "core/scene-graph/Node.h"

namespace cc {

// Registered as `cc.ClickEvent` for 1:1 parity with Cocos Creator JSON —
// despite the class name, this wrapper is reused for toggle / scroll / slide /
// page / editbox handler arrays throughout the UI layer.
CC_IMPLEMENT_ROOT_CLASS(ComponentEventHandler, "cc.ClickEvent")
    .property("target",          &ComponentEventHandler::target)
    .property("component",       &ComponentEventHandler::component)
    .property("_componentId",    &ComponentEventHandler::_componentId)
    .property("handler",         &ComponentEventHandler::handler)
    .property("customEventData", &ComponentEventHandler::customEventData)
CC_END_CLASS(ComponentEventHandler);

void ComponentEventHandler::emit(const reflection::MethodArgs &baseArgs) const {
    if (!target || handler.empty()) return;

    // Upstream resolves via the JS class registry, falling back to _componentId
    // when `component` is empty. For our C++ side the class-name string maps
    // directly to a registered ClassMeta, so either field can drive the lookup.
    const char *compName = !component.empty()        ? component.c_str()
                         : !_componentId.empty()     ? _componentId.c_str()
                         : nullptr;
    if (!compName) return;

    Component *comp = target->getComponentByClassName(compName);
    if (!comp) return;

    // Append customEventData as the final positional argument when present.
    // Handler signature may or may not take it — the reflection invoker's
    // arg-coercion fills or drops it as needed.
    if (customEventData.empty()) {
        comp->getClass()->invoke(comp, handler.c_str(), baseArgs);
    } else {
        reflection::MethodArgs extended = baseArgs;
        extended.push_back(reflection::MethodArg::makeString(customEventData));
        comp->getClass()->invoke(comp, handler.c_str(), extended);
    }
}

void ComponentEventHandler::emitEvents(
    const ccstd::vector<IntrusivePtr<ComponentEventHandler>> &events,
    const reflection::MethodArgs &args) {
    // Snapshot not required — upstream emits from a live array; handlers that
    // mutate the list while iterating are caller-problem (matches TS).
    for (const auto &ev : events) {
        if (ev) ev->emit(args);
    }
}

}  // namespace cc
