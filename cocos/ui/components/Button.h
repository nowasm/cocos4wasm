#pragma once

#include <functional>

#include "base/Ptr.h"
#include "base/std/container/vector.h"
#include "cocos/2d/components/Sprite.h"
#include "core/assets/Texture2D.h"
#include "core/component/Component.h"
#include "core/reflection/Reflection.h"
#include "math/Color.h"
#include "math/Vec3.h"

namespace cc {

class Node;

// Mirrors cocos/ui/button.ts — same enums, same property names, same
// event semantics. The only deviations are:
//   • SpriteFrame → Texture2D * (the SpriteFrame asset wrapper isn't
//     ported yet; when it lands the four state pointers change type
//     without touching the rest of the API).
//   • ComponentEventHandler[] → std::vector<Handler>. Creator's serialised
//     (target+method-name) tuples aren't a useful pattern in pure C++;
//     we subscribe std::function handlers programmatically.
//
// Transitions:
//   NONE   — no visual feedback beyond node.active changes
//   COLOR  — lerp between normalColor / hoverColor / pressedColor /
//             disabledColor over `duration` seconds
//   SPRITE — swap the target Sprite's spriteFrame between four assets
//   SCALE  — lerp between the original scale and original×zoomScale on
//             press, over `duration` seconds
//
// State machine:
//   NORMAL  ← onEnable / blur
//   HOVER   ← MouseEnter (desktop) while interactable
//   PRESSED ← MouseDown inside while interactable
//   DISABLED ← setInteractable(false)
//
// Click event fires on MouseUp that lands while still hovered and armed.
// Moving off mid-press cancels the click (the hover/leave handlers drop
// `_pressed`).
class Button : public Component {
    CC_CLASS_DECL(Button, Component)
public:
    // 过渡方式 — numeric layout matches TS.
    enum class Transition : uint32_t {
        NONE   = 0,
        COLOR  = 1,
        SPRITE = 2,
        SCALE  = 3,
    };

    // ButtonEventType — value matches TS string enum so scripts that
    // compare against `"click"` keep working.
    struct EventType {
        static constexpr const char *CLICK = "click";
    };

    using Handler = std::function<void(Button *)>;

    Button() { _wantsUpdate = true; }
    ~Button() override = default;

    // ── Component lifecycle ──────────────────────────────────────────────
    void onLoad() override { /* reflection hook; state applied at onEnable */ }
    void onEnable() override;
    void onDisable() override;
    void update(float dt) override;

    // ── Target node (visual feedback target) ─────────────────────────────
    // Defaults to `this.node`. Setting to another node makes the
    // transition affect that node's Sprite instead.
    Node *getTarget() const;
    void  setTarget(Node *n);

    // ── Interactable flag ────────────────────────────────────────────────
    bool isInteractable() const { return _interactable; }
    void setInteractable(bool v);

    // ── Transition type + per-type properties ────────────────────────────
    Transition getTransition() const { return _transition; }
    void       setTransition(Transition v);

    // COLOR transition properties.
    const Color &getNormalColor()   const { return _normalColor;   }
    const Color &getHoverColor()    const { return _hoverColor;    }
    const Color &getPressedColor()  const { return _pressedColor;  }
    const Color &getDisabledColor() const { return _disabledColor; }
    void setNormalColor  (const Color &c);
    void setHoverColor   (const Color &c) { _hoverColor    = c; }
    void setPressedColor (const Color &c) { _pressedColor  = c; }
    void setDisabledColor(const Color &c) { _disabledColor = c; }

    // Transition duration (COLOR + SCALE). Seconds.
    float getDuration() const { return _duration; }
    void  setDuration(float v) { _duration = v; }

    // Press zoom scale (SCALE transition). 1.2 = 20% larger.
    float getZoomScale() const { return _zoomScale; }
    void  setZoomScale(float v) { _zoomScale = v; }

    // SPRITE transition — per-state textures. Null values fall back to
    // whatever the target Sprite currently shows (no swap on that state).
    Texture2D *getNormalSprite()   const { return _normalSprite.get();   }
    Texture2D *getHoverSprite()    const { return _hoverSprite.get();    }
    Texture2D *getPressedSprite()  const { return _pressedSprite.get();  }
    Texture2D *getDisabledSprite() const { return _disabledSprite.get(); }
    void setNormalSprite  (Texture2D *t) { _normalSprite   = t; }
    void setHoverSprite   (Texture2D *t) { _hoverSprite    = t; }
    void setPressedSprite (Texture2D *t) { _pressedSprite  = t; }
    void setDisabledSprite(Texture2D *t) { _disabledSprite = t; }

    // ── Click listeners ──────────────────────────────────────────────────
    // Matches Creator's `clickEvents: ComponentEventHandler[]` — handlers
    // fire in registration order, a handler can remove itself safely
    // (iteration runs on a snapshot).
    void addClickListener(Handler fn)       { _clickEvents.push_back(std::move(fn)); }
    void clearClickListeners()              { _clickEvents.clear(); }
    size_t getClickListenerCount() const    { return _clickEvents.size(); }

private:
    enum class State { NORMAL, HOVER, PRESSED, DISABLED };

    void bindHandlers();
    void unbindHandlers();
    void transitionTo(State s);
    // Called each frame while _transitionDirty to lerp COLOR / SCALE.
    void stepAnimation(float dt);

    Sprite *resolveTargetSprite() const;  // Sprite on _target or own node
    void    applyInstantState(State s);   // jump directly to a state's visuals
    Color   colorForState(State s) const;
    Texture2D *spriteForState(State s) const;

    // ── Serialisable state ──────────────────────────────────────────────
    Node *_target{nullptr};  // weak — no ref-count (matches TS target)
    bool  _interactable{true};
    Transition _transition{Transition::NONE};

    Color _normalColor  {255, 255, 255, 255};
    Color _hoverColor   {211, 211, 211, 255};
    Color _pressedColor {255, 255, 255, 255};
    Color _disabledColor{124, 124, 124, 255};

    float _duration{0.1f};
    float _zoomScale{1.2f};

    IntrusivePtr<Texture2D> _normalSprite;
    IntrusivePtr<Texture2D> _hoverSprite;
    IntrusivePtr<Texture2D> _pressedSprite;
    IntrusivePtr<Texture2D> _disabledSprite;

    ccstd::vector<Handler> _clickEvents;

    // ── Runtime state ────────────────────────────────────────────────────
    State _state{State::NORMAL};
    bool  _pressed{false};
    bool  _hovered{false};

    // COLOR-transition lerp.
    Color _fromColor{255, 255, 255, 255};
    Color _toColor  {255, 255, 255, 255};
    float _animTime{0.f};           // elapsed time in current animation
    bool  _transitionDirty{false};  // true while an animation is pending

    // SCALE-transition lerp.
    Vec3  _originalScale{1.f, 1.f, 1.f};
    Vec3  _fromScale{1.f, 1.f, 1.f};
    Vec3  _toScale{1.f, 1.f, 1.f};
    bool  _originalScaleCaptured{false};

    bool _enabledVisual{false};

    struct HandlerIds;
    HandlerIds *_hids{nullptr};
};

}  // namespace cc
