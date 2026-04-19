#pragma once
#include <cstdint>
#include <functional>
#include "base/std/container/string.h"
#include "base/std/container/vector.h"

namespace cc {
namespace reflection {

// Runtime-typed argument passed into reflected method invocation.
//
// The reflection bridge exists to replay Cocos Creator's `ComponentEventHandler`
// entries (target + component-name + handler-name + customEventData) — so the
// argument shapes we actually see are narrow: a "sender" pointer, a string
// customEventData, and the occasional numeric payload. MethodArg stays small
// (tagged union + one optional string) to keep per-call cost near zero.
struct MethodArg {
    enum class Kind : uint8_t {
        NONE = 0,
        BOOL,
        INT32,
        FLOAT,
        STRING,
        POINTER,
    };

    Kind kind{Kind::NONE};
    union {
        bool b;
        int32_t i;
        float f;
        void *p;
    };
    // Kept outside the union because ccstd::string has a non-trivial ctor.
    // Only meaningful when kind == STRING; otherwise left empty.
    ccstd::string s;

    MethodArg() : i(0) {}

    static MethodArg makeBool(bool v)             { MethodArg a; a.kind = Kind::BOOL;    a.b = v; return a; }
    static MethodArg makeInt(int32_t v)           { MethodArg a; a.kind = Kind::INT32;   a.i = v; return a; }
    static MethodArg makeFloat(float v)           { MethodArg a; a.kind = Kind::FLOAT;   a.f = v; return a; }
    static MethodArg makeString(ccstd::string v)  { MethodArg a; a.kind = Kind::STRING;  a.s = std::move(v); return a; }
    static MethodArg makePointer(void *v)         { MethodArg a; a.kind = Kind::POINTER; a.p = v; return a; }
};

using MethodArgs = ccstd::vector<MethodArg>;

// Type-erased callable bound to a specific `void (Class::*)(Args...)` member.
// The builder emits one of these per registered method; the invoker knows how
// to unpack MethodArgs into the real parameter list and dispatches.
using MethodInvoker = std::function<void(void *instance, const MethodArgs &args)>;

struct MethodMeta {
    const char *name{nullptr};
    uint32_t arity{0};
    MethodInvoker invoker;
};

}  // namespace reflection
}  // namespace cc
