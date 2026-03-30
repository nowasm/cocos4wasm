#pragma once

#include "../Define.h"
#include "../PrivateObject.h"
#include "../RefCounter.h"
#include "../Value.h"
#include "Base.h"
#include "base/HasMemberFunction.h"
#include "base/std/container/string.h"
#include "base/std/container/unordered_map.h"
#include "base/std/container/vector.h"

#include <memory>
#include <string>
#include <utility>

namespace se {

class Class;

class Object final : public RefCounter {
public:
    enum class TypedArrayType {
        NONE,
        INT8,
        INT16,
        INT32,
        UINT8,
        UINT8_CLAMPED,
        UINT16,
        UINT32,
        FLOAT32,
        FLOAT64
    };

    using BufferContentsFreeFunc = void (*)(void *contents, size_t byteLength, void *userData);

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
    CC_DEPRECATED(3.7)
    static Object *getObjectWithPtr(void *ptr);

    bool getProperty(const char *name, Value *data);
    bool getProperty(const char *name, Value *data, bool /*cachePropertyName*/) { return getProperty(name, data); }
    inline bool getProperty(const ccstd::string &name, Value *value) { return getProperty(name.c_str(), value); }

    bool setProperty(const char *name, const Value &data);
    inline bool setProperty(const ccstd::string &name, const Value &value) { return setProperty(name.c_str(), value); }

    bool deleteProperty(const char *name);
    bool defineOwnProperty(const char *name, const Value &value, bool writable = true, bool enumerable = true, bool configurable = true);
    bool defineProperty(const char *name, NativeFunctionPtr getter, NativeFunctionPtr setter);
    bool defineFunction(const char *funcName, NativeFunctionPtr func);

    bool isFunction() const;
    bool call(const ValueArray &args, Object *thisObject, Value *rval = nullptr);

    bool isMap() const;
    bool isWeakMap() const;
    bool isSet() const;
    bool isWeakSet() const;
    bool isArray() const;
    bool getArrayLength(uint32_t *length) const;
    bool getArrayElement(uint32_t index, Value *data) const;
    bool setArrayElement(uint32_t index, const Value &data);

    bool isTypedArray() const;
    bool isProxy() const;
    TypedArrayType getTypedArrayType() const;
    bool getTypedArrayData(uint8_t **ptr, size_t *length) const;

    bool isArrayBuffer() const;
    bool getArrayBufferData(uint8_t **ptr, size_t *length) const;

    bool getAllKeys(ccstd::vector<ccstd::string> *allKeys) const;

    void clearMap();
    bool removeMapElement(const Value &key);
    bool getMapElement(const Value &key, Value *outValue) const;
    bool setMapElement(const Value &key, const Value &value);
    uint32_t getMapSize() const;
    ccstd::vector<std::pair<Value, Value>> getAllElementsInMap() const;

    void clearSet();
    bool removeSetElement(const Value &value);
    bool addSetElement(const Value &value);
    bool isElementInSet(const Value &value) const;
    uint32_t getSetSize() const;
    ValueArray getAllElementsInSet() const;

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
        static_assert(!std::is_void<T>::value, "void * is not allowed for private data");
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
        static_assert(!std::is_void<T>::value, "void * is not allowed for private data");
        auto *privateObject = se::rawref_private_object(data);
        if (tryDestroyInGC) {
            privateObject->tryAllowDestroyInGC();
        }
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

    void root();
    void unroot();
    bool isRooted() const;

    bool strictEquals(Object *o) const;

    bool attachObject(Object *obj);
    bool detachObject(Object *obj);

    ccstd::string toString() const;
    ccstd::string toStringExt() const;

    static Object *_createJSObject(Class *cls, void * /*dummy*/ = nullptr);
    void *_getJSObject() const { return nullptr; }
    Class *_getClass() const;

    void _setFinalizeCallback(V8FinalizeFunc finalizeCb);
    bool _isNativeFunction() const;

    Object();
    ~Object() override;

private:
    Class *_cls{nullptr};
    uint32_t _rootCount{0};
    PrivateObjectBase *_privateObject{nullptr};
    void *_privateData{nullptr};
    V8FinalizeFunc _finalizeCb{nullptr};
    bool _clearMappingInFinalizer{true};

    ccstd::unordered_map<ccstd::string, Value> _props;
    ccstd::vector<Value> _array;
    bool _isArrayObj{false};
    bool _isFunc{false};

    friend class ScriptEngine;
    friend class Class;
};

} // namespace se
