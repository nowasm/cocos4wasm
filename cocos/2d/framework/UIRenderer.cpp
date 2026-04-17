#include "cocos/2d/framework/UIRenderer.h"

#include "base/Log.h"
#include "core/Root.h"
#include "core/assets/Material.h"
#include "core/assets/RenderingSubMesh.h"
#include "core/scene-graph/Node.h"
#include "renderer/gfx-base/GFXBuffer.h"
#include "renderer/gfx-base/GFXDevice.h"
#include "scene/Model.h"
#include "scene/RenderScene.h"

namespace cc {

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
