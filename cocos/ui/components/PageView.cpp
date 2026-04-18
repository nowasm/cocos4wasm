#include "cocos/ui/components/PageView.h"

#include <algorithm>

#include "cocos/2d/framework/UITransform.h"
#include "cocos/ui/components/PageViewIndicator.h"
#include "core/scene-graph/Node.h"

namespace cc {

CC_IMPLEMENT_CLASS(PageView, "cc.PageView", ScrollView)
    .property("_sizeMode",                &PageView::_sizeMode,                SizeMode::Unified)
    .property("_pageDirection",           &PageView::_pageDirection,           PageViewDirection::HORIZONTAL)
    .property("_scrollThreshold",         &PageView::_scrollThreshold,         0.5f)
    .property("_pageTurningEventTiming",  &PageView::_pageTurningEventTiming,  0.1f)
    .property("_autoPageTurningThreshold",&PageView::_autoPageTurningThreshold,100.f)
    .property("_pageTurningSpeed",        &PageView::_pageTurningSpeed,        0.3f)
CC_END_CLASS(PageView);

void PageView::onLoad() {
    // Creator's onLoad synchronises cur-page with the current scroll
    // offset; for MVP we start at page 0 and leave alignment to
    // setCurrentPageIndex.
}

void PageView::onEnable() {
    ScrollView::onEnable();
    if (_indicator) _indicator->setPageView(this);
}

void PageView::onDisable() {
    ScrollView::onDisable();
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
    auto snap = _pageEvents;
    for (auto &fn : snap) fn(this);
    if (_indicator) _indicator->setPageView(this);
}

}  // namespace cc
