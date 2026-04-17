#include "SceneRegistry.h"
#include "base/Log.h"
#include "cocos/2d/framework/Canvas.h"
#include "cocos/2d/framework/UIRenderer.h"
#include "cocos/2d/framework/UITransform.h"
#include "core/component/NodeActivator.h"
#include "core/scene-graph/Node.h"
#include "game/MaterialFactory.h"

// ─── ColoredQuad — a first-pixel UIRenderer subclass ───────────────────────
//
// Emits a 200×200 world-space quad centred on its node, coloured by the
// `color` property. Vertex format is position-only (3 floats); color comes
// from the builtin-unlit material's `mainColor` uniform via MaterialFactory.
//
// This proves the path-A render flow end-to-end:
//   UIRenderer.onEnable → resolveMaterial → updateGeometry → create gfx
//   buffers → attach scene::Model → Canvas camera renders it.

class ColoredQuad : public cc::UIRenderer {
    CC_CLASS_DECL(ColoredQuad, cc::UIRenderer)
public:
    ColoredQuad() = default;
    ~ColoredQuad() override = default;

    void setColor(const cc::Color &c) {
        _color = c;
        markDirty();
    }
    void setHalfSize(float hx, float hy) {
        _halfX = hx;
        _halfY = hy;
        markDirty();
    }

protected:
    void updateGeometry() override {
        _vertexStrideFloats = 3;
        // 4 vertices × 3 floats = 12 floats
        _vertexData = {
            -_halfX, -_halfY, 0.0f,   // bottom-left
             _halfX, -_halfY, 0.0f,   // bottom-right
             _halfX,  _halfY, 0.0f,   // top-right
            -_halfX,  _halfY, 0.0f,   // top-left
        };
        _indexData = {0, 1, 2, 0, 2, 3};
        _vertexCount = 4;
        _indexCount  = 6;
    }

    cc::IntrusivePtr<cc::Material> resolveMaterial() override {
        cc::IntrusivePtr<cc::Material> m = cc::game::MaterialFactory::createUnlit(_color);
        return m;
    }

private:
    cc::Color _color{255, 64, 64, 255};  // coral red
    float     _halfX{100.0f};
    float     _halfY{100.0f};
};

CC_IMPLEMENT_CLASS(ColoredQuad, "demo.ColoredQuad", cc::UIRenderer)
    .property("color", &ColoredQuad::_color, cc::Color{255, 64, 64, 255})
CC_END_CLASS(ColoredQuad);

// ─── Scene ─────────────────────────────────────────────────────────────────

class ColoredQuadScene : public DemoScene {
public:
    const char *name() const override { return "P2b — first colored quad"; }

    void onEnter(cc::scene::RenderScene * /*rs*/, cc::Root * /*root*/) override {
        _root = ccnew cc::Node("cq-root");
        _root->addRef();

        // Canvas: UI camera clears dark-blue so the quad reads clearly.
        auto *canvas = _root->addComponent<cc::Canvas>();
        canvas->setClearColor(cc::Color{25, 30, 50, 255});

        // Quad: child node carries the drawable.
        auto *quadNode = ccnew cc::Node("quad");
        auto *quad = quadNode->addComponent<ColoredQuad>();
        quad->setColor(cc::Color{255, 100, 60, 255});
        quad->setHalfSize(150.0f, 90.0f);
        _root->addChild(quadNode);

        CC_LOG_INFO("[ColoredQuadScene] activating — expect a coral quad on dark-blue");
        cc::NodeActivator::get().activateNode(_root, true);
    }

    void onExit() override {
        CC_LOG_INFO("[ColoredQuadScene] exit");
        if (_root) {
            cc::NodeActivator::get().activateNode(_root, false);
            _root->release();
            _root = nullptr;
        }
    }

private:
    cc::Node *_root{nullptr};
};

REGISTER_DEMO_SCENE("ColoredQuadScene", ColoredQuadScene);
