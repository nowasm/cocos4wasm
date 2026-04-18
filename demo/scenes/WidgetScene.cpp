#include "SceneRegistry.h"

#include "base/Log.h"
#include "cocos/2d/components/Sprite.h"
#include "cocos/2d/framework/Canvas.h"
#include "cocos/2d/framework/UITransform.h"
#include "cocos/ui/components/Button.h"
#include "cocos/ui/components/Layout.h"
#include "cocos/ui/components/Widget.h"
#include "core/component/NodeActivator.h"
#include "core/scene-graph/Node.h"

// ─── P5 — UI widgets showcase ─────────────────────────────────────────────
//
// Grows as the P5 sub-phases land. Today (P5a) it shows three Buttons:
//   • a regular interactable button
//   • a "momentary" button whose label logs per-click
//   • a disabled button (tinted, unclickable)
//
// Visual states verify: Normal → Hover tint brightens on mouse-over,
// Pressed darkens on click, releasing on the button fires onClick, moving
// off mid-press cancels the click.

namespace {
void addButton(cc::Node *parent, const char *name, float x,
               const cc::Color &tint, bool interactable,
               cc::Button::Handler onClick) {
    auto *node = ccnew cc::Node(name);
    node->setPosition(cc::Vec3(x, 0, 0));

    auto *ui = node->addComponent<cc::UITransform>();
    ui->setContentSize(180.0f, 90.0f);
    ui->setAnchorPoint(0.5f, 0.5f);

    auto *sprite = node->addComponent<cc::Sprite>();
    sprite->setSize(180.0f, 90.0f);
    sprite->setColor(tint);  // initial; Button::onEnable will overwrite

    auto *btn = node->addComponent<cc::Button>();
    btn->setTransition(cc::Button::Transition::COLOR);
    btn->setInteractable(interactable);
    btn->setNormalColor  (tint);
    btn->setHoverColor   (cc::Color(
        static_cast<uint8_t>(std::min(255, tint.r + 40)),
        static_cast<uint8_t>(std::min(255, tint.g + 40)),
        static_cast<uint8_t>(std::min(255, tint.b + 40)),
        tint.a));
    btn->setPressedColor (cc::Color(
        static_cast<uint8_t>(std::max(0, tint.r - 60)),
        static_cast<uint8_t>(std::max(0, tint.g - 60)),
        static_cast<uint8_t>(std::max(0, tint.b - 60)),
        tint.a));
    btn->setDisabledColor(cc::Color(100, 100, 100, 180));

    if (onClick) {
        btn->addClickListener(std::move(onClick));
    }

    parent->addChild(node);
}
}  // namespace

class WidgetScene : public DemoScene {
public:
    const char *name() const override { return "P5 — UI widgets"; }

    void onEnter(cc::scene::RenderScene * /*rs*/, cc::Root * /*root*/) override {
        _root = ccnew cc::Node("widget-root");
        _root->addRef();

        auto *canvas = _root->addComponent<cc::Canvas>();
        canvas->setClearColor(cc::Color(35, 40, 55, 255));

        // Give the canvas a UITransform matching the viewport so Widgets
        // can anchor against its edges. 1280×720 matches the demo window;
        // real apps hook this to resize events.
        auto *rootUI = _root->addComponent<cc::UITransform>();
        rootUI->setContentSize(1280.0f, 720.0f);
        rootUI->setAnchorPoint(0.5f, 0.5f);

        addButton(_root, "btn-fire",     -260.0f, cc::Color(90, 160, 240, 255), true,
            [](cc::Button *b) {
                CC_LOG_INFO("[P5a] 'btn-fire' clicked");
            });
        addButton(_root, "btn-counter",     0.0f, cc::Color(80, 210, 120, 255), true,
            [count = 0](cc::Button *b) mutable {
                ++count;
                CC_LOG_INFO("[P5a] 'btn-counter' clicked (total=%d)", count);
            });
        addButton(_root, "btn-disabled",  260.0f, cc::Color(230, 80, 80, 255), false, {});

        // ── P5b widget demo: four corner badges anchored to the canvas
        //     rectangle + one stretched banner across the top. Corners
        //     verify TOP/BOTTOM × LEFT/RIGHT; banner verifies the
        //     LEFT+RIGHT stretch codepath.
        auto addBadge = [this](const char *name, uint32_t align,
                               float top, float bottom, float left, float right,
                               const cc::Color &tint, float w = 80.f, float h = 80.f) {
            auto *n = ccnew cc::Node(name);
            auto *ui = n->addComponent<cc::UITransform>();
            ui->setContentSize(w, h);
            ui->setAnchorPoint(0.5f, 0.5f);
            auto *sp = n->addComponent<cc::Sprite>();
            sp->setSize(w, h);
            sp->setColor(tint);
            auto *w_ = n->addComponent<cc::Widget>();
            w_->setAlign(align);
            w_->setTopMargin(top);
            w_->setBottomMargin(bottom);
            w_->setLeftMargin(left);
            w_->setRightMargin(right);
            _root->addChild(n);
        };
        addBadge("tl", cc::Widget::TOP | cc::Widget::LEFT,   20, 0,  20, 0, cc::Color(230,180, 80, 255));
        addBadge("tr", cc::Widget::TOP | cc::Widget::RIGHT,  20, 0,  0, 20, cc::Color(230,180, 80, 255));
        addBadge("bl", cc::Widget::BOT | cc::Widget::LEFT, 0, 20, 20, 0, cc::Color(200,110,160, 255));
        addBadge("br", cc::Widget::BOT | cc::Widget::RIGHT, 0, 20, 0, 20, cc::Color(200,110,160, 255));
        // Stretched banner: fills horizontally between 40 and 40 px
        // margins, anchored to the top with a 20 px gap.
        addBadge("banner",
                 cc::Widget::TOP | cc::Widget::LEFT | cc::Widget::RIGHT,
                 20, 0, 40, 40, cc::Color(100, 180, 230, 255),
                 /*w=*/0, /*h=*/40);  // width will be overwritten by stretch

        // ── P5c layout demo: a horizontal row at the bottom-left + a
        //     3×N grid bottom-right. Each container is a sized UITransform
        //     with Widget-anchoring to its parent, and Layout handles the
        //     children placement — no manual positions needed per cell.
        auto addColorTile = [](cc::Node *parent, const cc::Color &c, float w, float h) {
            auto *n = ccnew cc::Node("tile");
            auto *ui = n->addComponent<cc::UITransform>();
            ui->setContentSize(w, h);
            ui->setAnchorPoint(0.5f, 0.5f);
            auto *sp = n->addComponent<cc::Sprite>();
            sp->setSize(w, h);
            sp->setColor(c);
            parent->addChild(n);
            return n;
        };

        // Horizontal row: 5 tiles at bottom-left, LEFT_TO_RIGHT flow.
        auto *row = ccnew cc::Node("h-row");
        {
            auto *ui = row->addComponent<cc::UITransform>();
            ui->setContentSize(440.0f, 60.0f);
            ui->setAnchorPoint(0.5f, 0.5f);
            auto *w_ = row->addComponent<cc::Widget>();
            w_->setAlign(cc::Widget::BOT | cc::Widget::LEFT);
            w_->setBottomMargin(120);
            w_->setLeftMargin(20);
            auto *lay = row->addComponent<cc::Layout>();
            lay->setType(cc::Layout::Type::HORIZONTAL);
            lay->setSpacing(8.f, 0.f);
            lay->setPadding(0, 0, 4, 4);
            _root->addChild(row);
        }
        for (int i = 0; i < 5; ++i) {
            const uint8_t v = static_cast<uint8_t>(80 + i * 30);
            addColorTile(row, cc::Color(v, 200, 255 - v, 255), 80.f, 52.f);
        }

        // Grid: 3×3 tiles at bottom-right.
        auto *grid = ccnew cc::Node("grid");
        {
            auto *ui = grid->addComponent<cc::UITransform>();
            ui->setContentSize(260.0f, 260.0f);
            ui->setAnchorPoint(0.5f, 0.5f);
            auto *w_ = grid->addComponent<cc::Widget>();
            w_->setAlign(cc::Widget::BOT | cc::Widget::RIGHT);
            w_->setBottomMargin(120);
            w_->setRightMargin(20);
            auto *lay = grid->addComponent<cc::Layout>();
            lay->setType(cc::Layout::Type::GRID);
            lay->setCellSize(80.f, 80.f);
            lay->setSpacing(4.f, 4.f);
            lay->setPadding(4, 4, 4, 4);
            _root->addChild(grid);
        }
        for (int i = 0; i < 9; ++i) {
            const uint8_t g = static_cast<uint8_t>(60 + (i * 20));
            addColorTile(grid, cc::Color(g, 90, g, 255), 80.f, 80.f);
        }

        cc::NodeActivator::get().activateNode(_root, true);
        CC_LOG_INFO("[WidgetScene] armed: 3 buttons + 4 badges + 1 banner + 5-row + 3x3 grid");
    }

    void onExit() override {
        if (_root) {
            cc::NodeActivator::get().activateNode(_root, false);
            _root->release();
            _root = nullptr;
        }
    }

private:
    cc::Node *_root{nullptr};
};

REGISTER_DEMO_SCENE("WidgetScene", WidgetScene);
