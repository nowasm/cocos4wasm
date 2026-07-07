/****************************************************************************
 SkinnedMeshRendererComponent — the authoring-layer `cc.SkinnedMeshRenderer`.

 Sibling of MeshRendererComponent for GPU-skinned meshes: it builds a
 SkinningModel instead of a plain scene::Model and binds a Skeleton asset
 against a skinning-root node whose descendants are the joint nodes
 (resolved by Skeleton::getJoints() paths via Node::getChildByPath).

 Build order matters and is encapsulated here: createModel<SkinningModel> →
 initialize → setNode → bindSkeleton(skeleton, skinningRoot, mesh) →
 initSubModel (SkinningModel swaps in the joint-mapped vertex buffers) →
 setEnabled → RenderScene::addModel.

 Per-frame joint upload needs no pumping from this component:
 Root::frameMove → RenderScene::update(stamp) already calls
 updateTransform/updateUBOs on every enabled model with a monotonically
 increasing stamp, which is exactly what SkinningModel's IJointTransform
 caching wants.

 Serialization surface is empty for now, same rationale as
 MeshRendererComponent: Mesh/Skeleton are not loadable Assets yet.
****************************************************************************/

#pragma once

#include "3d/assets/Mesh.h"
#include "3d/assets/Skeleton.h"
#include "base/Ptr.h"
#include "base/std/container/vector.h"
#include "core/assets/Material.h"
#include "core/component/Component.h"
#include "core/reflection/Reflection.h"
#include "3d/models/SkinningModel.h"

namespace cc {

namespace scene { class RenderScene; }

class SkinnedMeshRendererComponent : public Component {
    CC_CLASS_DECL(SkinnedMeshRendererComponent, Component)
public:
    SkinnedMeshRendererComponent() = default;
    ~SkinnedMeshRendererComponent() override;

    void setMesh(Mesh *mesh);
    Mesh *getMesh() const { return _mesh.get(); }

    void setSkeleton(Skeleton *skeleton);
    Skeleton *getSkeleton() const { return _skeleton.get(); }

    // Root of the joint node tree. Skeleton joint paths are resolved
    // relative to this node. Defaults to the component's own node.
    void setSkinningRoot(Node *root);
    Node *getSkinningRoot() const;

    // Material for rendering submesh `index`; slot 0 doubles as the
    // fallback for submeshes without their own entry.
    void setMaterial(Material *material, size_t index = 0);
    Material *getMaterial(size_t index = 0) const;

    SkinningModel *getModel() const { return _model.get(); }

    void onEnable() override;
    void onDisable() override;
    void onDestroy() override;

    static int forceLink();

private:
    void rebuildModel();
    void destroyModel();
    void attachToScene();
    void detachFromScene();
    Material *materialForSubMesh(size_t index) const;

    IntrusivePtr<Mesh> _mesh;
    IntrusivePtr<Skeleton> _skeleton;
    Node *_skinningRoot{nullptr}; // not owned; lives in the same node tree
    ccstd::vector<IntrusivePtr<Material>> _materials;
    IntrusivePtr<SkinningModel> _model;
    scene::RenderScene *_renderScene{nullptr};
};

} // namespace cc
