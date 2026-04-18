#pragma once

#include <functional>

#include "base/Ptr.h"
#include "base/std/container/vector.h"
#include "cocos/2d/components/Sprite.h"
#include "core/component/Component.h"
#include "core/reflection/Reflection.h"
#include "math/Color.h"

namespace cc {

// Interactable button — wires per-state colour tints onto a Sprite on the
// same node and fires click callbacks when a MouseDown/Up pair lands within
// the node's bounds without being cancelled by a MouseLeave.
//
// Visual states map to tint colours (normal → hover → pressed → disabled).
// Sprite-frame swap transitions land in a later phase; for now COLOR is the
// only transition available. The target Sprite is auto-resolved from the
// owning node at onEnable — put the Button on the same node as the Sprite
// you want tinted.
//
// Click listeners fire after a confirmed "mouse-up-while-still-inside".
// MouseLeave during the pressed phase cancels the arm flag so moving off
// the button before releasing does NOT trigger a click (matches Cocos
// Creator and web-button semantics).
class Button : public Component {
    CC_CLASS_DECL(Button, Component)
public:
    using ClickHandler = std::function<void(Button *)>;

    Button() = default;
    ~Button() override = default;

    void onEnable() override;
    void onDisable() override;

    bool isInteractable() const { return _interactable; }
    void setInteractable(bool v);

    const Color &getNormalColor()   const { return _normalColor; }
    const Color &getHoverColor()    const { return _hoverColor; }
    const Color &getPressedColor()  const { return _pressedColor; }
    const Color &getDisabledColor() const { return _disabledColor; }

    void setNormalColor(const Color &c)   { _normalColor   = c; if (_enabledVisual) applyStateTint(); }
    void setHoverColor(const Color &c)    { _hoverColor    = c; if (_enabledVisual) applyStateTint(); }
    void setPressedColor(const Color &c)  { _pressedColor  = c; if (_enabledVisual) applyStateTint(); }
    void setDisabledColor(const Color &c) { _disabledColor = c; if (_enabledVisual) applyStateTint(); }

    // Callback registration. Use removeClickListener(id) to detach — the
    // returned id is an opaque token, not an index into the vector (slot
    // stability survives other listeners being removed).
    using ListenerID = size_t;
    ListenerID addClickListener(ClickHandler fn);
    bool       removeClickListener(ListenerID id);

    Sprite *getTargetSprite() const { return _sprite.get(); }
    void    setTargetSprite(Sprite *s);

private:
    enum class State { NORMAL, HOVER, PRESSED, DISABLED };

    void bindHandlers();
    void unbindHandlers();
    void transitionTo(State s);
    void applyStateTint();

    Color _normalColor  {230, 230, 230, 255};
    Color _hoverColor   {255, 255, 255, 255};
    Color _pressedColor {160, 160, 160, 255};
    Color _disabledColor{120, 120, 120, 128};
    bool  _interactable{true};

    State _state{State::NORMAL};

    // Armed between MouseDown and MouseUp on the owning node. MouseLeave
    // clears it so leaving the button mid-press cancels the click.
    bool _pressed{false};
    bool _hovered{false};

    // Cached Sprite component on the same node. Resolved at onEnable. If
    // nullptr the button still handles events but does not tint.
    IntrusivePtr<Sprite> _sprite;

    ccstd::vector<std::pair<ListenerID, ClickHandler>> _clickListeners;
    ListenerID _nextListenerId{1};

    // Set true once onEnable has wired handlers; flipped off at onDisable.
    // Guards setter paths from applying a tint before we know what sprite
    // to target.
    bool _enabledVisual{false};

    // Retained so we can detach them cleanly from the owning Node.
    struct HandlerIds;
    HandlerIds *_hids{nullptr};
};

}  // namespace cc
