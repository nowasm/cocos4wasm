#include "SceneRegistry.h"
#include "base/Log.h"
#include "cocos/2d/components/Mask.h"
#include "cocos/2d/components/Sprite.h"
#include "cocos/2d/framework/Canvas.h"
#include "core/component/NodeActivator.h"
#include "core/scene-graph/Node.h"
#include "game/TextureLoader.h"
#include "math/Vec3.h"

// ─── P2f-b / P2f-c — RECT + ELLIPSE + GRAPHICS + nested mask ────────────────
//
// Four quadrants, each showing a 300×300 atom Sprite clipped by a
// differently-shaped mask.
//
//   Top-left:     RECT mask 200×200
//   Top-right:    ELLIPSE mask 200×200 (32 segments)
//   Bottom-left:  GRAPHICS mask — hand-built triangle path
//   Bottom-right: NESTED — outer 220×220 RECT with an inner 160×160
//                 ELLIPSE; only the intersection is visible
//
// The nested case exercises P2f-c's bit-per-level stencil logic: outer
// mask writes stencil bit 0, inner mask writes bit 1 (both in the same
// frame). Content under both tests `stencil == 0b11` — only pixels where
// BOTH masks drew pass.

namespace {

cc::Node *buildMaskedAtomCluster(const char *name,
                                 const cc::Vec3 &clusterPos,
                                 cc::Mask::Type maskType,
                                 float maskW, float maskH,
                                 cc::Texture2D *tex) {
    auto *cluster = ccnew cc::Node(name);
    cluster->setPosition(clusterPos);

    auto *maskNode = ccnew cc::Node("mask");
    auto *mask = maskNode->addComponent<cc::Mask>();
    mask->setType(maskType);
    mask->setSize(maskW, maskH);
    if (maskType == cc::Mask::Type::GRAPHICS) {
        // A clean isoceles triangle pointing up.
        mask->graphicsMoveTo(0.0f,  100.0f);
        mask->graphicsLineTo(-110.0f, -90.0f);
        mask->graphicsLineTo( 110.0f, -90.0f);
    }
    cluster->addChild(maskNode);

    auto *spriteNode = ccnew cc::Node("sprite");
    auto *sp = spriteNode->addComponent<cc::Sprite>();
    sp->setTexture(tex);
    sp->setSize(300.0f, 300.0f);
    maskNode->addChild(spriteNode);
    return cluster;
}

cc::Node *buildNestedCluster(const cc::Vec3 &clusterPos, cc::Texture2D *tex) {
    auto *cluster = ccnew cc::Node("nested");
    cluster->setPosition(clusterPos);

    // Deliberate size mismatch so the nesting is visually obvious: outer
    // rect is short and wide (letterbox), inner ellipse is larger so it
    // pokes out top and bottom — the intersection becomes a horizontal
    // "band / lens" shape that only exists because BOTH shapes clip.
    auto *outerMaskNode = ccnew cc::Node("outer-rect-mask");
    auto *outerMask = outerMaskNode->addComponent<cc::Mask>();
    outerMask->setType(cc::Mask::Type::RECT);
    outerMask->setSize(220.0f, 120.0f);
    cluster->addChild(outerMaskNode);

    auto *innerMaskNode = ccnew cc::Node("inner-ellipse-mask");
    auto *innerMask = innerMaskNode->addComponent<cc::Mask>();
    innerMask->setType(cc::Mask::Type::ELLIPSE);
    innerMask->setSize(200.0f, 200.0f);
    outerMaskNode->addChild(innerMaskNode);

    auto *spriteNode = ccnew cc::Node("sprite");
    auto *sp = spriteNode->addComponent<cc::Sprite>();
    sp->setTexture(tex);
    sp->setSize(300.0f, 300.0f);
    innerMaskNode->addChild(spriteNode);
    return cluster;
}

}  // namespace

class MaskScene : public DemoScene {
public:
    const char *name() const override { return "P2f — mask shapes + nesting"; }

    void onEnter(cc::scene::RenderScene * /*rs*/, cc::Root * /*root*/) override {
        _root = ccnew cc::Node("mask-root");
        _root->addRef();

        auto *canvas = _root->addComponent<cc::Canvas>();
        canvas->setClearColor(cc::Color{25, 30, 50, 255});

        auto *tex = cc::game::TextureLoader::loadFromFile("default_ui/atom.png");
        if (!tex) {
            CC_LOG_ERROR("[MaskScene] atom.png not found");
            cc::NodeActivator::get().activateNode(_root, true);
            return;
        }

        _root->addChild(buildMaskedAtomCluster("rect",     cc::Vec3{-260.0f,  130.0f, 0.0f},
                                               cc::Mask::Type::RECT,     200.0f, 200.0f, tex));
        _root->addChild(buildMaskedAtomCluster("ellipse",  cc::Vec3{ 260.0f,  130.0f, 0.0f},
                                               cc::Mask::Type::ELLIPSE,  200.0f, 200.0f, tex));
        _root->addChild(buildMaskedAtomCluster("graphics", cc::Vec3{-260.0f, -130.0f, 0.0f},
                                               cc::Mask::Type::GRAPHICS, 0.0f, 0.0f, tex));
        _root->addChild(buildNestedCluster(                cc::Vec3{ 260.0f, -130.0f, 0.0f}, tex));

        CC_LOG_INFO("[MaskScene] 4 clusters: rect | ellipse | triangle | nested(rect intersect ellipse)");
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
