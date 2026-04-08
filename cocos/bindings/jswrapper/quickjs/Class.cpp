#include "Class.h"
#include "ScriptEngine.h"
#include "Utils.h"
#include "HelperMacros.h"
#include "../State.h"
#include "base/memory/Memory.h"
#include "base/std/container/unordered_set.h"

namespace se {

namespace {
ccstd::vector<Class *> __allClasses; // NOLINT
ccstd::vector<NativeFunctionPtr> __nativeFunctions; // NOLINT
ccstd::unordered_set<JSClassID> __registeredClassIDs; // NOLINT

int registerNativeFunction(NativeFunctionPtr func) {
    CC_ASSERT(func != nullptr);
    __nativeFunctions.push_back(func);
    return static_cast<int>(__nativeFunctions.size() - 1);
}

NativeFunctionPtr getRegisteredNativeFunction(int id) {
    if (id < 0 || id >= static_cast<int>(__nativeFunctions.size())) {
        return nullptr;
    }
    return __nativeFunctions[id];
}

// GC finalizer — releases the persistent se::Object stored in the JS
// object's opaque field.  Called by QuickJS when the JS object's reference
// count reaches zero (e.g. after the C++ side releases the DupValue).
void qjsFinalizer(JSRuntime * /*rt*/, JSValue val) {
    JSClassID classId = JS_GetClassID(val);
    if (classId == 0) return;
    auto *seObj = static_cast<se::Object *>(JS_GetOpaque(val, classId));
    if (!seObj) return;
    // The JS object is being collected — detach the JSValue first so the
    // se::Object destructor won't try to JS_FreeValue during a GC cycle.
    seObj->_detachJSValue();
    seObj->clearPrivateData(true);
    seObj->decRef();
}

JSValue qjsCallNative(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, int /*magic*/, JSValueConst *funcData) {
    int funcId = -1;
    JS_ToInt32(ctx, &funcId, funcData[0]);
    auto *funcPtr = getRegisteredNativeFunction(funcId);
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
        // Borrow native pointer from the persistent se::Object stored in opaque
        JSClassID thisClassId = JS_GetClassID(thisVal);
        if (Class::isRegisteredClassID(thisClassId)) {
            auto *existing = static_cast<se::Object *>(JS_GetOpaque(thisVal, thisClassId));
            if (existing && existing->getPrivateData()) {
                thisObj->_borrowPrivateData(existing->getPrivateData());
            }
        }
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

Class *findClassByMagic(int magic) {
    const auto classId = static_cast<JSClassID>(magic);
    for (auto *cls : __allClasses) {
        if (cls != nullptr && cls->getClassID() == classId) {
            return cls;
        }
    }
    return nullptr;
}

JSValue createQJSFunction(JSContext *ctx, NativeFunctionPtr func) {
    const int funcId = registerNativeFunction(func);
    JSValue funcDataVal = JS_NewInt32(ctx, funcId);
    return JS_NewCFunctionData(ctx, qjsCallNative, 0, 0, 1, &funcDataVal);
}

JSValue qjsCallConstructor(JSContext *ctx, JSValueConst newTarget, int argc, JSValueConst *argv, int magic) {
    Class *cls = findClassByMagic(magic);
    NativeFunctionPtr constructor = cls != nullptr ? cls->getConstructor() : nullptr;
    if (constructor == nullptr) {
        return JS_ThrowInternalError(ctx, "Native constructor metadata is missing");
    }

    // When called WITHOUT `new` (e.g. via Parent.apply(this, args) from a
    // _ctor chain), newTarget is the function itself but `this` (argv[-1]
    // in QuickJS) is the existing object.  We detect this by checking if
    // newTarget has a "prototype" — when called as a regular function via
    // JS_CFUNC_constructor_or_func_magic, newTarget is `undefined`.
    bool calledWithNew = !JS_IsUndefined(newTarget);

    JSValue obj;
    if (calledWithNew) {
        JSValue proto = JS_GetPropertyStr(ctx, newTarget, "prototype");
        if (JS_IsException(proto)) {
            return JS_EXCEPTION;
        }
        obj = JS_NewObjectProtoClass(ctx, proto, cls->getClassID());
        JS_FreeValue(ctx, proto);
        if (JS_IsException(obj)) {
            return JS_EXCEPTION;
        }
    } else {
        // Called as regular function — `this` is the first hidden arg.
        // The object already exists; just run the native constructor on it.
        // Return undefined (the caller doesn't use the return value).
        obj = JS_UNDEFINED;
    }

    se::ValueArray seArgs;
    seArgs.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        se::Value v;
        internal::jsToSeValue(ctx, argv[i], &v);
        seArgs.push_back(std::move(v));
    }

    // When called without new, we don't create a new object or run the
    // native C++ constructor again — _ctor only needs JS-side init.
    // Return undefined to let the _ctor chain continue.
    if (!calledWithNew) {
        return JS_UNDEFINED;
    }

    se::Object *thisObj = Object::_createJSObject(cls, JS_DupValue(ctx, obj));
    se::State state(thisObj, seArgs);
    const bool ok = constructor(state);

    if (!ok) {
        thisObj->decRef();
        JS_FreeValue(ctx, obj);
        return JS_ThrowInternalError(ctx, "Native constructor failed");
    }

    const auto &retVal = state.rval();
    if (retVal.isObject()) {
        thisObj->decRef();
        JS_FreeValue(ctx, obj);
        return internal::seToJsValue(ctx, retVal);
    }

    // Call _ctor if defined on the prototype — the Cocos Creator engine
    // uses this pattern to initialise JS-side properties (e.g. _nativeData
    // on ImageAsset) after the C++ constructor has run.  The V8 backend
    // does this in HelperMacros.cpp; QuickJS needs it here.
    //
    // Always look up _ctor fresh (don't cache) because the CCClass
    // decorator system may construct objects BEFORE cc.js defines _ctor
    // on the prototype, which would poison the cache.
    {
        JSValue ctorFn = JS_GetPropertyStr(ctx, obj, "_ctor");
        if (JS_IsFunction(ctx, ctorFn)) {
            JSValue ctorRet = JS_Call(ctx, ctorFn, obj, argc, const_cast<JSValueConst *>(argv));
            if (JS_IsException(ctorRet)) {
                JSValue exc = JS_GetException(ctx);
                const char *msg = JS_ToCString(ctx, exc);
                CC_LOG_WARNING("qjsCallConstructor[%s]: _ctor failed: %s", cls->getName(), msg ? msg : "?");
                if (msg) JS_FreeCString(ctx, msg);
                JS_FreeValue(ctx, exc);
            }
            JS_FreeValue(ctx, ctorRet);
        }
        JS_FreeValue(ctx, ctorFn);
    }

    // Persist the se::Object in the JS object's opaque field so that
    // native function arguments can recover the C++ private data later.
    // Without this, jsToSeValue would create fresh wrappers that lack
    // the native pointer set by the constructor above.
    JS_SetOpaque(obj, thisObj);

    // Keep the DupValue'd JSValue alive in the se::Object.  This prevents
    // QuickJS GC from collecting the JS object, but that is intentional:
    // engine-managed objects (Node, Material, etc.) have their lifecycle
    // controlled by the C++ side.  The DupValue also allows
    // nativevalue_to_se / property getters to return a valid JSValue.
    //
    // Do NOT decRef — the opaque now owns thisObj (refCount stays 1).
    // Cleanup happens when the C++ engine destroys the native object.

    return obj;
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

bool Class::isRegisteredClassID(JSClassID classId) {
    return __registeredClassIDs.count(classId) != 0;
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
        __registeredClassIDs.clear();
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
    classDef.finalizer = qjsFinalizer;

    JS_NewClass(rt, _classId, &classDef);
    __registeredClassIDs.insert(_classId);

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
        ctorFunc = JS_NewCFunctionMagic(ctx, qjsCallConstructor, _name.c_str(), 0, JS_CFUNC_constructor_or_func_magic, static_cast<int>(_classId));
        JS_SetConstructor(ctx, ctorFunc, protoVal);
    } else {
        ctorFunc = JS_NewObject(ctx);
        // cc.js expects jsb.ClassName.prototype even for abstract classes
        // (those without a JS-callable constructor).  Plain objects don't
        // have .prototype, so set it explicitly.
        JS_SetPropertyStr(ctx, ctorFunc, "prototype", JS_DupValue(ctx, protoVal));
        // Manual bindings use proto.constructor to locate the class object
        // and define static methods (e.g. getInstance) on it.
        JS_SetPropertyStr(ctx, protoVal, "constructor", JS_DupValue(ctx, ctorFunc));
    }

    for (const auto &sf : _staticFuncs) {
        JSValue fn = createQJSFunction(ctx, sf.func);
        JS_SetPropertyStr(ctx, ctorFunc, sf.name.c_str(), fn);
    }

    for (const auto &sv : _staticValues) {
        JSValue val = internal::seToJsValue(ctx, sv.second);
        JS_SetPropertyStr(ctx, ctorFunc, sv.first.c_str(), val);
    }

    for (const auto &sp : _staticProps) {
        JSAtom atom = JS_NewAtom(ctx, sp.name.c_str());
        JSValue getter = JS_UNDEFINED;
        JSValue setter = JS_UNDEFINED;
        if (sp.getter) getter = createQJSFunction(ctx, sp.getter);
        if (sp.setter) setter = createQJSFunction(ctx, sp.setter);

        int flags = JS_PROP_HAS_CONFIGURABLE | JS_PROP_CONFIGURABLE;
        if (sp.getter) flags |= JS_PROP_HAS_GET;
        if (sp.setter) flags |= JS_PROP_HAS_SET;

        JS_DefineProperty(ctx, ctorFunc, atom, JS_UNDEFINED, getter, setter, flags);

        if (sp.getter) JS_FreeValue(ctx, getter);
        if (sp.setter) JS_FreeValue(ctx, setter);
        JS_FreeAtom(ctx, atom);
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
