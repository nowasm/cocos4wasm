#include "cocos/ui/components/EditBox.h"

#include "base/Log.h"
#include "cocos/2d/components/Label.h"
#include "cocos/2d/components/Sprite.h"
#include "cocos/2d/framework/UITransform.h"
#include "cocos/2d/text/BmfFont.h"
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
    events::Mouse::Listener     mouseL;
    events::Keyboard::Listener  keyboardL;
    events::TextInput::Listener textL;
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

}  // namespace

void EditBox::onEnable() {
    ensureChildren();
    refreshLabel();
    updateCaretPos();
    if (_caret) _caret->getNode()->setActive(false);  // hidden until focus

    _hooks = new Hooks();

    _hooks->mouseL.bind([this](const MouseEvent &ev) {
        if (ev.type != MouseEvent::Type::DOWN || ev.button != 0) return;
        const bool inside = pointInside(ev.x, ev.y);
        if (inside && !_focused) {
            focus();
        } else if (!inside && _focused) {
            blur();
        }
    });

    _hooks->keyboardL.bind([this](const KeyboardEvent &ev) {
        if (!_focused) return;
        if (ev.action != KeyboardEvent::Action::PRESS) return;

        // KeyCode values are declared alongside MouseEvent in
        // EngineEvents.h — ASCII-ish for navigation keys.
        constexpr int KEY_BACKSPACE = 8;
        constexpr int KEY_ENTER     = 13;
        constexpr int KEY_ESCAPE    = 27;

        switch (ev.key) {
            case KEY_BACKSPACE:
                if (!_text.empty()) {
                    // Trim one UTF-8 sequence: step back while the tail
                    // byte is a continuation (10xxxxxx) so multi-byte
                    // codepoints delete cleanly.
                    size_t cut = _text.size();
                    while (cut > 0 &&
                           (static_cast<unsigned char>(_text[cut - 1]) & 0xC0) == 0x80) {
                        --cut;
                    }
                    if (cut > 0) --cut;
                    _text.resize(cut);
                    refreshLabel();
                    updateCaretPos();
                    if (_onChanged) _onChanged(this, _text);
                }
                break;
            case KEY_ENTER:
            case KEY_ESCAPE:
                blur();
                break;
            default:
                // Printable characters arrive via TextInput; nothing to
                // do here. Arrow keys / Home / End for caret movement
                // will land when EditBox grows past MVP.
                break;
        }
    });

    _hooks->textL.bind([this](const TextInputEvent &ev) {
        if (!_focused || ev.text.empty()) return;
        _text.append(ev.text);
        refreshLabel();
        updateCaretPos();
        if (_onChanged) _onChanged(this, _text);
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
}

float EditBox::_padLeft() const {
    // Hard-coded padding for MVP — EditBox lays its label just inside
    // the background sprite's left edge. When padding becomes authored
    // this returns whatever the user configured.
    auto *n = getNode();
    auto *ui = n ? n->getComponent<UITransform>() : nullptr;
    return ui ? ui->getContentSize().x * 0.5f - 8.f : 0.f;
}

void EditBox::refreshLabel() {
    if (!_label) return;
    if (!_text.empty()) {
        _label->setText(_text);
        _label->setColor(_textColor);
    } else {
        _label->setText(_placeholder);
        _label->setColor(_placeholderColor);
    }
}

void EditBox::updateCaretPos() {
    if (!_caret || !_label) return;
    const auto *font = _label->getFont();
    if (!font) return;
    // Measure the displayed text up through the current caret position
    // (always at end for the MVP). The Label centres its text, so the
    // pen starts at -totalAdvance/2 — caret sits at that endpoint.
    float advance = 0.f;
    for (unsigned char c : _text) {
        if (const auto *g = font->getGlyph(c)) advance += g->xadvance;
    }
    // Label node's local-X aligns the label's centre with whatever
    // position we set in ensureChildren. Caret local-X = labelNode.x +
    // advance/2 (because Label centres text on the origin).
    Vec3 cp = _caret->getNode()->getPosition();
    cp.x = _label->getNode()->getPosition().x + advance * 0.5f + 2.f;
    cp.y = 0.f;
    _caret->getNode()->setPosition(cp);
}

void EditBox::setText(const ccstd::string &t) {
    if (_text == t) return;
    _text = t;
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
    CC_LOG_INFO("[EditBox] blurred");
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
