#include "SceneRegistry.h"

DemoSceneRegistry &DemoSceneRegistry::get() {
    static DemoSceneRegistry instance;
    return instance;
}

void DemoSceneRegistry::add(const char *name, DemoSceneFactory factory) {
    _entries.push_back({name, std::move(factory)});
}
