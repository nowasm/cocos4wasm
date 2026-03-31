#include "Utils.h"
#include "Object.h"
#include "ScriptEngine.h"

#if SCRIPT_ENGINE_TYPE == SCRIPT_ENGINE_QJS

namespace se {
namespace internal {

void jsToSeValue(JSContext *ctx, JSValue jsVal, Value *v) {
    if (v == nullptr) return;

    if (JS_IsNull(jsVal)) {
        v->setNull();
    } else if (JS_IsUndefined(jsVal)) {
        v->setUndefined();
    } else if (JS_IsBool(jsVal)) {
        v->setBoolean(JS_ToBool(ctx, jsVal) != 0);
    } else if (JS_IsNumber(jsVal)) {
        double d = 0;
        JS_ToFloat64(ctx, &d, jsVal);
        v->setDouble(d);
    } else if (JS_IsString(jsVal)) {
        const char *str = JS_ToCString(ctx, jsVal);
        if (str) {
            v->setString(ccstd::string(str));
            JS_FreeCString(ctx, str);
        } else {
            v->setString("");
        }
    } else if (JS_IsObject(jsVal)) {
        auto *obj = Object::_createJSObject(nullptr, JS_DupValue(ctx, jsVal));
        v->setObject(obj);
        obj->decRef();
    } else if (JS_IsBigInt(ctx, jsVal)) {
        int64_t i64 = 0;
        JS_ToInt64(ctx, &i64, jsVal);
        v->setInt64(i64);
    } else {
        v->setUndefined();
    }
}

JSValue seToJsValue(JSContext *ctx, const Value &v) {
    switch (v.getType()) {
        case Value::Type::Undefined:
            return JS_UNDEFINED;
        case Value::Type::Null:
            return JS_NULL;
        case Value::Type::Boolean:
            return JS_NewBool(ctx, v.toBoolean() ? 1 : 0);
        case Value::Type::Number:
            return JS_NewFloat64(ctx, v.toDouble());
        case Value::Type::String:
            return JS_NewStringLen(ctx, v.toString().c_str(), v.toString().size());
        case Value::Type::Object:
            if (v.toObject() != nullptr) {
                return JS_DupValue(ctx, v.toObject()->_getJSObject());
            }
            return JS_NULL;
        case Value::Type::BigInt:
            return JS_NewBigInt64(ctx, v.toInt64());
        default:
            return JS_UNDEFINED;
    }
}

void jsToSeArgs(JSContext *ctx, int argc, JSValue *argv, ValueArray &outArr) {
    outArr.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        Value v;
        jsToSeValue(ctx, argv[i], &v);
        outArr.push_back(std::move(v));
    }
}

void setReturnValue(const Value &data, JSContext *ctx, JSValue *retVal) {
    if (retVal == nullptr) return;
    *retVal = seToJsValue(ctx, data);
}

void forceConvertJsValueToStdString(JSContext *ctx, JSValue jsVal, ccstd::string *result) {
    if (result == nullptr) return;
    const char *str = JS_ToCString(ctx, jsVal);
    if (str) {
        *result = str;
        JS_FreeCString(ctx, str);
    } else {
        result->clear();
    }
}

void seToJsArgs(JSContext *ctx, const ValueArray &args, JSValue *outArgs) {
    for (size_t i = 0; i < args.size(); ++i) {
        outArgs[i] = seToJsValue(ctx, args[i]);
    }
}

} // namespace internal
} // namespace se

#endif
