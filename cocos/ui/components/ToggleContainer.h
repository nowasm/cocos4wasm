#pragma once

#include <functional>

#include "base/std/container/vector.h"
#include "core/component/Component.h"
#include "core/reflection/Reflection.h"

namespace cc {

class Toggle;

// Mirrors cocos/ui/toggle-container.ts. Groups sibling Toggles into a
// radio-group — at most one checked at a time. When allowSwitchOff is
// false, at least one toggle must always be checked (the last click on
// the currently-checked toggle is absorbed).
class ToggleContainer : public Component {
    CC_CLASS_DECL(ToggleContainer, Component)
public:
    using Handler = std::function<void(Toggle *)>;

    ToggleContainer() = default;
    ~ToggleContainer() override = default;

    void onEnable() override;
    void onDisable() override;

    bool isSwitchOffAllowed() const { return _allowSwitchOff; }
    void setAllowSwitchOff(bool v) { _allowSwitchOff = v; }

    // toggleItems — child nodes with an enabled Toggle component.
    ccstd::vector<Toggle *> getToggleItems() const;
    ccstd::vector<Toggle *> getActiveToggles() const;  // currently checked
    bool                    anyTogglesChecked() const;

    // Called by a child Toggle on state change. emitEvent=true fires
    // checkEvents on the container too.
    void notifyToggleCheck(Toggle *toggle, bool emitEvent = true);

    // Re-validate after setup: if allowSwitchOff=false and nothing is
    // checked, checks the first available toggle.
    void ensureValidState();

    // checkEvents handler vector — fires for every toggle-group change.
    void addCheckListener(Handler fn) { _checkEvents.push_back(std::move(fn)); }
    void clearCheckListeners() { _checkEvents.clear(); }

private:
    bool _allowSwitchOff{false};
    ccstd::vector<Handler> _checkEvents;
};

}  // namespace cc
