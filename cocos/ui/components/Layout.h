#pragma once

#include "core/component/Component.h"
#include "core/reflection/Reflection.h"
#include "math/Vec2.h"

namespace cc {

class Node;
class UITransform;

// Mirrors cocos/ui/layout.ts. Enum values + property names match TS so
// behaviour is predictable for anyone cross-referencing the Creator
// docs.
//
// Layout runs deferred: any mutation schedules an update that commits on
// the next tick. This prevents O(n²) recomputes when a batch of
// children/siblings change in one frame.
class Layout : public Component {
    CC_CLASS_DECL(Layout, Component)
public:
    // cc.Layout.Type — numeric values match TS.
    enum class Type : uint32_t {
        NONE       = 0,
        HORIZONTAL = 1,
        VERTICAL   = 2,
        GRID       = 3,
    };

    // cc.Layout.ResizeMode.
    enum class ResizeMode : uint32_t {
        NONE      = 0,  // neither container nor children resize
        CONTAINER = 1,  // container expands/shrinks to fit children bounds
        CHILDREN  = 2,  // children resize to fill the container (spacing-
                        // aware split; not yet implemented)
    };

    // cc.Layout.AxisDirection — wrap axis for GRID.
    enum class AxisDirection : uint32_t {
        HORIZONTAL = 0,
        VERTICAL   = 1,
    };

    // cc.Layout.VerticalDirection — note: 0 is bottom-to-top, matching TS.
    enum class VerticalDirection : uint32_t {
        BOTTOM_TO_TOP = 0,
        TOP_TO_BOTTOM = 1,
    };

    // cc.Layout.HorizontalDirection.
    enum class HorizontalDirection : uint32_t {
        LEFT_TO_RIGHT = 0,
        RIGHT_TO_LEFT = 1,
    };

    // cc.Layout.Constraint — GRID-specific row/col count lock.
    enum class Constraint : uint32_t {
        NONE      = 0,
        FIXED_ROW = 1,
        FIXED_COL = 2,
    };

    Layout() = default;
    ~Layout() override = default;

    void onEnable() override;
    void onDisable() override;

    // ── Type + resize mode ──────────────────────────────────────────────
    Type getType() const { return _type; }
    void setType(Type t) { _type = t; scheduleLayout(); }

    ResizeMode getResizeMode() const { return _resizeMode; }
    void       setResizeMode(ResizeMode m) { _resizeMode = m; scheduleLayout(); }

    // ── Padding ─────────────────────────────────────────────────────────
    float getPaddingTop()    const { return _padTop;    }
    float getPaddingBottom() const { return _padBottom; }
    float getPaddingLeft()   const { return _padLeft;   }
    float getPaddingRight()  const { return _padRight;  }
    void  setPaddingTop(float v)    { _padTop    = v; scheduleLayout(); }
    void  setPaddingBottom(float v) { _padBottom = v; scheduleLayout(); }
    void  setPaddingLeft(float v)   { _padLeft   = v; scheduleLayout(); }
    void  setPaddingRight(float v)  { _padRight  = v; scheduleLayout(); }
    // TS `padding` uniform setter — assigns all four.
    void  setPadding(float v);
    // Four-argument overload kept for convenience (our own demos use it).
    void  setPadding(float t, float b, float l, float r) {
        _padTop = t; _padBottom = b; _padLeft = l; _padRight = r;
        scheduleLayout();
    }

    // ── Spacing ─────────────────────────────────────────────────────────
    float getSpacingX() const { return _spacingX; }
    float getSpacingY() const { return _spacingY; }
    void  setSpacingX(float v) { _spacingX = v; scheduleLayout(); }
    void  setSpacingY(float v) { _spacingY = v; scheduleLayout(); }
    // Legacy combined setter kept for our own demo; tuples map cleanly
    // to Creator's inspector.
    void  setSpacing(float x, float y) { _spacingX = x; _spacingY = y; scheduleLayout(); }

    // ── Directions + axis + constraint ──────────────────────────────────
    HorizontalDirection getHorizontalDirection() const { return _hDir; }
    void setHorizontalDirection(HorizontalDirection d) { _hDir = d; scheduleLayout(); }

    VerticalDirection getVerticalDirection() const { return _vDir; }
    void setVerticalDirection(VerticalDirection d) { _vDir = d; scheduleLayout(); }

    AxisDirection getStartAxis() const { return _startAxis; }
    void          setStartAxis(AxisDirection a) { _startAxis = a; scheduleLayout(); }

    Constraint getConstraint() const { return _constraint; }
    void       setConstraint(Constraint c) { _constraint = c; scheduleLayout(); }

    int32_t getConstraintNum() const { return _constraintNum; }
    void    setConstraintNum(int32_t n) { _constraintNum = n; scheduleLayout(); }

    // ── Cell size (GRID) ────────────────────────────────────────────────
    const Vec2 &getCellSize() const { return _cellSize; }
    void setCellSize(const Vec2 &v) { _cellSize = v; scheduleLayout(); }
    void setCellSize(float w, float h) { _cellSize.set(w, h); scheduleLayout(); }

    // ── affectedByScale ─────────────────────────────────────────────────
    // When true, child scales count against the cell advance — matches
    // Creator's flag. Rarely flipped but kept for parity.
    bool isAffectedByScale() const { return _affectedByScale; }
    void setAffectedByScale(bool v) { _affectedByScale = v; scheduleLayout(); }

    // ── updateLayout ────────────────────────────────────────────────────
    // TS signature: updateLayout(force?: boolean): void.
    // `force=true` bypasses the Type::NONE early-out so tests can probe
    // the layout math without wiring a Type.
    void updateLayout(bool force = false);

private:
    void scheduleLayout();
    void bindHooks();
    void unbindHooks();

    void layoutHorizontal(UITransform *ui);
    void layoutVertical(UITransform *ui);
    void layoutGrid(UITransform *ui);

    Type                _type{Type::NONE};
    ResizeMode          _resizeMode{ResizeMode::NONE};
    HorizontalDirection _hDir{HorizontalDirection::LEFT_TO_RIGHT};
    VerticalDirection   _vDir{VerticalDirection::TOP_TO_BOTTOM};
    AxisDirection       _startAxis{AxisDirection::HORIZONTAL};
    Constraint          _constraint{Constraint::NONE};
    int32_t             _constraintNum{2};

    float _padTop{0.f}, _padBottom{0.f}, _padLeft{0.f}, _padRight{0.f};
    float _spacingX{0.f}, _spacingY{0.f};
    Vec2  _cellSize{40.f, 40.f};
    bool  _affectedByScale{false};

    struct Hooks;
    Hooks *_hooks{nullptr};
};

}  // namespace cc
