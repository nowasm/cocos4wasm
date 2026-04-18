#include "SceneRegistry.h"
#include "base/Log.h"
#include "cocos/2d/components/Graphics.h"
#include "cocos/2d/framework/Canvas.h"
#include "core/component/NodeActivator.h"
#include "core/scene-graph/Node.h"

// ─── P2e — Graphics drawing ────────────────────────────────────────────────
//
// Draws three shapes using the immediate-mode Graphics API:
//   - A stroked triangle outline on the left
//   - A filled rectangle in the middle
//   - A filled circle on the right
//
// Validates stroke (thick-line-as-quad), fill (triangle-fan), subpath
// management, and per-vertex colour flowing through USE_VERTEX_COLOR.

class GraphicsScene : public DemoScene {
public:
    const char *name() const override { return "P2e — Graphics shapes"; }

    void onEnter(cc::scene::RenderScene * /*rs*/, cc::Root * /*root*/) override {
        _root = ccnew cc::Node("gfx-root");
        _root->addRef();

        auto *canvas = _root->addComponent<cc::Canvas>();
        canvas->setClearColor(cc::Color{25, 30, 50, 255});

        auto *gfxNode = ccnew cc::Node("gfx");
        auto *g = gfxNode->addComponent<cc::Graphics>();

        // Stroked triangle on the left (-200 centre), white outline 3px.
        g->setStrokeColor(cc::Color{240, 240, 240, 255});
        g->setLineWidth(3.0f);
        g->moveTo(-260.0f, -80.0f);
        g->lineTo(-140.0f, -80.0f);
        g->lineTo(-200.0f,  60.0f);
        g->lineTo(-260.0f, -80.0f);
        g->stroke();

        // Filled rect in the middle, orange.
        g->setFillColor(cc::Color{230, 140, 60, 255});
        g->rect(-60.0f, -60.0f, 120.0f, 120.0f);
        g->fill();

        // Filled circle on the right, teal, 32 segments.
        g->setFillColor(cc::Color{60, 200, 200, 255});
        g->circle(200.0f, 0.0f, 70.0f, 32);
        g->fill();

        _root->addChild(gfxNode);

        CC_LOG_INFO("[GraphicsScene] activating — expect triangle outline, orange square, teal circle");
        cc::NodeActivator::get().activateNode(_root, true);
    }

    void onExit() override {
        CC_LOG_INFO("[GraphicsScene] exit");
        if (_root) {
            cc::NodeActivator::get().activateNode(_root, false);
            _root->release();
            _root = nullptr;
        }
    }

private:
    cc::Node *_root{nullptr};
};

REGISTER_DEMO_SCENE("GraphicsScene", GraphicsScene);
