#pragma once

namespace cc {

class EditBox;

// Mirrors `cocos/ui/editbox/edit-box-impl-base.ts`. Pure virtual
// platform-backend contract — EditBox (component) talks to the outside
// world through this abstraction, and concrete implementations bind
// themselves to OS input.
//
// Lifecycle follows the TS base class 1:1:
//   • init(delegate)       — bind, register listeners
//   • onEnable / onDisable — scene-level active/inactive hooks (base
//                            implementation of onDisable flushes any
//                            active editing session, as TS does)
//   • beforeDraw           — per-frame tick, ordered before render
//   • clear                — teardown, called from the component's
//                            onDestroy
//   • setTabIndex          — focus-chain ordering hint
//   • setSize              — mirror the EditBox content size
//   • setFocus / isFocused — focus control (base routes to
//                            beginEditing / endEditing, same as TS)
//   • beginEditing / endEditing — concrete implementations switch on
//                                 OS input (SDL text input, DOM input,
//                                 native keyboard, …)
class EditBoxImplBase {
public:
    EditBoxImplBase() = default;
    virtual ~EditBoxImplBase() = default;

    virtual void init(EditBox *delegate) { _delegate = delegate; }
    virtual void onEnable() {}
    virtual void beforeDraw() {}

    // Per-frame tick from the delegate's update(). Default no-op — the
    // SDL impl uses this to drive caret blink.
    virtual void tick(float /*dt*/) {}

    // Matches TS ImplBase.onDisable: if we're mid-edit, close it.
    virtual void onDisable() {
        if (_editing) endEditing();
    }

    // Matches TS ImplBase.clear: drop the delegate pointer. Concrete
    // impls override and release listeners/resources before calling
    // this base.
    virtual void clear() { _delegate = nullptr; }

    virtual void setTabIndex(int /*index*/) {}
    virtual void setSize(float /*width*/, float /*height*/) {}

    void setFocus(bool value) {
        if (value) beginEditing();
        else       endEditing();
    }

    bool isFocused() const { return _editing; }

    virtual void beginEditing() {}
    virtual void endEditing() {}

    // Accessors — TS treats these as `@deprecated public` private
    // interfaces; we expose them so the SDL impl's global mouse hook can
    // query the delegate without re-running getters.
    EditBox *getDelegate() const { return _delegate; }

protected:
    bool     _editing{false};
    EditBox *_delegate{nullptr};
};

}  // namespace cc
