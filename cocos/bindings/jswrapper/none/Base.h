#pragma once

namespace se {
class Object;
class State;
} // namespace se

using V8FinalizeFunc = void (*)(se::Object *);
using NativeFunctionPtr = bool (*)(se::State &);
