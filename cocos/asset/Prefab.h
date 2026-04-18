#pragma once

#include "base/std/container/string.h"
#include "core/assets/Asset.h"
#include "core/reflection/Reflection.h"

namespace cc {

class Node;

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
    Node *instantiate() const;

private:
    ccstd::string _jsonText;
    size_t _rootIndex{0};
};

}  // namespace cc
