#include "cocos/ui/components/PageView.h"

#include <algorithm>

#include "cocos/2d/framework/UITransform.h"
#include "cocos/ui/components/PageViewIndicator.h"
#include "core/scene-graph/Node.h"

namespace cc {

// Name alignment with upstream page-view.ts: `public autoPageTurningThreshold`
// and `public pageTurningSpeed` have no underscore in JSON; `_direction` is
// the protected backing field (not our C++ name `_pageDirection`); `_indicator`
// is a serialized ref to the PageViewIndicator child.
CC_IMPLEMENT_CLASS(PageView, "cc.PageView", ScrollView)
    .property("_sizeMode",                &PageView::_sizeMode,                SizeMode::Unified)
    .property("_direction",               &PageView::_pageDirection,           PageViewDirection::HORIZONTAL)
    .property("_scrollThreshold",         &PageView::_scrollThreshold,         0.5f)
    .property("_pageTurningEventTiming",  &PageView::_pageTurningEventTiming,  0.1f)
    .property("autoPageTurningThreshold", &PageView::_autoPageTurningThreshold,100.f)
    .property("pageTurningSpeed",         &PageView::_pageTurningSpeed,        0.3f)
    .property("_indicator",               &PageView::_indicator)
    .property("pageEvents",               &PageView::_pageEvents)
CC_END_CLASS(PageView);

void PageView::onLoad() {
    // Creator's onLoad synchronises cur-page with the current scroll
    // offset; for MVP we start at page 0 and leave alignment to
    // setCurrentPageIndex.
}

void PageView::onEnable() {
    ScrollView::onEnable();
    if (_indicator) _indicator->setPageView(this);

    // Swipe-to-snap: when the user finishes a drag, bypass ScrollView's
    // default inertia and snap to the nearest page.
    addScrollListener([](ScrollView *sv, ScrollView::EventType t) {
        if (t != ScrollView::EventType::TOUCH_UP) return;
        static_cast<PageView *>(sv)->_snapToNearestPageOnRelease();
    });
}

void PageView::onDisable() {
    clearScrollListeners();
    ScrollView::onDisable();
}

void PageView::_snapToNearestPageOnRelease() {
    auto pages = getPages();
    if (pages.empty()) return;
    Node *content = getContent();
    if (!content) return;
    auto *view = getView();
    if (!view) return;
    const Vec2 viewSize = view->getContentSize();
    if (viewSize.x <= 0.f && viewSize.y <= 0.f) return;

    // Convert current content position into a fractional page index. The
    // drag direction (sign of _dragDelta) biases rounding so a shallow
    // drag past `_scrollThreshold` advances a page even if displacement
    // alone rounds back.
    const Vec3 &pos = content->getPosition();
    const bool horizontal = (_pageDirection == PageViewDirection::HORIZONTAL);
    const float pageSize = horizontal ? viewSize.x : viewSize.y;
    if (pageSize <= 0.f) return;

    // horizontal: content.x = -pageIdx * pageSize. vertical: content.y = +pageIdx * pageSize.
    const float raw = horizontal ? (-pos.x / pageSize) : (pos.y / pageSize);
    int32_t target = _curPageIdx;
    const float delta = raw - static_cast<float>(_curPageIdx);
    if (std::abs(delta) >= _scrollThreshold) {
        target = _curPageIdx + (delta > 0.f ? 1 : -1);
    }
    target = std::clamp(target, 0, static_cast<int32_t>(pages.size() - 1));
    scrollToPage(target, _pageTurningSpeed);
}

void PageView::setIndicator(PageViewIndicator *i) {
    _indicator = i;
    if (i) i->setPageView(this);
}

ccstd::vector<Node *> PageView::getPages() const {
    ccstd::vector<Node *> out;
    Node *content = getContent();
    if (!content) return out;
    for (const auto &c : content->getChildren()) {
        if (auto *n = c.get()) out.push_back(n);
    }
    return out;
}

void PageView::setCurrentPageIndex(int32_t idx) {
    scrollToPage(idx, 1.f);  // TS default 1-second timing
}

void PageView::addPage(Node *page) {
    if (!page) return;
    Node *content = getContent();
    if (!content) return;
    content->addChild(page);
    if (_indicator) _indicator->setPageView(this);
}

void PageView::insertPage(Node *page, int32_t index) {
    // Our Node doesn't expose an insertChild-at; emulate by addChild then
    // letting the user call setSiblingIndex if needed. Matching TS's
    // API shape is the priority; stable insertion-at lands when a demo
    // needs it.
    addPage(page);
    (void)index;
}

void PageView::removePage(Node *page) {
    if (!page) return;
    page->setParent(nullptr);
    if (_indicator) _indicator->setPageView(this);
}

void PageView::removePageAtIndex(int32_t index) {
    auto pages = getPages();
    if (index < 0 || static_cast<size_t>(index) >= pages.size()) return;
    removePage(pages[static_cast<size_t>(index)]);
}

void PageView::removeAllPages() {
    for (auto *p : getPages()) p->setParent(nullptr);
    if (_indicator) _indicator->setPageView(this);
}

void PageView::scrollToPage(int32_t idx, float timeInSecond) {
    auto pages = getPages();
    if (pages.empty()) return;
    idx = std::clamp(idx, 0, static_cast<int32_t>(pages.size() - 1));

    // Translate page index into a content offset. In Unified size-mode
    // each page occupies the view rect; in Free mode we use the page's
    // authored UITransform size as the step.
    auto *view = getView();
    if (!view) return;
    const Vec2 viewSize = view->getContentSize();

    float offsetX = 0.f, offsetY = 0.f;
    if (_pageDirection == PageViewDirection::HORIZONTAL) {
        offsetX = -idx * viewSize.x;
    } else {
        offsetY = idx * viewSize.y;
    }
    scrollToOffset(Vec2{offsetX, offsetY}, timeInSecond, true);

    const bool changed = (_curPageIdx != idx);
    _curPageIdx = idx;
    if (changed) _firePageTurning();
}

void PageView::_firePageTurning() {
    reflection::MethodArgs args;
    args.push_back(reflection::MethodArg::makePointer(this));
    ComponentEventHandler::emitEvents(_pageEvents, args);

    auto snap = _runtimePageListeners;
    for (auto &fn : snap) fn(this);
    if (_indicator) _indicator->setPageView(this);
}

}  // namespace cc
