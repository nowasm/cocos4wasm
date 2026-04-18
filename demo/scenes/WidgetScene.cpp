#include "SceneRegistry.h"

#include "base/Log.h"
#include "cocos/2d/components/Sprite.h"
#include "cocos/2d/framework/Canvas.h"
#include "cocos/2d/framework/UITransform.h"
#include "cocos/ui/components/Button.h"
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
               cc::Button::ClickHandler onClick) {
    auto *node = ccnew cc::Node(name);
    node->setPosition(cc::Vec3(x, 0, 0));

    auto *ui = node->addComponent<cc::UITransform>();
    ui->setContentSize(180.0f, 90.0f);
    ui->setAnchorPoint(0.5f, 0.5f);

    auto *sprite = node->addComponent<cc::Sprite>();
    sprite->setSize(180.0f, 90.0f);
    sprite->setColor(tint);  // initial; Button::onEnable will overwrite

    auto *btn = node->addComponent<cc::Button>();
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

        int clickCount = 0;  // captured by the middle button's handler
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

        cc::NodeActivator::get().activateNode(_root, true);
        CC_LOG_INFO("[WidgetScene] armed: 2 interactable + 1 disabled");
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
