#include "SceneRegistry.h"
#include "base/Log.h"
#include "base/std/container/string.h"
#include "cocos/asset/AssetManager.h"
#include "cocos/asset/Prefab.h"
#include "cocos/serialization/JsonDeserializer.h"
#include "core/assets/EffectAsset.h"
#include "core/assets/Material.h"
#include "core/component/NodeActivator.h"
#include "core/scene-graph/Node.h"
#include "platform/FileUtils.h"

// ─── P3-6 — full editor-scene load pipeline ────────────────────────────────
//
// Loads demo/assets/editor-sample/main.scene through JsonDeserializer,
// which delegates __uuid__ references to AssetManager (registered in
// DemoGame). The scene JSON describes a Canvas root + a Sprite node that
// references a Texture2D by UUID — no code hand-holding.
//
// Chain:
//   main.scene → JsonDeserializer → for each Sprite's _texture __uuid__
//       → AssetManager.loadAsset(uuid)
//           → uuidToPath lookup  → "atom.png"
//           → ".png" loader      → TextureLoader::loadFromFile
//           → Texture2D asset (cached in AssetManager)
//       → IntrusivePtr wired into Sprite._texture
//   activateNode(root)
//   → Canvas camera + Sprite quad render over dark-blue

class EditorSceneLoadScene : public DemoScene {
public:
    const char *name() const override { return "P3 — editor scene load"; }

    void onEnter(cc::scene::RenderScene * /*rs*/, cc::Root * /*root*/) override {
        auto *fu = cc::FileUtils::getInstance();
        const char *scenePath = "assets/editor-sample/main.scene";

        ccstd::string jsonText;
        if (fu->isFileExist(scenePath)) {
            jsonText = fu->getStringFromFile(scenePath);
        } else {
            auto full = fu->fullPathForFilename(scenePath);
            if (!full.empty()) jsonText = fu->getStringFromFile(full);
        }
        if (jsonText.empty()) {
            CC_LOG_ERROR("[EditorSceneLoadScene] scene not found: %s", scenePath);
            return;
        }

        cc::serialization::JsonDeserializer d;
        _root = d.deserializeAs<cc::Node>(jsonText);
        if (!_root) {
            CC_LOG_ERROR("[EditorSceneLoadScene] deserialize failed (see earlier log)");
            return;
        }
        _root->addRef();

        CC_LOG_INFO("[EditorSceneLoadScene] loaded scene — %zu objects, root '%s' activating",
                    d.objectCount(),
                    _root->getName().empty() ? "<unnamed>" : _root->getName().c_str());
        cc::NodeActivator::get().activateNode(_root, true);

        // P3-5 smoke probe: resolve a .mtl through the AssetManager and
        // confirm the Material landed with an effect. Not wired into the
        // scene graph — just exercises the loader path end-to-end.
        auto mat = cc::AssetManager::get().load<cc::Material>(
            "11111111-2222-3333-4444-555555555555");
        if (mat && mat->getEffectAsset()) {
            CC_LOG_INFO("[EditorSceneLoadScene] .mtl probe ok — effect='%s' techIdx=%u",
                        mat->getEffectName().c_str(), mat->getTechniqueIndex());
        } else {
            CC_LOG_WARNING("[EditorSceneLoadScene] .mtl probe failed");
        }

        // P3+c smoke probe: resolve a .effect via the builtin-shortcut loader.
        auto fx = cc::AssetManager::get().load<cc::EffectAsset>(
            "3fffffff-eeee-dddd-cccc-bbbbbbbbbbbb");
        if (fx) {
            CC_LOG_INFO("[EditorSceneLoadScene] .effect probe ok — name='%s'",
                        fx->getName().c_str());
        } else {
            CC_LOG_WARNING("[EditorSceneLoadScene] .effect probe failed");
        }

        // P3-7 smoke probe: resolve a .prefab, instantiate it twice, and
        // verify each clone is a distinct Node. Instantiated nodes are
        // parented under the scene root so the prefab's Sprite renders
        // alongside the authored ones.
        auto prefab = cc::AssetManager::get().load<cc::Prefab>(
            "22222222-aaaa-bbbb-cccc-333333333333");
        if (prefab) {
            cc::Node *a = prefab->instantiate();
            cc::Node *b = prefab->instantiate();
            if (a && b && a != b) {
                CC_LOG_INFO("[EditorSceneLoadScene] .prefab probe ok — "
                            "rootIndex=%zu a='%s' b='%s'",
                            prefab->getRootIndex(),
                            a->getName().c_str(), b->getName().c_str());
                if (_root) {
                    a->setParent(_root);
                    b->setParent(_root);
                    cc::NodeActivator::get().activateNode(a, true);
                    cc::NodeActivator::get().activateNode(b, true);
                }
                // Release our local refs — the parent now retains them.
                a->release();
                b->release();
            } else {
                CC_LOG_WARNING("[EditorSceneLoadScene] .prefab probe — "
                               "instantiate returned invalid nodes");
            }
        } else {
            CC_LOG_WARNING("[EditorSceneLoadScene] .prefab probe — load failed");
        }
    }

    void onExit() override {
        if (_root) {
            cc::NodeActivator::get().activateNode(_root, false);
            _root->release();
            _root = nullptr;
        }
    }

private:
    cc::Node *_root{nullptr};
};

REGISTER_DEMO_SCENE("EditorSceneLoadScene", EditorSceneLoadScene);
