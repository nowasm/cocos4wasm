#include "cocos/asset/Prefab.h"

#include "base/Log.h"
#include "cocos/asset/PrefabInfo.h"
#include "cocos/asset/prefab_utils.h"
#include "cocos/serialization/JsonDeserializer.h"
#include "core/scene-graph/Node.h"

namespace cc {

// Reflected so the JSON deserialiser can allocate a `cc.Prefab` slot 0
// when Creator-style wrapped .prefab files pass through the pipeline.
// The allocated instance is discarded (rootIndex > 0 selects the wrapped
// Node); this just silences the "unknown class" error.
CC_IMPLEMENT_ROOT_CLASS(Prefab, "cc.Prefab")
CC_END_CLASS(Prefab);

Node *Prefab::instantiate() const {
    return instantiate(nullptr, nullptr);
}

Node *Prefab::instantiate(
    PrefabInstance *instance,
    const ccstd::vector<IntrusivePtr<TargetOverrideInfo>> *targetOverrides) const {
    if (_jsonText.empty()) {
        CC_LOG_ERROR("[Prefab] instantiate called on empty prefab");
        return nullptr;
    }
    serialization::JsonDeserializer d;
    Node *root = d.deserializeAs<Node>(_jsonText, _rootIndex);
    if (!root) {
        CC_LOG_ERROR("[Prefab] deserialize failed");
        return nullptr;
    }
    root->addRef();  // caller owns the returned pointer

    // Short-circuit: no overrides → plain master clone.
    if (!instance && (!targetOverrides || targetOverrides->empty())) {
        return root;
    }

    // Build the fileId index over the cloned tree so override localIDs
    // resolve. Apply the five categories in the documented order.
    prefab_utils::TargetMap targetMap;
    prefab_utils::buildTargetMap(root, targetMap, /*isRoot*/ true);

    if (instance) {
        prefab_utils::applyMountedChildren(instance->mountedChildren, targetMap);
        prefab_utils::applyRemovedComponents(instance->removedComponents, targetMap);
        prefab_utils::applyMountedComponents(instance->mountedComponents, targetMap);
        prefab_utils::applyPropertyOverrides(instance->propertyOverrides, targetMap);
        instance->expanded = true;
    }
    if (targetOverrides) {
        prefab_utils::applyTargetOverrides(*targetOverrides, targetMap);
    }

    return root;
}

void Prefab::expandInto(Node *instanceNode) const {
    if (!instanceNode || _jsonText.empty()) return;

    // Produce a plain master clone (no overrides yet — we apply them
    // against `instanceNode` after transplant).
    IntrusivePtr<Node> masterRoot(instantiate());
    if (!masterRoot) return;

    // Transfer the master's transform / name / layer-level state onto
    // the instance. The identity fields (_id, _parent, current _prefab
    // pointer) stay put — they belong to the scene slot, not the master.
    // Authoring-visible state that came from the master:
    instanceNode->setPosition(masterRoot->getPosition());
    instanceNode->setRotation(masterRoot->getRotation());
    instanceNode->setScale(masterRoot->getScale());
    if (!masterRoot->getName().empty() && instanceNode->getName().empty()) {
        instanceNode->setName(masterRoot->getName());
    }

    // Reparent master's children onto the instance. setParent handles
    // removal from the master's own _children vector.
    auto childrenSnapshot = masterRoot->getChildren();
    for (auto &child : childrenSnapshot) {
        if (child) child->setParent(instanceNode);
    }

    // Components can't be moved via setParent; re-attach each through
    // addComponent. That bumps the refcount on the instance side; the
    // master's vector drops its ref when masterRoot destructs.
    auto compsSnapshot = masterRoot->getComponentList();
    for (auto &comp : compsSnapshot) {
        if (comp) instanceNode->addComponent(comp.get());
    }

    // Apply overrides once the transplanted tree is in place.
    PrefabInfo *pi = instanceNode->_prefab.get();
    if (!pi || !pi->instance || pi->instance->expanded) return;

    prefab_utils::TargetMap map;
    prefab_utils::buildTargetMap(instanceNode, map, /*isRoot*/ true);
    auto *inst = pi->instance.get();
    prefab_utils::applyMountedChildren(inst->mountedChildren, map);
    prefab_utils::applyRemovedComponents(inst->removedComponents, map);
    prefab_utils::applyMountedComponents(inst->mountedComponents, map);
    prefab_utils::applyPropertyOverrides(inst->propertyOverrides, map);
    if (!pi->targetOverrides.empty()) {
        prefab_utils::applyTargetOverrides(pi->targetOverrides, map);
    }
    inst->expanded = true;
}

int expandPrefabInstanceNode(Node *node) {
    if (!node) return 0;
    int expanded = 0;

    // Children are walked first (bottom-up) — nested prefab instances
    // must expand before the outer prefab's overrides try to reference
    // their fileIds in the targetMap.
    auto childrenSnapshot = node->getChildren();
    for (auto &child : childrenSnapshot) {
        if (child) expanded += expandPrefabInstanceNode(child.get());
    }

    PrefabInfo *pi = node->_prefab.get();
    if (pi && pi->asset && pi->instance && !pi->instance->expanded) {
        pi->asset->expandInto(node);
        ++expanded;
    }
    return expanded;
}

}  // namespace cc
