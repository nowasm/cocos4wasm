#include "ClassDB.h"
#include <cstring>
#include "base/Log.h"
#include "ClassMeta.h"
#include "TypeId.h"

namespace cc {
namespace reflection {

const char *typeIdName(TypeId id) {
    switch (id) {
        case TypeId::UNKNOWN: return "unknown";
        case TypeId::BOOL:    return "bool";
        case TypeId::INT32:   return "int32";
        case TypeId::UINT32:  return "uint32";
        case TypeId::INT64:   return "int64";
        case TypeId::UINT64:  return "uint64";
        case TypeId::FLOAT:   return "float";
        case TypeId::DOUBLE:  return "double";
        case TypeId::STRING:  return "string";
        case TypeId::VEC2:    return "Vec2";
        case TypeId::VEC3:    return "Vec3";
        case TypeId::VEC4:    return "Vec4";
        case TypeId::QUAT:    return "Quat";
        case TypeId::MAT4:    return "Mat4";
        case TypeId::COLOR:   return "Color";
        case TypeId::ENUM:    return "enum";
        case TypeId::POINTER: return "pointer";
    }
    return "<bad-type-id>";
}

bool ClassMeta::isDerivedFrom(const ClassMeta *other) const {
    const ClassMeta *cur = this;
    while (cur) {
        if (cur == other) return true;
        cur = cur->base;
    }
    return false;
}

const PropertyMeta *ClassMeta::findProperty(const char *propName) const {
    if (!propName) return nullptr;
    const ClassMeta *cur = this;
    while (cur) {
        for (const auto &p : cur->properties) {
            if (p.name && std::strcmp(p.name, propName) == 0) return &p;
        }
        cur = cur->base;
    }
    return nullptr;
}

ClassDB &ClassDB::get() {
    static ClassDB instance;
    return instance;
}

void ClassDB::registerClass(ClassMeta *meta) {
    if (!meta || !meta->name) return;
    auto it = _byName.find(meta->name);
    if (it != _byName.end()) {
        if (it->second == meta) return;  // idempotent re-registration
        CC_LOG_ERROR("[reflection] duplicate class registered: %s — first registration kept",
                     meta->name);
        return;
    }
    _byName.emplace(meta->name, meta);
}

const ClassMeta *ClassDB::findByName(const char *name) const {
    if (!name) return nullptr;
    auto it = _byName.find(name);
    return it == _byName.end() ? nullptr : it->second;
}

void *ClassDB::createInstance(const char *name) const {
    const ClassMeta *m = findByName(name);
    if (!m || !m->factory) return nullptr;
    return m->factory();
}

}  // namespace reflection
}  // namespace cc
