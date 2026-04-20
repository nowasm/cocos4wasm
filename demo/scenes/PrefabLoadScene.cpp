#include "SceneRegistry.h"
#include "base/Log.h"
#include "cocos/asset/AssetManager.h"
#include "cocos/asset/Prefab.h"
#include "cocos/asset/PrefabInfo.h"
#include "core/component/NodeActivator.h"
#include "core/scene-graph/Node.h"

#include <cstdio>  // fflush — the log sink is buffered when stdout is piped

// ─── P5.6 smoke test — load a real Editor-exported prefab ─────────────────
//
// Loads demo/assets/editor-sample/prefab01.prefab through the full asset
// pipeline (uuid-map → AssetManager → PrefabLoader → JsonDeserializer →
// Prefab::instantiate → NodeActivator). This exercises every piece we
// built in P5.5 / P5.6:
//   • cc.Node / cc.UITransform / cc.Sprite / cc.Label / cc.Button /
//     cc.Canvas / cc.Widget / cc.Camera deserialize via reflection
//   • _prefab field on the root → PrefabInfo + (maybe) PrefabInstance
//   • ClickEventHandler arrays on cc.Button resolve via reflection
//   • Cross-ref pointers (_parent, _children, _components) patch via
//     JsonDeserializer's phase 2.5 back-ref pass
//
// Runs at startup; if the prefab loads cleanly the log will print the
// node / component counts. Visible output: whatever the authored prefab
// renders (a Canvas-based UI layout).

class PrefabLoadScene : public DemoScene {
public:
    const char *name() const override { return "PrefabLoadScene"; }

    void onEnter(cc::scene::RenderScene * /*rs*/, cc::Root * /*root*/) override {
        constexpr const char *kUuid = "ec7cc104-9842-4c07-93e2-3a12cf40e7f6";

        auto prefab = cc::AssetManager::get().load<cc::Prefab>(kUuid);
        if (!prefab) {
            CC_LOG_ERROR("[PrefabLoadScene] failed to load prefab01 by uuid %s", kUuid);
            return;
        }
        CC_LOG_INFO("[PrefabLoadScene] prefab01 loaded — rootIndex=%zu jsonSize=%zu",
                    prefab->getRootIndex(), prefab->getData().size());

        cc::Node *root = prefab->instantiate();
        if (!root) {
            CC_LOG_ERROR("[PrefabLoadScene] instantiate() returned nullptr");
            return;
        }
        _root = root;

        // Structural dump to confirm the tree decoded as expected.
        dumpTree(root, 0);

        cc::NodeActivator::get().activateNode(root, true);
        CC_LOG_INFO("[PrefabLoadScene] tree activated — root='%s' children=%zu",
                    root->getName().c_str(), root->getChildren().size());

        // Force-flush so the CLI capture sees our structural dump even when
        // the demo is killed shortly after startup. Cheap — only runs once.
        std::fflush(stdout);
        std::fflush(stderr);
    }

    void onExit() override {
        if (_root) {
            cc::NodeActivator::get().activateNode(_root, false);
            _root->release();
            _root = nullptr;
        }
    }

private:
    // Walks the tree printing one line per node (depth-indented) with
    // component class names in brackets. Stops at 6 levels — our
    // prefab01 is shallow, but the guard avoids runaway logs if a cycle
    // slips through.
    static void dumpTree(cc::Node *node, int depth) {
        if (!node || depth > 6) return;
        ccstd::string indent(depth * 2, ' ');
        ccstd::string comps;
        for (const auto &c : node->getComponentList()) {
            if (!c || !c->getClass() || !c->getClass()->name) continue;
            if (!comps.empty()) comps += ",";
            comps += c->getClass()->name;
        }
        CC_LOG_INFO("[PrefabLoadScene] %s- '%s' [%s]",
                    indent.c_str(),
                    node->getName().empty() ? "<unnamed>" : node->getName().c_str(),
                    comps.empty() ? "(no components)" : comps.c_str());
        for (const auto &child : node->getChildren()) {
            dumpTree(child.get(), depth + 1);
        }
    }

    cc::Node *_root{nullptr};
};

REGISTER_DEMO_SCENE("PrefabLoadScene", PrefabLoadScene);
