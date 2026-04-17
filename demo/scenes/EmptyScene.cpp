#include "SceneRegistry.h"
#include "base/Log.h"

class EmptyScene : public DemoScene {
public:
    const char *name() const override { return "P0 — Empty (clear color only)"; }

    void onEnter(cc::scene::RenderScene * /*rs*/, cc::Root * /*root*/) override {
        CC_LOG_INFO("[EmptyScene] entered — window should show dark-blue clear color.");
    }

    void onExit() override {}
};

REGISTER_DEMO_SCENE("EmptyScene", EmptyScene);
