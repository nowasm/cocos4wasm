#include "cocos/ui/components/EditBox.h"

#include <algorithm>
#include <cmath>

#include "base/Log.h"
#include "cocos/2d/components/Label.h"
#include "cocos/2d/components/Sprite.h"
#include "cocos/2d/framework/UITransform.h"
#include "cocos/2d/text/BmfFont.h"
#include "core/scene-graph/Node.h"
#include "engine/EngineEvents.h"
#include "math/Mat4.h"
#include "platform/SDLHelper.h"

namespace cc {

EditBox *EditBox::s_currentlyFocused = nullptr;

CC_IMPLEMENT_CLASS(EditBox, "cc.EditBox", Component)
    .property("_string",      &EditBox::_string)
    .property("_placeholder", &EditBox::_placeholder)
    .property("_inputFlag",   &EditBox::_inputFlag,   InputFlag::DEFAULT)
    .property("_inputMode",   &EditBox::_inputMode,   InputMode::ANY)
    .property("_returnType",  &EditBox::_returnType,  KeyboardReturnType::DEFAULT)
    .property("_maxLength",   &EditBox::_maxLength,   20)
    .property("_tabIndex",    &EditBox::_tabIndex,    0)
CC_END_CLASS(EditBox);

// SDL bus listener bundle. Heap-allocated to keep EditBox.h lean —
// clients including EditBox.h shouldn't need to pull engine/EngineEvents.h.
struct EditBox::Hooks {
    events::Mouse::Listener       mouseL;
    events::Keyboard::Listener    keyboardL;
    events::TextInput::Listener   textL;
    events::TextEditing::Listener editingL;
};

namespace {

// UTF-8 step helpers (mirrors TS usage — codepoint-by-codepoint nav).
bool isUtf8Cont(unsigned char c) { return (c & 0xC0) == 0x80; }

size_t prevCodepoint(const ccstd::string &s, size_t pos) {
    if (pos == 0) return 0;
    --pos;
    while (pos > 0 && isUtf8Cont(static_cast<unsigned char>(s[pos]))) --pos;
    return pos;
}

size_t nextCodepoint(const ccstd::string &s, size_t pos) {
    if (pos >= s.size()) return s.size();
    ++pos;
    while (pos < s.size() && isUtf8Cont(static_cast<unsigned char>(s[pos]))) ++pos;
    return pos;
}

// Count codepoints in `s` (same count TS's `str.length` produces for
// pure-ASCII; for multi-byte UTF-8 this matches JavaScript string
// length *for BMP codepoints*).
size_t codepointCount(const ccstd::string &s) {
    size_t n = 0;
    for (unsigned char c : s) if (!isUtf8Cont(c)) ++n;
    return n;
}

// Find the byte offset that matches the first `cp` codepoints of `s`.
size_t byteOffsetOfCodepoint(const ccstd::string &s, size_t cp) {
    size_t cur = 0, cnt = 0;
    while (cur < s.size() && cnt < cp) {
        cur = nextCodepoint(s, cur);
        ++cnt;
    }
    return cur;
}

// Nearest UITransform-bearing ancestor gives us the canvas size for
// screen → world conversion. Demo scenes put a 1280×720 rect on the
// Canvas node; real apps would hook this to resize events.
void nearestCanvasSize(Node *from, float &w, float &h) {
    w = 1280.f; h = 720.f;
    for (Node *n = from; n; n = n->getParent()) {
        if (auto *ui = n->getComponent<UITransform>()) {
            w = ui->getContentSize().x;
            h = ui->getContentSize().y;
            return;
        }
    }
}

// Measure glyph-advance from the start of `s` up through byte `cutByte`.
float measureAdvance(const BmfFont &font, const ccstd::string &s, size_t cutByte) {
    float out = 0.f;
    const size_t n = std::min(cutByte, s.size());
    for (size_t i = 0; i < n; ++i) {
        if (const auto *g = font.getGlyph(static_cast<unsigned char>(s[i]))) {
            out += g->xadvance;
        }
    }
    return out;
}

// Cocos Creator uses U+25CF (●) as the canonical password mask, but our
// default BMFont atlas only includes ASCII glyphs — rendering the UTF-8
// bullet would come out as empty squares. Fall back to '*' so masking
// actually shows something. A future BmfFont upgrade (Unicode codepoint
// decoding + fuller atlas) can swap this to the proper bullet.
const char *kPasswordMask = "*";

// Upper-case helpers matching TS's behaviour.
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

}  // namespace

// ────────────────────────────────────────────────────────────────────────
// Lifecycle
// ────────────────────────────────────────────────────────────────────────

void EditBox::onEnable() {
    _init();
    bindEventHooks();
}

void EditBox::onDisable() {
    if (_focused) blur();
    unbindEventHooks();
}

void EditBox::onDestroy() {
    if (_focused) blur();
    unbindEventHooks();
}

void EditBox::update(float dt) {
    if (!_focused || !_caret) return;
    _caretTimer += dt;
    if (_caretTimer >= kCaretBlinkPeriod) {
        _caretTimer = 0.f;
        _caretVisible = !_caretVisible;
        _caret->getNode()->setActive(_caretVisible);
    }
}

// ────────────────────────────────────────────────────────────────────────
// TS-style internal setup
// ────────────────────────────────────────────────────────────────────────

void EditBox::_init() {
    auto *node = getNode();
    if (!node) return;

    _ensureBackground();
    _updateTextLabel();
    _updatePlaceholderLabel();

    // Caret + selection highlight live on the EditBox node itself, drawn
    // on top of the labels. Creation is idempotent — a re-enable keeps
    // the existing children.
    if (!_caret) {
        Node *existing = node->getChildByName("EDIT_CARET");
        if (existing) _caret = existing->getComponent<Sprite>();
    }
    if (!_caret) {
        auto *cnode = ccnew Node("EDIT_CARET");
        auto *ui = cnode->addComponent<UITransform>();
        ui->setContentSize(2.f, 24.f);
        ui->setAnchorPoint(0.5f, 0.5f);
        _caret = cnode->addComponent<Sprite>();
        _caret->setSize(2.f, 24.f);
        _caret->setColor(_textColor);
        cnode->setActive(false);
        node->addChild(cnode);
    }

    if (!_selHighlight) {
        Node *existing = node->getChildByName("EDIT_SELECTION");
        if (existing) _selHighlight = existing->getComponent<Sprite>();
    }
    if (!_selHighlight) {
        auto *sn = ccnew Node("EDIT_SELECTION");
        auto *ui = sn->addComponent<UITransform>();
        ui->setContentSize(0.f, 24.f);
        ui->setAnchorPoint(0.f, 0.5f);
        _selHighlight = sn->addComponent<Sprite>();
        _selHighlight->setSize(0.f, 24.f);
        _selHighlight->setColor(_selectionColor);
        sn->setActive(false);
        node->addChild(sn);
    }

    _isLabelVisible = true;
    _updateString(_string);
    _syncSize();
    updateCaretPos();
    updateSelectionHighlight();
}

void EditBox::_ensureBackground() {
    auto *node = getNode();
    if (!node) return;
    if (_background) return;
    _background = node->getComponent<Sprite>();
    if (!_background) {
        _background = node->addComponent<Sprite>();
    }
}

void EditBox::_updateTextLabel() {
    auto *node = getNode();
    if (!node) return;

    if (!_textLabel) {
        Node *labelNode = node->getChildByName("TEXT_LABEL");
        if (!labelNode) {
            labelNode = ccnew Node("TEXT_LABEL");
            node->addChild(labelNode);
        }
        auto *ui = labelNode->getComponent<UITransform>();
        if (!ui) ui = labelNode->addComponent<UITransform>();
        _textLabel = labelNode->getComponent<Label>();
        if (!_textLabel) _textLabel = labelNode->addComponent<Label>();
    }
    if (_font) _textLabel->setFont(_font);
    _textLabel->setColor(_textColor);
    _textLabel->setText(_updateLabelStringStyle(_string));
}

void EditBox::_updatePlaceholderLabel() {
    auto *node = getNode();
    if (!node) return;

    if (!_placeholderLabel) {
        Node *labelNode = node->getChildByName("PLACEHOLDER_LABEL");
        if (!labelNode) {
            labelNode = ccnew Node("PLACEHOLDER_LABEL");
            node->addChild(labelNode);
        }
        auto *ui = labelNode->getComponent<UITransform>();
        if (!ui) ui = labelNode->addComponent<UITransform>();
        _placeholderLabel = labelNode->getComponent<Label>();
        if (!_placeholderLabel) _placeholderLabel = labelNode->addComponent<Label>();
    }
    if (_font) _placeholderLabel->setFont(_font);
    _placeholderLabel->setColor(_placeholderColor);
    _placeholderLabel->setText(_placeholder);
}

void EditBox::_updateLabels() {
    // Empty string → placeholder visible, text hidden (and vice versa).
    // Matches TS: `textLabel.node.active = (content !== '')`.
    if (!_isLabelVisible) return;
    const bool empty = _string.empty();
    if (_textLabel)        _textLabel->getNode()->setActive(!empty);
    if (_placeholderLabel) _placeholderLabel->getNode()->setActive(empty);
}

void EditBox::_updateString(const ccstd::string &text) {
    if (!_textLabel) return;
    _textLabel->setText(_updateLabelStringStyle(text));
    _updateLabels();
}

ccstd::string EditBox::_updateLabelStringStyle(const ccstd::string &text,
                                                 bool ignorePassword) const {
    switch (_inputFlag) {
        case InputFlag::PASSWORD:
            if (ignorePassword) return text;
            {
                ccstd::string out;
                out.reserve(codepointCount(text) * 3);
                const size_t cps = codepointCount(text);
                for (size_t i = 0; i < cps; ++i) out.append(kPasswordMask);
                return out;
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

    // Labels sit centred inside the edit box — Label itself centres the
    // glyph run on its node's origin, so placing the label node at (0,0)
    // works. Multi-line labels grow above/below this baseline.
    auto placeLabelNode = [&](Label *lbl) {
        if (!lbl) return;
        auto *ln = lbl->getNode();
        ln->setPosition(Vec3{0.f, 0.f, 0.f});
        if (auto *lui = ln->getComponent<UITransform>()) {
            lui->setContentSize(sz);
        }
    };
    placeLabelNode(_textLabel);
    placeLabelNode(_placeholderLabel);
}

// ────────────────────────────────────────────────────────────────────────
// Public setters — each re-renders as needed.
// ────────────────────────────────────────────────────────────────────────

void EditBox::setString(const ccstd::string &value) {
    ccstd::string v = value;
    if (_maxLength >= 0) {
        const size_t cpLimit = static_cast<size_t>(_maxLength);
        const size_t cpCount = codepointCount(v);
        if (cpCount > cpLimit) {
            v = v.substr(0, byteOffsetOfCodepoint(v, cpLimit));
        }
    }
    if (_string == v) return;
    _string = v;
    if (_caretIdx > _string.size()) _caretIdx = _string.size();
    _selAnchor = _caretIdx;
    _updateString(_string);
    updateCaretPos();
    updateSelectionHighlight();
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
        _updateString(_string);
    }
}

void EditBox::setPlaceholderLabel(Label *v) {
    if (_placeholderLabel == v) return;
    _placeholderLabel = v;
    if (_placeholderLabel) {
        if (_font) _placeholderLabel->setFont(_font);
        _placeholderLabel->setText(_placeholder);
        _updateLabels();
    }
}

void EditBox::setInputFlag(InputFlag v) {
    if (_inputFlag == v) return;
    _inputFlag = v;
    _updateString(_string);
    updateCaretPos();
}

void EditBox::setInputMode(InputMode v) {
    if (_inputMode == v) return;
    _inputMode = v;
    _updateTextLabel();
    _updatePlaceholderLabel();
    updateCaretPos();
}

void EditBox::setFont(BmfFont *f) {
    _font = f;
    if (_textLabel)        _textLabel->setFont(f);
    if (_placeholderLabel) _placeholderLabel->setFont(f);
    updateCaretPos();
}

// ────────────────────────────────────────────────────────────────────────
// Focus
// ────────────────────────────────────────────────────────────────────────

void EditBox::focus() {
    if (_focused) return;
    if (s_currentlyFocused && s_currentlyFocused != this) {
        s_currentlyFocused->blur();
    }
    _focused = true;
    s_currentlyFocused = this;
    SDLHelper::startTextInput();
    _caretVisible = true;
    _caretTimer   = 0.f;
    if (_caret) _caret->getNode()->setActive(true);
    fireEditingDidBegan();
}

void EditBox::blur() {
    if (!_focused) return;
    _focused = false;
    if (s_currentlyFocused == this) {
        s_currentlyFocused = nullptr;
        SDLHelper::stopTextInput();
    }
    if (_caret)        _caret->getNode()->setActive(false);
    if (_selHighlight) _selHighlight->getNode()->setActive(false);
    _mouseDragging = false;
    _composition.clear();
    _updateString(_string);
    fireEditingDidEnded();
}

// ────────────────────────────────────────────────────────────────────────
// Event emission — both component handler vector + Node typed event.
// ────────────────────────────────────────────────────────────────────────

void EditBox::fireEditingDidBegan() {
    auto snap = _editingDidBegan;
    for (auto &fn : snap) fn(this);
    if (auto *n = getNode()) n->dispatchEvent<Node::EditBoxBegan>(this);
}
void EditBox::fireTextChanged() {
    auto snap = _textChanged;
    for (auto &fn : snap) fn(this);
    if (auto *n = getNode()) n->dispatchEvent<Node::EditBoxTextChanged>(this);
}
void EditBox::fireEditingDidEnded() {
    auto snap = _editingDidEnded;
    for (auto &fn : snap) fn(this);
    if (auto *n = getNode()) n->dispatchEvent<Node::EditBoxEnded>(this);
}
void EditBox::fireEditingReturn() {
    auto snap = _editingReturn;
    for (auto &fn : snap) fn(this);
    if (auto *n = getNode()) n->dispatchEvent<Node::EditBoxReturn>(this);
}

// ────────────────────────────────────────────────────────────────────────
// Engine-bus event subscriptions
// ────────────────────────────────────────────────────────────────────────

void EditBox::bindEventHooks() {
    if (_hooks) return;
    _hooks = new Hooks();

    _hooks->mouseL.bind([this](const MouseEvent &ev) {
        if (ev.button != 0 && ev.type != MouseEvent::Type::MOVE) return;

        if (ev.type == MouseEvent::Type::DOWN) {
            const bool inside = pointInside(ev.x, ev.y);
            if (inside) {
                if (!_focused) focus();
                _caretIdx = windowPointToCaretIndex(ev.x, ev.y);
                _selAnchor = _caretIdx;
                _mouseDragging = true;
                _preferredCaretXValid = false;
                updateCaretPos();
                updateSelectionHighlight();
                resetCaretBlink();
            } else if (_focused) {
                blur();
            }
            return;
        }

        if (ev.type == MouseEvent::Type::MOVE && _mouseDragging && _focused) {
            _caretIdx = windowPointToCaretIndex(ev.x, ev.y);
            updateCaretPos();
            updateSelectionHighlight();
            return;
        }

        if (ev.type == MouseEvent::Type::UP) {
            _mouseDragging = false;
            return;
        }
    });

    _hooks->keyboardL.bind([this](const KeyboardEvent &ev) {
        _ctrlDown = ev.ctrlKeyActive;
        if (!_focused) return;
        if (ev.action != KeyboardEvent::Action::PRESS) return;

        constexpr int KEY_BACKSPACE = 8;
        constexpr int KEY_ENTER     = 13;
        constexpr int KEY_ESCAPE    = 27;
        constexpr int KEY_END       = 35;
        constexpr int KEY_HOME      = 36;
        constexpr int KEY_LEFT      = 37;
        constexpr int KEY_UP        = 38;
        constexpr int KEY_RIGHT     = 39;
        constexpr int KEY_DOWN      = 40;
        constexpr int KEY_DELETE    = 46;

        const bool shift = ev.shiftKeyActive;
        const bool ctrl  = ev.ctrlKeyActive;

        auto move = [&](size_t newIdx) {
            _caretIdx = newIdx;
            if (!shift) _selAnchor = _caretIdx;
            updateCaretPos();
            updateSelectionHighlight();
            resetCaretBlink();
        };

        // Clipboard + select-all bypass the normal switch.
        if (ctrl) {
            switch (ev.key) {
                case 'A':
                    _selAnchor = 0;
                    _caretIdx  = _string.size();
                    updateCaretPos();
                    updateSelectionHighlight();
                    resetCaretBlink();
                    return;
                case 'C':
                    if (hasSelection()) {
                        SDLHelper::setClipboardText(
                            _string.substr(getSelectionStart(),
                                           getSelectionEnd() - getSelectionStart()));
                    }
                    return;
                case 'X':
                    if (hasSelection()) {
                        SDLHelper::setClipboardText(
                            _string.substr(getSelectionStart(),
                                           getSelectionEnd() - getSelectionStart()));
                        deleteSelection();
                        fireTextChanged();
                    }
                    return;
                case 'V': {
                    ccstd::string clip = SDLHelper::getClipboardText();
                    if (clip.empty()) return;
                    if (hasSelection()) deleteSelection();
                    // Strip newlines in single-line modes.
                    if (_inputMode != InputMode::ANY) {
                        clip.erase(std::remove(clip.begin(), clip.end(), '\n'), clip.end());
                    }
                    // Enforce maxLength.
                    if (_maxLength >= 0) {
                        const size_t budget = static_cast<size_t>(_maxLength)
                                               - codepointCount(_string);
                        if (budget == 0) return;
                        if (codepointCount(clip) > budget) {
                            clip = clip.substr(0, byteOffsetOfCodepoint(clip, budget));
                        }
                    }
                    _string.insert(_caretIdx, clip);
                    _caretIdx += clip.size();
                    _selAnchor = _caretIdx;
                    _updateString(_string);
                    updateCaretPos();
                    updateSelectionHighlight();
                    resetCaretBlink();
                    fireTextChanged();
                    return;
                }
                default:
                    return;
            }
        }

        switch (ev.key) {
            case KEY_BACKSPACE:
                if (hasSelection()) {
                    deleteSelection();
                    fireTextChanged();
                } else if (_caretIdx > 0) {
                    const size_t prev = prevCodepoint(_string, _caretIdx);
                    _string.erase(prev, _caretIdx - prev);
                    _caretIdx = prev;
                    _selAnchor = _caretIdx;
                    _updateString(_string);
                    updateCaretPos();
                    updateSelectionHighlight();
                    fireTextChanged();
                }
                break;
            case KEY_DELETE:
                if (hasSelection()) {
                    deleteSelection();
                    fireTextChanged();
                } else if (_caretIdx < _string.size()) {
                    const size_t next = nextCodepoint(_string, _caretIdx);
                    _string.erase(_caretIdx, next - _caretIdx);
                    _updateString(_string);
                    updateCaretPos();
                    updateSelectionHighlight();
                    fireTextChanged();
                }
                break;
            case KEY_LEFT:
                _preferredCaretXValid = false;
                move(prevCodepoint(_string, _caretIdx));
                break;
            case KEY_RIGHT:
                _preferredCaretXValid = false;
                move(nextCodepoint(_string, _caretIdx));
                break;
            case KEY_HOME: {
                _preferredCaretXValid = false;
                if (_inputMode == InputMode::ANY) {
                    ccstd::vector<std::pair<size_t, size_t>> lines;
                    rebuildLineIndex(lines);
                    const size_t li = lineIndexOf(lines, _caretIdx);
                    move(lines[li].first);
                } else {
                    move(0);
                }
                break;
            }
            case KEY_END: {
                _preferredCaretXValid = false;
                if (_inputMode == InputMode::ANY) {
                    ccstd::vector<std::pair<size_t, size_t>> lines;
                    rebuildLineIndex(lines);
                    const size_t li = lineIndexOf(lines, _caretIdx);
                    move(lines[li].first + lines[li].second);
                } else {
                    move(_string.size());
                }
                break;
            }
            case KEY_UP:
            case KEY_DOWN: {
                if (_inputMode != InputMode::ANY) break;
                ccstd::vector<std::pair<size_t, size_t>> lines;
                rebuildLineIndex(lines);
                const size_t li = lineIndexOf(lines, _caretIdx);
                if (!_preferredCaretXValid) {
                    _preferredCaretX = columnAdvance(lines[li].first, _caretIdx);
                    _preferredCaretXValid = true;
                }
                const int delta = (ev.key == KEY_UP) ? -1 : 1;
                const int target = static_cast<int>(li) + delta;
                if (target < 0 || target >= static_cast<int>(lines.size())) break;
                const size_t ts = lines[target].first;
                const size_t tl = lines[target].second;
                move(byteIdxAtColumn(ts, tl, _preferredCaretX));
                break;
            }
            case KEY_ENTER:
                if (_inputMode == InputMode::ANY) {
                    // Multiline — insert newline (respecting maxLength).
                    if (_maxLength >= 0 &&
                        codepointCount(_string) >= static_cast<size_t>(_maxLength)) {
                        fireEditingReturn();
                        break;
                    }
                    if (hasSelection()) deleteSelection();
                    _string.insert(_caretIdx, "\n");
                    ++_caretIdx;
                    _selAnchor = _caretIdx;
                    _preferredCaretXValid = false;
                    _updateString(_string);
                    updateCaretPos();
                    updateSelectionHighlight();
                    resetCaretBlink();
                    fireTextChanged();
                    fireEditingReturn();
                } else {
                    // Single-line — return fires, then blur.
                    fireEditingReturn();
                    blur();
                }
                break;
            case KEY_ESCAPE:
                blur();
                break;
            default:
                break;
        }
    });

    _hooks->textL.bind([this](const TextInputEvent &ev) {
        if (!_focused || ev.text.empty()) return;
        if (_ctrlDown) return;
        _composition.clear();

        ccstd::string incoming = ev.text;

        // Mode-specific filtering — NUMERIC / DECIMAL / PHONE accept a
        // restricted character set; others accept anything. Matches the
        // mobile-keyboard constraint TS describes for each mode.
        auto filterOut = [&](auto predicate) {
            incoming.erase(std::remove_if(incoming.begin(), incoming.end(), predicate),
                           incoming.end());
        };
        switch (_inputMode) {
            case InputMode::NUMERIC:
                filterOut([](unsigned char c) { return !(c >= '0' && c <= '9'); });
                break;
            case InputMode::DECIMAL:
                filterOut([](unsigned char c) { return !((c >= '0' && c <= '9') || c == '.'); });
                break;
            case InputMode::PHONE_NUMBER:
                filterOut([](unsigned char c) {
                    return !((c >= '0' && c <= '9') || c == '+' || c == '-' || c == ' ');
                });
                break;
            default:
                break;  // ANY / SINGLE_LINE / EMAIL_ADDR / URL accept everything
        }
        if (incoming.empty()) return;

        if (hasSelection()) deleteSelection();

        // maxLength — Creator truncates the resulting string, so we
        // clip the INCOMING chunk to fit.
        if (_maxLength >= 0) {
            const size_t budget = static_cast<size_t>(_maxLength)
                                   - codepointCount(_string);
            if (budget == 0) return;
            if (codepointCount(incoming) > budget) {
                incoming = incoming.substr(0, byteOffsetOfCodepoint(incoming, budget));
            }
        }

        _string.insert(_caretIdx, incoming);
        _caretIdx += incoming.size();
        _selAnchor = _caretIdx;
        _updateString(_string);
        updateCaretPos();
        updateSelectionHighlight();
        resetCaretBlink();
        fireTextChanged();
    });

    _hooks->editingL.bind([this](const TextEditingEvent &ev) {
        if (!_focused) return;
        _composition = ev.text;
        _updateString(_string);  // re-render; refresh inlines composition
        updateCaretPos();
        updateSelectionHighlight();
    });
}

void EditBox::unbindEventHooks() {
    if (_hooks) { delete _hooks; _hooks = nullptr; }
}

// ────────────────────────────────────────────────────────────────────────
// Caret & selection
// ────────────────────────────────────────────────────────────────────────

void EditBox::resetCaretBlink() {
    _caretVisible = true;
    _caretTimer = 0.f;
    if (_caret) _caret->getNode()->setActive(_focused);
}

size_t EditBox::getSelectionStart() const { return std::min(_caretIdx, _selAnchor); }
size_t EditBox::getSelectionEnd()   const { return std::max(_caretIdx, _selAnchor); }

void EditBox::clearSelection() {
    _selAnchor = _caretIdx;
    updateSelectionHighlight();
}

void EditBox::setCaretIndex(size_t i) {
    _caretIdx = std::min(i, _string.size());
    while (_caretIdx > 0 && _caretIdx < _string.size() &&
           isUtf8Cont(static_cast<unsigned char>(_string[_caretIdx]))) {
        --_caretIdx;
    }
    _selAnchor = _caretIdx;
    updateCaretPos();
    resetCaretBlink();
}

void EditBox::deleteSelection() {
    if (!hasSelection()) return;
    const size_t s = getSelectionStart();
    const size_t e = getSelectionEnd();
    _string.erase(s, e - s);
    _caretIdx = s;
    _selAnchor = s;
    _updateString(_string);
    updateCaretPos();
    updateSelectionHighlight();
}

void EditBox::updateCaretPos() {
    if (!_caret || !_textLabel) return;
    const auto *font = _textLabel->getFont();
    if (!font) return;

    // Multi-line (InputMode::ANY): use line/column math. Single-line:
    // measure advance to caret within a single baseline.
    if (_inputMode == InputMode::ANY) {
        ccstd::vector<std::pair<size_t, size_t>> lines;
        rebuildLineIndex(lines);
        const size_t li = lineIndexOf(lines, _caretIdx);
        const size_t ls = lines[li].first;
        const size_t ll = lines[li].second;

        float lineAdv = 0.f;
        for (size_t i = 0; i < ll; ++i) {
            if (const auto *g = font->getGlyph(static_cast<unsigned char>(_string[ls + i]))) {
                lineAdv += g->xadvance;
            }
        }
        const float colX = columnAdvance(ls, _caretIdx);
        const float lineH = static_cast<float>(font->getLineHeight());
        const float blockTop = (static_cast<int>(lines.size()) - 1) * 0.5f * lineH;
        const float baselineY = blockTop - static_cast<int>(li) * lineH;

        Vec3 cp = _caret->getNode()->getPosition();
        cp.x = (colX - lineAdv * 0.5f);
        cp.y = baselineY - lineH * 0.5f;
        _caret->getNode()->setPosition(cp);
        return;
    }

    // Single-line: walk the rendered (masked) string's prefix up to the
    // caret position in _string codepoints.
    ccstd::string renderedPrefix;
    ccstd::string renderedFull = _updateLabelStringStyle(_string);
    if (_inputFlag == InputFlag::PASSWORD) {
        const size_t cpToCaret = codepointCount(_string.substr(0, _caretIdx));
        renderedPrefix.reserve(cpToCaret * 3);
        for (size_t i = 0; i < cpToCaret; ++i) renderedPrefix.append(kPasswordMask);
    } else {
        renderedPrefix = _updateLabelStringStyle(_string.substr(0, _caretIdx));
    }
    const float total  = measureAdvance(*font, renderedFull, renderedFull.size());
    const float prefix = measureAdvance(*font, renderedPrefix, renderedPrefix.size());

    Vec3 cp = _caret->getNode()->getPosition();
    cp.x = prefix - total * 0.5f;
    cp.y = 0.f;
    _caret->getNode()->setPosition(cp);
}

void EditBox::updateSelectionHighlight() {
    if (!_selHighlight) return;
    if (!hasSelection() || !_focused || _inputMode == InputMode::ANY) {
        // Multi-line selection highlight is MVP-deferred (would need one
        // rect per line); hide to avoid the stripe-on-one-line artifact.
        _selHighlight->getNode()->setActive(false);
        return;
    }
    if (!_textLabel || !_textLabel->getFont()) {
        _selHighlight->getNode()->setActive(false);
        return;
    }
    // Single-line: compute start/end X against the rendered prefix.
    const auto *font = _textLabel->getFont();
    ccstd::string renderedFull = _updateLabelStringStyle(_string);
    auto renderedPrefix = [&](size_t idx) {
        if (_inputFlag == InputFlag::PASSWORD) {
            const size_t cp = codepointCount(_string.substr(0, idx));
            ccstd::string out;
            out.reserve(cp * 3);
            for (size_t i = 0; i < cp; ++i) out.append(kPasswordMask);
            return out;
        }
        return _updateLabelStringStyle(_string.substr(0, idx));
    };
    const float total = measureAdvance(*font, renderedFull, renderedFull.size());
    const float xS = measureAdvance(*font, renderedPrefix(getSelectionStart()),
                                    renderedPrefix(getSelectionStart()).size()) - total * 0.5f;
    const float xE = measureAdvance(*font, renderedPrefix(getSelectionEnd()),
                                    renderedPrefix(getSelectionEnd()).size()) - total * 0.5f;
    const float width = std::max(1.f, xE - xS);
    _selHighlight->setSize(width, 24.f);
    if (auto *ui = _selHighlight->getNode()->getComponent<UITransform>()) {
        ui->setContentSize(width, 24.f);
    }
    _selHighlight->getNode()->setPosition(Vec3{xS, 0.f, 0.f});
    _selHighlight->getNode()->setActive(true);
}

bool EditBox::pointInside(float winX, float winY) const {
    auto *node = getNode();
    if (!node) return false;
    auto *ui = node->getComponent<UITransform>();
    if (!ui) return false;

    float winW = 0, winH = 0;
    nearestCanvasSize(node, winW, winH);
    const float wx = winX - winW * 0.5f;
    const float wy = winH * 0.5f - winY;
    return ui->hitTestWorld(wx, wy);
}

size_t EditBox::windowPointToCaretIndex(float winX, float winY) const {
    if (!_textLabel || _string.empty()) return 0;
    const auto *font = _textLabel->getFont();
    if (!font) return _string.size();

    auto *node = getNode();
    if (!node) return _string.size();

    if (!_composition.empty()) return _caretIdx;  // don't reposition during IME

    float winW = 0, winH = 0;
    nearestCanvasSize(node, winW, winH);
    const float wx = winX - winW * 0.5f;
    const float wy = winH * 0.5f - winY;

    const Mat4 boxInv = node->getWorldMatrix().getInversed();
    const float bx = boxInv.m[0] * wx + boxInv.m[4] * wy + boxInv.m[12];
    const float by = boxInv.m[1] * wx + boxInv.m[5] * wy + boxInv.m[13];

    // Multi-line: first pick the line from `by`, then the column from bx.
    if (_inputMode == InputMode::ANY) {
        ccstd::vector<std::pair<size_t, size_t>> lines;
        rebuildLineIndex(lines);
        const float lineH = static_cast<float>(font->getLineHeight());
        const float blockTop = (static_cast<int>(lines.size()) - 1) * 0.5f * lineH;
        // For each line, centre Y = blockTop - li*lineH.
        int bestLi = 0;
        float bestD = 1e9f;
        for (int li = 0; li < static_cast<int>(lines.size()); ++li) {
            const float cy = blockTop - li * lineH;
            const float d = std::fabs(by - cy);
            if (d < bestD) { bestD = d; bestLi = li; }
        }
        const size_t ls = lines[bestLi].first;
        const size_t ll = lines[bestLi].second;
        return byteIdxAtColumn(ls, ll, bx);
    }

    // Single-line walk.
    ccstd::string renderedFull = _updateLabelStringStyle(_string);
    const float total = measureAdvance(*font, renderedFull, renderedFull.size());
    float gapX = -total * 0.5f;
    size_t bestIdx = 0;
    float  bestDist = std::fabs(bx - gapX);
    size_t i = 0;
    while (i < _string.size()) {
        const size_t cpEnd = nextCodepoint(_string, i);
        float cpAdv;
        if (_inputFlag == InputFlag::PASSWORD) {
            const auto *mg = font->getGlyph(static_cast<uint32_t>(kPasswordMask[0]));
            cpAdv = mg ? static_cast<float>(mg->xadvance) : 0.f;
        } else {
            cpAdv = 0.f;
            for (size_t k = i; k < cpEnd; ++k) {
                if (const auto *g = font->getGlyph(static_cast<unsigned char>(_string[k]))) {
                    cpAdv += g->xadvance;
                }
            }
        }
        gapX += cpAdv;
        const float d = std::fabs(bx - gapX);
        if (d < bestDist) { bestDist = d; bestIdx = cpEnd; }
        i = cpEnd;
    }
    return bestIdx;
}

// ── Multi-line helpers ──────────────────────────────────────────────────

void EditBox::rebuildLineIndex(ccstd::vector<std::pair<size_t, size_t>> &out) const {
    out.clear();
    size_t start = 0;
    for (size_t i = 0; i <= _string.size(); ++i) {
        if (i == _string.size() || _string[i] == '\n') {
            out.emplace_back(start, i - start);
            start = i + 1;
        }
    }
}

size_t EditBox::lineIndexOf(const ccstd::vector<std::pair<size_t, size_t>> &lines,
                             size_t byteIdx) const {
    for (size_t i = 0; i < lines.size(); ++i) {
        const size_t s = lines[i].first;
        const size_t e = s + lines[i].second;
        if (byteIdx >= s && byteIdx <= e) return i;
    }
    return lines.empty() ? 0 : lines.size() - 1;
}

float EditBox::columnAdvance(size_t lineStart, size_t byteIdx) const {
    if (!_textLabel || !_textLabel->getFont()) return 0.f;
    const auto *font = _textLabel->getFont();
    float adv = 0.f;
    for (size_t i = lineStart; i < byteIdx && i < _string.size(); ++i) {
        if (const auto *g = font->getGlyph(static_cast<unsigned char>(_string[i]))) {
            adv += g->xadvance;
        }
    }
    return adv;
}

size_t EditBox::byteIdxAtColumn(size_t lineStart, size_t lineLen, float targetX) const {
    if (!_textLabel || !_textLabel->getFont()) return lineStart;
    const auto *font = _textLabel->getFont();

    // Line advance for centring — the label centres each line's run on
    // the node origin, so a click at absolute local-X `targetX` maps to
    // an intra-line column of `targetX - (-lineAdv/2) = targetX + lineAdv/2`.
    float lineAdv = 0.f;
    for (size_t i = 0; i < lineLen; ++i) {
        if (const auto *g = font->getGlyph(static_cast<unsigned char>(_string[lineStart + i]))) {
            lineAdv += g->xadvance;
        }
    }
    const float goal = targetX + lineAdv * 0.5f;

    float adv = 0.f;
    size_t best = lineStart;
    float  bestDist = std::fabs(goal - adv);
    size_t i = lineStart;
    const size_t lineEnd = lineStart + lineLen;
    while (i < lineEnd) {
        const size_t cpEnd = nextCodepoint(_string, i);
        for (size_t k = i; k < cpEnd; ++k) {
            if (const auto *g = font->getGlyph(static_cast<unsigned char>(_string[k]))) {
                adv += g->xadvance;
            }
        }
        const float d = std::fabs(goal - adv);
        if (d < bestDist) { bestDist = d; best = cpEnd; }
        i = cpEnd;
    }
    return best;
}

}  // namespace cc
