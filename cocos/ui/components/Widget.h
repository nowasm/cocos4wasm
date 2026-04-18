#pragma once

#include "core/component/Component.h"
#include "core/reflection/Reflection.h"

namespace cc {

class UITransform;

// Anchor/margin-based positioning relative to a parent UITransform.
//
// Usage pattern (Cocos Creator-style):
//   auto *w = node->addComponent<Widget>();
//   w->setAlign(Widget::TOP | Widget::LEFT);
//   w->setTopMargin(20);
//   w->setLeftMargin(20);
//
// Align flags are a bitmask. Combining two opposite edges (TOP+BOTTOM or
// LEFT+RIGHT) stretches the node's UITransform contentSize to fill the
// axis between the given margins. Combining the centres with their edges
// is allowed but only the edge flags drive position — the centre flags
// are nops in that case.
//
// The widget re-runs whenever:
//   • onEnable fires (initial layout)
//   • the parent's UITransform emits SizeChanged
//   • setAlign / setMargin / setAlignMode is called at runtime
//
// MVP scope:
//   • pixel margins (no percentage / ratio)
//   • no rotation/scale aware — assumes orthogonal parent
//   • no target-node override — always uses the direct parent
class Widget : public Component {
    CC_CLASS_DECL(Widget, Component)
public:
    // Bit flags; combine with |.
    enum AlignFlag : uint32_t {
        NONE              = 0,
        TOP               = 1u << 0,
        BOTTOM            = 1u << 1,
        LEFT              = 1u << 2,
        RIGHT             = 1u << 3,
        HORIZONTAL_CENTER = 1u << 4,
        VERTICAL_CENTER   = 1u << 5,
    };

    enum class AlignMode : uint32_t {
        ONCE   = 0,  // apply on enable, ignore later parent resizes
        ALWAYS = 1,  // re-apply every parent SizeChanged
    };

    Widget() = default;
    ~Widget() override = default;

    void onEnable() override;
    void onDisable() override;

    uint32_t getAlign() const { return _align; }
    void setAlign(uint32_t flags);

    AlignMode getAlignMode() const { return _alignMode; }
    void setAlignMode(AlignMode m);

    float getTopMargin()    const { return _top;    }
    float getBottomMargin() const { return _bottom; }
    float getLeftMargin()   const { return _left;   }
    float getRightMargin()  const { return _right;  }
    float getHorizontalCenter() const { return _hCenter; }
    float getVerticalCenter()   const { return _vCenter; }

    void setTopMargin(float v)    { _top    = v; scheduleLayout(); }
    void setBottomMargin(float v) { _bottom = v; scheduleLayout(); }
    void setLeftMargin(float v)   { _left   = v; scheduleLayout(); }
    void setRightMargin(float v)  { _right  = v; scheduleLayout(); }
    void setHorizontalCenter(float v) { _hCenter = v; scheduleLayout(); }
    void setVerticalCenter(float v)   { _vCenter = v; scheduleLayout(); }

    // Force an immediate re-layout. Normally you shouldn't need to call
    // this — setters + parent-resize hook cover the common case.
    void updateLayout();

private:
    void scheduleLayout();
    void bindParentListener();
    void unbindParentListener();

    uint32_t  _align{0};
    AlignMode _alignMode{AlignMode::ALWAYS};
    float _top{0.f}, _bottom{0.f}, _left{0.f}, _right{0.f};
    float _hCenter{0.f}, _vCenter{0.f};

    struct ParentHook;
    ParentHook *_hook{nullptr};
};

}  // namespace cc
