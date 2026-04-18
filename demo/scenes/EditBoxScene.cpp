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

        auto makeBox = [&](const char *name, float y, float h,
                           const char *placeholder,
                           cc::EditBox::InputMode mode,
                           cc::EditBox::InputFlag flag) -> cc::EditBox * {
            auto *box = ccnew cc::Node(name);
            box->setPosition(cc::Vec3(0.f, y, 0.f));
            auto *ui = box->addComponent<cc::UITransform>();
            ui->setContentSize(400.f, h);
            ui->setAnchorPoint(0.5f, 0.5f);

            // EditBox auto-adds a background Sprite; set its tint after.
            auto *eb = box->addComponent<cc::EditBox>();
            eb->setFont(_font.get());
            eb->setPlaceholder(placeholder);
            eb->setInputMode(mode);
            eb->setInputFlag(flag);
            eb->setMaxLength(200);
            eb->addTextChangedHandler([name](cc::EditBox *b) {
                CC_LOG_INFO("[EditBox:%s] text='%s'", name, b->getString().c_str());
            });
            eb->addEditingReturnHandler([name](cc::EditBox *b) {
                CC_LOG_INFO("[EditBox:%s] return pressed (text='%s')",
                            name, b->getString().c_str());
            });
            if (auto *bg = eb->getBackgroundSprite()) {
                bg->setSize(400.f, h);
                bg->setColor(cc::Color(60, 62, 80, 255));
            }
            _root->addChild(box);
            return eb;
        };

        makeBox("username", 120.f, 50.f, "username",
                cc::EditBox::InputMode::SINGLE_LINE, cc::EditBox::InputFlag::DEFAULT);
        makeBox("password",  60.f, 50.f, "password (hidden)",
                cc::EditBox::InputMode::SINGLE_LINE, cc::EditBox::InputFlag::PASSWORD);
        makeBox("notes",    -60.f, 120.f, "multi-line notes (Enter for newline)",
                cc::EditBox::InputMode::ANY, cc::EditBox::InputFlag::DEFAULT);

        cc::NodeActivator::get().activateNode(_root, true);
        CC_LOG_INFO("[EditBoxScene] click in a field to start typing. "
                    "Arrows/Home/End move caret, Shift+Arrows select, "
                    "Ctrl+C/X/V/A for clipboard, Enter/Esc to blur.");
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
