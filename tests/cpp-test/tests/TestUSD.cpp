#ifdef USE_TINYUSDZ

#include "../TestScene.h"
#include "cocos/game/USDLoader.h"
#include "3d/framework/SkinnedMeshRendererComponent.h"
#include "animation/AnimationState.h"
#include "base/Log.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>

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
        _usdResult.renderers.clear();
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
        _usdResult.renderers.clear();
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
        _usdResult.renderers.clear();
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
        writeCubeMesh(f, "Ground", "GroundMat",  0, -0.55f, 0,  8, 0.1f, 8,  0.35f,0.35f,0.35f);
        // Four colored cubes
        writeCubeMesh(f, "BoxRed",   "RedMat",   -3,  0, 0,  1, 1, 1,  0.85f,0.12f,0.12f);
        writeCubeMesh(f, "BoxGreen", "GreenMat",  0,  0, 0,  1, 1, 1,  0.12f,0.75f,0.20f);
        writeCubeMesh(f, "BoxBlue",  "BlueMat",   3,  0, 0,  1, 1, 1,  0.12f,0.30f,0.90f);
        writeCubeMesh(f, "BoxGold",  "GoldMat",   0,  1, 0,  0.6f, 0.6f, 0.6f,  1.00f,0.75f,0.15f);

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
                               float sx, float sy, float sz,
                               float cr = 0.18f, float cg = 0.18f, float cb = 0.18f) {
        fprintf(f,
            "    def Mesh \"%s\" {\n"
            "        rel material:binding = </Materials/%s>\n"
            "        double3 xformOp:translate = (%g, %g, %g)\n"
            "        double3 xformOp:scale = (%g, %g, %g)\n"
            "        uniform token[] xformOpOrder = [\"xformOp:translate\", \"xformOp:scale\"]\n"
            "        color3f[] primvars:displayColor = [(%g, %g, %g)] (interpolation = \"constant\")\n",
            primName, matName, tx, ty, tz, sx, sy, sz, cr, cg, cb);
        fputs("        ", f); fputs(k_cube_fv_counts,  f);
        fputs("        ", f); fputs(k_cube_fv_indices, f);
        fputs("        ", f); fputs(k_cube_points,     f);
        fputs("        ", f); fputs(k_cube_normals,    f);
        fputs("    }\n", f);
    }
};
REGISTER_TEST("USD", "USD PBR Scene", TestUSDScene);

// ─── TestUSDSkel ──────────────────────────────────────────────────────────────
// UsdSkel sanity: writes a two-joint skinned arm (SkelRoot + Skeleton +
// SkelAnimation + skinned Mesh), loads it, and checks:
//   1. load succeeds and one SkinnedMeshRendererComponent was created
//   2. the SkinningModel exists on the component
//   3. the converted AnimationClip has tracks with rotation keys
//   4. manually advancing an AnimationState changes the joint rotation
// Failures are logged with CC_LOG_ERROR so they are greppable in the run
// output; the visual harness keeps running either way.
class TestUSDSkel : public TestScene {
public:
    std::string name()     override { return "USD Skel Animation"; }
    std::string category() override { return "USD"; }

    void setup(scene::RenderScene*, Root*) override {
        const char* path = "usd_skel_test.usda";
        if (!writeSkelUSDA(path)) {
            CC_LOG_ERROR("TestUSDSkel: cannot write file");
            return;
        }

        _usdResult = USDLoader::load(path, nullptr);
        if (!_usdResult.success) {
            CC_LOG_ERROR("TestUSDSkel: FAIL load: %s", _usdResult.error.c_str());
            return;
        }

        // 1. skinned renderer created
        if (_usdResult.skinnedRenderers.size() != 1) {
            CC_LOG_ERROR("TestUSDSkel: FAIL expected 1 skinned renderer, got %zu",
                         _usdResult.skinnedRenderers.size());
            return;
        }
        auto* renderer = _usdResult.skinnedRenderers[0];

        // 2. SkinningModel exists
        if (!renderer->getModel()) {
            CC_LOG_ERROR("TestUSDSkel: FAIL SkinningModel was not created");
        }

        // 3. clip has rotation tracks
        if (_usdResult.animationClips.empty()) {
            CC_LOG_ERROR("TestUSDSkel: FAIL no AnimationClip converted");
            return;
        }
        AnimationClip* clip = _usdResult.animationClips[0].get();
        size_t rotKeys = 0;
        for (const auto& t : clip->tracks()) rotKeys += t.rotation.size();
        if (clip->tracks().empty() || rotKeys == 0) {
            CC_LOG_ERROR("TestUSDSkel: FAIL clip has %zu track(s), %zu rotation key(s)",
                         clip->tracks().size(), rotKeys);
            return;
        }

        // 4. advancing the state moves the joint
        Node* skinningRoot = renderer->getSkinningRoot();
        Node* upper = skinningRoot ? skinningRoot->getChildByPath("Base/Upper") : nullptr;
        if (!upper) {
            CC_LOG_ERROR("TestUSDSkel: FAIL joint node 'Base/Upper' not resolvable");
            return;
        }
        AnimationState state(clip, skinningRoot);
        state.play();
        state.update(0.f);                    // sample t = 0 → +45° around Z
        const Quaternion q0 = upper->getRotation();
        state.update(1.f);                    // t = 1 s (24 tc) → -45° around Z
        const Quaternion q1 = upper->getRotation();
        const float delta = std::fabs(q0.z - q1.z);
        if (delta < 0.1f) {
            CC_LOG_ERROR("TestUSDSkel: FAIL joint rotation did not change "
                         "(q0.z=%.3f q1.z=%.3f)", q0.z, q1.z);
            return;
        }

        CC_LOG_INFO("TestUSDSkel: PASS — model=%p, %zu track(s), %zu rot key(s), "
                    "joint z %.3f → %.3f",
                    static_cast<void*>(renderer->getModel()),
                    clip->tracks().size(), rotKeys, q0.z, q1.z);

        for (auto* r : _usdResult.renderers) _renderers.push_back(r);
        _usdResult.renderers.clear();
    }

    void cleanup() override { _usdResult.destroy(); TestScene::cleanup(); }

private:
    USDLoadResult _usdResult;

    static bool writeSkelUSDA(const char* path) {
        FILE* f = fopen(path, "w");
        if (!f) return false;
        fputs(
            "#usda 1.0\n"
            "(\n"
            "    upAxis = \"Y\"\n"
            "    metersPerUnit = 1\n"
            "    timeCodesPerSecond = 24\n"
            "    startTimeCode = 0\n"
            "    endTimeCode = 48\n"
            "    defaultPrim = \"Root\"\n"
            ")\n"
            "\n"
            "def Xform \"Root\"\n"
            "{\n"
            "    def SkelRoot \"Arm\"\n"
            "    {\n"
            "        def Skeleton \"Skel\" (\n"
            "            prepend apiSchemas = [\"SkelBindingAPI\"]\n"
            "        )\n"
            "        {\n"
            "            uniform token[] joints = [\"Base\", \"Base/Upper\"]\n"
            "            uniform matrix4d[] bindTransforms = [\n"
            "                ((1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (0, 0, 0, 1)),\n"
            "                ((1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (0, 1, 0, 1))\n"
            "            ]\n"
            "            uniform matrix4d[] restTransforms = [\n"
            "                ((1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (0, 0, 0, 1)),\n"
            "                ((1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (0, 1, 0, 1))\n"
            "            ]\n"
            "            rel skel:animationSource = </Root/Arm/Skel/Anim>\n"
            "\n"
            "            def SkelAnimation \"Anim\"\n"
            "            {\n"
            "                uniform token[] joints = [\"Base\", \"Base/Upper\"]\n"
            "                float3[] translations.timeSamples = {\n"
            "                    0: [(0, 0, 0), (0, 1, 0)],\n"
            "                    48: [(0, 0, 0), (0, 1, 0)]\n"
            "                }\n"
            "                quatf[] rotations.timeSamples = {\n"
            "                    0: [(1, 0, 0, 0), (0.9238795, 0, 0, 0.3826834)],\n"
            "                    24: [(1, 0, 0, 0), (0.9238795, 0, 0, -0.3826834)],\n"
            "                    48: [(1, 0, 0, 0), (0.9238795, 0, 0, 0.3826834)]\n"
            "                }\n"
            "                half3[] scales.timeSamples = {\n"
            "                    0: [(1, 1, 1), (1, 1, 1)],\n"
            "                    48: [(1, 1, 1), (1, 1, 1)]\n"
            "                }\n"
            "            }\n"
            "        }\n"
            "\n"
            "        def Mesh \"ArmMesh\" (\n"
            "            prepend apiSchemas = [\"SkelBindingAPI\"]\n"
            "        )\n"
            "        {\n"
            "            rel skel:skeleton = </Root/Arm/Skel>\n"
            "            color3f[] primvars:displayColor = [(0.85, 0.55, 0.15)] (interpolation = \"constant\")\n"
            "            int[] faceVertexCounts = [4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4]\n"
            "            int[] faceVertexIndices = [\n"
            "                0,1,2,3, 4,5,6,7, 0,4,7,3, 1,5,6,2, 0,1,5,4, 3,2,6,7,\n"
            "                8,9,10,11, 12,13,14,15, 8,12,15,11, 9,13,14,10, 8,9,13,12, 11,10,14,15\n"
            "            ]\n"
            "            point3f[] points = [\n"
            "                (-0.25, 0, 0.25), (0.25, 0, 0.25), (0.25, 1, 0.25), (-0.25, 1, 0.25),\n"
            "                (-0.25, 0, -0.25), (0.25, 0, -0.25), (0.25, 1, -0.25), (-0.25, 1, -0.25),\n"
            "                (-0.25, 1, 0.25), (0.25, 1, 0.25), (0.25, 2, 0.25), (-0.25, 2, 0.25),\n"
            "                (-0.25, 1, -0.25), (0.25, 1, -0.25), (0.25, 2, -0.25), (-0.25, 2, -0.25)\n"
            "            ]\n"
            "            int[] primvars:skel:jointIndices = [\n"
            "                0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1\n"
            "            ] (\n"
            "                elementSize = 1\n"
            "                interpolation = \"vertex\"\n"
            "            )\n"
            "            float[] primvars:skel:jointWeights = [\n"
            "                1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1\n"
            "            ] (\n"
            "                elementSize = 1\n"
            "                interpolation = \"vertex\"\n"
            "            )\n"
            "        }\n"
            "    }\n"
            "}\n",
            f);
        fclose(f);
        return true;
    }
};
REGISTER_TEST("USD", "USD Skel Animation", TestUSDSkel);

// ─── TestUSDSmoke ─────────────────────────────────────────────────────────────
// Real-world asset smoke test. Loads the file named by the COCOS_USD_FILE
// environment variable (default: usd_models/McUsd.usdz), then auto-fits the
// model into the default orbit camera by scaling/centering the root node from
// the merged world bounds of every model. PASS/FAIL is greppable in the log.
class TestUSDSmoke : public TestScene {
public:
    std::string name()     override { return "USD Real Asset Smoke"; }
    std::string category() override { return "USD"; }

    void setup(scene::RenderScene*, Root*) override {
        // Note: the pristine McUsd.usdz fails tinyusdz's strict USDA check
        // ("doubleSided must be uniform variability"); the extracted copy in
        // usd_models/McUsd/ has the attribute patched to `uniform bool`.
        const char* env = ::getenv("COCOS_USD_FILE");
        std::string path = env && env[0] ? env : "usd_models/McUsd/McUsd.usda";

        _pivot = createNode("USDSmokePivot");
        _usdResult = USDLoader::load(path, _pivot);
        if (!_usdResult.success) {
            CC_LOG_ERROR("TestUSDSmoke: FAIL load '%s': %s",
                         path.c_str(), _usdResult.error.c_str());
            return;
        }
        if (_usdResult.renderers.empty() && _usdResult.skinnedRenderers.empty()) {
            CC_LOG_ERROR("TestUSDSmoke: FAIL '%s' loaded but produced no renderers",
                         path.c_str());
            return;
        }

        _sceneRoot = _usdResult.rootNode;
        for (auto* r : _usdResult.renderers) _renderers.push_back(r);
        _usdResult.renderers.clear();

        // Merge world bounds of every model, then scale/center the root so the
        // largest dimension spans ~5 units around the origin.
        Vec3 mn(FLT_MAX, FLT_MAX, FLT_MAX), mx(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        bool any = false;
        for (auto* r : _renderers) {
            r->updateTransform();
            auto* model = r->getModel();
            if (!model || !model->getWorldBounds()) continue;
            const auto* wb = model->getWorldBounds();
            Vec3 c = wb->getCenter(), h = wb->getHalfExtents();
            mn.x = std::min(mn.x, c.x - h.x); mx.x = std::max(mx.x, c.x + h.x);
            mn.y = std::min(mn.y, c.y - h.y); mx.y = std::max(mx.y, c.y + h.y);
            mn.z = std::min(mn.z, c.z - h.z); mx.z = std::max(mx.z, c.z + h.z);
            any = true;
        }
        if (any && _sceneRoot) {
            Vec3 size = mx - mn;
            float maxDim = std::max(size.x, std::max(size.y, size.z));
            float s = maxDim > 1e-6f ? 5.f / maxDim : 1.f;
            Vec3 center = (mn + mx) * 0.5f;
            _sceneRoot->setScale(s, s, s);
            _sceneRoot->setPosition(-center.x * s, -center.y * s, -center.z * s);
            for (auto* r : _renderers) r->updateTransform();
            CC_LOG_INFO("TestUSDSmoke: bounds min(%.3f %.3f %.3f) max(%.3f %.3f %.3f) fit scale %.4f",
                        mn.x, mn.y, mn.z, mx.x, mx.y, mx.z, s);
        }

        CC_LOG_INFO("TestUSDSmoke: PASS '%s' — %zu mesh(es), %zu material(s), "
                    "%zu renderer(s), %zu skinned, %zu clip(s)",
                    path.c_str(),
                    _usdResult.meshes.size(), _usdResult.materials.size(),
                    _renderers.size(), _usdResult.skinnedRenderers.size(),
                    _usdResult.animationClips.size());
    }

    void update(float dt) override {
        _angle += dt * 20.f;
        if (!_pivot) return;
        _pivot->setRotationFromEuler(0, _angle, 0);
        for (auto* r : _renderers) r->updateTransform();
    }

    void cleanup() override {
        _usdResult.destroy();
        _sceneRoot = nullptr;
        _pivot = nullptr;
        _angle = 0;
        TestScene::cleanup();
    }

private:
    USDLoadResult _usdResult;
    Node* _sceneRoot{nullptr};
    Node* _pivot{nullptr};
    float _angle{0};
};
REGISTER_TEST("USD", "USD Real Asset Smoke", TestUSDSmoke);

#endif // USE_TINYUSDZ
