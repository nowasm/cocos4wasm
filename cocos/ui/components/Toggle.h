#pragma once

#include <functional>

#include "base/Ptr.h"
#include "base/std/container/vector.h"
#include "cocos/ui/components/Button.h"
#include "core/scene-graph/ComponentEventHandler.h"

namespace cc {

class Sprite;
class ToggleContainer;

// Mirrors cocos/ui/toggle.ts. Extends Button, adds a `checkMark` Sprite
// and a boolean `isChecked`. Clicking flips the checked state; the mark
// sprite is visible when checked.
//
// Event model matches TS:
//   • 'toggle' fires on node whenever the checked state changes (both
//     via user click AND programmatic setIsChecked).
//   • `checkEvents[]` handler vector fires alongside the Node event —
//     Editor-authored handlers run before C++ runtime subscribers.
class Toggle : public Button {
    CC_CLASS_DECL(Toggle, Button)
public:
    // Re-export Button's Handler signature so demo code can stay short.
    using Handler = std::function<void(Toggle *)>;

    Toggle() = default;
    ~Toggle() override = default;

    void onEnable() override;
    void onDisable() override;

    bool isChecked() const { return _isChecked; }
    // Setting via setIsChecked fires the 'toggle' event; the
    // -WithoutNotify variant is for ToggleContainer's silent updates.
    void setIsChecked(bool v);
    void setIsCheckedWithoutNotify(bool v);

    Sprite *getCheckMark() const { return _checkMark; }
    void    setCheckMark(Sprite *s);

    // Creator merges ButtonEventType + ToggleEventType into
    // Toggle.EventType. We expose just the toggle string for parity.
    struct ToggleEventType {
        static constexpr const char *TOGGLE = "toggle";
    };

    // C++ runtime subscribers — fired after the serialized `checkEvents`
    // array. Order within each group is registration order.
    void addCheckListener(Handler fn) { _runtimeCheckListeners.push_back(std::move(fn)); }
    void clearCheckListeners()        { _runtimeCheckListeners.clear(); }

    // Editor-authored check handlers, serialized as `checkEvents`.
    ccstd::vector<IntrusivePtr<ComponentEventHandler>>       &getCheckEvents()       { return _checkEvents; }
    const ccstd::vector<IntrusivePtr<ComponentEventHandler>> &getCheckEvents() const { return _checkEvents; }

    // Called by ToggleContainer to register/deregister. Not part of the
    // public inspector surface — mirrors TS private wiring.
    void _setContainer(ToggleContainer *c) { _container = c; }
    ToggleContainer *_getContainer() const { return _container; }

private:
    void _syncMark();
    void _fireToggle();
    void _onOwnClick();

    bool             _isChecked{true};
    Sprite          *_checkMark{nullptr};
    ToggleContainer *_container{nullptr};

    // Editor-serialized handler list (`cc.ClickEvent[]` in JSON).
    ccstd::vector<IntrusivePtr<ComponentEventHandler>> _checkEvents;

    // C++ runtime subscribers.
    ccstd::vector<Handler> _runtimeCheckListeners;

    struct ToggleHooks;
    ToggleHooks *_tHooks{nullptr};
};

}  // namespace cc
