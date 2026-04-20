#pragma once
#include <cstddef>
#include <type_traits>
#include <utility>
#include "base/Ptr.h"
#include "base/memory/Memory.h"
#include "base/std/container/vector.h"
#include "ClassDB.h"
#include "ClassMeta.h"
#include "MethodMeta.h"
#include "TypeId.h"

namespace cc {
namespace reflection {

namespace detail {
// SFINAE: detects whether T has a release() method callable with no args.
template <typename T, typename = void>
struct HasRelease : std::false_type {};

template <typename T>
struct HasRelease<T, std::void_t<decltype(std::declval<T *>()->release())>> : std::true_type {};

// ─── MethodArg → native-type coercion ────────────────────────────────────
//
// Upstream's ComponentEventHandler.emit() is duck-typed: the TS handler can
// declare whatever arity it likes and JS silently drops or undefines missing
// args. We emulate that with default-valued coercion — missing indices fall
// back to a zero / empty value of the target type.

template <typename T>
struct ArgCoercer;

template <>
struct ArgCoercer<bool> {
    static bool get(const MethodArgs &args, std::size_t i) {
        return i < args.size() && args[i].kind == MethodArg::Kind::BOOL ? args[i].b : false;
    }
};

template <>
struct ArgCoercer<int32_t> {
    static int32_t get(const MethodArgs &args, std::size_t i) {
        return i < args.size() && args[i].kind == MethodArg::Kind::INT32 ? args[i].i : 0;
    }
};

template <>
struct ArgCoercer<uint32_t> {
    static uint32_t get(const MethodArgs &args, std::size_t i) {
        return i < args.size() && args[i].kind == MethodArg::Kind::INT32 ? static_cast<uint32_t>(args[i].i) : 0u;
    }
};

template <>
struct ArgCoercer<float> {
    static float get(const MethodArgs &args, std::size_t i) {
        return i < args.size() && args[i].kind == MethodArg::Kind::FLOAT ? args[i].f : 0.0f;
    }
};

// Strings get a by-value copy to keep lifetime simple; the reference form
// below avoids the copy when the callee takes `const ccstd::string&`.
template <>
struct ArgCoercer<ccstd::string> {
    static ccstd::string get(const MethodArgs &args, std::size_t i) {
        return i < args.size() && args[i].kind == MethodArg::Kind::STRING ? args[i].s : ccstd::string{};
    }
};

template <>
struct ArgCoercer<const ccstd::string &> {
    static const ccstd::string &get(const MethodArgs &args, std::size_t i) {
        static const ccstd::string kEmpty;
        return (i < args.size() && args[i].kind == MethodArg::Kind::STRING) ? args[i].s : kEmpty;
    }
};

// Any pointer-to-T arg pulls from MethodArg::p. We can't verify the concrete
// pointee type at invocation time, so callers are responsible for pushing
// pointers that match the declared handler signature (the Editor produces
// these via target-uuid+component-name lookup, which is trustworthy).
template <typename T>
struct ArgCoercer<T *> {
    static T *get(const MethodArgs &args, std::size_t i) {
        return i < args.size() && args[i].kind == MethodArg::Kind::POINTER
               ? static_cast<T *>(args[i].p) : nullptr;
    }
};

template <typename ClassT, typename... Args, std::size_t... Is>
void invokeWithArgs(ClassT *inst, void (ClassT::*fn)(Args...),
                    const MethodArgs &args, std::index_sequence<Is...>) {
    (inst->*fn)(ArgCoercer<Args>::get(args, Is)...);
}
}  // namespace detail

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
        if constexpr (detail::HasRelease<ClassT>::value) {
            _meta->addRef  = [](void *p) { static_cast<ClassT *>(p)->addRef(); };
            _meta->release = [](void *p) { static_cast<ClassT *>(p)->release(); };
        } else {
            _meta->addRef  = nullptr;
            _meta->release = nullptr;
        }
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

    // Reflect a single IntrusivePtr<ElemT> field — used for asset refs
    // like Sprite::_texture : IntrusivePtr<Texture2D>. Serialised form is
    // `{"__uuid__":"..."}`; the deserializer resolves via AssetManager
    // and hands us a raw T* which IntrusivePtr::operator= then addRefs.
    template <typename ElemT>
    ClassBuilder &property(const char *propName, IntrusivePtr<ElemT> ClassT::*field) {
        PropertyMeta p;
        p.name = propName;
        p.typeId = TypeId::POINTER;
        p.pointeeClassFn = &ElemT::getStaticClass;
        p.setter = [field](void *inst, const void *value) {
            ElemT *ptr = *static_cast<ElemT *const *>(value);
            (static_cast<ClassT *>(inst)->*field) = ptr;
        };
        p.getter = [field](const void *inst, void *outValue) {
            ElemT *ptr = (static_cast<const ClassT *>(inst)->*field).get();
            *static_cast<ElemT **>(outValue) = ptr;
        };
        p.applyDefault = [field](void *inst) {
            (static_cast<ClassT *>(inst)->*field).reset();
        };
        _meta->properties.push_back(std::move(p));
        return *this;
    }

    // Reflect an array property stored as ccstd::vector<ccstd::string> —
    // used by prefab TargetInfo.localID / PropertyOverrideInfo.propertyPath,
    // where the serialized form is a JSON array of strings.
    ClassBuilder &property(const char *propName,
                           ccstd::vector<ccstd::string> ClassT::*field) {
        PropertyMeta p;
        p.name = propName;
        p.typeId = TypeId::ARRAY;
        p.elementTypeId = TypeId::STRING;
        p.applyDefault = [field](void *inst) {
            (static_cast<ClassT *>(inst)->*field).clear();
        };
        p.arrayClear = [field](void *inst) {
            (static_cast<ClassT *>(inst)->*field).clear();
        };
        p.arrayAppend = [field](void *inst, void *elementValue) {
            const ccstd::string *src = static_cast<const ccstd::string *>(elementValue);
            (static_cast<ClassT *>(inst)->*field).push_back(*src);
        };
        _meta->properties.push_back(std::move(p));
        return *this;
    }

    // Reflect an array property stored as ccstd::vector<IntrusivePtr<ElemT>>.
    // Deserializer uses arrayClear + arrayAppend to rebuild the vector from a
    // JSON array. ElemT must be a reflected class.
    template <typename ElemT>
    ClassBuilder &property(const char *propName,
                           ccstd::vector<IntrusivePtr<ElemT>> ClassT::*field) {
        PropertyMeta p;
        p.name = propName;
        p.typeId = TypeId::ARRAY;
        p.elementTypeId = TypeId::POINTER;
        p.pointeeClassFn = &ElemT::getStaticClass;  // resolved lazily — avoids self-recursion
        p.applyDefault = [field](void *inst) {
            (static_cast<ClassT *>(inst)->*field).clear();
        };
        p.arrayClear = [field](void *inst) {
            (static_cast<ClassT *>(inst)->*field).clear();
        };
        p.arrayAppend = [field](void *inst, void *elementValue) {
            ElemT *ptr = *static_cast<ElemT **>(elementValue);
            (static_cast<ClassT *>(inst)->*field).emplace_back(ptr);
        };
        _meta->properties.push_back(std::move(p));
        return *this;
    }

    // Reflect a member method. The signature may take any combination of the
    // supported arg kinds (bool / int32 / uint32 / float / ccstd::string /
    // const ccstd::string& / pointer-to-T). At invoke time each positional
    // arg is coerced from MethodArg; missing or type-mismatched args resolve
    // to the target type's default value — matching the loose JS semantics of
    // ComponentEventHandler.emit.
    //
    // Usage:
    //   .method("onClick",       &MyComp::onClick)                 // void()
    //   .method("onButtonClick", &MyComp::onButtonClick)           // void(Button*, const string&)
    template <typename... Args>
    ClassBuilder &method(const char *methodName, void (ClassT::*fn)(Args...)) {
        MethodMeta m;
        m.name = methodName;
        m.arity = static_cast<uint32_t>(sizeof...(Args));
        m.invoker = [fn](void *inst, const MethodArgs &args) {
            detail::invokeWithArgs<ClassT, Args...>(
                static_cast<ClassT *>(inst), fn, args,
                std::index_sequence_for<Args...>{});
        };
        _meta->methods.push_back(std::move(m));
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
