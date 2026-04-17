#pragma once
#include <cstdint>
#include <type_traits>
#include "base/std/container/string.h"
#include "math/Vec2.h"
#include "math/Vec3.h"
#include "math/Vec4.h"
#include "math/Quaternion.h"
#include "math/Mat4.h"
#include "math/Color.h"

namespace cc {
namespace reflection {

// Primary type tags used by the reflection system. Each reflected field maps
// to exactly one TypeId. The deserializer inspects TypeId to pick the right
// JSON decoder path.
enum class TypeId : uint32_t {
    UNKNOWN = 0,
    BOOL,
    INT32,
    UINT32,
    INT64,
    UINT64,
    FLOAT,
    DOUBLE,
    STRING,   // ccstd::string
    VEC2,
    VEC3,
    VEC4,
    QUAT,     // Quaternion
    MAT4,
    COLOR,    // cc::Color
    ENUM,     // any enum / enum class (stored as int32)
    POINTER,  // raw pointer to any CCObject-derived type
    ARRAY,    // ccstd::vector<IntrusivePtr<T>> or similar; element info on PropertyMeta
    // Reserved for later phases:
    //   MAP, UUID_REF, NODE_REF, OBJECT (value-embedded nested struct)
};

// Human-readable name for a TypeId, for logging and debug dumps.
const char *typeIdName(TypeId id);

// Compile-time mapping from C++ type T → TypeId. Unrecognised types resolve
// to TypeId::UNKNOWN — the property will still be registered but serialization
// will skip it. Specialise or extend when adding new TypeId values.
template <typename T>
constexpr TypeId typeIdOf() {
    using U = std::remove_cv_t<std::remove_reference_t<T>>;
    if constexpr (std::is_pointer_v<U>) {
        return TypeId::POINTER;
    } else if constexpr (std::is_enum_v<U>) {
        return TypeId::ENUM;
    } else if constexpr (std::is_same_v<U, bool>) {
        return TypeId::BOOL;
    } else if constexpr (std::is_same_v<U, int32_t>) {
        return TypeId::INT32;
    } else if constexpr (std::is_same_v<U, uint32_t>) {
        return TypeId::UINT32;
    } else if constexpr (std::is_same_v<U, int64_t>) {
        return TypeId::INT64;
    } else if constexpr (std::is_same_v<U, uint64_t>) {
        return TypeId::UINT64;
    } else if constexpr (std::is_same_v<U, float>) {
        return TypeId::FLOAT;
    } else if constexpr (std::is_same_v<U, double>) {
        return TypeId::DOUBLE;
    } else if constexpr (std::is_same_v<U, ccstd::string>) {
        return TypeId::STRING;
    } else if constexpr (std::is_same_v<U, Vec2>) {
        return TypeId::VEC2;
    } else if constexpr (std::is_same_v<U, Vec3>) {
        return TypeId::VEC3;
    } else if constexpr (std::is_same_v<U, Vec4>) {
        return TypeId::VEC4;
    } else if constexpr (std::is_same_v<U, Quaternion>) {
        return TypeId::QUAT;
    } else if constexpr (std::is_same_v<U, Mat4>) {
        return TypeId::MAT4;
    } else if constexpr (std::is_same_v<U, Color>) {
        return TypeId::COLOR;
    } else {
        return TypeId::UNKNOWN;
    }
}

}  // namespace reflection
}  // namespace cc
