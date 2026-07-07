#include "3d/framework/MeshRendererComponent.h"

#include "core/Root.h"
#include "core/assets/RenderingSubMesh.h"
#include "core/scene-graph/Node.h"
#include "scene/RenderScene.h"

namespace cc {

CC_IMPLEMENT_CLASS(MeshRendererComponent, "cc.MeshRenderer", Component)
CC_END_CLASS(MeshRendererComponent);

int MeshRendererComponent::forceLink() {
    return getStaticClass() != nullptr ? 1 : 0;
}

MeshRendererComponent::~MeshRendererComponent() {
    destroyModel();
}

void MeshRendererComponent::setMesh(Mesh *mesh) {
    _mesh = mesh;
    rebuildModel();
}

void MeshRendererComponent::setMaterial(Material *material, size_t index) {
    if (_materials.size() <= index) _materials.resize(index + 1);
    _materials[index] = material;
    rebuildModel();
}

Material *MeshRendererComponent::getMaterial(size_t index) const {
    return index < _materials.size() ? _materials[index].get() : nullptr;
}

Material *MeshRendererComponent::materialForSubMesh(size_t index) const {
    if (index < _materials.size() && _materials[index]) return _materials[index].get();
    return _materials.empty() ? nullptr : _materials[0].get();
}

void MeshRendererComponent::rebuildModel() {
    destroyModel();
    if (!_mesh || _materials.empty() || !_node) return;

    const auto &subMeshes = _mesh->getRenderingSubMeshes();
    if (subMeshes.empty()) return;

    auto *root = Root::getInstance();
    if (!root) return;

    _model = root->createModel<scene::Model>();
    _model->initialize();
    _model->setNode(_node);
    _model->setTransform(_node);

    for (index_t i = 0; i < static_cast<index_t>(subMeshes.size()); ++i) {
        Material *mat = materialForSubMesh(static_cast<size_t>(i));
        if (mat) _model->initSubModel(i, subMeshes[i], mat);
    }

    _model->setEnabled(isEnabledInHierarchy());
    if (isEnabledInHierarchy()) attachToScene();
}

void MeshRendererComponent::destroyModel() {
    detachFromScene();
    if (_model) {
        _model->destroy();
        _model = nullptr;
    }
}

void MeshRendererComponent::attachToScene() {
    if (!_model || _renderScene) return;
    auto *root = Root::getInstance();
    if (root && !root->getScenes().empty()) {
        _renderScene = root->getScenes()[0].get();
        _renderScene->addModel(_model);
    }
}

void MeshRendererComponent::detachFromScene() {
    if (_model && _renderScene) {
        _renderScene->removeModel(_model);
        _renderScene = nullptr;
    }
}

void MeshRendererComponent::onEnable() {
    if (!_model) {
        rebuildModel();
    } else {
        _model->setEnabled(true);
        attachToScene();
    }
}

void MeshRendererComponent::onDisable() {
    if (_model) _model->setEnabled(false);
    detachFromScene();
}

void MeshRendererComponent::onDestroy() {
    destroyModel();
}

void MeshRendererComponent::lateUpdate(float /*dt*/) {
    if (_model) {
        _model->updateTransform(0);
        _model->updateUBOs(0);
    }
}

} // namespace cc
