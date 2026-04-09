#pragma once

#include "../Define.h"
#include "../PrivateObject.h"
#include "../RefCounter.h"
#include "../Value.h"
#include "../config.h"
#include "Base.h"
#include "base/HasMemberFunction.h"
#include "base/std/container/string.h"
#include "base/std/container/unordered_map.h"
#include "base/std/container/vector.h"

#include <emscripten/val.h>
#include <memory>
#include <string>
#include <utility>

namespace se {

class Class;

class Object final : public RefCounter {
public:
    enum class TypedArrayType {
        NONE, INT8, INT16, INT32, UINT8, UINT8_CLAMPED,
        UINT16, UINT32, FLOAT32, FLOAT64, BIGINT64, BIGUINT64
    };

    using BufferContentsFreeFunc = void (*)(void *contents, size_t byteLength, void *userData);

    // --- Static factory methods ---
    static Object *createPlainObject();
    static Object *createMapObject();
    static Object *createSetObject();
    static Object *createArrayObject(size_t length);
    static Object *createPromise();
    static void rejectPromise(Object *object, const Value &value);
    static void resolverPromise(Object *object, const Value &value);
    static Object *createUint8TypedArray(uint8_t *bytes, size_t byteLength);
    static Object *createTypedArray(TypedArrayType type, const void *data, size_t byteLength);
    static Object *createTypedArrayWithBuffer(TypedArrayType type, const Object *obj);
    static Object *createTypedArrayWithBuffer(TypedArrayType type, const Object *obj, size_t offset);
    static Object *createTypedArrayWithBuffer(TypedArrayType type, const Object *obj, size_t offset, size_t byteLength);
    static Object *createArrayBufferObject(const void *data, size_t byteLength);
    static Object *createExternalArrayBufferObject(void *contents, size_t byteLength, BufferContentsFreeFunc freeFunc, void *freeUserData = nullptr);
    static Object *createJSONObject(const ccstd::string &jsonStr);
    static Object *createJSONObject(std::u16string &&jsonStr);
    static Object *createObjectWithClass(Class *cls);
    static Object *createObjectWithConstructor(Object *constructor);
    static Object *createObjectWithConstructor(Object *constructor, const ValueArray &args);
    static Object *createProxyTarget(Object *proxy);
    static Object *getObjectWithPtr(void *ptr);

    // --- Property access ---
    bool getProperty(const char *name, Value *data, bool cachePropertyName = false);
    inline bool getProperty(const ccstd::string &name, Value *value) { return getProperty(name.c_str(), value); }
    bool setProperty(const char *name, const Value &data);
    inline bool setProperty(const ccstd::string &name, const Value &value) { return setProperty(name.c_str(), value); }
    bool deleteProperty(const char *name);
    bool defineOwnProperty(const char *name, const Value &value, bool writable = true, bool enumerable = true, bool configurable = true);
    bool defineProperty(const char *name, NativeFunctionPtr getter, NativeFunctionPtr setter);
    bool defineFunction(const char *funcName, NativeFunctionPtr func);

    // --- Function ---
    bool isFunction() const;
    bool call(const ValueArray &args, Object *thisObject, Value *rval = nullptr);

    // --- Collection types ---
    bool isMap() const;
    bool isWeakMap() const;
    bool isSet() const;
    bool isWeakSet() const;

    // --- Array ---
    bool isArray() const;
    bool getArrayLength(uint32_t *length) const;
    bool getArrayElement(uint32_t index, Value *data) const;
    bool setArrayElement(uint32_t index, const Value &data);

    // --- TypedArray / ArrayBuffer ---
    bool isTypedArray() const;
    bool isProxy() const;
    TypedArrayType getTypedArrayType() const;
    bool getTypedArrayData(uint8_t **ptr, size_t *length) const;
    bool isArrayBuffer() const;
    bool getArrayBufferData(uint8_t **ptr, size_t *length) const;

    // --- Keys ---
    bool getAllKeys(ccstd::vector<ccstd::string> *allKeys) const;

    // --- Map ---
    void clearMap();
    bool removeMapElement(const Value &key);
    bool getMapElement(const Value &key, Value *outValue) const;
    bool setMapElement(const Value &key, const Value &value);
    uint32_t getMapSize() const;
    ccstd::vector<std::pair<Value, Value>> getAllElementsInMap() const;

    // --- Set ---
    void clearSet();
    bool removeSetElement(const Value &value);
    bool addSetElement(const Value &value);
    bool isElementInSet(const Value &value) const;
    uint32_t getSetSize() const;
    ValueArray getAllElementsInSet() const;

    // --- Private data ---
    void setPrivateObject(PrivateObjectBase *data);
    template <typename T>
    inline void setPrivateObject(TypedPrivateObject<T> *data) {
        setPrivateObject(static_cast<PrivateObjectBase *>(data));
        if constexpr (cc::has_setScriptObject<T, void(Object *)>::value) {
            data->template get<T>()->setScriptObject(this);
        }
    }
    PrivateObjectBase *getPrivateObject() const;
    inline void *getPrivateData() const { return _privateData; }

    template <typename T>
    inline void setPrivateData(T *data) {
        static_assert(!std::is_void<T>::value, "void * is not allowed");
        setPrivateObject(se::make_shared_private_object(data));
    }
    template <typename T>
    inline void setPrivateData(const cc::IntrusivePtr<T> &data) {
        setPrivateObject(se::ccintrusive_ptr_private_object(data));
    }
    template <typename T>
    inline void setPrivateData(const std::shared_ptr<T> &data) {
        setPrivateObject(se::shared_ptr_private_object(data));
    }
    template <typename T>
    inline void setRawPrivateData(T *data, bool tryDestroyInGC = false) {
        static_assert(!std::is_void<T>::value, "void * is not allowed");
        auto *privateObject = se::rawref_private_object(data);
        if (tryDestroyInGC) privateObject->tryAllowDestroyInGC();
        setPrivateObject(privateObject);
    }
    template <typename T>
    inline std::shared_ptr<T> getPrivateSharedPtr() const {
        CC_ASSERT(_privateObject && _privateObject->isSharedPtr());
        return static_cast<se::SharedPtrPrivateObject<T> *>(_privateObject)->getData();
    }
    template <typename T>
    inline cc::IntrusivePtr<T> getPrivateInstrusivePtr() const {
        CC_ASSERT(_privateObject && _privateObject->isCCIntrusivePtr());
        return static_cast<se::CCIntrusivePtrPrivateObject<T> *>(_privateObject)->getData();
    }
    template <typename T>
    inline T *getTypedPrivateData() const {
        return reinterpret_cast<T *>(getPrivateData());
    }

    void clearPrivateData(bool clearMapping = true);
    void setClearMappingInFinalizer(bool v) { _clearMappingInFinalizer = v; }
    bool isClearMappingInFinalizer() const { return _clearMappingInFinalizer; }

    // --- Root/reference ---
    void root();
    void unroot();
    bool isRooted() const;

    // --- Comparison ---
    bool strictEquals(Object *o) const;
    bool attachObject(Object *obj);
    bool detachObject(Object *obj);
    ccstd::string toString() const;
    ccstd::string toStringExt() const;

    // --- Internal ---
    emscripten::val _getJSObject() const { return _jsVal; }
    static Object *_createJSObject(Class *cls, emscripten::val jsVal);
    Class *_getClass() const;
    void _setFinalizeCallback(V8FinalizeFunc finalizeCb);
    bool _isNativeFunction() const;

private:
    Object();
    ~Object() override;

    emscripten::val _jsVal{emscripten::val::undefined()};
    Class *_cls{nullptr};
    V8FinalizeFunc _finalizeCb{nullptr};
    PrivateObjectBase *_privateObject{nullptr};
    void *_privateData{nullptr};
    uint32_t _rootCount{0};
    bool _clearMappingInFinalizer{true};

    friend class ScriptEngine;
    friend class Class;
};

} // namespace se
