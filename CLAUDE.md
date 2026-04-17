# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository identity

This is **cocos4wasm** — a fork of the Cocos Creator native engine (`cocos-engine`) re-targeted at pure C++ / WebAssembly. The upstream engine is driven by a JS script engine (V8/SpiderMonkey/NAPI). This fork is in the middle of **ripping out the JS script engine** and replacing it with a direct C++ game API. Recent commits reflect this direction:

- `d13600d modify wasm compile error`
- `454ecd2 remove js script engine`
- `30e394e add basic box renderer test`
- `cbcdc29 add material test`
- `2f1c3e9 remove js binding file`

When CMake sees no script engine flag set it defines `USE_SE_NONE` (pure C++ mode). `USE_SE_BROWSER` is auto-selected for Emscripten builds. Assume by default that changes should keep pure-C++ mode working and should not rely on JSB bindings.

## Build system

CMake-driven, minimum 3.8 (3.16 for wasm32, 3.18 for cpp-test). C++17 required. No npm-based build — `package.json` exists only to run `utils/` helpers.

### External dependencies

Third-party libs (boost, spine, webgpu, bullet, physx, etc.) are not in the repo. They are cloned from `github.com/nowasm/cocos4wasm-external` (branch `cocos4wasm`) into `./external/` by:

```bash
node utils/download-deps.js
```

`external-config.json` pins the source. `npm run clear-platform` trims `external/` to only the libs the current host OS needs.

### Platform matrix

Platform selection happens in `cmake/predefine.cmake` via `CMAKE_SYSTEM_NAME`. Supported: Windows, macOS, iOS, Android, Linux, OHOS/OpenHarmony, QNX, NX, **Emscripten** (the primary target for this fork — sets `CC_WGPU_WASM`, forces `CC_USE_GLES3`, disables video/webview, enables `USE_SE_BROWSER`). Per-platform code lives under `cocos/platform/<name>/`.

### Key CMake options (cocos/CMakeLists.txt)

- `USE_SE_V8 / USE_SE_SM / USE_SE_NAPI / USE_SE_JSVM / USE_SE_QJS / USE_SE_BROWSER` — script engine selection. All OFF ⇒ `USE_SE_NONE` (pure C++).
- `USE_MODULES` — toggled ON by module-tests to build a reduced engine.
- `CC_USE_GLES3 / CC_USE_VULKAN / CC_USE_METAL / CC_USE_GLES2` — GFX backend, usually auto-set per platform.
- `USE_SPINE_3_8 / USE_SPINE_4_2`, `USE_PHYSICS_PHYSX`, `USE_DRAGONBONES`, `USE_AUDIO`, `USE_WEBP`, etc. — feature flags.

## Tests

There is no single `npm test`. Three distinct test trees, each with its own CMakeLists:

### `tests/cpp-test/` — integration/visual tests (Windows first)

Pure-C++ rendering test harness built on the new `cocos::game` API. `Game.cpp` bootstraps engine → device → `Root` → `ForwardPipeline` → `RenderScene` → camera + light, then swaps in a `TestScene` subclass (`TestShapes`, `TestMaterials`, `TestLighting`, `TestTransform`, `TestText`, `TestBuiltinAssets`). Tests self-register via `REGISTER_TEST(category, name, Class)` in `TestScene.h`. Build:

```bash
cd tests/cpp-test
cmake -B build -A x64
cmake --build build --config Debug    # or Release
# output: build/Debug/CocosCppTest.exe
```

Post-build copies `external/win64/libs/*.dll`, `templates/wasm32/builtin-effects.json`, and everything under `editor/assets/` next to the exe. If the exe runs but shows a gray screen, check whether `Canvas.onEnable` fires and whether `Batcher2D` has root nodes — there is a known open issue around the JS→C++ shared buffer for 2D rendering (see `memory/project_gray_screen_root_cause.md`).

### `tests/unit-test/` — gtest-style unit tests

CMake + gtest. Covers base, math, geometry, scheduler, scene-graph, framegraph. Build per its README:

```bash
cd tests/unit-test
mkdir build && cd build
cmake ..
cmake --build .
./src/CocosTest      # or src/Debug/CocosTest.exe on Windows
```

### `tests/module-tests/` — reduced-engine module harnesses

Uses `USE_MODULES=ON`. Targets: `test-log`, `test-bindings`, `test-math`, `test-fs`. Drive via `run-test-mac.sh` or `run-test-windows.sh` (builds for both Windows and Android NDK, then executes each exe). Each subdir (`log/`, `bindings/`, `math/`, `filesystem/`) has its own CMakeLists.

## Code architecture

`cocos/` is the engine. Read top-down:

- `cocos/cocos.h` — umbrella public header.
- `cocos/application/` — `BaseGame`, `CocosApplication`, `ApplicationManager`. `BaseGame` is the app entry-point subclass target; `main.cpp` files just call `START_PLATFORM(argc, argv)` from `platform/BasePlatform.h`.
- `cocos/engine/` — `BaseEngine`, `Engine`, `EngineEvents.h` (typed event listeners for `Tick`, `Mouse`, `Keyboard`, etc.).
- `cocos/core/` — `Root` (top-level runtime), scene-graph (`Node`, `Scene`, component system), assets, builtin (`BuiltinResMgr`, `BuiltinEffectLoader`), event bus, animation, geometry, memop.
- `cocos/scene/` — render-scene primitives: `RenderScene`, `Camera`, `Model`, `SubModel`, lights, `Pass`, reflection probes.
- `cocos/renderer/` — GFX abstraction (`gfx-base`) + backends (`gfx-gles3`, `gfx-vulkan`, `gfx-metal`, `gfx-wgpu`, `gfx-empty` stub, `gfx-agent`/`gfx-validator` wrappers), `pipeline/` (forward/deferred), `frame-graph/`. Use `GFXDeviceManager.h` to obtain the device; pipelines live under `pipeline/forward/`, `pipeline/deferred/`.
- `cocos/2d/`, `cocos/3d/`, `cocos/ui/`, `cocos/primitive/`, `cocos/physics/`, `cocos/audio/`, `cocos/network/`, `cocos/profiler/`, `cocos/gi/`, `cocos/xr/` — feature subsystems.
- `cocos/game/` — **the new pure-C++ game-authoring API being built in this fork**: `CocosGame.h` umbrella, plus `PrimitiveFactory`, `MaterialFactory`, `MeshRenderer`, `TextureLoader`. New tests should prefer this API over JSB.
- `cocos/platform/` — per-OS windowing, file I/O, input. `wasm/WasmPlatform.cpp` and `FileUtils-wasm.cpp` are the Emscripten entry points. SDL2 is used on desktop.
- `cocos/base/` — primitives: `Log`, `RefCounted`, `Ptr`, `RefVector`/`RefMap`, threading helpers, job system, `StringUtil`, `Value`, `Data`.

Minor layout notes:
- `bin/adapter/native/engine-adapter.js` and `bin/dev/cc/` host the JS runtime files that the (now being removed) script engine consumed. They're still referenced from cc.js / shared-buffer code paths.
- `templates/wasm32/` is the Emscripten build template (`cfg.cmake`, `main.cpp`, `main.js`, `builtin-effects.json`); `templates/common/` is shared source.
- `editor/assets/` holds builtin runtime assets (cubemaps, fonts, UI textures, default materials/effects). Tests copy this directory next to the exe; don't assume it ships in a zip.

## Coding standards

- C++17. Readme lists `std::string_view` and `constexpr if` as safe; `std::optional` is not available on iOS 11.
- Formatting via `.clang-format`, linting via `.clang-tidy`. See `docs/LINTER_AUTOFIX_GUIDE.md` (from upstream) and `utils/fix-tidy-format.sh`.
- Use `ccnew` (engine allocator macro) instead of raw `new` for engine objects; release with `->release()` on ref-counted types or via `_nodes`/`_renderers` tracking as `TestScene` does.

## Generators (rarely touched)

- `tools/bindings-generator/`, `tools/tojs/`, `tools/swig-config/` — JS binding generators (becoming dead weight as the script engine is ripped out; avoid adding new bindings).
- `tools/gfx-define-generator/`, `tools/gles-wrangler-generator/`, `tools/compile-effects.js` — GFX header and effect compilation.
- `scripts/native-pack-tool/` — packaging for template projects.
