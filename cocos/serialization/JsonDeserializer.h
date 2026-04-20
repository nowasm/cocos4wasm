#pragma once

#include "base/std/container/string.h"
#include "base/std/container/vector.h"

namespace cc {
namespace reflection {
struct ClassMeta;
struct PropertyMeta;
}  // namespace reflection

namespace serialization {

// Minimal JSON → object-graph deserializer for the Cocos Creator array format:
//
//   [
//     {"__type__": "cc.Node",      "children":   [{"__id__": 1}],
//                                  "components": [{"__id__": 2}]},
//     {"__type__": "cc.Node"},
//     {"__type__": "demo.MyComp",  "enabled": true, "tag": "hello"}
//   ]
//
// Two-phase: first allocate every object by __type__ (filling an index table),
// then resolve each object's properties — __id__ references are patched from
// the table, basic types decode directly, Vec3/Color etc. unpack from their
// canonical JSON shape, and ARRAY properties append one element at a time.
//
// P1c scope: scalars (bool/int/uint/float/string/enum), nested value types
// (Vec2/Vec3/Vec4/Color/Quat), pointer-array properties (vector<IntrusivePtr<T>>),
// __id__ refs. Out of scope for now: __uuid__ asset refs (P3), prefab
// instancing (P3), enum-by-name decoding, non-pointer arrays.
//
// Ownership: the deserializer returns the root with refcount=1 (it holds one
// initial reference on behalf of the caller). Non-root objects are retained
// solely by their parent's IntrusivePtr vectors (refcount=1). Typical usage:
//   cc::Node *root = deserializer.deserializeAs<cc::Node>(jsonText);
//   // ... use root ...
//   root->release();   // cascades through vector destructors
// Or wrap in IntrusivePtr with operator= to skip the extra addRef:
//   cc::IntrusivePtr<cc::Node> rootPtr;
//   rootPtr = deserializer.deserializeAs<cc::Node>(jsonText);
class JsonDeserializer {
public:
    JsonDeserializer() = default;
    ~JsonDeserializer() = default;

    // Returns the object at `rootIndex` on success, nullptr if:
    //   - JSON fails to parse
    //   - top level is not an array
    //   - rootIndex has no reflectable __type__
    // Partial failures on inner objects log an error and continue.
    //
    // rootIndex lets callers pick a non-zero object as the owned result —
    // used by PrefabLoader to skip the `cc.Prefab` wrapper and return the
    // wrapped Node at `wrapper.data.__id__`.
    void *deserialize(const char *jsonText, size_t jsonLength, size_t rootIndex = 0);
    void *deserialize(const ccstd::string &jsonText, size_t rootIndex = 0) {
        return deserialize(jsonText.data(), jsonText.size(), rootIndex);
    }

    template <typename T>
    T *deserializeAs(const ccstd::string &jsonText, size_t rootIndex = 0) {
        return static_cast<T *>(deserialize(jsonText, rootIndex));
    }

    size_t objectCount() const { return _objects.size(); }
    void  *objectAt(size_t idx) const { return idx < _objects.size() ? _objects[idx] : nullptr; }

private:
    struct Slot {
        void *ptr{nullptr};
        const reflection::ClassMeta *meta{nullptr};
    };

    void reset();

    ccstd::vector<Slot>   _slots;    // indexed by __id__
    ccstd::vector<void *> _objects;  // parallel to _slots; pointer form only
};

// Decode an arbitrary JSON text fragment into the C++ field described by
// `prop` on `instance`. Used by prefab override application to realise
// `CCPropertyOverrideInfo.value` (stored as raw JSON text) against the
// actual property type discovered via reflection.
//
// Returns true if the value was decoded and the setter ran; false on:
//   - parse error
//   - property type not supported by the scalar decoder (e.g. POINTER —
//     UUID-to-asset resolution for override values isn't wired yet)
//
// The supported type set matches JsonDeserializer's scalar decode path:
// bool / int32 / uint32 / float / double / string / Vec2..4 / Color / Quat
// / enum. Array and pointer overrides are future work.
bool decodeJsonValueToProperty(void *instance,
                               const reflection::PropertyMeta &prop,
                               const ccstd::string &jsonText);

}  // namespace serialization
}  // namespace cc
