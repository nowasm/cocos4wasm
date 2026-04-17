#include <cstdio>

#include "SceneRegistry.h"
#include "base/Log.h"
#include "base/std/container/string.h"
#include "cocos/serialization/JsonDeserializer.h"
#include "core/component/NodeActivator.h"
#include "core/scene-graph/Node.h"
#include "platform/FileUtils.h"

// ─── JSON-driven tree activation ───────────────────────────────────────────
//
// Loads demo/assets/test-tree.json through the serialization layer built in
// P1c. The file describes the same 3-level node tree that LifecycleScene
// builds programmatically, but with the shape Cocos Creator's editor export
// produces: top-level array, __type__ + __id__ refs, reflected fields.
//
// Success = activation fires the same onLoad/onEnable/start/update log
// sequence as LifecycleScene, driven entirely from the JSON description.

class JsonLoadScene : public DemoScene {
public:
    const char *name() const override { return "P1c — JSON → node tree"; }

    void onEnter(cc::scene::RenderScene * /*rs*/, cc::Root * /*root*/) override {
        auto *fu = cc::FileUtils::getInstance();
        // Try several candidate paths: exe-relative (POST_BUILD copies to
        // $<TARGET_FILE_DIR>/assets/) first, then common fallbacks.
        const char *candidates[] = {
            "assets/test-tree.json",
            "demo/assets/test-tree.json",
            "../../demo/assets/test-tree.json",
        };

        ccstd::string jsonText;
        ccstd::string resolvedPath;
        for (const char *p : candidates) {
            if (fu->isFileExist(p)) {
                jsonText = fu->getStringFromFile(p);
                resolvedPath = p;
                break;
            }
            auto full = fu->fullPathForFilename(p);
            if (!full.empty() && fu->isFileExist(full)) {
                jsonText = fu->getStringFromFile(full);
                resolvedPath = full;
                break;
            }
        }

        if (jsonText.empty()) {
            CC_LOG_ERROR("[JsonLoadScene] test-tree.json not found — "
                         "expected next to the exe at assets/test-tree.json");
            return;
        }
        CC_LOG_INFO("[JsonLoadScene] loading '%s' (%zu bytes)",
                    resolvedPath.c_str(), jsonText.size());

        cc::serialization::JsonDeserializer deser;
        _loadedRoot = deser.deserializeAs<cc::Node>(jsonText);
        if (!_loadedRoot) {
            CC_LOG_ERROR("[JsonLoadScene] deserialize failed — see errors above");
            return;
        }

        CC_LOG_INFO("[JsonLoadScene] deserialized %zu objects; activating root",
                    deser.objectCount());
        CC_LOG_INFO("  root shape: children=%zu components=%zu",
                    _loadedRoot->getChildren().size(),
                    _loadedRoot->getComponentList().size());

        cc::NodeActivator::get().activateNode(_loadedRoot, true);
    }

    void onExit() override {
        if (_loadedRoot) {
            cc::NodeActivator::get().activateNode(_loadedRoot, false);
            _loadedRoot->release();
            _loadedRoot = nullptr;
        }
    }

private:
    cc::Node *_loadedRoot{nullptr};
};

REGISTER_DEMO_SCENE("JsonLoadScene", JsonLoadScene);
