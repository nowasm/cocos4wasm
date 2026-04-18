#include "cocos/2d/framework/UIRenderer.h"

#include "base/Log.h"
#include "cocos/2d/renderer/UIBatcher2d.h"
#include "core/assets/EffectAsset.h"
#include "core/assets/Material.h"
#include "core/scene-graph/Node.h"
#include "renderer/core/MaterialInstance.h"
#include "renderer/gfx-base/GFXTexture.h"
#include "renderer/pipeline/Define.h"

namespace cc {

namespace {

// Walks the parent chain counting Mask ancestors (via the UIRenderer
// isMask() virtual, so Mask.h stays off this translation unit).
int countMaskAncestors(Node *startNode) {
    if (!startNode) return 0;
    int count = 0;
    const auto *uirMeta = UIRenderer::getStaticClass();
    Node *p = startNode->getParent();
    while (p) {
        for (const auto &c : p->getComponentList()) {
            if (!c) continue;
            if (c->getClass()->isDerivedFrom(uirMeta)) {
                auto *uir = static_cast<UIRenderer *>(c.get());
                if (uir->isMask()) {
                    ++count;
                    break;  // one Mask per node at most
                }
            }
        }
        p = p->getParent();
    }
    return count;
}

// Same bit-per-level model as P2f: bit (depth-1) for mask level N, content
// tests `stencil == (1<<depth)-1` for all lower bits set.
DepthStencilStateInfo buildStencilDssInfo(uint32_t depth, bool isMaskShape) {
    DepthStencilStateInfo info;
    info.depthTest  = false;
    info.depthWrite = false;
    if (depth == 0) return info;

    const uint32_t writeBit  = (1u << (depth - 1));
    const uint32_t stencilRef = (1u << depth) - 1u;

    info.stencilTestFront = true;
    info.stencilTestBack  = true;

    if (isMaskShape) {
        info.stencilFuncFront      = gfx::ComparisonFunc::NEVER;
        info.stencilFuncBack       = gfx::ComparisonFunc::NEVER;
        info.stencilReadMaskFront  = writeBit;
        info.stencilReadMaskBack   = writeBit;
        info.stencilWriteMaskFront = writeBit;
        info.stencilWriteMaskBack  = writeBit;
        info.stencilRefFront       = writeBit;
        info.stencilRefBack        = writeBit;
        info.stencilFailOpFront    = gfx::StencilOp::REPLACE;
        info.stencilFailOpBack     = gfx::StencilOp::REPLACE;
        info.stencilZFailOpFront   = gfx::StencilOp::REPLACE;
        info.stencilZFailOpBack    = gfx::StencilOp::REPLACE;
        info.stencilPassOpFront    = gfx::StencilOp::KEEP;
        info.stencilPassOpBack     = gfx::StencilOp::KEEP;
    } else {
        info.stencilFuncFront      = gfx::ComparisonFunc::EQUAL;
        info.stencilFuncBack       = gfx::ComparisonFunc::EQUAL;
        info.stencilReadMaskFront  = stencilRef;
        info.stencilReadMaskBack   = stencilRef;
        info.stencilWriteMaskFront = 0;
        info.stencilWriteMaskBack  = 0;
        info.stencilRefFront       = stencilRef;
        info.stencilRefBack        = stencilRef;
        info.stencilFailOpFront    = gfx::StencilOp::KEEP;
        info.stencilFailOpBack     = gfx::StencilOp::KEEP;
        info.stencilZFailOpFront   = gfx::StencilOp::KEEP;
        info.stencilZFailOpBack    = gfx::StencilOp::KEEP;
        info.stencilPassOpFront    = gfx::StencilOp::KEEP;
        info.stencilPassOpBack     = gfx::StencilOp::KEEP;
    }
    return info;
}

IntrusivePtr<Material> wrapWithStencilUncached(IntrusivePtr<Material> base,
                                               uint32_t depth, bool isMaskShape) {
    if (!base || depth == 0) return base;

    IMaterialInstanceInfo instInfo;
    instInfo.parent = base.get();
    auto *inst = ccnew MaterialInstance(instInfo);

    PassOverrides po;
    po.depthStencilState = buildStencilDssInfo(depth, isMaskShape);
    if (isMaskShape) {
        po.priority = static_cast<int32_t>(pipeline::RenderPriority::MIN) +
                      static_cast<int32_t>(depth - 1);
    }
    inst->overridePipelineStates(po);
    return IntrusivePtr<Material>(inst);
}

}  // namespace

// Exposed to UIBatcher2d so the stencil cache can build wrapped materials
// on demand (keeps the wrapping details in one place).
IntrusivePtr<Material> uiRendererWrapMaterialUncached(IntrusivePtr<Material> base,
                                                     uint32_t depth, bool isMaskShape) {
    return wrapWithStencilUncached(std::move(base), depth, isMaskShape);
}

CC_IMPLEMENT_CLASS(UIRenderer, "cc.UIRenderer", Component)
CC_END_CLASS(UIRenderer);

UIRenderer::UIRenderer() {
    _wantsLateUpdate = true;
}

UIRenderer::~UIRenderer() = default;

ccstd::vector<gfx::Attribute> UIRenderer::vertexAttributes() const {
    return {
        gfx::Attribute{gfx::ATTR_NAME_POSITION, gfx::Format::RGB32F},
    };
}

IntrusivePtr<Material> UIRenderer::resolveMaterial() {
    return nullptr;
}

void UIRenderer::onEnable() {
    _dirty = true;
    rebuildForRender();
    registerSelf();
}

void UIRenderer::onDisable() {
    unregisterSelf();
    _material = nullptr;
}

void UIRenderer::lateUpdate(float /*dt*/) {
    if (!_dirty) return;
    rebuildForRender();
    _dirty = false;
}

void UIRenderer::rebuildForRender() {
    updateGeometry();
    _attributes   = vertexAttributes();
    _batchTexture = resolveBatchTexture();

    const int ancestors = countMaskAncestors(getNode());
    _stencilDepth = static_cast<uint32_t>(ancestors) + (isMask() ? 1u : 0u);

    IntrusivePtr<Material> base = resolveMaterial();
    if (!base) {
        _material = nullptr;
        _batchKey = 0;
        return;
    }
    _material = UIBatcher2d::get().getStencilMaterial(base.get(), _stencilDepth, isMask());
    computeBatchKey();
}

void UIRenderer::computeBatchKey() {
    size_t h = reinterpret_cast<size_t>(_material.get());
    h ^= reinterpret_cast<size_t>(_batchTexture) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= static_cast<size_t>(_stencilDepth)     + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= static_cast<size_t>(isMask() ? 1u : 0u) + 0x9e3779b9 + (h << 6) + (h >> 2);
    _batchKey = static_cast<ccstd::hash_t>(h);
}

void UIRenderer::registerSelf() {
    if (_registered) return;
    UIBatcher2d::get().registerRenderer(this);
    _registered = true;
}

void UIRenderer::unregisterSelf() {
    if (!_registered) return;
    UIBatcher2d::get().unregisterRenderer(this);
    _registered = false;
}

}  // namespace cc
