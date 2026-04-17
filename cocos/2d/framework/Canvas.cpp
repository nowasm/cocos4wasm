#include "cocos/2d/framework/Canvas.h"

#include "base/Log.h"
#include "core/Root.h"
#include "core/scene-graph/Node.h"
#include "scene/Camera.h"
#include "scene/RenderScene.h"

namespace cc {

CC_IMPLEMENT_CLASS(Canvas, "cc.Canvas", Component)
    .property("clearColor", &Canvas::_clearColor)
    .property("priority",   &Canvas::_priority)
CC_END_CLASS(Canvas);

Canvas::Canvas() = default;
Canvas::~Canvas() = default;

void Canvas::onEnable() {
    createCamera();
}

void Canvas::onDisable() {
    destroyCamera();
}

void Canvas::setClearColor(const Color &c) {
    _clearColor = c;
    if (_camera) applyClearColor();
}

void Canvas::setPriority(int32_t v) {
    _priority = v;
    if (_camera) _camera->setPriority(static_cast<uint32_t>(v));
}

void Canvas::applyClearColor() {
    _camera->setClearColor(gfx::Color{
        _clearColor.r / 255.0f,
        _clearColor.g / 255.0f,
        _clearColor.b / 255.0f,
        _clearColor.a / 255.0f,
    });
}

void Canvas::createCamera() {
    if (_camera) return;
    auto *root = Root::getInstance();
    if (!root) {
        CC_LOG_ERROR("[Canvas] Root not available at onEnable");
        return;
    }

    const auto &scenes = root->getScenes();
    if (scenes.empty()) {
        CC_LOG_ERROR("[Canvas] no RenderScene registered on Root");
        return;
    }
    _renderScene = scenes[0].get();

    _camera = root->createCamera();

    // DemoGame window is 1280x720. Ortho height in Cocos is the half-
    // height (distance from centre to top), so 360 gives pixel-perfect
    // 1:1 UI units. Real screen-fit policies land in a later phase.
    const float orthoHeight = 360.0f;

    scene::ICameraInfo ci;
    ci.name = "Canvas_Cam";
    ci.node = getNode();
    ci.projection = scene::CameraProjection::ORTHO;
    ci.window = root->getMainWindow();
    ci.priority = static_cast<uint32_t>(_priority);
    _camera->initialize(ci);

    _camera->setOrthoHeight(orthoHeight);
    _camera->setNearClip(-1000.0f);
    _camera->setFarClip(1000.0f);
    _camera->setVisibility(0xFFFFFFFF);
    _camera->setClearFlag(gfx::ClearFlagBit::ALL);
    applyClearColor();

    _renderScene->addCamera(_camera);

    CC_LOG_INFO("[Canvas] camera created (priority=%d, orthoH=%.1f)",
                _priority, orthoHeight);
}

void Canvas::destroyCamera() {
    if (!_camera) return;
    if (_renderScene) {
        _renderScene->removeCamera(_camera);
    }
    // Camera is owned by Root; removing from scene detaches it. Don't delete.
    _camera = nullptr;
    _renderScene = nullptr;
}

}  // namespace cc
