#include "VoxelWorld.h"
#include <cmath>
#include "base/Log.h"

using namespace cc;
using namespace cc::game;

namespace {

// --- voxel grid ---
constexpr int GW = 26;   // tiles along X
constexpr int GD = 20;   // tiles along Z
constexpr float TILE = 1.0f;

inline float worldX(float gx) { return (gx + 0.5f - GW * 0.5f) * TILE; }
inline float worldZ(float gz) { return (gz + 0.5f - GD * 0.5f) * TILE; }

inline Color rgb(int r, int g, int b) {
    return Color(static_cast<uint8_t>(r), static_cast<uint8_t>(g),
                 static_cast<uint8_t>(b), 255);
}

// deterministic per-tile pseudo-random in [0,1)
float vrand(int a, int b, int salt) {
    uint32_t h = static_cast<uint32_t>(a * 73856093) ^
                 static_cast<uint32_t>(b * 19349663) ^
                 static_cast<uint32_t>(salt * 83492791);
    h ^= h >> 13;
    h *= 0x85ebca6bu;
    h ^= h >> 16;
    return (h & 0xffffffu) / static_cast<float>(0x1000000u);
}

// --- palette — punchy cartoon-saturated, tuned for tonemap output ---
const Color GRASS[4] = {rgb(150, 210, 70), rgb(132, 196, 60),
                        rgb(168, 222, 90), rgb(118, 182, 56)};
const Color WATER[3] = {rgb(80, 188, 234), rgb(110, 206, 242), rgb(62, 168, 222)};
const Color SAND[2] = {rgb(240, 218, 158), rgb(226, 202, 140)};
const Color SOIL[2] = {rgb(150, 100, 62), rgb(168, 116, 76)};
const Color DIRT1 = rgb(172, 122, 78);
const Color DIRT2 = rgb(146, 100, 62);
const Color DIRT3 = rgb(118, 82, 50);
const Color WALL_A = rgb(248, 232, 196);
const Color WALL_B = rgb(228, 208, 162);
const Color ROOF_BLUE = rgb(46, 80, 200);
const Color ROOF_RED = rgb(208, 76, 56);
const Color ROOF_BROWN = rgb(96, 72, 56);
const Color TRUNK = rgb(118, 82, 50);
const Color LEAF[3] = {rgb(86, 168, 60), rgb(116, 192, 70), rgb(64, 138, 50)};
const Color STONE = rgb(206, 202, 192);
const Color STONE_D = rgb(160, 156, 146);
const Color WOOD = rgb(158, 116, 74);
const Color WOOD_D = rgb(108, 78, 52);
const Color FENCE = rgb(180, 134, 88);
const Color CLOUD_A = rgb(255, 255, 255);
const Color CLOUD_B = rgb(240, 242, 248);

enum Tile { T_GRASS, T_WATER, T_PATH, T_FARM };

// footprint regions to keep terrain types apart
bool inFarmRegion(int gx, int gz) {
    return gx >= 2 && gx <= 8 && gz >= 11 && gz <= 16;
}
float riverCenter(int gz) { return 17.6f + std::sin(gz * 0.5f) * 2.1f; }
bool isWater(int gx, int gz) {
    return std::fabs(gx - riverCenter(gz)) <= 1.55f;
}
bool isPath(int gx, int gz) {
    bool vert = (gx == GW / 2 - 1 || gx == GW / 2) && gz >= 3 && gz <= GD - 3;
    bool horiz = (gz == GD / 2 - 1 || gz == GD / 2) && gx >= 6 && gx <= 20;
    return vert || horiz;
}

} // namespace

// ---------------------------------------------------------------------------

void VoxelWorld::build(scene::RenderScene *rs, Root *root) {
    _rs = rs;
    _root = root;
    _boxMesh = PrimitiveFactory::createBox(1.0f, 1.0f, 1.0f);

    buildTerrain();
    buildPlateau();

    // village houses (world XZ, roof colour, yaw, style)
    buildHouse(4.0f, 6.0f, 0.0f, ROOF_BLUE, 0.0f, 0);
    buildHouse(-2.0f, -1.0f, 0.0f, ROOF_BLUE, 180.0f, 1);
    buildHouse(-3.0f, 8.0f, 0.0f, ROOF_RED, 0.0f, 0);
    buildHouse(3.5f, 1.0f, 0.0f, ROOF_BROWN, 90.0f, 1);
    buildHouse(8.5f, 3.5f, 0.0f, ROOF_BROWN, -90.0f, 0);
    buildHouse(-0.5f, -6.0f, 0.0f, ROOF_BLUE, 180.0f, 1);

    buildFountain(0.5f, 1.5f, 0.0f);
    buildWindmill(9.5f, -3.0f, 0.0f);
    buildFarm(-7.0f, 4.0f, 0.0f);

    // scattered trees (world XZ)
    const float trees[][3] = {
        {-10.5f, -2.5f, 1.05f}, {-8.0f, 0.5f, 0.9f},  {7.5f, 7.5f, 1.1f},
        {10.5f, 5.5f, 0.95f},   {-4.5f, 4.0f, 0.85f}, {1.5f, 8.5f, 1.0f},
        {6.0f, -6.5f, 1.05f},   {-9.5f, 8.0f, 0.9f},  {11.5f, -7.0f, 1.0f},
        {-11.0f, 5.5f, 0.95f},  {2.0f, -3.0f, 0.8f},  {9.0f, 1.0f, 0.9f},
        {-6.0f, -8.0f, 1.0f},   {12.0f, 8.0f, 1.05f}, {5.0f, 9.0f, 0.85f},
        {-1.0f, 4.5f, 0.8f},
    };
    for (auto &t : trees) buildTree(t[0], t[1], 0.0f, t[2]);

    // floating clouds around the slab
    buildCloud(-17.0f, 6.5f, -4.0f, 1.15f);
    buildCloud(16.0f, 7.5f, 6.0f, 1.0f);
    buildCloud(-14.0f, 8.0f, 9.0f, 0.85f);
    buildCloud(14.0f, 6.0f, -10.0f, 1.05f);
    buildCloud(0.0f, 9.0f, -14.0f, 0.95f);
    buildCloud(-4.0f, 7.0f, 14.0f, 0.9f);

    CC_LOG_INFO("VoxelWorld: %zu boxes placed", _renderers.size());
}

void VoxelWorld::update(float dt) {
    _time += dt;
    if (_windmillHub) _windmillHub->setRotationFromEuler(0.0f, 0.0f, _time * 48.0f);
}

void VoxelWorld::destroy() {
    // detaches every Model from the render scene; nodes are released by the
    // engine on shutdown.
    for (auto *r : _renderers) delete r;
    _renderers.clear();
    _nodes.clear();
    _windmillHub = nullptr;
}

// ---------------------------------------------------------------------------

cc::Material *VoxelWorld::material(const Color &c, float roughness) {
    uint64_t key = (static_cast<uint64_t>(c.r) << 32) |
                   (static_cast<uint64_t>(c.g) << 24) |
                   (static_cast<uint64_t>(c.b) << 16) |
                   static_cast<uint64_t>(roughness * 100.0f);
    auto it = _matCache.find(key);
    if (it != _matCache.end()) return it->second;

    PBRParams params;
    params.albedo = c;
    params.roughness = roughness;
    params.metallic = 0.0f;
    params.specularIntensity = 0.35f;
    auto *mat = MaterialFactory::createStandard(params);
    _matCache[key] = mat;
    return mat;
}

cc::Node *VoxelWorld::cube(const Vec3 &center, const Vec3 &size,
                           const Color &color, Node *parent) {
    auto *n = ccnew Node("v");
    if (parent) n->setParent(parent);
    n->setPosition(center);
    n->setScale(size);
    auto *r = ccnew MeshRenderer(n);
    r->setMesh(_boxMesh);
    r->setMaterial(material(color));
    _nodes.push_back(n);
    _renderers.push_back(r);
    return n;
}

// ---------------------------------------------------------------------------

void VoxelWorld::buildTerrain() {
    // dirt body — three inset layers so the grass top overhangs slightly
    float bw = GW * TILE - 0.5f;
    float bd = GD * TILE - 0.5f;
    cube(Vec3(0, -1.6f, 0), Vec3(bw, 1.2f, bd), DIRT1);
    cube(Vec3(0, -3.0f, 0), Vec3(bw - 0.2f, 1.6f, bd - 0.2f), DIRT2);
    cube(Vec3(0, -4.6f, 0), Vec3(bw - 0.5f, 1.6f, bd - 0.5f), DIRT3);

    // top tiles — slight gaps between them read as the voxel grid
    for (int gz = 0; gz < GD; ++gz) {
        for (int gx = 0; gx < GW; ++gx) {
            Tile type = T_GRASS;
            if (isWater(gx, gz)) type = T_WATER;
            else if (inFarmRegion(gx, gz)) type = T_FARM;
            else if (isPath(gx, gz)) type = T_PATH;

            float wx = worldX(gx), wz = worldZ(gz);
            if (type == T_WATER) {
                int idx = static_cast<int>(vrand(gx, gz, 1) * 3.0f) % 3;
                cube(Vec3(wx, -0.55f, wz), Vec3(0.99f, 0.9f, 0.99f), WATER[idx],
                     nullptr);
            } else if (type == T_PATH) {
                int idx = static_cast<int>(vrand(gx, gz, 2) * 2.0f) % 2;
                cube(Vec3(wx, -0.5f, wz), Vec3(0.98f, 1.0f, 0.98f), SAND[idx]);
            } else if (type == T_FARM) {
                cube(Vec3(wx, -0.5f, wz), Vec3(0.98f, 1.0f, 0.98f),
                     SOIL[(gx + gz) & 1]);
            } else {
                int idx = static_cast<int>(vrand(gx, gz, 3) * 4.0f) % 4;
                cube(Vec3(wx, -0.5f, wz), Vec3(0.98f, 1.0f, 0.98f), GRASS[idx]);
            }
        }
    }

    // waterfall — river cubes spilling off the front edge
    int gzEdge = GD - 1;
    float cx = riverCenter(gzEdge);
    for (int gx = 0; gx < GW; ++gx) {
        if (!isWater(gx, gzEdge)) continue;
        float wx = worldX(gx);
        float wz = worldZ(gzEdge) + 0.5f;
        for (int d = 0; d < 5; ++d) {
            cube(Vec3(wx, -0.6f - d * 1.0f, wz + 0.05f),
                 Vec3(0.95f, 1.05f, 0.35f), WATER[d % 3]);
        }
        // splash pool at the base
        cube(Vec3(wx, -5.2f, wz + 0.6f), Vec3(1.3f, 0.5f, 1.3f), WATER[1]);
    }
}

void VoxelWorld::buildPlateau() {
    // stepped grassy mound in the back-left corner with a house on top
    float px = -7.0f, pz = -6.0f;
    cube(Vec3(px, 0.5f, pz), Vec3(10.0f, 1.0f, 8.0f), GRASS[1]);
    cube(Vec3(px - 0.5f, 1.5f, pz - 0.4f), Vec3(6.2f, 1.0f, 5.0f), GRASS[0]);

    // stairs down the front face (toward +Z)
    cube(Vec3(px, 0.84f, pz + 4.3f), Vec3(2.6f, 0.34f, 0.9f), SAND[0]);
    cube(Vec3(px, 0.50f, pz + 5.1f), Vec3(2.6f, 0.34f, 0.9f), SAND[1]);
    cube(Vec3(px, 0.16f, pz + 5.9f), Vec3(2.6f, 0.34f, 0.9f), SAND[0]);

    buildHouse(px - 0.5f, pz - 0.4f, 2.0f, ROOF_BLUE, 0.0f, 0);
    buildTree(px + 2.3f, pz + 0.5f, 2.0f, 0.8f);
    buildTree(px - 2.5f, pz - 1.5f, 2.0f, 0.9f);
}

void VoxelWorld::buildHouse(float wx, float wz, float baseY, const Color &roof,
                            float yaw, int style) {
    auto *parent = ccnew Node("house");
    parent->setPosition(Vec3(wx, baseY, wz));
    parent->setRotationFromEuler(0.0f, yaw, 0.0f);
    _nodes.push_back(parent);

    const Color wall = (style == 0) ? WALL_A : WALL_B;
    const float W = 3.0f, D = 3.0f, H = 2.2f;

    cube(Vec3(0, 0.16f, 0), Vec3(W + 0.3f, 0.32f, D + 0.3f), rgb(120, 110, 100),
         parent);                                                  // foundation
    cube(Vec3(0, H * 0.5f + 0.16f, 0), Vec3(W, H, D), wall, parent); // body
    // corner posts for a timbered look
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sz = -1; sz <= 1; sz += 2)
            cube(Vec3(sx * W * 0.5f, H * 0.5f + 0.16f, sz * D * 0.5f),
                 Vec3(0.28f, H, 0.28f), WOOD_D, parent);
    // door + windows on the +Z face
    cube(Vec3(0, 0.86f, D * 0.5f + 0.03f), Vec3(0.78f, 1.4f, 0.16f), WOOD_D,
         parent);
    cube(Vec3(-0.95f, 1.55f, D * 0.5f + 0.03f), Vec3(0.6f, 0.6f, 0.14f),
         rgb(128, 172, 192), parent);
    cube(Vec3(0.95f, 1.55f, D * 0.5f + 0.03f), Vec3(0.6f, 0.6f, 0.14f),
         rgb(128, 172, 192), parent);

    // stepped gable roof (ridge runs along Z)
    const int steps = 4;
    float wallTop = H + 0.16f;
    for (int k = 0; k < steps; ++k) {
        float t = k / static_cast<float>(steps);
        float rw = (W + 0.7f) - t * (W + 0.3f);
        float ry = wallTop + 0.24f + k * 0.42f;
        cube(Vec3(0, ry, 0), Vec3(rw, 0.46f, D + 0.6f), roof, parent);
    }
    // chimney
    cube(Vec3(W * 0.28f, wallTop + 1.5f, -D * 0.22f), Vec3(0.5f, 1.3f, 0.5f),
         rgb(120, 96, 84), parent);
}

void VoxelWorld::buildTree(float wx, float wz, float baseY, float s) {
    cube(Vec3(wx, baseY + 0.85f * s, wz), Vec3(0.55f * s, 1.7f * s, 0.55f * s),
         TRUNK);
    float cy = baseY + 1.7f * s;
    cube(Vec3(wx, cy + 0.9f * s, wz),
         Vec3(2.0f * s, 1.9f * s, 2.0f * s), LEAF[0]);
    cube(Vec3(wx - 0.85f * s, cy + 0.5f * s, wz + 0.3f * s),
         Vec3(1.3f * s, 1.3f * s, 1.3f * s), LEAF[1]);
    cube(Vec3(wx + 0.8f * s, cy + 0.6f * s, wz - 0.45f * s),
         Vec3(1.35f * s, 1.35f * s, 1.35f * s), LEAF[2]);
    cube(Vec3(wx + 0.15f * s, cy + 1.75f * s, wz + 0.1f * s),
         Vec3(1.1f * s, 1.1f * s, 1.1f * s), LEAF[1]);
}

void VoxelWorld::buildWindmill(float wx, float wz, float baseY) {
    cube(Vec3(wx, baseY + 1.7f, wz), Vec3(3.2f, 3.4f, 3.2f), WOOD);
    cube(Vec3(wx, baseY + 3.9f, wz), Vec3(2.6f, 1.2f, 2.6f), WOOD_D);
    cube(Vec3(wx, baseY + 4.85f, wz), Vec3(2.1f, 0.95f, 2.1f), rgb(82, 74, 68));
    cube(Vec3(wx, baseY + 0.85f, wz + 1.6f + 0.04f), Vec3(0.8f, 1.5f, 0.16f),
         WOOD_D);
    // small balcony detail
    cube(Vec3(wx, baseY + 2.6f, wz + 1.7f), Vec3(2.0f, 0.2f, 0.5f), WOOD_D);

    // rotating blade assembly on the +Z face
    auto *hub = ccnew Node("windmillHub");
    hub->setPosition(Vec3(wx, baseY + 2.7f, wz + 1.85f));
    _nodes.push_back(hub);
    _windmillHub = hub;
    cube(Vec3(0, 0, 0), Vec3(0.6f, 0.6f, 0.45f), WOOD_D, hub);
    for (int i = 0; i < 4; ++i) {
        auto *arm = ccnew Node("blade");
        arm->setParent(hub);
        arm->setPosition(Vec3(0, 0, 0));
        arm->setRotationFromEuler(0.0f, 0.0f, i * 90.0f);
        _nodes.push_back(arm);
        cube(Vec3(0, 1.7f, 0), Vec3(0.26f, 3.4f, 0.16f), WOOD_D, arm);
        cube(Vec3(0.5f, 1.8f, 0.04f), Vec3(0.8f, 2.3f, 0.1f),
             rgb(220, 210, 184), arm);
    }
}

void VoxelWorld::buildFountain(float wx, float wz, float baseY) {
    cube(Vec3(wx, baseY + 0.3f, wz), Vec3(3.4f, 0.6f, 3.4f), STONE);
    cube(Vec3(wx, baseY + 0.78f, wz), Vec3(2.5f, 0.5f, 2.5f), WATER[1]);
    // raised rim
    for (int sx = -1; sx <= 1; sx += 2)
        cube(Vec3(wx + sx * 1.55f, baseY + 0.7f, wz), Vec3(0.4f, 0.7f, 3.4f),
             STONE_D);
    for (int sz = -1; sz <= 1; sz += 2)
        cube(Vec3(wx, baseY + 0.7f, wz + sz * 1.55f), Vec3(3.4f, 0.7f, 0.4f),
             STONE_D);
    // central column + top basin
    cube(Vec3(wx, baseY + 1.4f, wz), Vec3(0.65f, 1.3f, 0.65f), STONE);
    cube(Vec3(wx, baseY + 2.1f, wz), Vec3(1.25f, 0.4f, 1.25f), STONE_D);
    cube(Vec3(wx, baseY + 2.45f, wz), Vec3(0.55f, 0.35f, 0.55f), WATER[1]);
}

void VoxelWorld::buildFarm(float wx, float wz, float baseY) {
    const float halfW = 3.2f, halfD = 2.7f;
    // fence — posts with two rails per side
    for (float x = -halfW; x <= halfW + 0.01f; x += 0.8f) {
        for (int sz = -1; sz <= 1; sz += 2)
            cube(Vec3(wx + x, baseY + 0.5f, wz + sz * halfD),
                 Vec3(0.18f, 1.0f, 0.18f), FENCE);
    }
    for (float z = -halfD; z <= halfD + 0.01f; z += 0.9f) {
        for (int sx = -1; sx <= 1; sx += 2)
            cube(Vec3(wx + sx * halfW, baseY + 0.5f, wz + z),
                 Vec3(0.18f, 1.0f, 0.18f), FENCE);
    }
    for (int sz = -1; sz <= 1; sz += 2)
        for (float ry : {0.35f, 0.75f})
            cube(Vec3(wx, baseY + ry, wz + sz * halfD),
                 Vec3(halfW * 2.0f, 0.12f, 0.12f), FENCE);
    for (int sx = -1; sx <= 1; sx += 2)
        for (float ry : {0.35f, 0.75f})
            cube(Vec3(wx + sx * halfW, baseY + ry, wz),
                 Vec3(0.12f, 0.12f, halfD * 2.0f), FENCE);

    // crop rows — pumpkins (orange) and leafy greens
    for (int r = -2; r <= 2; ++r) {
        for (int c = -2; c <= 2; ++c) {
            float cx = wx + c * 1.1f, cz = wz + r * 0.95f;
            bool pumpkin = ((r + c) & 1) == 0;
            if (pumpkin) {
                cube(Vec3(cx, baseY + 0.35f, cz), Vec3(0.7f, 0.62f, 0.7f),
                     rgb(224, 132, 48));
                cube(Vec3(cx, baseY + 0.72f, cz), Vec3(0.16f, 0.2f, 0.16f),
                     LEAF[2]);
            } else {
                cube(Vec3(cx, baseY + 0.3f, cz), Vec3(0.6f, 0.5f, 0.6f),
                     LEAF[1]);
            }
        }
    }
}

void VoxelWorld::buildCloud(float wx, float wy, float wz, float s) {
    cube(Vec3(wx, wy, wz), Vec3(2.6f * s, 1.5f * s, 2.1f * s), CLOUD_A);
    cube(Vec3(wx - 1.4f * s, wy - 0.25f * s, wz + 0.25f * s),
         Vec3(1.7f * s, 1.2f * s, 1.6f * s), CLOUD_B);
    cube(Vec3(wx + 1.5f * s, wy - 0.15f * s, wz - 0.35f * s),
         Vec3(1.8f * s, 1.25f * s, 1.7f * s), CLOUD_B);
    cube(Vec3(wx + 0.35f * s, wy + 0.75f * s, wz),
         Vec3(1.6f * s, 1.05f * s, 1.45f * s), CLOUD_A);
    cube(Vec3(wx - 0.5f * s, wy - 0.55f * s, wz - 0.4f * s),
         Vec3(1.4f * s, 0.95f * s, 1.3f * s), CLOUD_B);
}
