#pragma once

#include <functional>

#include "base/Ptr.h"
#include "base/std/container/string.h"
#include "base/std/container/vector.h"
#include "core/component/Component.h"
#include "core/reflection/Reflection.h"
#include "math/Color.h"

namespace cc {

class BmfFont;
class Label;
class Node;
class Sprite;

// Single-line text input.
//
// Expected structure — EditBox auto-builds it on onEnable if absent:
//   editBoxNode            (UITransform + Sprite for background + EditBox)
//     ├─ label-node        (UITransform + Label, positioned inside)
//     └─ caret-node        (UITransform + Sprite, 2-px vertical bar)
//
// Focus model:
//   • Click inside → focus (opens OS text input).
//   • Click outside the focused box → blur (closes text input).
//   • Only one EditBox focused at any moment; focus transfer handled by
//     the static current-focus pointer.
//
// Input sources:
//   • events::TextInput — character insertion (UTF-8 chunks from the OS).
//   • events::Keyboard  — Backspace / Delete / Left / Right / Home / End /
//                          Enter / Escape (focus release).
//
// MVP scope — no selection, no mouse-driven caret placement, no clipboard,
// no password masking, no multi-line. Each arrives behind its own task.
class EditBox : public Component {
    CC_CLASS_DECL(EditBox, Component)
public:
    using TextChangedHandler = std::function<void(EditBox *, const ccstd::string &)>;

    EditBox() { _wantsUpdate = true; }
    ~EditBox() override = default;

    void onEnable() override;
    void onDisable() override;
    void update(float dt) override;

    const ccstd::string &getText() const { return _text; }
    void setText(const ccstd::string &t);

    const ccstd::string &getPlaceholder() const { return _placeholder; }
    void setPlaceholder(const ccstd::string &t);

    void setFont(BmfFont *f);

    void focus();
    void blur();
    bool isFocused() const { return _focused; }

    // Register a text-changed callback. Fires after every insertion /
    // deletion that changes `_text`.
    void setOnTextChanged(TextChangedHandler fn) { _onChanged = std::move(fn); }

private:
    // UI-tree plumbing.
    void ensureChildren();
    void refreshLabel();
    float _padLeft() const;  // MVP constant; becomes a setter later

    // Caret helpers.
    void updateCaretPos();
    void hideCaret();
    void showCaret();

    // Focus machinery.
    static EditBox *s_currentlyFocused;

    // Hit-test against our node bounds so a click outside blurs — the
    // dispatcher doesn't deliver "click-elsewhere" as an event so we watch
    // the engine bus directly.
    bool pointInside(float winX, float winY) const;

    // Event binding state kept out-of-header — see EditBox.cpp.
    struct Hooks;
    Hooks *_hooks{nullptr};

    ccstd::string _text;
    ccstd::string _placeholder;
    bool _focused{false};
    bool _caretVisible{true};
    float _caretTimer{0.f};
    static constexpr float kCaretBlinkPeriod = 0.5f;

    BmfFont *_font{nullptr};

    // Cached children (looked up once at onEnable).
    Label  *_label{nullptr};
    Sprite *_caret{nullptr};

    Color _textColor{230, 230, 230, 255};
    Color _placeholderColor{130, 130, 130, 255};

    TextChangedHandler _onChanged;
};

}  // namespace cc
