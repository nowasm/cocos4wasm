#pragma once

#include "../PrivateObject.h"

namespace se {
class Object;
class State;

using V8FinalizeFunc = void (*)(se::Object *);
using NativeFunctionPtr = bool (*)(se::State &);

} // namespace se
