#pragma once
#include "base/Ptr.h"
#include "core/scene-graph/Node.h"
#include "scene/Model.h"
#include "3d/assets/Mesh.h"
#include "core/assets/Material.h"
#include "scene/RenderScene.h"

namespace cc::game {

class MeshRenderer {
public:
    explicit MeshRenderer(Node* node);
    ~MeshRenderer();

    void setMesh(Mesh* mesh);
    void setMaterial(Material* material);
    void setEnabled(bool enabled);

    Mesh* getMesh() const { return _mesh; }
    Material* getMaterial() const { return _material; }
    scene::Model* getModel() const { return _model.get(); }

    void updateTransform();

private:
    void rebuildModel();
    void attachToScene();
    void detachFromScene();

    Node* _node{nullptr};
    Mesh* _mesh{nullptr};
    Material* _material{nullptr};
    IntrusivePtr<scene::Model> _model;
    scene::RenderScene* _renderScene{nullptr};
    bool _enabled{true};
};

} // namespace cc::game
