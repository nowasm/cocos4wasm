#pragma once
#include <functional>
#include <vector>
#include "PropertyMeta.h"

namespace cc {
namespace reflection {

// Runtime description of a reflected class.
//
// Each reflected class owns a single ClassMeta instance, constructed lazily by
// its `getStaticClass()` function and registered into ClassDB. `properties`
// contains only properties declared directly on this class — walking `base`
// recovers inherited properties.
struct ClassMeta {
    const char *name{nullptr};
    const ClassMeta *base{nullptr};
    size_t size{0};
    std::function<void *()> factory;

    std::vector<PropertyMeta> properties;

    bool isDerivedFrom(const ClassMeta *other) const;

    // Walks own properties first, then base chain. Returns null if not found.
    const PropertyMeta *findProperty(const char *name) const;
};

}  // namespace reflection
}  // namespace cc
