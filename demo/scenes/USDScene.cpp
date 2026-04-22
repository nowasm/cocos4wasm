#ifdef USE_TINYUSDZ

#include "SceneRegistry.h"
#include "cocos/game/USDLoader.h"
#include "base/Log.h"
#include <cstdio>

using namespace cc;
using namespace cc::game;

// ─── Inline USDA helpers ─────────────────────────────────────────────────────

static void writeMat(FILE* f, const char* name,
                     float r, float g, float b,
                     float roughness, float metallic) {
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
        name, name, r, g, b, roughness, metallic);
}

static void writeCube(FILE* f, const char* prim, const char* mat,
                      float tx, float ty, float tz,
                      float sx, float sy, float sz) {
    fprintf(f,
        "    def Mesh \"%s\" {\n"
        "        rel material:binding = </Materials/%s>\n"
        "        float3 xformOp:translate = (%g, %g, %g)\n"
        "        float3 xformOp:scale    = (%g, %g, %g)\n"
        "        uniform token[] xformOpOrder = [\"xformOp:translate\",\"xformOp:scale\"]\n"
        "        int[] faceVertexCounts = [4,4,4,4,4,4]\n"
        "        int[] faceVertexIndices = [0,1,2,3, 4,5,6,7, 0,4,7,3, 1,5,6,2, 0,1,5,4, 3,2,6,7]\n"
        "        point3f[] points = [\n"
        "            (-0.5,-0.5, 0.5),( 0.5,-0.5, 0.5),( 0.5, 0.5, 0.5),(-0.5, 0.5, 0.5),\n"
        "            (-0.5,-0.5,-0.5),( 0.5,-0.5,-0.5),( 0.5, 0.5,-0.5),(-0.5, 0.5,-0.5)\n"
        "        ]\n"
        "        normal3f[] normals = [\n"
        "            (0,0,1),(0,0,1),(0,0,1),(0,0,1),\n"
        "            (0,0,-1),(0,0,-1),(0,0,-1),(0,0,-1),\n"
        "            (-1,0,0),(-1,0,0),(-1,0,0),(-1,0,0),\n"
        "            (1,0,0),(1,0,0),(1,0,0),(1,0,0),\n"
        "            (0,-1,0),(0,-1,0),(0,-1,0),(0,-1,0),\n"
        "            (0,1,0),(0,1,0),(0,1,0),(0,1,0)\n"
        "        ] (interpolation = \"faceVarying\")\n"
        "    }\n",
        prim, mat, tx, ty, tz, sx, sy, sz);
}

static bool writeScene(const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) return false;

    fputs("#usda 1.0\n(\n    upAxis = \"Y\"\n)\n\n", f);

    fputs("def Scope \"Materials\" {\n", f);
    writeMat(f, "Red",    0.85f, 0.12f, 0.12f, 0.5f,  0.0f);
    writeMat(f, "Green",  0.12f, 0.75f, 0.20f, 0.6f,  0.0f);
    writeMat(f, "Blue",   0.12f, 0.30f, 0.90f, 0.3f,  0.5f);
    writeMat(f, "Gold",   1.00f, 0.75f, 0.15f, 0.2f,  0.9f);
    writeMat(f, "Ground", 0.30f, 0.30f, 0.30f, 0.95f, 0.0f);
    fputs("}\n\n", f);

    fputs("def Xform \"Scene\" {\n", f);
    // Ground slab
    writeCube(f, "Ground", "Ground",  0, -1.05f, 0,  7, 0.1f, 4);
    // Three cubes in a row
    writeCube(f, "BoxRed",   "Red",   -2.5f, -0.5f, 0,  1, 1, 1);
    writeCube(f, "BoxGreen", "Green",  0,    -0.5f, 0,  1, 1, 1);
    writeCube(f, "BoxBlue",  "Blue",   2.5f, -0.5f, 0,  1, 1, 1);
    // Gold box on top of green
    writeCube(f, "BoxGold",  "Gold",   0,     0.8f, 0,  0.6f, 0.6f, 0.6f);
    fputs("}\n", f);

    fclose(f);
    return true;
}

// ─── USDScene ─────────────────────────────────────────────────────────────────

class USDScene : public DemoScene {
public:
    const char* name() const override { return "USD — PBR Scene"; }

    void onEnter(scene::RenderScene*, Root*) override {
        const char* path = "usd_demo_scene.usda";
        if (!writeScene(path)) {
            CC_LOG_ERROR("[USDScene] failed to write inline USDA");
            return;
        }

        _result = USDLoader::load(path, nullptr);
        if (!_result.success) {
            CC_LOG_ERROR("[USDScene] load failed: %s", _result.error.c_str());
            return;
        }

        CC_LOG_INFO("[USDScene] loaded: %zu mesh(es), %zu material(s)",
                    _result.meshes.size(), _result.materials.size());
    }

    void onUpdate(float dt) override {
        if (!_result.success || !_result.rootNode) return;
        _angle += dt * 25.f;
        _result.rootNode->setRotationFromEuler(0, _angle, 0);
        for (auto* r : _result.renderers) r->updateTransform();
    }

    void onExit() override {
        _result.destroy();
        _angle = 0;
        CC_LOG_INFO("[USDScene] exit");
    }

private:
    USDLoadResult _result;
    float         _angle{0};
};

REGISTER_DEMO_SCENE("USD — PBR Scene", USDScene);

#endif // USE_TINYUSDZ
