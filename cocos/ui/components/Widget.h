#pragma once

#include "core/component/Component.h"
#include "core/reflection/Reflection.h"

namespace cc {

class Node;
class UITransform;

// Mirrors cocos/ui/widget.ts. Same enum values, property names and
// semantics; percentage-vs-pixel mode is controlled by `isAbsoluteXxx`
// flags matching the TS flip toggles.
//
// AlignMode:
//   ONCE              — align once on onEnable, then stop.
//   ALWAYS            — re-align every frame regardless of input.
//   ON_WINDOW_RESIZE  — align once plus whenever the target (or window)
//                       resizes.
//
// Target semantics: when `target` is null the direct parent is used
// (Creator default). Explicit target lets a widget anchor to a non-
// parent node (e.g. a screen root that isn't in the same subtree).
class Widget : public Component {
    CC_CLASS_DECL(Widget, Component)
public:
    // AlignFlags — bit values match cocos/ui/widget.ts. HORIZONTAL and
    // VERTICAL convenience masks match TS (56 = LEFT|CENTER|RIGHT;
    // 7 = TOP|MID|BOT).
    enum AlignFlags : uint32_t {
        NONE   = 0,
        TOP    = 1u << 0,
        MID    = 1u << 1,  // vertical centre
        BOT    = 1u << 2,
        LEFT   = 1u << 3,
        CENTER = 1u << 4,  // horizontal centre
        RIGHT  = 1u << 5,

        HORIZONTAL = LEFT | CENTER | RIGHT,
        VERTICAL   = TOP  | MID    | BOT,
    };

    // AlignMode — numeric values match TS.
    enum class AlignMode : uint32_t {
        ONCE             = 0,
        ALWAYS           = 1,
        ON_WINDOW_RESIZE = 2,
    };

    Widget() = default;
    ~Widget() override = default;

    void onEnable() override;
    void onDisable() override;

    // ── target ────────────────────────────────────────────────────────
    // Null falls back to the direct parent. Matches TS `target` getter.
    Node *getTarget() const { return _target; }
    void  setTarget(Node *n);

    // ── Alignment bit flags ───────────────────────────────────────────
    uint32_t getAlignFlags() const { return _alignFlags; }
    void     setAlignFlags(uint32_t v);

    bool isAlignTop()              const { return (_alignFlags & TOP)    != 0; }
    bool isAlignBottom()           const { return (_alignFlags & BOT)    != 0; }
    bool isAlignLeft()             const { return (_alignFlags & LEFT)   != 0; }
    bool isAlignRight()            const { return (_alignFlags & RIGHT)  != 0; }
    bool isAlignVerticalCenter()   const { return (_alignFlags & MID)    != 0; }
    bool isAlignHorizontalCenter() const { return (_alignFlags & CENTER) != 0; }

    // Setters mirror TS behaviour — turning on a centre flag clears the
    // matching opposite-edge flags (MID clears TOP+BOT, CENTER clears
    // LEFT+RIGHT).
    void setAlignTop(bool v);
    void setAlignBottom(bool v);
    void setAlignLeft(bool v);
    void setAlignRight(bool v);
    void setAlignVerticalCenter(bool v);
    void setAlignHorizontalCenter(bool v);

    // Read-only stretch helpers — true when opposite edges are both
    // aligned, which causes contentSize to stretch on that axis.
    bool isStretchWidth()  const { return (_alignFlags & LEFT) && (_alignFlags & RIGHT); }
    bool isStretchHeight() const { return (_alignFlags & TOP)  && (_alignFlags & BOT); }

    // ── Margins ───────────────────────────────────────────────────────
    // Values are pixels by default. Flip the matching `isAbsoluteXxx`
    // to `false` to interpret them as a ratio (0..1) of the parent's
    // contentSize on that axis.
    float getTop()              const { return _top;     }
    float getBottom()           const { return _bottom;  }
    float getLeft()             const { return _left;    }
    float getRight()            const { return _right;   }
    float getHorizontalCenter() const { return _hCenter; }
    float getVerticalCenter()   const { return _vCenter; }

    void setTop(float v)              { _top     = v; scheduleLayout(); }
    void setBottom(float v)           { _bottom  = v; scheduleLayout(); }
    void setLeft(float v)             { _left    = v; scheduleLayout(); }
    void setRight(float v)            { _right   = v; scheduleLayout(); }
    void setHorizontalCenter(float v) { _hCenter = v; scheduleLayout(); }
    void setVerticalCenter(float v)   { _vCenter = v; scheduleLayout(); }

    // Set all four pixel paddings (TOP+BOT+LEFT+RIGHT) to the same value.
    // Matches TS `padding` setter.
    void setPadding(float v);

    // ── Absolute (pixel) vs ratio (percentage) mode ───────────────────
    bool isAbsoluteTop()              const { return _absTop;     }
    bool isAbsoluteBottom()           const { return _absBottom;  }
    bool isAbsoluteLeft()             const { return _absLeft;    }
    bool isAbsoluteRight()            const { return _absRight;   }
    bool isAbsoluteHorizontalCenter() const { return _absHCenter; }
    bool isAbsoluteVerticalCenter()   const { return _absVCenter; }

    void setAbsoluteTop(bool v)              { _absTop = v;     scheduleLayout(); }
    void setAbsoluteBottom(bool v)           { _absBottom = v;  scheduleLayout(); }
    void setAbsoluteLeft(bool v)             { _absLeft = v;    scheduleLayout(); }
    void setAbsoluteRight(bool v)            { _absRight = v;   scheduleLayout(); }
    void setAbsoluteHorizontalCenter(bool v) { _absHCenter = v; scheduleLayout(); }
    void setAbsoluteVerticalCenter(bool v)   { _absVCenter = v; scheduleLayout(); }

    // ── AlignMode + updateAlignment ──────────────────────────────────
    AlignMode getAlignMode() const { return _alignMode; }
    void      setAlignMode(AlignMode m);

    // Force an immediate re-layout. Creator runs this deferred via the
    // WidgetManager; we do it synchronously — fine for MVP.
    void updateAlignment();

    // Deprecated names kept for compatibility with earlier demo code.
    void setAlign(uint32_t flags)       { setAlignFlags(flags); }
    uint32_t getAlign() const           { return _alignFlags; }
    void setTopMargin(float v)          { setTop(v); }
    void setBottomMargin(float v)       { setBottom(v); }
    void setLeftMargin(float v)         { setLeft(v); }
    void setRightMargin(float v)        { setRight(v); }
    float getTopMargin() const          { return _top; }
    float getBottomMargin() const       { return _bottom; }
    float getLeftMargin() const         { return _left; }
    float getRightMargin() const        { return _right; }
    void updateLayout()                 { updateAlignment(); }

private:
    void scheduleLayout();
    void bindTargetListener();
    void unbindTargetListener();
    Node *effectiveTarget() const;  // _target ?: parent

    // Serialised state.
    Node     *_target{nullptr};  // weak ref; null = use parent
    uint32_t  _alignFlags{0};
    AlignMode _alignMode{AlignMode::ON_WINDOW_RESIZE};  // TS default

    float _top{0.f}, _bottom{0.f}, _left{0.f}, _right{0.f};
    float _hCenter{0.f}, _vCenter{0.f};

    bool _absTop{true}, _absBottom{true}, _absLeft{true}, _absRight{true};
    bool _absHCenter{true}, _absVCenter{true};

    struct TargetHook;
    TargetHook *_hook{nullptr};

    // ONCE alignment applies exactly one layout on onEnable; _didAlignOnce
    // suppresses subsequent runs without having to unhook listeners.
    bool _didAlignOnce{false};
};

}  // namespace cc
