cocos4wasm — pure C++ Cocos engine
==================================

A fork of the [Cocos Creator](https://www.cocos.com/en/creator) native engine
(`cocos-engine`), re-targeted at a **pure C++ / WebAssembly game engine**.
The JS script engine (V8 / SpiderMonkey / NAPI) and the entire JSB binding
layer have been removed; games are authored directly in C++ against the new
`cocos/game/` API plus a reflected Node/Component system that mirrors the
TypeScript API shape (`node->addComponent<Sprite>()`, `getComponent<T>()`).

What this fork adds on top of upstream native
---------------------------------------------
- **Reflection** — `ClassDB` + `CC_CLASS` / `CC_PROPERTY` / `CC_METHOD`
  macros (`cocos/core/reflection/`) powering component lookup by name,
  JSON deserialization and Editor-handler dispatch.
- **Component system** — `Component`, `ComponentScheduler`, `NodeActivator`
  with the full Creator lifecycle (`cocos/core/component/`).
- **2D / UI in C++** — Canvas, Sprite, Label (TTF + BMFont), Graphics, Mask,
  Button, ScrollView, EditBox, Widget, Layout, … (`cocos/2d/`, `cocos/ui/`),
  rendered by a pure-C++ `UIBatcher2d`.
- **Asset pipeline** — `AssetManager` with UUID map, Editor-JSON scene /
  prefab loading including prefab instance overrides (`cocos/asset/`,
  `cocos/serialization/`).
- **3D via USD** — TinyUSDZ integration (USDA/USDC/USDZ) at
  `cocos/game/USDLoader`, UsdPreviewSurface → PBR materials.
- **Input** — window events → UI hit-test → capture/bubble event dispatch
  (`cocos/input/InputEventDispatcher`).

Getting started
---------------
```bash
node utils/download-deps.js        # fetch external/ prebuilt third-party libs

cd demo                            # the main dev vehicle (scene playground)
cmake -B build -A x64
cmake --build build --config Debug
```
Test trees: `tests/cpp-test` (visual/integration), `tests/unit-test` (gtest),
`tests/voxel-test` (3D stress demo), `tests/module-tests` (reduced engine).

Platforms
---------
Windows is the primary development target today; Emscripten/WASM is the
eventual deployment target (pure C++ — no JS engine, minimal JS facade only).
Other upstream platforms (macOS, iOS, Android, Linux, OHOS) retain their
platform layers but are not actively maintained in this fork.

Build requirements
------------------
- Visual Studio 2019+ (win64)
- CMake 3.16+
- Node.js (only for `utils/` helper scripts)

C++ standard
------------
C++17. `std::string_view` and `constexpr if` are safe everywhere;
`std::optional` is avoided for iOS 11 compatibility.

Coding style
------------
Format with [clang-format](https://clang.llvm.org/docs/ClangFormat.html)
(`.clang-format`) and lint with clang-tidy (`.clang-tidy`); see
[docs/LINTER_AUTOFIX_GUIDE.md](docs/LINTER_AUTOFIX_GUIDE.md).
Use `ccnew` instead of raw `new` for engine objects.
