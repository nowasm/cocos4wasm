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

    // For TypeId::POINTER: the target class's ClassMeta, if known. May be
    // null if the pointee type hasn't been reflected yet (deserialization
    // will then leave the field null and log a warning).
    const ClassMeta *pointeeClass{nullptr};

    std::function<void(void *instance, const void *value)> setter;
    std::function<void(const void *instance, void *outValue)> getter;
    std::function<void(void *instance)> applyDefault;
};

}  // namespace reflection
}  // namespace cc
