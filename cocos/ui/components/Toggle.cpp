#include "cocos/ui/components/Toggle.h"

#include "cocos/2d/components/Sprite.h"
#include "cocos/ui/components/ToggleContainer.h"
#include "core/scene-graph/Node.h"

namespace cc {

CC_IMPLEMENT_CLASS(Toggle, "cc.Toggle", Button)
    .property("_isChecked", &Toggle::_isChecked, true)
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
    auto snap = _checkEvents;
    for (auto &fn : snap) fn(this);
    if (auto *n = getNode()) {
        // We don't have a ToggleClick typed Node event — fire a generic
        // ButtonClick so any existing subscribers still see the change
        // (Creator's Toggle dispatches a string-keyed 'toggle' event via
        // node.emit; our closest equivalent without adding another Node
        // event is the per-component handler vector, which most callers
        // use in practice).
        (void)n;  // reserved for a future TARGET_EVENT(ToggleChanged)
    }
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
