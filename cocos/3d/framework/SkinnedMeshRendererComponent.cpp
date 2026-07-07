#include "3d/framework/SkinnedMeshRendererComponent.h"

#include "core/Root.h"
#include "core/assets/RenderingSubMesh.h"
#include "core/scene-graph/Node.h"
#include "scene/RenderScene.h"

namespace cc {

CC_IMPLEMENT_CLASS(SkinnedMeshRendererComponent, "cc.SkinnedMeshRenderer", Component)
CC_END_CLASS(SkinnedMeshRendererComponent);

int SkinnedMeshRendererComponent::forceLink() {
    return getStaticClass() != nullptr ? 1 : 0;
}

SkinnedMeshRendererComponent::~SkinnedMeshRendererComponent() {
    destroyModel();
}

void SkinnedMeshRendererComponent::setMesh(Mesh *mesh) {
    _mesh = mesh;
    rebuildModel();
}

void SkinnedMeshRendererComponent::setSkeleton(Skeleton *skeleton) {
    _skeleton = skeleton;
    rebuildModel();
}

void SkinnedMeshRendererComponent::setSkinningRoot(Node *root) {
    _skinningRoot = root;
    rebuildModel();
}

Node *SkinnedMeshRendererComponent::getSkinningRoot() const {
    return _skinningRoot != nullptr ? _skinningRoot : _node;
}

void SkinnedMeshRendererComponent::setMaterial(Material *material, size_t index) {
    if (_materials.size() <= index) _materials.resize(index + 1);
    _materials[index] = material;
    rebuildModel();
}

Material *SkinnedMeshRendererComponent::getMaterial(size_t index) const {
    return index < _materials.size() ? _materials[index].get() : nullptr;
}

Material *SkinnedMeshRendererComponent::materialForSubMesh(size_t index) const {
    if (index < _materials.size() && _materials[index]) return _materials[index].get();
    return _materials.empty() ? nullptr : _materials[0].get();
}

void SkinnedMeshRendererComponent::rebuildModel() {
    destroyModel();
    if (!_mesh || !_skeleton || _materials.empty() || !_node) return;

    const auto &subMeshes = _mesh->getRenderingSubMeshes();
    if (subMeshes.empty()) return;

    auto *root = Root::getInstance();
    if (!root) return;

    _model = root->createModel<SkinningModel>();
    _model->initialize();
    _model->setNode(_node);

    // bindSkeleton MUST run before initSubModel — initSubModel swaps the
    // submesh vertex buffers for their joint-mapped variants and the
    // CC_USE_SKINNING macro patches are queried during submodel creation.
    _model->bindSkeleton(_skeleton.get(), getSkinningRoot(), _mesh.get());

    for (index_t i = 0; i < static_cast<index_t>(subMeshes.size()); ++i) {
        Material *mat = materialForSubMesh(static_cast<size_t>(i));
        if (mat) _model->initSubModel(i, subMeshes[i], mat);
    }

    _model->setEnabled(isEnabledInHierarchy());
    if (isEnabledInHierarchy()) attachToScene();
}

void SkinnedMeshRendererComponent::destroyModel() {
    detachFromScene();
    if (_model) {
        _model->destroy();
        _model = nullptr;
    }
}

void SkinnedMeshRendererComponent::attachToScene() {
    if (!_model || _renderScene) return;
    auto *root = Root::getInstance();
    if (root && !root->getScenes().empty()) {
        _renderScene = root->getScenes()[0].get();
        _renderScene->addModel(_model);
    }
}

void SkinnedMeshRendererComponent::detachFromScene() {
    if (_model && _renderScene) {
        _renderScene->removeModel(_model);
        _renderScene = nullptr;
    }
}

void SkinnedMeshRendererComponent::onEnable() {
    if (!_model) {
        rebuildModel();
    } else {
        _model->setEnabled(true);
        attachToScene();
    }
}

void SkinnedMeshRendererComponent::onDisable() {
    if (_model) _model->setEnabled(false);
    detachFromScene();
}

void SkinnedMeshRendererComponent::onDestroy() {
    destroyModel();
}

} // namespace cc
