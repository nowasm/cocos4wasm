#pragma once

#include "base/Ptr.h"
#include "base/std/container/vector.h"
#include "base/std/hash/hash_fwd.hpp"
#include "core/component/Component.h"
#include "renderer/gfx-base/GFXDef-common.h"

namespace cc {

namespace gfx { class Texture; }
class Material;

// Base class for 2D UI drawables (path A).
//
// After B2 the UIRenderer no longer owns a scene::Model / gfx::Buffer /
// RenderingSubMesh. Each activated instance registers with UIBatcher2d,
// which per-frame walks all registered renderers, groups consecutive
// same-key ones, and emits pooled scene::Models with the combined
// geometry. UIRenderer's job is therefore:
//
//   1. Produce vertex/index data in LOCAL space via updateGeometry()
//   2. Declare vertex attributes + stride
//   3. Resolve a Material (subclass implements resolveMaterial())
//   4. Expose a gfx::Texture* so the batcher can key by texture
//   5. Register / unregister with UIBatcher2d on activation
//
// The stencil pass override (for Masks and their content) still lives in
// UIRenderer — wrapped via UIBatcher2d::getStencilMaterial so siblings
// under a common mask share the same wrapped Material instance and end
// up in the same batch.
class UIRenderer : public Component {
    CC_CLASS_DECL(UIRenderer, Component)
public:
    UIRenderer();
    ~UIRenderer() override;

    void onEnable() override;
    void onDisable() override;
    void lateUpdate(float dt) override;

    virtual bool isMask() const { return false; }

    // ─── Accessors consumed by UIBatcher2d ──────────────────────────────
    Material *getMaterial() const { return _material.get(); }
    const ccstd::vector<gfx::Attribute> &getAttributes() const { return _attributes; }
    uint32_t getVertexStrideFloats() const { return _vertexStrideFloats; }
    const ccstd::vector<float>    &getVertexData() const { return _vertexData; }
    const ccstd::vector<uint16_t> &getIndexData()  const { return _indexData; }
    uint32_t getVertexCount() const { return _vertexCount; }
    uint32_t getIndexCount()  const { return _indexCount; }
    gfx::Texture *getBatchTexture() const { return _batchTexture; }
    uint32_t getStencilDepth() const { return _stencilDepth; }
    ccstd::hash_t getBatchKey() const { return _batchKey; }

    bool isRenderable() const { return _material && _vertexCount > 0 && _indexCount > 0; }

protected:
    virtual void updateGeometry() {}
    virtual IntrusivePtr<Material> resolveMaterial();
    virtual ccstd::vector<gfx::Attribute> vertexAttributes() const;
    virtual gfx::Texture *resolveBatchTexture() const { return nullptr; }

    void markDirty() { _dirty = true; }

    // Local-space vertex / index buffers — the batcher reads these each
    // frame, applies the node world matrix to positions, and appends into
    // the group's shared CPU buffer before GPU upload.
    ccstd::vector<float>    _vertexData;
    ccstd::vector<uint16_t> _indexData;
    uint32_t _vertexCount{0};
    uint32_t _indexCount{0};
    uint32_t _vertexStrideFloats{3};

private:
    void rebuildForRender();     // material + attributes + geometry + batch key
    void computeBatchKey();
    void registerSelf();
    void unregisterSelf();

    IntrusivePtr<Material>         _material;
    ccstd::vector<gfx::Attribute>  _attributes;
    gfx::Texture                  *_batchTexture{nullptr};
    uint32_t                       _stencilDepth{0};
    ccstd::hash_t                  _batchKey{0};
    bool                           _dirty{true};
    bool                           _registered{false};
};

}  // namespace cc
