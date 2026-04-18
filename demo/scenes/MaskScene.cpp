#include "SceneRegistry.h"
#include "base/Log.h"
#include "cocos/2d/components/Mask.h"
#include "cocos/2d/components/Sprite.h"
#include "cocos/2d/framework/Canvas.h"
#include "core/component/NodeActivator.h"
#include "core/scene-graph/Node.h"
#include "game/TextureLoader.h"

// ─── P2f-a — single-level RECT mask ────────────────────────────────────────
//
// A 300×300 Sprite lives under a 200×200 rectangular Mask. Expected result:
// only the centre 200×200 region of the atom texture is visible; everything
// outside the mask rect is clipped by stencil.
//
// Tree:
//   root
//   └─ maskNode (Mask, 200×200 rect)
//      └─ spriteNode (Sprite, 300×300, loads atom.png)

class MaskScene : public DemoScene {
public:
    const char *name() const override { return "P2f-a — RECT mask"; }

    void onEnter(cc::scene::RenderScene * /*rs*/, cc::Root * /*root*/) override {
        _root = ccnew cc::Node("mask-root");
        _root->addRef();

        auto *canvas = _root->addComponent<cc::Canvas>();
        canvas->setClearColor(cc::Color{25, 30, 50, 255});

        // Mask: 200×200 rect at origin.
        auto *maskNode = ccnew cc::Node("mask");
        auto *mask = maskNode->addComponent<cc::Mask>();
        mask->setType(cc::Mask::Type::RECT);
        mask->setSize(200.0f, 200.0f);
        _root->addChild(maskNode);

        // Sprite larger than the mask — we expect the outer regions clipped.
        auto *tex = cc::game::TextureLoader::loadFromFile("default_ui/atom.png");
        if (!tex) {
            CC_LOG_ERROR("[MaskScene] atom.png not found");
            cc::NodeActivator::get().activateNode(_root, true);
            return;
        }

        auto *spriteNode = ccnew cc::Node("clipped-sprite");
        auto *sprite = spriteNode->addComponent<cc::Sprite>();
        sprite->setTexture(tex);
        sprite->setSize(300.0f, 300.0f);
        maskNode->addChild(spriteNode);

        CC_LOG_INFO("[MaskScene] activating — expect atom clipped to a 200x200 square");
        cc::NodeActivator::get().activateNode(_root, true);
    }

    void onExit() override {
        CC_LOG_INFO("[MaskScene] exit");
        if (_root) {
            cc::NodeActivator::get().activateNode(_root, false);
            _root->release();
            _root = nullptr;
        }
    }

private:
    cc::Node *_root{nullptr};
};

REGISTER_DEMO_SCENE("MaskScene", MaskScene);
