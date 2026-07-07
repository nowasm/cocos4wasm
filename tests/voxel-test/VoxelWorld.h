#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "cocos/game/CocosGame.h"

// Builds a voxel-style village out of unit boxes — a procedural recreation of
// the isometric map in example1.jpg (terrain slab, houses, trees, windmill,
// fountain, fenced farm, river + waterfall, floating clouds).
class VoxelWorld {
public:
    void build(cc::scene::RenderScene *rs, cc::Root *root);
    void update(float dt);
    void destroy();

private:
    // Place a box. When `parent` is set, `center` is interpreted in the
    // parent's local space; otherwise it is world space.
    cc::Node *cube(const cc::Vec3 &center, const cc::Vec3 &size,
                   const cc::Color &color, cc::Node *parent = nullptr);
    cc::Material *material(const cc::Color &color, float roughness = 0.92f);

    void buildTerrain();
    void buildPlateau();
    void buildHouse(float wx, float wz, float baseY, const cc::Color &roof,
                    float yaw, int style);
    void buildTree(float wx, float wz, float baseY, float scale);
    void buildWindmill(float wx, float wz, float baseY);
    void buildFountain(float wx, float wz, float baseY);
    void buildFarm(float wx, float wz, float baseY);
    void buildCloud(float wx, float wy, float wz, float scale);

    cc::scene::RenderScene *_rs{nullptr};
    cc::Root *_root{nullptr};
    cc::Mesh *_boxMesh{nullptr};
    std::unordered_map<uint64_t, cc::Material *> _matCache;
    std::vector<cc::Node *> _nodes;
    std::vector<cc::game::MeshRenderer *> _renderers;
    cc::Node *_windmillHub{nullptr};
    float _time{0};
};
