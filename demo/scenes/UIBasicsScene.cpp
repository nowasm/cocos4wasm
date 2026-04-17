#include "SceneRegistry.h"
#include "base/Log.h"
#include "cocos/2d/framework/Canvas.h"
#include "cocos/2d/framework/UITransform.h"
#include "core/component/NodeActivator.h"
#include "core/scene-graph/Node.h"

// ─── P2a — Canvas + UITransform smoke test ─────────────────────────────────
//
// Builds a minimal UI root programmatically: Canvas on the root node, one
// UITransform child. Activating the tree causes the Canvas to create its
// ortho camera and push it to the RenderScene. The engine's default 3D
// camera still renders (dark blue), but the Canvas camera's high priority
// + CLEAR_ALL means its light-blue clear color takes over the framebuffer.
//
// Verification:
//   • window background changes from dark blue → light blue on scene entry
//   • log shows "[Canvas] camera created" during activation
//   • scene exit restores the default 3D camera's dark-blue background

class UIBasicsScene : public DemoScene {
public:
    const char *name() const override { return "P2a — Canvas + UITransform"; }

    void onEnter(cc::scene::RenderScene * /*rs*/, cc::Root * /*root*/) override {
        _rootNode = ccnew cc::Node("ui-root");
        _rootNode->addRef();

        auto *canvas = _rootNode->addComponent<cc::Canvas>();
        // leave defaults — light-blue clear, max priority
        (void)canvas;

        // Child with a UITransform — nothing rendered yet (Sprite lands in P2c),
        // but logs confirm the component system + reflection are happy.
        auto *child = ccnew cc::Node("ui-child");
        auto *ut = child->addComponent<cc::UITransform>();
        ut->setContentSize(200.0f, 120.0f);
        ut->setAnchorPoint(0.5f, 0.5f);
        _rootNode->addChild(child);

        CC_LOG_INFO("[UIBasicsScene] tree built — activating");
        cc::NodeActivator::get().activateNode(_rootNode, true);
        CC_LOG_INFO("[UIBasicsScene] expect window to turn LIGHT BLUE");
    }

    void onExit() override {
        CC_LOG_INFO("[UIBasicsScene] deactivating — UI camera removed");
        if (_rootNode) {
            cc::NodeActivator::get().activateNode(_rootNode, false);
            _rootNode->release();
            _rootNode = nullptr;
        }
    }

private:
    cc::Node *_rootNode{nullptr};
};

REGISTER_DEMO_SCENE("UIBasicsScene", UIBasicsScene);
