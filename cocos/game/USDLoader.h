#pragma once
#ifdef USE_TINYUSDZ

#include <string>
#include <vector>
#include "core/scene-graph/Node.h"
#include "3d/assets/Mesh.h"
#include "3d/assets/Skeleton.h"
#include "core/assets/Material.h"
#include "game/MeshRenderer.h"
#include "animation/AnimationClip.h"
#include "animation/AnimationComponent.h"
#include "base/Ptr.h"

namespace cc {
class SkinnedMeshRendererComponent;
}

namespace cc::game {

struct USDLoadResult {
    bool success{false};
    std::string error;

    Node* rootNode{nullptr};

    // All resources created during load — caller must destroy via destroy().
    std::vector<Mesh*>         meshes;
    std::vector<Material*>     materials;
    std::vector<MeshRenderer*> renderers;
    // Non-owning list of every node the loader created (includes rootNode).
    // Ownership: the loader addRef()s only rootNode; every other node is
    // kept alive by its parent's IntrusivePtr. destroy() releases the root
    // and the cascade frees the whole tree — releasing each node
    // individually would double-free children.
    std::vector<Node*>         nodes;

    // ── UsdSkel additions ────────────────────────────────────────────────
    // Skinned meshes come through as authoring components instead of the
    // manually-pumped game::MeshRenderer: each skinned RenderMesh gets a
    // SkinnedMeshRendererComponent + joint node tree on its mesh node, and
    // the skeleton's SkelAnimation (if any) becomes an AnimationClip wired
    // to an AnimationComponent on the same node. The caller activates the
    // tree (NodeActivator) and plays the components; the loader never
    // auto-plays.
    std::vector<IntrusivePtr<AnimationClip>>  animationClips;
    std::vector<IntrusivePtr<Skeleton>>       skeletons;
    std::vector<AnimationComponent*>          animationComponents;  // owned by their nodes
    std::vector<SkinnedMeshRendererComponent*> skinnedRenderers;     // owned by their nodes

    void destroy() {
        for (auto* r : renderers) delete r;
        renderers.clear();
        // components are owned by their nodes — dropping the nodes drops them
        animationComponents.clear();
        skinnedRenderers.clear();
        // Release the root only; children die with their parent (see `nodes`).
        nodes.clear();
        if (rootNode) rootNode->release();
        rootNode = nullptr;
        // Mesh/Material are ref-counted; released when scene models are destroyed.
        meshes.clear();
        materials.clear();
        animationClips.clear();
        skeletons.clear();
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
