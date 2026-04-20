/****************************************************************************
 Port of cocos-engine/cocos/scene-graph/prefab/prefab-info.ts

 Collection of plain data classes that describe the differences between a
 prefab instance and its master. Every one of them is reflected because
 Editor-exported .scene / .prefab JSON references them by `__type__`
 string; the five override categories (property overrides, mounted
 children / components, removed components, target overrides) live under
 PrefabInstance / PrefabInfo and are applied at load time by
 cocos/asset/prefab_utils.cpp.

 Upstream `__type__` strings are preserved verbatim — note the quirks:
   - `cc.PrefabInfo` / `cc.PrefabInstance`  — normal cc.-namespaced
   - `cc.TargetInfo` / `cc.TargetOverrideInfo`
   - `cc.CompPrefabInfo`
   - `cc.MountedChildrenInfo` / `cc.MountedComponentsInfo` (plural)
   - `CCPropertyOverrideInfo`            — no "cc." prefix in upstream
****************************************************************************/

#pragma once

#include "base/Ptr.h"
#include "base/std/container/string.h"
#include "base/std/container/vector.h"
#include "core/data/Object.h"
#include "core/reflection/Reflection.h"

namespace cc {

class Node;
class Component;
class Prefab;

// ─── TargetInfo ───────────────────────────────────────────────────────────
// Path of fileIds used to locate a node or component inside a cloned
// prefab instance. Walked by prefab_utils::getTarget against the targetMap
// built at instantiate time.
class TargetInfo : public CCObject {
    CC_CLASS_DECL(TargetInfo, void)
public:
    TargetInfo() = default;
    ~TargetInfo() override = default;

    ccstd::vector<ccstd::string> localID;
};

// ─── TargetOverrideInfo ───────────────────────────────────────────────────
// Cross-prefab pointer patch: "source.<propertyPath> should point at
// target". Used when a prefab references a node outside itself (e.g. a
// Button's clickEvents[].target pointing at a scene-level controller).
class TargetOverrideInfo : public CCObject {
    CC_CLASS_DECL(TargetOverrideInfo, void)
public:
    TargetOverrideInfo() = default;
    ~TargetOverrideInfo() override = default;

    // Source side: either a direct pointer (when resolvable at serialize
    // time) or a TargetInfo (when the source itself lives inside a prefab
    // instance). `source` as a raw CCObject* covers both Node and Component.
    CCObject                 *source{nullptr};
    IntrusivePtr<TargetInfo>  sourceInfo;

    ccstd::vector<ccstd::string> propertyPath;

    Node                     *target{nullptr};
    IntrusivePtr<TargetInfo>  targetInfo;
};

// ─── CompPrefabInfo ───────────────────────────────────────────────────────
// Attached to each Component inside a prefab master. `fileId` uniquely
// identifies the component within the prefab so override operations can
// target it by string ID rather than pointer.
class CompPrefabInfo : public CCObject {
    CC_CLASS_DECL(CompPrefabInfo, void)
public:
    CompPrefabInfo() = default;
    ~CompPrefabInfo() override = default;

    ccstd::string fileId;
};

// ─── PropertyOverrideInfo ─────────────────────────────────────────────────
// "On this node/component, set this field path to this value." The value
// field is serialized as an arbitrary JSON fragment; we stash it as a raw
// JSON string and decode against the target PropertyMeta.typeId at apply
// time (see prefab_utils::applyPropertyOverrides).
class PropertyOverrideInfo : public CCObject {
    CC_CLASS_DECL(PropertyOverrideInfo, void)
public:
    PropertyOverrideInfo() = default;
    ~PropertyOverrideInfo() override = default;

    IntrusivePtr<TargetInfo>     targetInfo;
    ccstd::vector<ccstd::string> propertyPath;

    // Raw JSON text of the override value — populated by
    // JsonDeserializer's cc.PropertyOverrideInfo special case because
    // the value's C++ type is only known at apply time.
    ccstd::string valueJson;
};

// ─── MountedChildrenInfo ──────────────────────────────────────────────────
// Child nodes present on this instance but not in the master prefab.
// Applied by reparenting each node to the target node.
class MountedChildrenInfo : public CCObject {
    CC_CLASS_DECL(MountedChildrenInfo, void)
public:
    MountedChildrenInfo() = default;
    ~MountedChildrenInfo() override = default;

    IntrusivePtr<TargetInfo>           targetInfo;
    ccstd::vector<IntrusivePtr<Node>>  nodes;
};

// ─── MountedComponentsInfo ────────────────────────────────────────────────
// Components present on this instance but not in the master prefab.
class MountedComponentsInfo : public CCObject {
    CC_CLASS_DECL(MountedComponentsInfo, void)
public:
    MountedComponentsInfo() = default;
    ~MountedComponentsInfo() override = default;

    IntrusivePtr<TargetInfo>                targetInfo;
    ccstd::vector<IntrusivePtr<Component>>  components;
};

// ─── PrefabInstance ───────────────────────────────────────────────────────
// The override bundle glued to a prefab instance in the scene. Four
// vectors — each produced by the Editor when the user modifies an
// instance — are applied in the order documented in
// prefab_utils::expandPrefabInstanceNode.
class PrefabInstance : public CCObject {
    CC_CLASS_DECL(PrefabInstance, void)
public:
    PrefabInstance() = default;
    ~PrefabInstance() override = default;

    ccstd::string fileId;

    // Back-ref to the instance root node. Optional — may be null when
    // the instance is nested inside another prefab's mount list.
    Node *prefabRootNode{nullptr};

    ccstd::vector<IntrusivePtr<MountedChildrenInfo>>   mountedChildren;
    ccstd::vector<IntrusivePtr<MountedComponentsInfo>> mountedComponents;
    ccstd::vector<IntrusivePtr<PropertyOverrideInfo>>  propertyOverrides;
    ccstd::vector<IntrusivePtr<TargetInfo>>            removedComponents;

    // Set to true by prefab_utils::expandPrefabInstanceNode once the
    // overrides have been applied — guards against double-application
    // when a scene re-expands a nested instance.
    bool expanded{false};
};

// ─── PrefabInfo ───────────────────────────────────────────────────────────
// Attached to a Node that is a prefab instance (Node._prefab). Points at
// the master Prefab asset + the instance bundle + the cross-prefab
// targetOverrides list (which lives on PrefabInfo, not PrefabInstance,
// per upstream).
class PrefabInfo : public CCObject {
    CC_CLASS_DECL(PrefabInfo, void)
public:
    PrefabInfo() = default;
    ~PrefabInfo() override = default;

    // Instance root node (same node that owns this PrefabInfo via _prefab).
    Node *root{nullptr};

    // Master prefab asset. Resolved through AssetManager by __uuid__.
    IntrusivePtr<Prefab> asset;

    // Node fileId — unique within the prefab asset.
    ccstd::string fileId;

    // Override bundle — null when this node is part of a prefab master
    // tree rather than a live instance.
    IntrusivePtr<PrefabInstance> instance;

    ccstd::vector<IntrusivePtr<TargetOverrideInfo>> targetOverrides;
};

}  // namespace cc
