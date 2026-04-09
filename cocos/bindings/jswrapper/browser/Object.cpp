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

// Global registry: funcId → NativeFunctionPtr
namespace {
ccstd::vector<se::NativeFunctionPtr> __nativeFunctions; // NOLINT
int registerNativeFunction(se::NativeFunctionPtr func) {
    __nativeFunctions.push_back(func);
    return static_cast<int>(__nativeFunctions.size() - 1);
}

// Per-object native function registry (for defineFunction on instances)
ccstd::vector<se::NativeFunctionPtr> __objectNativeFunctions; // NOLINT
int registerObjectNativeFunction(se::NativeFunctionPtr func) {
    __objectNativeFunctions.push_back(func);
    return static_cast<int>(__objectNativeFunctions.size() - 1);
}
} // namespace

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
    // TODO: implement with native function trampolines
    return false;
}

bool Object::defineFunction(const char *funcName, NativeFunctionPtr func) {
    // TODO: implement with native function trampolines
    // For now, create a no-op function
    CC_LOG_WARNING("Object::defineFunction('%s') - native trampoline not yet implemented", funcName);
    return false;
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
