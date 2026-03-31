/****************************************************************************
 Copyright (c) 2024 Xiamen Yaji Software Co., Ltd.

 Tests for wasm platform window and rendering-related modules:
 - SystemWindow: construction, ID, handle, view size
 - SystemWindowManager: init, createWindow, getWindow, multi-window
 - GFX data structures: default values, enum sanity, struct layout
****************************************************************************/

#include "gtest/gtest.h"

// ── Window tests (wasm stub, no OS/GPU required) ──────────────────────────────
#include "cocos/platform/wasm/modules/SystemWindow.h"
#include "cocos/platform/wasm/modules/SystemWindowManager.h"
#include "cocos/platform/interfaces/modules/ISystemWindow.h"
#include "cocos/platform/interfaces/modules/ISystemWindowManager.h"
#include <emscripten/html5.h>

namespace {

bool getCanvasSizeForTest(int *width, int *height) {
    return emscripten_get_canvas_element_size("#canvas", width, height) == EMSCRIPTEN_RESULT_SUCCESS ||
           emscripten_get_canvas_element_size("canvas", width, height) == EMSCRIPTEN_RESULT_SUCCESS ||
           emscripten_get_canvas_element_size("#GameCanvas", width, height) == EMSCRIPTEN_RESULT_SUCCESS ||
           emscripten_get_canvas_element_size("GameCanvas", width, height) == EMSCRIPTEN_RESULT_SUCCESS;
}

} // namespace

// ── SystemWindow ─────────────────────────────────────────────────────────────

TEST(WasmSystemWindowTest, ConstructWithId) {
    cc::SystemWindow win(1, nullptr);
    EXPECT_EQ(win.getWindowId(), 1u);
}

TEST(WasmSystemWindowTest, HandleNullExternal) {
    cc::SystemWindow win(2, nullptr);
    // No external handle → handle should be 0
    EXPECT_EQ(win.getWindowHandle(), 0u);
}

TEST(WasmSystemWindowTest, HandleExternalPointer) {
    int dummy = 42;
    cc::SystemWindow win(3, &dummy);
    EXPECT_EQ(win.getWindowHandle(), reinterpret_cast<uintptr_t>(&dummy));
}

TEST(WasmSystemWindowTest, InitReturnsZero) {
    cc::SystemWindow win(1, nullptr);
    EXPECT_EQ(win.init(), 0);
}

TEST(WasmSystemWindowTest, ViewSizeMatchesCanvasOrDefaults) {
    cc::SystemWindow win(1, nullptr);
    auto size = win.getViewSize();
    int expectedWidth = 0;
    int expectedHeight = 0;
    getCanvasSizeForTest(&expectedWidth, &expectedHeight);
    EXPECT_FLOAT_EQ(size.width, static_cast<float>(expectedWidth));
    EXPECT_FLOAT_EQ(size.height, static_cast<float>(expectedHeight));
}

TEST(WasmSystemWindowTest, SetCursorEnabledNocrash) {
    cc::SystemWindow win(1, nullptr);
    // stub — must not crash
    win.setCursorEnabled(true);
    win.setCursorEnabled(false);
}

TEST(WasmSystemWindowTest, PollEventNocrash) {
    cc::SystemWindow win(1, nullptr);
    bool quit = false;
    win.pollEvent(&quit);
    EXPECT_FALSE(quit); // stub never sets quit
}

TEST(WasmSystemWindowTest, MainWindowIdConstant) {
    EXPECT_EQ(cc::ISystemWindow::mainWindowId, 1u);
}

// ── SystemWindowManager ───────────────────────────────────────────────────────

TEST(WasmSystemWindowManagerTest, InitReturnsZero) {
    cc::SystemWindowManager mgr;
    EXPECT_EQ(mgr.init(), 0);
}

TEST(WasmSystemWindowManagerTest, InitialWindowsEmpty) {
    cc::SystemWindowManager mgr;
    mgr.init();
    EXPECT_EQ(mgr.getWindows().size(), 1u);
    EXPECT_NE(mgr.getWindow(cc::ISystemWindow::mainWindowId), nullptr);
}

TEST(WasmSystemWindowManagerTest, GetWindowInvalidIdReturnsNull) {
    cc::SystemWindowManager mgr;
    mgr.init();
    EXPECT_EQ(mgr.getWindow(0), nullptr);   // 0 is always invalid
    EXPECT_EQ(mgr.getWindow(99), nullptr);  // non-existent
}

TEST(WasmSystemWindowManagerTest, ProcessEventNocrash) {
    cc::SystemWindowManager mgr;
    mgr.init();
    mgr.processEvent(); // stub — must not crash
}

// ── GFX data structure tests (no GPU required) ───────────────────────────────
#include "cocos/renderer/gfx-base/GFXDef-common.h"

TEST(WasmGfxStructTest, RectDefaults) {
    cc::gfx::Rect r;
    EXPECT_EQ(r.x, 0);
    EXPECT_EQ(r.y, 0);
    EXPECT_EQ(r.width, 0u);
    EXPECT_EQ(r.height, 0u);
}

TEST(WasmGfxStructTest, ViewportDefaults) {
    cc::gfx::Viewport vp;
    EXPECT_EQ(vp.left, 0);
    EXPECT_EQ(vp.top, 0);
    EXPECT_EQ(vp.width, 0u);
    EXPECT_EQ(vp.height, 0u);
    EXPECT_FLOAT_EQ(vp.minDepth, 0.f);
    EXPECT_FLOAT_EQ(vp.maxDepth, 1.f);
}

TEST(WasmGfxStructTest, ColorDefaults) {
    cc::gfx::Color c;
    EXPECT_FLOAT_EQ(c.x, 0.f);
    EXPECT_FLOAT_EQ(c.y, 0.f);
    EXPECT_FLOAT_EQ(c.z, 0.f);
    EXPECT_FLOAT_EQ(c.w, 0.f);
}

TEST(WasmGfxStructTest, ExtentDefaults) {
    cc::gfx::Extent e;
    EXPECT_EQ(e.width, 0u);
    EXPECT_EQ(e.height, 0u);
    EXPECT_EQ(e.depth, 1u); // depth defaults to 1
}

TEST(WasmGfxStructTest, SwapchainInfoDefaults) {
    cc::gfx::SwapchainInfo info;
    EXPECT_EQ(info.windowId, 0u);
    EXPECT_EQ(info.windowHandle, nullptr);
    EXPECT_EQ(info.vsyncMode, cc::gfx::VsyncMode::ON);
    EXPECT_EQ(info.width, 0u);
    EXPECT_EQ(info.height, 0u);
}

TEST(WasmGfxStructTest, TextureInfoDefaults) {
    cc::gfx::TextureInfo info;
    EXPECT_EQ(info.type, cc::gfx::TextureType::TEX2D);
    EXPECT_EQ(info.format, cc::gfx::Format::UNKNOWN);
    EXPECT_EQ(info.width, 0u);
    EXPECT_EQ(info.height, 0u);
    EXPECT_EQ(info.layerCount, 1u);
    EXPECT_EQ(info.levelCount, 1u);
    EXPECT_EQ(info.depth, 1u);
}

TEST(WasmGfxStructTest, SamplerInfoDefaults) {
    cc::gfx::SamplerInfo info;
    EXPECT_EQ(info.minFilter, cc::gfx::Filter::LINEAR);
    EXPECT_EQ(info.magFilter, cc::gfx::Filter::LINEAR);
    EXPECT_EQ(info.mipFilter, cc::gfx::Filter::NONE);
    EXPECT_EQ(info.addressU, cc::gfx::Address::WRAP);
    EXPECT_EQ(info.cmpFunc, cc::gfx::ComparisonFunc::ALWAYS);
}

TEST(WasmGfxStructTest, BufferInfoDefaults) {
    cc::gfx::BufferInfo info;
    EXPECT_EQ(info.usage, cc::gfx::BufferUsageBit::NONE);
    EXPECT_EQ(info.memUsage, cc::gfx::MemoryUsageBit::NONE);
    EXPECT_EQ(info.size, 0u);
    EXPECT_EQ(info.stride, 1u); // stride defaults to 1
}

TEST(WasmGfxStructTest, DrawInfoDefaults) {
    cc::gfx::DrawInfo info;
    EXPECT_EQ(info.vertexCount, 0u);
    EXPECT_EQ(info.firstVertex, 0u);
    EXPECT_EQ(info.indexCount, 0u);
    EXPECT_EQ(info.instanceCount, 0u);
}

TEST(WasmGfxStructTest, DeviceCapsDefaults) {
    cc::gfx::DeviceCaps caps;
    EXPECT_EQ(caps.maxVertexAttributes, 0u);
    EXPECT_EQ(caps.uboOffsetAlignment, 1u);
    EXPECT_FALSE(caps.supportQuery);
    EXPECT_FLOAT_EQ(caps.clipSpaceMinZ, -1.f);
    EXPECT_FLOAT_EQ(caps.screenSpaceSignY, 1.f);
}

// ── GFX enum sanity checks ────────────────────────────────────────────────────

TEST(WasmGfxEnumTest, FormatUnknownIsZero) {
    EXPECT_EQ(static_cast<uint32_t>(cc::gfx::Format::UNKNOWN), 0u);
}

TEST(WasmGfxEnumTest, ApiValues) {
    EXPECT_NE(cc::gfx::API::GLES3, cc::gfx::API::GLES2);
    EXPECT_NE(cc::gfx::API::WEBGL2, cc::gfx::API::WEBGL);
}

TEST(WasmGfxEnumTest, BufferUsageBitwise) {
    using B = cc::gfx::BufferUsageBit;
    auto combined = B::VERTEX | B::INDEX;
    EXPECT_TRUE(static_cast<uint32_t>(combined) & static_cast<uint32_t>(B::VERTEX));
    EXPECT_TRUE(static_cast<uint32_t>(combined) & static_cast<uint32_t>(B::INDEX));
    EXPECT_FALSE(static_cast<uint32_t>(combined) & static_cast<uint32_t>(B::UNIFORM));
}

TEST(WasmGfxEnumTest, ClearFlagsCombination) {
    using C = cc::gfx::ClearFlagBit;
    EXPECT_EQ(C::DEPTH_STENCIL, C::DEPTH | C::STENCIL);
    EXPECT_EQ(C::ALL, C::COLOR | C::DEPTH | C::STENCIL);
}

TEST(WasmGfxEnumTest, TextureUsageBitwise) {
    using T = cc::gfx::TextureUsageBit;
    auto rt = T::COLOR_ATTACHMENT | T::SAMPLED;
    EXPECT_TRUE(static_cast<uint32_t>(rt) & static_cast<uint32_t>(T::COLOR_ATTACHMENT));
    EXPECT_TRUE(static_cast<uint32_t>(rt) & static_cast<uint32_t>(T::SAMPLED));
}

// ── ISystemWindowInfo struct ──────────────────────────────────────────────────

TEST(WasmWindowInfoTest, DefaultValues) {
    cc::ISystemWindowInfo info;
    EXPECT_EQ(info.x, -1);
    EXPECT_EQ(info.y, -1);
    EXPECT_EQ(info.width, -1);
    EXPECT_EQ(info.height, -1);
    EXPECT_EQ(info.flags, -1);
    EXPECT_EQ(info.externalHandle, nullptr);
}

TEST(WasmWindowInfoTest, WindowFlagConstants) {
    // Verify key flag values are distinct powers of two
    EXPECT_EQ(cc::ISystemWindow::WindowFlags::CC_WINDOW_FULLSCREEN, 0x00000001);
    EXPECT_EQ(cc::ISystemWindow::WindowFlags::CC_WINDOW_OPENGL,     0x00000002);
    EXPECT_EQ(cc::ISystemWindow::WindowFlags::CC_WINDOW_SHOWN,      0x00000004);
    EXPECT_EQ(cc::ISystemWindow::WindowFlags::CC_WINDOW_RESIZABLE,  0x00000020);
}
