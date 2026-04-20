#include "cocos/ui/editBox/EditBox.h"

#include <cstdio>

#include "base/Log.h"
#include "cocos/2d/components/Label.h"
#include "cocos/2d/components/Sprite.h"
#include "cocos/2d/framework/UITransform.h"
#include "cocos/2d/text/TextFont.h"
#include "cocos/ui/editBox/EditBoxImpl.h"
#include "cocos/ui/editBox/EditBoxImplBase.h"
#include "core/scene-graph/Node.h"

namespace cc {

CC_IMPLEMENT_CLASS(EditBox, "cc.EditBox", Component)
    .property("_string",          &EditBox::_string)
    .property("_placeholder",     &EditBox::_placeholder)
    .property("_inputFlag",       &EditBox::_inputFlag,   InputFlag::DEFAULT)
    .property("_inputMode",       &EditBox::_inputMode,   InputMode::ANY)
    .property("_returnType",      &EditBox::_returnType,  KeyboardReturnType::DEFAULT)
    .property("_maxLength",       &EditBox::_maxLength,   20)
    .property("_tabIndex",        &EditBox::_tabIndex,    0)
    // Editor-authored event arrays — names match upstream verbatim so
    // JSON round-trips cleanly.
    .property("editingDidBegan",  &EditBox::_editingDidBegan)
    .property("textChanged",      &EditBox::_textChanged)
    .property("editingDidEnded",  &EditBox::_editingDidEnded)
    .property("editingReturn",    &EditBox::_editingReturn)
CC_END_CLASS(EditBox);

// Node-event subscription bundle. Heap-allocated so the header doesn't
// need to pull in EventTarget templates for every include site.
struct EditBox::EventIds {
    cc::event::TargetEventID<Node::MouseDown>   mouseDown;
    cc::event::TargetEventID<Node::TouchEnd>    touchEnd;
    cc::event::TargetEventID<Node::SizeChanged> sizeChanged;
};

namespace {

bool isUtf8Cont(unsigned char c) { return (c & 0xC0) == 0x80; }

size_t codepointCount(const ccstd::string &s) {
    size_t n = 0;
    for (unsigned char c : s) if (!isUtf8Cont(c)) ++n;
    return n;
}

size_t byteOffsetOfCodepoint(const ccstd::string &s, size_t cp) {
    size_t cur = 0, cnt = 0;
    while (cur < s.size() && cnt < cp) {
        // Advance one codepoint.
        ++cur;
        while (cur < s.size() && isUtf8Cont(static_cast<unsigned char>(s[cur]))) ++cur;
        ++cnt;
    }
    return cur;
}

// TS `_updateLabelStringStyle` — PASSWORD masks each codepoint with the
// sentinel char; CAPS_* applies the equivalent transform.
char asciiUpper(char c) { return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 32) : c; }

ccstd::string upperAll(const ccstd::string &s) {
    ccstd::string o = s;
    for (auto &c : o) c = asciiUpper(c);
    return o;
}

ccstd::string upperFirst(const ccstd::string &s) {
    if (s.empty()) return s;
    ccstd::string o = s;
    o[0] = asciiUpper(o[0]);
    return o;
}

ccstd::string upperEachWord(const ccstd::string &s) {
    ccstd::string o = s;
    bool capitaliseNext = true;
    for (size_t i = 0; i < o.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(o[i]);
        if (c == ' ' || c == '\t' || c == '\n') {
            capitaliseNext = true;
        } else if (capitaliseNext) {
            o[i] = asciiUpper(o[i]);
            capitaliseNext = false;
        }
    }
    return o;
}

// PASSWORD mask — match the impl's choice so display and caret math agree.
constexpr char kPasswordMask = '*';

}  // namespace

// ────────────────────────────────────────────────────────────────────────
// Lifecycle
// ────────────────────────────────────────────────────────────────────────

EditBox::~EditBox() {
    // onDestroy should have run, but be safe for test harnesses that
    // tear down directly.
    if (_impl) {
        _impl->clear();
        delete _impl;
        _impl = nullptr;
    }
    if (_eventIds) {
        delete _eventIds;
        _eventIds = nullptr;
    }
}

void EditBox::onLoad() {
    _init();
}

void EditBox::onEnable() {
    _registerEvent();
    _ensureBackgroundSprite();
    if (_impl) _impl->onEnable();
}

void EditBox::onDisable() {
    _unregisterEvent();
    if (_impl) _impl->onDisable();
}

void EditBox::onDestroy() {
    if (_impl) {
        _impl->clear();
        delete _impl;
        _impl = nullptr;
    }
    if (_eventIds) {
        delete _eventIds;
        _eventIds = nullptr;
    }
}

void EditBox::update(float dt) {
    if (_impl) _impl->tick(dt);
}

// ────────────────────────────────────────────────────────────────────────
// TS-style init / child-node setup
// ────────────────────────────────────────────────────────────────────────

void EditBox::_init() {
    if (!getNode()) return;
    _updateTextLabel();
    _updatePlaceholderLabel();
    _isLabelVisible = true;

    if (!_impl) {
        _impl = new EditBoxImpl();
        _impl->init(this);
    }

    _updateString(_string);
    _syncSize();
}

void EditBox::_ensureBackgroundSprite() {
    auto *node = getNode();
    if (!node) return;
    if (!_background) {
        _background = node->getComponent<Sprite>();
        if (!_background) {
            _background = node->addComponent<Sprite>();
        }
    }
}

void EditBox::_updateTextLabel() {
    auto *node = getNode();
    if (!node) return;

    if (_textLabel) {
        // Already wired (e.g. set via setTextLabel before onLoad). Just
        // refresh font / color / text.
        if (_font) _textLabel->setFont(_font);
        _textLabel->setColor(_textColor);
        _textLabel->setText(_applyDisplayStyle(_string));
        return;
    }

    // Reuse existing child if present (scene-deserialise re-enters onLoad).
    if (Node *existing = node->getChildByName("TEXT_LABEL")) {
        if (!existing->getComponent<UITransform>()) existing->addComponent<UITransform>();
        _textLabel = existing->getComponent<Label>();
        if (!_textLabel) _textLabel = existing->addComponent<Label>();
        if (_font) _textLabel->setFont(_font);
        _textLabel->setText(_applyDisplayStyle(_string));
        _ownsTextLabelNode = false;  // Editor-authored: respect its layout
        return;
    }

    // Fresh creation — detached so setFont / setColor land BEFORE the
    // Label's first onEnable. Label::updateGeometry bails when font is
    // missing, and the resulting empty vertex buffer lingered after
    // attach. Setting font-first sidesteps that quirk.
    auto *labelNode = ccnew Node("TEXT_LABEL");
    labelNode->addComponent<UITransform>();
    _textLabel = labelNode->addComponent<Label>();
    if (_font) _textLabel->setFont(_font);
    _textLabel->setColor(_textColor);
    _textLabel->setText(_applyDisplayStyle(_string));
    node->addChild(labelNode);
    _ownsTextLabelNode = true;
}

void EditBox::_updatePlaceholderLabel() {
    auto *node = getNode();
    if (!node) return;

    // Reuse path: only push our own _font so rendering works. Leave the
    // Label's color / text alone — Editor-authored prefabs set both on
    // the PLACEHOLDER_LABEL directly, and our `_placeholder` /
    // `_placeholderColor` are empty-string / default until the caller
    // explicitly invokes setPlaceholder / setPlaceholderColor.
    auto applyProgrammaticStyle = [this](Label *lbl) {
        if (_font) lbl->setFont(_font);
        if (!_placeholder.empty()) lbl->setText(_placeholder);
    };

    if (_placeholderLabel) {
        applyProgrammaticStyle(_placeholderLabel);
        return;
    }

    if (Node *existing = node->getChildByName("PLACEHOLDER_LABEL")) {
        if (!existing->getComponent<UITransform>()) existing->addComponent<UITransform>();
        _placeholderLabel = existing->getComponent<Label>();
        if (!_placeholderLabel) _placeholderLabel = existing->addComponent<Label>();
        applyProgrammaticStyle(_placeholderLabel);
        _ownsPlaceholderLabelNode = false;
        return;
    }

    auto *labelNode = ccnew Node("PLACEHOLDER_LABEL");
    labelNode->addComponent<UITransform>();
    _placeholderLabel = labelNode->addComponent<Label>();
    if (_font) _placeholderLabel->setFont(_font);
    _placeholderLabel->setColor(_placeholderColor);
    _placeholderLabel->setText(_placeholder);
    node->addChild(labelNode);
    _ownsPlaceholderLabelNode = true;
}

void EditBox::_updateLabels() {
    // Matches TS _updateLabels: placeholder visible when empty,
    // textLabel visible when non-empty. Only effective while
    // _isLabelVisible (i.e. the impl hasn't called _hideLabels).
    if (!_isLabelVisible) return;
    const bool empty = _string.empty();
    if (_textLabel) {
        _textLabel->getNode()->setActive(!empty);
    }
    if (_placeholderLabel) {
        _placeholderLabel->getNode()->setActive(empty);
    }
}

void EditBox::_updateString(const ccstd::string &text) {
    if (_textLabel) {
        _textLabel->setText(_applyDisplayStyle(text));
    }
    _updateLabels();
}

ccstd::string EditBox::_applyDisplayStyle(const ccstd::string &text,
                                            bool ignorePassword) const {
    switch (_inputFlag) {
        case InputFlag::PASSWORD:
            if (ignorePassword) return text;
            {
                const size_t cps = codepointCount(text);
                return ccstd::string(cps, kPasswordMask);
            }
        case InputFlag::INITIAL_CAPS_ALL_CHARACTERS: return upperAll(text);
        case InputFlag::INITIAL_CAPS_WORD:           return upperEachWord(text);
        case InputFlag::INITIAL_CAPS_SENTENCE:       return upperFirst(text);
        case InputFlag::SENSITIVE:
        case InputFlag::DEFAULT:
        default:
            return text;
    }
}

void EditBox::_syncSize() {
    auto *node = getNode();
    if (!node) return;
    auto *ui = node->getComponent<UITransform>();
    if (!ui) return;
    const Vec2 &sz = ui->getContentSize();

    if (_background) {
        if (auto *bgui = _background->getNode()->getComponent<UITransform>()) {
            bgui->setContentSize(sz);
        }
        _background->setSize(sz.x, sz.y);
    }

    // Labels we CREATED (programmatic EditBox) get centred on the node
    // origin and stretched to the EditBox content size. Editor-authored
    // labels carry their own lpos / anchor / contentSize — respect them.
    auto place = [&](Label *lbl, bool ownsNode) {
        if (!lbl || !ownsNode) return;
        auto *ln = lbl->getNode();
        ln->setPosition(Vec3{0.f, 0.f, 0.f});
        if (auto *lui = ln->getComponent<UITransform>()) {
            lui->setContentSize(sz);
        }
    };
    place(_textLabel,        _ownsTextLabelNode);
    place(_placeholderLabel, _ownsPlaceholderLabelNode);

    if (_impl) _impl->setSize(sz.x, sz.y);
}

void EditBox::_resizeChildNodes() {
    _syncSize();
}

// ────────────────────────────────────────────────────────────────────────
// Public setters — mirror TS accessor semantics
// ────────────────────────────────────────────────────────────────────────

void EditBox::setString(const ccstd::string &value) {
    ccstd::string v = value;
    if (_maxLength >= 0) {
        const size_t cpLimit = static_cast<size_t>(_maxLength);
        if (codepointCount(v) > cpLimit) {
            v = v.substr(0, byteOffsetOfCodepoint(v, cpLimit));
        }
    }
    if (_string == v) return;
    _string = v;
    _updateString(_string);
}

void EditBox::setPlaceholder(const ccstd::string &value) {
    if (_placeholder == value) return;
    _placeholder = value;
    if (_placeholderLabel) _placeholderLabel->setText(_placeholder);
    _updateLabels();
}

void EditBox::setTextLabel(Label *v) {
    if (_textLabel == v) return;
    _textLabel = v;
    if (_textLabel) {
        if (_font) _textLabel->setFont(_font);
        _textLabel->setColor(_textColor);
        _updateString(_string);
        _updateLabels();
    }
}

void EditBox::setPlaceholderLabel(Label *v) {
    if (_placeholderLabel == v) return;
    _placeholderLabel = v;
    if (_placeholderLabel) {
        if (_font) _placeholderLabel->setFont(_font);
        _placeholderLabel->setColor(_placeholderColor);
        _placeholderLabel->setText(_placeholder);
        _updateLabels();
    }
}

void EditBox::setInputFlag(InputFlag v) {
    if (_inputFlag == v) return;
    _inputFlag = v;
    _updateString(_string);
}

void EditBox::setInputMode(InputMode v) {
    if (_inputMode == v) return;
    _inputMode = v;
    _updateTextLabel();
    _updatePlaceholderLabel();
}

void EditBox::setTabIndex(int32_t v) {
    if (_tabIndex == v) return;
    _tabIndex = v;
    if (_impl) _impl->setTabIndex(v);
}

void EditBox::setFont(TextFont *f) {
    _font = f;
    if (_textLabel)        _textLabel->setFont(f);
    if (_placeholderLabel) _placeholderLabel->setFont(f);
}

// ────────────────────────────────────────────────────────────────────────
// Focus control — forward to impl
// ────────────────────────────────────────────────────────────────────────

void EditBox::setFocus() { if (_impl) _impl->setFocus(true); }
void EditBox::focus()    { if (_impl) _impl->setFocus(true); }
void EditBox::blur()     { if (_impl) _impl->setFocus(false); }

bool EditBox::isFocused() const { return _impl ? _impl->isFocused() : false; }

size_t EditBox::getCaretIndex() const {
    auto *sdl = static_cast<EditBoxImpl *>(_impl);
    return sdl ? sdl->getCaretIndex() : 0;
}

void EditBox::setCaretIndex(size_t i) {
    if (auto *sdl = static_cast<EditBoxImpl *>(_impl)) sdl->setCaretIndex(i);
}

bool EditBox::hasSelection() const {
    auto *sdl = static_cast<EditBoxImpl *>(_impl);
    return sdl ? sdl->hasSelection() : false;
}

size_t EditBox::getSelectionStart() const {
    auto *sdl = static_cast<EditBoxImpl *>(_impl);
    return sdl ? sdl->getSelectionStart() : 0;
}

size_t EditBox::getSelectionEnd() const {
    auto *sdl = static_cast<EditBoxImpl *>(_impl);
    return sdl ? sdl->getSelectionEnd() : 0;
}

void EditBox::clearSelection() {
    if (auto *sdl = static_cast<EditBoxImpl *>(_impl)) sdl->clearSelection();
}

// ────────────────────────────────────────────────────────────────────────
// Impl-facing hooks — match TS @deprecated-public signatures
// ────────────────────────────────────────────────────────────────────────

void EditBox::_editBoxEditingDidBegan() {
    reflection::MethodArgs args;
    args.push_back(reflection::MethodArg::makePointer(this));
    ComponentEventHandler::emitEvents(_editingDidBegan, args);

    auto snap = _runtimeEditingDidBegan;
    for (auto &fn : snap) fn(this);
    if (auto *n = getNode()) n->dispatchEvent<Node::EditBoxBegan>(this);
}

void EditBox::_editBoxEditingDidEnded(const ccstd::string * /*text*/) {
    // The optional `text` param carries a platform-filtered string on
    // ByteDance minigame; desktop gets nullptr. Both the handler vector
    // and the Node event fire in the TS-documented order.
    reflection::MethodArgs args;
    args.push_back(reflection::MethodArg::makePointer(this));
    ComponentEventHandler::emitEvents(_editingDidEnded, args);

    auto snap = _runtimeEditingDidEnded;
    for (auto &fn : snap) fn(this);
    if (auto *n = getNode()) n->dispatchEvent<Node::EditBoxEnded>(this);
}

void EditBox::_editBoxTextChanged(const ccstd::string &text) {
    // Apply the CAPS transform before writing back — TS authoritative
    // string reflects the case-change, not the raw keystroke stream.
    // `ignorePassword=true` keeps the mask out of the stored string.
    const ccstd::string styled = _applyDisplayStyle(text, /*ignorePassword*/ true);
    // Go through setString so maxLength + display refresh re-run. It's
    // a no-op if the string is unchanged from the impl's last write.
    setString(styled);

    // Upstream textChanged.emit signature: (text, this) — text is the first
    // positional. Runtime C++ listeners still use the (EditBox*) shape.
    reflection::MethodArgs args;
    args.push_back(reflection::MethodArg::makeString(_string));
    args.push_back(reflection::MethodArg::makePointer(this));
    ComponentEventHandler::emitEvents(_textChanged, args);

    auto snap = _runtimeTextChanged;
    for (auto &fn : snap) fn(this);
    if (auto *n = getNode()) n->dispatchEvent<Node::EditBoxTextChanged>(this);
}

void EditBox::_editBoxEditingReturn(const ccstd::string * /*text*/) {
    reflection::MethodArgs args;
    args.push_back(reflection::MethodArg::makePointer(this));
    ComponentEventHandler::emitEvents(_editingReturn, args);

    auto snap = _runtimeEditingReturn;
    for (auto &fn : snap) fn(this);
    if (auto *n = getNode()) n->dispatchEvent<Node::EditBoxReturn>(this);
}

void EditBox::_showLabels() {
    _isLabelVisible = true;
    _updateLabels();
}

void EditBox::_hideLabels() {
    _isLabelVisible = false;
    if (_textLabel)        _textLabel->getNode()->setActive(false);
    if (_placeholderLabel) _placeholderLabel->getNode()->setActive(false);
}

void EditBox::_impl_refreshDisplay() {
    _updateString(_string);
}

// ────────────────────────────────────────────────────────────────────────
// Node-level event wiring (click-to-focus + resize)
// ────────────────────────────────────────────────────────────────────────

void EditBox::_registerEvent() {
    auto *node = getNode();
    if (!node) return;
    if (_eventIds) return;  // already registered
    _eventIds = new EventIds();

    _eventIds->mouseDown = node->on<Node::MouseDown>(
        [this](Node *, const NodeMouseEventArg &arg) {
            if (arg.button != 0) return;
            _onNodeClick(arg.x, arg.y);
        });

    _eventIds->touchEnd = node->on<Node::TouchEnd>(
        [this](Node *, const NodeTouchEventArg &arg) {
            _onNodeClick(arg.x, arg.y);
        });

    _eventIds->sizeChanged = node->on<Node::SizeChanged>(
        [this](Node *) { _resizeChildNodes(); });
}

void EditBox::_unregisterEvent() {
    auto *node = getNode();
    if (!_eventIds) return;
    if (node) {
        node->off<Node::MouseDown>(_eventIds->mouseDown);
        node->off<Node::TouchEnd>(_eventIds->touchEnd);
        node->off<Node::SizeChanged>(_eventIds->sizeChanged);
    }
    delete _eventIds;
    _eventIds = nullptr;
}

void EditBox::_onNodeClick(float localX, float localY) {
    if (auto *sdl = static_cast<EditBoxImpl *>(_impl)) {
        sdl->onDelegateNodeClick(localX, localY);
    }
}

}  // namespace cc
