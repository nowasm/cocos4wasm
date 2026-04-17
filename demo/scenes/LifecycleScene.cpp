#include "SceneRegistry.h"
#include "base/Log.h"
#include "core/component/Component.h"
#include "core/component/NodeActivator.h"
#include "core/scene-graph/Node.h"

// ─── Auto-logging component ────────────────────────────────────────────────
//
// Each lifecycle hook emits to the engine log. Scripted scene transitions
// below (LifecycleScene::onUpdate) then exercise activate/deactivate/reparent/
// destroy so the log reveals the real ordering.

class LifecycleLogger : public cc::Component {
    CC_CLASS_DECL(LifecycleLogger, cc::Component)
public:
    LifecycleLogger() {
        _wantsUpdate = true;
        _wantsLateUpdate = true;
    }
    ~LifecycleLogger() override = default;

    ccstd::string tag;
    int updateCount{0};
    int lateUpdateCount{0};

    void onLoad() override {
        CC_LOG_INFO("[LC:%s] onLoad", tag.c_str());
    }
    void onEnable() override {
        CC_LOG_INFO("[LC:%s] onEnable", tag.c_str());
    }
    void start() override {
        CC_LOG_INFO("[LC:%s] start", tag.c_str());
    }
    void update(float /*dt*/) override {
        if (updateCount < 2) {
            CC_LOG_INFO("[LC:%s] update #%d", tag.c_str(), updateCount);
        }
        ++updateCount;
    }
    void lateUpdate(float /*dt*/) override {
        if (lateUpdateCount < 2) {
            CC_LOG_INFO("[LC:%s] lateUpdate #%d", tag.c_str(), lateUpdateCount);
        }
        ++lateUpdateCount;
    }
    void onDisable() override {
        CC_LOG_INFO("[LC:%s] onDisable (updates=%d, lateUpdates=%d)",
                    tag.c_str(), updateCount, lateUpdateCount);
    }
    void onDestroy() override {
        CC_LOG_INFO("[LC:%s] onDestroy", tag.c_str());
    }
};

CC_IMPLEMENT_CLASS(LifecycleLogger, "demo.LifecycleLogger", cc::Component)
    .property("tag", &LifecycleLogger::tag)
CC_END_CLASS(LifecycleLogger);

// ─── Scene ─────────────────────────────────────────────────────────────────

class LifecycleScene : public DemoScene {
public:
    const char *name() const override { return "P1b — Component lifecycle"; }

    void onEnter(cc::scene::RenderScene * /*rs*/, cc::Root * /*root*/) override {
        _rootNode = ccnew cc::Node("lc-root");
        _childNode = ccnew cc::Node("lc-child");
        _grandchildNode = ccnew cc::Node("lc-grandchild");
        _rootNode->addRef();
        _childNode->addRef();
        _grandchildNode->addRef();

        // Attach loggers before activation so onLoad/onEnable fire in a
        // predictable top-down order when the tree is activated.
        auto *r  = _rootNode->addComponent<LifecycleLogger>();        r->tag  = "root";
        auto *c  = _childNode->addComponent<LifecycleLogger>();       c->tag  = "child";
        auto *gc = _grandchildNode->addComponent<LifecycleLogger>();  gc->tag = "grandchild";

        _rootNode->addChild(_childNode);
        _childNode->addChild(_grandchildNode);

        CC_LOG_INFO("========== LifecycleScene: activating tree ==========");
        CC_LOG_INFO("expected order: onLoad root→child→grandchild, then onEnable same order");
        cc::NodeActivator::get().activateNode(_rootNode, true);

        CC_LOG_INFO("---------- next tick will fire start() for all three ----------");
        _elapsed = 0;
        _frameCount = 0;
        _phase = 0;
    }

    void onUpdate(float dt) override {
        _elapsed += dt;
        // Each phase is triggered after ~N additional frames so the scripted
        // lifecycle transitions complete in seconds rather than waiting on
        // wall-clock thresholds (dt accumulation is engine-tick-dependent).
        ++_frameCount;
        const int stride = 60;  // ~1 second at 60fps
        if (_phase == 0 && _frameCount >= stride) {
            CC_LOG_INFO("========== disabling child subtree (child+grandchild onDisable) ==========");
            _childNode->setActive(false);
            _phase = 1;
        } else if (_phase == 1 && _frameCount >= stride * 2) {
            CC_LOG_INFO("========== re-enabling child subtree (child+grandchild onEnable) ==========");
            _childNode->setActive(true);
            _phase = 2;
        } else if (_phase == 2 && _frameCount >= stride * 3) {
            CC_LOG_INFO("========== deactivating entire tree (leaf-first onDisable) ==========");
            cc::NodeActivator::get().activateNode(_rootNode, false);
            _phase = 3;
        } else if (_phase == 3 && _frameCount >= stride * 4) {
            CC_LOG_INFO("========== re-activating tree (top-down onEnable) ==========");
            cc::NodeActivator::get().activateNode(_rootNode, true);
            _phase = 4;
        } else if (_phase == 4 && _frameCount >= stride * 5) {
            CC_LOG_INFO("========== (idle — press arrows to switch scenes, ESC to quit) ==========");
            _phase = 5;
        }
    }

    void onExit() override {
        CC_LOG_INFO("========== LifecycleScene exit — destroying nodes ==========");
        if (_rootNode) _rootNode->release();
        if (_childNode) _childNode->release();
        if (_grandchildNode) _grandchildNode->release();
        _rootNode = nullptr;
        _childNode = nullptr;
        _grandchildNode = nullptr;
    }

private:
    cc::Node *_rootNode{nullptr};
    cc::Node *_childNode{nullptr};
    cc::Node *_grandchildNode{nullptr};
    float _elapsed{0};
    int   _frameCount{0};
    int   _phase{0};
};

REGISTER_DEMO_SCENE("LifecycleScene", LifecycleScene);
