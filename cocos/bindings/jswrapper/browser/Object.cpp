#include "Object.h"
#include "Class.h"
#include "HelperMacros.h"
#include "ScriptEngine.h"
#include "Utils.h"
#include "../State.h"
#include "../MappingUtils.h"
#include "base/memory/Memory.h"
#include "base/UTF8.h"

#include <emscripten.h>
#include <emscripten/val.h>

#if SCRIPT_ENGINE_TYPE == SCRIPT_ENGINE_BROWSER

using emscripten::val;
using emscripten::EM_VAL;

namespace se {
// Global registry: funcId → NativeFunctionPtr (shared with Class.cpp via extern)
ccstd::vector<NativeFunctionPtr> g_nativeFunctions; // NOLINT

ccstd::vector<NativeFunctionPtr> &browser_getNativeFunctions() {
    return g_nativeFunctions;
}
} // namespace se

static int registerNativeFunction(se::NativeFunctionPtr func) {
    se::g_nativeFunctions.push_back(func);
    return static_cast<int>(se::g_nativeFunctions.size() - 1);
}

// ---- Native function trampoline ----
// This C++ function is called from browser JS when a native-registered
// function is invoked.  It receives the funcId, `this` object, and
// arguments array as emscripten::val handles.
//
// The JS side creates wrapper functions via EM_JS that call this.
extern "C" {

EMSCRIPTEN_KEEPALIVE
void browser_native_call(int funcId, EM_VAL thisHandle, EM_VAL argsHandle, EM_VAL retHandle) {
    if (funcId < 0 || funcId >= static_cast<int>(se::g_nativeFunctions.size())) {
        return;
    }

    auto thisVal = val::global("Emval").call<val>("toValue", static_cast<int>(reinterpret_cast<intptr_t>(thisHandle)));
    auto argsVal = val::global("Emval").call<val>("toValue", static_cast<int>(reinterpret_cast<intptr_t>(argsHandle)));

    // Actually, use take_ownership for proper handle management
    // emscripten::val::take_ownership is not available in all versions.
    // Use a different approach: pass via Module global.
    // This function is called from JS, so we can read args from Module globals.
    (void)thisHandle;
    (void)argsHandle;
    (void)retHandle;
}

} // extern "C"

// Simpler approach: use EM_JS + Module properties for argument passing
EM_JS(int, browser_make_native_func, (int funcId), {
    // Create a JS function that calls the C++ native function via Module globals
    var func = function() {
        // Store this and args in Module globals for C++ to read
        Module.__nativeCallThis = this;
        Module.__nativeCallArgs = Array.prototype.slice.call(arguments);
        Module.__nativeCallRet = undefined;
        Module.__nativeCallOk = false;
        // Call C++ trampoline
        Module._browser_do_native_call(funcId);
        return Module.__nativeCallRet;
    };
    return Emval.toHandle(func);
});

EM_JS(int, browser_make_native_ctor, (int funcId), {
    // Create a constructor function
    var ctor = function() {
        Module.__nativeCallThis = this;
        Module.__nativeCallArgs = Array.prototype.slice.call(arguments);
        Module.__nativeCallRet = undefined;
        Module.__nativeCallOk = false;
        Module._browser_do_native_call(funcId);
        // For constructors, also call _ctor if defined
        if (this._ctor) {
            this._ctor.apply(this, arguments);
        }
        return Module.__nativeCallRet !== undefined ? Module.__nativeCallRet : this;
    };
    return Emval.toHandle(ctor);
});

extern "C" {
EMSCRIPTEN_KEEPALIVE
void browser_do_native_call(int funcId) {
    if (funcId < 0 || funcId >= static_cast<int>(se::g_nativeFunctions.size())) {
        return;
    }

    // Read this and args from Module globals
    val module = val::global("Module");
    val thisJsVal = module["__nativeCallThis"];
    val argsJsVal = module["__nativeCallArgs"];

    // Convert to se types
    se::ValueArray seArgs;
    uint32_t argc = argsJsVal["length"].as<uint32_t>();
    seArgs.reserve(argc);
    for (uint32_t i = 0; i < argc; ++i) {
        se::Value v;
        se::internal::jsToSeValue(argsJsVal[i], &v);
        seArgs.push_back(std::move(v));
    }

    se::Object *thisObj = nullptr;
    if (!thisJsVal.isUndefined() && !thisJsVal.isNull()) {
        // Check if this JS object has a persistent se::Object (set by setPrivateObject)
        val ptrVal = thisJsVal["__cc_se_ptr"];
        if (!ptrVal.isUndefined() && ptrVal.isNumber()) {
            // Recover the persistent se::Object
            thisObj = reinterpret_cast<se::Object *>(static_cast<uintptr_t>(ptrVal.as<double>()));
            if (thisObj) thisObj->incRef();
        } else {
            // Create a temporary wrapper
            thisObj = se::Object::_createJSObject(nullptr, thisJsVal);
        }
    }

    se::State state(thisObj, seArgs);
    bool ok = se::g_nativeFunctions[funcId](state);
    // Note: do NOT manually decRef thisObj — State::~State() handles it
    // via SAFE_DEC_REF(_thisObject).

    if (ok) {
        const auto &rval = state.rval();
        if (!rval.isUndefined()) {
            module.set("__nativeCallRet", se::internal::seToJsValue(rval));
        }
    }
    module.set("__nativeCallOk", ok);
}
} // extern "C"

namespace se {

Object::Object() = default;

Object::~Object() {
    clearPrivateData(true);
    _jsVal = val::undefined();
}

Object *Object::_createJSObject(Class *cls, val jsVal) {
    auto *o = ccnew Object();
    o->_jsVal = jsVal;
    o->_cls = cls;
    return o;
}

// --- Factory methods ---

Object *Object::createPlainObject() {
    return _createJSObject(nullptr, val::object());
}

Object *Object::createMapObject() {
    return _createJSObject(nullptr, val::global("Map").new_());
}

Object *Object::createSetObject() {
    return _createJSObject(nullptr, val::global("Set").new_());
}

Object *Object::createArrayObject(size_t length) {
    return _createJSObject(nullptr, val::global("Array").new_(static_cast<unsigned>(length)));
}

Object *Object::createJSONObject(const ccstd::string &jsonStr) {
    val result = val::global("JSON").call<val>("parse", jsonStr);
    return _createJSObject(nullptr, result);
}

Object *Object::createJSONObject(std::u16string &&jsonStr) {
    ccstd::string utf8;
    if (!cc::StringUtils::UTF16ToUTF8(jsonStr, utf8) || utf8.empty()) {
        return nullptr;
    }
    return createJSONObject(utf8);
}

Object *Object::createObjectWithClass(Class *cls) {
    return Class::_createJSObjectWithClass(cls);
}

Object *Object::createObjectWithConstructor(Object *constructor) {
    val ret = constructor->_jsVal.new_();
    return _createJSObject(nullptr, ret);
}

Object *Object::createObjectWithConstructor(Object *constructor, const ValueArray &args) {
    // Build args array and use Reflect.construct
    val argsArray = val::array();
    for (const auto &a : args) {
        argsArray.call<void>("push", internal::seToJsValue(a));
    }
    val ret = val::global("Reflect").call<val>("construct", constructor->_jsVal, argsArray);
    return _createJSObject(nullptr, ret);
}

Object *Object::createProxyTarget(Object *proxy) { return proxy; }

Object *Object::getObjectWithPtr(void *ptr) {
    auto iter = NativePtrToObjectMap::find(ptr);
    if (iter != NativePtrToObjectMap::end()) {
        return iter->second;
    }
    return nullptr;
}

Object *Object::createPromise() {
    // TODO: implement with resolve/reject handles
    return _createJSObject(nullptr, val::global("Promise").call<val>("resolve"));
}

void Object::rejectPromise(Object * /*object*/, const Value & /*value*/) { /* TODO */ }
void Object::resolverPromise(Object * /*object*/, const Value & /*value*/) { /* TODO */ }

Object *Object::createUint8TypedArray(uint8_t *bytes, size_t byteLength) {
    return createTypedArray(TypedArrayType::UINT8, bytes, byteLength);
}

Object *Object::createTypedArray(TypedArrayType /*type*/, const void *data, size_t byteLength) {
    // Create an ArrayBuffer in WASM heap, then create a TypedArray view
    val buffer = val::global("ArrayBuffer").new_(static_cast<unsigned>(byteLength));
    val u8view = val::global("Uint8Array").new_(buffer);
    if (data && byteLength > 0) {
        // Copy data from WASM heap to JS
        val heapView = val::module_property("HEAPU8");
        val src = heapView.call<val>("subarray",
            reinterpret_cast<uintptr_t>(data),
            reinterpret_cast<uintptr_t>(data) + byteLength);
        u8view.call<void>("set", src);
    }
    return _createJSObject(nullptr, u8view);
}

Object *Object::createTypedArrayWithBuffer(TypedArrayType /*type*/, const Object *obj) {
    return _createJSObject(nullptr, val::global("Uint8Array").new_(obj->_jsVal));
}
Object *Object::createTypedArrayWithBuffer(TypedArrayType /*type*/, const Object *obj, size_t offset) {
    return _createJSObject(nullptr, val::global("Uint8Array").new_(obj->_jsVal, static_cast<unsigned>(offset)));
}
Object *Object::createTypedArrayWithBuffer(TypedArrayType /*type*/, const Object *obj, size_t offset, size_t byteLength) {
    return _createJSObject(nullptr, val::global("Uint8Array").new_(obj->_jsVal, static_cast<unsigned>(offset), static_cast<unsigned>(byteLength)));
}

Object *Object::createArrayBufferObject(const void *data, size_t byteLength) {
    val buffer = val::global("ArrayBuffer").new_(static_cast<unsigned>(byteLength));
    if (data && byteLength > 0) {
        val u8 = val::global("Uint8Array").new_(buffer);
        val heapView = val::module_property("HEAPU8");
        val src = heapView.call<val>("subarray",
            reinterpret_cast<uintptr_t>(data),
            reinterpret_cast<uintptr_t>(data) + byteLength);
        u8.call<void>("set", src);
    }
    return _createJSObject(nullptr, buffer);
}

Object *Object::createExternalArrayBufferObject(void * /*contents*/, size_t byteLength, BufferContentsFreeFunc /*freeFunc*/, void * /*freeUserData*/) {
    // Browser can't create external array buffers pointing to WASM heap directly.
    // Just create a regular one for now.
    return createArrayBufferObject(nullptr, byteLength);
}

// --- Property access ---

bool Object::getProperty(const char *name, Value *data, bool /*cachePropertyName*/) {
    val prop = _jsVal[name];
    if (prop.isUndefined()) {
        if (data) data->setUndefined();
        return false;
    }
    internal::jsToSeValue(prop, data);
    return true;
}

bool Object::setProperty(const char *name, const Value &data) {
    _jsVal.set(name, internal::seToJsValue(data));
    return true;
}

bool Object::deleteProperty(const char *name) {
    val::global("Reflect").call<bool>("deleteProperty", _jsVal, val(name));
    return true;
}

bool Object::defineOwnProperty(const char *name, const Value &value, bool writable, bool enumerable, bool configurable) {
    val desc = val::object();
    desc.set("value", internal::seToJsValue(value));
    desc.set("writable", writable);
    desc.set("enumerable", enumerable);
    desc.set("configurable", configurable);
    val::global("Object").call<void>("defineProperty", _jsVal, val(name), desc);
    return true;
}

bool Object::defineProperty(const char *name, NativeFunctionPtr getter, NativeFunctionPtr setter) {
    val desc = val::object();
    desc.set("configurable", true);
    if (getter) {
        int getterId = registerNativeFunction(getter);
        val getterFunc = val::take_ownership(static_cast<EM_VAL>(
            reinterpret_cast<void*>(static_cast<intptr_t>(browser_make_native_func(getterId)))));
        desc.set("get", getterFunc);
    }
    if (setter) {
        int setterId = registerNativeFunction(setter);
        val setterFunc = val::take_ownership(static_cast<EM_VAL>(
            reinterpret_cast<void*>(static_cast<intptr_t>(browser_make_native_func(setterId)))));
        desc.set("set", setterFunc);
    }
    val::global("Object").call<void>("defineProperty", _jsVal, val(name), desc);
    return true;
}

bool Object::defineFunction(const char *funcName, NativeFunctionPtr func) {
    int funcId = registerNativeFunction(func);
    val jsFunc = val::take_ownership(static_cast<EM_VAL>(
        reinterpret_cast<void*>(static_cast<intptr_t>(browser_make_native_func(funcId)))));
    _jsVal.set(funcName, jsFunc);
    return true;
}

// --- Function ---

bool Object::isFunction() const {
    return _jsVal.typeOf().as<std::string>() == "function";
}

bool Object::call(const ValueArray &args, Object *thisObject, Value *rval) {
    val argsArray = val::array();
    for (const auto &a : args) {
        argsArray.call<void>("push", internal::seToJsValue(a));
    }
    val thisVal = thisObject ? thisObject->_jsVal : val::undefined();
    val result = _jsVal.call<val>("apply", thisVal, argsArray);
    if (rval != nullptr) {
        internal::jsToSeValue(result, rval);
    }
    return true;
}

// --- Type checks ---

bool Object::isArray() const { return val::global("Array").call<bool>("isArray", _jsVal); }
bool Object::isMap() const { return _jsVal.instanceof(val::global("Map")); }
bool Object::isWeakMap() const { return _jsVal.instanceof(val::global("WeakMap")); }
bool Object::isSet() const { return _jsVal.instanceof(val::global("Set")); }
bool Object::isWeakSet() const { return _jsVal.instanceof(val::global("WeakSet")); }
bool Object::isProxy() const { return false; }
bool Object::isTypedArray() const { return val::global("ArrayBuffer").call<bool>("isView", _jsVal); }
bool Object::isArrayBuffer() const { return _jsVal.instanceof(val::global("ArrayBuffer")); }

Object::TypedArrayType Object::getTypedArrayType() const {
    if (_jsVal.instanceof(val::global("Float32Array"))) return TypedArrayType::FLOAT32;
    if (_jsVal.instanceof(val::global("Uint8Array"))) return TypedArrayType::UINT8;
    if (_jsVal.instanceof(val::global("Int32Array"))) return TypedArrayType::INT32;
    if (_jsVal.instanceof(val::global("Uint16Array"))) return TypedArrayType::UINT16;
    if (_jsVal.instanceof(val::global("Float64Array"))) return TypedArrayType::FLOAT64;
    return TypedArrayType::NONE;
}

bool Object::getTypedArrayData(uint8_t **ptr, size_t *length) const {
    if (!ptr || !length) return false;
    // Get byteLength and copy to WASM heap
    uint32_t byteLen = _jsVal["byteLength"].as<uint32_t>();
    // Allocate temp buffer in WASM heap
    static thread_local std::vector<uint8_t> tempBuf;
    tempBuf.resize(byteLen);
    // Copy from JS TypedArray to WASM
    val u8view = val::global("Uint8Array").new_(_jsVal["buffer"], _jsVal["byteOffset"], byteLen);
    val heapView = val::module_property("HEAPU8");
    heapView.call<void>("set", u8view, reinterpret_cast<uintptr_t>(tempBuf.data()));
    *ptr = tempBuf.data();
    *length = byteLen;
    return true;
}

bool Object::getArrayBufferData(uint8_t **ptr, size_t *length) const {
    if (!ptr || !length) return false;
    uint32_t byteLen = _jsVal["byteLength"].as<uint32_t>();
    static thread_local std::vector<uint8_t> tempBuf;
    tempBuf.resize(byteLen);
    val u8view = val::global("Uint8Array").new_(_jsVal);
    val heapView = val::module_property("HEAPU8");
    heapView.call<void>("set", u8view, reinterpret_cast<uintptr_t>(tempBuf.data()));
    *ptr = tempBuf.data();
    *length = byteLen;
    return true;
}

// --- Array ---

bool Object::getArrayLength(uint32_t *length) const {
    if (!length) return false;
    *length = _jsVal["length"].as<uint32_t>();
    return true;
}

bool Object::getArrayElement(uint32_t index, Value *data) const {
    internal::jsToSeValue(_jsVal[index], data);
    return true;
}

bool Object::setArrayElement(uint32_t index, const Value &data) {
    _jsVal.set(index, internal::seToJsValue(data));
    return true;
}

// --- Keys ---

bool Object::getAllKeys(ccstd::vector<ccstd::string> *allKeys) const {
    if (!allKeys) return false;
    val keys = val::global("Object").call<val>("keys", _jsVal);
    uint32_t len = keys["length"].as<uint32_t>();
    for (uint32_t i = 0; i < len; ++i) {
        allKeys->push_back(keys[i].as<std::string>());
    }
    return true;
}

// --- Map operations ---
void Object::clearMap() { _jsVal.call<void>("clear"); }
bool Object::removeMapElement(const Value &key) { return _jsVal.call<bool>("delete", internal::seToJsValue(key)); }
bool Object::getMapElement(const Value &key, Value *outValue) const {
    val result = _jsVal.call<val>("get", internal::seToJsValue(key));
    if (outValue) internal::jsToSeValue(result, outValue);
    return !result.isUndefined();
}
bool Object::setMapElement(const Value &key, const Value &value) {
    _jsVal.call<val>("set", internal::seToJsValue(key), internal::seToJsValue(value));
    return true;
}
uint32_t Object::getMapSize() const { return _jsVal["size"].as<uint32_t>(); }

ccstd::vector<std::pair<Value, Value>> Object::getAllElementsInMap() const {
    ccstd::vector<std::pair<Value, Value>> result;
    // TODO: iterate map entries
    return result;
}

// --- Set operations ---
void Object::clearSet() { _jsVal.call<void>("clear"); }
bool Object::removeSetElement(const Value &value) { return _jsVal.call<bool>("delete", internal::seToJsValue(value)); }
bool Object::addSetElement(const Value &value) { _jsVal.call<val>("add", internal::seToJsValue(value)); return true; }
bool Object::isElementInSet(const Value &value) const { return _jsVal.call<bool>("has", internal::seToJsValue(value)); }
uint32_t Object::getSetSize() const { return _jsVal["size"].as<uint32_t>(); }
ValueArray Object::getAllElementsInSet() const { return {}; /* TODO */ }

// --- Private data ---

void Object::setPrivateObject(PrivateObjectBase *data) {
    _privateObject = data;
    _privateData = data != nullptr ? data->getRaw() : nullptr;
    if (_privateData != nullptr) {
        NativePtrToObjectMap::emplace(_privateData, this);
        // Store this se::Object* on the JS object as a hidden property
        // so that the native trampoline can recover it when the object
        // is used as 'this' in a native method call.
        if (!_jsVal.isUndefined() && !_jsVal.isNull()) {
            _jsVal.set("__cc_se_ptr", val(static_cast<double>(reinterpret_cast<uintptr_t>(this))));
        }
    }
}

PrivateObjectBase *Object::getPrivateObject() const { return _privateObject; }

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

// --- Root / reference ---

void Object::root() { ++_rootCount; }
void Object::unroot() { if (_rootCount > 0) --_rootCount; }
bool Object::isRooted() const { return _rootCount > 0; }

// --- Comparison ---

bool Object::strictEquals(Object *o) const {
    if (!o) return false;
    return _jsVal.strictlyEquals(o->_jsVal);
}

bool Object::attachObject(Object * /*obj*/) { return true; }
bool Object::detachObject(Object * /*obj*/) { return true; }

ccstd::string Object::toString() const {
    if (_jsVal.isUndefined()) return "undefined";
    if (_jsVal.isNull()) return "null";
    return _jsVal.call<val>("toString").as<std::string>();
}

ccstd::string Object::toStringExt() const { return toString(); }

Class *Object::_getClass() const { return _cls; }
void Object::_setFinalizeCallback(V8FinalizeFunc cb) { _finalizeCb = cb; }
bool Object::_isNativeFunction() const { return isFunction(); }

} // namespace se

#endif
