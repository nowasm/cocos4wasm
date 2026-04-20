/****************************************************************************
 Stub port of cocos-engine/cocos/misc/camera-component.ts

 The authoring-layer `cc.Camera` component — distinct from the render-
 scene primitive `cc::scene::Camera`. For pure 2D UI scenes, Canvas
 owns its own render camera, so this component is primarily load-time
 metadata. This stub registers the class with reflection so Editor-
 exported scenes that include a Camera node deserialize cleanly, but
 the fields are informational only — no render-side wiring yet. Full
 implementation lands with P6 (3D minimum).
****************************************************************************/

#pragma once

#include "core/component/Component.h"
#include "core/reflection/Reflection.h"
#include "math/Color.h"
#include "math/Vec4.h"

namespace cc {

class CameraComponent : public Component {
    CC_CLASS_DECL(CameraComponent, Component)
public:
    CameraComponent() = default;
    ~CameraComponent() override = default;

    // Field-style accessors so game code can still read back what the
    // Editor authored. Setters do nothing beyond the field write until
    // P6 wires this to a live render-camera.
    int32_t getPriority() const { return _priority; }
    void    setPriority(int32_t v) { _priority = v; }

    const Color &getClearColor() const { return _color; }
    void         setClearColor(const Color &c) { _color = c; }

    float getOrthoHeight() const { return _orthoHeight; }
    void  setOrthoHeight(float v) { _orthoHeight = v; }

    float getNear() const { return _near; }
    float getFar()  const { return _far; }
    float getFov()  const { return _fov; }

    // Pull this translation unit into the link when CameraComponent is
    // never referenced by symbol — CC_END_CLASS's static initializer
    // alone isn't enough to keep the TU alive under MSVC's static-lib
    // dead-code strip. DemoGame calls forceLinkCameraComponent() at
    // startup; any non-empty body works.
    static int forceLink();

private:
    int32_t _projection{0};      // 0 = PERSPECTIVE (upstream ProjectionType)
    int32_t _priority{0};
    float   _fov{45.f};
    int32_t _fovAxis{0};         // 0 = VERTICAL
    float   _orthoHeight{10.f};
    float   _near{1.f};
    float   _far{1000.f};
    Color   _color{51, 51, 51, 255};   // matches upstream '#333333'
    float   _depth{1.f};
    int32_t _stencil{0};
    int32_t _clearFlags{7};      // SOLID_COLOR | DEPTH | STENCIL
    Vec4    _rect{0.f, 0.f, 1.f, 1.f};
    int32_t _aperture{0};
    int32_t _shutter{0};
    int32_t _iso{0};
    float   _screenScale{1.f};
    int32_t _visibility{0};
};

}  // namespace cc
