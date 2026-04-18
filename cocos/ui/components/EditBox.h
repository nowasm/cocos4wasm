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

    // Password masking — display substitutes each codepoint in the
    // authoritative _text with `getPasswordChar()` for rendering. The
    // underlying _text / selection / clipboard behaviour is unchanged.
    bool isPasswordMode() const { return _passwordMode; }
    void setPasswordMode(bool v);
    char getPasswordChar() const { return _passwordChar; }
    void setPasswordChar(char c);

    // Multi-line: Enter inserts '\n' and Up/Down arrows navigate across
    // lines; off by default so single-line forms keep the "Enter = blur"
    // behaviour. Only hard-wraps on explicit newlines — auto word-wrap
    // is deferred until a demand surfaces.
    bool isMultiLine() const { return _multiLine; }
    void setMultiLine(bool v) { _multiLine = v; }

    void focus();
    void blur();
    bool isFocused() const { return _focused; }

    // Caret byte index into _text. P5e-1 adds navigation so this can be
    // anywhere in [0, _text.size()], not just at the end. Accessors let
    // callers query/move programmatically without routing through key
    // events.
    size_t getCaretIndex() const { return _caretIdx; }
    void   setCaretIndex(size_t i);

    // Selection range [min(anchor, caret), max(...)). When anchor ==
    // caret there's no selection. Selection is collapsed by non-shift
    // movement, a fresh click, or successful insertion/deletion.
    size_t getSelectionStart() const;
    size_t getSelectionEnd() const;
    bool   hasSelection() const { return _selAnchor != _caretIdx; }
    void   clearSelection();

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
    void resetCaretBlink();
    void hideCaret();
    void showCaret();
    // Convert a window-space point (e.g. from MouseEvent) to the byte
    // offset in _text whose gap is closest to that point. Used by click-
    // to-place-caret.
    size_t windowPointToCaretIndex(float winX, float winY) const;

    // Selection helpers.
    void deleteSelection();
    void updateSelectionHighlight();
    // Measure the pixel X of the gap just before the byte at `idx` in the
    // label's local space (same frame updateCaretPos uses). `idx` is a
    // byte offset into the *authoritative* _text.
    float caretXAt(size_t idx) const;

    // Build the label's rendered string — applies password masking, then
    // inserts the IME composition preview at the caret. Returns the byte
    // offset of the caret inside the rendered string.
    ccstd::string renderedText(size_t &outCaretByte) const;

    // Multi-line caret helpers. Walk _text, counting '\n' to locate the
    // line containing `_caretIdx`; build a vector<(startByte, lineLen)>
    // index and compute the current column advance in pixels.
    void rebuildLineIndex(ccstd::vector<std::pair<size_t, size_t>> &out) const;
    size_t lineIndexOf(const ccstd::vector<std::pair<size_t, size_t>> &lines,
                       size_t byteIdx) const;
    float  columnAdvance(size_t lineStart, size_t byteIdx) const;
    size_t byteIdxAtColumn(size_t lineStart, size_t lineLen, float targetX) const;

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
    // Live IME composition string. Non-empty while the user is typing
    // pinyin/kana; cleared when the IME commits (TextInput arrives) or
    // when SDL sends an SDL_TEXTEDITING with empty text. Rendered
    // inline at the caret position in brackets so the preview is
    // distinguishable from committed text.
    ccstd::string _composition;
    size_t _caretIdx{0};  // byte offset into _text
    // Anchor end of the selection. When == _caretIdx, nothing is selected.
    // Shift+move extends the selection; plain move / click collapses it.
    size_t _selAnchor{0};
    // While the primary button is held down on our node, pointer motion
    // continuously rewrites _caretIdx to grow the selection from
    // _selAnchor — mirrors the desktop text-box drag-select behaviour.
    bool   _mouseDragging{false};
    // Tracks the last modifier state seen on a keyboard event so we can
    // suppress the SDL_TEXTINPUT that sometimes trails Ctrl+letter combos
    // (e.g. Ctrl+V on some Windows layouts emits a 'v' alongside the
    // clipboard-paste keypress).
    bool   _ctrlDown{false};
    bool _focused{false};
    bool _caretVisible{true};
    float _caretTimer{0.f};
    static constexpr float kCaretBlinkPeriod = 0.5f;

    BmfFont *_font{nullptr};

    // Cached children (looked up once at onEnable).
    Label  *_label{nullptr};
    Sprite *_caret{nullptr};
    Sprite *_selHighlight{nullptr};  // drawn behind the label when there's a selection

    Color _textColor{230, 230, 230, 255};
    Color _placeholderColor{130, 130, 130, 255};
    Color _selectionColor{80, 140, 210, 180};

    bool _passwordMode{false};
    char _passwordChar{'*'};
    bool _multiLine{false};
    // Remembered X pixel for Up/Down navigation. Preserves the user's
    // intended column when moving through a line shorter than the
    // starting column.
    float _preferredCaretX{0.f};
    bool  _preferredCaretXValid{false};

    TextChangedHandler _onChanged;
};

}  // namespace cc
