#include "cocos/ui/components/PageViewIndicator.h"

#include "cocos/2d/components/Sprite.h"
#include "cocos/2d/framework/UITransform.h"
#include "cocos/ui/components/PageView.h"
#include "core/scene-graph/Node.h"
#include "math/Vec3.h"

namespace cc {

// Upstream page-view-indicator.ts: `public spacing` (no underscore);
// `_direction`, `_cellSize`, `_spriteFrame` are protected fields.
// `_pageView` is populated by the owning PageView at onEnable, not
// serialized.
CC_IMPLEMENT_CLASS(PageViewIndicator, "cc.PageViewIndicator", Component)
    .property("_spriteFrame", &PageViewIndicator::_spriteFrame)
    .property("_direction",   &PageViewIndicator::_direction, Direction::HORIZONTAL)
    .property("_cellSize",    &PageViewIndicator::_cellSize,  Vec2{20.f, 20.f})
    .property("spacing",      &PageViewIndicator::_spacing,   0.f)
CC_END_CLASS(PageViewIndicator);

void PageViewIndicator::onLoad() { _refresh(); }

void PageViewIndicator::setPageView(PageView *pv) {
    _pageView = pv;
    _refresh();
}

Sprite *PageViewIndicator::_createIndicatorNode(int index) {
    auto *node = ccnew Node("PageDot");
    auto *ui = node->addComponent<UITransform>();
    ui->setContentSize(_cellSize);
    ui->setAnchorPoint(0.5f, 0.5f);
    auto *sp = node->addComponent<Sprite>();
    sp->setSize(_cellSize.x, _cellSize.y);
    if (_spriteFrame) sp->setSpriteFrame(_spriteFrame.get());
    (void)index;
    return sp;
}

void PageViewIndicator::_refresh() {
    auto *node = getNode();
    if (!node) return;

    // Clear existing dots.
    for (auto *s : _dots) {
        if (s) s->getNode()->setParent(nullptr);
    }
    _dots.clear();

    if (!_pageView) return;

    const auto pages = _pageView->getPages();
    const int count = static_cast<int>(pages.size());
    if (count == 0) return;

    const float step = (_direction == Direction::HORIZONTAL
                         ? _cellSize.x : _cellSize.y) + _spacing;
    const float half = (count - 1) * 0.5f;

    for (int i = 0; i < count; ++i) {
        auto *dot = _createIndicatorNode(i);
        node->addChild(dot->getNode());
        Vec3 p{0.f, 0.f, 0.f};
        if (_direction == Direction::HORIZONTAL) {
            p.x = (i - half) * step;
        } else {
            p.y = (half - i) * step;
        }
        dot->getNode()->setPosition(p);

        // Highlight the current page: current = full opacity white;
        // others = dimmed.
        const bool current = (i == _pageView->getCurrentPageIndex());
        dot->setColor(current ? Color{255, 255, 255, 255}
                               : Color{255, 255, 255, 120});
        _dots.push_back(dot);
    }
}

}  // namespace cc
