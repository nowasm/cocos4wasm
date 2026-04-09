#pragma once

#include "../Value.h"
#include <emscripten/val.h>

namespace se {
namespace internal {

void jsToSeValue(emscripten::val jsVal, Value *v);
emscripten::val seToJsValue(const Value &v);
void jsToSeArgs(emscripten::val argsArray, ValueArray &outArr);
void forceConvertJsValueToStdString(emscripten::val jsVal, ccstd::string *result);

} // namespace internal
} // namespace se
