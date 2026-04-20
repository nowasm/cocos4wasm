#pragma once

#include "base/Ptr.h"
#include "base/std/container/string.h"
#include "base/std/container/vector.h"
#include "core/assets/Asset.h"
#include "core/reflection/Reflection.h"

namespace cc {

class Node;
class PrefabInstance;
class TargetOverrideInfo;

// A Prefab wraps the serialised JSON of a Node sub-tree so it can be
// instantiated repeatedly. Each `instantiate()` deserialises a fresh copy,
// so shared refs (textures, materials) resolve through AssetManager and
// hit the cache.
//
// Why re-parse instead of deep-cloning the Node tree?
//   - Deep-cloning Components requires per-field copy logic the reflection
//     system does not yet expose.
//   - Re-parsing reuses the existing JsonDeserializer + back-ref wiring, so
//     every instance has correct Component::_node and Node::_parent links.
//   - At ~KB-per-prefab sizes the parse cost is negligible; we can swap in
//     a prototype-based clone later without touching callers.
class Prefab : public Asset {
    CC_CLASS_DECL(Prefab, void)
public:
    Prefab() = default;
    ~Prefab() override = default;

    void setData(const ccstd::string &jsonText) { _jsonText = jsonText; }
    const ccstd::string &getData() const { return _jsonText; }

    // The `cc.Prefab` wrapper (when present) lives at slot 0; the actual
    // Node tree it wraps is at `data.__id__`. PrefabLoader stashes that
    // resolved index here so instantiate() can return the right slot.
    void setRootIndex(size_t idx) { _rootIndex = idx; }
    size_t getRootIndex() const { return _rootIndex; }

    // Deserialise a fresh Node tree from the stored JSON. Caller owns the
    // returned pointer — it has an initial refcount of 1.
    //
    // The `instance` overload applies a PrefabInstance's override bundle
    // (mountedChildren / mountedComponents / removedComponents /
    // propertyOverrides) against the freshly-cloned tree, in the order
    // specified by upstream's expandPrefabInstanceNode. `targetOverrides`
    // patches cross-instance pointer refs after the main overrides land.
    // Both override parameters are optional — calling instantiate() with
    // no arguments returns a plain master clone.
    Node *instantiate() const;
    Node *instantiate(
        PrefabInstance *instance,
        const ccstd::vector<IntrusivePtr<TargetOverrideInfo>> *targetOverrides = nullptr) const;

    // In-place expansion: called by the scene-load integration when a
    // Node deserialized from a .scene JSON carries a populated `_prefab`
    // field. The empty shell `instanceNode` receives the master's
    // children / components / transform, then the PrefabInstance
    // overrides are applied against the resulting tree.
    //
    // Used via the static helper `expandPrefabInstanceNode()` below,
    // which handles the master-lookup + argument prep. Most callers
    // should use that helper.
    void expandInto(Node *instanceNode) const;

private:
    ccstd::string _jsonText;
    size_t _rootIndex{0};
};

// Scene-load helper: expands every Node in the tree that carries a
// `_prefab.asset + _prefab.instance` combo. Walks children recursively
// so nested prefabs — master-A containing an instance of master-B —
// expand correctly (inner expansion runs before outer applies its
// overrides, matching upstream's bottom-up traversal order).
//
// Idempotent: any `PrefabInstance` with `expanded = true` is skipped.
// Returns the number of instances expanded (for diagnostics only).
int expandPrefabInstanceNode(Node *node);

}  // namespace cc
