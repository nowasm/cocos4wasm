#pragma once

#include "core/component/Component.h"
#include "math/Color.h"
#include "renderer/gfx-base/GFXDef-common.h"

namespace cc {

namespace scene {
class Camera;
class RenderScene;
}  // namespace scene

// Turns its owning node into the root of a 2D UI subtree.
//
// On activation the Canvas creates an orthographic camera attached to its
// node and adds it to the engine's main RenderScene, with high priority so
// it composites after any 3D content. Deactivation tears the camera back
// down.
//
// Scope (P2a): camera creation + clear flag/color control. No screen-fit
// policy yet (ortho height == system window height, 1:1 pixel). Multiple
// Canvases can coexist; priorities determine stacking order.
class Canvas : public Component {
    CC_CLASS_DECL(Canvas, Component)
public:
    Canvas();
    ~Canvas() override;

    void onEnable() override;
    void onDisable() override;

    const Color &getClearColor() const { return _clearColor; }
    void setClearColor(const Color &c);

    int32_t getPriority() const { return _priority; }
    void setPriority(int32_t v);

    scene::Camera *getCamera() const { return _camera; }

private:
    void createCamera();
    void destroyCamera();
    void applyClearColor();

    // Matches Editor default — black clears let UI authoring
    // previews look the same between Cocos Creator and our runtime.
    Color   _clearColor{0, 0, 0, 255};
    int32_t _priority{1073741823};  // INT_MAX/2 — UI composites over 3D

    scene::Camera      *_camera{nullptr};
    scene::RenderScene *_renderScene{nullptr};
};

}  // namespace cc
