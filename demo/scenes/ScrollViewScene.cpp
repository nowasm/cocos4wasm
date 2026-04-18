#include "SceneRegistry.h"

#include "base/Log.h"
#include "cocos/2d/components/Mask.h"
#include "cocos/2d/components/Sprite.h"
#include "cocos/2d/framework/Canvas.h"
#include "cocos/2d/framework/UITransform.h"
#include "cocos/ui/components/Layout.h"
#include "cocos/ui/components/ScrollView.h"
#include "core/component/NodeActivator.h"
#include "core/scene-graph/Node.h"

// ─── P5d — ScrollView drag demo ──────────────────────────────────────────
//
// A 400×240 viewport with a rectangular Mask that clips its children.
// Inside the viewport is a content node holding 20 coloured rows stacked
// by a Layout (VERTICAL). ScrollView on the viewport pans the content
// node in response to drag. Release with motion → momentum decay.
//
// Expected behaviour:
//   • Drag up: content scrolls up revealing later rows (Y-inverted).
//   • Drag further than bounds: clamp hard at edges (no rubber-band).
//   • Fast flick → content keeps moving briefly then decays to rest.

class ScrollViewScene : public DemoScene {
public:
    const char *name() const override { return "P5d — scroll view"; }

    void onEnter(cc::scene::RenderScene * /*rs*/, cc::Root * /*root*/) override {
        _root = ccnew cc::Node("scroll-root");
        _root->addRef();

        auto *canvas = _root->addComponent<cc::Canvas>();
        canvas->setClearColor(cc::Color(30, 32, 45, 255));

        // Canvas UITransform matches the render viewport so Widget +
        // ScrollView's window-size estimate both agree.
        auto *rootUI = _root->addComponent<cc::UITransform>();
        rootUI->setContentSize(1280.f, 720.f);
        rootUI->setAnchorPoint(0.5f, 0.5f);

        // ── Viewport ─────────────────────────────────────────────────────
        auto *view = ccnew cc::Node("viewport");
        {
            auto *ui = view->addComponent<cc::UITransform>();
            ui->setContentSize(400.f, 240.f);
            ui->setAnchorPoint(0.5f, 0.5f);
            view->setPosition(cc::Vec3(0.f, 0.f, 0.f));

            // Background so the masked viewport is visible even when
            // content doesn't cover it.
            auto *bg = view->addComponent<cc::Sprite>();
            bg->setSize(400.f, 240.f);
            bg->setColor(cc::Color(20, 22, 30, 255));

            // Rect mask — children outside this rect are stencil-clipped.
            auto *mask = view->addComponent<cc::Mask>();
            mask->setType(cc::Mask::Type::RECT);
            mask->setSize(400.f, 240.f);

            _root->addChild(view);
        }

        // ── Content (child of viewport, bigger than viewport on Y) ───────
        auto *content = ccnew cc::Node("scroll-content");
        {
            auto *ui = content->addComponent<cc::UITransform>();
            ui->setContentSize(380.f, 1200.f);  // tall enough that scrolling matters
            ui->setAnchorPoint(0.5f, 1.0f);     // top-centred — simpler scroll math
            content->setPosition(cc::Vec3(0.f, 120.f, 0.f));  // start at view top

            auto *lay = content->addComponent<cc::Layout>();
            lay->setType(cc::Layout::Type::VERTICAL);
            lay->setSpacing(0.f, 6.f);
            lay->setPadding(10.f, 10.f, 10.f, 10.f);

            view->addChild(content);
        }

        // 20 row tiles — hue ramps so scrolling is visually obvious.
        for (int i = 0; i < 20; ++i) {
            auto *row = ccnew cc::Node("row");
            auto *ui = row->addComponent<cc::UITransform>();
            ui->setContentSize(360.f, 50.f);
            ui->setAnchorPoint(0.5f, 0.5f);
            auto *sp = row->addComponent<cc::Sprite>();
            sp->setSize(360.f, 50.f);
            const uint8_t r = static_cast<uint8_t>(40 + (i * 10));
            const uint8_t g = static_cast<uint8_t>(220 - (i * 8));
            const uint8_t b = static_cast<uint8_t>(80 + ((i * 13) % 160));
            sp->setColor(cc::Color(r, g, b, 255));
            content->addChild(row);
        }

        // ── ScrollView on the viewport, pointing at content ─────────────
        auto *sv = view->addComponent<cc::ScrollView>();
        sv->setContent(content);
        sv->setHorizontalEnabled(false);  // pure vertical scroll for this demo
        sv->setVerticalEnabled(true);

        cc::NodeActivator::get().activateNode(_root, true);
        CC_LOG_INFO("[ScrollViewScene] viewport 400x240, content 380x1200, 20 rows");
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

REGISTER_DEMO_SCENE("ScrollViewScene", ScrollViewScene);
