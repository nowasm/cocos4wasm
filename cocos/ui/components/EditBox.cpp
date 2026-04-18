#include "cocos/ui/components/EditBox.h"

#include <algorithm>
#include <cmath>

#include "base/Log.h"
#include "cocos/2d/components/Label.h"
#include "cocos/2d/components/Sprite.h"
#include "cocos/2d/framework/UITransform.h"
#include "cocos/2d/text/BmfFont.h"
#include "math/Mat4.h"
#include "core/component/NodeActivator.h"
#include "core/scene-graph/Node.h"
#include "engine/EngineEvents.h"
#include "platform/SDLHelper.h"

namespace cc {

EditBox *EditBox::s_currentlyFocused = nullptr;

CC_IMPLEMENT_CLASS(EditBox, "cc.EditBox", Component)
    .property("text",        &EditBox::_text)
    .property("placeholder", &EditBox::_placeholder)
CC_END_CLASS(EditBox);

struct EditBox::Hooks {
    events::Mouse::Listener       mouseL;
    events::Keyboard::Listener    keyboardL;
    events::TextInput::Listener   textL;
    events::TextEditing::Listener editingL;
};

namespace {

// Ancestor UITransform is the cheapest source of "what's the canvas
// size?" that also survives multi-canvas scenes — the containing Canvas
// node carries a transform sized to the window in our demos.
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

// UTF-8 step helpers. Byte 0b10xxxxxx is a continuation byte; a codepoint
// boundary is anywhere the top two bits aren't 10.
bool isUtf8Cont(unsigned char c) { return (c & 0xC0) == 0x80; }

size_t prevCodepoint(const ccstd::string &s, size_t pos) {
    if (pos == 0) return 0;
    --pos;
    while (pos > 0 && isUtf8Cont(static_cast<unsigned char>(s[pos]))) {
        --pos;
    }
    return pos;
}

size_t nextCodepoint(const ccstd::string &s, size_t pos) {
    if (pos >= s.size()) return s.size();
    ++pos;
    while (pos < s.size() && isUtf8Cont(static_cast<unsigned char>(s[pos]))) {
        ++pos;
    }
    return pos;
}

}  // namespace

void EditBox::onEnable() {
    ensureChildren();
    refreshLabel();
    updateCaretPos();
    if (_caret) _caret->getNode()->setActive(false);  // hidden until focus

    _hooks = new Hooks();

    _hooks->mouseL.bind([this](const MouseEvent &ev) {
        if (ev.button != 0 && ev.type != MouseEvent::Type::MOVE) return;

        if (ev.type == MouseEvent::Type::DOWN) {
            const bool inside = pointInside(ev.x, ev.y);
            if (inside) {
                if (!_focused) focus();
                _caretIdx = windowPointToCaretIndex(ev.x, ev.y);
                _selAnchor = _caretIdx;       // fresh click collapses selection
                _mouseDragging = true;
                updateCaretPos();
                updateSelectionHighlight();
                resetCaretBlink();
            } else if (_focused) {
                blur();
            }
            return;
        }

        if (ev.type == MouseEvent::Type::MOVE && _mouseDragging && _focused) {
            // Drag extends selection from the original anchor. No hit-test
            // requirement — users often drag past the box edge to select
            // all text.
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
        _ctrlDown = ev.ctrlKeyActive;  // track on every event, focused or not
        if (!_focused) return;
        if (ev.action != KeyboardEvent::Action::PRESS) return;

        // KeyCode values live in EngineEvents.h — ASCII-ish for editing
        // keys, dedicated enum values for navigation.
        constexpr int KEY_BACKSPACE  = 8;
        constexpr int KEY_ENTER      = 13;
        constexpr int KEY_ESCAPE     = 27;
        constexpr int KEY_END        = 35;
        constexpr int KEY_HOME       = 36;
        constexpr int KEY_LEFT       = 37;
        constexpr int KEY_UP         = 38;
        constexpr int KEY_RIGHT      = 39;
        constexpr int KEY_DOWN       = 40;
        constexpr int KEY_DELETE     = 46;

        const bool shift = ev.shiftKeyActive;
        const bool ctrl  = ev.ctrlKeyActive;

        // Helper: after a navigation key, collapse the selection unless
        // the user is extending it with shift.
        auto move = [&](size_t newIdx) {
            _caretIdx = newIdx;
            if (!shift) _selAnchor = _caretIdx;
            updateCaretPos();
            updateSelectionHighlight();
            resetCaretBlink();
        };

        // Clipboard + select-all before the switch so the letter keys
        // can still fall through to TextInput when Ctrl isn't held.
        if (ctrl) {
            switch (ev.key) {
                case 'A':
                    _selAnchor = 0;
                    _caretIdx  = _text.size();
                    updateCaretPos();
                    updateSelectionHighlight();
                    resetCaretBlink();
                    return;
                case 'C':
                    if (hasSelection()) {
                        SDLHelper::setClipboardText(
                            _text.substr(getSelectionStart(),
                                         getSelectionEnd() - getSelectionStart()));
                    }
                    return;
                case 'X':
                    if (hasSelection()) {
                        SDLHelper::setClipboardText(
                            _text.substr(getSelectionStart(),
                                         getSelectionEnd() - getSelectionStart()));
                        deleteSelection();
                    }
                    return;
                case 'V': {
                    const ccstd::string clip = SDLHelper::getClipboardText();
                    if (!clip.empty()) {
                        if (hasSelection()) deleteSelection();
                        _text.insert(_caretIdx, clip);
                        _caretIdx += clip.size();
                        _selAnchor = _caretIdx;
                        refreshLabel();
                        updateCaretPos();
                        updateSelectionHighlight();
                        resetCaretBlink();
                        if (_onChanged) _onChanged(this, _text);
                    }
                    return;
                }
                default:
                    // Other Ctrl combos are ignored — they shouldn't fall
                    // through to text-insertion either.
                    return;
            }
        }

        switch (ev.key) {
            case KEY_BACKSPACE:
                if (hasSelection()) {
                    deleteSelection();
                } else if (_caretIdx > 0) {
                    const size_t prev = prevCodepoint(_text, _caretIdx);
                    _text.erase(prev, _caretIdx - prev);
                    _caretIdx = prev;
                    _selAnchor = _caretIdx;
                    refreshLabel();
                    updateCaretPos();
                    updateSelectionHighlight();
                    if (_onChanged) _onChanged(this, _text);
                }
                break;
            case KEY_DELETE:
                if (hasSelection()) {
                    deleteSelection();
                } else if (_caretIdx < _text.size()) {
                    const size_t next = nextCodepoint(_text, _caretIdx);
                    _text.erase(_caretIdx, next - _caretIdx);
                    refreshLabel();
                    updateCaretPos();
                    updateSelectionHighlight();
                    if (_onChanged) _onChanged(this, _text);
                }
                break;
            case KEY_LEFT:
                _preferredCaretXValid = false;
                move(prevCodepoint(_text, _caretIdx));
                break;
            case KEY_RIGHT:
                _preferredCaretXValid = false;
                move(nextCodepoint(_text, _caretIdx));
                break;
            case KEY_HOME: {
                // Multi-line: go to start of current line; single-line:
                // start of buffer. Shift extends selection as usual.
                _preferredCaretXValid = false;
                if (_multiLine) {
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
                if (_multiLine) {
                    ccstd::vector<std::pair<size_t, size_t>> lines;
                    rebuildLineIndex(lines);
                    const size_t li = lineIndexOf(lines, _caretIdx);
                    move(lines[li].first + lines[li].second);
                } else {
                    move(_text.size());
                }
                break;
            }
            case KEY_UP:
            case KEY_DOWN: {
                if (!_multiLine) break;
                ccstd::vector<std::pair<size_t, size_t>> lines;
                rebuildLineIndex(lines);
                const size_t li = lineIndexOf(lines, _caretIdx);
                // Remember the original column so vertical travel through
                // short lines eventually comes back to the same X.
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
                if (_multiLine) {
                    if (hasSelection()) deleteSelection();
                    _text.insert(_caretIdx, "\n");
                    ++_caretIdx;
                    _selAnchor = _caretIdx;
                    _preferredCaretXValid = false;
                    refreshLabel();
                    updateCaretPos();
                    updateSelectionHighlight();
                    resetCaretBlink();
                    if (_onChanged) _onChanged(this, _text);
                } else {
                    blur();
                }
                break;
            case KEY_ESCAPE:
                blur();
                break;
            default:
                // Printable characters arrive via TextInput. Clipboard
                // shortcuts land in P5e-3.
                break;
        }
    });

    _hooks->textL.bind([this](const TextInputEvent &ev) {
        if (!_focused || ev.text.empty()) return;
        // Ctrl-letter combos (clipboard / select-all) are handled in the
        // keyboard callback; drop the stray TextInput some platforms
        // emit for those so 'v' doesn't appear after Ctrl+V.
        if (_ctrlDown) return;
        // A TextInput commits the IME composition — clear the preview
        // first so the authoritative text doesn't duplicate the glyphs.
        _composition.clear();
        if (hasSelection()) deleteSelection();
        _text.insert(_caretIdx, ev.text);
        _caretIdx += ev.text.size();
        _selAnchor = _caretIdx;
        refreshLabel();
        updateCaretPos();
        updateSelectionHighlight();
        resetCaretBlink();
        if (_onChanged) _onChanged(this, _text);
    });

    _hooks->editingL.bind([this](const TextEditingEvent &ev) {
        if (!_focused) return;
        // Store the live composition. Empty string clears the preview
        // (SDL sends this on IME cancel or right before a commit).
        _composition = ev.text;
        refreshLabel();
        updateCaretPos();
        updateSelectionHighlight();
    });
}

void EditBox::onDisable() {
    if (_focused) blur();
    if (_hooks) { delete _hooks; _hooks = nullptr; }
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

void EditBox::ensureChildren() {
    auto *node = getNode();
    if (!node) return;

    // Label child.
    if (!_label) {
        if (auto *existing = node->getChildByName("edit-label")) {
            _label = existing->getComponent<Label>();
        }
    }
    if (!_label) {
        auto *lnode = ccnew Node("edit-label");
        lnode->setPosition(Vec3(-_padLeft(), 0.f, 0.f));
        _label = lnode->addComponent<Label>();
        if (_font) _label->setFont(_font);
        node->addChild(lnode);
    }

    // Caret child — thin vertical bar.
    if (!_caret) {
        if (auto *existing = node->getChildByName("edit-caret")) {
            _caret = existing->getComponent<Sprite>();
        }
    }
    if (!_caret) {
        auto *cnode = ccnew Node("edit-caret");
        auto *ui = cnode->addComponent<UITransform>();
        ui->setContentSize(2.f, 24.f);
        ui->setAnchorPoint(0.5f, 0.5f);
        _caret = cnode->addComponent<Sprite>();
        _caret->setSize(2.f, 24.f);
        _caret->setColor(_textColor);
        node->addChild(cnode);
    }

    // Selection highlight — draw BEFORE label/caret so text sits on top.
    // Adding this first in onEnable would be ideal; appending here is
    // fine because UIBatcher2d honours sibling order at render time and
    // our scene-graph + stencil pipeline keeps children sorted by index.
    if (!_selHighlight) {
        if (auto *existing = node->getChildByName("edit-selection")) {
            _selHighlight = existing->getComponent<Sprite>();
        }
    }
    if (!_selHighlight) {
        auto *sn = ccnew Node("edit-selection");
        auto *ui = sn->addComponent<UITransform>();
        ui->setContentSize(0.f, 24.f);
        ui->setAnchorPoint(0.f, 0.5f);  // left-anchored so x = selection start
        _selHighlight = sn->addComponent<Sprite>();
        _selHighlight->setSize(0.f, 24.f);
        _selHighlight->setColor(_selectionColor);
        sn->setActive(false);
        node->addChild(sn);
    }
}

float EditBox::_padLeft() const {
    // Hard-coded padding for MVP — EditBox lays its label just inside
    // the background sprite's left edge. When padding becomes authored
    // this returns whatever the user configured.
    auto *n = getNode();
    auto *ui = n ? n->getComponent<UITransform>() : nullptr;
    return ui ? ui->getContentSize().x * 0.5f - 8.f : 0.f;
}

ccstd::string EditBox::renderedText(size_t &outCaretByte) const {
    if (_text.empty() && _composition.empty()) {
        outCaretByte = 0;
        return {};
    }
    ccstd::string base;
    size_t caretInBase;
    if (_passwordMode) {
        // Count codepoints and where the caret sits within them; map
        // each codepoint to one mask char. Composition preview remains
        // visible even in password mode (users need typing feedback).
        size_t cps = 0, cpsToCaret = 0;
        for (size_t i = 0; i < _text.size(); ++i) {
            if (!isUtf8Cont(static_cast<unsigned char>(_text[i]))) {
                if (i < _caretIdx) ++cpsToCaret;
                ++cps;
            }
        }
        base.assign(cps, _passwordChar);
        caretInBase = cpsToCaret;
    } else {
        base = _text;
        caretInBase = _caretIdx;
    }
    if (!_composition.empty()) {
        ccstd::string preview = "[" + _composition + "]";
        base.insert(caretInBase, preview);
    }
    outCaretByte = caretInBase;
    return base;
}

void EditBox::refreshLabel() {
    if (!_label) return;
    if (_text.empty() && _composition.empty()) {
        _label->setText(_placeholder);
        _label->setColor(_placeholderColor);
        return;
    }
    size_t dummy;
    _label->setText(renderedText(dummy));
    _label->setColor(_textColor);
}

namespace {
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
}  // namespace

void EditBox::updateCaretPos() {
    if (!_caret || !_label) return;
    const auto *font = _label->getFont();
    if (!font) return;

    // Single-line / password / composition: walk the rendered string.
    // The Label centres the whole thing, pen starts at -total/2.
    if (!_multiLine) {
        size_t renderedCaret = 0;
        const ccstd::string shown = renderedText(renderedCaret);
        const float total  = measureAdvance(*font, shown, shown.size());
        const float prefix = measureAdvance(*font, shown, renderedCaret);

        Vec3 cp = _caret->getNode()->getPosition();
        cp.x = _label->getNode()->getPosition().x + (prefix - total * 0.5f);
        cp.y = 0.f;
        _caret->getNode()->setPosition(cp);
        return;
    }

    // Multi-line: find the caret's line, measure its X within that line,
    // and drop Y by (line index × line height) from the block-top baseline.
    ccstd::vector<std::pair<size_t, size_t>> lines;
    rebuildLineIndex(lines);
    const size_t li = lineIndexOf(lines, _caretIdx);
    const size_t ls = lines[li].first;
    const size_t ll = lines[li].second;

    float lineAdv = 0.f;
    for (size_t i = 0; i < ll; ++i) {
        if (const auto *g = font->getGlyph(static_cast<unsigned char>(_text[ls + i]))) {
            lineAdv += g->xadvance;
        }
    }
    const float colX = columnAdvance(ls, _caretIdx);

    const float lineH = static_cast<float>(font->getLineHeight());
    const float blockTopBaselineY = (static_cast<int>(lines.size()) - 1) * 0.5f * lineH;
    const float baselineY = blockTopBaselineY - static_cast<int>(li) * lineH;

    Vec3 cp = _caret->getNode()->getPosition();
    cp.x = _label->getNode()->getPosition().x + (colX - lineAdv * 0.5f);
    cp.y = baselineY - lineH * 0.5f;  // centre the caret bar on the line
    _caret->getNode()->setPosition(cp);
}

void EditBox::rebuildLineIndex(ccstd::vector<std::pair<size_t, size_t>> &out) const {
    out.clear();
    size_t start = 0;
    for (size_t i = 0; i <= _text.size(); ++i) {
        if (i == _text.size() || _text[i] == '\n') {
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
        // A caret sitting on the line-ending boundary belongs to the
        // line before the \n, except when the caret is right past the
        // whole buffer end.
        if (byteIdx >= s && byteIdx <= e) return i;
    }
    return lines.empty() ? 0 : lines.size() - 1;
}

float EditBox::columnAdvance(size_t lineStart, size_t byteIdx) const {
    if (!_label || !_label->getFont()) return 0.f;
    const auto *font = _label->getFont();
    float adv = 0.f;
    for (size_t i = lineStart; i < byteIdx && i < _text.size(); ++i) {
        if (const auto *g = font->getGlyph(static_cast<unsigned char>(_text[i]))) {
            adv += g->xadvance;
        }
    }
    return adv;
}

size_t EditBox::byteIdxAtColumn(size_t lineStart, size_t lineLen, float targetX) const {
    if (!_label || !_label->getFont()) return lineStart;
    const auto *font = _label->getFont();
    float adv = 0.f;
    size_t best = lineStart;
    float  bestDist = std::fabs(targetX - adv);
    size_t i = lineStart;
    const size_t lineEnd = lineStart + lineLen;
    while (i < lineEnd) {
        const size_t cpEnd = nextCodepoint(_text, i);
        for (size_t k = i; k < cpEnd; ++k) {
            if (const auto *g = font->getGlyph(static_cast<unsigned char>(_text[k]))) {
                adv += g->xadvance;
            }
        }
        const float d = std::fabs(targetX - adv);
        if (d < bestDist) {
            bestDist = d;
            best = cpEnd;
        }
        i = cpEnd;
    }
    return best;
}

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

void EditBox::deleteSelection() {
    if (!hasSelection()) return;
    const size_t s = getSelectionStart();
    const size_t e = getSelectionEnd();
    _text.erase(s, e - s);
    _caretIdx = s;
    _selAnchor = s;
    refreshLabel();
    updateCaretPos();
    updateSelectionHighlight();
    if (_onChanged) _onChanged(this, _text);
}

float EditBox::caretXAt(size_t idx) const {
    if (!_label || !_label->getFont()) return 0.f;
    // Translate the _text-byte index into a rendered-string byte index:
    //   • password mode: byte idx → codepoint count
    //   • composition active: if idx > _caretIdx, skip past the
    //     "[comp]" wrapper so selection highlights don't cut through it
    size_t renderedIdx;
    if (_passwordMode) {
        size_t cps = 0;
        for (size_t i = 0; i < _text.size() && i < idx; ++i) {
            if (!isUtf8Cont(static_cast<unsigned char>(_text[i]))) ++cps;
        }
        renderedIdx = cps;
    } else {
        renderedIdx = idx;
    }
    if (!_composition.empty()) {
        const size_t compLen = _composition.size() + 2;  // "[...]"
        const size_t pivot = _passwordMode
            ? [&]() -> size_t {
                size_t cps = 0;
                for (size_t i = 0; i < _caretIdx; ++i) {
                    if (!isUtf8Cont(static_cast<unsigned char>(_text[i]))) ++cps;
                }
                return cps;
              }()
            : _caretIdx;
        if (renderedIdx > pivot) renderedIdx += compLen;
    }

    size_t dummy;
    const ccstd::string shown = renderedText(dummy);
    const float total  = measureAdvance(*_label->getFont(), shown, shown.size());
    const float prefix = measureAdvance(*_label->getFont(), shown, renderedIdx);
    return _label->getNode()->getPosition().x + (prefix - total * 0.5f);
}

void EditBox::updateSelectionHighlight() {
    if (!_selHighlight) return;
    if (!hasSelection() || !_focused) {
        _selHighlight->getNode()->setActive(false);
        return;
    }
    const float xStart = caretXAt(getSelectionStart());
    const float xEnd   = caretXAt(getSelectionEnd());
    const float width  = std::max(1.f, xEnd - xStart);
    _selHighlight->setSize(width, 24.f);
    auto *ui = _selHighlight->getNode()->getComponent<UITransform>();
    if (ui) ui->setContentSize(width, 24.f);
    _selHighlight->getNode()->setPosition(Vec3(xStart, 0.f, 0.f));
    _selHighlight->getNode()->setActive(true);
}

void EditBox::setCaretIndex(size_t i) {
    _caretIdx = std::min(i, _text.size());
    // Guard: don't land mid-codepoint. Align to the previous boundary.
    while (_caretIdx > 0 && _caretIdx < _text.size() &&
           isUtf8Cont(static_cast<unsigned char>(_text[_caretIdx]))) {
        --_caretIdx;
    }
    updateCaretPos();
    resetCaretBlink();
}

void EditBox::setText(const ccstd::string &t) {
    if (_text == t) return;
    _text = t;
    _caretIdx = _text.size();  // jump to end on programmatic set
    refreshLabel();
    updateCaretPos();
    if (_onChanged) _onChanged(this, _text);
}

void EditBox::setPlaceholder(const ccstd::string &t) {
    _placeholder = t;
    refreshLabel();
}

void EditBox::setFont(BmfFont *f) {
    _font = f;
    if (_label) _label->setFont(f);
    updateCaretPos();
}

void EditBox::setPasswordMode(bool v) {
    if (_passwordMode == v) return;
    _passwordMode = v;
    refreshLabel();
    updateCaretPos();
    updateSelectionHighlight();
}

void EditBox::setPasswordChar(char c) {
    if (_passwordChar == c) return;
    _passwordChar = c;
    if (_passwordMode) {
        refreshLabel();
        updateCaretPos();
        updateSelectionHighlight();
    }
}

void EditBox::focus() {
    if (_focused) return;
    if (s_currentlyFocused && s_currentlyFocused != this) {
        s_currentlyFocused->blur();
    }
    _focused = true;
    s_currentlyFocused = this;
    SDLHelper::startTextInput();
    _caretVisible = true;
    _caretTimer = 0.f;
    if (_caret) _caret->getNode()->setActive(true);
    CC_LOG_INFO("[EditBox] focused");
}

void EditBox::blur() {
    if (!_focused) return;
    _focused = false;
    if (s_currentlyFocused == this) {
        s_currentlyFocused = nullptr;
        SDLHelper::stopTextInput();
    }
    if (_caret) _caret->getNode()->setActive(false);
    if (_selHighlight) _selHighlight->getNode()->setActive(false);
    _mouseDragging = false;
    CC_LOG_INFO("[EditBox] blurred");
}

size_t EditBox::windowPointToCaretIndex(float winX, float winY) const {
    if (!_label) return _text.size();
    const auto *font = _label->getFont();
    if (!font || _text.empty()) return 0;

    auto *node = getNode();
    if (!node) return _text.size();

    // IME-composition clicks aren't routed into the preview — treat as
    // "caret stays put". Users rarely click mid-compose.
    if (!_composition.empty()) return _caretIdx;

    float winW = 0, winH = 0;
    nearestCanvasSize(node, winW, winH);
    const float wx = winX - winW * 0.5f;
    const float wy = winH * 0.5f - winY;

    const Mat4 boxInv = node->getWorldMatrix().getInversed();
    const float bx = boxInv.m[0] * wx + boxInv.m[4] * wy + boxInv.m[12];
    (void)wy;

    const float labelX = _label->getNode()->getPosition().x;
    const float penStartX = bx - labelX;

    // Walk codepoints of _text; for each, the rendered width is either
    // the mask glyph (password) or the sum of the codepoint's glyphs.
    // Returned index is a byte offset into _text.
    const BmfGlyph *maskGlyph = _passwordMode
        ? font->getGlyph(static_cast<uint32_t>(_passwordChar))
        : nullptr;
    const float maskW = maskGlyph ? static_cast<float>(maskGlyph->xadvance) : 0.f;

    float total = 0.f;
    {
        size_t i = 0;
        while (i < _text.size()) {
            const size_t e = nextCodepoint(_text, i);
            if (_passwordMode) {
                total += maskW;
            } else {
                for (size_t k = i; k < e; ++k) {
                    if (const auto *g = font->getGlyph(static_cast<unsigned char>(_text[k]))) {
                        total += g->xadvance;
                    }
                }
            }
            i = e;
        }
    }

    float gapX = -total * 0.5f;
    size_t bestIdx = 0;
    float  bestDist = std::fabs(penStartX - gapX);

    size_t i = 0;
    while (i < _text.size()) {
        const size_t cpEnd = nextCodepoint(_text, i);
        float cpAdvance;
        if (_passwordMode) {
            cpAdvance = maskW;
        } else {
            cpAdvance = 0.f;
            for (size_t k = i; k < cpEnd; ++k) {
                if (const auto *g = font->getGlyph(static_cast<unsigned char>(_text[k]))) {
                    cpAdvance += g->xadvance;
                }
            }
        }
        gapX += cpAdvance;
        const float d = std::fabs(penStartX - gapX);
        if (d < bestDist) {
            bestDist = d;
            bestIdx  = cpEnd;
        }
        i = cpEnd;
    }
    return bestIdx;
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

}  // namespace cc
