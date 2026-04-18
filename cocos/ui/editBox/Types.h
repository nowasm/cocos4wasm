#pragma once

#include <cstdint>

namespace cc {

// Mirrors cocos/ui/editBox/types.ts — value order and numbers match the
// TypeScript enum so serialised scenes and code cross-referenced to the
// Creator docs produce the same behaviour.

// 键盘的返回键类型 — mobile-only hint for the OS keyboard's return key.
// Ignored on desktop/web but carried through for scene-file fidelity.
enum class KeyboardReturnType : uint32_t {
    DEFAULT = 0,
    DONE    = 1,
    SEND    = 2,
    SEARCH  = 3,
    GO      = 4,
    NEXT    = 5,
};

// 输入模式 — ANY is multiline (Enter inserts '\n'); every other value is
// single-line. The non-ANY values also hint at mobile keyboard layouts
// (numeric / URL / phone) that desktop platforms ignore.
enum class InputMode : uint32_t {
    ANY          = 0,
    EMAIL_ADDR   = 1,
    NUMERIC      = 2,
    PHONE_NUMBER = 3,
    URL          = 4,
    DECIMAL      = 5,
    SINGLE_LINE  = 6,
};

// 文本显示与格式化标志位 — PASSWORD masks each codepoint with U+25CF
// (●); the CAPS flags transform display text, not the authoritative
// string the user sees in getString().
enum class InputFlag : uint32_t {
    PASSWORD                    = 0,
    SENSITIVE                   = 1,
    INITIAL_CAPS_WORD           = 2,
    INITIAL_CAPS_SENTENCE       = 3,
    INITIAL_CAPS_ALL_CHARACTERS = 4,
    DEFAULT                     = 5,
};

}  // namespace cc
