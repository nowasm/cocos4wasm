# cocos4wasm demo

Standalone dev vehicle for the engine-restoration roadmap (P0–P8). Each phase
that adds a feature also lands a new `DemoScene` under `scenes/` so regressions
stay visible.

## Build (Windows, MSVC)

```bash
cd demo
cmake -B build -A x64
cmake --build build --config Debug
```

Output: `demo/build/Debug/CocosDemo.exe`

DLLs, `builtin-effects.json`, and `editor/assets/` are auto-copied next to the
exe by `CMakeLists.txt` (mirroring the `tests/cpp-test` pattern).

## Controls

| Key | Action |
|---|---|
| ← / → | Switch between registered demo scenes |
| ESC   | Quit |

## Adding a scene

1. Create `scenes/FooScene.cpp`:
   ```cpp
   #include "SceneRegistry.h"
   class FooScene : public DemoScene {
       const char *name() const override { return "Foo — demo"; }
       void onEnter(cc::scene::RenderScene *rs, cc::Root *root) override { ... }
       void onExit() override { ... }
   };
   REGISTER_DEMO_SCENE("FooScene", FooScene);
   ```
2. Add `scenes/FooScene.cpp` to `demo/CMakeLists.txt` → `DEMO_SOURCES`.
3. Rebuild.

## Phase milestones

| Phase | Validation scene |
|---|---|
| P0    | `EmptyScene` — dark-blue clear color, window opens |
| P1b   | `LifecycleScene` — custom Component prints onLoad/onEnable/start/update |
| P1c   | `JsonLoadScene` — load a Node-only `.scene` JSON into the node tree |
| P2    | `UIBasicsScene` — Canvas + Sprite + Label render correctly |
| P3    | `AssetLoadScene` — UUID refs auto-resolve |
| P4    | `UIInputScene` — Sprite receives touch/click with bubbling |
| P5    | `WidgetsScene` — Button, EditBox, ScrollView working |

See `memory/project_engine_restoration_plan.md` for the full roadmap.
