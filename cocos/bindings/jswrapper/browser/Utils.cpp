#include "Utils.h"
#include "Object.h"
#include "ScriptEngine.h"

#if SCRIPT_ENGINE_TYPE == SCRIPT_ENGINE_BROWSER

namespace se {
namespace internal {

void jsToSeValue(emscripten::val jsVal, Value *v) {
    if (v == nullptr) return;

    if (jsVal.isNull()) {
        v->setNull();
    } else if (jsVal.isUndefined()) {
        v->setUndefined();
    } else if (jsVal.isTrue() || jsVal.isFalse()) {
        v->setBoolean(jsVal.as<bool>());
    } else if (jsVal.isNumber()) {
        v->setDouble(jsVal.as<double>());
    } else if (jsVal.isString()) {
        v->setString(jsVal.as<std::string>());
    } else if (jsVal.typeOf().as<std::string>() == "bigint") {
        // BigInt — convert to int64 via Number()
        v->setInt64(jsVal.as<int64_t>());
    } else {
        // Object (including arrays, functions, etc.)
        auto *obj = Object::_createJSObject(nullptr, jsVal);
        // Recover native private data if this JS object was created by a JSB
        // constructor (setPrivateObject stores __cc_se_ptr on the JS object).
        emscripten::val ptrVal = jsVal["__cc_se_ptr"];
        if (!ptrVal.isUndefined() && ptrVal.isNumber()) {
            auto *persistent = reinterpret_cast<Object *>(
                static_cast<uintptr_t>(ptrVal.as<double>()));
            if (persistent && persistent->getPrivateData()) {
                obj->_borrowPrivateData(persistent->getPrivateData());
                // Don't erase NativePtrToObjectMap on temp destruction
                obj->setClearMappingInFinalizer(false);
            }
        }
        v->setObject(obj);
        obj->decRef();
    }
}

emscripten::val seToJsValue(const Value &v) {
    switch (v.getType()) {
        case Value::Type::Undefined:
            return emscripten::val::undefined();
        case Value::Type::Null:
            return emscripten::val::null();
        case Value::Type::Boolean:
            return emscripten::val(v.toBoolean());
        case Value::Type::Number:
            return emscripten::val(v.toDouble());
        case Value::Type::String:
            return emscripten::val(v.toString());
        case Value::Type::Object:
            if (v.toObject() != nullptr) {
                return v.toObject()->_getJSObject();
            }
            return emscripten::val::null();
        case Value::Type::BigInt:
            return emscripten::val(static_cast<double>(v.toInt64()));
        default:
            return emscripten::val::undefined();
    }
}

void jsToSeArgs(emscripten::val argsArray, ValueArray &outArr) {
    uint32_t len = argsArray["length"].as<uint32_t>();
    outArr.reserve(len);
    for (uint32_t i = 0; i < len; ++i) {
        Value v;
        jsToSeValue(argsArray[i], &v);
        outArr.push_back(std::move(v));
    }
}

void forceConvertJsValueToStdString(emscripten::val jsVal, ccstd::string *result) {
    if (result == nullptr) return;
    if (jsVal.isString()) {
        *result = jsVal.as<std::string>();
    } else if (!jsVal.isUndefined() && !jsVal.isNull()) {
        auto str = jsVal.call<emscripten::val>("toString");
        *result = str.as<std::string>();
    } else {
        result->clear();
    }
}

} // namespace internal
} // namespace se

#endif
