#ifdef USE_TINYUSDZ

#include "../TestScene.h"
#include "cocos/game/USDLoader.h"
#include "base/Log.h"
#include <cstdio>

using namespace cc;
using namespace cc::game;

// ─── Shared inline USDA writer ────────────────────────────────────────────────

static const char* k_cube_fv_counts   = "int[] faceVertexCounts = [4, 4, 4, 4, 4, 4]\n";
static const char* k_cube_fv_indices  = "int[] faceVertexIndices = [0,1,2,3, 4,5,6,7, 0,4,7,3, 1,5,6,2, 0,1,5,4, 3,2,6,7]\n";
static const char* k_cube_points      =
    "point3f[] points = [\n"
    "    (-0.5,-0.5, 0.5),( 0.5,-0.5, 0.5),( 0.5, 0.5, 0.5),(-0.5, 0.5, 0.5),\n"
    "    (-0.5,-0.5,-0.5),( 0.5,-0.5,-0.5),( 0.5, 0.5,-0.5),(-0.5, 0.5,-0.5)\n"
    "]\n";
static const char* k_cube_normals     =
    "normal3f[] normals = [\n"
    "    (0,0,1),(0,0,1),(0,0,1),(0,0,1),\n"
    "    (0,0,-1),(0,0,-1),(0,0,-1),(0,0,-1),\n"
    "    (-1,0,0),(-1,0,0),(-1,0,0),(-1,0,0),\n"
    "    (1,0,0),(1,0,0),(1,0,0),(1,0,0),\n"
    "    (0,-1,0),(0,-1,0),(0,-1,0),(0,-1,0),\n"
    "    (0,1,0),(0,1,0),(0,1,0),(0,1,0)\n"
    "] (interpolation = \"faceVarying\")\n";

// ─── TestUSDBasic ─────────────────────────────────────────────────────────────
// Loads "test.usda" from the working directory; skips gracefully if not found.
class TestUSDBasic : public TestScene {
public:
    std::string name()     override { return "USD Basic Load"; }
    std::string category() override { return "USD"; }

    void setup(scene::RenderScene*, Root*) override {
        _usdResult = USDLoader::load("test.usda", nullptr);
        if (!_usdResult.success) {
            CC_LOG_WARNING("TestUSDBasic: %s", _usdResult.error.c_str());
            return;
        }
        for (auto* r : _usdResult.renderers) _renderers.push_back(r);
        for (auto* n : _usdResult.nodes)     _nodes.push_back(n);
        _usdResult.renderers.clear();
        _usdResult.nodes.clear();
        CC_LOG_INFO("TestUSDBasic: %zu mesh(es), %zu material(s)",
                    _usdResult.meshes.size(), _usdResult.materials.size());
    }
    void cleanup() override { _usdResult.destroy(); TestScene::cleanup(); }
private:
    USDLoadResult _usdResult;
};
REGISTER_TEST("USD", "USD Basic Load", TestUSDBasic);

// ─── TestUSDInline ────────────────────────────────────────────────────────────
// Single gray cube — minimal pipeline smoke test.
class TestUSDInline : public TestScene {
public:
    std::string name()     override { return "USD Inline Cube"; }
    std::string category() override { return "USD"; }

    void setup(scene::RenderScene*, Root*) override {
        const char* path = "usd_inline_cube.usda";
        FILE* f = fopen(path, "w");
        if (!f) { CC_LOG_ERROR("TestUSDInline: cannot write file"); return; }
        fprintf(f, "#usda 1.0\ndef Xform \"Root\" {\n    def Mesh \"Cube\" {\n");
        fputs(k_cube_fv_counts,  f);
        fputs(k_cube_fv_indices, f);
        fputs(k_cube_points,     f);
        fputs(k_cube_normals,    f);
        fprintf(f, "    }\n}\n");
        fclose(f);

        _usdResult = USDLoader::load(path, nullptr);
        if (!_usdResult.success) {
            CC_LOG_ERROR("TestUSDInline: %s", _usdResult.error.c_str());
            return;
        }
        for (auto* r : _usdResult.renderers) _renderers.push_back(r);
        for (auto* n : _usdResult.nodes)     _nodes.push_back(n);
        _usdResult.renderers.clear();
        _usdResult.nodes.clear();
        CC_LOG_INFO("TestUSDInline: OK, %zu mesh(es)", _usdResult.meshes.size());
    }
    void cleanup() override { _usdResult.destroy(); TestScene::cleanup(); }
private:
    USDLoadResult _usdResult;
};
REGISTER_TEST("USD", "USD Inline Cube", TestUSDInline);

// ─── TestUSDScene ─────────────────────────────────────────────────────────────
// A full PBR scene written inline as USDA:
//   • 3 colored cubes (red / green / blue) at different positions
//   • 1 metallic gold cube in the center, elevated
//   • 1 flat ground plane (gray)
//   • UsdPreviewSurface materials with diffuseColor / roughness / metallic
//   • Scene Xform hierarchy — one Xform per cube group
//   • Rotates the whole scene in update()
class TestUSDScene : public TestScene {
public:
    std::string name()     override { return "USD PBR Scene"; }
    std::string category() override { return "USD"; }

    void setup(scene::RenderScene*, Root*) override {
        const char* path = "usd_pbr_scene.usda";
        if (!writeScene(path)) {
            CC_LOG_ERROR("TestUSDScene: cannot write file");
            return;
        }
        _usdResult = USDLoader::load(path, nullptr);
        if (!_usdResult.success) {
            CC_LOG_ERROR("TestUSDScene: %s", _usdResult.error.c_str());
            return;
        }
        // Keep root pointer for rotation animation
        _sceneRoot = _usdResult.rootNode;
        for (auto* r : _usdResult.renderers) _renderers.push_back(r);
        for (auto* n : _usdResult.nodes)     _nodes.push_back(n);
        _usdResult.renderers.clear();
        _usdResult.nodes.clear();
        CC_LOG_INFO("TestUSDScene: %zu mesh(es), %zu material(s)",
                    _usdResult.meshes.size(), _usdResult.materials.size());
    }

    void update(float dt) override {
        _angle += dt * 25.f;  // 25 deg/s
        if (!_sceneRoot) return;
        _sceneRoot->setRotationFromEuler(0, _angle, 0);
        for (auto* r : _renderers) r->updateTransform();
    }

    void cleanup() override {
        _usdResult.destroy();
        _sceneRoot = nullptr;
        _angle = 0;
        TestScene::cleanup();
    }

private:
    USDLoadResult _usdResult;
    Node*  _sceneRoot{nullptr};
    float  _angle{0};

    // Write a USDA scene with PBR materials and multiple meshes.
    static bool writeScene(const char* path) {
        FILE* f = fopen(path, "w");
        if (!f) return false;

        // ── Header ──────────────────────────────────────────────────────────
        fputs("#usda 1.0\n(\n    upAxis = \"Y\"\n)\n\n", f);

        // ── Materials ────────────────────────────────────────────────────────
        fputs("def Scope \"Materials\" {\n", f);
        writeMaterial(f, "RedMat",    0.85f, 0.12f, 0.12f, 0.5f, 0.0f);
        writeMaterial(f, "GreenMat",  0.12f, 0.75f, 0.20f, 0.6f, 0.0f);
        writeMaterial(f, "BlueMat",   0.12f, 0.30f, 0.90f, 0.3f, 0.5f);
        writeMaterial(f, "GoldMat",   1.00f, 0.75f, 0.15f, 0.2f, 0.9f);
        writeMaterial(f, "GroundMat", 0.35f, 0.35f, 0.35f, 0.9f, 0.0f);
        fputs("}\n\n", f);

        // ── Scene root ───────────────────────────────────────────────────────
        fputs("def Xform \"Scene\" {\n", f);

        // Ground plane (a very flat scaled cube)
        writeCubeMesh(f, "Ground", "GroundMat",  0, -0.55f, 0,  8, 0.1f, 8);
        // Four colored cubes
        writeCubeMesh(f, "BoxRed",   "RedMat",   -3,  0, 0,  1, 1, 1);
        writeCubeMesh(f, "BoxGreen", "GreenMat",  0,  0, 0,  1, 1, 1);
        writeCubeMesh(f, "BoxBlue",  "BlueMat",   3,  0, 0,  1, 1, 1);
        writeCubeMesh(f, "BoxGold",  "GoldMat",   0,  1, 0,  0.6f, 0.6f, 0.6f);

        fputs("}\n", f);  // Scene
        fclose(f);
        return true;
    }

    // Emit a UsdPreviewSurface material block.
    static void writeMaterial(FILE* f, const char* name,
                               float r, float g, float b,
                               float roughness, float metallic) {
        // Material path: /Materials/<name>; Shader path: /Materials/<name>/Shader
        fprintf(f,
            "    def Material \"%s\" {\n"
            "        token outputs:surface.connect = </Materials/%s/Shader.outputs:surface>\n"
            "        def Shader \"Shader\" {\n"
            "            uniform token info:id = \"UsdPreviewSurface\"\n"
            "            color3f inputs:diffuseColor = (%g, %g, %g)\n"
            "            float inputs:roughness = %g\n"
            "            float inputs:metallic = %g\n"
            "            token outputs:surface\n"
            "        }\n"
            "    }\n",
            name, name,
            r, g, b, roughness, metallic);
    }

    // Emit a Mesh prim inside the Scene Xform, with scale and translate ops.
    static void writeCubeMesh(FILE* f, const char* primName, const char* matName,
                               float tx, float ty, float tz,
                               float sx, float sy, float sz) {
        fprintf(f,
            "    def Mesh \"%s\" {\n"
            "        rel material:binding = </Materials/%s>\n"
            "        float3 xformOp:translate = (%g, %g, %g)\n"
            "        float3 xformOp:scale = (%g, %g, %g)\n"
            "        uniform token[] xformOpOrder = [\"xformOp:translate\", \"xformOp:scale\"]\n",
            primName, matName, tx, ty, tz, sx, sy, sz);
        fputs("        ", f); fputs(k_cube_fv_counts,  f);
        fputs("        ", f); fputs(k_cube_fv_indices, f);
        fputs("        ", f); fputs(k_cube_points,     f);
        fputs("        ", f); fputs(k_cube_normals,    f);
        fputs("    }\n", f);
    }
};
REGISTER_TEST("USD", "USD PBR Scene", TestUSDScene);

#endif // USE_TINYUSDZ
