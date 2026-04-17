#pragma once
#include <functional>
#include "TypeId.h"

namespace cc {
namespace reflection {

struct ClassMeta;

// Runtime description of a single reflected field on a ClassMeta.
//
// setter/getter/applyDefault use type-erased lambdas. `instance` is always a
// void* pointing at the most-derived class layout; casts inside the lambdas
// handle the rest. This allows reflection to touch private members without
// exposing them in the public header.
struct PropertyMeta {
    const char *name{nullptr};
    TypeId typeId{TypeId::UNKNOWN};

    // Resolver for the pointee class (TypeId::POINTER) or array element
    // class (TypeId::ARRAY of POINTER). Stored as a function pointer rather
    // than a cached ClassMeta* so that self-referential fields like
    // Node._children (vector<IntrusivePtr<Node>>) don't recurse into Node's
    // own getStaticClass during static init.
    const ClassMeta *(*pointeeClassFn)(){nullptr};

    // Populated only when typeId == ARRAY — type of each element.
    TypeId elementTypeId{TypeId::UNKNOWN};

    // Scalar accessors. setter/getter are empty when typeId == ARRAY; the
    // deserializer uses the array-specific accessors below instead.
    std::function<void(void *instance, const void *value)> setter;
    std::function<void(const void *instance, void *outValue)> getter;
    std::function<void(void *instance)> applyDefault;

    // Array-only accessors. arrayAppend receives a raw element value — for
    // vector<IntrusivePtr<T>> that's a T** pointing at the target pointer;
    // the builder wraps it in an IntrusivePtr inside the lambda.
    std::function<void(void *instance)> arrayClear;
    std::function<void(void *instance, void *elementValue)> arrayAppend;
};

}  // namespace reflection
}  // namespace cc
