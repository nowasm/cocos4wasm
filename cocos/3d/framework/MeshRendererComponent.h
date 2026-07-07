/****************************************************************************
 MeshRendererComponent — the authoring-layer `cc.MeshRenderer` (P6).

 Component-ised successor to the manually-managed game::MeshRenderer helper:
 lifecycle handles scene attach/detach (onEnable/onDisable), destruction
 releases the model, and lateUpdate pushes the node transform to the GPU
 every frame — callers no longer hand-call updateTransform().

 Per-submesh materials: setMaterial(mat, i) targets rendering submesh i;
 submeshes without an explicit entry fall back to slot 0.

 Serialization surface is deliberately empty for now — Mesh is not yet a
 loadable Asset, so there is no uuid to reflect. When a mesh loader lands
 in AssetManager, `_mesh` / `_materials` join the reflected fields the way
 Sprite::_spriteFrame did.

 Known simplification (kept from game::MeshRenderer): models attach to
 Root::getScenes()[0]; revisit when multi-RenderScene setups appear.
****************************************************************************/

#pragma once

#include "3d/assets/Mesh.h"
#include "base/Ptr.h"
#include "base/std/container/vector.h"
#include "core/assets/Material.h"
#include "core/component/Component.h"
#include "core/reflection/Reflection.h"
#include "scene/Model.h"

namespace cc {

namespace scene { class RenderScene; }

class MeshRendererComponent : public Component {
    CC_CLASS_DECL(MeshRendererComponent, Component)
public:
    MeshRendererComponent() { _wantsLateUpdate = true; }
    ~MeshRendererComponent() override;

    void setMesh(Mesh *mesh);
    Mesh *getMesh() const { return _mesh.get(); }

    // Material for rendering submesh `index`; slot 0 doubles as the
    // fallback for submeshes without their own entry.
    void setMaterial(Material *material, size_t index = 0);
    Material *getMaterial(size_t index = 0) const;
    size_t getMaterialCount() const { return _materials.size(); }

    scene::Model *getModel() const { return _model.get(); }

    void onEnable() override;
    void onDisable() override;
    void onDestroy() override;
    void lateUpdate(float dt) override;

    static int forceLink();

private:
    void rebuildModel();
    void destroyModel();
    void attachToScene();
    void detachFromScene();
    Material *materialForSubMesh(size_t index) const;

    IntrusivePtr<Mesh> _mesh;
    ccstd::vector<IntrusivePtr<Material>> _materials;
    IntrusivePtr<scene::Model> _model;
    scene::RenderScene *_renderScene{nullptr};
};

} // namespace cc
