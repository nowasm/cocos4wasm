#pragma once

#include "base/Ptr.h"
#include "base/std/container/vector.h"
#include "cocos/asset/SpriteFrame.h"
#include "core/component/Component.h"
#include "core/reflection/Reflection.h"
#include "math/Vec2.h"

namespace cc {

class Node;
class PageView;
class Sprite;

// Mirrors cocos/ui/page-view-indicator.ts. Renders a row of dots whose
// count matches the attached PageView's page count; the dot matching the
// current page is highlighted.
//
// The shared sprite frame is applied to every indicator dot. When null
// the dots render as plain colored Sprites without a texture.
class PageViewIndicator : public Component {
    CC_CLASS_DECL(PageViewIndicator, Component)
public:
    enum class Direction : uint32_t {
        HORIZONTAL = 0,
        VERTICAL   = 1,
    };

    PageViewIndicator() = default;
    ~PageViewIndicator() override = default;

    void onLoad() override;

    SpriteFrame *getSpriteFrame() const { return _spriteFrame.get(); }
    void         setSpriteFrame(SpriteFrame *sf) { _spriteFrame = sf; _refresh(); }

    Direction getDirection() const { return _direction; }
    void      setDirection(Direction d) { _direction = d; _refresh(); }

    const Vec2 &getCellSize() const { return _cellSize; }
    void        setCellSize(const Vec2 &v) { _cellSize = v; _refresh(); }
    void        setCellSize(float w, float h) { _cellSize.set(w, h); _refresh(); }

    float getSpacing() const { return _spacing; }
    void  setSpacing(float v) { _spacing = v; _refresh(); }

    // Attach to the PageView whose page count + current index drive
    // the dot layout and highlight.
    void setPageView(PageView *pv);

private:
    void _refresh();
    Sprite *_createIndicatorNode(int index);

    IntrusivePtr<SpriteFrame> _spriteFrame;
    Direction _direction{Direction::HORIZONTAL};
    Vec2      _cellSize{20.f, 20.f};
    float     _spacing{0.f};

    PageView *_pageView{nullptr};

    // Retained indicator sprites. Recreated each _refresh — simple and
    // rare-enough that the churn doesn't matter.
    ccstd::vector<Sprite *> _dots;
};

}  // namespace cc
