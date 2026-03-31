#pragma once

#include "../Value.h"
#include "../config.h"

extern "C" {
#include "quickjs.h"
}

#if SCRIPT_ENGINE_TYPE == SCRIPT_ENGINE_QJS

namespace se {
namespace internal {

void jsToSeValue(JSContext *ctx, JSValue jsVal, Value *v);
JSValue seToJsValue(JSContext *ctx, const Value &v);
void jsToSeArgs(JSContext *ctx, int argc, JSValue *argv, ValueArray &outArr);
void setReturnValue(const Value &data, JSContext *ctx, JSValue *retVal);

void forceConvertJsValueToStdString(JSContext *ctx, JSValue jsVal, ccstd::string *result);

void seToJsArgs(JSContext *ctx, const ValueArray &args, JSValue *outArgs);

} // namespace internal
} // namespace se

#endif
