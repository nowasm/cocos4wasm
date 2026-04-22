#ifdef USE_TINYUSDZ

#include "../TestScene.h"
#include "cocos/game/USDLoader.h"
#include "base/Log.h"

using namespace cc;
using namespace cc::game;

// ─── TestUSDBasic ─────────────────────────────────────────────────────────────
// Loads "test.usda" (or .usdc/.usdz) from the working directory.
// If the file is missing the test logs a warning and skips.
class TestUSDBasic : public TestScene {
public:
    std::string name()     override { return "USD Basic Load"; }
    std::string category() override { return "USD"; }

    void setup(scene::RenderScene* /*rs*/, Root* /*root*/) override {
        _usdResult = USDLoader::load("test.usda", nullptr);
        if (!_usdResult.success) {
            CC_LOG_WARNING("TestUSDBasic: %s", _usdResult.error.c_str());
            return;
        }
        for (auto* r : _usdResult.renderers) _renderers.push_back(r);
        for (auto* n : _usdResult.nodes)     _nodes.push_back(n);
        _usdResult.renderers.clear();
        _usdResult.nodes.clear();

        CC_LOG_INFO("TestUSDBasic: loaded test.usda — %zu mesh(es), %zu material(s)",
                    _usdResult.meshes.size(), _usdResult.materials.size());
    }

    void cleanup() override {
        _usdResult.destroy();
        TestScene::cleanup();
    }

private:
    USDLoadResult _usdResult;
};
REGISTER_TEST("USD", "USD Basic Load", TestUSDBasic);

// ─── TestUSDInline ────────────────────────────────────────────────────────────
// Writes a minimal inline USDA cube to disk, loads it, and renders it.
// Validates the full parse → mesh → material → render pipeline.
class TestUSDInline : public TestScene {
public:
    std::string name()     override { return "USD Inline Cube"; }
    std::string category() override { return "USD"; }

    void setup(scene::RenderScene* /*rs*/, Root* /*root*/) override {
        const char* path = "usd_test_cube.usda";
        if (!writeCube(path)) {
            CC_LOG_ERROR("TestUSDInline: could not write temp file");
            return;
        }
        _usdResult = USDLoader::load(path, nullptr);
        if (!_usdResult.success) {
            CC_LOG_ERROR("TestUSDInline: %s", _usdResult.error.c_str());
            return;
        }
        for (auto* r : _usdResult.renderers) _renderers.push_back(r);
        for (auto* n : _usdResult.nodes)     _nodes.push_back(n);
        _usdResult.renderers.clear();
        _usdResult.nodes.clear();

        CC_LOG_INFO("TestUSDInline: cube loaded — %zu mesh(es)", _usdResult.meshes.size());
    }

    void cleanup() override {
        _usdResult.destroy();
        TestScene::cleanup();
    }

private:
    USDLoadResult _usdResult;

    static bool writeCube(const char* path) {
        FILE* f = fopen(path, "w");
        if (!f) return false;
        fputs(R"usda(#usda 1.0

def Xform "Root" {
    def Mesh "Cube" {
        int[] faceVertexCounts = [4, 4, 4, 4, 4, 4]
        int[] faceVertexIndices = [
            0,1,2,3,  4,5,6,7,  0,4,7,3,
            1,5,6,2,  0,1,5,4,  3,2,6,7
        ]
        point3f[] points = [
            (-0.5,-0.5, 0.5), ( 0.5,-0.5, 0.5), ( 0.5, 0.5, 0.5), (-0.5, 0.5, 0.5),
            (-0.5,-0.5,-0.5), ( 0.5,-0.5,-0.5), ( 0.5, 0.5,-0.5), (-0.5, 0.5,-0.5)
        ]
        normal3f[] normals = [
            (0,0,1),(0,0,1),(0,0,1),(0,0,1),
            (0,0,-1),(0,0,-1),(0,0,-1),(0,0,-1),
            (-1,0,0),(-1,0,0),(-1,0,0),(-1,0,0),
            (1,0,0),(1,0,0),(1,0,0),(1,0,0),
            (0,-1,0),(0,-1,0),(0,-1,0),(0,-1,0),
            (0,1,0),(0,1,0),(0,1,0),(0,1,0)
        ] (interpolation = "faceVarying")
    }
}
)usda", f);
        fclose(f);
        return true;
    }
};
REGISTER_TEST("USD", "USD Inline Cube", TestUSDInline);

#endif // USE_TINYUSDZ
