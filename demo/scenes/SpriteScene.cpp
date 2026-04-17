#include "SceneRegistry.h"
#include "base/Log.h"
#include "cocos/2d/components/Sprite.h"
#include "cocos/2d/framework/Canvas.h"
#include "core/component/NodeActivator.h"
#include "core/scene-graph/Node.h"
#include "game/TextureLoader.h"

// ─── P2c — first textured quad (Sprite) ────────────────────────────────────
//
// Loads a PNG from disk via TextureLoader (editor/assets/default_ui/atom.png
// is staged next to the exe by the demo's POST_BUILD step), assigns it to
// a Sprite component, and shows it inside a Canvas.
//
// Verification:
//   • log: "[UIRenderer] model attached (verts=4, idx=6, stride=20B)"
//   • window: the atom icon rendered centred on a dark-blue background

class SpriteScene : public DemoScene {
public:
    const char *name() const override { return "P2c — textured sprite"; }

    void onEnter(cc::scene::RenderScene * /*rs*/, cc::Root * /*root*/) override {
        _root = ccnew cc::Node("sprite-root");
        _root->addRef();

        auto *canvas = _root->addComponent<cc::Canvas>();
        canvas->setClearColor(cc::Color{25, 30, 50, 255});

        // Load a test texture from the copied editor assets.
        const char *texPath = "default_ui/atom.png";
        auto *tex = cc::game::TextureLoader::loadFromFile(texPath);
        if (!tex) {
            CC_LOG_ERROR("[SpriteScene] texture load failed: %s", texPath);
            cc::NodeActivator::get().activateNode(_root, true);  // still activate Canvas
            return;
        }

        auto *spriteNode = ccnew cc::Node("sprite");
        auto *sprite = spriteNode->addComponent<cc::Sprite>();
        sprite->setTexture(tex);
        sprite->setSize(200.0f, 200.0f);
        _root->addChild(spriteNode);

        CC_LOG_INFO("[SpriteScene] texture '%s' bound, activating", texPath);
        cc::NodeActivator::get().activateNode(_root, true);
    }

    void onExit() override {
        CC_LOG_INFO("[SpriteScene] exit");
        if (_root) {
            cc::NodeActivator::get().activateNode(_root, false);
            _root->release();
            _root = nullptr;
        }
    }

private:
    cc::Node *_root{nullptr};
};

REGISTER_DEMO_SCENE("SpriteScene", SpriteScene);
