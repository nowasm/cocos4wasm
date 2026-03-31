# 实现计划：wasm-platform-support

## 概述

按依赖顺序实现 WebAssembly 平台支持：先建立 C++ 平台层，再接入引擎构建系统，然后提供 CMake 宏和项目模板，最后完善 CLI 工具并补充属性测试。

## 任务

- [x] 1. 创建 Wasm 平台模块 stub 实现（cocos/platform/wasm/modules/）
  - 在 `cocos/platform/wasm/modules/` 下创建 9 个模块的头文件和实现文件，直接复用 empty 平台的接口签名
  - 模块列表：`Accelerometer`、`Battery`、`Network`、`Screen`、`System`、`SystemWindow`、`SystemWindowManager`、`Vibrator`、`CanvasRenderingContext2DDelegate`
  - 每个模块的行为与 `cocos/platform/empty/modules/` 对应模块保持一致（stub 空实现）
  - _需求：1.3_

- [x] 2. 创建 WasmPlatform 主类
  - [x] 2.1 创建 `cocos/platform/wasm/WasmPlatform.h`
    - 继承 `UniversalPlatform`，声明 `init()`、`loop()`、`exit()` 方法
    - 声明私有成员 `_quit`（bool）
    - _需求：1.1_

  - [x] 2.2 创建 `cocos/platform/wasm/WasmPlatform.cpp`
    - `init()`：注册 Accelerometer、Battery、Network、Screen、System、SystemWindowManager、Vibrator 七个接口模块，返回 0
    - `loop()`：调用 `emscripten_set_main_loop` 注册帧回调（lambda 调用 `runTask()`），立即返回
    - `exit()`：调用 `emscripten_cancel_main_loop()` 后调用 `onDestroy()`
    - _需求：1.1、1.2、1.4、1.5、1.6_

  - [x] 2.3 为属性 1 编写属性测试（C++，RapidCheck）
    - **属性 1：init() 注册所有平台接口**
    - 验证 `init()` 返回 0，且所有 7 个接口模块均非空指针
    - **验证：需求 1.2**

- [x] 3. 检查点 — 确认平台层编译无误
  - 确保所有测试通过，如有疑问请向用户确认。

- [x] 4. 修改根 CMakeLists.txt 添加 EMSCRIPTEN 条件分支
  - [x] 4.1 在 `elseif(LINUX)` 平台主类分支（约第 576 行）之后插入 `elseif(EMSCRIPTEN)` 分支
    - 加入 `cocos/platform/wasm/WasmPlatform.cpp` 和 `cocos/platform/wasm/WasmPlatform.h`
    - _需求：2.1、2.5_

  - [x] 4.2 在 `elseif(LINUX)` CanvasRenderingContext2DDelegate 分支（约第 699 行）之后插入 EMSCRIPTEN 分支
    - 加入 `cocos/platform/wasm/modules/CanvasRenderingContext2DDelegate.cpp` 和 `.h`
    - _需求：2.2_

  - [x] 4.3 在 `elseif(LINUX)` 模块实现分支（约第 921 行）之后插入 EMSCRIPTEN 模块分支
    - 加入 wasm/modules/ 下 Accelerometer、Battery、Network、Screen、System、SystemWindow、SystemWindowManager、Vibrator 的 .cpp 和 .h
    - _需求：2.2_

  - [x] 4.4 在图形选项设置区域（`elseif(ANDROID OR WINDOWS OR OHOS)` 附近）添加 EMSCRIPTEN 图形选项
    - 设置 `CC_USE_GLES3 ON`、`CC_USE_VULKAN OFF`、`CC_USE_METAL OFF`
    - 设置 `USE_VIDEO OFF`、`USE_WEBVIEW OFF`、`USE_AUDIO OFF`（默认关闭）
    - _需求：2.3、2.4_

- [x] 5. 创建 templates/cmake/emscripten.cmake
  - [x] 5.1 实现 `cc_emscripten_before_target(_target_name)` 宏
    - 设置 `CC_EXECUTABLE_NAME`（来自 `APP_NAME`，校验 `[_0-9a-zA-Z-]+` 格式）
    - 将 `${CC_PROJECT_DIR}/main.cpp` 加入 `CC_PROJ_SOURCES`
    - 合并 `CC_ALL_SOURCES`，调用 `cc_common_before_target`
    - _需求：3.1_

  - [x] 5.2 实现 `cc_emscripten_after_target(_target_name)` 宏
    - 链接 `${ENGINE_NAME}`
    - 通过 `target_include_directories` 添加 `${CC_PROJECT_DIR}/../common/Classes`
    - 通过 `target_link_options` 设置：`-sUSE_WEBGL2=1`、`-sFULL_ES3=1`、`-sALLOW_MEMORY_GROWTH=1`、`--bind`、`-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']`
    - 通过 `set_target_properties` 设置输出后缀 `.js`
    - 当 `${RES_DIR}/data` 存在时，添加 `--preload-file` 链接选项
    - 调用 `cc_common_after_target`
    - _需求：3.2、3.3、3.4、3.5、3.6_

  - [x] 5.3 为属性 4 编写属性测试（Node.js，fast-check）
    - **属性 4：Emscripten 编译选项正确配置**
    - 解析生成的 CMakeCache.txt，验证链接选项包含 `-sUSE_WEBGL2=1`、`-sFULL_ES3=1`、`-sALLOW_MEMORY_GROWTH=1`、`--bind`，且 SUFFIX 为 `.js`
    - **验证：需求 3.3、3.4、3.6**

- [x] 6. 创建 templates/wasm32/ 项目模板
  - [x] 6.1 创建 `templates/wasm32/CMakeLists.txt`
    - 风格与 `templates/linux/CMakeLists.txt` 保持一致
    - 调用 `cc_emscripten_before_target(${APP_NAME})` 和 `cc_emscripten_after_target(${APP_NAME})`
    - include `${CC_PROJECT_DIR}/../cmake/emscripten.cmake`
    - _需求：4.1_

  - [x] 6.2 创建 `templates/wasm32/main.cpp`
    - 内容与 `templates/linux/main.cpp` 相同，调用 `START_PLATFORM(argc, argv)`
    - _需求：4.2_

  - [x] 6.3 创建 `templates/wasm32/cfg.cmake`
    - 设置：`USE_SE_V8 OFF`、`USE_SE_SM OFF`、`USE_V8_DEBUGGER OFF`、`CC_USE_GLES3 ON`
    - _需求：4.3_

- [x] 7. 完善 c4-cli new.js 的 wasm32 支持
  - [x] 7.1 添加 wasm32 模板目录不存在时的错误处理
    - 在复制平台模板前，检测 `templates/wasm32/` 是否存在
    - 若不存在，输出明确错误信息并以非零状态码退出（`process.exit(1)`）
    - _需求：5.5_

  - [x] 7.2 在项目创建成功后输出 wasm32 专属构建提示
    - 当 `platform === 'wasm32'` 时，在成功消息后追加以下提示：
      - 激活 emsdk 的命令（`source /path/to/emsdk/emsdk_env.sh`）
      - `emcmake cmake .. -G Ninja` 配置命令
      - `cmake --build .` 编译命令
      - `python -m http.server 8080` 预览命令
    - _需求：5.6_

  - [x] 7.3 为属性 2 编写属性测试（Node.js，fast-check）
    - **属性 2：new 命令生成完整项目结构**
    - 对任意符合 `[_0-9a-zA-Z-]+` 的项目名，验证生成目录包含 `proj/`、`cmake/`、`common/`、`Classes/`、`Resources/`、`CMakeLists.txt`、`.cocos-engine`
    - **验证：需求 4.4、5.1、5.2、5.3**

  - [x] 7.4 为属性 3 编写属性测试（Node.js，fast-check）
    - **属性 3：模板变量替换完整性**
    - 对任意有效项目名，验证 `proj/CMakeLists.txt` 中不含 `CocosGame`，且包含实际项目名
    - **验证：需求 4.5**

  - [x] 7.5 为属性 5 编写属性测试（Node.js，fast-check）
    - **属性 5：wasm32 构建提示完整性**
    - 对任意有效项目名，验证 `new` 命令输出包含 `emcmake`、`cmake --build`、`http.server`
    - **验证：需求 5.6**

  - [x] 7.6 为属性 6 编写属性测试（Node.js，fast-check）
    - **属性 6：run 命令产物检测与预览提示**
    - 验证 `run -p wasm32` 命令输出包含 `http.server`，无论产物是否存在
    - **验证：需求 6.2、6.4**

- [x] 8. 最终检查点 — 确保所有测试通过
  - 确保所有测试通过，如有疑问请向用户确认。

## 备注

- 标有 `*` 的子任务为可选项，可跳过以加快 MVP 进度
- 每个任务均引用具体需求条目，便于追溯
- 检查点确保增量验证
- 属性测试验证普遍正确性，单元测试验证具体示例和边界条件

---

## 扩展任务：恢复5个禁用模块（任务 9–13）

### 概述

按依赖复杂度从低到高恢复：JPEG/PNG（sysroot 直接可用）→ WebP（需预编译库）→ Audio（需 ports + 源文件补充）→ FreeType/DebugRenderer（需 port + 强制禁用解除）。

- [x] 9. 恢复 JPEG/PNG 图片格式支持
  - [x] 9.1 移除 `CMakeLists.txt` 中强制禁用 CC_USE_JPEG/CC_USE_PNG 的两行
    - 定位 `cc_apply_definations` 函数中的以下两行并删除：
      ```cmake
      $<$<BOOL:${EMSCRIPTEN}>:CC_USE_JPEG=0>
      $<$<BOOL:${EMSCRIPTEN}>:CC_USE_PNG=0>
      ```
    - _需求：7.3_

  - [x] 9.2 在 `external/emscripten/CMakeLists.txt` 中添加 jpeg 和 png INTERFACE 目标
    - 取消注释或新增以下内容：
      ```cmake
      add_library(jpeg INTERFACE)
      target_link_options(jpeg INTERFACE -sUSE_LIBJPEG=1)

      add_library(png INTERFACE)
      target_link_options(png INTERFACE -sUSE_LIBPNG=1)
      ```
    - 将 `jpeg` 和 `png` 加入 `CC_EXTERNAL_LIBS`
    - _需求：7.1、7.2、7.4_

  - [x] 9.3 修复 `cocos/platform/Image.cpp` 中 EMSCRIPTEN 下的 include 路径
    - `#include "jpeg/jpeglib.h"` 在 EMSCRIPTEN 下改为 `#include <jpeglib.h>`
    - PNG 的 include 已有 `__LINUX__` 分支使用 `"png.h"`，在该条件中补充 `|| defined(__EMSCRIPTEN__)` 或添加独立 EMSCRIPTEN 分支
    - _需求：7.5_

  - [ ]* 9.4 验证 unit-test 仍然通过
    - 确认 EMSCRIPTEN 构建中 `CC_USE_JPEG=1`、`CC_USE_PNG=1` 编译定义生效
    - 确保 unit-test 编译和运行不受影响
    - _需求：7.1、7.2_

- [x] 10. 恢复 WebP 图片格式支持
  - [x] 10.1 准备 WebP wasm32 预编译库
    - 将 wasm32 编译的 `libwebp.a` 放入 `external/emscripten/libs/libwebp/libwebp.a`
    - 将 WebP 头文件（`decode.h`、`encode.h` 等）放入 `external/emscripten/include/webp/`
    - 可从 `external/linux/lib/libwebp/` 参考目录结构，或通过 `emcmake cmake` 自行编译 libwebp 源码
    - _需求：8.1_

  - [x] 10.2 在 `external/emscripten/CMakeLists.txt` 中添加 webp IMPORTED STATIC 目标
    - 添加：
      ```cmake
      if(USE_WEBP)
        add_library(webp STATIC IMPORTED GLOBAL)
        set_target_properties(webp PROPERTIES
          IMPORTED_LOCATION ${CMAKE_CURRENT_LIST_DIR}/libs/libwebp/libwebp.a
          INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_LIST_DIR}/include
        )
        list(APPEND CC_EXTERNAL_LIBS webp)
      endif()
      ```
    - _需求：8.2、8.4_

  - [x] 10.3 修改 `CMakeLists.txt` 中 `CC_USE_WEBP` generator expression
    - 将：
      ```cmake
      $<IF:$<AND:$<BOOL:${USE_WEBP}>,$<NOT:$<BOOL:${EMSCRIPTEN}>>>,CC_USE_WEBP=1,CC_USE_WEBP=0>
      ```
      改为：
      ```cmake
      $<IF:$<BOOL:${USE_WEBP}>,CC_USE_WEBP=1,CC_USE_WEBP=0>
      ```
    - _需求：8.3_

  - [ ]* 10.4 验证 unit-test 仍然通过
    - 确认 EMSCRIPTEN 构建中 `CC_USE_WEBP=1` 编译定义生效
    - 确保 unit-test 编译和运行不受影响
    - _需求：8.3_

- [x] 11. 检查点 — 确保 JPEG/PNG/WebP 相关测试通过
  - 确保所有测试通过，如有疑问请向用户确认。

- [x] 12. 恢复音频模块支持
  - [x] 12.1 移除 `CMakeLists.txt` EMSCRIPTEN 块中的 `USE_AUDIO` 默认禁用行
    - 删除或注释掉：`cc_set_if_undefined(USE_AUDIO OFF)`（位于 `## disable unsupported features on EMSCRIPTEN` 块中）
    - 使全局默认值 `cc_set_if_undefined(USE_AUDIO ON)` 对 EMSCRIPTEN 生效
    - _需求：9.1_

  - [x] 12.2 在 `CMakeLists.txt` 的 EMSCRIPTEN `if(USE_AUDIO)` 分支中补充 ogg/vorbis/mpg123 解码器源文件
    - 参考 `elseif(LINUX OR QNX)` 分支，在 `elseif(EMSCRIPTEN)` 分支中补充：
      ```cmake
      cocos/audio/common/decoder/AudioDecoderMp3.cpp
      cocos/audio/common/decoder/AudioDecoderMp3.h
      cocos/audio/common/decoder/AudioDecoderOgg.cpp
      cocos/audio/common/decoder/AudioDecoderOgg.h
      ```
    - _需求：9.1_

  - [x] 12.3 在 `external/emscripten/CMakeLists.txt` 中添加音频 INTERFACE 目标
    - 添加：
      ```cmake
      if(USE_AUDIO)
        add_library(openal INTERFACE)
        target_link_options(openal INTERFACE -lopenal)

        add_library(ogg INTERFACE)
        target_link_options(ogg INTERFACE -sUSE_OGG=1)
        target_compile_options(ogg INTERFACE -sUSE_OGG=1)

        add_library(vorbis INTERFACE)
        target_link_options(vorbis INTERFACE -sUSE_VORBIS=1)
        target_compile_options(vorbis INTERFACE -sUSE_VORBIS=1)

        add_library(mpg123 INTERFACE)
        target_link_options(mpg123 INTERFACE -sUSE_MPG123=1)
        target_compile_options(mpg123 INTERFACE -sUSE_MPG123=1)

        list(APPEND CC_EXTERNAL_LIBS openal ogg vorbis mpg123)
      endif()
      ```
    - _需求：9.2、9.3、9.4_

  - [ ]* 12.4 验证音频模块编译正确
    - 确认 `CC_USE_AUDIO=1` 编译定义生效
    - 确认 OpenAL include 路径（`AL/al.h`）在 EMSCRIPTEN sysroot 下可正确解析
    - 确保 unit-test 编译和运行不受影响
    - _需求：9.5_

- [x] 13. 恢复 FreeType/DebugRenderer 支持
  - [x] 13.1 移除 `CMakeLists.txt` EMSCRIPTEN 块中强制禁用 `USE_DEBUG_RENDERER` 的行
    - 删除：`set(USE_DEBUG_RENDERER OFF CACHE BOOL "Debug renderer (FreeType)" FORCE)`
    - 使全局默认值 `cc_set_if_undefined(USE_DEBUG_RENDERER ON)` 对 EMSCRIPTEN 生效
    - _需求：10.1_

  - [x] 13.2 在 `external/emscripten/CMakeLists.txt` 中添加 freetype INTERFACE 目标
    - 添加：
      ```cmake
      if(USE_DEBUG_RENDERER)
        add_library(freetype INTERFACE)
        target_link_options(freetype INTERFACE -sUSE_FREETYPE=1)
        target_compile_options(freetype INTERFACE -sUSE_FREETYPE=1)
        list(APPEND CC_EXTERNAL_LIBS freetype)
      endif()
      ```
    - _需求：10.2、10.3_

  - [ ]* 13.3 验证 FreeType 头文件可正确解析
    - 确认 `cocos/core/assets/FreeTypeFont.cpp` 中的 `#include <ft2build.h>` 在 EMSCRIPTEN 下能找到（emscripten freetype port 自动将头文件放入 sysroot）
    - 确保 unit-test 编译和运行不受影响
    - _需求：10.4_

- [x] 14. 最终检查点 — 确保所有模块恢复后测试通过
  - 确保所有测试通过，如有疑问请向用户确认。
