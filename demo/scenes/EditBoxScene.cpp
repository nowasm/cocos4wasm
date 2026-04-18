#include "SceneRegistry.h"

#include <memory>

#include "base/Log.h"
#include "cocos/2d/components/Sprite.h"
#include "cocos/2d/framework/Canvas.h"
#include "cocos/2d/framework/UITransform.h"
#include "cocos/2d/text/BmfFont.h"
#include "cocos/ui/components/EditBox.h"
#include "core/component/NodeActivator.h"
#include "core/scene-graph/Node.h"

// ─── P5e — EditBox single-line input ─────────────────────────────────────
//
// A text input field with a placeholder, a live caret, and an above-field
// label that mirrors the text so typed input is observable.
//
// Focus model:
//   • Click inside the input to focus (caret appears, OS text input opens).
//   • Click elsewhere or hit Enter / Escape to blur.
//
// Input:
//   • SDL_TEXTINPUT → TextInput bus → EditBox appends the UTF-8 chunk.
//   • Backspace deletes the last codepoint (UTF-8 aware).

class EditBoxScene : public DemoScene {
public:
    const char *name() const override { return "P5e — edit box"; }

    void onEnter(cc::scene::RenderScene * /*rs*/, cc::Root * /*root*/) override {
        _root = ccnew cc::Node("editbox-root");
        _root->addRef();

        auto *canvas = _root->addComponent<cc::Canvas>();
        canvas->setClearColor(cc::Color(35, 38, 55, 255));

        auto *rootUI = _root->addComponent<cc::UITransform>();
        rootUI->setContentSize(1280.f, 720.f);
        rootUI->setAnchorPoint(0.5f, 0.5f);

        _font = std::make_unique<cc::BmfFont>();
        if (!_font->load("default_fonts/builtin-bitmap/OpenSans-Regular.fnt")) {
            CC_LOG_ERROR("[EditBoxScene] font load failed — EditBox will render blank");
        }

        // Background rectangle + EditBox component.
        auto *box = ccnew cc::Node("editbox");
        {
            auto *ui = box->addComponent<cc::UITransform>();
            ui->setContentSize(400.f, 60.f);
            ui->setAnchorPoint(0.5f, 0.5f);
            auto *bg = box->addComponent<cc::Sprite>();
            bg->setSize(400.f, 60.f);
            bg->setColor(cc::Color(60, 62, 80, 255));

            auto *eb = box->addComponent<cc::EditBox>();
            eb->setFont(_font.get());
            eb->setPlaceholder("type here...");
            eb->setOnTextChanged([](cc::EditBox *, const ccstd::string &t) {
                CC_LOG_INFO("[EditBox] text='%s'", t.c_str());
            });
            _root->addChild(box);
        }

        cc::NodeActivator::get().activateNode(_root, true);
        CC_LOG_INFO("[EditBoxScene] click in the rectangle to start typing");
    }

    void onExit() override {
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

REGISTER_DEMO_SCENE("EditBoxScene", EditBoxScene);
