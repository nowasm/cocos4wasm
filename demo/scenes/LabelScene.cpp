#include "SceneRegistry.h"
#include "base/Log.h"
#include "cocos/2d/components/Label.h"
#include "cocos/2d/framework/Canvas.h"
#include "cocos/2d/text/BmfFont.h"
#include "core/component/NodeActivator.h"
#include "core/scene-graph/Node.h"

// ─── P2d — BitmapFont text rendering ───────────────────────────────────────
//
// Loads OpenSans-Regular.fnt + the matching atlas PNG via a lightweight
// AngelCode BMFont parser (cocos/2d/text/BmfFont). The Label component
// walks the string and emits one textured quad per glyph.
//
// Expected: "Hello, Cocos!" rendered in white-ish text, centred, on the
// Canvas's dark-blue clear colour.

class LabelScene : public DemoScene {
public:
    const char *name() const override { return "P2d — BitmapFont label"; }

    void onEnter(cc::scene::RenderScene * /*rs*/, cc::Root * /*root*/) override {
        _root = ccnew cc::Node("label-root");
        _root->addRef();

        auto *canvas = _root->addComponent<cc::Canvas>();
        canvas->setClearColor(cc::Color{25, 30, 50, 255});

        // Owns the BMFont resource for the scene's lifetime.
        _font = std::make_unique<cc::BmfFont>();
        const char *fntPath = "default_fonts/builtin-bitmap/OpenSans-Regular.fnt";
        if (!_font->load(fntPath)) {
            CC_LOG_ERROR("[LabelScene] font load failed");
            cc::NodeActivator::get().activateNode(_root, true);
            return;
        }

        auto *labelNode = ccnew cc::Node("label");
        auto *label = labelNode->addComponent<cc::Label>();
        label->setFont(_font.get());
        label->setText("Hello, Cocos!");
        label->setColor(cc::Color{255, 255, 255, 255});
        _root->addChild(labelNode);

        CC_LOG_INFO("[LabelScene] activating — expect 'Hello, Cocos!' on dark-blue");
        cc::NodeActivator::get().activateNode(_root, true);
    }

    void onExit() override {
        CC_LOG_INFO("[LabelScene] exit");
        if (_root) {
            cc::NodeActivator::get().activateNode(_root, false);
            _root->release();
            _root = nullptr;
        }
        _font.reset();
    }

private:
    cc::Node                    *_root{nullptr};
    std::unique_ptr<cc::BmfFont> _font;
};

REGISTER_DEMO_SCENE("LabelScene", LabelScene);
