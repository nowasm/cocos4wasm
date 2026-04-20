#pragma once

#include "base/Ptr.h"
#include "core/data/Object.h"
#include "core/reflection/Reflection.h"

namespace cc {

namespace serialization { class JsonDeserializer; }

class CompPrefabInfo;
class Node;

// Component is the base class for all authoring-layer behaviors attached to a
// Node. Lifecycle follows Cocos Creator's ordering:
//
//   onLoad      - called once the first time the node becomes active
//   onEnable    - called when enabled in hierarchy (can fire multiple times)
//   start       - called on the first frame tick after onEnable (deferred)
//   update      - each frame while enabled, if _wantsUpdate
//   lateUpdate  - each frame after update, if _wantsLateUpdate
//   onDisable   - when hierarchy enabled-state goes false
//   onDestroy   - on destruction, after final onDisable
//
// Lifecycle state bits are stored on CCObject::_objFlags (IS_ON_LOAD_CALLED,
// IS_ON_ENABLE_CALLED, IS_START_CALLED) — reusing the existing scaffold, not
// duplicating bookkeeping.
//
// Reflection: Component is a *reflection-root*. C++ inherits CCObject, but
// JSON/Editor serialization does not see CCObject — "cc.Component" is the top
// of the reflected type tree.
class Component : public CCObject {
    CC_CLASS_DECL(Component, void)
public:
    // Ctor / dtor are out-of-line so the IntrusivePtr<CompPrefabInfo>
    // member only needs a forward declaration here.
    Component();
    ~Component() override;

    inline Node *getNode() const { return _node; }

    bool isEnabled() const { return _enabled; }
    void setEnabled(bool v);
    bool isEnabledInHierarchy() const;

    // Prefab-authored components carry a fileId here, used by
    // prefab_utils to locate them during override apply. Returns null
    // for components added at runtime. setPrefabInfo is mainly for test
    // fixtures and manual-construction paths; the deserializer writes
    // the field directly through reflection. Both out-of-line so the
    // header can keep `CompPrefabInfo` as a forward declaration.
    CompPrefabInfo *getPrefabInfo() const;
    void            setPrefabInfo(CompPrefabInfo *v);

    virtual void onLoad() {}
    virtual void onEnable() {}
    virtual void start() {}
    virtual void update(float /*dt*/) {}
    virtual void lateUpdate(float /*dt*/) {}
    virtual void onDisable() {}
    virtual void onDestroy() {}

protected:
    friend class Node;
    friend class NodeActivator;
    friend class ComponentScheduler;
    friend class serialization::JsonDeserializer;

    // Subclasses that override update()/lateUpdate() should set the
    // corresponding flag in their constructor so the scheduler knows to call
    // them. Leaving false avoids wasted virtual dispatch.
    bool _wantsUpdate{false};
    bool _wantsLateUpdate{false};

    Node *_node{nullptr};
    bool  _enabled{true};

    // Set on components that were authored inside a prefab. `__prefab.fileId`
    // uniquely identifies the component within the master so prefab
    // instance overrides can target it by name. Null on script-added
    // components at runtime.
    IntrusivePtr<CompPrefabInfo> __prefab;
};

}  // namespace cc
