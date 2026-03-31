#pragma once

#include <functional>
#include "../PrivateObject.h"
#include "base/std/container/string.h"
#include "base/std/container/unordered_map.h"

extern "C" {
#include "quickjs.h"
}

namespace se {
class Object;
class State;
} // namespace se

using V8FinalizeFunc = void (*)(se::Object *);
using NativeFunctionPtr = bool (*)(se::State &);
