#include "cocos/ui/components/Button.h"

#include <algorithm>
#include <cmath>

#include "base/Log.h"
#include "core/scene-graph/Node.h"

namespace cc {

CC_IMPLEMENT_CLASS(Button, "cc.Button", Component)
    .property("_interactable",  &Button::_interactable,  true)
    .property("_transition",    &Button::_transition,    Transition::NONE)
    .property("_normalColor",   &Button::_normalColor,   Color{255, 255, 255, 255})
    .property("_hoverColor",    &Button::_hoverColor,    Color{211, 211, 211, 255})
    .property("_pressedColor",  &Button::_pressedColor,  Color{255, 255, 255, 255})
    .property("_disabledColor", &Button::_disabledColor, Color{124, 124, 124, 255})
    .property("_duration",      &Button::_duration,      0.1f)
    .property("_zoomScale",     &Button::_zoomScale,     1.2f)
CC_END_CLASS(Button);

struct Button::HandlerIds {
    cc::event::TargetEventID<Node::MouseEnter> enterId;
    cc::event::TargetEventID<Node::MouseLeave> leaveId;
    cc::event::TargetEventID<Node::MouseDown>  downId;
    cc::event::TargetEventID<Node::MouseUp>    upId;
};

// ────────────────────────────────────────────────────────────────────────
// Lifecycle
// ────────────────────────────────────────────────────────────────────────

void Button::onEnable() {
    _enabledVisual = true;
    if (!_originalScaleCaptured) {
        if (auto *n = getNode()) {
            _originalScale = n->getScale();
            _originalScaleCaptured = true;
        }
    }
    bindHandlers();
    _pressed = false;
    _hovered = false;
    _state = State::NORMAL;
    applyInstantState(_interactable ? State::NORMAL : State::DISABLED);
}

void Button::onDisable() {
    unbindHandlers();
    _enabledVisual = false;
    _pressed = false;
    _hovered = false;
    _transitionDirty = false;
}

void Button::update(float dt) {
    if (!_transitionDirty) return;
    stepAnimation(dt);
}

// ────────────────────────────────────────────────────────────────────────
// Target
// ────────────────────────────────────────────────────────────────────────

Node *Button::getTarget() const {
    return _target ? _target : getNode();
}

void Button::setTarget(Node *n) {
    _target = n;
    if (_enabledVisual) {
        _originalScaleCaptured = false;
        if (auto *t = getTarget()) {
            _originalScale = t->getScale();
            _originalScaleCaptured = true;
        }
        applyInstantState(_state);
    }
}

Sprite *Button::resolveTargetSprite() const {
    auto *t = getTarget();
    return t ? t->getComponent<Sprite>() : nullptr;
}

// ────────────────────────────────────────────────────────────────────────
// Interactable / transition / colors
// ────────────────────────────────────────────────────────────────────────

void Button::setInteractable(bool v) {
    if (_interactable == v) return;
    _interactable = v;
    if (!_enabledVisual) return;
    if (!v) {
        _pressed = false;
        transitionTo(State::DISABLED);
    } else {
        transitionTo(_hovered ? State::HOVER : State::NORMAL);
    }
}

void Button::setTransition(Transition v) {
    if (_transition == v) return;
    _transition = v;
    if (_enabledVisual) applyInstantState(_state);
}

void Button::setNormalColor(const Color &c) {
    _normalColor = c;
    if (_enabledVisual && _state == State::NORMAL) applyInstantState(_state);
}

// ────────────────────────────────────────────────────────────────────────
// Event wiring
// ────────────────────────────────────────────────────────────────────────

void Button::bindHandlers() {
    auto *node = getNode();
    if (!node || _hids) return;
    _hids = new HandlerIds();

    _hids->enterId = node->on<Node::MouseEnter>(
        [this](Node *, const NodeMouseEventArg &) {
            _hovered = true;
            if (!_interactable) return;
            transitionTo(_pressed ? State::PRESSED : State::HOVER);
        });

    _hids->leaveId = node->on<Node::MouseLeave>(
        [this](Node *, const NodeMouseEventArg &) {
            _hovered = false;
            _pressed = false;  // leaving mid-press cancels the arm
            if (!_interactable) return;
            transitionTo(State::NORMAL);
        });

    _hids->downId = node->on<Node::MouseDown>(
        [this](Node *, const NodeMouseEventArg &arg) {
            if (!_interactable) return;
            if (arg.button != 0) return;
            _pressed = true;
            transitionTo(State::PRESSED);
        });

    _hids->upId = node->on<Node::MouseUp>(
        [this](Node *self, const NodeMouseEventArg &arg) {
            if (!_interactable) return;
            if (arg.button != 0) return;
            const bool armed = _pressed;
            _pressed = false;
            transitionTo(_hovered ? State::HOVER : State::NORMAL);
            if (armed) {
                // Emit on node (TS parity) + fire local vector.
                self->dispatchEvent<Node::ButtonClick>(this);
                auto snap = _clickEvents;
                for (auto &fn : snap) fn(this);
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

// ────────────────────────────────────────────────────────────────────────
// State → visual
// ────────────────────────────────────────────────────────────────────────

Color Button::colorForState(State s) const {
    switch (s) {
        case State::NORMAL:   return _normalColor;
        case State::HOVER:    return _hoverColor;
        case State::PRESSED:  return _pressedColor;
        case State::DISABLED: return _disabledColor;
    }
    return _normalColor;
}

Texture2D *Button::spriteForState(State s) const {
    switch (s) {
        case State::NORMAL:   return _normalSprite.get();
        case State::HOVER:    return _hoverSprite.get();
        case State::PRESSED:  return _pressedSprite.get();
        case State::DISABLED: return _disabledSprite.get();
    }
    return nullptr;
}

void Button::applyInstantState(State s) {
    _state = s;
    _transitionDirty = false;
    switch (_transition) {
        case Transition::NONE:
            break;
        case Transition::COLOR:
            if (auto *sp = resolveTargetSprite()) sp->setColor(colorForState(s));
            break;
        case Transition::SPRITE:
            if (auto *sp = resolveTargetSprite()) {
                if (auto *tex = spriteForState(s)) sp->setTexture(tex);
            }
            break;
        case Transition::SCALE:
            if (auto *t = getTarget()) {
                if (!_originalScaleCaptured) {
                    _originalScale = t->getScale();
                    _originalScaleCaptured = true;
                }
                Vec3 tgt = _originalScale;
                if (s == State::PRESSED) {
                    tgt.x *= _zoomScale;
                    tgt.y *= _zoomScale;
                    tgt.z *= _zoomScale;
                }
                t->setScale(tgt);
            }
            break;
    }
}

void Button::transitionTo(State s) {
    if (_state == s) return;
    if (_transition == Transition::NONE || _transition == Transition::SPRITE ||
        _duration <= 0.f) {
        applyInstantState(s);
        return;
    }

    _state = s;
    _animTime = 0.f;
    _transitionDirty = true;

    if (_transition == Transition::COLOR) {
        if (auto *sp = resolveTargetSprite()) {
            _fromColor = sp->getColor();
        } else {
            _fromColor = colorForState(_state);
        }
        _toColor = colorForState(s);
    } else if (_transition == Transition::SCALE) {
        if (auto *t = getTarget()) {
            if (!_originalScaleCaptured) {
                _originalScale = t->getScale();
                _originalScaleCaptured = true;
            }
            _fromScale = t->getScale();
            _toScale = _originalScale;
            if (s == State::PRESSED) {
                _toScale.x *= _zoomScale;
                _toScale.y *= _zoomScale;
                _toScale.z *= _zoomScale;
            }
        }
    }
}

void Button::stepAnimation(float dt) {
    _animTime += dt;
    float t = (_duration > 0.f) ? (_animTime / _duration) : 1.f;
    if (t >= 1.f) { t = 1.f; _transitionDirty = false; }

    if (_transition == Transition::COLOR) {
        if (auto *sp = resolveTargetSprite()) {
            Color c;
            c.r = static_cast<uint8_t>(_fromColor.r + (_toColor.r - _fromColor.r) * t);
            c.g = static_cast<uint8_t>(_fromColor.g + (_toColor.g - _fromColor.g) * t);
            c.b = static_cast<uint8_t>(_fromColor.b + (_toColor.b - _fromColor.b) * t);
            c.a = static_cast<uint8_t>(_fromColor.a + (_toColor.a - _fromColor.a) * t);
            sp->setColor(c);
        }
    } else if (_transition == Transition::SCALE) {
        if (auto *n = getTarget()) {
            Vec3 s;
            s.x = _fromScale.x + (_toScale.x - _fromScale.x) * t;
            s.y = _fromScale.y + (_toScale.y - _fromScale.y) * t;
            s.z = _fromScale.z + (_toScale.z - _fromScale.z) * t;
            n->setScale(s);
        }
    }
}

}  // namespace cc
