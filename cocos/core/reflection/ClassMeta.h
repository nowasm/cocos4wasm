#pragma once
#include <functional>
#include <vector>
#include "MethodMeta.h"
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

    // Present only for RefCounted types. addRef pairs with release so the
    // deserializer can balance ownership across the graph. Empty for plain types.
    std::function<void(void *)> addRef;
    std::function<void(void *)> release;

    std::vector<PropertyMeta> properties;
    std::vector<MethodMeta> methods;

    bool isDerivedFrom(const ClassMeta *other) const;

    // Walks own properties first, then base chain. Returns null if not found.
    const PropertyMeta *findProperty(const char *name) const;

    // Same walk for methods — base-class methods are callable on derived
    // instances so long as the `this` layout is compatible (single inheritance
    // from reflected bases, which is the only shape we permit).
    const MethodMeta *findMethod(const char *name) const;

    // Convenience: look up a method by name and call it. Returns false if the
    // method isn't found; the invoker itself is infallible (arg count / type
    // mismatches resolve to defaults — matching upstream JS's loose calling
    // convention for ComponentEventHandler).
    bool invoke(void *instance, const char *methodName, const MethodArgs &args) const;
};

}  // namespace reflection
}  // namespace cc
