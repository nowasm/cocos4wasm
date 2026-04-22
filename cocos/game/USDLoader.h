#pragma once
#ifdef USE_TINYUSDZ

#include <string>
#include <vector>
#include "core/scene-graph/Node.h"
#include "3d/assets/Mesh.h"
#include "core/assets/Material.h"
#include "game/MeshRenderer.h"

namespace cc::game {

struct USDLoadResult {
    bool success{false};
    std::string error;

    Node* rootNode{nullptr};

    // All resources created during load — caller must destroy via destroy().
    std::vector<Mesh*>         meshes;
    std::vector<Material*>     materials;
    std::vector<MeshRenderer*> renderers;
    std::vector<Node*>         nodes;   // includes rootNode

    void destroy() {
        for (auto* r : renderers) delete r;
        renderers.clear();
        for (auto* n : nodes) n->release();
        nodes.clear();
        rootNode = nullptr;
        // Mesh/Material are ref-counted; released when scene models are destroyed.
        meshes.clear();
        materials.clear();
    }
};

class USDLoader {
public:
    // Load a USD/USDA/USDC/USDZ file.
    // Builds a Node tree and attaches it to `parent`.  `parent` must not be null.
    // Caller owns the result; call result.destroy() in cleanup.
    static USDLoadResult load(const std::string& filePath, Node* parent);
};

} // namespace cc::game

#endif // USE_TINYUSDZ
