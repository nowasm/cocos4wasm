#pragma once
#include <unordered_map>
#include "base/std/container/string.h"
#include "ClassMeta.h"

namespace cc {
namespace reflection {

// Global registry mapping class name ("cc.Sprite") → ClassMeta*.
//
// ClassMeta pointers are long-lived (owned by per-class static locals inside
// `getStaticClass()` functions). ClassDB only stores borrowed pointers; it
// never frees.
class ClassDB {
public:
    static ClassDB &get();

    // Registers `meta` under `meta->name`. Duplicate names log an error and
    // leave the first registration in place. Idempotent when called with the
    // same pointer.
    void registerClass(ClassMeta *meta);

    const ClassMeta *findByName(const char *name) const;
    const ClassMeta *findByName(const ccstd::string &name) const {
        return findByName(name.c_str());
    }

    // Constructs a new instance by class name. Returns nullptr if the name is
    // unknown. Caller owns the returned pointer.
    void *createInstance(const char *name) const;

    template <typename T>
    T *createInstance(const char *name) const {
        return static_cast<T *>(createInstance(name));
    }

    const std::unordered_map<ccstd::string, ClassMeta *> &allClasses() const {
        return _byName;
    }

private:
    ClassDB() = default;
    std::unordered_map<ccstd::string, ClassMeta *> _byName;
};

}  // namespace reflection
}  // namespace cc
