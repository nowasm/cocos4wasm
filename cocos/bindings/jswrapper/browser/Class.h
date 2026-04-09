#pragma once

#include "Object.h"
#include "../Define.h"
#include "../Value.h"
#include "Base.h"
#include "base/std/optional.h"

#include <emscripten/val.h>
#include <initializer_list>
#include <vector>

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

    void setCreateProto(bool createProto) { _createProto = createProto; }
    Object *getProto() const;
    const char *getName() const { return _name.c_str(); }

    // No JSClassID in browser — use a simple integer ID
    uint32_t getClassID() const { return _classId; }
    NativeFunctionPtr getConstructor() const { return _constructor; }
    V8FinalizeFunc _getFinalizeFunction() const;
    void _setCtor(Object *obj);
    inline const ccstd::optional<Object *> &_getCtor() const { return _ctor; }

    static Object *_createJSObjectWithClass(Class *cls);
    static void cleanup();

private:
    Class();
    ~Class();
    bool init(const ccstd::string &clsName, Object *parent, Object *parentProto, NativeFunctionPtr ctor, void *data);
    void destroy();

    struct FuncEntry { ccstd::string name; NativeFunctionPtr func; };
    struct PropEntry { ccstd::string name; NativeFunctionPtr getter; NativeFunctionPtr setter; };

    ccstd::string _name;
    Object *_parent{nullptr};
    Object *_parentProto{nullptr};
    Object *_proto{nullptr};
    NativeFunctionPtr _constructor{nullptr};
    V8FinalizeFunc _finalizeFunc{nullptr};
    ccstd::optional<Object *> _ctor;

    ccstd::vector<FuncEntry> _funcs;
    ccstd::vector<PropEntry> _props;
    ccstd::vector<FuncEntry> _staticFuncs;
    ccstd::vector<PropEntry> _staticProps;
    ccstd::vector<std::pair<ccstd::string, Value>> _staticValues;

    uint32_t _classId{0};
    bool _createProto{false};
};

} // namespace se
