#pragma once
#include "ClassMeta.h"

namespace cc {
namespace reflection {

using GetStaticClassFn = const ClassMeta *(*)();

// Static-init hook that forces evaluation of a class's getStaticClass(),
// ensuring the class registers with ClassDB even if no runtime code path has
// referenced it yet (the JSON deserializer looks up classes by string name).
//
// Emitted by the CC_END_CLASS macro in an anonymous namespace of the .cpp.
struct ClassAutoReg {
    explicit ClassAutoReg(GetStaticClassFn fn) {
        if (fn) (void)fn();
    }
};

}  // namespace reflection
}  // namespace cc
