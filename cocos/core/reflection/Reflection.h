#pragma once

// Umbrella header for the reflection system. Include this from any class
// that uses CC_CLASS_DECL / CC_IMPLEMENT_CLASS macros, or from code that
// needs runtime ClassDB access.

#include "ClassAutoReg.h"
#include "ClassBuilder.h"
#include "ClassDB.h"
#include "ClassMeta.h"
#include "MethodMeta.h"
#include "PropertyMeta.h"
#include "ReflectionMacros.h"
#include "TypeId.h"
