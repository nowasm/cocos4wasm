#include "Object.h"
#include "Class.h"
#include "base/memory/Memory.h"

namespace se {

Object::Object() = default;

Object::~Object() {
    clearPrivateData(true);
}

Object *Object::createPlainObject() {
    auto *o = ccnew Object();
    return o;
}

Object *Object::createMapObject() {
    return createPlainObject();
}

Object *Object::createSetObject() {
    return createPlainObject();
}

Object *Object::createArrayObject(size_t length) {
    auto *o = ccnew Object();
    o->_isArrayObj = true;
    o->_array.resize(length);
    return o;
}

Object *Object::createPromise() {
    return createPlainObject();
}

void Object::rejectPromise(Object * /*object*/, const Value & /*value*/) {}

void Object::resolverPromise(Object * /*object*/, const Value & /*value*/) {}

Object *Object::createUint8TypedArray(uint8_t * /*bytes*/, size_t /*byteLength*/) {
    return createPlainObject();
}

Object *Object::createTypedArray(TypedArrayType /*type*/, const void * /*data*/, size_t /*byteLength*/) {
    return createPlainObject();
}

Object *Object::createTypedArrayWithBuffer(TypedArrayType /*type*/, const Object * /*obj*/) {
    return createPlainObject();
}

Object *Object::createTypedArrayWithBuffer(TypedArrayType /*type*/, const Object * /*obj*/, size_t /*offset*/) {
    return createPlainObject();
}

Object *Object::createTypedArrayWithBuffer(TypedArrayType /*type*/, const Object * /*obj*/, size_t /*offset*/, size_t /*byteLength*/) {
    return createPlainObject();
}

Object *Object::createArrayBufferObject(const void * /*data*/, size_t /*byteLength*/) {
    return createPlainObject();
}

Object *Object::createExternalArrayBufferObject(void * /*contents*/, size_t /*byteLength*/, BufferContentsFreeFunc /*freeFunc*/, void * /*freeUserData*/) {
    return createPlainObject();
}

Object *Object::createJSONObject(const ccstd::string & /*jsonStr*/) {
    return nullptr;
}

Object *Object::createJSONObject(std::u16string && /*jsonStr*/) {
    return nullptr;
}

Object *Object::createObjectWithClass(Class * /*cls*/) {
    return createPlainObject();
}

Object *Object::createObjectWithConstructor(Object * /*constructor*/) {
    return createPlainObject();
}

Object *Object::createObjectWithConstructor(Object * /*constructor*/, const ValueArray & /*args*/) {
    return createPlainObject();
}

Object *Object::createProxyTarget(Object *proxy) {
    return proxy;
}

Object *Object::getObjectWithPtr(void * /*ptr*/) {
    return nullptr;
}

bool Object::getProperty(const char *name, Value *data) {
    if (data == nullptr || name == nullptr) {
        return false;
    }
    auto it = _props.find(name);
    if (it == _props.end()) {
        *data = Value::Undefined;
        return false;
    }
    *data = it->second;
    return true;
}

bool Object::setProperty(const char *name, const Value &data) {
    if (name == nullptr) {
        return false;
    }
    _props[name] = data;
    return true;
}

bool Object::deleteProperty(const char *name) {
    if (name == nullptr) {
        return false;
    }
    return _props.erase(name) > 0;
}

bool Object::defineOwnProperty(const char *name, const Value &value, bool /*writable*/, bool /*enumerable*/, bool /*configurable*/) {
    return setProperty(name, value);
}

bool Object::defineProperty(const char * /*name*/, NativeFunctionPtr /*getter*/, NativeFunctionPtr /*setter*/) {
    return true;
}

bool Object::defineFunction(const char * /*funcName*/, NativeFunctionPtr /*func*/) {
    return true;
}

bool Object::isFunction() const {
    return _isFunc;
}

bool Object::call(const ValueArray & /*args*/, Object * /*thisObject*/, Value * /*rval*/) {
    return false;
}

bool Object::isMap() const { return false; }
bool Object::isWeakMap() const { return false; }
bool Object::isSet() const { return false; }
bool Object::isWeakSet() const { return false; }

bool Object::isArray() const {
    return _isArrayObj;
}

bool Object::getArrayLength(uint32_t *length) const {
    if (length == nullptr) {
        return false;
    }
    *length = static_cast<uint32_t>(_array.size());
    return _isArrayObj;
}

bool Object::getArrayElement(uint32_t index, Value *data) const {
    if (data == nullptr || !_isArrayObj || index >= _array.size()) {
        return false;
    }
    *data = _array[index];
    return true;
}

bool Object::setArrayElement(uint32_t index, const Value &data) {
    if (!_isArrayObj || index >= _array.size()) {
        return false;
    }
    _array[index] = data;
    return true;
}

bool Object::isTypedArray() const { return false; }
bool Object::isProxy() const { return false; }
Object::TypedArrayType Object::getTypedArrayType() const { return TypedArrayType::NONE; }

bool Object::getTypedArrayData(uint8_t ** /*ptr*/, size_t * /*length*/) const {
    return false;
}

bool Object::isArrayBuffer() const { return false; }

bool Object::getArrayBufferData(uint8_t ** /*ptr*/, size_t * /*length*/) const {
    return false;
}

bool Object::getAllKeys(ccstd::vector<ccstd::string> *allKeys) const {
    if (allKeys == nullptr) {
        return false;
    }
    allKeys->clear();
    for (const auto &e : _props) {
        allKeys->push_back(e.first);
    }
    return true;
}

void Object::clearMap() {}
bool Object::removeMapElement(const Value & /*key*/) { return false; }
bool Object::getMapElement(const Value & /*key*/, Value * /*outValue*/) const { return false; }
bool Object::setMapElement(const Value & /*key*/, const Value & /*value*/) { return false; }
uint32_t Object::getMapSize() const { return 0; }
ccstd::vector<std::pair<Value, Value>> Object::getAllElementsInMap() const { return {}; }

void Object::clearSet() {}
bool Object::removeSetElement(const Value & /*value*/) { return false; }
bool Object::addSetElement(const Value & /*value*/) { return false; }
bool Object::isElementInSet(const Value & /*value*/) const { return false; }
uint32_t Object::getSetSize() const { return 0; }
ValueArray Object::getAllElementsInSet() const { return {}; }

void Object::setPrivateObject(PrivateObjectBase *data) {
    _privateObject = data;
    _privateData = data != nullptr ? data->getRaw() : nullptr;
}

PrivateObjectBase *Object::getPrivateObject() const {
    return _privateObject;
}

void Object::clearPrivateData(bool clearMapping) {
    CC_UNUSED_PARAM(clearMapping);
    _privateObject = nullptr;
    _privateData = nullptr;
}

void Object::root() {
    ++_rootCount;
}

void Object::unroot() {
    if (_rootCount > 0) {
        --_rootCount;
    }
}

bool Object::isRooted() const {
    return _rootCount > 0;
}

bool Object::strictEquals(Object *o) const {
    return this == o;
}

bool Object::attachObject(Object * /*obj*/) {
    return true;
}

bool Object::detachObject(Object * /*obj*/) {
    return true;
}

ccstd::string Object::toString() const {
    return "[object NoneObject]";
}

ccstd::string Object::toStringExt() const {
    return toString();
}

Object *Object::_createJSObject(Class *cls, void * /*dummy*/) {
    auto *o = createPlainObject();
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
    return false;
}

} // namespace se
