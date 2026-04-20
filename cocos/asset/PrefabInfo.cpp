#include "cocos/asset/PrefabInfo.h"

#include "cocos/asset/Prefab.h"
#include "core/component/Component.h"
#include "core/scene-graph/Node.h"

namespace cc {

// ─── TargetInfo ─────────────────────────────────────────────────────────
CC_IMPLEMENT_ROOT_CLASS(TargetInfo, "cc.TargetInfo")
    .property("localID", &TargetInfo::localID)
CC_END_CLASS(TargetInfo);

// ─── TargetOverrideInfo ─────────────────────────────────────────────────
CC_IMPLEMENT_ROOT_CLASS(TargetOverrideInfo, "cc.TargetOverrideInfo")
    .property("source",       &TargetOverrideInfo::source)
    .property("sourceInfo",   &TargetOverrideInfo::sourceInfo)
    .property("propertyPath", &TargetOverrideInfo::propertyPath)
    .property("target",       &TargetOverrideInfo::target)
    .property("targetInfo",   &TargetOverrideInfo::targetInfo)
CC_END_CLASS(TargetOverrideInfo);

// ─── CompPrefabInfo ─────────────────────────────────────────────────────
CC_IMPLEMENT_ROOT_CLASS(CompPrefabInfo, "cc.CompPrefabInfo")
    .property("fileId", &CompPrefabInfo::fileId)
CC_END_CLASS(CompPrefabInfo);

// ─── PropertyOverrideInfo ───────────────────────────────────────────────
// Note: upstream type string has NO "cc." prefix — preserve it.
// `valueJson` is NOT reflected; JsonDeserializer captures the `value` JSON
// fragment as a raw string in a dedicated slot-finalize pass because the
// value's concrete C++ type is only known at override apply time.
CC_IMPLEMENT_ROOT_CLASS(PropertyOverrideInfo, "CCPropertyOverrideInfo")
    .property("targetInfo",   &PropertyOverrideInfo::targetInfo)
    .property("propertyPath", &PropertyOverrideInfo::propertyPath)
CC_END_CLASS(PropertyOverrideInfo);

// ─── MountedChildrenInfo ────────────────────────────────────────────────
CC_IMPLEMENT_ROOT_CLASS(MountedChildrenInfo, "cc.MountedChildrenInfo")
    .property("targetInfo", &MountedChildrenInfo::targetInfo)
    .property("nodes",      &MountedChildrenInfo::nodes)
CC_END_CLASS(MountedChildrenInfo);

// ─── MountedComponentsInfo ──────────────────────────────────────────────
CC_IMPLEMENT_ROOT_CLASS(MountedComponentsInfo, "cc.MountedComponentsInfo")
    .property("targetInfo", &MountedComponentsInfo::targetInfo)
    .property("components", &MountedComponentsInfo::components)
CC_END_CLASS(MountedComponentsInfo);

// ─── PrefabInstance ─────────────────────────────────────────────────────
CC_IMPLEMENT_ROOT_CLASS(PrefabInstance, "cc.PrefabInstance")
    .property("fileId",            &PrefabInstance::fileId)
    .property("prefabRootNode",    &PrefabInstance::prefabRootNode)
    .property("mountedChildren",   &PrefabInstance::mountedChildren)
    .property("mountedComponents", &PrefabInstance::mountedComponents)
    .property("propertyOverrides", &PrefabInstance::propertyOverrides)
    .property("removedComponents", &PrefabInstance::removedComponents)
CC_END_CLASS(PrefabInstance);

// ─── PrefabInfo ─────────────────────────────────────────────────────────
CC_IMPLEMENT_ROOT_CLASS(PrefabInfo, "cc.PrefabInfo")
    .property("root",            &PrefabInfo::root)
    .property("asset",           &PrefabInfo::asset)
    .property("fileId",          &PrefabInfo::fileId)
    .property("instance",        &PrefabInfo::instance)
    .property("targetOverrides", &PrefabInfo::targetOverrides)
CC_END_CLASS(PrefabInfo);

}  // namespace cc
