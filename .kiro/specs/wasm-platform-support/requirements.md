# 需求文档

## 简介

本功能为 c4 引擎（基于 Cocos Creator 的 C++ 游戏引擎）添加 WebAssembly（Wasm）平台编译支持，并完善 `tools/c4-cli` 工具的 `wasm32` 平台项目创建能力。

目标是让开发者能够通过 `c4-cli new` 命令创建一个完整可用的 wasm32 项目，并通过 `emcmake cmake` + `cmake --build` 成功编译出可在浏览器中运行的 `.wasm` / `.js` 产物。

实现路径参考现有 `empty` 平台（最小化平台实现）和 `linux` 平台（SDL 窗口管理），以及 `templates/linux/` 模板目录结构。

---

## 术语表

- **WASM_Platform**：`cocos/platform/wasm/` 下的 Wasm 平台实现层，负责注册平台接口模块。
- **Emscripten**：将 C/C++ 编译为 WebAssembly 的工具链，通过 `emcmake` / `emmake` 调用。
- **emsdk**：Emscripten SDK，本机安装路径为 `/Users/t5/devlib/emsdk`。
- **emscripten.cmake**：`templates/cmake/emscripten.cmake`，提供 `cc_emscripten_before_target` / `cc_emscripten_after_target` 宏，供 wasm32 项目模板的 `CMakeLists.txt` 调用。
- **wasm32 模板**：`templates/wasm32/` 目录，包含 `CMakeLists.txt`、`main.cpp`、`cfg.cmake`，由 `c4-cli new -p wasm32` 复制到新项目的 `proj/` 目录。
- **c4-cli**：`tools/c4-cli`，Node.js CLI 工具，提供 `new` / `run` / `compile` 命令。
- **CC_EXECUTABLE_NAME**：CMake 变量，存储最终可执行目标名称。
- **ENGINE_NAME**：CMake 变量，值为 `cocos_engine`，代表引擎静态库目标。

---

## 需求

### 需求 1：Wasm 平台实现层（cocos/platform/wasm/）

**用户故事：** 作为引擎开发者，我希望在 `cocos/platform/wasm/` 下有一套完整的平台接口实现，以便引擎在 Emscripten 编译环境下能正确初始化平台模块。

#### 验收标准

1. THE WASM_Platform SHALL 提供 `WasmPlatform` 类，继承自 `UniversalPlatform`，实现 `init()`、`loop()` 和 `exit()` 方法。
2. WHEN `WasmPlatform::init()` 被调用时，THE WASM_Platform SHALL 注册以下接口模块：`Accelerometer`、`Battery`、`Network`、`Screen`、`System`、`SystemWindowManager`、`Vibrator`。
3. THE WASM_Platform SHALL 在 `cocos/platform/wasm/modules/` 下提供所有接口模块的 stub 实现（`Accelerometer`、`Battery`、`Network`、`Screen`、`System`、`SystemWindow`、`SystemWindowManager`、`Vibrator`、`CanvasRenderingContext2DDelegate`），每个模块的行为与 `empty` 平台对应模块保持一致。
4. WHEN `WasmPlatform::loop()` 被调用时，THE WASM_Platform SHALL 通过 Emscripten 的 `emscripten_set_main_loop` 机制驱动主循环，而非阻塞线程。
5. IF Emscripten 主循环回调被触发，THEN THE WASM_Platform SHALL 调用 `runTask()` 执行一帧逻辑。
6. WHEN `WasmPlatform::exit()` 被调用时，THE WASM_Platform SHALL 调用 `emscripten_cancel_main_loop()` 停止主循环并调用 `onDestroy()`。

---

### 需求 2：根 CMakeLists.txt 添加 WASM 平台条件分支

**用户故事：** 作为引擎构建系统维护者，我希望根 `CMakeLists.txt` 能识别 `EMSCRIPTEN` 工具链并编译 Wasm 平台源码，以便引擎库在 Emscripten 环境下正确构建。

#### 验收标准

1. WHEN CMake 检测到 `EMSCRIPTEN` 变量为真时，THE 构建系统 SHALL 将 `cocos/platform/wasm/WasmPlatform.cpp` 和 `cocos/platform/wasm/WasmPlatform.h` 加入编译源文件列表。
2. WHEN CMake 检测到 `EMSCRIPTEN` 变量为真时，THE 构建系统 SHALL 将 `cocos/platform/wasm/modules/` 下的所有模块源文件加入编译列表，包括 `CanvasRenderingContext2DDelegate`。
3. WHEN CMake 检测到 `EMSCRIPTEN` 变量为真时，THE 构建系统 SHALL 将 `CC_USE_GLES3` 设置为 `ON`，将 `CC_USE_VULKAN` 和 `CC_USE_METAL` 设置为 `OFF`。
4. WHEN CMake 检测到 `EMSCRIPTEN` 变量为真时，THE 构建系统 SHALL 禁用不适用于 Wasm 的功能选项：`USE_VIDEO`、`USE_WEBVIEW`、`USE_AUDIO`（默认关闭，可由项目覆盖）。
5. THE 构建系统 SHALL 在现有 `elseif(LINUX)` 平台分支之后、`elseif(ANDROID)` 之前插入 `elseif(EMSCRIPTEN)` 分支，保持与其他平台条件分支风格一致。

---

### 需求 3：templates/cmake/emscripten.cmake 宏定义

**用户故事：** 作为 wasm32 项目开发者，我希望 `templates/cmake/emscripten.cmake` 提供标准的 `cc_emscripten_before_target` 和 `cc_emscripten_after_target` 宏，以便项目模板的 `CMakeLists.txt` 能以统一方式配置 Emscripten 编译选项。

#### 验收标准

1. THE emscripten.cmake SHALL 提供 `cc_emscripten_before_target(_target_name)` 宏，负责收集项目源文件（`main.cpp`）、设置 `CC_EXECUTABLE_NAME`，并调用 `cc_common_before_target`。
2. THE emscripten.cmake SHALL 提供 `cc_emscripten_after_target(_target_name)` 宏，负责链接 `ENGINE_NAME`、设置 Emscripten 链接选项，并调用 `cc_common_after_target`。
3. WHEN `cc_emscripten_after_target` 被调用时，THE emscripten.cmake SHALL 通过 `target_link_options` 设置以下 Emscripten 链接标志：`-sUSE_WEBGL2=1`、`-sFULL_ES3=1`、`-sALLOW_MEMORY_GROWTH=1`、`--bind`、`-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']`。
4. WHEN `cc_emscripten_after_target` 被调用时，THE emscripten.cmake SHALL 通过 `set_target_properties` 将输出后缀设置为 `.js`，使 Emscripten 同时生成 `.js` 胶水代码和 `.wasm` 文件。
5. WHERE `RES_DIR/data` 目录存在，THE emscripten.cmake SHALL 通过 `--preload-file` 链接选项将资源目录打包进 `.data` 文件。
6. THE emscripten.cmake SHALL 通过 `target_include_directories` 将 `${CC_PROJECT_DIR}/../common/Classes` 加入头文件搜索路径。

---

### 需求 4：templates/wasm32/ 项目模板

**用户故事：** 作为游戏开发者，我希望 `templates/wasm32/` 目录提供完整的项目模板文件，以便 `c4-cli new -p wasm32` 命令能生成一个开箱即用的 wasm32 项目。

#### 验收标准

1. THE wasm32 模板 SHALL 包含 `CMakeLists.txt`，其内容调用 `cc_emscripten_before_target` 和 `cc_emscripten_after_target` 宏，风格与 `templates/linux/CMakeLists.txt` 保持一致。
2. THE wasm32 模板 SHALL 包含 `main.cpp`，其内容与 `templates/linux/main.cpp` 相同（调用 `START_PLATFORM` 宏）。
3. THE wasm32 模板 SHALL 包含 `cfg.cmake`，用于设置 wasm32 平台专属的 CMake 选项，包括：`USE_SE_V8 OFF`、`USE_SE_SM OFF`、`USE_V8_DEBUGGER OFF`、`CC_USE_GLES3 ON`。
4. WHEN `c4-cli new -p wasm32 <ProjectName>` 执行后，THE c4-cli SHALL 将 `templates/wasm32/` 内容复制到 `<ProjectName>/proj/` 目录。
5. WHEN 模板变量替换执行时，THE c4-cli SHALL 将模板中的 `CocosGame` 替换为实际项目名称。

---

### 需求 5：c4-cli new 命令完整支持 wasm32 平台

**用户故事：** 作为游戏开发者，我希望 `c4-cli new -p wasm32 <ProjectName>` 命令能生成一个结构完整、可直接用 `emcmake cmake` 编译的项目，以便快速开始 Wasm 游戏开发。

#### 验收标准

1. WHEN `c4-cli new -p wasm32 <ProjectName>` 执行时，THE c4-cli SHALL 创建包含以下目录结构的项目：`proj/`（来自 wasm32 模板）、`cmake/`（来自 templates/cmake/）、`common/`（来自 templates/common/）、`Classes/`、`Resources/`。
2. WHEN `c4-cli new -p wasm32 <ProjectName>` 执行时，THE c4-cli SHALL 在项目根目录生成 `CMakeLists.txt`，其中包含正确的引擎路径自动检测逻辑和 `add_subdirectory(proj)` 调用。
3. WHEN `c4-cli new -p wasm32 <ProjectName>` 执行时，THE c4-cli SHALL 在项目根目录生成 `.cocos-engine` 文件，记录当前引擎路径。
4. WHEN 生成的项目在 Emscripten 环境下执行 `emcmake cmake ..` 时，THE 构建系统 SHALL 成功完成 CMake 配置阶段，不产生致命错误。
5. IF `templates/wasm32/` 目录不存在，THEN THE c4-cli SHALL 输出明确的错误信息并以非零状态码退出，而非静默跳过。
6. THE c4-cli SHALL 在项目创建成功后，输出针对 wasm32 平台的构建提示，包括：激活 emsdk 的命令、`emcmake cmake` 配置命令、`cmake --build` 编译命令、以及使用本地 HTTP 服务器预览的命令。

---

### 需求 6：构建产物验证

**用户故事：** 作为游戏开发者，我希望 wasm32 项目能成功编译并产出可在浏览器中加载的文件，以便验证整个工具链的正确性。

#### 验收标准

1. WHEN wasm32 项目完成编译时，THE 构建系统 SHALL 在构建目录的 `proj/` 子目录下生成 `<ProjectName>.js` 和 `<ProjectName>.wasm` 文件。
2. WHEN `c4-cli run -p wasm32` 执行时，THE c4-cli SHALL 检测 `<ProjectName>.js` 或 `<ProjectName>.wasm` 是否存在，并输出对应的文件路径。
3. IF 编译产物不存在，THEN THE c4-cli SHALL 输出警告信息，提示用户检查构建目录路径。
4. THE c4-cli run 命令 SHALL 在 wasm32 平台下输出使用 `python -m http.server` 或等效工具预览项目的具体命令。
