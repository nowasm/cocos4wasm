#include "cocos/ui/components/Button.h"

#include <algorithm>

#include "base/Log.h"
#include "core/scene-graph/Node.h"

namespace cc {

CC_IMPLEMENT_CLASS(Button, "cc.Button", Component)
    .property("interactable",  &Button::_interactable, true)
    .property("normalColor",   &Button::_normalColor,   Color{230, 230, 230, 255})
    .property("hoverColor",    &Button::_hoverColor,    Color{255, 255, 255, 255})
    .property("pressedColor",  &Button::_pressedColor,  Color{160, 160, 160, 255})
    .property("disabledColor", &Button::_disabledColor, Color{120, 120, 120, 128})
CC_END_CLASS(Button);

// Stashed event-listener ids per Button instance — kept out of the header
// so the Node event-template goo doesn't leak into clients that include
// Button.h. Heap-allocated on onEnable, freed on onDisable.
struct Button::HandlerIds {
    cc::event::TargetEventID<Node::MouseEnter> enterId;
    cc::event::TargetEventID<Node::MouseLeave> leaveId;
    cc::event::TargetEventID<Node::MouseDown>  downId;
    cc::event::TargetEventID<Node::MouseUp>    upId;
};

void Button::onEnable() {
    // Auto-resolve the sprite target from the owning node. Users can
    // override with setTargetSprite() if they want a child's sprite tinted.
    if (!_sprite) {
        if (auto *n = getNode()) {
            _sprite = n->getComponent<Sprite>();
        }
    }
    _enabledVisual = true;
    bindHandlers();
    transitionTo(_interactable ? State::NORMAL : State::DISABLED);
}

void Button::onDisable() {
    unbindHandlers();
    _enabledVisual = false;
    _pressed = false;
    _hovered = false;
}

void Button::setInteractable(bool v) {
    if (_interactable == v) return;
    _interactable = v;
    if (_enabledVisual) {
        transitionTo(v ? (_hovered ? State::HOVER : State::NORMAL) : State::DISABLED);
    }
}

void Button::setTargetSprite(Sprite *s) {
    _sprite = s;
    if (_enabledVisual) applyStateTint();
}

Button::ListenerID Button::addClickListener(ClickHandler fn) {
    const ListenerID id = _nextListenerId++;
    _clickListeners.emplace_back(id, std::move(fn));
    return id;
}

bool Button::removeClickListener(ListenerID id) {
    auto it = std::find_if(_clickListeners.begin(), _clickListeners.end(),
                           [id](const auto &p) { return p.first == id; });
    if (it == _clickListeners.end()) return false;
    _clickListeners.erase(it);
    return true;
}

void Button::bindHandlers() {
    auto *node = getNode();
    if (!node || _hids) return;
    _hids = new HandlerIds();

    _hids->enterId = node->on<Node::MouseEnter>(
        [this](Node * /*self*/, const NodeMouseEventArg & /*arg*/) {
            _hovered = true;
            if (!_interactable) return;
            // Preserve pressed state if user is holding the button down and
            // happens to re-enter; otherwise show Hover.
            transitionTo(_pressed ? State::PRESSED : State::HOVER);
        });

    _hids->leaveId = node->on<Node::MouseLeave>(
        [this](Node * /*self*/, const NodeMouseEventArg & /*arg*/) {
            _hovered = false;
            // Leaving mid-press cancels the click arm — the subsequent
            // MouseUp will land on whatever node is under the cursor, not
            // on us.
            _pressed = false;
            if (!_interactable) return;
            transitionTo(State::NORMAL);
        });

    _hids->downId = node->on<Node::MouseDown>(
        [this](Node * /*self*/, const NodeMouseEventArg &arg) {
            if (!_interactable) return;
            if (arg.button != 0) return;  // left mouse only for now
            _pressed = true;
            transitionTo(State::PRESSED);
        });

    _hids->upId = node->on<Node::MouseUp>(
        [this](Node * /*self*/, const NodeMouseEventArg &arg) {
            if (!_interactable) return;
            if (arg.button != 0) return;
            const bool wasArmed = _pressed;
            _pressed = false;
            transitionTo(_hovered ? State::HOVER : State::NORMAL);
            if (wasArmed) {
                // Snapshot the listener list so a handler removing itself
                // (or adding siblings) doesn't invalidate our iterator.
                auto listeners = _clickListeners;
                for (auto &p : listeners) {
                    p.second(this);
                }
            }
        });
}

void Button::unbindHandlers() {
    auto *node = getNode();
    if (!_hids) return;
    if (node) {
        node->off<Node::MouseEnter>(_hids->enterId);
        node->off<Node::MouseLeave>(_hids->leaveId);
        node->off<Node::MouseDown> (_hids->downId);
        node->off<Node::MouseUp>   (_hids->upId);
    }
    delete _hids;
    _hids = nullptr;
}

void Button::transitionTo(State s) {
    if (_state == s) return;
    _state = s;
    applyStateTint();
}

void Button::applyStateTint() {
    if (!_sprite) return;
    switch (_state) {
        case State::NORMAL:   _sprite->setColor(_normalColor);   break;
        case State::HOVER:    _sprite->setColor(_hoverColor);    break;
        case State::PRESSED:  _sprite->setColor(_pressedColor);  break;
        case State::DISABLED: _sprite->setColor(_disabledColor); break;
    }
}

}  // namespace cc
