#pragma once

#include "base/Ptr.h"
#include "base/std/container/vector.h"
#include "core/component/Component.h"
#include "renderer/gfx-base/GFXDef-common.h"

namespace cc {

namespace scene {
class Model;
class RenderScene;
}  // namespace scene

class Material;
class RenderingSubMesh;

namespace gfx { class Buffer; }

// Base class for 2D UI drawables.
//
// Path A (per docs/memory/project_p2_rendering_path.md): each UIRenderer
// instance owns one cc::scene::Model and attaches it directly to the main
// RenderScene. Subclass responsibility:
//
//   - updateGeometry():  fill _vertexData (interleaved floats) and
//                        _indexData (uint16); set _vertexCount/_indexCount;
//                        set _vertexStrideFloats once before first upload.
//   - vertexAttributes(): the gfx::Attribute list describing the layout —
//                        default is position-only (3 floats).
//   - resolveMaterial(): return an IntrusivePtr<Material>. Usually built
//                        via MaterialFactory or loaded from BuiltinResMgr.
//
// The base handles buffer creation, model attach, and per-frame upload on
// markDirty(). No batching; that's deliberate for this phase.
class UIRenderer : public Component {
    CC_CLASS_DECL(UIRenderer, Component)
public:
    UIRenderer();
    ~UIRenderer() override;

    void onEnable() override;
    void onDisable() override;
    void lateUpdate(float dt) override;

protected:
    // Empty defaults so the class stays concrete (reflection factory needs
    // default constructibility). Subclasses override with real geometry and
    // shader selection — a bare UIRenderer silently no-ops.
    virtual void updateGeometry() {}
    virtual IntrusivePtr<Material> resolveMaterial() { return nullptr; }
    virtual ccstd::vector<gfx::Attribute> vertexAttributes() const;

    void markDirty() { _dirty = true; }

    ccstd::vector<float>    _vertexData;   // interleaved floats
    ccstd::vector<uint16_t> _indexData;
    uint32_t _vertexCount{0};
    uint32_t _indexCount{0};
    uint32_t _vertexStrideFloats{3};       // default position-only

private:
    void ensureModel();
    void destroyModel();
    void uploadBuffers();

    IntrusivePtr<scene::Model>     _model;
    IntrusivePtr<Material>         _material;
    IntrusivePtr<gfx::Buffer>      _vb;
    IntrusivePtr<gfx::Buffer>      _ib;
    IntrusivePtr<RenderingSubMesh> _subMesh;
    scene::RenderScene            *_renderScene{nullptr};

    bool   _dirty{true};
    size_t _vbCapacityBytes{0};
    size_t _ibCapacityBytes{0};
};

}  // namespace cc
