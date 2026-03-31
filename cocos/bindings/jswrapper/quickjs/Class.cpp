#include "Class.h"
#include "ScriptEngine.h"
#include "Utils.h"
#include "base/memory/Memory.h"

namespace se {

namespace {
ccstd::vector<Class *> __allClasses; // NOLINT

struct QJSClassOpaque {
    Class *cls{nullptr};
};

void qjsFinalizer(JSRuntime *rt, JSValue val) {
    auto *opaque = static_cast<QJSClassOpaque *>(JS_GetOpaque(val, 0));
    if (opaque && opaque->cls) {
        auto *fin = opaque->cls->_getFinalizeFunction();
        auto *obj = static_cast<se::Object *>(JS_GetOpaque2(nullptr, val, opaque->cls->getClassID()));
        CC_UNUSED_PARAM(obj);
        CC_UNUSED_PARAM(fin);
    }
    delete opaque;
}

JSValue qjsCallNative(JSContext *ctx, JSValue thisVal, int argc, JSValue *argv, int /*magic*/, JSValue *funcData) {
    auto *funcPtr = reinterpret_cast<NativeFunctionPtr>(JS_GetOpaque(funcData[0], 0));
    if (funcPtr == nullptr) {
        return JS_UNDEFINED;
    }

    se::ValueArray seArgs;
    seArgs.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        se::Value v;
        internal::jsToSeValue(ctx, argv[i], &v);
        seArgs.push_back(std::move(v));
    }

    se::Object *thisObj = nullptr;
    if (!JS_IsUndefined(thisVal) && !JS_IsNull(thisVal)) {
        thisObj = Object::_createJSObject(nullptr, JS_DupValue(ctx, thisVal));
    }

    se::State state(thisObj, seArgs);
    bool ok = funcPtr(state);

    if (thisObj) {
        thisObj->decRef();
    }

    if (!ok) {
        return JS_ThrowInternalError(ctx, "Native function failed");
    }

    const auto &retVal = state.rval();
    if (retVal.isUndefined()) {
        return JS_UNDEFINED;
    }
    return internal::seToJsValue(ctx, retVal);
}

JSValue createQJSFunction(JSContext *ctx, NativeFunctionPtr func) {
    JSValue funcDataVal = JS_NewObjectClass(ctx, 0);
    JS_SetOpaque(funcDataVal, reinterpret_cast<void *>(func));
    return JS_NewCFunctionData(ctx, qjsCallNative, 0, 0, 1, &funcDataVal);
}

} // namespace

Class::Class() {
    __allClasses.push_back(this);
}

Class::~Class() = default;

Class *Class::create(const ccstd::string &clsName, Object *parent, Object *parentProto, NativeFunctionPtr ctor, void *data) {
    auto *c = ccnew Class();
    if (!c->init(clsName, parent, parentProto, ctor, data)) {
        delete c;
        return nullptr;
    }
    return c;
}

Class *Class::create(const std::initializer_list<const char *> &classPath, Object *parent, Object *parentProto, NativeFunctionPtr ctor, void *data) {
    se::AutoHandleScope scope;
    se::Value currentParent{parent};
    int i = 0;
    int last = static_cast<int>(classPath.size()) - 1;
    for (auto *p : classPath) {
        if (i < last) {
            se::Value tmp;
            bool ok = currentParent.toObject()->getProperty(p, &tmp);
            CC_ASSERT(ok);
            currentParent = tmp;
        }
        ++i;
    }
    return create(*(classPath.end() - 1), currentParent.toObject(), parentProto, ctor, data);
}

bool Class::init(const ccstd::string &clsName, Object *parent, Object *parentProto, NativeFunctionPtr ctor, void * /*data*/) {
    _name = clsName;
    _parent = parent;
    if (_parent != nullptr) _parent->incRef();
    _parentProto = parentProto;
    if (_parentProto != nullptr) _parentProto->incRef();
    _constructor = ctor;

    JS_NewClassID(ScriptEngine::getInstance()->getRuntime(), &_classId);

    return true;
}

void Class::_setCtor(Object *obj) {
    _ctor = obj;
    if (obj != nullptr) {
        obj->root();
        obj->incRef();
    }
}

void Class::destroy() {
    if (_parent != nullptr) { _parent->decRef(); _parent = nullptr; }
    if (_proto != nullptr) { _proto->decRef(); _proto = nullptr; }
    if (_parentProto != nullptr) { _parentProto->decRef(); _parentProto = nullptr; }
    if (_ctor.has_value()) {
        if (_ctor.value() != nullptr) {
            _ctor.value()->unroot();
            _ctor.value()->decRef();
        }
        _ctor.reset();
    }
}

void Class::cleanup() {
    for (auto *cls : __allClasses) {
        cls->destroy();
    }

    se::ScriptEngine::getInstance()->addAfterCleanupHook([]() {
        for (auto *cls : __allClasses) {
            delete cls;
        }
        __allClasses.clear();
    });
}

bool Class::defineFunction(const char *name, NativeFunctionPtr func, void * /*data*/) {
    _funcs.push_back({name, func});
    return true;
}

bool Class::defineProperty(const char *name, NativeFunctionPtr getter, NativeFunctionPtr setter, void * /*data*/) {
    _props.push_back({name, getter, setter});
    return true;
}

bool Class::defineProperty(const std::initializer_list<const char *> &names, NativeFunctionPtr getter, NativeFunctionPtr setter, void *data) {
    for (const auto *n : names) {
        defineProperty(n, getter, setter, data);
    }
    return true;
}

bool Class::defineStaticFunction(const char *name, NativeFunctionPtr func, void * /*data*/) {
    _staticFuncs.push_back({name, func});
    return true;
}

bool Class::defineStaticProperty(const char *name, NativeFunctionPtr getter, NativeFunctionPtr setter, void * /*data*/) {
    _staticProps.push_back({name, getter, setter});
    return true;
}

bool Class::defineStaticProperty(const char *name, const Value &value, PropertyAttribute /*attribute*/) {
    _staticValues.push_back({name, value});
    return true;
}

bool Class::defineFinalizeFunction(V8FinalizeFunc func) {
    _finalizeFunc = func;
    return true;
}

bool Class::install() {
    auto *ctx = ScriptEngine::getInstance()->getContext();
    auto *rt = ScriptEngine::getInstance()->getRuntime();

    JSClassDef classDef{};
    classDef.class_name = _name.c_str();

    JS_NewClass(rt, _classId, &classDef);

    JSValue protoVal = JS_NewObject(ctx);

    if (_parentProto != nullptr) {
        JS_SetPrototype(ctx, protoVal, _parentProto->_getJSObject());
    }

    for (const auto &f : _funcs) {
        JSValue fn = createQJSFunction(ctx, f.func);
        JS_SetPropertyStr(ctx, protoVal, f.name.c_str(), fn);
    }

    for (const auto &p : _props) {
        JSAtom atom = JS_NewAtom(ctx, p.name.c_str());
        JSValue getter = JS_UNDEFINED;
        JSValue setter = JS_UNDEFINED;
        if (p.getter) getter = createQJSFunction(ctx, p.getter);
        if (p.setter) setter = createQJSFunction(ctx, p.setter);

        int flags = JS_PROP_HAS_CONFIGURABLE | JS_PROP_CONFIGURABLE;
        if (p.getter) flags |= JS_PROP_HAS_GET;
        if (p.setter) flags |= JS_PROP_HAS_SET;

        JS_DefineProperty(ctx, protoVal, atom, JS_UNDEFINED, getter, setter, flags);

        if (p.getter) JS_FreeValue(ctx, getter);
        if (p.setter) JS_FreeValue(ctx, setter);
        JS_FreeAtom(ctx, atom);
    }

    JS_SetClassProto(ctx, _classId, protoVal);

    JSValue ctorFunc = JS_UNDEFINED;
    if (_constructor != nullptr) {
        ctorFunc = createQJSFunction(ctx, _constructor);
    } else {
        ctorFunc = JS_NewObject(ctx);
    }

    for (const auto &sf : _staticFuncs) {
        JSValue fn = createQJSFunction(ctx, sf.func);
        JS_SetPropertyStr(ctx, ctorFunc, sf.name.c_str(), fn);
    }

    for (const auto &sv : _staticValues) {
        JSValue val = internal::seToJsValue(ctx, sv.second);
        JS_SetPropertyStr(ctx, ctorFunc, sv.first.c_str(), val);
    }

    if (_parent != nullptr) {
        JS_SetPropertyStr(ctx, _parent->_getJSObject(), _name.c_str(), JS_DupValue(ctx, ctorFunc));
    }

    if (_createProto) {
        JSValue protoRef = JS_GetClassProto(ctx, _classId);
        _proto = Object::_createJSObject(this, protoRef);
        _proto->root();
    }

    Object *ctorObj = Object::_createJSObject(nullptr, ctorFunc);
    _setCtor(ctorObj);

    return true;
}

Object *Class::getProto() const {
    return _proto;
}

V8FinalizeFunc Class::_getFinalizeFunction() const {
    return _finalizeFunc;
}

Object *Class::_createJSObjectWithClass(Class *cls) {
    if (cls == nullptr) return nullptr;
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue obj = JS_NewObjectClass(ctx, static_cast<int>(cls->_classId));
    return Object::_createJSObject(cls, obj);
}

} // namespace se
