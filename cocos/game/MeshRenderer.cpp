#include "game/MeshRenderer.h"
#include "core/Root.h"
#include "core/assets/RenderingSubMesh.h"
#include "base/Log.h"

namespace cc::game {

MeshRenderer::MeshRenderer(Node* node) : _node(node) {
}

MeshRenderer::~MeshRenderer() {
    detachFromScene();
    if (_model) {
        _model->destroy();
        _model = nullptr;
    }
}

void MeshRenderer::setMesh(Mesh* mesh) {
    _mesh = mesh;
    rebuildModel();
}

void MeshRenderer::setMaterial(Material* material) {
    _material = material;
    rebuildModel();
}

void MeshRenderer::setEnabled(bool enabled) {
    _enabled = enabled;
    if (_model) {
        _model->setEnabled(enabled);
    }
}

void MeshRenderer::updateTransform() {
    if (_model) {
        _model->updateTransform(0);
        _model->updateUBOs(0);
    }
}

void MeshRenderer::rebuildModel() {
    if (!_mesh || !_material || !_node) return;

    const auto& subMeshes = _mesh->getRenderingSubMeshes();
    if (subMeshes.empty()) return;

    detachFromScene();
    if (_model) {
        _model->destroy();
        _model = nullptr;
    }

    auto *root = Root::getInstance();
    if (!root) return;

    _model = root->createModel<scene::Model>();
    _model->initialize();
    _model->setNode(_node);
    _model->setTransform(_node);

    for (index_t i = 0; i < static_cast<index_t>(subMeshes.size()); ++i) {
        _model->initSubModel(i, subMeshes[i], _material);
    }

    // Bounding shape from the mesh's authored/calculated bounds — without it
    // Model::getWorldBounds() stays null (no culling, no scene queries).
    const auto& meshStruct = _mesh->getStruct();
    _model->createBoundingShape(meshStruct.minPosition, meshStruct.maxPosition);

    _model->setEnabled(_enabled);
    attachToScene();
}

void MeshRenderer::attachToScene() {
    if (!_model) return;
    auto* root = Root::getInstance();
    if (root && !root->getScenes().empty()) {
        _renderScene = root->getScenes()[0].get();
        _renderScene->addModel(_model);
    }
}

void MeshRenderer::detachFromScene() {
    if (_model && _renderScene) {
        _renderScene->removeModel(_model);
        _renderScene = nullptr;
    }
}

} // namespace cc::game
