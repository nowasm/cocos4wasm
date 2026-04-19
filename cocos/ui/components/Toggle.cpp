#include "cocos/ui/components/Toggle.h"

#include "cocos/2d/components/Sprite.h"
#include "cocos/ui/components/ToggleContainer.h"
#include "core/scene-graph/Node.h"

namespace cc {

CC_IMPLEMENT_CLASS(Toggle, "cc.Toggle", Button)
    .property("_isChecked",  &Toggle::_isChecked, true)
    .property("_checkMark",  &Toggle::_checkMark)
    .property("checkEvents", &Toggle::_checkEvents)
CC_END_CLASS(Toggle);

// Toggle's own hooks live beside the inherited Button hooks. Button's
// HandlerIds is private to Button.cpp; we can't peek into it, so we
// add a separate ButtonClick listener to respond to clicks.
struct Toggle::ToggleHooks {
    cc::event::TargetEventID<Node::ButtonClick> clickId;
};

void Toggle::onEnable() {
    Button::onEnable();  // base state machine + mouse handlers

    auto *node = getNode();
    if (node && !_tHooks) {
        _tHooks = new ToggleHooks();
        _tHooks->clickId = node->on<Node::ButtonClick>(
            [this](Node *, Button *b) {
                if (b != static_cast<Button *>(this)) return;
                _onOwnClick();
            });
    }
    _syncMark();
}

void Toggle::onDisable() {
    auto *node = getNode();
    if (_tHooks && node) {
        node->off<Node::ButtonClick>(_tHooks->clickId);
    }
    if (_tHooks) { delete _tHooks; _tHooks = nullptr; }
    Button::onDisable();
}

void Toggle::setIsChecked(bool v) {
    if (_isChecked == v) return;
    _isChecked = v;
    _syncMark();
    _fireToggle();
}

void Toggle::setIsCheckedWithoutNotify(bool v) {
    if (_isChecked == v) return;
    _isChecked = v;
    _syncMark();
}

void Toggle::setCheckMark(Sprite *s) {
    _checkMark = s;
    _syncMark();
}

void Toggle::_syncMark() {
    if (!_checkMark) return;
    _checkMark->getNode()->setActive(_isChecked);
}

void Toggle::_fireToggle() {
    // Upstream order: node.emit(TOGGLE, this) → checkEvents.emit → (Container).
    // Node event is still reserved for a future TARGET_EVENT(ToggleChanged);
    // for now we fire the two handler vectors, Editor-authored first.
    reflection::MethodArgs args;
    args.push_back(reflection::MethodArg::makePointer(this));
    ComponentEventHandler::emitEvents(_checkEvents, args);

    auto snap = _runtimeCheckListeners;
    for (auto &fn : snap) fn(this);
}

void Toggle::_onOwnClick() {
    // In a ToggleContainer, clicking an already-checked toggle is allowed
    // to uncheck it only if the container permits it; otherwise the
    // click is absorbed without state change.
    if (_container) {
        const bool wantChecked = !_isChecked;
        if (!wantChecked) {
            // TS: if allowSwitchOff is false AND this is the last checked
            // one, do nothing.
            if (!_container->isSwitchOffAllowed()) {
                // Count other checked in the group.
                auto active = _container->getActiveToggles();
                if (active.size() <= 1 && std::find(active.begin(), active.end(), this) != active.end()) {
                    return;
                }
            }
        }
        _isChecked = wantChecked;
        _syncMark();
        _fireToggle();
        _container->notifyToggleCheck(this, true);
    } else {
        _isChecked = !_isChecked;
        _syncMark();
        _fireToggle();
    }
}

}  // namespace cc
