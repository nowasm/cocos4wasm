#include "cocos/2d/renderer/UIBatcher2d.h"

#include <algorithm>

#include "base/Log.h"
#include "base/std/container/unordered_map.h"
#include "cocos/2d/framework/UIRenderer.h"
#include "core/Root.h"
#include "core/assets/Material.h"
#include "core/assets/RenderingSubMesh.h"
#include "core/scene-graph/Node.h"
#include "renderer/core/MaterialInstance.h"
#include "renderer/gfx-base/GFXBuffer.h"
#include "renderer/gfx-base/GFXDevice.h"
#include "scene/Model.h"
#include "scene/RenderScene.h"

#include <cstdio>

namespace cc {

// Declared here at namespace scope so linker finds cc::uiRendererWrap...
// (defined in UIRenderer.cpp). A function-body extern would have created
// a global-namespace declaration and caused a link miss.
IntrusivePtr<Material> uiRendererWrapMaterialUncached(IntrusivePtr<Material> base,
                                                     uint32_t depth, bool isMaskShape);

size_t UIBatcher2d::StencilCacheKeyHash::operator()(const StencilCacheKey &k) const noexcept {
    return std::hash<void *>()(k.base) ^ (k.stateBits * 0x9e3779b97f4a7c15ull);
}

UIBatcher2d &UIBatcher2d::get() {
    static UIBatcher2d instance;
    return instance;
}

void UIBatcher2d::registerRenderer(UIRenderer *r) {
    if (!r) return;
    if (std::find(_registered.begin(), _registered.end(), r) != _registered.end()) return;
    _registered.push_back(r);
}

void UIBatcher2d::unregisterRenderer(UIRenderer *r) {
    _registered.erase(std::remove(_registered.begin(), _registered.end(), r),
                      _registered.end());
}

IntrusivePtr<Material> UIBatcher2d::getStencilMaterial(Material *base,
                                                      uint32_t stencilDepth,
                                                      bool isMaskShape) {
    // stencilDepth 0 means plain content outside any mask — just return
    // the base material unchanged.
    if (stencilDepth == 0) return IntrusivePtr<Material>(base);
    if (!base) return nullptr;

    StencilCacheKey key{base,
                        (static_cast<uint64_t>(stencilDepth) << 1) |
                            (isMaskShape ? 1ull : 0ull)};
    auto it = _stencilCache.find(key);
    if (it != _stencilCache.end()) return it->second;

    // Actual stencil wrap lives in UIRenderer.cpp (reuses the DSS builder
    // + MaterialInstance override).
    extern IntrusivePtr<Material> uiRendererWrapMaterialUncached(
        IntrusivePtr<Material> base, uint32_t depth, bool isMaskShape);
    IntrusivePtr<Material> wrapped =
        uiRendererWrapMaterialUncached(IntrusivePtr<Material>(base),
                                       stencilDepth, isMaskShape);
    _stencilCache[key] = wrapped;
    return wrapped;
}

namespace {

// Copy vertex data from a local-space renderer into a batch CPU buffer,
// transforming ONLY the first three floats (position) by the node's
// world matrix. UV / color / other interleaved floats pass through.
void appendTransformed(const float *src, uint32_t vertCount, uint32_t stride,
                       const Mat4 &worldMat,
                       ccstd::vector<float> &dst) {
    dst.reserve(dst.size() + static_cast<size_t>(vertCount) * stride);
    const float *m = worldMat.m;  // column-major 4x4
    for (uint32_t v = 0; v < vertCount; ++v) {
        const float *sp = src + static_cast<size_t>(v) * stride;
        const float x = sp[0], y = sp[1], z = sp[2];
        dst.push_back(m[0] * x + m[4] * y + m[8]  * z + m[12]);
        dst.push_back(m[1] * x + m[5] * y + m[9]  * z + m[13]);
        dst.push_back(m[2] * x + m[6] * y + m[10] * z + m[14]);
        for (uint32_t i = 3; i < stride; ++i) {
            dst.push_back(sp[i]);
        }
    }
}

}  // namespace

UIBatcher2d::Batch &UIBatcher2d::findOrAppendBatch(ccstd::hash_t key, UIRenderer *sample) {
    // Reuse an existing batch only if it hasn't already closed out a
    // sub-batch run this frame. Once a batch has been written to and
    // then interrupted by another key, a later same-key renderer must
    // go into a NEW batch positioned AFTER the interrupter — otherwise
    // scene-graph draw order is violated (opaque sibling sprites end
    // up painting over earlier labels even though the label came first
    // in DFS).
    for (auto &b : _batches) {
        if (b.key == key && !b.usedThisFrame) return b;
    }
    _batches.emplace_back();
    Batch &nb = _batches.back();
    nb.key                = key;
    nb.material           = sample->getMaterial();
    nb.texture            = sample->getBatchTexture();
    nb.attributes         = sample->getAttributes();
    nb.vertexStrideFloats = sample->getVertexStrideFloats();
    return nb;
}

void UIBatcher2d::uploadAndSubmit(Batch &b, scene::RenderScene *scene) {
    if (b.vertexCount == 0 || b.indexData.empty() || !b.material) {
        // Nothing to draw for this batch this frame — detach if we had a
        // model from a previous frame so the pipeline doesn't render stale
        // geometry.
        if (b.attachedToScene && b.model && scene) {
            scene->removeModel(b.model);
            b.attachedToScene = false;
        }
        return;
    }

    auto *device = Root::getInstance()->getDevice();
    const uint32_t strideBytes = b.vertexStrideFloats * sizeof(float);
    const size_t vbBytes = b.vertexData.size() * sizeof(float);
    const size_t ibBytes = b.indexData.size() * sizeof(uint16_t);

    // Re-allocate buffers on first use or when geometry grows past capacity.
    const bool needRebuild =
        !b.vb || !b.ib || !b.subMesh || !b.model ||
        vbBytes > b.vbCapacityBytes ||
        ibBytes > b.ibCapacityBytes;

    if (needRebuild) {
        if (b.attachedToScene && b.model && scene) {
            scene->removeModel(b.model);
            b.attachedToScene = false;
        }

        gfx::BufferInfo vbi;
        vbi.usage    = gfx::BufferUsageBit::VERTEX | gfx::BufferUsageBit::TRANSFER_DST;
        vbi.memUsage = gfx::MemoryUsageBit::DEVICE;
        vbi.size     = static_cast<uint32_t>(vbBytes);
        vbi.stride   = strideBytes;
        b.vb = device->createBuffer(vbi);
        b.vbCapacityBytes = vbBytes;

        gfx::BufferInfo ibi;
        ibi.usage    = gfx::BufferUsageBit::INDEX | gfx::BufferUsageBit::TRANSFER_DST;
        ibi.memUsage = gfx::MemoryUsageBit::DEVICE;
        ibi.size     = static_cast<uint32_t>(ibBytes);
        ibi.stride   = sizeof(uint16_t);
        b.ib = device->createBuffer(ibi);
        b.ibCapacityBytes = ibBytes;

        gfx::BufferList vbs = {b.vb.get()};
        b.subMesh = ccnew RenderingSubMesh(vbs, b.attributes,
                                           gfx::PrimitiveMode::TRIANGLE_LIST,
                                           b.ib.get());

        b.model = Root::getInstance()->createModel<scene::Model>();
        b.model->initialize();
        // Use the scene node's transform — keeping identity works because
        // vertices are already in world space after appendTransformed.
        static IntrusivePtr<Node> transformAnchor;
        if (!transformAnchor) {
            transformAnchor = ccnew Node("UIBatcherAnchor");
        }
        b.model->setNode(transformAnchor);
        b.model->setTransform(transformAnchor);
        b.model->initSubModel(0, b.subMesh, b.material);
        b.model->setEnabled(true);
    } else {
        // In-place upload — buffer capacity still fits this frame's data.
        b.vb->update(b.vertexData.data(), static_cast<uint32_t>(vbBytes));
        b.ib->update(b.indexData.data(), static_cast<uint32_t>(ibBytes));
    }

    if (needRebuild) {
        b.vb->update(b.vertexData.data(), static_cast<uint32_t>(vbBytes));
        b.ib->update(b.indexData.data(), static_cast<uint32_t>(ibBytes));
    }

    if (!b.attachedToScene && scene) {
        scene->addModel(b.model);
        b.attachedToScene = true;
    }
}

void UIBatcher2d::detachBatch(Batch &b, scene::RenderScene *scene) {
    if (b.attachedToScene && b.model && scene) {
        scene->removeModel(b.model);
        b.attachedToScene = false;
    }
}

void UIBatcher2d::tick() {
    auto *root = Root::getInstance();
    if (!root) return;
    const auto &scenes = root->getScenes();
    scene::RenderScene *scene = scenes.empty() ? nullptr : scenes[0].get();

    // one-shot diagnostic: print on first tick where any renderer is registered.
    static bool logged_once = false;
    if (!logged_once && !_registered.empty()) {
        logged_once = true;
        size_t rc = 0, fc = 0;
        for (auto *r : _registered) {
            if (!r) { ++fc; continue; }
            if (!r->isRenderable()) { ++fc; continue; }
            ++rc;
        }
        CC_LOG_INFO("[UIBatcher2d FIRST-TICK] _registered=%zu renderable=%zu skipped=%zu",
                    _registered.size(), rc, fc);
        std::fflush(stdout); std::fflush(stderr);
    }

    // Mark all batches as "not touched this frame" by clearing their CPU
    // buffers and the usedThisFrame flag. GPU resources live in the
    // pool; we only rebuild when sizes exceed capacity.
    for (auto &b : _batches) {
        b.vertexData.clear();
        b.indexData.clear();
        b.vertexCount = 0;
        b.usedThisFrame = false;
    }

    // Walk registered UIRenderers in insertion (= scene-graph DFS) order,
    // group consecutive same-batch-key into one batch. A batch becomes
    // "closed" the moment we switch to a different key; subsequent
    // renderers with that same original key must go into a fresh batch
    // appended after the interrupter, not folded back into the closed
    // one — that's what keeps draw order matching scene-graph order.
    Batch *current = nullptr;
    for (auto *r : _registered) {
        if (!r || !r->isRenderable()) continue;

        const ccstd::hash_t key = r->getBatchKey();
        if (!current || current->key != key) {
            current = &findOrAppendBatch(key, r);
        }
        current->usedThisFrame = true;

        const Mat4 &worldMat = r->getNode()->getWorldMatrix();
        const uint32_t stride = r->getVertexStrideFloats();
        const uint32_t vbase  = current->vertexCount;

        appendTransformed(r->getVertexData().data(),
                          r->getVertexCount(), stride,
                          worldMat, current->vertexData);
        current->vertexCount += r->getVertexCount();

        // Offset indices by the pre-append base vertex count.
        current->indexData.reserve(current->indexData.size() + r->getIndexCount());
        for (uint16_t idx : r->getIndexData()) {
            current->indexData.push_back(static_cast<uint16_t>(idx + vbase));
        }
    }

    size_t activeBatches = 0;
    size_t renderableCount = 0;
    for (auto *r : _registered) if (r && r->isRenderable()) ++renderableCount;
    for (auto &b : _batches) {
        uploadAndSubmit(b, scene);
        if (b.vertexCount > 0) ++activeBatches;
    }

    // Diagnostic: log when the count changes so we can observe batching
    // efficiency without spamming every frame.
    if (activeBatches != _lastBatchCount || renderableCount != _lastRenderableCount) {
        CC_LOG_INFO("[UIBatcher2d] renderers=%zu renderable=%zu -> batches=%zu",
                    _registered.size(), renderableCount, activeBatches);
        _lastRenderableCount = renderableCount;
        std::fflush(stdout); std::fflush(stderr);
    }
    _lastBatchCount = activeBatches;
}

}  // namespace cc
