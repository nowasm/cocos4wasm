#include "cocos/ui/components/ToggleContainer.h"

#include "cocos/ui/components/Toggle.h"
#include "core/scene-graph/Node.h"

namespace cc {

CC_IMPLEMENT_CLASS(ToggleContainer, "cc.ToggleContainer", Component)
    .property("_allowSwitchOff", &ToggleContainer::_allowSwitchOff, false)
    .property("checkEvents",     &ToggleContainer::_checkEvents)
CC_END_CLASS(ToggleContainer);

void ToggleContainer::onEnable() {
    // Register this container on every child Toggle. A Toggle attached
    // after onEnable needs to call _setContainer itself — matching TS's
    // deferred-wiring pattern is overkill for MVP.
    for (Toggle *t : getToggleItems()) {
        t->_setContainer(this);
    }
    ensureValidState();
}

void ToggleContainer::onDisable() {
    for (Toggle *t : getToggleItems()) {
        t->_setContainer(nullptr);
    }
}

ccstd::vector<Toggle *> ToggleContainer::getToggleItems() const {
    ccstd::vector<Toggle *> out;
    auto *node = getNode();
    if (!node) return out;
    for (const auto &c : node->getChildren()) {
        Node *child = c.get();
        if (!child) continue;
        if (auto *t = child->getComponent<Toggle>()) out.push_back(t);
    }
    return out;
}

ccstd::vector<Toggle *> ToggleContainer::getActiveToggles() const {
    ccstd::vector<Toggle *> out;
    for (Toggle *t : getToggleItems()) {
        if (t->isChecked()) out.push_back(t);
    }
    return out;
}

bool ToggleContainer::anyTogglesChecked() const {
    for (Toggle *t : getToggleItems()) {
        if (t->isChecked()) return true;
    }
    return false;
}

void ToggleContainer::notifyToggleCheck(Toggle *toggle, bool emitEvent) {
    // Enforce radio semantics: if the newly-clicked toggle is now
    // checked, every sibling becomes unchecked.
    if (toggle && toggle->isChecked()) {
        for (Toggle *other : getToggleItems()) {
            if (other != toggle && other->isChecked()) {
                other->setIsCheckedWithoutNotify(false);
            }
        }
    }

    if (emitEvent) {
        reflection::MethodArgs args;
        args.push_back(reflection::MethodArg::makePointer(toggle));
        ComponentEventHandler::emitEvents(_checkEvents, args);

        auto snap = _runtimeCheckListeners;
        for (auto &fn : snap) fn(toggle);
    }
}

void ToggleContainer::ensureValidState() {
    if (_allowSwitchOff) return;
    if (anyTogglesChecked()) return;
    auto items = getToggleItems();
    if (!items.empty()) {
        items[0]->setIsCheckedWithoutNotify(true);
    }
}

}  // namespace cc
