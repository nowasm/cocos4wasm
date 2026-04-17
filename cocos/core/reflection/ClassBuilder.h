#pragma once
#include <utility>
#include "base/memory/Memory.h"
#include "ClassDB.h"
#include "ClassMeta.h"
#include "TypeId.h"

namespace cc {
namespace reflection {

// Per-class fluent builder for ClassMeta. One instance is created inside each
// class's `getStaticClass()` to chain property() calls and register with
// ClassDB on build().
//
// The ClassMeta instance itself is a function-local static inside metaFor(),
// uniquely instantiated per ClassT, so the lifetime spans the entire process
// without any heap allocation.
template <typename ClassT>
class ClassBuilder {
public:
    ClassBuilder(const char *name, const ClassMeta *base) {
        _meta = &metaFor();
        _meta->name = name;
        _meta->base = base;
        _meta->size = sizeof(ClassT);
        _meta->factory = []() -> void * { return ccnew ClassT(); };
        _meta->properties.clear();  // idempotent if builder is invoked twice
    }

    // Reflect a field, using the type's default constructor as the default
    // value.
    template <typename FieldT>
    ClassBuilder &property(const char *propName, FieldT ClassT::*field) {
        PropertyMeta p;
        p.name = propName;
        p.typeId = typeIdOf<FieldT>();
        p.setter = [field](void *inst, const void *value) {
            static_cast<ClassT *>(inst)->*field = *static_cast<const FieldT *>(value);
        };
        p.getter = [field](const void *inst, void *outValue) {
            *static_cast<FieldT *>(outValue) = static_cast<const ClassT *>(inst)->*field;
        };
        p.applyDefault = [field](void *inst) {
            static_cast<ClassT *>(inst)->*field = FieldT{};
        };
        _meta->properties.push_back(std::move(p));
        return *this;
    }

    // Reflect a field with an explicit default value (copied by value into the
    // closure and stamped on fresh instances by applyDefault).
    template <typename FieldT, typename DefaultT>
    ClassBuilder &property(const char *propName, FieldT ClassT::*field, DefaultT &&defaultValue) {
        PropertyMeta p;
        p.name = propName;
        p.typeId = typeIdOf<FieldT>();
        p.setter = [field](void *inst, const void *value) {
            static_cast<ClassT *>(inst)->*field = *static_cast<const FieldT *>(value);
        };
        p.getter = [field](const void *inst, void *outValue) {
            *static_cast<FieldT *>(outValue) = static_cast<const ClassT *>(inst)->*field;
        };
        FieldT captured(std::forward<DefaultT>(defaultValue));
        p.applyDefault = [field, captured](void *inst) {
            static_cast<ClassT *>(inst)->*field = captured;
        };
        _meta->properties.push_back(std::move(p));
        return *this;
    }

    const ClassMeta *build() {
        ClassDB::get().registerClass(_meta);
        return _meta;
    }

private:
    static ClassMeta &metaFor() {
        static ClassMeta m;
        return m;
    }

    ClassMeta *_meta{nullptr};
};

}  // namespace reflection
}  // namespace cc
