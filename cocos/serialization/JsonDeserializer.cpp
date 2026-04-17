#include "cocos/serialization/JsonDeserializer.h"

#include <cstring>

#include "base/Log.h"
#include "core/reflection/ClassDB.h"
#include "core/reflection/ClassMeta.h"
#include "core/reflection/PropertyMeta.h"
#include "math/Color.h"
#include "math/Mat4.h"
#include "math/Quaternion.h"
#include "math/Vec2.h"
#include "math/Vec3.h"
#include "math/Vec4.h"
#include "rapidjson/document.h"

namespace cc {
namespace serialization {

using reflection::ClassDB;
using reflection::ClassMeta;
using reflection::PropertyMeta;
using reflection::TypeId;
using JsonValue = rapidjson::Value;

namespace {

// Extract a numeric (int or float) from a JSON value, with a fallback.
float jsonAsFloat(const JsonValue &v, float fallback = 0.0f) {
    if (v.IsNumber()) return v.GetFloat();
    return fallback;
}

int32_t jsonAsInt(const JsonValue &v, int32_t fallback = 0) {
    if (v.IsInt()) return v.GetInt();
    if (v.IsUint()) return static_cast<int32_t>(v.GetUint());
    if (v.IsNumber()) return static_cast<int32_t>(v.GetDouble());
    return fallback;
}

// Cocos JSON convention: {"x":..., "y":...} for Vec2/Vec3/Vec4, optional components default to 0.
bool decodeVec2(const JsonValue &v, Vec2 &out) {
    if (!v.IsObject()) return false;
    out.x = v.HasMember("x") ? jsonAsFloat(v["x"]) : 0.0f;
    out.y = v.HasMember("y") ? jsonAsFloat(v["y"]) : 0.0f;
    return true;
}

bool decodeVec3(const JsonValue &v, Vec3 &out) {
    if (!v.IsObject()) return false;
    out.x = v.HasMember("x") ? jsonAsFloat(v["x"]) : 0.0f;
    out.y = v.HasMember("y") ? jsonAsFloat(v["y"]) : 0.0f;
    out.z = v.HasMember("z") ? jsonAsFloat(v["z"]) : 0.0f;
    return true;
}

bool decodeVec4(const JsonValue &v, Vec4 &out) {
    if (!v.IsObject()) return false;
    out.x = v.HasMember("x") ? jsonAsFloat(v["x"]) : 0.0f;
    out.y = v.HasMember("y") ? jsonAsFloat(v["y"]) : 0.0f;
    out.z = v.HasMember("z") ? jsonAsFloat(v["z"]) : 0.0f;
    out.w = v.HasMember("w") ? jsonAsFloat(v["w"]) : 0.0f;
    return true;
}

bool decodeQuat(const JsonValue &v, Quaternion &out) {
    if (!v.IsObject()) return false;
    out.x = v.HasMember("x") ? jsonAsFloat(v["x"]) : 0.0f;
    out.y = v.HasMember("y") ? jsonAsFloat(v["y"]) : 0.0f;
    out.z = v.HasMember("z") ? jsonAsFloat(v["z"]) : 0.0f;
    out.w = v.HasMember("w") ? jsonAsFloat(v["w"], 1.0f) : 1.0f;
    return true;
}

bool decodeColor(const JsonValue &v, Color &out) {
    if (!v.IsObject()) return false;
    out.r = static_cast<uint8_t>(v.HasMember("r") ? jsonAsInt(v["r"], 255) : 255);
    out.g = static_cast<uint8_t>(v.HasMember("g") ? jsonAsInt(v["g"], 255) : 255);
    out.b = static_cast<uint8_t>(v.HasMember("b") ? jsonAsInt(v["b"], 255) : 255);
    out.a = static_cast<uint8_t>(v.HasMember("a") ? jsonAsInt(v["a"], 255) : 255);
    return true;
}

bool isIdRef(const JsonValue &v) {
    return v.IsObject() && v.HasMember("__id__") && v["__id__"].IsInt();
}

// Decode a JSON value into the reflected property's field and invoke setter.
// Caller handles arrays and pointers separately; this path covers all
// value-type scalars.
bool decodeAndSet(void *inst, const PropertyMeta &prop, const JsonValue &v) {
    switch (prop.typeId) {
        case TypeId::BOOL: {
            bool x = v.IsBool() ? v.GetBool() : (v.IsInt() ? v.GetInt() != 0 : false);
            if (prop.setter) prop.setter(inst, &x);
            return true;
        }
        case TypeId::INT32:
        case TypeId::ENUM: {
            int32_t x = jsonAsInt(v);
            if (prop.setter) prop.setter(inst, &x);
            return true;
        }
        case TypeId::UINT32: {
            uint32_t x = v.IsUint() ? v.GetUint() : static_cast<uint32_t>(jsonAsInt(v));
            if (prop.setter) prop.setter(inst, &x);
            return true;
        }
        case TypeId::INT64: {
            int64_t x = v.IsInt64() ? v.GetInt64() : jsonAsInt(v);
            if (prop.setter) prop.setter(inst, &x);
            return true;
        }
        case TypeId::UINT64: {
            uint64_t x = v.IsUint64() ? v.GetUint64() : static_cast<uint64_t>(jsonAsInt(v));
            if (prop.setter) prop.setter(inst, &x);
            return true;
        }
        case TypeId::FLOAT: {
            float x = jsonAsFloat(v);
            if (prop.setter) prop.setter(inst, &x);
            return true;
        }
        case TypeId::DOUBLE: {
            double x = v.IsNumber() ? v.GetDouble() : 0.0;
            if (prop.setter) prop.setter(inst, &x);
            return true;
        }
        case TypeId::STRING: {
            ccstd::string x = v.IsString() ? v.GetString() : "";
            if (prop.setter) prop.setter(inst, &x);
            return true;
        }
        case TypeId::VEC2: {
            Vec2 x;
            if (!decodeVec2(v, x)) return false;
            if (prop.setter) prop.setter(inst, &x);
            return true;
        }
        case TypeId::VEC3: {
            Vec3 x;
            if (!decodeVec3(v, x)) return false;
            if (prop.setter) prop.setter(inst, &x);
            return true;
        }
        case TypeId::VEC4: {
            Vec4 x;
            if (!decodeVec4(v, x)) return false;
            if (prop.setter) prop.setter(inst, &x);
            return true;
        }
        case TypeId::QUAT: {
            Quaternion x;
            if (!decodeQuat(v, x)) return false;
            if (prop.setter) prop.setter(inst, &x);
            return true;
        }
        case TypeId::COLOR: {
            Color x;
            if (!decodeColor(v, x)) return false;
            if (prop.setter) prop.setter(inst, &x);
            return true;
        }
        case TypeId::MAT4:
            // Rare in authoring data; defer until needed.
            return false;
        default:
            return false;
    }
}

}  // namespace

// ─── JsonDeserializer impl ─────────────────────────────────────────────────

void JsonDeserializer::reset() {
    _slots.clear();
    _objects.clear();
}

void *JsonDeserializer::deserialize(const char *jsonText, size_t jsonLength) {
    reset();

    rapidjson::Document doc;
    doc.Parse(jsonText, jsonLength);
    if (doc.HasParseError()) {
        CC_LOG_ERROR("[deserialize] JSON parse error at offset %zu",
                     (size_t)doc.GetErrorOffset());
        return nullptr;
    }
    if (!doc.IsArray()) {
        CC_LOG_ERROR("[deserialize] top-level JSON must be an array");
        return nullptr;
    }

    const auto &arr = doc;

    // ── Phase 1: allocate every object by __type__, build the index table.
    _slots.reserve(arr.Size());
    _objects.reserve(arr.Size());
    for (rapidjson::SizeType i = 0; i < arr.Size(); ++i) {
        const JsonValue &elem = arr[i];
        if (!elem.IsObject() || !elem.HasMember("__type__") || !elem["__type__"].IsString()) {
            CC_LOG_ERROR("[deserialize] index %u: missing __type__", (unsigned)i);
            _slots.push_back({nullptr, nullptr});
            _objects.push_back(nullptr);
            continue;
        }
        const char *typeName = elem["__type__"].GetString();
        const ClassMeta *meta = ClassDB::get().findByName(typeName);
        if (!meta || !meta->factory) {
            CC_LOG_ERROR("[deserialize] index %u: unknown class '%s'", (unsigned)i, typeName);
            _slots.push_back({nullptr, nullptr});
            _objects.push_back(nullptr);
            continue;
        }
        void *obj = meta->factory();
        // RefCounted types start at refcount=0 out of ccnew. Claim one
        // initial ref so the graph-building emplace_backs (which addRef)
        // and phase-3 balance both work cleanly.
        if (meta->addRef) meta->addRef(obj);
        _slots.push_back({obj, meta});
        _objects.push_back(obj);
    }

    // ── Phase 2: resolve properties. __id__ refs now patch cleanly since all
    // allocations are complete.
    for (rapidjson::SizeType i = 0; i < arr.Size(); ++i) {
        const Slot &slot = _slots[i];
        if (!slot.ptr || !slot.meta) continue;
        const JsonValue &elem = arr[i];

        for (auto it = elem.MemberBegin(); it != elem.MemberEnd(); ++it) {
            const char *key = it->name.GetString();
            if (std::strcmp(key, "__type__") == 0) continue;
            if (std::strcmp(key, "__id__") == 0) continue;

            const PropertyMeta *prop = slot.meta->findProperty(key);
            if (!prop) continue;  // unknown fields silently skipped — forward compat

            const JsonValue &v = it->value;

            // Array of pointer references
            if (prop->typeId == TypeId::ARRAY) {
                if (!v.IsArray()) continue;
                if (prop->arrayClear) prop->arrayClear(slot.ptr);
                for (const JsonValue &elemV : v.GetArray()) {
                    if (prop->elementTypeId == TypeId::POINTER) {
                        if (!isIdRef(elemV)) continue;
                        int id = elemV["__id__"].GetInt();
                        if (id < 0 || id >= (int)_slots.size()) continue;
                        void *target = _slots[id].ptr;
                        if (prop->arrayAppend) prop->arrayAppend(slot.ptr, &target);
                    }
                    // non-pointer element types deferred
                }
                continue;
            }

            // Single pointer reference
            if (prop->typeId == TypeId::POINTER) {
                if (!isIdRef(v)) {
                    if (v.IsNull() && prop->setter) {
                        void *np = nullptr;
                        prop->setter(slot.ptr, &np);
                    }
                    continue;
                }
                int id = v["__id__"].GetInt();
                if (id < 0 || id >= (int)_slots.size()) continue;
                void *target = _slots[id].ptr;
                if (prop->setter) prop->setter(slot.ptr, &target);
                continue;
            }

            // Value types / scalars
            decodeAndSet(slot.ptr, *prop, v);
        }
    }

    // ── Phase 3: hand off ownership. Release the initial ccnew refcount on
    // every non-root object; they are now retained by their parent's
    // IntrusivePtr members. The root's initial refcount is preserved and
    // passed to the caller.
    void *root = _slots.empty() ? nullptr : _slots[0].ptr;
    for (size_t i = 1; i < _slots.size(); ++i) {
        auto &s = _slots[i];
        if (s.ptr && s.meta && s.meta->release) {
            s.meta->release(s.ptr);
        }
    }

    return root;
}

}  // namespace serialization
}  // namespace cc
