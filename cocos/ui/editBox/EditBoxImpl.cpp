#include "cocos/ui/editBox/EditBoxImpl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "base/Log.h"
#include "cocos/2d/components/Label.h"
#include "cocos/2d/components/Sprite.h"
#include "cocos/2d/framework/UITransform.h"
#include "cocos/2d/text/TextFont.h"
#include "cocos/ui/editBox/EditBox.h"
#include "cocos/ui/editBox/TabIndexUtil.h"
#include "core/scene-graph/Node.h"
#include "engine/EngineEvents.h"
#include "math/Mat4.h"
#include "platform/SDLHelper.h"

namespace cc {

EditBoxImpl *EditBoxImpl::s_current = nullptr;

// SDL bus listener bundle. Heap-allocated so the header doesn't pull in
// EngineEvents.h — keeping EditBoxImpl.h cheap to include keeps the
// component API surface clean.
struct EditBoxImpl::Listeners {
    events::Mouse::Listener       mouseL;
    events::Keyboard::Listener    keyboardL;
    events::TextInput::Listener   textL;
    events::TextEditing::Listener editingL;
};

namespace {

// Cocos Creator uses U+25CF (●) as the canonical password mask. Our
// default BMFont atlas is ASCII-only, so '●' would render as an empty
// box. Falling back to '*' keeps masking visibly correct; a later
// BmfFont that carries U+25CF can swap the sentinel.
constexpr char kPasswordMask = '*';

// UTF-8 step helpers — codepoint-granular caret navigation matches TS
// semantics even when the script engine is absent.
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

size_t codepointCount(const ccstd::string &s) {
    size_t n = 0;
    for (unsigned char c : s) if (!isUtf8Cont(c)) ++n;
    return n;
}

size_t byteOffsetOfCodepoint(const ccstd::string &s, size_t cp) {
    size_t cur = 0, cnt = 0;
    while (cur < s.size() && cnt < cp) {
        cur = nextCodepoint(s, cur);
        ++cnt;
    }
    return cur;
}

// Glyph-advance measurement — walks one rendered string (what the Label
// draws, not the authoritative input string) and sums xadvance. Used for
// both caret placement and click→index resolution.
float measureAdvance(TextFont &font, const ccstd::string &s, size_t cutByte) {
    float out = 0.f;
    const size_t n = std::min(cutByte, s.size());
    for (size_t i = 0; i < n; ++i) {
        if (const auto *g = font.getGlyph(static_cast<unsigned char>(s[i]))) {
            out += g->xadvance;
        }
    }
    return out;
}

// Nearest UITransform walking up from `from` (inclusive of `from`).
// Used for window→local math when the raw global mouse listener fires —
// we still need to know the canvas size to recentre window coords.
void nearestCanvasSize(Node *from, float &w, float &h) {
    w = 1280.f; h = 720.f;
    Node *n = from ? from->getParent() : nullptr;
    for (; n; n = n->getParent()) {
        if (auto *ui = n->getComponent<UITransform>()) {
            w = ui->getContentSize().x;
            h = ui->getContentSize().y;
            return;
        }
    }
}

// Render the input string through the same InputFlag transform the Label
// shows. For PASSWORD we substitute each codepoint with a mask char;
// for CAPS_* we apply the TS-equivalent uppercase logic.
ccstd::string renderForDisplay(const EditBox *box, const ccstd::string &text,
                                bool ignorePassword) {
    if (!box) return text;
    return box->_applyDisplayStyle(text, ignorePassword);
}

}  // namespace

// ────────────────────────────────────────────────────────────────────────
// Lifecycle
// ────────────────────────────────────────────────────────────────────────

EditBoxImpl::~EditBoxImpl() {
    EditBoxImpl::clear();
}

void EditBoxImpl::init(EditBox *delegate) {
    EditBoxImplBase::init(delegate);
    ensureVisualChildren();
    bindListeners();
    TabIndexUtil::add(this);
    if (_delegate) TabIndexUtil::resort();
}

void EditBoxImpl::onEnable() {
    // Nothing to do — listeners are always on (gated on `_editing`).
    // Kept for TS parity so the component can call _impl.onEnable()
    // without branching.
}

void EditBoxImpl::beforeDraw() {
    // Label follows the scene graph, so there's no matrix to sync (unlike
    // the DOM impl that has to push a CSS transform each frame).
    // Left as a hook point for future platforms that need it.
}

void EditBoxImpl::onDisable() {
    // Match TS base: flush any active editing session when the
    // component is disabled.
    if (_editing) endEditing();
}

void EditBoxImpl::clear() {
    if (_editing) endEditing();
    unbindListeners();
    TabIndexUtil::remove(this);
    if (s_current == this) s_current = nullptr;
    // Visual children outlive the impl (they're owned by the delegate's
    // node). They'll be torn down with the node itself.
    _caret = nullptr;
    _selHighlight = nullptr;
    EditBoxImplBase::clear();
}

void EditBoxImpl::setTabIndex(int /*index*/) {
    TabIndexUtil::resort();
}

void EditBoxImpl::setSize(float width, float height) {
    _width = width;
    _height = height;
    // Caret height follows the box height for single-line inputs; clamp
    // it to a sensible glyph-sized band so multi-line caret strokes
    // still fit within one line row.
    const float caretH = std::min(height, 24.f);
    if (_caret) {
        _caret->setSize(2.f, caretH);
        if (auto *ui = _caret->getNode()->getComponent<UITransform>()) {
            ui->setContentSize(2.f, caretH);
        }
    }
    if (_selHighlight) {
        if (auto *ui = _selHighlight->getNode()->getComponent<UITransform>()) {
            const Vec2 &cs = ui->getContentSize();
            ui->setContentSize(cs.x, caretH);
        }
        _selHighlight->setSize(_selHighlight->getSize().x, caretH);
    }
}

void EditBoxImpl::beginEditing() {
    if (_editing) return;
    if (s_current && s_current != this) {
        s_current->setFocus(false);  // blur the previously-focused box
    }
    _editing = true;
    s_current = this;
    SDLHelper::startTextInput();

    _caretVisible = true;
    _caretTimer = 0.f;
    setCaretVisible(true);

    if (_delegate) _delegate->_editBoxEditingDidBegan();
}

void EditBoxImpl::endEditing() {
    if (!_editing) return;
    _editing = false;
    if (s_current == this) {
        s_current = nullptr;
        SDLHelper::stopTextInput();
    }
    setCaretVisible(false);
    setSelectionVisible(false);
    _mouseDragging = false;
    _composition.clear();
    // Re-render the string (strips any leftover composition overlay).
    if (_delegate) _delegate->_impl_refreshDisplay();
    if (_delegate) _delegate->_editBoxEditingDidEnded();
}

// ────────────────────────────────────────────────────────────────────────
// Per-frame tick
// ────────────────────────────────────────────────────────────────────────

void EditBoxImpl::tick(float dt) {
    if (!_editing || !_caret) return;
    _caretTimer += dt;
    if (_caretTimer >= kCaretBlinkPeriod) {
        _caretTimer = 0.f;
        _caretVisible = !_caretVisible;
        Color c = _caret->getColor();
        c.a = _caretVisible ? static_cast<uint8_t>(_caretColor.a) : 0;
        _caret->setColor(c);
    }
}

// ────────────────────────────────────────────────────────────────────────
// Delegate → impl click entry-point
// ────────────────────────────────────────────────────────────────────────

void EditBoxImpl::onDelegateNodeClick(float localX, float localY) {
    if (!_delegate) return;
    if (!_editing) beginEditing();
    // Translate local into caret index directly — localX is already
    // anchor-relative which is the same space the Label centres text on.
    // Reuse the multi-line helpers via line-range.
    auto *font = _delegate->getTextLabel() ? _delegate->getTextLabel()->getFont() : nullptr;
    if (!font) {
        _caretIdx = _delegate->getString().size();
    } else if (_delegate->getInputMode() == InputMode::ANY) {
        ccstd::vector<LineRange> lines;
        rebuildLineIndex(lines);
        const float lineH = static_cast<float>(font->getLineHeight());
        const float blockTop = (static_cast<int>(lines.size()) - 1) * 0.5f * lineH;
        int bestLi = 0;
        float bestD = 1e9f;
        for (int li = 0; li < static_cast<int>(lines.size()); ++li) {
            const float cy = blockTop - li * lineH;
            const float d = std::fabs(localY - cy);
            if (d < bestD) { bestD = d; bestLi = li; }
        }
        const size_t ls = lines[bestLi].first;
        const size_t ll = lines[bestLi].second;
        _caretIdx = byteIdxAtColumn(ls, ll, localX);
    } else {
        // Single-line — walk the rendered prefix, find the gap closest
        // to localX. Password mask width is uniform so we can use the
        // mask-character advance.
        const ccstd::string &s = _delegate->getString();
        const ccstd::string rendered = renderForDisplay(_delegate, s, false);
        const float total = measureAdvance(*font, rendered, rendered.size());
        float gapX = -total * 0.5f;
        size_t bestIdx = 0;
        float  bestDist = std::fabs(localX - gapX);
        size_t i = 0;
        const bool isPassword = (_delegate->getInputFlag() == InputFlag::PASSWORD);
        while (i < s.size()) {
            const size_t cpEnd = nextCodepoint(s, i);
            float cpAdv = 0.f;
            if (isPassword) {
                if (const auto *mg = font->getGlyph(static_cast<uint32_t>(kPasswordMask))) {
                    cpAdv = static_cast<float>(mg->xadvance);
                }
            } else {
                for (size_t k = i; k < cpEnd; ++k) {
                    if (const auto *g = font->getGlyph(static_cast<unsigned char>(s[k]))) {
                        cpAdv += g->xadvance;
                    }
                }
            }
            gapX += cpAdv;
            const float d = std::fabs(localX - gapX);
            if (d < bestDist) { bestDist = d; bestIdx = cpEnd; }
            i = cpEnd;
        }
        _caretIdx = bestIdx;
    }
    _selAnchor = _caretIdx;
    _mouseDragging = true;
    _preferredCaretXValid = false;
    updateCaretPos();
    updateSelectionHighlight();
    resetCaretBlink();
}

// ────────────────────────────────────────────────────────────────────────
// Visual children
// ────────────────────────────────────────────────────────────────────────

void EditBoxImpl::ensureVisualChildren() {
    if (!_delegate) return;
    Node *parent = _delegate->getNode();
    if (!parent) return;

    if (!_caret) {
        Node *existing = parent->getChildByName("EDIT_CARET");
        if (existing) _caret = existing->getComponent<Sprite>();
    }
    if (!_caret) {
        auto *cn = ccnew Node("EDIT_CARET");
        auto *ui = cn->addComponent<UITransform>();
        ui->setContentSize(2.f, 24.f);
        ui->setAnchorPoint(0.5f, 0.5f);
        _caret = cn->addComponent<Sprite>();
        _caret->setSize(2.f, 24.f);
        // Alpha-driven visibility — node.setActive toggles proved flaky
        // for freshly-created Sprite children when combined with Label
        // re-rendering cycles. Keeping the node active and tweaking
        // alpha avoids the rebuild hazard.
        Color c = _caretColor;
        c.a = 0;
        _caret->setColor(c);
        parent->addChild(cn);
    }

    if (!_selHighlight) {
        Node *existing = parent->getChildByName("EDIT_SELECTION");
        if (existing) _selHighlight = existing->getComponent<Sprite>();
    }
    if (!_selHighlight) {
        auto *sn = ccnew Node("EDIT_SELECTION");
        auto *ui = sn->addComponent<UITransform>();
        ui->setContentSize(0.f, 24.f);
        ui->setAnchorPoint(0.f, 0.5f);
        _selHighlight = sn->addComponent<Sprite>();
        _selHighlight->setSize(0.f, 24.f);
        Color hl = _selectionColor;
        hl.a = 0;
        _selHighlight->setColor(hl);
        parent->addChild(sn);
    }
}

void EditBoxImpl::setCaretVisible(bool on) {
    if (!_caret) {
        CC_LOG_INFO("[EditBox DBG] setCaretVisible(%d) but _caret is null", (int)on);
        return;
    }
    Color c = _caret->getColor();
    c.a = on ? static_cast<uint8_t>(_caretColor.a) : 0;
    _caret->setColor(c);
    const Vec3 &p = _caret->getNode()->getPosition();
    CC_LOG_INFO("[EditBox DBG] setCaretVisible(%d) alpha=%u pos=(%.1f,%.1f)",
                (int)on, c.a, p.x, p.y);
    std::fflush(stdout); std::fflush(stderr);
}

void EditBoxImpl::setSelectionVisible(bool on) {
    if (!_selHighlight) return;
    Color c = _selHighlight->getColor();
    c.a = on ? static_cast<uint8_t>(_selectionColor.a) : 0;
    _selHighlight->setColor(c);
}

// ────────────────────────────────────────────────────────────────────────
// Listener lifecycle
// ────────────────────────────────────────────────────────────────────────

void EditBoxImpl::bindListeners() {
    if (_listeners) return;
    _listeners = new Listeners();

    // Mouse: we only need three things from the global bus —
    //   1. Click-outside-to-blur while editing
    //   2. Drag-select when the user drags past the box edge
    //   3. End-drag on any mouse-up
    // Click-to-focus lives on the delegate's Node::MouseDown handler so
    // it runs only when the hit-test dispatcher gives us priority (not
    // when we're under an overlay).
    _listeners->mouseL.bind([this](const MouseEvent &ev) {
        if (!_editing) return;
        if (ev.button != 0 && ev.type != MouseEvent::Type::MOVE) return;

        if (ev.type == MouseEvent::Type::DOWN) {
            if (!pointInsideBox(ev.x, ev.y)) {
                endEditing();
            }
            return;
        }
        if (ev.type == MouseEvent::Type::MOVE) {
            if (!_mouseDragging) return;
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

    _listeners->keyboardL.bind([this](const KeyboardEvent &ev) {
        _ctrlDown = ev.ctrlKeyActive;
        if (!_editing || !_delegate) return;
        if (ev.action != KeyboardEvent::Action::PRESS &&
            ev.action != KeyboardEvent::Action::REPEAT) return;

        constexpr int KEY_BACKSPACE = 8;
        constexpr int KEY_TAB       = 9;
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

        auto moveTo = [&](size_t newIdx) {
            _caretIdx = newIdx;
            if (!shift) _selAnchor = _caretIdx;
            updateCaretPos();
            updateSelectionHighlight();
            resetCaretBlink();
        };

        ccstd::string &mutableStr = _delegate->_impl_mutableString();

        // Clipboard + select-all first — Ctrl branch shadows the plain
        // switch so character keys don't fall through into insertion.
        if (ctrl) {
            switch (ev.key) {
                case 'A': {
                    _selAnchor = 0;
                    _caretIdx  = mutableStr.size();
                    updateCaretPos();
                    updateSelectionHighlight();
                    resetCaretBlink();
                    return;
                }
                case 'C': {
                    if (hasSelection()) {
                        SDLHelper::setClipboardText(
                            mutableStr.substr(getSelectionStart(),
                                               getSelectionEnd() - getSelectionStart()));
                    }
                    return;
                }
                case 'X': {
                    if (hasSelection()) {
                        SDLHelper::setClipboardText(
                            mutableStr.substr(getSelectionStart(),
                                               getSelectionEnd() - getSelectionStart()));
                        deleteSelection();
                        _delegate->_editBoxTextChanged(mutableStr);
                    }
                    return;
                }
                case 'V': {
                    ccstd::string clip = SDLHelper::getClipboardText();
                    if (clip.empty()) return;
                    // Strip newlines for single-line modes.
                    if (_delegate->getInputMode() != InputMode::ANY) {
                        clip.erase(std::remove(clip.begin(), clip.end(), '\n'), clip.end());
                    }
                    insertText(clip);
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
                    _delegate->_editBoxTextChanged(mutableStr);
                } else if (_caretIdx > 0) {
                    const size_t prev = prevCodepoint(mutableStr, _caretIdx);
                    mutableStr.erase(prev, _caretIdx - prev);
                    _caretIdx = prev;
                    _selAnchor = _caretIdx;
                    _delegate->_impl_refreshDisplay();
                    updateCaretPos();
                    updateSelectionHighlight();
                    _delegate->_editBoxTextChanged(mutableStr);
                }
                break;
            case KEY_DELETE:
                if (hasSelection()) {
                    deleteSelection();
                    _delegate->_editBoxTextChanged(mutableStr);
                } else if (_caretIdx < mutableStr.size()) {
                    const size_t next = nextCodepoint(mutableStr, _caretIdx);
                    mutableStr.erase(_caretIdx, next - _caretIdx);
                    _delegate->_impl_refreshDisplay();
                    updateCaretPos();
                    updateSelectionHighlight();
                    _delegate->_editBoxTextChanged(mutableStr);
                }
                break;
            case KEY_LEFT:
                _preferredCaretXValid = false;
                moveTo(prevCodepoint(mutableStr, _caretIdx));
                break;
            case KEY_RIGHT:
                _preferredCaretXValid = false;
                moveTo(nextCodepoint(mutableStr, _caretIdx));
                break;
            case KEY_HOME: {
                _preferredCaretXValid = false;
                if (_delegate->getInputMode() == InputMode::ANY) {
                    ccstd::vector<LineRange> lines;
                    rebuildLineIndex(lines);
                    const size_t li = lineIndexOf(lines, _caretIdx);
                    moveTo(lines[li].first);
                } else {
                    moveTo(0);
                }
                break;
            }
            case KEY_END: {
                _preferredCaretXValid = false;
                if (_delegate->getInputMode() == InputMode::ANY) {
                    ccstd::vector<LineRange> lines;
                    rebuildLineIndex(lines);
                    const size_t li = lineIndexOf(lines, _caretIdx);
                    moveTo(lines[li].first + lines[li].second);
                } else {
                    moveTo(mutableStr.size());
                }
                break;
            }
            case KEY_UP:
            case KEY_DOWN: {
                if (_delegate->getInputMode() != InputMode::ANY) break;
                ccstd::vector<LineRange> lines;
                rebuildLineIndex(lines);
                const size_t li = lineIndexOf(lines, _caretIdx);
                if (!_preferredCaretXValid) {
                    _preferredCaretX = columnAdvance(lines[li].first, _caretIdx);
                    _preferredCaretXValid = true;
                }
                const int delta = (ev.key == KEY_UP) ? -1 : 1;
                const int target = static_cast<int>(li) + delta;
                if (target < 0 || target >= static_cast<int>(lines.size())) break;
                moveTo(byteIdxAtColumn(lines[target].first, lines[target].second,
                                         _preferredCaretX));
                break;
            }
            case KEY_TAB:
                TabIndexUtil::next(this);
                break;
            case KEY_ENTER:
                if (_delegate->getInputMode() == InputMode::ANY) {
                    // Multiline — insert newline, emit EditingReturn (TS
                    // fires return for each enter-in-multiline as well).
                    if (_delegate->getMaxLength() >= 0 &&
                        codepointCount(mutableStr) >=
                            static_cast<size_t>(_delegate->getMaxLength())) {
                        _delegate->_editBoxEditingReturn();
                        break;
                    }
                    if (hasSelection()) deleteSelection();
                    mutableStr.insert(_caretIdx, "\n");
                    ++_caretIdx;
                    _selAnchor = _caretIdx;
                    _preferredCaretXValid = false;
                    _delegate->_impl_refreshDisplay();
                    updateCaretPos();
                    updateSelectionHighlight();
                    resetCaretBlink();
                    _delegate->_editBoxTextChanged(mutableStr);
                    _delegate->_editBoxEditingReturn();
                } else {
                    // Single-line — return fires, then blur.
                    _delegate->_editBoxEditingReturn();
                    endEditing();
                }
                break;
            case KEY_ESCAPE:
                endEditing();
                break;
            default:
                break;
        }
    });

    _listeners->textL.bind([this](const TextInputEvent &ev) {
        if (!_editing || !_delegate || ev.text.empty()) return;
        if (_ctrlDown) return;
        _composition.clear();

        ccstd::string incoming = ev.text;

        // InputMode filtering — NUMERIC / DECIMAL / PHONE restrict the
        // accepted character set. Others accept anything (mobile
        // keyboards respect the enum; desktop has to enforce it
        // ourselves).
        auto filterOut = [&](auto predicate) {
            incoming.erase(std::remove_if(incoming.begin(), incoming.end(), predicate),
                           incoming.end());
        };
        switch (_delegate->getInputMode()) {
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
                break;
        }
        if (incoming.empty()) return;

        insertText(incoming);
    });

    _listeners->editingL.bind([this](const TextEditingEvent &ev) {
        if (!_editing || !_delegate) return;
        _composition = ev.text;
        // Re-render the label so the caret position is recomputed against
        // the latest authoritative string (composition preview is not
        // drawn separately — IME cursor rendering is a later phase).
        _delegate->_impl_refreshDisplay();
        updateCaretPos();
        updateSelectionHighlight();
    });
}

void EditBoxImpl::unbindListeners() {
    if (_listeners) {
        delete _listeners;
        _listeners = nullptr;
    }
}

// ────────────────────────────────────────────────────────────────────────
// Caret / selection
// ────────────────────────────────────────────────────────────────────────

void EditBoxImpl::resetCaretBlink() {
    _caretVisible = true;
    _caretTimer = 0.f;
    setCaretVisible(_editing);
}

void EditBoxImpl::setCaretIndex(size_t i) {
    if (!_delegate) return;
    const ccstd::string &s = _delegate->getString();
    _caretIdx = std::min(i, s.size());
    while (_caretIdx > 0 && _caretIdx < s.size() &&
           isUtf8Cont(static_cast<unsigned char>(s[_caretIdx]))) {
        --_caretIdx;
    }
    _selAnchor = _caretIdx;
    updateCaretPos();
    resetCaretBlink();
}

size_t EditBoxImpl::getSelectionStart() const { return std::min(_caretIdx, _selAnchor); }
size_t EditBoxImpl::getSelectionEnd()   const { return std::max(_caretIdx, _selAnchor); }

void EditBoxImpl::clearSelection() {
    _selAnchor = _caretIdx;
    updateSelectionHighlight();
}

void EditBoxImpl::deleteSelection() {
    if (!hasSelection() || !_delegate) return;
    auto &s = _delegate->_impl_mutableString();
    const size_t a = getSelectionStart();
    const size_t b = getSelectionEnd();
    s.erase(a, b - a);
    _caretIdx = a;
    _selAnchor = a;
    _delegate->_impl_refreshDisplay();
    updateCaretPos();
    updateSelectionHighlight();
}

void EditBoxImpl::insertText(const ccstd::string &chunk) {
    if (!_delegate) return;
    ccstd::string in = chunk;
    auto &s = _delegate->_impl_mutableString();

    if (hasSelection()) deleteSelection();

    // Enforce maxLength — truncate the incoming chunk to the remaining
    // budget. Matches TS behaviour where the final string (not the
    // keystroke stream) is clipped.
    const int32_t maxLen = _delegate->getMaxLength();
    if (maxLen >= 0) {
        const size_t budget = static_cast<size_t>(maxLen) - codepointCount(s);
        if (budget == 0) return;
        if (codepointCount(in) > budget) {
            in = in.substr(0, byteOffsetOfCodepoint(in, budget));
        }
    }

    s.insert(_caretIdx, in);
    _caretIdx += in.size();
    _selAnchor = _caretIdx;
    _delegate->_impl_refreshDisplay();
    updateCaretPos();
    updateSelectionHighlight();
    resetCaretBlink();
    _delegate->_editBoxTextChanged(s);
}

void EditBoxImpl::updateCaretPos() {
    if (!_caret || !_delegate) return;
    auto *label = _delegate->getTextLabel();
    if (!label) return;
    auto *font = label->getFont();
    if (!font) return;
    const ccstd::string &s = _delegate->getString();

    if (_delegate->getInputMode() == InputMode::ANY) {
        ccstd::vector<LineRange> lines;
        rebuildLineIndex(lines);
        const size_t li = lineIndexOf(lines, _caretIdx);
        const size_t ls = lines[li].first;
        const size_t ll = lines[li].second;

        float lineAdv = 0.f;
        for (size_t i = 0; i < ll; ++i) {
            if (const auto *g = font->getGlyph(static_cast<unsigned char>(s[ls + i]))) {
                lineAdv += g->xadvance;
            }
        }
        const float colX = columnAdvance(ls, _caretIdx);
        const float lineH = static_cast<float>(font->getLineHeight());
        const float blockTop = (static_cast<int>(lines.size()) - 1) * 0.5f * lineH;
        const float baselineY = blockTop - static_cast<int>(li) * lineH;

        Vec3 cp = _caret->getNode()->getPosition();
        cp.x = colX - lineAdv * 0.5f;
        cp.y = baselineY - lineH * 0.5f;
        _caret->getNode()->setPosition(cp);
        return;
    }

    // Single-line: walk the *rendered* (masked) string prefix up to
    // _caretIdx in input codepoints.
    const ccstd::string renderedFull = renderForDisplay(_delegate, s, false);
    ccstd::string renderedPrefix;
    if (_delegate->getInputFlag() == InputFlag::PASSWORD) {
        const size_t cpToCaret = codepointCount(s.substr(0, _caretIdx));
        renderedPrefix.assign(cpToCaret, kPasswordMask);
    } else {
        renderedPrefix = renderForDisplay(_delegate, s.substr(0, _caretIdx), false);
    }
    const float total  = measureAdvance(*font, renderedFull, renderedFull.size());
    const float prefix = measureAdvance(*font, renderedPrefix, renderedPrefix.size());

    Vec3 cp = _caret->getNode()->getPosition();
    cp.x = prefix - total * 0.5f;
    cp.y = 0.f;
    _caret->getNode()->setPosition(cp);
}

void EditBoxImpl::updateSelectionHighlight() {
    if (!_selHighlight || !_delegate) return;
    if (!hasSelection() || !_editing || _delegate->getInputMode() == InputMode::ANY) {
        // Multi-line selection spans multiple rects — MVP-deferred. Hide
        // to avoid a bogus single-line stripe that misrepresents the
        // real selection.
        setSelectionVisible(false);
        return;
    }
    auto *label = _delegate->getTextLabel();
    if (!label || !label->getFont()) {
        setSelectionVisible(false);
        return;
    }
    auto *font = label->getFont();
    const ccstd::string &s = _delegate->getString();
    const ccstd::string renderedFull = renderForDisplay(_delegate, s, false);
    auto renderedPrefix = [&](size_t idx) {
        if (_delegate->getInputFlag() == InputFlag::PASSWORD) {
            const size_t cp = codepointCount(s.substr(0, idx));
            return ccstd::string(cp, kPasswordMask);
        }
        return renderForDisplay(_delegate, s.substr(0, idx), false);
    };
    const float total = measureAdvance(*font, renderedFull, renderedFull.size());
    const ccstd::string preS = renderedPrefix(getSelectionStart());
    const ccstd::string preE = renderedPrefix(getSelectionEnd());
    const float xS = measureAdvance(*font, preS, preS.size()) - total * 0.5f;
    const float xE = measureAdvance(*font, preE, preE.size()) - total * 0.5f;
    const float width = std::max(1.f, xE - xS);
    _selHighlight->setSize(width, 24.f);
    if (auto *ui = _selHighlight->getNode()->getComponent<UITransform>()) {
        ui->setContentSize(width, 24.f);
    }
    _selHighlight->getNode()->setPosition(Vec3{xS, 0.f, 0.f});
    setSelectionVisible(true);
}

// ────────────────────────────────────────────────────────────────────────
// Hit-test / coord conversion
// ────────────────────────────────────────────────────────────────────────

bool EditBoxImpl::pointInsideBox(float winX, float winY) const {
    if (!_delegate) return false;
    Node *node = _delegate->getNode();
    if (!node) return false;
    auto *ui = node->getComponent<UITransform>();
    if (!ui) return false;

    float winW = 0, winH = 0;
    nearestCanvasSize(node, winW, winH);
    const float wx = winX - winW * 0.5f;
    const float wy = winH * 0.5f - winY;
    return ui->hitTestWorld(wx, wy);
}

size_t EditBoxImpl::windowPointToCaretIndex(float winX, float winY) const {
    if (!_delegate) return 0;
    auto *label = _delegate->getTextLabel();
    if (!label) return _delegate->getString().size();
    auto *font = label->getFont();
    const ccstd::string &s = _delegate->getString();
    if (!font || s.empty()) return 0;

    Node *node = _delegate->getNode();
    if (!node) return s.size();

    if (!_composition.empty()) return _caretIdx;  // don't reposition during IME

    float winW = 0, winH = 0;
    nearestCanvasSize(node, winW, winH);
    const float wx = winX - winW * 0.5f;
    const float wy = winH * 0.5f - winY;

    const Mat4 boxInv = node->getWorldMatrix().getInversed();
    const float bx = boxInv.m[0] * wx + boxInv.m[4] * wy + boxInv.m[12];
    const float by = boxInv.m[1] * wx + boxInv.m[5] * wy + boxInv.m[13];

    if (_delegate->getInputMode() == InputMode::ANY) {
        ccstd::vector<LineRange> lines;
        rebuildLineIndex(lines);
        const float lineH = static_cast<float>(font->getLineHeight());
        const float blockTop = (static_cast<int>(lines.size()) - 1) * 0.5f * lineH;
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

    // Single-line.
    const ccstd::string renderedFull = renderForDisplay(_delegate, s, false);
    const float total = measureAdvance(*font, renderedFull, renderedFull.size());
    float gapX = -total * 0.5f;
    size_t bestIdx = 0;
    float  bestDist = std::fabs(bx - gapX);
    size_t i = 0;
    const bool isPassword = (_delegate->getInputFlag() == InputFlag::PASSWORD);
    while (i < s.size()) {
        const size_t cpEnd = nextCodepoint(s, i);
        float cpAdv = 0.f;
        if (isPassword) {
            if (const auto *mg = font->getGlyph(static_cast<uint32_t>(kPasswordMask))) {
                cpAdv = static_cast<float>(mg->xadvance);
            }
        } else {
            for (size_t k = i; k < cpEnd; ++k) {
                if (const auto *g = font->getGlyph(static_cast<unsigned char>(s[k]))) {
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

// ────────────────────────────────────────────────────────────────────────
// Multi-line helpers
// ────────────────────────────────────────────────────────────────────────

void EditBoxImpl::rebuildLineIndex(ccstd::vector<LineRange> &out) const {
    out.clear();
    if (!_delegate) { out.push_back({0, 0}); return; }
    const ccstd::string &s = _delegate->getString();
    size_t start = 0;
    for (size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || s[i] == '\n') {
            out.push_back({start, i - start});
            start = i + 1;
        }
    }
    if (out.empty()) out.push_back({0, 0});
}

size_t EditBoxImpl::lineIndexOf(const ccstd::vector<LineRange> &lines, size_t byteIdx) const {
    for (size_t i = 0; i < lines.size(); ++i) {
        const size_t s = lines[i].first;
        const size_t e = s + lines[i].second;
        if (byteIdx >= s && byteIdx <= e) return i;
    }
    return lines.empty() ? 0 : lines.size() - 1;
}

float EditBoxImpl::columnAdvance(size_t lineStart, size_t byteIdx) const {
    if (!_delegate) return 0.f;
    auto *label = _delegate->getTextLabel();
    if (!label || !label->getFont()) return 0.f;
    // Font::getGlyph is non-const because TTF fonts lazily rasterise
    // on first use; the physical caret math here doesn't care, it
    // just needs the metrics. Casting away const keeps this helper's
    // const-correctness at the caret-math level.
    auto *font = label->getFont();
    const ccstd::string &s = _delegate->getString();
    float adv = 0.f;
    for (size_t i = lineStart; i < byteIdx && i < s.size(); ++i) {
        if (const auto *g = font->getGlyph(static_cast<unsigned char>(s[i]))) {
            adv += g->xadvance;
        }
    }
    return adv;
}

size_t EditBoxImpl::byteIdxAtColumn(size_t lineStart, size_t lineLen, float targetX) const {
    if (!_delegate) return lineStart;
    auto *label = _delegate->getTextLabel();
    if (!label || !label->getFont()) return lineStart;
    auto *font = label->getFont();  // non-const: see columnAdvance note
    const ccstd::string &s = _delegate->getString();

    float lineAdv = 0.f;
    for (size_t i = 0; i < lineLen; ++i) {
        if (const auto *g = font->getGlyph(static_cast<unsigned char>(s[lineStart + i]))) {
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
        const size_t cpEnd = nextCodepoint(s, i);
        for (size_t k = i; k < cpEnd; ++k) {
            if (const auto *g = font->getGlyph(static_cast<unsigned char>(s[k]))) {
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
