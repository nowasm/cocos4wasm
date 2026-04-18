#include "cocos/ui/components/Layout.h"

#include "base/Log.h"
#include "cocos/2d/framework/UITransform.h"
#include "core/scene-graph/Node.h"
#include "math/Vec3.h"

namespace cc {

CC_IMPLEMENT_CLASS(Layout, "cc.Layout", Component)
    .property("type",       &Layout::_type,       static_cast<Type>(0))
    .property("hDir",       &Layout::_hDir,       static_cast<HorizontalDirection>(0))
    .property("vDir",       &Layout::_vDir,       static_cast<VerticalDirection>(0))
    .property("padTop",     &Layout::_padTop)
    .property("padBottom",  &Layout::_padBottom)
    .property("padLeft",    &Layout::_padLeft)
    .property("padRight",   &Layout::_padRight)
    .property("spacingX",   &Layout::_spacingX)
    .property("spacingY",   &Layout::_spacingY)
    .property("cellSize",   &Layout::_cellSize,   Vec2{0.f, 0.f})
CC_END_CLASS(Layout);

// Keep the per-subscription ids out of the header so consumers don't pull
// in the Node event-template machinery. Heap-alloced on first onEnable.
struct Layout::Hooks {
    cc::event::TargetEventID<Node::ChildAdded>          addedId;
    cc::event::TargetEventID<Node::ChildRemoved>        removedId;
    cc::event::TargetEventID<Node::SiblingOrderChanged> orderId;
    cc::event::TargetEventID<Node::SizeChanged>         sizeId;
};

void Layout::onEnable() {
    bindHooks();
    updateLayout();
}

void Layout::onDisable() {
    unbindHooks();
}

void Layout::scheduleLayout() {
    if (getNode() && getNode()->isActiveInHierarchy()) {
        updateLayout();
    }
}

void Layout::bindHooks() {
    auto *node = getNode();
    if (!node || _hooks) return;
    _hooks = new Hooks();

    _hooks->addedId = node->on<Node::ChildAdded>(
        [this](Node *, Node *) { scheduleLayout(); });
    _hooks->removedId = node->on<Node::ChildRemoved>(
        [this](Node *, Node *) { scheduleLayout(); });
    _hooks->orderId = node->on<Node::SiblingOrderChanged>(
        [this](Node *) { scheduleLayout(); });
    _hooks->sizeId = node->on<Node::SizeChanged>(
        [this](Node *) { scheduleLayout(); });
}

void Layout::unbindHooks() {
    auto *node = getNode();
    if (!_hooks) return;
    if (node) {
        node->off<Node::ChildAdded>         (_hooks->addedId);
        node->off<Node::ChildRemoved>       (_hooks->removedId);
        node->off<Node::SiblingOrderChanged>(_hooks->orderId);
        node->off<Node::SizeChanged>        (_hooks->sizeId);
    }
    delete _hooks;
    _hooks = nullptr;
}

void Layout::updateLayout() {
    if (_type == Type::NONE) return;
    auto *node = getNode();
    if (!node) return;
    auto *ui = node->getComponent<UITransform>();
    if (!ui) return;

    switch (_type) {
        case Type::HORIZONTAL: layoutHorizontal(ui); break;
        case Type::VERTICAL:   layoutVertical(ui);   break;
        case Type::GRID:       layoutGrid(ui);       break;
        default: break;
    }
}

namespace {

// Helpers that compute a child's half-extent on each axis so we can lay
// the edge of the child against the running cursor instead of its origin.
float halfW(Node *c) {
    auto *ui = c->getComponent<UITransform>();
    return ui ? ui->getContentSize().x * 0.5f : 0.f;
}
float halfH(Node *c) {
    auto *ui = c->getComponent<UITransform>();
    return ui ? ui->getContentSize().y * 0.5f : 0.f;
}

// Children iterate as a vector<IntrusivePtr<Node>>. We want a plain
// vector<Node *> in the requested direction so the layout loop stays
// readable and directional flips are a one-liner.
ccstd::vector<Node *> gatherActiveChildren(Node *owner) {
    ccstd::vector<Node *> out;
    for (const auto &c : owner->getChildren()) {
        Node *n = c.get();
        if (n && n->isActiveInHierarchy()) out.push_back(n);
    }
    return out;
}

}  // namespace

void Layout::layoutHorizontal(UITransform *ui) {
    auto *node = getNode();
    auto children = gatherActiveChildren(node);
    if (children.empty()) return;
    if (_hDir == HorizontalDirection::RIGHT_TO_LEFT) {
        std::reverse(children.begin(), children.end());
    }

    // Place the first child's left edge at the parent's left-padding
    // boundary, then advance by child-width + spacing. Y axis comes from
    // the vertical-direction setting — TOP_TO_BOTTOM anchors to the top
    // inside-padding line, BOTTOM_TO_TOP to the bottom.
    const Vec2 &pSize = ui->getContentSize();
    const Vec2 &pAnc  = ui->getAnchorPoint();
    const float pLeft   = -pAnc.x * pSize.x;
    const float pRight  = (1.f - pAnc.x) * pSize.x;
    const float pBottom = -pAnc.y * pSize.y;
    const float pTop    = (1.f - pAnc.y) * pSize.y;

    float cursorX = pLeft + _padLeft;
    float yLine   = (_vDir == VerticalDirection::TOP_TO_BOTTOM)
                        ? pTop - _padTop
                        : pBottom + _padBottom;

    for (auto *c : children) {
        const float hw = halfW(c);
        const float hh = halfH(c);
        auto *cui = c->getComponent<UITransform>();
        const Vec2 cAnc = cui ? cui->getAnchorPoint() : Vec2{0.5f, 0.5f};

        Vec3 pos = c->getPosition();
        pos.x = cursorX + cAnc.x * (hw * 2.f);
        // Align the child's top (or bottom) edge against yLine.
        pos.y = (_vDir == VerticalDirection::TOP_TO_BOTTOM)
                    ? yLine - (1.f - cAnc.y) * (hh * 2.f)
                    : yLine + cAnc.y * (hh * 2.f);
        c->setPosition(pos);

        cursorX += hw * 2.f + _spacingX;
        if (cursorX > pRight - _padRight && false) {
            // Overflow is silent for HORIZONTAL mode — GRID wraps; rows/cols
            // don't. Keeping the compare here as a marker in case we later
            // add an onOverflow warning.
        }
    }
}

void Layout::layoutVertical(UITransform *ui) {
    auto *node = getNode();
    auto children = gatherActiveChildren(node);
    if (children.empty()) return;
    if (_vDir == VerticalDirection::BOTTOM_TO_TOP) {
        std::reverse(children.begin(), children.end());
    }

    const Vec2 &pSize = ui->getContentSize();
    const Vec2 &pAnc  = ui->getAnchorPoint();
    const float pLeft   = -pAnc.x * pSize.x;
    const float pRight  = (1.f - pAnc.x) * pSize.x;
    const float pBottom = -pAnc.y * pSize.y;
    const float pTop    = (1.f - pAnc.y) * pSize.y;

    float cursorY = pTop - _padTop;
    float xLine   = (_hDir == HorizontalDirection::LEFT_TO_RIGHT)
                        ? pLeft + _padLeft
                        : pRight - _padRight;

    for (auto *c : children) {
        const float hw = halfW(c);
        const float hh = halfH(c);
        auto *cui = c->getComponent<UITransform>();
        const Vec2 cAnc = cui ? cui->getAnchorPoint() : Vec2{0.5f, 0.5f};

        Vec3 pos = c->getPosition();
        pos.y = cursorY - (1.f - cAnc.y) * (hh * 2.f);
        pos.x = (_hDir == HorizontalDirection::LEFT_TO_RIGHT)
                    ? xLine + cAnc.x * (hw * 2.f)
                    : xLine - (1.f - cAnc.x) * (hw * 2.f);
        c->setPosition(pos);

        cursorY -= hh * 2.f + _spacingY;
        (void)pBottom;  // unused in HORIZONTAL-of-VERTICAL mode
    }
}

void Layout::layoutGrid(UITransform *ui) {
    auto *node = getNode();
    auto children = gatherActiveChildren(node);
    if (children.empty()) return;

    const Vec2 &pSize = ui->getContentSize();
    const Vec2 &pAnc  = ui->getAnchorPoint();
    const float pLeft   = -pAnc.x * pSize.x;
    const float pTop    = (1.f - pAnc.y) * pSize.y;

    // Cell size: explicit takes precedence. Falling back to the first
    // child's size keeps it forgiving for quick demos where every cell
    // is uniform and sized via its own UITransform.
    float cellW = _cellSize.x;
    float cellH = _cellSize.y;
    if (cellW <= 0.f || cellH <= 0.f) {
        if (auto *cui = children[0]->getComponent<UITransform>()) {
            if (cellW <= 0.f) cellW = cui->getContentSize().x;
            if (cellH <= 0.f) cellH = cui->getContentSize().y;
        }
    }
    if (cellW <= 0.f || cellH <= 0.f) return;

    const float usableW = pSize.x - _padLeft - _padRight;
    int perRow = static_cast<int>((usableW + _spacingX) / (cellW + _spacingX));
    if (perRow < 1) perRow = 1;

    const float startX = pLeft + _padLeft;
    const float startY = pTop  - _padTop;

    for (size_t i = 0; i < children.size(); ++i) {
        auto *c = children[i];
        auto *cui = c->getComponent<UITransform>();
        const Vec2 cAnc = cui ? cui->getAnchorPoint() : Vec2{0.5f, 0.5f};

        const int col = (_hDir == HorizontalDirection::LEFT_TO_RIGHT)
                            ? static_cast<int>(i) % perRow
                            : perRow - 1 - (static_cast<int>(i) % perRow);
        const int row = static_cast<int>(i) / perRow;

        const float cellLeft = startX + col * (cellW + _spacingX);
        const float cellTop  = startY - row * (cellH + _spacingY);

        // Anchor-aware placement: position = cell-top-left + anchor·cellSize.
        Vec3 pos = c->getPosition();
        pos.x = cellLeft + cAnc.x * cellW;
        pos.y = cellTop  - (1.f - cAnc.y) * cellH;
        c->setPosition(pos);
    }
}

}  // namespace cc
