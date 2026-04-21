#include "cocos/ui/editBox/EditBoxImpl.h"

#include <algorithm>
#include <chrono>
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

// Label.updateGeometry renders every glyph at `xadvance * scale`, where
// scale = fontSize / baseFontSize. Caret, selection highlight and
// click→caret-index math all need this same factor so native-unit pen
// measurements line up with what the Label actually paints; without it
// the error accumulates linearly with text length.
float labelDrawScale(const Label *label) {
    if (!label) return 1.f;
    const float fs   = label->getFontSize();
    const auto *font = label->getFont();
    const float base = font ? static_cast<float>(font->getBaseFontSize()) : 0.f;
    if (fs <= 0.f || base <= 0.f) return 1.f;
    return fs / base;
}

// Label content-box rect in its parent (EditBox) local space. All caret,
// selection and hit-test math anchors to these numbers so the position
// of a caret drawn at index N matches where the Label's Nth glyph sits.
//
// For the caret's PRIMARY axis (X for single-line, X within a line for
// multi-line) LEFT-align puts glyphs against `leftX`; CENTER / RIGHT
// recentre around the content-box mid/right. The centre-Y sits on the
// content-box centre so single-line boxes keep matching the Label's
// single-line CENTER vAlign. Multi-line uses `topY` directly and
// decrements by lineH per visual row.
struct LabelRect {
    float leftX{0.f};
    float rightX{0.f};
    float topY{0.f};     // highest Y (content-box top edge in EditBox space)
    float bottomY{0.f};  // lowest Y
    float width{0.f};
    float height{0.f};
};

// Small breathing gap so the caret doesn't kiss the previous glyph's
// right edge. Matches what Cocos Creator ships visually.
constexpr float kCaretGlyphGap = 2.f;

LabelRect labelContentRect(const Label *label) {
    LabelRect r;
    if (!label || !label->getNode()) return r;
    const Vec3 lp = label->getNode()->getPosition();
    float sx = 0.f, sy = 0.f, ax = 0.5f, ay = 0.5f;
    if (auto *ui = label->getNode()->getComponent<UITransform>()) {
        sx = ui->getContentSize().x;
        sy = ui->getContentSize().y;
        ax = ui->getAnchorPoint().x;
        ay = ui->getAnchorPoint().y;
    }
    r.leftX   = lp.x - ax * sx;
    r.rightX  = r.leftX + sx;
    r.bottomY = lp.y - ay * sy;
    r.topY    = r.bottomY + sy;
    r.width   = sx;
    r.height  = sy;
    return r;
}

// Nearest UITransform walking up from `from` (skipping `from` itself).
// Used for window→local math when the raw global mouse listener fires —
// we need both the canvas size (to recentre window coords) and the
// canvas node's world position (so the centred coords line up with
// the same world space the main dispatcher's hit-tests run in).
// Editor-authored prefabs usually put their Canvas node at the design-
// resolution centre e.g. (640, 360); if we ignored that offset the
// recentred coords would sit at origin and every pointInsideBox check
// would miss the box.
void nearestCanvasSize(Node *from, float &w, float &h,
                       float &canvasWorldX, float &canvasWorldY) {
    w = 1280.f; h = 720.f;
    canvasWorldX = 0.f; canvasWorldY = 0.f;
    Node *n = from ? from->getParent() : nullptr;
    for (; n; n = n->getParent()) {
        if (auto *ui = n->getComponent<UITransform>()) {
            w = ui->getContentSize().x;
            h = ui->getContentSize().y;
            const Vec3 &wp = n->getWorldPosition();
            canvasWorldX = wp.x;
            canvasWorldY = wp.y;
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

uint64_t nowMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
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
    _selHighlights.clear();
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
    for (auto *sp : _selHighlights) {
        if (!sp) continue;
        if (auto *ui = sp->getNode()->getComponent<UITransform>()) {
            const Vec2 &cs = ui->getContentSize();
            ui->setContentSize(cs.x, caretH);
        }
        sp->setSize(sp->getSize().x, caretH);
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
    _caretLastBlinkMs = nowMs();
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

void EditBoxImpl::tick(float /*dt*/) {
    if (!_editing || !_caret) return;
    const uint64_t now = nowMs();
    if (now - _caretLastBlinkMs < kCaretBlinkPeriodMs) return;
    _caretLastBlinkMs = now;
    _caretVisible = !_caretVisible;
    Color c = _caret->getColor();
    c.a = _caretVisible ? static_cast<uint8_t>(_caretColor.a) : 0;
    _caret->setColor(c);
}

// ────────────────────────────────────────────────────────────────────────
// Delegate → impl click entry-point
// ────────────────────────────────────────────────────────────────────────

void EditBoxImpl::onDelegateNodeClick(float winX, float winY) {
    if (!_delegate) return;
    if (!_editing) beginEditing();
    // Same window→box-local → gap-scan path drag uses on mouse MOVE,
    // so a click lands on the same character gap the caret would see
    // if the user had dragged up to that point.
    _caretIdx = windowPointToCaretIndex(winX, winY);
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

    if (_selHighlights.empty()) {
        // Reuse a single legacy EDIT_SELECTION child if the prefab was
        // saved with the old name (pre-multi-line rework).
        if (Node *existing = parent->getChildByName("EDIT_SELECTION")) {
            if (auto *sp = existing->getComponent<Sprite>()) {
                _selHighlights.push_back(sp);
            }
        }
    }
    if (_selHighlights.empty()) {
        _selHighlights.push_back(allocSelectionRect(parent, 0));
    }
}

// Create (or fetch cached) Nth selection highlight Sprite under `parent`.
// Per-line rect nodes are named EDIT_SELECTION_N so legacy prefabs and
// fresh multi-line spans coexist.
Sprite *EditBoxImpl::allocSelectionRect(Node *parent, size_t idx) {
    const ccstd::string name = "EDIT_SELECTION_" + std::to_string(idx);
    if (Node *existing = parent->getChildByName(name.c_str())) {
        if (auto *sp = existing->getComponent<Sprite>()) return sp;
    }
    auto *sn = ccnew Node(name.c_str());
    auto *ui = sn->addComponent<UITransform>();
    ui->setContentSize(0.f, 24.f);
    ui->setAnchorPoint(0.f, 0.5f);  // pin left edge to node position
    auto *sp = sn->addComponent<Sprite>();
    sp->setSize(0.f, 24.f);
    Color hl = _selectionColor;
    hl.a = 0;
    sp->setColor(hl);
    parent->addChild(sn);
    return sp;
}

void EditBoxImpl::setCaretVisible(bool on) {
    if (!_caret) return;
    Color c = _caret->getColor();
    c.a = on ? static_cast<uint8_t>(_caretColor.a) : 0;
    _caret->setColor(c);
}

void EditBoxImpl::setSelectionVisible(bool on) {
    // Hide every cached highlight rect. Multi-line selections repopulate
    // just the ones they need each tick via updateSelectionHighlight; the
    // extras stay alpha=0 until needed again.
    for (auto *sp : _selHighlights) {
        if (!sp) continue;
        Color c = sp->getColor();
        c.a = on ? static_cast<uint8_t>(_selectionColor.a) : 0;
        sp->setColor(c);
    }
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
                    // Store preferred X in RENDERED pixels from line start
                    // so byteIdxAtColumn can match directly (it walks
                    // glyphs in the same pixel space).
                    const float scale = labelDrawScale(
                        _delegate->getTextLabel());
                    _preferredCaretX =
                        columnAdvance(lines[li].first, _caretIdx) * scale;
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
    _caretLastBlinkMs = nowMs();
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

        const float drawScale = labelDrawScale(label);
        const float colX      = columnAdvance(ls, _caretIdx) * drawScale;
        // Use the Label's own line height so caret's per-line stride
        // matches the text layout. Label applies the same fallback:
        // authored `_lineHeight` if > 0, else the font's native lineH
        // scaled by fontSize/baseFontSize.
        const float nativeLineH = static_cast<float>(font->getLineHeight()) * drawScale;
        const float lineH = (label->getLineHeight() > 0.f)
                                ? label->getLineHeight()
                                : nativeLineH;
        const float fontPx = (label->getFontSize() > 0.f
                                  ? label->getFontSize()
                                  : static_cast<float>(font->getBaseFontSize()))
                             * drawScale;
        const LabelRect rect = labelContentRect(label);

        // Label's TOP-vAlign places line N's PEN Y at `contentTop - N*lineH`.
        // Glyph bodies hang below each pen; caret sits on the visible
        // stroke centre, empirically `pen - fontPx/2 - lineH/4`.
        Vec3 cp = _caret->getNode()->getPosition();
        cp.x = rect.leftX + colX + kCaretGlyphGap;
        cp.y = rect.topY
             - static_cast<float>(li) * lineH
             - fontPx * 0.5f
             - lineH * 0.20f;
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
    const float drawScale = labelDrawScale(label);
    const float total  = measureAdvance(*font, renderedFull, renderedFull.size()) * drawScale;
    const float prefix = measureAdvance(*font, renderedPrefix, renderedPrefix.size()) * drawScale;

    // Caret lives in EditBox-local space; Label has its own lpos/anchor set
    // by the Editor prefab. Place the caret against the Label's content-box
    // so it sits where the text actually renders, honouring the Label's
    // horizontal alignment.
    float labelOriginX = 0.f;
    float labelOriginY = 0.f;
    auto *labelNode = label->getNode();
    if (labelNode) {
        const Vec3 lp = labelNode->getPosition();
        labelOriginX = lp.x;
        labelOriginY = lp.y;
        if (auto *lui = labelNode->getComponent<UITransform>()) {
            const Vec2 sz = lui->getContentSize();
            const Vec2 ap = lui->getAnchorPoint();
            const float leftEdge = lp.x - ap.x * sz.x;
            switch (label->getHorizontalAlign()) {
                case Label::HorizontalAlign::LEFT:
                    labelOriginX = leftEdge;
                    break;
                case Label::HorizontalAlign::CENTER:
                    labelOriginX = leftEdge + sz.x * 0.5f - total * 0.5f;
                    break;
                case Label::HorizontalAlign::RIGHT:
                    labelOriginX = leftEdge + sz.x - total;
                    break;
            }
            labelOriginY = lp.y + (0.5f - ap.y) * sz.y;
        }
    }

    Vec3 cp = _caret->getNode()->getPosition();
    cp.x = labelOriginX + prefix + kCaretGlyphGap;
    cp.y = labelOriginY;
    _caret->getNode()->setPosition(cp);
}

// Place (or reuse) one highlight rect covering the range [xLeft, xRight]
// at vertical centre `yCentre`, with the given height. Idempotent across
// frames — only the first `usedCount` highlights this frame end up
// visible; the rest are left alpha=0 for reuse next time.
void EditBoxImpl::setHighlightRect(size_t idx, float xLeft, float xRight,
                                   float yCentre, float height) {
    if (!_delegate) return;
    auto *parent = _delegate->getNode();
    if (!parent) return;
    while (_selHighlights.size() <= idx) {
        _selHighlights.push_back(allocSelectionRect(parent, _selHighlights.size()));
    }
    Sprite *sp = _selHighlights[idx];
    if (!sp) return;
    const float width = std::max(1.f, xRight - xLeft);
    sp->setSize(width, height);
    if (auto *ui = sp->getNode()->getComponent<UITransform>()) {
        ui->setContentSize(width, height);
    }
    sp->getNode()->setPosition(Vec3{xLeft, yCentre, 0.f});
    Color c = _selectionColor;
    c.a = static_cast<uint8_t>(_selectionColor.a);
    sp->setColor(c);
}

void EditBoxImpl::hideHighlightsFrom(size_t firstIdx) {
    for (size_t i = firstIdx; i < _selHighlights.size(); ++i) {
        Sprite *sp = _selHighlights[i];
        if (!sp) continue;
        Color c = sp->getColor();
        c.a = 0;
        sp->setColor(c);
    }
}

void EditBoxImpl::updateSelectionHighlight() {
    if (!_delegate) return;
    if (!hasSelection() || !_editing) {
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
    const float drawScale = labelDrawScale(label);

    const size_t selS = getSelectionStart();
    const size_t selE = getSelectionEnd();
    const bool isPassword = (_delegate->getInputFlag() == InputFlag::PASSWORD);
    auto prefixWithin = [&](size_t lineStart, size_t byteIdx) {
        if (isPassword) {
            const size_t cp = codepointCount(
                s.substr(lineStart, byteIdx - lineStart));
            return static_cast<float>(cp) *
                   static_cast<float>(font->getGlyph(kPasswordMask)
                       ? font->getGlyph(kPasswordMask)->xadvance : 0);
        }
        float adv = 0.f;
        for (size_t i = lineStart; i < byteIdx; ++i) {
            if (const auto *g = font->getGlyph(
                    static_cast<unsigned char>(s[i]))) {
                adv += g->xadvance;
            }
        }
        return adv;
    };

    if (_delegate->getInputMode() == InputMode::ANY) {
        ccstd::vector<LineRange> lines;
        rebuildLineIndex(lines);
        if (lines.empty()) { setSelectionVisible(false); return; }

        const float nativeLineH = static_cast<float>(font->getLineHeight()) * drawScale;
        const float lineH = (label->getLineHeight() > 0.f)
                                ? label->getLineHeight()
                                : nativeLineH;
        const float fontPx = (label->getFontSize() > 0.f
                                  ? label->getFontSize()
                                  : static_cast<float>(font->getBaseFontSize()))
                             * drawScale;
        const LabelRect rect = labelContentRect(label);
        const float rectH = fontPx;  // highlight height matches caret / glyph body

        size_t used = 0;
        for (size_t li = 0; li < lines.size(); ++li) {
            const size_t lineStart = lines[li].first;
            const size_t lineEnd   = lineStart + lines[li].second;
            if (selE <= lineStart || selS >= lineEnd) continue;  // outside

            const size_t sx = std::max(selS, lineStart);
            const size_t ex = std::min(selE, lineEnd);
            const float x0 = rect.leftX +
                prefixWithin(lineStart, sx) * drawScale;
            const float x1 = rect.leftX +
                prefixWithin(lineStart, ex) * drawScale;
            // Same centre-Y formula the caret uses.
            const float yCentre = rect.topY
                                - static_cast<float>(li) * lineH
                                - fontPx * 0.5f
                                - lineH * 0.20f;
            setHighlightRect(used++, x0, x1, yCentre, rectH);
        }
        hideHighlightsFrom(used);
        return;
    }

    // Single-line.
    const ccstd::string renderedFull = renderForDisplay(_delegate, s, false);
    const float total = measureAdvance(*font, renderedFull,
                                       renderedFull.size()) * drawScale;
    auto renderedPrefix = [&](size_t idx) {
        if (isPassword) {
            const size_t cp = codepointCount(s.substr(0, idx));
            return ccstd::string(cp, kPasswordMask);
        }
        return renderForDisplay(_delegate, s.substr(0, idx), false);
    };
    const ccstd::string preS = renderedPrefix(selS);
    const ccstd::string preE = renderedPrefix(selE);
    const float pS = measureAdvance(*font, preS, preS.size()) * drawScale;
    const float pE = measureAdvance(*font, preE, preE.size()) * drawScale;

    // Same label-content-box anchoring as updateCaretPos, so the highlight
    // sits under the actual glyph stripe regardless of Label lpos/anchor.
    float labelOriginX = 0.f;
    float labelOriginY = 0.f;
    if (label->getNode()) {
        const Vec3 lp = label->getNode()->getPosition();
        labelOriginX = lp.x;
        labelOriginY = lp.y;
        if (auto *lui = label->getNode()->getComponent<UITransform>()) {
            const Vec2 sz = lui->getContentSize();
            const Vec2 ap = lui->getAnchorPoint();
            const float leftEdge = lp.x - ap.x * sz.x;
            switch (label->getHorizontalAlign()) {
                case Label::HorizontalAlign::LEFT:
                    labelOriginX = leftEdge; break;
                case Label::HorizontalAlign::CENTER:
                    labelOriginX = leftEdge + sz.x * 0.5f - total * 0.5f; break;
                case Label::HorizontalAlign::RIGHT:
                    labelOriginX = leftEdge + sz.x - total; break;
            }
            labelOriginY = lp.y + (0.5f - ap.y) * sz.y;
        }
    }

    setHighlightRect(0, labelOriginX + pS, labelOriginX + pE,
                     labelOriginY, 24.f);
    hideHighlightsFrom(1);
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

    float winW = 0, winH = 0, cwx = 0, cwy = 0;
    nearestCanvasSize(node, winW, winH, cwx, cwy);
    const float wx = winX - winW * 0.5f + cwx;
    const float wy = winH * 0.5f - winY + cwy;
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

    float winW = 0, winH = 0, cwx = 0, cwy = 0;
    nearestCanvasSize(node, winW, winH, cwx, cwy);
    const float wx = winX - winW * 0.5f + cwx;
    const float wy = winH * 0.5f - winY + cwy;

    const Mat4 boxInv = node->getWorldMatrix().getInversed();
    const float bx = boxInv.m[0] * wx + boxInv.m[4] * wy + boxInv.m[12];
    const float by = boxInv.m[1] * wx + boxInv.m[5] * wy + boxInv.m[13];

    if (_delegate->getInputMode() == InputMode::ANY) {
        ccstd::vector<LineRange> lines;
        rebuildLineIndex(lines);
        const float drawScale   = labelDrawScale(label);
        const float nativeLineH = static_cast<float>(font->getLineHeight()) * drawScale;
        const float lineH = (label->getLineHeight() > 0.f)
                                ? label->getLineHeight()
                                : nativeLineH;
        const LabelRect rect  = labelContentRect(label);

        // Vertical pick: find the line whose cell contains `by`. Clamp to
        // first / last line when the click falls outside the text block.
        int bestLi = 0;
        if (!lines.empty()) {
            const int lastLi = static_cast<int>(lines.size()) - 1;
            if (by >= rect.topY) {
                bestLi = 0;
            } else {
                const float row = (rect.topY - by) / lineH;
                bestLi = std::clamp(static_cast<int>(std::floor(row)), 0, lastLi);
            }
        }
        const size_t ls = lines[bestLi].first;
        const size_t ll = lines[bestLi].second;
        // Horizontal pick: express bx relative to the Label's LEFT edge,
        // then scan glyph advances (scaled) for the closest gap.
        return byteIdxAtColumn(ls, ll, bx - rect.leftX);
    }

    // Single-line.
    const ccstd::string renderedFull = renderForDisplay(_delegate, s, false);
    const float drawScale = labelDrawScale(label);
    const float total = measureAdvance(*font, renderedFull, renderedFull.size()) * drawScale;
    // Seed gapX at the text's LEFT edge in EditBox-local space so drag /
    // hit-test agrees with the caret/selection placement math.
    float gapX = 0.f;
    if (label->getNode()) {
        const Vec3 lp = label->getNode()->getPosition();
        gapX = lp.x;
        if (auto *lui = label->getNode()->getComponent<UITransform>()) {
            const Vec2 sz = lui->getContentSize();
            const Vec2 ap = lui->getAnchorPoint();
            const float leftEdge = lp.x - ap.x * sz.x;
            switch (label->getHorizontalAlign()) {
                case Label::HorizontalAlign::LEFT:
                    gapX = leftEdge; break;
                case Label::HorizontalAlign::CENTER:
                    gapX = leftEdge + sz.x * 0.5f - total * 0.5f; break;
                case Label::HorizontalAlign::RIGHT:
                    gapX = leftEdge + sz.x - total; break;
            }
        }
    }
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
        cpAdv *= drawScale;
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
    // When the Label auto-wraps, its `_visualLines` is the authoritative
    // break map — caret / hit-test / selection must follow the same
    // wraps the Label paints. Copy it out. Label refreshes this during
    // its rebuildForRender (one frame behind on the very first tick,
    // which is acceptable — the empty-string bootstrap has 1 line).
    if (auto *label = _delegate->getTextLabel()) {
        if (label->isWrapEnabled()) {
            // Force a refresh so the line map reflects the CURRENT text
            // (setText + updateCaretPos happen in the same call for key
            // insertion; without this, the caret would read last frame's
            // wrap and appear on the prior line edge when a character
            // just pushed the line over the wrap limit).
            label->recomputeVisualLines();
            const auto &vl = label->getVisualLines();
            if (!vl.empty()) {
                out.assign(vl.begin(), vl.end());
                return;
            }
        }
    }
    // Fallback: split on explicit '\n' only.
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

// targetX is measured from the Label's content-box LEFT edge (in pixels,
// same scale the Label actually paints — i.e. xadvance × fontSize/baseFontSize).
// Returns the byte index whose preceding-prefix width is closest to targetX.
size_t EditBoxImpl::byteIdxAtColumn(size_t lineStart, size_t lineLen, float targetX) const {
    if (!_delegate) return lineStart;
    auto *label = _delegate->getTextLabel();
    if (!label || !label->getFont()) return lineStart;
    auto *font = label->getFont();  // non-const: see columnAdvance note
    const ccstd::string &s = _delegate->getString();
    const float drawScale = labelDrawScale(label);

    float adv = 0.f;  // pen position from line start, in rendered pixels
    size_t best = lineStart;
    float  bestDist = std::fabs(targetX - adv);
    size_t i = lineStart;
    const size_t lineEnd = lineStart + lineLen;
    while (i < lineEnd) {
        const size_t cpEnd = nextCodepoint(s, i);
        float cpAdv = 0.f;
        for (size_t k = i; k < cpEnd; ++k) {
            if (const auto *g = font->getGlyph(static_cast<unsigned char>(s[k]))) {
                cpAdv += g->xadvance;
            }
        }
        adv += cpAdv * drawScale;
        const float d = std::fabs(targetX - adv);
        if (d < bestDist) { bestDist = d; best = cpEnd; }
        i = cpEnd;
    }
    return best;
}

}  // namespace cc
