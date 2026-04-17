#pragma once
#include <functional>
#include <string>
#include <vector>

namespace cc {
class Root;
namespace scene { class RenderScene; }
}

class DemoScene {
public:
    virtual ~DemoScene() = default;
    virtual const char *name() const = 0;
    virtual void onEnter(cc::scene::RenderScene *rs, cc::Root *root) = 0;
    virtual void onUpdate(float /*dt*/) {}
    virtual void onExit() = 0;
};

using DemoSceneFactory = std::function<DemoScene *()>;

struct DemoSceneEntry {
    std::string name;
    DemoSceneFactory factory;
};

class DemoSceneRegistry {
public:
    static DemoSceneRegistry &get();
    void add(const char *name, DemoSceneFactory factory);
    const std::vector<DemoSceneEntry> &list() const { return _entries; }

private:
    std::vector<DemoSceneEntry> _entries;
};

struct DemoSceneAutoReg {
    DemoSceneAutoReg(const char *displayName, DemoSceneFactory f) {
        DemoSceneRegistry::get().add(displayName, std::move(f));
    }
};

#define REGISTER_DEMO_SCENE(DisplayName, Class) \
    static DemoSceneAutoReg _demoReg_##Class(DisplayName, [] { return new Class(); })
