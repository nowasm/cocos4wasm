/****************************************************************************
 Port of cocos-engine/cocos/scene-graph/prefab/utils.ts — the algorithms
 that turn a PrefabInstance's difference-encoded override bundle into
 concrete mutations on a cloned prefab tree.

 Apply order (mirrors upstream expandPrefabInstanceNode):
   1. buildTargetMap(clonedRoot, map)
   2. applyMountedChildren(mountedChildren, map)
   3. applyRemovedComponents(removedComponents, map)
   4. applyMountedComponents(mountedComponents, map)
   5. applyPropertyOverrides(propertyOverrides, map)
   6. applyTargetOverrides(targetOverrides, map)     (from PrefabInfo, not
                                                       PrefabInstance)
****************************************************************************/

#pragma once

#include "base/std/container/string.h"
#include "base/std/container/unordered_map.h"
#include "base/std/container/vector.h"
#include "cocos/asset/PrefabInfo.h"

#include <memory>

namespace cc {

class Node;
class Component;

namespace prefab_utils {

// Hierarchical map keyed by fileId at each nesting level. At a given
// level, keys partition into three buckets — node refs, component refs,
// and sub-maps for nested prefab instances. A `localID` is just a walk
// through this tree, following whichever bucket contains the next key.
struct TargetMap {
    ccstd::unordered_map<ccstd::string, Node *>                       nodes;
    ccstd::unordered_map<ccstd::string, Component *>                  components;
    ccstd::unordered_map<ccstd::string, std::unique_ptr<TargetMap>>   subMaps;
};

// Traverse a freshly cloned prefab tree and populate the targetMap.
// Walks children recursively. `isRoot=true` on the first call suppresses
// nested-prefab-submap creation for the instance's own root (the root's
// fileId sits at the top level).
void buildTargetMap(Node *node, TargetMap &targetMap, bool isRoot);

// Follow `localID` through the map. Returns:
//   - a Node* / Component* if the final key resolves to a leaf
//   - nullptr if the path is malformed or a key isn't found
// Use isNode/isComponent helpers to discriminate; raw CCObject* suffices
// for apply-time dispatch (apply functions cast after resolving).
CCObject *getTarget(const ccstd::vector<ccstd::string> &localID,
                    const TargetMap &targetMap);

// Apply the five override categories. Each is a no-op when its input
// vector is empty. Order matters and matches upstream — see header
// comment at top. Never throws; malformed overrides log + skip.

void applyMountedChildren(
    const ccstd::vector<IntrusivePtr<MountedChildrenInfo>> &mountedChildren,
    TargetMap &targetMap);

void applyMountedComponents(
    const ccstd::vector<IntrusivePtr<MountedComponentsInfo>> &mountedComponents,
    TargetMap &targetMap);

void applyRemovedComponents(
    const ccstd::vector<IntrusivePtr<TargetInfo>> &removedComponents,
    TargetMap &targetMap);

void applyPropertyOverrides(
    const ccstd::vector<IntrusivePtr<PropertyOverrideInfo>> &propertyOverrides,
    TargetMap &targetMap);

void applyTargetOverrides(
    const ccstd::vector<IntrusivePtr<TargetOverrideInfo>> &targetOverrides,
    TargetMap &targetMap);

}  // namespace prefab_utils
}  // namespace cc
