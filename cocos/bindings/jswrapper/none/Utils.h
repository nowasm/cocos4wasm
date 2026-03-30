#pragma once

#include "../Value.h"
#include "../config.h"

#if SCRIPT_ENGINE_TYPE == SCRIPT_ENGINE_NONE

namespace se {
namespace internal {
inline void jsToSeArgs(void *, int, void *, ValueArray &) {}
inline void setReturnValue(const Value &, void *) {}
} // namespace internal
} // namespace se

#endif
