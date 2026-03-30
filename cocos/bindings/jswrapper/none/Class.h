#pragma once

#include "Object.h"
#include "../Define.h"
#include "../Value.h"
#include "Base.h"
#include "base/std/optional.h"

#include <initializer_list>

namespace se {

class Class {
public:
    static Class *create(const ccstd::string &clsName, Object *parent, Object *parentProto, NativeFunctionPtr ctor, void *data = nullptr);
    static Class *create(const std::initializer_list<const char *> &classPath, Object *parent, Object *parentProto, NativeFunctionPtr ctor, void *data = nullptr);

    bool defineFunction(const char *name, NativeFunctionPtr func, void *data = nullptr);
    bool defineProperty(const char *name, NativeFunctionPtr getter, NativeFunctionPtr setter, void *data = nullptr);
    bool defineProperty(const std::initializer_list<const char *> &names, NativeFunctionPtr getter, NativeFunctionPtr setter, void *data = nullptr);
    bool defineStaticFunction(const char *name, NativeFunctionPtr func, void *data = nullptr);
    bool defineStaticProperty(const char *name, NativeFunctionPtr getter, NativeFunctionPtr setter, void *data = nullptr);
    bool defineStaticProperty(const char *name, const Value &value, PropertyAttribute attribute = PropertyAttribute::NONE);
    bool defineFinalizeFunction(V8FinalizeFunc func);
    bool install();

    Object *getProto() const;
    const char *getName() const { return _name.c_str(); }

    V8FinalizeFunc _getFinalizeFunction() const;
    void _setCtor(Object *obj);
    inline const ccstd::optional<Object *> &_getCtor() const { return _ctor; }

    static Object *_createJSObjectWithClass(Class *cls);

private:
    Class();
    ~Class();
    bool init(const ccstd::string &clsName, Object *parent, Object *parentProto, NativeFunctionPtr ctor, void *data);
    void destroy();

    ccstd::string _name;
    Object *_parent{nullptr};
    Object *_parentProto{nullptr};
    Object *_proto{nullptr};
    ccstd::optional<Object *> _ctor;
    NativeFunctionPtr _constructor{nullptr};
    V8FinalizeFunc _finalizeFunc{nullptr};
    bool _createProto{true};

    friend class ScriptEngine;
    friend class Object;
};

} // namespace se
