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

    // Returns the first top-level object (index 0) on success, nullptr if:
    //   - JSON fails to parse
    //   - top level is not an array
    //   - index 0 has no reflectable __type__
    // Partial failures on inner objects log an error and continue.
    void *deserialize(const char *jsonText, size_t jsonLength);
    void *deserialize(const ccstd::string &jsonText) {
        return deserialize(jsonText.data(), jsonText.size());
    }

    template <typename T>
    T *deserializeAs(const ccstd::string &jsonText) {
        return static_cast<T *>(deserialize(jsonText));
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

}  // namespace serialization
}  // namespace cc
