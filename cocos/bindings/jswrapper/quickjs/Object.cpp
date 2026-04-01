#include "Object.h"
#include "Class.h"
#include "ScriptEngine.h"
#include "Utils.h"
#include "HelperMacros.h"
#include "../State.h"
#include "../MappingUtils.h"
#include "base/memory/Memory.h"

namespace se {

namespace {
ccstd::vector<NativeFunctionPtr> __objectNativeFunctions; // NOLINT

int registerObjectNativeFunction(NativeFunctionPtr func) {
    CC_ASSERT(func != nullptr);
    __objectNativeFunctions.push_back(func);
    return static_cast<int>(__objectNativeFunctions.size() - 1);
}

NativeFunctionPtr getObjectNativeFunction(int id) {
    if (id < 0 || id >= static_cast<int>(__objectNativeFunctions.size())) {
        return nullptr;
    }
    return __objectNativeFunctions[id];
}
} // namespace

Object::Object() = default;

Object::~Object() {
    clearPrivateData(true);
    auto *ctx = ScriptEngine::getInstance() ? ScriptEngine::getInstance()->getContext() : nullptr;
    if (ctx && !JS_IsUndefined(_jsVal)) {
        JS_FreeValue(ctx, _jsVal);
    }
    _jsVal = JS_UNDEFINED;
}

Object *Object::createPlainObject() {
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue obj = JS_NewObject(ctx);
    return _createJSObject(nullptr, obj);
}

Object *Object::createMapObject() {
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue globalObj = JS_GetGlobalObject(ctx);
    JSValue mapCtor = JS_GetPropertyStr(ctx, globalObj, "Map");
    JSValue mapObj = JS_CallConstructor(ctx, mapCtor, 0, nullptr);
    JS_FreeValue(ctx, mapCtor);
    JS_FreeValue(ctx, globalObj);
    auto *obj = _createJSObject(nullptr, mapObj);
    return obj;
}

Object *Object::createSetObject() {
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue globalObj = JS_GetGlobalObject(ctx);
    JSValue setCtor = JS_GetPropertyStr(ctx, globalObj, "Set");
    JSValue setObj = JS_CallConstructor(ctx, setCtor, 0, nullptr);
    JS_FreeValue(ctx, setCtor);
    JS_FreeValue(ctx, globalObj);
    return _createJSObject(nullptr, setObj);
}

Object *Object::createArrayObject(size_t length) {
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue arr = JS_NewArray(ctx);
    if (length > 0) {
        JS_SetPropertyStr(ctx, arr, "length", JS_NewInt64(ctx, static_cast<int64_t>(length)));
    }
    return _createJSObject(nullptr, arr);
}

Object *Object::createPromise() {
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue resolvingFuncs[2];
    JSValue promise = JS_NewPromiseCapability(ctx, resolvingFuncs);
    JS_FreeValue(ctx, resolvingFuncs[0]);
    JS_FreeValue(ctx, resolvingFuncs[1]);
    return _createJSObject(nullptr, promise);
}

void Object::rejectPromise(Object *object, const Value &value) {
    if (!object) return;
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue resolvingFuncs[2];
    JSValue promise = JS_NewPromiseCapability(ctx, resolvingFuncs);
    JSValue val = internal::seToJsValue(ctx, value);
    JSValue ret = JS_Call(ctx, resolvingFuncs[1], JS_UNDEFINED, 1, &val);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, val);
    JS_FreeValue(ctx, resolvingFuncs[0]);
    JS_FreeValue(ctx, resolvingFuncs[1]);
    JS_FreeValue(ctx, promise);
}

void Object::resolverPromise(Object *object, const Value &value) {
    if (!object) return;
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue resolvingFuncs[2];
    JSValue promise = JS_NewPromiseCapability(ctx, resolvingFuncs);
    JSValue val = internal::seToJsValue(ctx, value);
    JSValue ret = JS_Call(ctx, resolvingFuncs[0], JS_UNDEFINED, 1, &val);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, val);
    JS_FreeValue(ctx, resolvingFuncs[0]);
    JS_FreeValue(ctx, resolvingFuncs[1]);
    JS_FreeValue(ctx, promise);
}

Object *Object::createUint8TypedArray(uint8_t *bytes, size_t byteLength) {
    return createTypedArray(TypedArrayType::UINT8, bytes, byteLength);
}

Object *Object::createTypedArray(TypedArrayType type, const void *data, size_t byteLength) {
    auto *ctx = ScriptEngine::getInstance()->getContext();

    JSValue ab = JS_NewArrayBufferCopy(ctx, reinterpret_cast<const uint8_t *>(data), byteLength);
    JSValue globalObj = JS_GetGlobalObject(ctx);

    const char *ctorName = "Uint8Array";
    int elemSize = 1;
    switch (type) {
        case TypedArrayType::INT8:      ctorName = "Int8Array"; elemSize = 1; break;
        case TypedArrayType::UINT8:     ctorName = "Uint8Array"; elemSize = 1; break;
        case TypedArrayType::UINT8_CLAMPED: ctorName = "Uint8ClampedArray"; elemSize = 1; break;
        case TypedArrayType::INT16:     ctorName = "Int16Array"; elemSize = 2; break;
        case TypedArrayType::UINT16:    ctorName = "Uint16Array"; elemSize = 2; break;
        case TypedArrayType::INT32:     ctorName = "Int32Array"; elemSize = 4; break;
        case TypedArrayType::UINT32:    ctorName = "Uint32Array"; elemSize = 4; break;
        case TypedArrayType::FLOAT32:   ctorName = "Float32Array"; elemSize = 4; break;
        case TypedArrayType::FLOAT64:   ctorName = "Float64Array"; elemSize = 8; break;
        default: break;
    }
    CC_UNUSED_PARAM(elemSize);

    JSValue ctor = JS_GetPropertyStr(ctx, globalObj, ctorName);
    JSValue args[1] = { ab };
    JSValue ta = JS_CallConstructor(ctx, ctor, 1, args);
    JS_FreeValue(ctx, ctor);
    JS_FreeValue(ctx, ab);
    JS_FreeValue(ctx, globalObj);

    return _createJSObject(nullptr, ta);
}

Object *Object::createTypedArrayWithBuffer(TypedArrayType type, const Object *obj) {
    return createTypedArrayWithBuffer(type, obj, 0, 0);
}

Object *Object::createTypedArrayWithBuffer(TypedArrayType type, const Object *obj, size_t offset) {
    return createTypedArrayWithBuffer(type, obj, offset, 0);
}

Object *Object::createTypedArrayWithBuffer(TypedArrayType type, const Object *obj, size_t offset, size_t byteLength) {
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue globalObj = JS_GetGlobalObject(ctx);

    const char *ctorName = "Uint8Array";
    switch (type) {
        case TypedArrayType::INT8:      ctorName = "Int8Array"; break;
        case TypedArrayType::UINT8:     ctorName = "Uint8Array"; break;
        case TypedArrayType::UINT8_CLAMPED: ctorName = "Uint8ClampedArray"; break;
        case TypedArrayType::INT16:     ctorName = "Int16Array"; break;
        case TypedArrayType::UINT16:    ctorName = "Uint16Array"; break;
        case TypedArrayType::INT32:     ctorName = "Int32Array"; break;
        case TypedArrayType::UINT32:    ctorName = "Uint32Array"; break;
        case TypedArrayType::FLOAT32:   ctorName = "Float32Array"; break;
        case TypedArrayType::FLOAT64:   ctorName = "Float64Array"; break;
        default: break;
    }

    JSValue ctor = JS_GetPropertyStr(ctx, globalObj, ctorName);
    int argc = 1;
    JSValue args[3];
    args[0] = JS_DupValue(ctx, obj->_getJSObject());

    if (byteLength > 0) {
        args[1] = JS_NewInt64(ctx, static_cast<int64_t>(offset));
        args[2] = JS_NewInt64(ctx, static_cast<int64_t>(byteLength));
        argc = 3;
    } else if (offset > 0) {
        args[1] = JS_NewInt64(ctx, static_cast<int64_t>(offset));
        argc = 2;
    }

    JSValue ta = JS_CallConstructor(ctx, ctor, argc, args);
    for (int i = 0; i < argc; ++i) JS_FreeValue(ctx, args[i]);
    JS_FreeValue(ctx, ctor);
    JS_FreeValue(ctx, globalObj);

    return _createJSObject(nullptr, ta);
}

Object *Object::createArrayBufferObject(const void *data, size_t byteLength) {
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue ab = JS_NewArrayBufferCopy(ctx, reinterpret_cast<const uint8_t *>(data), byteLength);
    return _createJSObject(nullptr, ab);
}

Object *Object::createExternalArrayBufferObject(void *contents, size_t byteLength, BufferContentsFreeFunc freeFunc, void *freeUserData) {
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue ab = JS_NewArrayBuffer(ctx, reinterpret_cast<uint8_t *>(contents), byteLength,
        [](JSRuntime *rt, void *opaque, void *ptr) {
            CC_UNUSED_PARAM(rt);
            CC_UNUSED_PARAM(opaque);
            CC_UNUSED_PARAM(ptr);
        }, freeUserData, false);
    return _createJSObject(nullptr, ab);
}

Object *Object::createJSONObject(const ccstd::string &jsonStr) {
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue val = JS_ParseJSON(ctx, jsonStr.c_str(), jsonStr.size(), "<json>");
    if (JS_IsException(val)) {
        JS_FreeValue(ctx, val);
        return nullptr;
    }
    return _createJSObject(nullptr, val);
}

Object *Object::createJSONObject(std::u16string && /*jsonStr*/) {
    return nullptr;
}

Object *Object::createObjectWithClass(Class *cls) {
    return Class::_createJSObjectWithClass(cls);
}

Object *Object::createObjectWithConstructor(Object *constructor) {
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue ret = JS_CallConstructor(ctx, constructor->_getJSObject(), 0, nullptr);
    if (JS_IsException(ret)) {
        JS_FreeValue(ctx, ret);
        return nullptr;
    }
    return _createJSObject(nullptr, ret);
}

Object *Object::createObjectWithConstructor(Object *constructor, const ValueArray &args) {
    auto *ctx = ScriptEngine::getInstance()->getContext();
    int argc = static_cast<int>(args.size());
    auto *jsArgs = reinterpret_cast<JSValue *>(alloca(sizeof(JSValue) * (argc > 0 ? argc : 1)));
    for (int i = 0; i < argc; ++i) {
        jsArgs[i] = internal::seToJsValue(ctx, args[i]);
    }
    JSValue ret = JS_CallConstructor(ctx, constructor->_getJSObject(), argc, jsArgs);
    for (int i = 0; i < argc; ++i) {
        JS_FreeValue(ctx, jsArgs[i]);
    }
    if (JS_IsException(ret)) {
        JS_FreeValue(ctx, ret);
        return nullptr;
    }
    return _createJSObject(nullptr, ret);
}

Object *Object::createProxyTarget(Object *proxy) {
    return proxy;
}

Object *Object::getObjectWithPtr(void *ptr) {
    Object *obj = nullptr;
    NativePtrToObjectMap::forEach(ptr, [&obj](se::Object *o) {
        obj = o;
    });
    return obj;
}

bool Object::getProperty(const char *name, Value *data, bool /*cachePropertyName*/) {
    if (data == nullptr || name == nullptr) return false;
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue val = JS_GetPropertyStr(ctx, _jsVal, name);
    if (JS_IsUndefined(val)) {
        *data = Value::Undefined;
        JS_FreeValue(ctx, val);
        return false;
    }
    internal::jsToSeValue(ctx, val, data);
    JS_FreeValue(ctx, val);
    return true;
}

bool Object::setProperty(const char *name, const Value &data) {
    if (name == nullptr) return false;
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue val = internal::seToJsValue(ctx, data);
    JS_SetPropertyStr(ctx, _jsVal, name, val);
    return true;
}

bool Object::deleteProperty(const char *name) {
    if (name == nullptr) return false;
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSAtom atom = JS_NewAtom(ctx, name);
    int ret = JS_DeleteProperty(ctx, _jsVal, atom, 0);
    JS_FreeAtom(ctx, atom);
    return ret >= 0;
}

bool Object::defineOwnProperty(const char *name, const Value &value, bool /*writable*/, bool /*enumerable*/, bool /*configurable*/) {
    return setProperty(name, value);
}

bool Object::defineProperty(const char *name, NativeFunctionPtr getter, NativeFunctionPtr setter) {
    if (name == nullptr) return false;
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSAtom atom = JS_NewAtom(ctx, name);

    JSValue getterVal = JS_UNDEFINED;
    JSValue setterVal = JS_UNDEFINED;
    int flags = JS_PROP_HAS_CONFIGURABLE | JS_PROP_CONFIGURABLE;

    auto wrapNative = [](JSContext *cx, JSValueConst thisVal, int argc, JSValueConst *argv, int magic, JSValueConst *funcData) -> JSValue {
        int funcId = -1;
        JS_ToInt32(cx, &funcId, funcData[0]);
        auto *fn = getObjectNativeFunction(funcId);
        if (!fn) return JS_UNDEFINED;
        se::ValueArray seArgs;
        seArgs.reserve(argc);
        for (int i = 0; i < argc; ++i) {
            se::Value v;
            internal::jsToSeValue(cx, argv[i], &v);
            seArgs.push_back(std::move(v));
        }
        se::Object *thisObj = nullptr;
        if (!JS_IsUndefined(thisVal) && !JS_IsNull(thisVal)) {
            thisObj = Object::_createJSObject(nullptr, JS_DupValue(cx, thisVal));
        }
        se::State state(thisObj, seArgs);
        bool ok = fn(state);
        if (thisObj) thisObj->decRef();
        if (!ok) return JS_UNDEFINED;
        const auto &retVal = state.rval();
        if (retVal.isUndefined()) return JS_UNDEFINED;
        return internal::seToJsValue(cx, retVal);
    };

    if (getter) {
        JSValue funcDataVal = JS_NewInt32(ctx, registerObjectNativeFunction(getter));
        getterVal = JS_NewCFunctionData(ctx, wrapNative, 0, 0, 1, &funcDataVal);
        flags |= JS_PROP_HAS_GET;
    }
    if (setter) {
        JSValue funcDataVal = JS_NewInt32(ctx, registerObjectNativeFunction(setter));
        setterVal = JS_NewCFunctionData(ctx, wrapNative, 1, 0, 1, &funcDataVal);
        flags |= JS_PROP_HAS_SET;
    }

    JS_DefineProperty(ctx, _jsVal, atom, JS_UNDEFINED, getterVal, setterVal, flags);
    if (getter) JS_FreeValue(ctx, getterVal);
    if (setter) JS_FreeValue(ctx, setterVal);
    JS_FreeAtom(ctx, atom);
    return true;
}

bool Object::defineFunction(const char *funcName, NativeFunctionPtr func) {
    if (funcName == nullptr || func == nullptr) return false;
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue funcDataVal = JS_NewInt32(ctx, registerObjectNativeFunction(func));

    auto wrapper = [](JSContext *cx, JSValueConst thisVal, int argc, JSValueConst *argv, int magic, JSValueConst *funcData) -> JSValue {
        int funcId = -1;
        JS_ToInt32(cx, &funcId, funcData[0]);
        auto *fn = getObjectNativeFunction(funcId);
        if (!fn) return JS_UNDEFINED;

        se::ValueArray seArgs;
        seArgs.reserve(argc);
        for (int i = 0; i < argc; ++i) {
            se::Value v;
            internal::jsToSeValue(cx, argv[i], &v);
            seArgs.push_back(std::move(v));
        }

        se::Object *thisObj = nullptr;
        if (!JS_IsUndefined(thisVal) && !JS_IsNull(thisVal)) {
            thisObj = Object::_createJSObject(nullptr, JS_DupValue(cx, thisVal));
        }
        se::State state(thisObj, seArgs);
        bool ok = fn(state);
        if (thisObj) thisObj->decRef();
        if (!ok) return JS_ThrowInternalError(cx, "Native function failed");
        const auto &retVal = state.rval();
        if (retVal.isUndefined()) return JS_UNDEFINED;
        return internal::seToJsValue(cx, retVal);
    };

    JSValue fn = JS_NewCFunctionData(ctx, wrapper, 0, 0, 1, &funcDataVal);
    JS_SetPropertyStr(ctx, _jsVal, funcName, fn);
    return true;
}

bool Object::isFunction() const {
    auto *ctx = ScriptEngine::getInstance()->getContext();
    return JS_IsFunction(ctx, _jsVal) != 0;
}

bool Object::call(const ValueArray &args, Object *thisObject, Value *rval) {
    auto *ctx = ScriptEngine::getInstance()->getContext();
    int argc = static_cast<int>(args.size());
    auto *jsArgs = reinterpret_cast<JSValue *>(alloca(sizeof(JSValue) * (argc > 0 ? argc : 1)));
    for (int i = 0; i < argc; ++i) {
        jsArgs[i] = internal::seToJsValue(ctx, args[i]);
    }
    JSValue thisVal = (thisObject != nullptr) ? thisObject->_getJSObject() : JS_UNDEFINED;
    JSValue ret = JS_Call(ctx, _jsVal, thisVal, argc, jsArgs);
    for (int i = 0; i < argc; ++i) {
        JS_FreeValue(ctx, jsArgs[i]);
    }
    bool ok = !JS_IsException(ret);
    if (ok && rval != nullptr) {
        internal::jsToSeValue(ctx, ret, rval);
    }
    JS_FreeValue(ctx, ret);
    return ok;
}

bool Object::isMap() const {
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue globalObj = JS_GetGlobalObject(ctx);
    JSValue mapCtor = JS_GetPropertyStr(ctx, globalObj, "Map");
    int r = JS_IsInstanceOf(ctx, _jsVal, mapCtor);
    JS_FreeValue(ctx, mapCtor);
    JS_FreeValue(ctx, globalObj);
    return r > 0;
}

bool Object::isWeakMap() const {
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue globalObj = JS_GetGlobalObject(ctx);
    JSValue ctor = JS_GetPropertyStr(ctx, globalObj, "WeakMap");
    int r = JS_IsInstanceOf(ctx, _jsVal, ctor);
    JS_FreeValue(ctx, ctor);
    JS_FreeValue(ctx, globalObj);
    return r > 0;
}

bool Object::isSet() const {
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue globalObj = JS_GetGlobalObject(ctx);
    JSValue ctor = JS_GetPropertyStr(ctx, globalObj, "Set");
    int r = JS_IsInstanceOf(ctx, _jsVal, ctor);
    JS_FreeValue(ctx, ctor);
    JS_FreeValue(ctx, globalObj);
    return r > 0;
}

bool Object::isWeakSet() const {
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue globalObj = JS_GetGlobalObject(ctx);
    JSValue ctor = JS_GetPropertyStr(ctx, globalObj, "WeakSet");
    int r = JS_IsInstanceOf(ctx, _jsVal, ctor);
    JS_FreeValue(ctx, ctor);
    JS_FreeValue(ctx, globalObj);
    return r > 0;
}

bool Object::isArray() const {
    return JS_IsArray(_jsVal) != 0;
}

bool Object::getArrayLength(uint32_t *length) const {
    if (length == nullptr) return false;
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue lenVal = JS_GetPropertyStr(ctx, _jsVal, "length");
    int64_t len = 0;
    JS_ToInt64(ctx, &len, lenVal);
    JS_FreeValue(ctx, lenVal);
    *length = static_cast<uint32_t>(len);
    return true;
}

bool Object::getArrayElement(uint32_t index, Value *data) const {
    if (data == nullptr) return false;
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue val = JS_GetPropertyUint32(ctx, _jsVal, index);
    internal::jsToSeValue(ctx, val, data);
    JS_FreeValue(ctx, val);
    return true;
}

bool Object::setArrayElement(uint32_t index, const Value &data) {
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue val = internal::seToJsValue(ctx, data);
    JS_SetPropertyUint32(ctx, _jsVal, index, val);
    return true;
}

bool Object::isTypedArray() const {
    auto *ctx = ScriptEngine::getInstance()->getContext();
    size_t byteOffset = 0, byteLen = 0, elemSize = 0;
    JSValue ab = JS_GetTypedArrayBuffer(ctx, _jsVal, &byteOffset, &byteLen, &elemSize);
    if (JS_IsException(ab)) {
        JS_FreeValue(ctx, ab);
        return false;
    }
    JS_FreeValue(ctx, ab);
    return true;
}

bool Object::isProxy() const { return false; }

Object::TypedArrayType Object::getTypedArrayType() const {
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue globalObj = JS_GetGlobalObject(ctx);

    static const struct { const char *name; TypedArrayType type; } checks[] = {
        {"Int8Array",         TypedArrayType::INT8},
        {"Uint8Array",        TypedArrayType::UINT8},
        {"Uint8ClampedArray", TypedArrayType::UINT8_CLAMPED},
        {"Int16Array",        TypedArrayType::INT16},
        {"Uint16Array",       TypedArrayType::UINT16},
        {"Int32Array",        TypedArrayType::INT32},
        {"Uint32Array",       TypedArrayType::UINT32},
        {"Float32Array",      TypedArrayType::FLOAT32},
        {"Float64Array",      TypedArrayType::FLOAT64},
        {"BigInt64Array",     TypedArrayType::BIGINT64},
        {"BigUint64Array",    TypedArrayType::BIGUINT64},
    };

    TypedArrayType result = TypedArrayType::NONE;
    for (const auto &c : checks) {
        JSValue ctor = JS_GetPropertyStr(ctx, globalObj, c.name);
        if (JS_IsInstanceOf(ctx, _jsVal, ctor) > 0) {
            result = c.type;
            JS_FreeValue(ctx, ctor);
            break;
        }
        JS_FreeValue(ctx, ctor);
    }
    JS_FreeValue(ctx, globalObj);
    return result;
}

bool Object::getTypedArrayData(uint8_t **ptr, size_t *length) const {
    if (!ptr || !length) return false;
    auto *ctx = ScriptEngine::getInstance()->getContext();
    size_t byteOffset = 0, byteLen = 0, elemSize = 0;
    JSValue ab = JS_GetTypedArrayBuffer(ctx, _jsVal, &byteOffset, &byteLen, &elemSize);
    if (JS_IsException(ab)) {
        JS_FreeValue(ctx, ab);
        return false;
    }
    size_t abLen = 0;
    uint8_t *abPtr = JS_GetArrayBuffer(ctx, &abLen, ab);
    JS_FreeValue(ctx, ab);
    if (abPtr) {
        *ptr = abPtr + byteOffset;
        *length = byteLen;
        return true;
    }
    return false;
}

bool Object::isArrayBuffer() const {
    return JS_IsArrayBuffer(_jsVal);
}

bool Object::getArrayBufferData(uint8_t **ptr, size_t *length) const {
    if (!ptr || !length) return false;
    auto *ctx = ScriptEngine::getInstance()->getContext();
    size_t len = 0;
    uint8_t *p = JS_GetArrayBuffer(ctx, &len, _jsVal);
    if (p) {
        *ptr = p;
        *length = len;
        return true;
    }
    return false;
}

bool Object::getAllKeys(ccstd::vector<ccstd::string> *allKeys) const {
    if (allKeys == nullptr) return false;
    auto *ctx = ScriptEngine::getInstance()->getContext();
    allKeys->clear();

    JSPropertyEnum *props = nullptr;
    uint32_t count = 0;
    if (JS_GetOwnPropertyNames(ctx, &props, &count, _jsVal, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) == 0) {
        for (uint32_t i = 0; i < count; ++i) {
            const char *name = JS_AtomToCString(ctx, props[i].atom);
            if (name) {
                allKeys->push_back(name);
                JS_FreeCString(ctx, name);
            }
        }
        js_free(ctx, props);
    }
    return true;
}

void Object::clearMap() {
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue clearFunc = JS_GetPropertyStr(ctx, _jsVal, "clear");
    if (JS_IsFunction(ctx, clearFunc)) {
        JSValue ret = JS_Call(ctx, clearFunc, _jsVal, 0, nullptr);
        JS_FreeValue(ctx, ret);
    }
    JS_FreeValue(ctx, clearFunc);
}

bool Object::removeMapElement(const Value &key) {
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue deleteFunc = JS_GetPropertyStr(ctx, _jsVal, "delete");
    if (!JS_IsFunction(ctx, deleteFunc)) { JS_FreeValue(ctx, deleteFunc); return false; }
    JSValue jsKey = internal::seToJsValue(ctx, key);
    JSValue ret = JS_Call(ctx, deleteFunc, _jsVal, 1, &jsKey);
    bool ok = JS_ToBool(ctx, ret) != 0;
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, jsKey);
    JS_FreeValue(ctx, deleteFunc);
    return ok;
}

bool Object::getMapElement(const Value &key, Value *outValue) const {
    if (!outValue) return false;
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue getFunc = JS_GetPropertyStr(ctx, _jsVal, "get");
    if (!JS_IsFunction(ctx, getFunc)) { JS_FreeValue(ctx, getFunc); return false; }
    JSValue jsKey = internal::seToJsValue(ctx, key);
    JSValue ret = JS_Call(ctx, getFunc, _jsVal, 1, &jsKey);
    internal::jsToSeValue(ctx, ret, outValue);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, jsKey);
    JS_FreeValue(ctx, getFunc);
    return true;
}

bool Object::setMapElement(const Value &key, const Value &value) {
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue setFunc = JS_GetPropertyStr(ctx, _jsVal, "set");
    if (!JS_IsFunction(ctx, setFunc)) { JS_FreeValue(ctx, setFunc); return false; }
    JSValue args[2] = { internal::seToJsValue(ctx, key), internal::seToJsValue(ctx, value) };
    JSValue ret = JS_Call(ctx, setFunc, _jsVal, 2, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, args[0]);
    JS_FreeValue(ctx, args[1]);
    JS_FreeValue(ctx, setFunc);
    return true;
}

uint32_t Object::getMapSize() const {
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue sizeVal = JS_GetPropertyStr(ctx, _jsVal, "size");
    int64_t size = 0;
    JS_ToInt64(ctx, &size, sizeVal);
    JS_FreeValue(ctx, sizeVal);
    return static_cast<uint32_t>(size);
}

ccstd::vector<std::pair<Value, Value>> Object::getAllElementsInMap() const {
    ccstd::vector<std::pair<Value, Value>> result;
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue entriesFunc = JS_GetPropertyStr(ctx, _jsVal, "entries");
    if (!JS_IsFunction(ctx, entriesFunc)) { JS_FreeValue(ctx, entriesFunc); return result; }
    JSValue iter = JS_Call(ctx, entriesFunc, _jsVal, 0, nullptr);
    JSValue nextFunc = JS_GetPropertyStr(ctx, iter, "next");
    while (true) {
        JSValue nextResult = JS_Call(ctx, nextFunc, iter, 0, nullptr);
        JSValue done = JS_GetPropertyStr(ctx, nextResult, "done");
        if (JS_ToBool(ctx, done)) {
            JS_FreeValue(ctx, done);
            JS_FreeValue(ctx, nextResult);
            break;
        }
        JS_FreeValue(ctx, done);
        JSValue val = JS_GetPropertyStr(ctx, nextResult, "value");
        JSValue k = JS_GetPropertyUint32(ctx, val, 0);
        JSValue v = JS_GetPropertyUint32(ctx, val, 1);
        Value seKey, seVal;
        internal::jsToSeValue(ctx, k, &seKey);
        internal::jsToSeValue(ctx, v, &seVal);
        result.emplace_back(std::move(seKey), std::move(seVal));
        JS_FreeValue(ctx, k);
        JS_FreeValue(ctx, v);
        JS_FreeValue(ctx, val);
        JS_FreeValue(ctx, nextResult);
    }
    JS_FreeValue(ctx, nextFunc);
    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, entriesFunc);
    return result;
}

void Object::clearSet() {
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue clearFunc = JS_GetPropertyStr(ctx, _jsVal, "clear");
    if (JS_IsFunction(ctx, clearFunc)) {
        JSValue ret = JS_Call(ctx, clearFunc, _jsVal, 0, nullptr);
        JS_FreeValue(ctx, ret);
    }
    JS_FreeValue(ctx, clearFunc);
}

bool Object::removeSetElement(const Value &value) {
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue deleteFunc = JS_GetPropertyStr(ctx, _jsVal, "delete");
    if (!JS_IsFunction(ctx, deleteFunc)) { JS_FreeValue(ctx, deleteFunc); return false; }
    JSValue jsVal = internal::seToJsValue(ctx, value);
    JSValue ret = JS_Call(ctx, deleteFunc, _jsVal, 1, &jsVal);
    bool ok = JS_ToBool(ctx, ret) != 0;
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, jsVal);
    JS_FreeValue(ctx, deleteFunc);
    return ok;
}

bool Object::addSetElement(const Value &value) {
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue addFunc = JS_GetPropertyStr(ctx, _jsVal, "add");
    if (!JS_IsFunction(ctx, addFunc)) { JS_FreeValue(ctx, addFunc); return false; }
    JSValue jsVal = internal::seToJsValue(ctx, value);
    JSValue ret = JS_Call(ctx, addFunc, _jsVal, 1, &jsVal);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, jsVal);
    JS_FreeValue(ctx, addFunc);
    return true;
}

bool Object::isElementInSet(const Value &value) const {
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue hasFunc = JS_GetPropertyStr(ctx, _jsVal, "has");
    if (!JS_IsFunction(ctx, hasFunc)) { JS_FreeValue(ctx, hasFunc); return false; }
    JSValue jsVal = internal::seToJsValue(ctx, value);
    JSValue ret = JS_Call(ctx, hasFunc, _jsVal, 1, &jsVal);
    bool has = JS_ToBool(ctx, ret) != 0;
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, jsVal);
    JS_FreeValue(ctx, hasFunc);
    return has;
}

uint32_t Object::getSetSize() const {
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue sizeVal = JS_GetPropertyStr(ctx, _jsVal, "size");
    int64_t size = 0;
    JS_ToInt64(ctx, &size, sizeVal);
    JS_FreeValue(ctx, sizeVal);
    return static_cast<uint32_t>(size);
}

ValueArray Object::getAllElementsInSet() const {
    ValueArray result;
    auto *ctx = ScriptEngine::getInstance()->getContext();
    JSValue valuesFunc = JS_GetPropertyStr(ctx, _jsVal, "values");
    if (!JS_IsFunction(ctx, valuesFunc)) { JS_FreeValue(ctx, valuesFunc); return result; }
    JSValue iter = JS_Call(ctx, valuesFunc, _jsVal, 0, nullptr);
    JSValue nextFunc = JS_GetPropertyStr(ctx, iter, "next");
    while (true) {
        JSValue nextResult = JS_Call(ctx, nextFunc, iter, 0, nullptr);
        JSValue done = JS_GetPropertyStr(ctx, nextResult, "done");
        if (JS_ToBool(ctx, done)) {
            JS_FreeValue(ctx, done);
            JS_FreeValue(ctx, nextResult);
            break;
        }
        JS_FreeValue(ctx, done);
        JSValue val = JS_GetPropertyStr(ctx, nextResult, "value");
        Value seVal;
        internal::jsToSeValue(ctx, val, &seVal);
        result.push_back(std::move(seVal));
        JS_FreeValue(ctx, val);
        JS_FreeValue(ctx, nextResult);
    }
    JS_FreeValue(ctx, nextFunc);
    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, valuesFunc);
    return result;
}

void Object::setPrivateObject(PrivateObjectBase *data) {
    _privateObject = data;
    _privateData = data != nullptr ? data->getRaw() : nullptr;
    if (_privateData != nullptr) {
        NativePtrToObjectMap::emplace(_privateData, this);
    }
}

PrivateObjectBase *Object::getPrivateObject() const {
    return _privateObject;
}

void Object::clearPrivateData(bool clearMapping) {
    if (clearMapping && _privateData != nullptr) {
        NativePtrToObjectMap::erase(_privateData, this);
    }
    if (_privateObject != nullptr) {
        delete _privateObject;
        _privateObject = nullptr;
    }
    _privateData = nullptr;
}

void Object::root() {
    if (_rootCount == 0) {
        auto *ctx = ScriptEngine::getInstance() ? ScriptEngine::getInstance()->getContext() : nullptr;
        if (ctx && !JS_IsUndefined(_jsVal)) {
            _jsVal = JS_DupValue(ctx, _jsVal);
        }
    }
    ++_rootCount;
}

void Object::unroot() {
    if (_rootCount > 0) {
        --_rootCount;
        if (_rootCount == 0) {
            auto *ctx = ScriptEngine::getInstance() ? ScriptEngine::getInstance()->getContext() : nullptr;
            if (ctx && !JS_IsUndefined(_jsVal)) {
                JS_FreeValue(ctx, _jsVal);
            }
        }
    }
}

bool Object::isRooted() const {
    return _rootCount > 0;
}

bool Object::strictEquals(Object *o) const {
    if (o == nullptr) return false;
    if (this == o) return true;
    auto *ctx = ScriptEngine::getInstance()->getContext();
    return JS_IsStrictEqual(ctx, _jsVal, o->_jsVal) != 0;
}

bool Object::attachObject(Object *) { return true; }
bool Object::detachObject(Object *) { return true; }

ccstd::string Object::toString() const {
    auto *ctx = ScriptEngine::getInstance()->getContext();
    const char *str = JS_ToCString(ctx, _jsVal);
    ccstd::string result = str ? str : "[object QuickJSObject]";
    if (str) JS_FreeCString(ctx, str);
    return result;
}

ccstd::string Object::toStringExt() const {
    return toString();
}

Object *Object::_createJSObject(Class *cls, JSValue jsVal) {
    auto *o = ccnew Object();
    o->_jsVal = jsVal;
    o->_cls = cls;
    return o;
}

Class *Object::_getClass() const {
    return _cls;
}

void Object::_setFinalizeCallback(V8FinalizeFunc finalizeCb) {
    _finalizeCb = finalizeCb;
}

bool Object::_isNativeFunction() const {
    auto *ctx = ScriptEngine::getInstance()->getContext();
    return JS_IsFunction(ctx, _jsVal) != 0;
}

} // namespace se
