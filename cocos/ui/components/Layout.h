#pragma once

#include "core/component/Component.h"
#include "core/reflection/Reflection.h"
#include "math/Vec2.h"

namespace cc {

class Node;
class UITransform;

// Auto-arrange children of the owning node.
//
// Modes:
//   HORIZONTAL — line children up along the X axis
//   VERTICAL   — stack children along the Y axis
//   GRID       — wrap into rows/columns based on container width + cellSize
//
// Layout runs on:
//   • onEnable (once)
//   • own Node::ChildAdded / ChildRemoved / SiblingIndexChanged events
//   • own Node::SizeChanged (only GRID uses it — other modes are size-
//     independent if resizeMode=NONE, which is the P5c default)
//   • setter calls at runtime (scheduleLayout)
//
// Direction flags control scan order along each axis. Primary direction is
// the flow axis; for GRID the secondary direction is the wrap axis.
//
// P5c scope: ResizeMode is fixed at NONE — children keep their own sizes
// and the layout doesn't resize itself to fit. "Fit container" and "fit
// children" resize modes land later when we have a concrete demand.
class Layout : public Component {
    CC_CLASS_DECL(Layout, Component)
public:
    enum class Type : uint32_t {
        NONE       = 0,
        HORIZONTAL = 1,
        VERTICAL   = 2,
        GRID       = 3,
    };

    enum class HorizontalDirection : uint32_t {
        LEFT_TO_RIGHT = 0,
        RIGHT_TO_LEFT = 1,
    };

    enum class VerticalDirection : uint32_t {
        TOP_TO_BOTTOM = 0,
        BOTTOM_TO_TOP = 1,
    };

    Layout() = default;
    ~Layout() override = default;

    void onEnable() override;
    void onDisable() override;

    Type getType() const { return _type; }
    void setType(Type t) { _type = t; scheduleLayout(); }

    HorizontalDirection getHorizontalDirection() const { return _hDir; }
    void setHorizontalDirection(HorizontalDirection d) { _hDir = d; scheduleLayout(); }

    VerticalDirection getVerticalDirection() const { return _vDir; }
    void setVerticalDirection(VerticalDirection d) { _vDir = d; scheduleLayout(); }

    float getPaddingTop()    const { return _padTop;    }
    float getPaddingBottom() const { return _padBottom; }
    float getPaddingLeft()   const { return _padLeft;   }
    float getPaddingRight()  const { return _padRight;  }
    float getSpacingX()      const { return _spacingX;  }
    float getSpacingY()      const { return _spacingY;  }

    void setPadding(float t, float b, float l, float r) {
        _padTop = t; _padBottom = b; _padLeft = l; _padRight = r;
        scheduleLayout();
    }
    void setSpacing(float x, float y) { _spacingX = x; _spacingY = y; scheduleLayout(); }

    // GRID uses this to decide where to wrap. If (0, 0), GRID falls back
    // to the container's contentSize and lays out as many cells per row
    // as fit before wrapping.
    const Vec2 &getCellSize() const { return _cellSize; }
    void setCellSize(const Vec2 &v) { _cellSize = v; scheduleLayout(); }
    void setCellSize(float w, float h) { _cellSize.set(w, h); scheduleLayout(); }

    // Force an immediate re-layout. Normally automatic via hooks; exposed
    // for tests + programmatic tweaks that batch many setters.
    void updateLayout();

private:
    void scheduleLayout();
    void bindHooks();
    void unbindHooks();

    void layoutHorizontal(UITransform *ui);
    void layoutVertical(UITransform *ui);
    void layoutGrid(UITransform *ui);

    Type _type{Type::NONE};
    HorizontalDirection _hDir{HorizontalDirection::LEFT_TO_RIGHT};
    VerticalDirection   _vDir{VerticalDirection::TOP_TO_BOTTOM};

    float _padTop{0.f}, _padBottom{0.f}, _padLeft{0.f}, _padRight{0.f};
    float _spacingX{0.f}, _spacingY{0.f};
    Vec2 _cellSize{0.f, 0.f};

    struct Hooks;
    Hooks *_hooks{nullptr};
};

}  // namespace cc
