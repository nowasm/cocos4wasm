#pragma once

#include "base/Ptr.h"
#include "base/std/container/unordered_map.h"
#include "base/std/container/vector.h"
#include "base/std/hash/hash_fwd.hpp"
#include "renderer/gfx-base/GFXDef-common.h"

namespace cc {

class UIRenderer;
class Material;
class RenderingSubMesh;
namespace gfx { class Buffer; class Texture; }
namespace scene { class Model; class RenderScene; }

// Lightweight 2D batcher used by path A's UIRenderers. Every activated
// UIRenderer registers here; each engine tick the batcher walks the
// registered list (insertion / DFS order), groups consecutive renderers
// that share a batch key, accumulates their world-transformed vertices
// into a pooled scene::Model, and submits the minimum number of draws
// to the main RenderScene.
//
// This file lives beside the dormant legacy cocos/2d/renderer/Batcher2d
// but is a completely independent implementation — none of the JS-era
// shared-memory / RenderEntity / UIMeshBuffer scaffolding is reused.
class UIBatcher2d {
public:
    static UIBatcher2d &get();

    // Called from UIRenderer::onEnable / onDisable.
    void registerRenderer(UIRenderer *r);
    void unregisterRenderer(UIRenderer *r);

    // Called once per engine tick, after component lateUpdate. Rebuilds all
    // batches from scratch.
    void tick();

    // Returns a material wrapper carrying the appropriate stencil pass
    // override for the given base material + mask context. Cached so that
    // same (base, stencilDepth, isMaskShape) tuples share one Material
    // pointer, which is what makes sibling UIRenderers under a shared
    // mask batch together.
    IntrusivePtr<Material> getStencilMaterial(Material *base,
                                              uint32_t stencilDepth,
                                              bool isMaskShape);

    // Telemetry — number of batches emitted last tick. Useful for
    // verifying batching really happened.
    size_t getLastBatchCount() const { return _lastBatchCount; }

private:
    UIBatcher2d() = default;
    ~UIBatcher2d() = default;
    UIBatcher2d(const UIBatcher2d &) = delete;
    UIBatcher2d &operator=(const UIBatcher2d &) = delete;

    struct Batch {
        // Key stable across frames — used to find an existing batch slot
        // when the same group of renderers produces identical output.
        ccstd::hash_t key{0};
        Material     *material{nullptr};
        gfx::Texture *texture{nullptr};

        ccstd::vector<gfx::Attribute> attributes;
        uint32_t vertexStrideFloats{0};

        // CPU-side accumulation, rewritten each frame.
        ccstd::vector<float>    vertexData;
        ccstd::vector<uint16_t> indexData;
        uint32_t vertexCount{0};

        // GPU resources — allocated on first flush, reused per frame,
        // rebuilt only when capacity exceeded.
        IntrusivePtr<gfx::Buffer>      vb;
        IntrusivePtr<gfx::Buffer>      ib;
        IntrusivePtr<RenderingSubMesh> subMesh;
        IntrusivePtr<scene::Model>     model;
        size_t vbCapacityBytes{0};
        size_t ibCapacityBytes{0};

        bool attachedToScene{false};
    };

    Batch &findOrAppendBatch(ccstd::hash_t key, UIRenderer *sample);
    void   uploadAndSubmit(Batch &b, scene::RenderScene *scene);
    void   detachBatch(Batch &b, scene::RenderScene *scene);

    // (baseMaterialPtr, stencilDepth<<1 | isMaskShape) → wrapped material.
    struct StencilCacheKey {
        Material *base;
        uint64_t  stateBits;  // (depth << 1) | isMaskShape
        bool operator==(const StencilCacheKey &o) const noexcept {
            return base == o.base && stateBits == o.stateBits;
        }
    };
    struct StencilCacheKeyHash {
        size_t operator()(const StencilCacheKey &k) const noexcept;
    };

    ccstd::vector<UIRenderer *> _registered;
    ccstd::vector<Batch>        _batches;
    size_t                      _lastBatchCount{0};
    ccstd::unordered_map<StencilCacheKey, IntrusivePtr<Material>, StencilCacheKeyHash>
        _stencilCache;
};

}  // namespace cc
