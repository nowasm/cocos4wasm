#pragma once

#include <functional>

#include "base/Ptr.h"
#include "base/std/container/string.h"
#include "base/std/container/vector.h"
#include "cocos/ui/editBox/Types.h"
#include "core/component/Component.h"
#include "core/event/EventTarget.h"
#include "core/reflection/Reflection.h"
#include "core/scene-graph/ComponentEventHandler.h"
#include "math/Color.h"

namespace cc {

class EditBoxImplBase;
class Label;
class TextFont;
class Node;
class Sprite;

// Pure-C++ port of `cocos/ui/editbox/edit-box.ts`. Same property names,
// same enums, same event channels; the only divergence is language
// syntax (getters/setters instead of accessor properties) and the impl
// backend (SDL native instead of DOM <input>).
//
// Architecture mirrors TS:
//   EditBox          — this component. Owns serialised state (string,
//                      placeholder, flags, labels, background) and the
//                      public API surface. Delegates IO to _impl.
//   EditBoxImplBase  — platform backend contract. One impl is attached
//                      per EditBox, created once in onLoad.
//   EditBoxImpl      — SDL desktop backend. Binds bus listeners, drives
//                      caret + selection visuals.
//
// Child-node layout (auto-created on first onLoad):
//   editboxNode (UITransform + Sprite[background] + EditBox)
//     ├─ TEXT_LABEL           (Label — rendered input)
//     ├─ PLACEHOLDER_LABEL    (Label — shown when empty)
//     ├─ EDIT_CARET           (Sprite — managed by impl)
//     └─ EDIT_SELECTION       (Sprite — managed by impl)
//
// Event channels — both fire together, in the TS-documented order:
//   1. Component-level handler vectors
//        `editingDidBegan`, `textChanged`, `editingDidEnded`, `editingReturn`
//   2. Node typed events (Node.h)
//        `Node::EditBoxBegan / EditBoxEnded / EditBoxTextChanged /
//         EditBoxReturn`
class EditBox : public Component {
    CC_CLASS_DECL(EditBox, Component)
public:
    // Enum re-exports so user code can write `EditBox::InputMode::ANY`,
    // matching Creator's `EditBox.InputMode.ANY` idiom.
    using InputMode          = cc::InputMode;
    using InputFlag          = cc::InputFlag;
    using KeyboardReturnType = cc::KeyboardReturnType;

    // Event name constants — matches `EditBox.EventType` in TS. Useful
    // for Creator-style `node.on('editing-did-began', ...)` code paths
    // when a migrated scene references the string name directly.
    struct EventType {
        static constexpr const char *EDITING_DID_BEGAN = "editing-did-began";
        static constexpr const char *EDITING_DID_ENDED = "editing-did-ended";
        static constexpr const char *TEXT_CHANGED      = "text-changed";
        static constexpr const char *EDITING_RETURN    = "editing-return";
    };

    // Component-level callback handler. Each edit-event channel has two
    // parallel lists:
    //   • serialized `ComponentEventHandler[]` — populated from Editor JSON
    //   • runtime `std::function` listeners — registered programmatically
    // Both fire on every event, serialized first.
    using Handler = std::function<void(EditBox *)>;

    EditBox() { _wantsUpdate = true; }
    ~EditBox() override;

    // ── Component lifecycle ─────────────────────────────────────────────
    void onLoad() override;
    void onEnable() override;
    void onDisable() override;
    void onDestroy() override;
    void update(float dt) override;

    // ── Public properties (mirror TS getters/setters) ───────────────────

    const ccstd::string &getString() const { return _string; }
    void setString(const ccstd::string &value);

    const ccstd::string &getPlaceholder() const { return _placeholder; }
    void setPlaceholder(const ccstd::string &value);

    Label *getTextLabel() const { return _textLabel; }
    void   setTextLabel(Label *v);

    Label *getPlaceholderLabel() const { return _placeholderLabel; }
    void   setPlaceholderLabel(Label *v);

    InputFlag getInputFlag() const { return _inputFlag; }
    void      setInputFlag(InputFlag v);

    InputMode getInputMode() const { return _inputMode; }
    void      setInputMode(InputMode v);

    KeyboardReturnType getReturnType() const { return _returnType; }
    void               setReturnType(KeyboardReturnType v) { _returnType = v; }

    int32_t getMaxLength() const { return _maxLength; }
    void    setMaxLength(int32_t v) { _maxLength = v; }

    int32_t getTabIndex() const { return _tabIndex; }
    void    setTabIndex(int32_t v);

    // Background access — until P3 brings SpriteFrame assets, callers
    // wire a Sprite manually. EditBox auto-creates a plain Sprite on
    // the same node (matches TS `_ensureBackgroundSprite`).
    Sprite *getBackgroundSprite() const { return _background; }
    void    setBackgroundSprite(Sprite *s) { _background = s; }

    // Convenience: font shared by both labels. Not in TS (which uses
    // fontFamily strings + TTF asset); exposed for C++ BMFont workflow.
    void setFont(TextFont *f);

    // ── Focus control ───────────────────────────────────────────────────
    void setFocus();  // TS alias — delegates to impl.setFocus(true)
    void focus();
    void blur();
    bool isFocused() const;

    // ── Caret / selection (programmatic access passes through impl) ─────
    size_t getCaretIndex() const;
    void   setCaretIndex(size_t i);
    bool   hasSelection() const;
    size_t getSelectionStart() const;
    size_t getSelectionEnd() const;
    void   clearSelection();

    // ── Event subscription (C++ runtime listeners) ──────────────────────
    void addEditingDidBeganHandler(Handler fn) { _runtimeEditingDidBegan.push_back(std::move(fn)); }
    void addTextChangedHandler(Handler fn)     { _runtimeTextChanged.push_back(std::move(fn)); }
    void addEditingDidEndedHandler(Handler fn) { _runtimeEditingDidEnded.push_back(std::move(fn)); }
    void addEditingReturnHandler(Handler fn)   { _runtimeEditingReturn.push_back(std::move(fn)); }

    void clearEditingDidBeganHandlers() { _runtimeEditingDidBegan.clear(); }
    void clearTextChangedHandlers()     { _runtimeTextChanged.clear(); }
    void clearEditingDidEndedHandlers() { _runtimeEditingDidEnded.clear(); }
    void clearEditingReturnHandlers()   { _runtimeEditingReturn.clear(); }

    // ── Editor-authored event arrays (serialized) ───────────────────────
    ccstd::vector<IntrusivePtr<ComponentEventHandler>>       &getEditingDidBeganEvents()       { return _editingDidBegan; }
    const ccstd::vector<IntrusivePtr<ComponentEventHandler>> &getEditingDidBeganEvents() const { return _editingDidBegan; }
    ccstd::vector<IntrusivePtr<ComponentEventHandler>>       &getTextChangedEvents()       { return _textChanged; }
    const ccstd::vector<IntrusivePtr<ComponentEventHandler>> &getTextChangedEvents() const { return _textChanged; }
    ccstd::vector<IntrusivePtr<ComponentEventHandler>>       &getEditingDidEndedEvents()       { return _editingDidEnded; }
    const ccstd::vector<IntrusivePtr<ComponentEventHandler>> &getEditingDidEndedEvents() const { return _editingDidEnded; }
    ccstd::vector<IntrusivePtr<ComponentEventHandler>>       &getEditingReturnEvents()       { return _editingReturn; }
    const ccstd::vector<IntrusivePtr<ComponentEventHandler>> &getEditingReturnEvents() const { return _editingReturn; }

    // ── Impl hooks — public because the Impl needs to call them, TS
    //    uses the same `@deprecated public` pattern ──────────────────────

    // Fire the "began editing" handler vector + Node event. Called by
    // the impl from beginEditing.
    void _editBoxEditingDidBegan();

    // Fire the "did end" channel. `text` is an optional sensitive-word-
    // filtered version of the string (Creator's ByteDance minigame
    // platform supplies this); desktop passes nullptr.
    void _editBoxEditingDidEnded(const ccstd::string *text = nullptr);

    // Called from impl after the string content changed. Applies the
    // display-style transform (password mask handled separately) and
    // fires the text-changed handler vector + Node event.
    void _editBoxTextChanged(const ccstd::string &text);

    // Called on Enter key. Same optional `text` semantics as
    // _editBoxEditingDidEnded.
    void _editBoxEditingReturn(const ccstd::string *text = nullptr);

    // Toggle label-node-active state. TS uses these from EditBoxImpl
    // around the DOM show/hide dance. Our SDL impl keeps both labels
    // visible throughout editing (Label renders the text directly), so
    // these are effectively cosmetic hooks — provided for TS parity so
    // a future backend (e.g. WASM using a real DOM input) can override.
    void _showLabels();
    void _hideLabels();

    // Impl-side escape hatches — bypass `setString` to avoid the
    // maxLength re-clip loop when the impl is the authoritative source.
    ccstd::string &_impl_mutableString() { return _string; }
    void           _impl_refreshDisplay();

    // Render-for-display helper. Exposed so the impl can compute caret
    // advances against the same transformed string the Label shows.
    // `ignorePassword=true` returns the raw string minus CAPS transforms
    // (the impl uses this during caret-at-click math where counting by
    // input codepoints is needed).
    ccstd::string _applyDisplayStyle(const ccstd::string &text,
                                       bool ignorePassword = false) const;

private:
    // ── TS-mirroring internal helpers ───────────────────────────────────
    void _init();
    void _ensureBackgroundSprite();
    void _updateTextLabel();
    void _updatePlaceholderLabel();
    void _updateLabels();
    void _updateString(const ccstd::string &text);
    void _syncSize();
    void _resizeChildNodes();

    void _registerEvent();
    void _unregisterEvent();
    void _onNodeClick(float localX, float localY);

    // ── Serialised state ────────────────────────────────────────────────
    ccstd::string      _string;
    ccstd::string      _placeholder;
    Label             *_textLabel{nullptr};
    Label             *_placeholderLabel{nullptr};
    Sprite            *_background{nullptr};
    InputFlag          _inputFlag{InputFlag::DEFAULT};
    InputMode          _inputMode{InputMode::ANY};
    KeyboardReturnType _returnType{KeyboardReturnType::DEFAULT};
    int32_t            _maxLength{20};
    int32_t            _tabIndex{0};

    // ── Editor-serialized event arrays — match upstream `@serializable`
    //    names exactly (editingDidBegan / textChanged / ...) ──────────────
    ccstd::vector<IntrusivePtr<ComponentEventHandler>> _editingDidBegan;
    ccstd::vector<IntrusivePtr<ComponentEventHandler>> _textChanged;
    ccstd::vector<IntrusivePtr<ComponentEventHandler>> _editingDidEnded;
    ccstd::vector<IntrusivePtr<ComponentEventHandler>> _editingReturn;

    // ── C++ runtime subscribers ─────────────────────────────────────────
    ccstd::vector<Handler> _runtimeEditingDidBegan;
    ccstd::vector<Handler> _runtimeTextChanged;
    ccstd::vector<Handler> _runtimeEditingDidEnded;
    ccstd::vector<Handler> _runtimeEditingReturn;

    // ── Runtime ─────────────────────────────────────────────────────────
    EditBoxImplBase *_impl{nullptr};
    bool             _isLabelVisible{true};
    TextFont        *_font{nullptr};  // cached for later-created labels

    Color _textColor{230, 230, 230, 255};
    Color _placeholderColor{130, 130, 130, 255};

    // Node event subscription handles.
    struct EventIds;
    EventIds *_eventIds{nullptr};
};

}  // namespace cc
