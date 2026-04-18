#include "cocos/2d/framework/UIRenderer.h"

#include "base/Log.h"
#include "core/Root.h"
#include "core/assets/EffectAsset.h"
#include "core/assets/Material.h"
#include "core/assets/RenderingSubMesh.h"
#include "core/scene-graph/Node.h"
#include "renderer/core/MaterialInstance.h"
#include "renderer/gfx-base/GFXBuffer.h"
#include "renderer/gfx-base/GFXDevice.h"
#include "renderer/pipeline/Define.h"
#include "scene/Model.h"
#include "scene/RenderScene.h"

namespace cc {

namespace {

// Counts Mask ancestors on the parent chain of `startNode` (not counting
// any component attached to startNode itself). Uses the UIRenderer::isMask()
// virtual so we don't need a Mask-specific include here.
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
                    break;  // one Mask per node max
                }
            }
        }
        p = p->getParent();
    }
    return count;
}

// Builds a DepthStencilStateInfo for a given nesting depth and stage.
// Mirrors Cocos' StencilManager bit-mask model (bit N-1 owned by the Nth
// nested mask) without mutating the global singleton's state.
//
//   ENABLED:      content inside masks — stencil EQUAL (bits 0..depth-1 set)
//   ENTER_LEVEL:  the mask's own shape — stencil NEVER + REPLACE bit (depth-1),
//                 so rasterised fragments fail the test (no colour output)
//                 while failOp writes our stencil bit.
DepthStencilStateInfo buildStencilDssInfo(uint32_t depth, bool isMaskShape) {
    DepthStencilStateInfo info;
    info.depthTest = false;
    info.depthWrite = false;

    if (depth == 0) return info;  // nothing to do (safety)

    const uint32_t writeBit  = (1u << (depth - 1));
    const uint32_t stencilRef = (1u << depth) - 1u;  // bits 0..depth-1 all set

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

// Wraps a base Material in a MaterialInstance carrying a stencil pass
// override. Returns the parent unchanged when no stencil work is needed.
IntrusivePtr<Material> wrapWithStencil(IntrusivePtr<Material> base,
                                       uint32_t depth, bool isMaskShape) {
    if (!base || depth == 0) return base;

    IMaterialInstanceInfo instInfo;
    instInfo.parent = base.get();
    auto *inst = ccnew MaterialInstance(instInfo);

    PassOverrides po;
    po.depthStencilState = buildStencilDssInfo(depth, isMaskShape);
    if (isMaskShape) {
        // RenderQueue sorts transparent passes ascending by pass priority
        // (see RenderQueue::insertRenderPass), so mask-writes render before
        // default-priority (0x80) content. Deeper nested masks bump their
        // priority slightly so inner-mask writes land after outer-mask
        // writes — REPLACE order matters when bits overlap.
        po.priority = static_cast<int32_t>(pipeline::RenderPriority::MIN) +
                      static_cast<int32_t>(depth - 1);
    }
    inst->overridePipelineStates(po);

    return IntrusivePtr<Material>(inst);
}

}  // namespace

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
    ensureModel();
}

void UIRenderer::onDisable() {
    destroyModel();
}

void UIRenderer::lateUpdate(float /*dt*/) {
    if (!_dirty) return;
    ensureModel();
    uploadBuffers();
    _dirty = false;
}

void UIRenderer::ensureModel() {
    if (_model) return;

    _material = resolveMaterial();
    if (!_material) {
        CC_LOG_WARNING("[UIRenderer] no material from resolveMaterial(); geometry not created");
        return;
    }

    // Stencil wrapping — if this renderer is itself a mask, or lives under
    // Mask ancestors, replace the freshly-built material with a
    // MaterialInstance carrying stencil pass overrides.
    const int   ancestors = countMaskAncestors(getNode());
    const bool  meIsMask  = isMask();
    const uint32_t depth  = static_cast<uint32_t>(ancestors) + (meIsMask ? 1u : 0u);
    if (depth > 0) {
        _material = wrapWithStencil(_material, depth, meIsMask);
        CC_LOG_INFO("[UIRenderer] stencil wrap: depth=%u isMask=%d", depth, (int)meIsMask);
    }

    updateGeometry();
    if (_vertexCount == 0 || _indexCount == 0) {
        CC_LOG_WARNING("[UIRenderer] updateGeometry() produced empty geometry");
        return;
    }

    auto *root = Root::getInstance();
    auto *device = root ? root->getDevice() : nullptr;
    if (!device) {
        CC_LOG_ERROR("[UIRenderer] no gfx::Device available");
        return;
    }

    const uint32_t strideBytes = _vertexStrideFloats * sizeof(float);
    const size_t   vbBytes     = _vertexData.size() * sizeof(float);
    const size_t   ibBytes     = _indexData.size() * sizeof(uint16_t);

    gfx::BufferInfo vbi;
    vbi.usage    = gfx::BufferUsageBit::VERTEX | gfx::BufferUsageBit::TRANSFER_DST;
    vbi.memUsage = gfx::MemoryUsageBit::DEVICE;
    vbi.size     = static_cast<uint32_t>(vbBytes);
    vbi.stride   = strideBytes;
    _vb = device->createBuffer(vbi);
    _vb->update(_vertexData.data(), vbi.size);
    _vbCapacityBytes = vbBytes;

    gfx::BufferInfo ibi;
    ibi.usage    = gfx::BufferUsageBit::INDEX | gfx::BufferUsageBit::TRANSFER_DST;
    ibi.memUsage = gfx::MemoryUsageBit::DEVICE;
    ibi.size     = static_cast<uint32_t>(ibBytes);
    ibi.stride   = sizeof(uint16_t);
    _ib = device->createBuffer(ibi);
    _ib->update(_indexData.data(), ibi.size);
    _ibCapacityBytes = ibBytes;

    gfx::BufferList vbs = {_vb.get()};
    _subMesh = ccnew RenderingSubMesh(vbs, vertexAttributes(),
                                      gfx::PrimitiveMode::TRIANGLE_LIST, _ib.get());

    _model = root->createModel<scene::Model>();
    _model->initialize();
    _model->setNode(getNode());
    _model->setTransform(getNode());
    _model->initSubModel(0, _subMesh, _material);
    _model->setEnabled(true);

    const auto &scenes = root->getScenes();
    if (!scenes.empty()) {
        _renderScene = scenes[0].get();
        _renderScene->addModel(_model);
    }

    CC_LOG_INFO("[UIRenderer] model attached (verts=%u, idx=%u, stride=%uB)",
                _vertexCount, _indexCount, strideBytes);
}

void UIRenderer::uploadBuffers() {
    if (!_vb || !_ib) return;

    const size_t vbBytes = _vertexData.size() * sizeof(float);
    const size_t ibBytes = _indexData.size() * sizeof(uint16_t);

    if (vbBytes > _vbCapacityBytes || ibBytes > _ibCapacityBytes) {
        // Geometry grew; rebuild from scratch.
        destroyModel();
        ensureModel();
        return;
    }

    _vb->update(_vertexData.data(), static_cast<uint32_t>(vbBytes));
    _ib->update(_indexData.data(), static_cast<uint32_t>(ibBytes));
}

void UIRenderer::destroyModel() {
    if (_renderScene && _model) {
        _renderScene->removeModel(_model);
    }
    _renderScene = nullptr;
    _model    = nullptr;
    _subMesh  = nullptr;
    _vb       = nullptr;
    _ib       = nullptr;
    _material = nullptr;
    _vbCapacityBytes = 0;
    _ibCapacityBytes = 0;
}

}  // namespace cc
