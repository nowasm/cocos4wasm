#pragma once

#include <functional>

#include "base/Ptr.h"
#include "base/std/container/string.h"
#include "base/std/container/vector.h"
#include "cocos/ui/editBox/Types.h"
#include "core/component/Component.h"
#include "core/reflection/Reflection.h"
#include "math/Color.h"

namespace cc {

class BmfFont;
class Label;
class Node;
class Sprite;

// Mirrors `cocos/ui/editBox/edit-box.ts` in the Creator engine. Same
// property names, same enums, same public event channels — the only
// differences are language syntax (C++ setters/getters instead of TS
// accessor properties) and the IME plumbing sitting over SDL instead
// of DOM.
//
// Child-node structure (auto-created on first onEnable):
//   editBoxNode (UITransform + Sprite[background] + EditBox)
//     ├─ TEXT_LABEL          (UITransform + Label — rendered input)
//     └─ PLACEHOLDER_LABEL   (UITransform + Label — shown when empty)
//
// Event channels — both fire together, same order as Creator:
//   1. Component-level callback vectors
//        `editingDidBegan`, `textChanged`, `editingDidEnded`, `editingReturn`
//      — programmatic subscribers push `std::function` handlers; matches
//      the TS `ComponentEventHandler[]` array.
//   2. Node typed events (Node.h)
//        `Node::EditBoxBegan / EditBoxEnded / EditBoxTextChanged / EditBoxReturn`
//      — follows the `node.emit(EditBoxEventType.X, this)` convention.
//
// Focus state machine:
//   • Touch inside  → focus() → open SDL text input → emit EditingDidBegan
//   • TextInput     → append at caret (maxLength respected) → TextChanged
//   • TextEditing   → live IME composition preview
//   • Enter:
//       - InputMode::ANY        → insert '\n'
//       - InputMode::SINGLE_LINE→ blur + emit EditingReturn
//       - all other modes       → blur + emit EditingReturn (Creator parity)
//   • Click outside → blur() → close SDL text input → EditingDidEnded
//
// Multi-caret / selection / clipboard / password masking / multi-line
// navigation are handled internally; no separate sub-component.
class EditBox : public Component {
    CC_CLASS_DECL(EditBox, Component)
public:
    // ── Enums re-exported so demo code can write EditBox::InputMode::ANY
    //    just like TS does EditBox.InputMode.ANY.
    using InputMode          = cc::InputMode;
    using InputFlag          = cc::InputFlag;
    using KeyboardReturnType = cc::KeyboardReturnType;

    // Event-handler vector element shape. Matches Creator's
    // `ComponentEventHandler[]` semantics — handlers fire in registration
    // order, a handler can remove itself safely (we iterate a snapshot).
    using Handler = std::function<void(EditBox *)>;

    EditBox() { _wantsUpdate = true; }
    ~EditBox() override = default;

    // ── Component lifecycle ──────────────────────────────────────────────
    void onEnable() override;
    void onDisable() override;
    void onDestroy() override;
    void update(float dt) override;

    // ── Public properties (mirror TS getters/setters) ────────────────────

    // Current input text.
    const ccstd::string &getString() const { return _string; }
    void setString(const ccstd::string &value);

    // Placeholder shown when string is empty.
    const ccstd::string &getPlaceholder() const { return _placeholder; }
    void setPlaceholder(const ccstd::string &value);

    // Text label component (auto-created on first onEnable; may be swapped).
    Label *getTextLabel() const { return _textLabel; }
    void   setTextLabel(Label *v);

    // Placeholder label component.
    Label *getPlaceholderLabel() const { return _placeholderLabel; }
    void   setPlaceholderLabel(Label *v);

    // InputFlag — PASSWORD masks the display, CAPS_* flags transform text.
    InputFlag getInputFlag() const { return _inputFlag; }
    void      setInputFlag(InputFlag v);

    // InputMode — ANY = multiline, everything else = single line.
    InputMode getInputMode() const { return _inputMode; }
    void      setInputMode(InputMode v);

    // Return-key style (OS keyboard hint; desktop-no-op).
    KeyboardReturnType getReturnType() const { return _returnType; }
    void               setReturnType(KeyboardReturnType v) { _returnType = v; }

    // Max character count. < 0 = unlimited; 0 = read-only (refuses input).
    int32_t getMaxLength() const { return _maxLength; }
    void    setMaxLength(int32_t v) { _maxLength = v; }

    // Web DOM tabindex — stored for fidelity, no effect on desktop C++.
    int32_t getTabIndex() const { return _tabIndex; }
    void    setTabIndex(int32_t v) { _tabIndex = v; }

    // Background sprite — a BmfFont/SpriteFrame cache is outside this
    // component; the setter just assigns a textured-sprite the caller
    // already built. Null keeps the plain-rect background.
    // Not yet wired to SpriteFrame asset flow; this is a pointer pair
    // with the Creator API so migrations compile.
    void setBackgroundSprite(Sprite *s) { _background = s; }
    Sprite *getBackgroundSprite() const { return _background; }

    // Convenience: set the font used by both labels.
    void setFont(BmfFont *f);

    // ── Focus control ────────────────────────────────────────────────────
    void setFocus() { focus(); }  // TS alias
    void focus();
    void blur();
    bool isFocused() const { return _focused; }

    // ── Event subscription (component-level callback vectors) ────────────
    // Matches Creator's four inspector-editable arrays. Handlers persist
    // until the component is destroyed or clearXxxHandlers() is called.
    void addEditingDidBeganHandler(Handler fn)  { _editingDidBegan.push_back(std::move(fn)); }
    void addTextChangedHandler(Handler fn)      { _textChanged.push_back(std::move(fn)); }
    void addEditingDidEndedHandler(Handler fn)  { _editingDidEnded.push_back(std::move(fn)); }
    void addEditingReturnHandler(Handler fn)    { _editingReturn.push_back(std::move(fn)); }

    void clearEditingDidBeganHandlers() { _editingDidBegan.clear(); }
    void clearTextChangedHandlers()     { _textChanged.clear(); }
    void clearEditingDidEndedHandlers() { _editingDidEnded.clear(); }
    void clearEditingReturnHandlers()   { _editingReturn.clear(); }

    // ── Caret / selection helpers (useful for programmatic control) ──────
    size_t getCaretIndex() const { return _caretIdx; }
    void   setCaretIndex(size_t i);
    bool   hasSelection() const { return _selAnchor != _caretIdx; }
    size_t getSelectionStart() const;
    size_t getSelectionEnd() const;
    void   clearSelection();

private:
    // ── TS-style internal helpers ────────────────────────────────────────
    void _init();
    void _ensureBackground();
    void _updateTextLabel();
    void _updatePlaceholderLabel();
    void _updateLabels();                       // toggles empty/non-empty label visibility
    void _updateString(const ccstd::string &);  // re-renders with InputFlag rules
    void _syncSize();
    ccstd::string _updateLabelStringStyle(const ccstd::string &text,
                                           bool ignorePassword = false) const;

    // ── Internal machinery (carried over from the previous impl) ─────────
    void bindEventHooks();
    void unbindEventHooks();
    bool pointInside(float winX, float winY) const;
    size_t windowPointToCaretIndex(float winX, float winY) const;

    // Caret + selection.
    void updateCaretPos();
    void resetCaretBlink();
    void deleteSelection();
    void updateSelectionHighlight();

    // Event firing (both channels).
    void fireEditingDidBegan();
    void fireTextChanged();
    void fireEditingDidEnded();
    void fireEditingReturn();

    // Multi-line geometry helpers.
    void rebuildLineIndex(ccstd::vector<std::pair<size_t, size_t>> &out) const;
    size_t lineIndexOf(const ccstd::vector<std::pair<size_t, size_t>> &lines,
                        size_t byteIdx) const;
    float  columnAdvance(size_t lineStart, size_t byteIdx) const;
    size_t byteIdxAtColumn(size_t lineStart, size_t lineLen, float targetX) const;

    // Only a single EditBox can own focus (mirrors the TS
    // `_currentEditBoxImpl` singleton) — focus transfer is handled here.
    static EditBox *s_currentlyFocused;

    // Serialised state.
    ccstd::string      _string;
    ccstd::string      _placeholder;
    Label             *_textLabel{nullptr};
    Label             *_placeholderLabel{nullptr};
    Sprite            *_background{nullptr};
    InputFlag          _inputFlag{InputFlag::DEFAULT};
    InputMode          _inputMode{InputMode::ANY};
    KeyboardReturnType _returnType{KeyboardReturnType::DEFAULT};
    int32_t            _maxLength{20};   // matches TS default
    int32_t            _tabIndex{0};

    // Component event handler vectors (TS-style inspector arrays).
    ccstd::vector<Handler> _editingDidBegan;
    ccstd::vector<Handler> _textChanged;
    ccstd::vector<Handler> _editingDidEnded;
    ccstd::vector<Handler> _editingReturn;

    // Runtime (non-serialised).
    bool    _focused{false};
    bool    _isLabelVisible{true};
    BmfFont *_font{nullptr};  // cached so newly created labels pick it up

    // Caret / selection — same shape as the previous impl.
    size_t _caretIdx{0};
    size_t _selAnchor{0};
    bool   _mouseDragging{false};
    bool   _ctrlDown{false};
    bool   _caretVisible{true};
    float  _caretTimer{0.f};
    static constexpr float kCaretBlinkPeriod = 0.5f;

    // IME composition preview.
    ccstd::string _composition;

    // Up/Down preferred column (multi-line).
    float _preferredCaretX{0.f};
    bool  _preferredCaretXValid{false};

    // Visual children.
    Sprite *_caret{nullptr};
    Sprite *_selHighlight{nullptr};

    Color _textColor{230, 230, 230, 255};
    Color _placeholderColor{130, 130, 130, 255};
    Color _selectionColor{80, 140, 210, 180};

    // SDL bus listener bundle (heap-alloc'd to keep the header deps light).
    struct Hooks;
    Hooks *_hooks{nullptr};
};

}  // namespace cc
