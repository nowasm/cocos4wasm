/****************************************************************************
 Copyright (c) 2024 Xiamen Yaji Software Co., Ltd.

 Integration test: create a window, build a scene with basic elements,
 and verify the scene graph is correctly wired.

 Uses the EmptyDevice (no real GPU) that main.cpp already initialises via
 Root / DeviceManager::create().  All assertions are structural — we verify
 object creation, attachment, and query APIs rather than pixel output.

 Key API notes (from RenderScene.cpp):
 - scene->addCamera(cam)    → calls cam->attachToScene(scene) AND adds to _cameras
 - scene->removeCamera(cam) → calls cam->detachFromScene() AND removes from _cameras
 - scene->removeCameras()   → calls cam->destroy() on each camera, then clears list
 - root->destroyScene(s)    → calls s->destroy() which calls removeCameras() internally
 So: never call camera->destroy() after destroyScene() — it's already done.
****************************************************************************/

#include "gtest/gtest.h"

#include "core/Root.h"
#include "scene/Camera.h"
#include "scene/DirectionalLight.h"
#include "scene/Model.h"
#include "scene/RenderScene.h"
#include "scene/RenderWindow.h"
#include "core/scene-graph/Node.h"
#include "renderer/gfx-base/GFXDef-common.h"

// ── helpers ──────────────────────────────────────────────────────────────────

static cc::scene::RenderWindow *createOffscreenWindow(cc::Root *root,
                                                       uint32_t w, uint32_t h) {
    cc::scene::IRenderWindowInfo info;
    info.title  = "test-window";
    info.width  = w;
    info.height = h;
    info.swapchain = nullptr;
    info.renderPassInfo.colorAttachments.resize(1);
    info.renderPassInfo.colorAttachments[0].format = cc::gfx::Format::RGBA8;
    info.renderPassInfo.depthStencilAttachment.format = cc::gfx::Format::DEPTH_STENCIL;
    return root->createWindow(info);
}

// ── Fixture ───────────────────────────────────────────────────────────────────

class WasmSceneRenderTest : public ::testing::Test {
protected:
    cc::Root *root{nullptr};

    void SetUp() override {
        root = cc::Root::getInstance();
        ASSERT_NE(root, nullptr) << "Root must be initialised by main()";
        ASSERT_NE(root->getDevice(), nullptr) << "GFX device must exist";
    }
};

// ── Window creation ───────────────────────────────────────────────────────────

TEST_F(WasmSceneRenderTest, CreateOffscreenWindow) {
    auto *win = createOffscreenWindow(root, 800, 600);
    ASSERT_NE(win, nullptr);
    EXPECT_EQ(win->getWidth(),  800u);
    EXPECT_EQ(win->getHeight(), 600u);
    EXPECT_NE(win->getFramebuffer(), nullptr);
    root->destroyWindow(win);
}

TEST_F(WasmSceneRenderTest, MultipleWindowsHaveUniqueIds) {
    auto *w1 = createOffscreenWindow(root, 320, 240);
    auto *w2 = createOffscreenWindow(root, 640, 480);
    ASSERT_NE(w1, nullptr);
    ASSERT_NE(w2, nullptr);
    EXPECT_NE(w1->getRenderWindowId(), w2->getRenderWindowId());
    root->destroyWindow(w1);
    root->destroyWindow(w2);
}

// ── Scene creation ────────────────────────────────────────────────────────────

TEST_F(WasmSceneRenderTest, CreateAndDestroyScene) {
    cc::scene::IRenderSceneInfo info;
    info.name = "test-scene";
    auto *scene = root->createScene(info);
    ASSERT_NE(scene, nullptr);
    EXPECT_EQ(scene->getName(), "test-scene");
    EXPECT_TRUE(scene->getModels().empty());
    EXPECT_TRUE(scene->getCameras().empty());
    root->destroyScene(scene);
}

// ── Camera ────────────────────────────────────────────────────────────────────

// Step 1: verify camera can be created and initialized without adding to scene
TEST_F(WasmSceneRenderTest, CreateCameraInitialize) {
    auto *win = createOffscreenWindow(root, 800, 600);
    ASSERT_NE(win, nullptr);

    auto *node = ccnew cc::Node("camera-node");
    node->addRef();

    auto *camera = root->createCamera();
    ASSERT_NE(camera, nullptr);
    camera->addRef();

    cc::scene::ICameraInfo camInfo;
    camInfo.name       = "main-camera";
    camInfo.node       = node;
    camInfo.projection = cc::scene::CameraProjection::PERSPECTIVE;
    camInfo.window     = win;
    camInfo.priority   = 0;
    camInfo.usage      = cc::scene::CameraUsage::GAME;
    EXPECT_TRUE(camera->initialize(camInfo));

    camera->setFov(cc::mathutils::toRadian(60.f));
    EXPECT_NEAR(camera->getFov(), cc::mathutils::toRadian(60.f), 1e-4f);
    camera->setNearClip(0.1f);
    camera->setFarClip(1000.f);
    EXPECT_FLOAT_EQ(camera->getNearClip(), 0.1f);
    EXPECT_FLOAT_EQ(camera->getFarClip(), 1000.f);

    camera->destroy();
    camera->release();
    root->destroyWindow(win);
    node->release();
}

// Step 2: verify camera can be added to and removed from scene
TEST_F(WasmSceneRenderTest, CreateCameraAndAddToScene) {
    auto *win = createOffscreenWindow(root, 800, 600);
    ASSERT_NE(win, nullptr);

    cc::scene::IRenderSceneInfo sceneInfo;
    sceneInfo.name = "camera-scene";
    auto *scene = root->createScene(sceneInfo);
    ASSERT_NE(scene, nullptr);

    auto *node = ccnew cc::Node("camera-node");
    node->addRef();

    auto *camera = root->createCamera();
    ASSERT_NE(camera, nullptr);
    camera->addRef();

    cc::scene::ICameraInfo camInfo;
    camInfo.name       = "main-camera";
    camInfo.node       = node;
    camInfo.projection = cc::scene::CameraProjection::PERSPECTIVE;
    camInfo.window     = win;
    camInfo.priority   = 0;
    camInfo.usage      = cc::scene::CameraUsage::GAME;
    EXPECT_TRUE(camera->initialize(camInfo));

    scene->addCamera(camera);
    EXPECT_EQ(scene->getCameras().size(), 1u);

    scene->removeCamera(camera);
    EXPECT_TRUE(scene->getCameras().empty());

    camera->destroy();
    camera->release();
    root->destroyScene(scene);
    root->destroyWindow(win);
    node->release();
}

// ── Model ─────────────────────────────────────────────────────────────────────

TEST_F(WasmSceneRenderTest, AddAndRemoveModelFromScene) {
    cc::scene::IRenderSceneInfo sceneInfo;
    sceneInfo.name = "model-scene";
    auto *scene = root->createScene(sceneInfo);
    ASSERT_NE(scene, nullptr);

    auto *model = root->createModel<cc::scene::Model>();
    ASSERT_NE(model, nullptr);
    EXPECT_TRUE(model->isInited());

    scene->addModel(model);
    EXPECT_EQ(scene->getModels().size(), 1u);
    EXPECT_EQ(scene->getModels()[0].get(), model);

    scene->removeModel(model);
    EXPECT_TRUE(scene->getModels().empty());

    root->destroyModel(model);
    root->destroyScene(scene);
}

TEST_F(WasmSceneRenderTest, MultipleModelsInScene) {
    cc::scene::IRenderSceneInfo sceneInfo;
    sceneInfo.name = "multi-model-scene";
    auto *scene = root->createScene(sceneInfo);
    ASSERT_NE(scene, nullptr);

    constexpr int kCount = 5;
    ccstd::vector<cc::scene::Model *> models;
    for (int i = 0; i < kCount; ++i) {
        auto *m = root->createModel<cc::scene::Model>();
        ASSERT_NE(m, nullptr);
        scene->addModel(m);
        models.push_back(m);
    }
    EXPECT_EQ(scene->getModels().size(), static_cast<size_t>(kCount));

    // removeModels() clears the list but does NOT destroy the models
    scene->removeModels();
    EXPECT_TRUE(scene->getModels().empty());

    for (auto *m : models) root->destroyModel(m);
    root->destroyScene(scene);
}

// ── Directional light ─────────────────────────────────────────────────────────

TEST_F(WasmSceneRenderTest, AddDirectionalLightToScene) {
    cc::scene::IRenderSceneInfo sceneInfo;
    sceneInfo.name = "light-scene";
    auto *scene = root->createScene(sceneInfo);
    ASSERT_NE(scene, nullptr);

    auto *node  = ccnew cc::Node("dir-light-node");
    auto *light = root->createLight<cc::scene::DirectionalLight>();
    ASSERT_NE(light, nullptr);

    light->setNode(node);
    scene->addDirectionalLight(light);
    scene->setMainLight(light);
    EXPECT_EQ(scene->getMainLight(), light);

    scene->unsetMainLight(light);
    EXPECT_EQ(scene->getMainLight(), nullptr);

    scene->removeDirectionalLight(light);
    root->destroyLight(light);
    root->destroyScene(scene);
    CC_SAFE_RELEASE(node);
}

// ── Full mini-scene: window + camera + model + light ─────────────────────────

TEST_F(WasmSceneRenderTest, FullMiniScene) {
    // 1. Off-screen window (1280×720)
    auto *win = createOffscreenWindow(root, 1280, 720);
    ASSERT_NE(win, nullptr);

    // 2. Scene
    cc::scene::IRenderSceneInfo sceneInfo;
    sceneInfo.name = "mini-scene";
    auto *scene = root->createScene(sceneInfo);
    ASSERT_NE(scene, nullptr);

    // 3. Camera
    auto *camNode = ccnew cc::Node("cam");
    camNode->addRef();
    auto *camera  = root->createCamera();
    ASSERT_NE(camera, nullptr);
    camera->addRef(); // keep alive after removeCamera drops scene's IntrusivePtr
    {
        cc::scene::ICameraInfo camInfo;
        camInfo.name       = "cam";
        camInfo.node       = camNode;
        camInfo.projection = cc::scene::CameraProjection::PERSPECTIVE;
        camInfo.window     = win;
        camInfo.priority   = 0;
        camInfo.usage      = cc::scene::CameraUsage::GAME;
        EXPECT_TRUE(camera->initialize(camInfo));
    }
    scene->addCamera(camera);  // correct API

    // 4. Directional light (sun)
    auto *lightNode = ccnew cc::Node("sun");
    lightNode->addRef();
    auto *dirLight  = root->createLight<cc::scene::DirectionalLight>();
    ASSERT_NE(dirLight, nullptr);
    dirLight->setNode(lightNode);
    scene->addDirectionalLight(dirLight);
    scene->setMainLight(dirLight);

    // 5. Three models (cube / sphere / plane — structural only, no mesh data)
    auto *cube   = root->createModel<cc::scene::Model>();
    auto *sphere = root->createModel<cc::scene::Model>();
    auto *plane  = root->createModel<cc::scene::Model>();
    scene->addModel(cube);
    scene->addModel(sphere);
    scene->addModel(plane);

    // ── Assertions ────────────────────────────────────────────────────────────
    EXPECT_EQ(scene->getCameras().size(), 1u);
    EXPECT_EQ(scene->getModels().size(),  3u);
    EXPECT_EQ(scene->getMainLight(),      dirLight);
    EXPECT_EQ(win->getWidth(),  1280u);
    EXPECT_EQ(win->getHeight(),  720u);

    // ── Teardown (order matters) ───────────────────────────────────────────────
    // Remove camera before destroyScene, then destroy + release our extra ref
    scene->removeCamera(camera);
    camera->destroy();
    camera->release();
    // Remove models before destroyScene
    scene->removeModels();
    // Remove light before destroyScene
    scene->unsetMainLight(dirLight);
    scene->removeDirectionalLight(dirLight);

    root->destroyModel(cube);
    root->destroyModel(sphere);
    root->destroyModel(plane);
    root->destroyLight(dirLight);
    root->destroyScene(scene);
    root->destroyWindow(win);
    camNode->release();
    lightNode->release();
}
