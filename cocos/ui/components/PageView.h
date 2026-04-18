#pragma once

#include <functional>

#include "base/std/container/vector.h"
#include "cocos/ui/components/ScrollView.h"

namespace cc {

class PageViewIndicator;

// Mirrors cocos/ui/page-view.ts. Extends ScrollView; adds page-turning
// semantics: content scrolls in discrete page-sized increments,
// snapping to the nearest page on release. Each child of `content`
// counts as one page.
class PageView : public ScrollView {
    CC_CLASS_DECL(PageView, ScrollView)
public:
    // cc.PageView.SizeMode.
    enum class SizeMode : uint32_t {
        Unified = 0,  // every page same size (derived from view rect)
        Free    = 1,  // each page keeps its authored size
    };

    // cc.PageView.Direction.
    enum class PageViewDirection : uint32_t {
        HORIZONTAL = 0,
        VERTICAL   = 1,
    };

    // cc.PageView.EventType extras. PAGE_TURNING fires when
    // curPageIdx changes.
    struct PageViewEventType {
        static constexpr const char *PAGE_TURNING = "page-turning";
    };

    using PageHandler = std::function<void(PageView *)>;

    PageView() = default;
    ~PageView() override = default;

    void onLoad() override;
    void onEnable() override;
    void onDisable() override;

    SizeMode getSizeMode() const { return _sizeMode; }
    void     setSizeMode(SizeMode m) { _sizeMode = m; }

    PageViewDirection getPageDirection() const { return _pageDirection; }
    void              setPageDirection(PageViewDirection d) { _pageDirection = d; }

    float getScrollThreshold() const { return _scrollThreshold; }
    void  setScrollThreshold(float v) { _scrollThreshold = v; }

    float getPageTurningEventTiming() const { return _pageTurningEventTiming; }
    void  setPageTurningEventTiming(float v) { _pageTurningEventTiming = v; }

    PageViewIndicator *getIndicator() const { return _indicator; }
    void               setIndicator(PageViewIndicator *i);

    float getAutoPageTurningThreshold() const { return _autoPageTurningThreshold; }
    void  setAutoPageTurningThreshold(float v) { _autoPageTurningThreshold = v; }

    float getPageTurningSpeed() const { return _pageTurningSpeed; }
    void  setPageTurningSpeed(float v) { _pageTurningSpeed = v; }

    // Pages live as children of getContent(); these helpers keep the
    // index stable across mutations.
    ccstd::vector<Node *> getPages() const;
    int32_t               getCurrentPageIndex() const { return _curPageIdx; }
    void                  setCurrentPageIndex(int32_t idx);

    void addPage(Node *page);
    void insertPage(Node *page, int32_t index);
    void removePage(Node *page);
    void removePageAtIndex(int32_t index);
    void removeAllPages();

    void scrollToPage(int32_t idx, float timeInSecond = 0.3f);

    // pageEvents handler vector — fires on PAGE_TURNING.
    void addPageListener(PageHandler fn) { _pageEvents.push_back(std::move(fn)); }
    void clearPageListeners() { _pageEvents.clear(); }

private:
    void _firePageTurning();

    SizeMode          _sizeMode{SizeMode::Unified};
    PageViewDirection _pageDirection{PageViewDirection::HORIZONTAL};
    float             _scrollThreshold{0.5f};
    float             _pageTurningEventTiming{0.1f};
    float             _autoPageTurningThreshold{100.f};
    float             _pageTurningSpeed{0.3f};
    int32_t           _curPageIdx{0};

    PageViewIndicator *_indicator{nullptr};
    ccstd::vector<PageHandler> _pageEvents;
};

}  // namespace cc
