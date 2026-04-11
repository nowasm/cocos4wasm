#include "../TestScene.h"
#include "game/TextureLoader.h"
#include "platform/FileUtils.h"
#include "profiler/DebugRenderer.h"

using namespace cc;
using namespace cc::game;

// Helper: try load a texture, return nullptr on failure or if GPU texture invalid
static Texture2D *tryLoad(const ccstd::string &path) {
    auto *fu = FileUtils::getInstance();
    auto full = fu->fullPathForFilename(path);
    if (full.empty() || !fu->isFileExist(full)) return nullptr;
    auto *tex = TextureLoader::loadFromFile(full);
    if (tex && !tex->getGFXTexture()) {
        CC_LOG_WARNING("tryLoad: GPU texture failed for %s", path.c_str());
        return nullptr;
    }
    return tex;
}

// ---- Cubemap Textures ----
class TestCubemapTextures : public TestScene {
    struct TexResult { ccstd::string name; Texture2D *tex{nullptr}; };
    ccstd::vector<TexResult> _results;
public:
    std::string name() override { return "Cubemap Textures"; }
    std::string category() override { return "Builtin Assets"; }
    void setup(scene::RenderScene *rs, Root *root) override {
        const char *faces[] = {"front", "back", "left", "right", "top", "bottom"};
        for (auto *face : faces) {
            ccstd::string path = ccstd::string("default_cubemap/") + face + ".jpg";
            auto *tex = tryLoad(path);
            _results.push_back({path, tex});
        }

        // Display loaded textures on quads
        float x = -7.0f;
        for (auto &r : _results) {
            if (!r.tex) continue;
            auto *node = createNode(r.name);
            node->setPosition(Vec3(x, 0, 0));
            node->setScale(Vec3(2.5f, 2.5f, 2.5f));
            auto *mat = MaterialFactory::createStandardTextured(r.tex);
            addMeshRenderer(node, PrimitiveFactory::createQuad(), mat);
            x += 3.0f;
        }
    }
    void update(float dt) override {
        auto *dr = DebugRenderer::getInstance();
        if (!dr) return;

        DebugTextInfo title;
        title.color = {1, 1, 0, 1};
        title.bold = true;
        title.scale = 1.5f;
        dr->addText("Cubemap Face Textures (JPG)", Vec2(40, 40), title);

        float y = 70.0f;
        for (auto &r : _results) {
            DebugTextInfo info;
            info.scale = 1.0f;
            info.color = r.tex ? gfx::Color{0, 1, 0, 1} : gfx::Color{1, 0, 0, 1};
            ccstd::string status = r.name + (r.tex ? " [OK]" : " [FAILED]");
            dr->addText(status, Vec2(40, y), info);
            y += 20.0f;
        }
    }
};
REGISTER_TEST("Builtin Assets", "Cubemap Textures", TestCubemapTextures);

// ---- UI Textures ----
class TestUITextures : public TestScene {
    struct TexResult { ccstd::string name; Texture2D *tex{nullptr}; };
    ccstd::vector<TexResult> _results;
public:
    std::string name() override { return "UI Textures"; }
    std::string category() override { return "Builtin Assets"; }
    void setup(scene::RenderScene *rs, Root *root) override {
        const char *files[] = {
            "default_ui/default_sprite.png",
            "default_ui/default_sprite_splash.png",
            "default_ui/default_btn_normal.png",
            "default_ui/default_btn_pressed.png",
            "default_ui/default_btn_disabled.png",
            "default_ui/default_editbox_bg.png",
            "default_ui/default_panel.png",
            "default_ui/default_progressbar.png",
            "default_ui/default_progressbar_bg.png",
            "default_ui/default_scrollbar.png",
            "default_ui/default_toggle_normal.png",
            "default_ui/default_toggle_checkmark.png",
            "default_ui/default_radio_button_on.png",
            "default_ui/default_radio_button_off.png",
        };

        for (auto *f : files) {
            auto *tex = tryLoad(f);
            _results.push_back({f, tex});
        }

        // Show loaded textures as textured quads in a grid
        int col = 0, row = 0;
        for (auto &r : _results) {
            if (!r.tex) continue;
            auto *node = createNode(r.name);
            node->setPosition(Vec3(-5.0f + col * 2.5f, 3.0f - row * 2.5f, 0));
            node->setScale(Vec3(1.0f, 1.0f, 1.0f));
            auto *mat = MaterialFactory::createUnlitTextured(r.tex);
            addMeshRenderer(node, PrimitiveFactory::createQuad(), mat);
            col++;
            if (col >= 5) { col = 0; row++; }
        }
    }
    void update(float dt) override {
        auto *dr = DebugRenderer::getInstance();
        if (!dr) return;

        DebugTextInfo title;
        title.color = {1, 1, 0, 1};
        title.bold = true;
        title.scale = 1.5f;
        dr->addText("Default UI Textures (PNG)", Vec2(40, 40), title);

        float y = 70.0f;
        int ok = 0, fail = 0;
        for (auto &r : _results) {
            if (r.tex) ok++; else fail++;
        }
        DebugTextInfo info;
        info.scale = 1.2f;
        info.color = {0.8f, 0.8f, 0.8f, 1};
        char buf[128];
        snprintf(buf, sizeof(buf), "Loaded: %d / %d  |  Failed: %d", ok, ok + fail, fail);
        dr->addText(buf, Vec2(40, y), info);
    }
};
REGISTER_TEST("Builtin Assets", "UI Textures", TestUITextures);

// ---- Skybox & Misc Textures ----
class TestMiscTextures : public TestScene {
    struct TexResult { ccstd::string name; Texture2D *tex{nullptr}; };
    ccstd::vector<TexResult> _results;
public:
    std::string name() override { return "Skybox & Misc"; }
    std::string category() override { return "Builtin Assets"; }
    void setup(scene::RenderScene *rs, Root *root) override {
        const char *files[] = {
            "default_skybox/default_skybox.png",
            "Default-Particle.png",
            "default-terrain/default-layer-texture.jpg",
            "default_ui/atom.png",
            "gizmo/camera.png",
            "gizmo/directional-light.png",
            "gizmo/sphere-light.png",
            "gizmo/spot-light.png",
            "gizmo/particle-system.png",
            "dependencies/textures/lut/original-color-grading.png",
            "dependencies/textures/preintegrated-skin.png",
            "default_fonts/builtin-bitmap/OpenSans-Regular_0.png",
        };

        for (auto *f : files) {
            auto *tex = tryLoad(f);
            _results.push_back({f, tex});
        }

        int col = 0, row = 0;
        for (auto &r : _results) {
            if (!r.tex) continue;
            auto *node = createNode(r.name);
            node->setPosition(Vec3(-6.0f + col * 3.0f, 3.0f - row * 3.0f, 0));
            node->setScale(Vec3(1.3f, 1.3f, 1.3f));
            auto *mat = MaterialFactory::createUnlitTextured(r.tex);
            addMeshRenderer(node, PrimitiveFactory::createQuad(), mat);
            col++;
            if (col >= 5) { col = 0; row++; }
        }
    }
    void update(float dt) override {
        auto *dr = DebugRenderer::getInstance();
        if (!dr) return;

        DebugTextInfo title;
        title.color = {1, 1, 0, 1};
        title.bold = true;
        title.scale = 1.5f;
        dr->addText("Skybox, Gizmo & Misc Textures", Vec2(40, 40), title);

        float y = 70.0f;
        for (auto &r : _results) {
            DebugTextInfo info;
            info.scale = 0.9f;
            info.color = r.tex ? gfx::Color{0.5f, 1, 0.5f, 1} : gfx::Color{1, 0.3f, 0.3f, 1};
            ccstd::string s = (r.tex ? "[OK] " : "[FAIL] ") + r.name;
            dr->addText(s, Vec2(40, y), info);
            y += 16.0f;
        }
    }
};
REGISTER_TEST("Builtin Assets", "Skybox & Misc", TestMiscTextures);

// ---- Full Asset Inventory ----
class TestAssetInventory : public TestScene {
    struct Entry { ccstd::string path; bool loaded{false}; };
    ccstd::vector<Entry> _entries;
public:
    std::string name() override { return "Full Inventory"; }
    std::string category() override { return "Builtin Assets"; }
    void setup(scene::RenderScene *rs, Root *root) override {
        auto *fu = FileUtils::getInstance();

        // All loadable image files from editor/assets
        const char *files[] = {
            // cubemap
            "default_cubemap/front.jpg", "default_cubemap/back.jpg",
            "default_cubemap/left.jpg", "default_cubemap/right.jpg",
            "default_cubemap/top.jpg", "default_cubemap/bottom.jpg",
            // skybox
            "default_skybox/default_skybox.png",
            // terrain
            "default-terrain/default-layer-texture.jpg",
            // particle
            "Default-Particle.png",
            // fonts bitmap
            "default_fonts/builtin-bitmap/OpenSans-Regular_0.png",
            "default_fonts/builtin-bitmap/OpenSans-Bold_0.png",
            "default_fonts/builtin-bitmap/OpenSans-Italic_0.png",
            "default_fonts/builtin-bitmap/OpenSans-BoldItalic_0.png",
            // fonts ttf
            "fonts/OpenSans-Regular.ttf",
            "fonts/OpenSans-Bold.ttf",
            "fonts/OpenSans-Italic.ttf",
            "fonts/OpenSans-BoldItalic.ttf",
            // UI
            "default_ui/default_sprite.png", "default_ui/default_sprite_splash.png",
            "default_ui/default_btn_normal.png", "default_ui/default_btn_pressed.png",
            "default_ui/default_btn_disabled.png", "default_ui/default_editbox_bg.png",
            "default_ui/default_panel.png",
            "default_ui/default_progressbar.png", "default_ui/default_progressbar_bg.png",
            "default_ui/default_scrollbar.png", "default_ui/default_scrollbar_bg.png",
            "default_ui/default_scrollbar_vertical.png", "default_ui/default_scrollbar_vertical_bg.png",
            "default_ui/default_toggle_normal.png", "default_ui/default_toggle_pressed.png",
            "default_ui/default_toggle_disabled.png", "default_ui/default_toggle_checkmark.png",
            "default_ui/default_radio_button_on.png", "default_ui/default_radio_button_off.png",
            "default_ui/atom.png",
            // gizmo
            "gizmo/camera.png", "gizmo/directional-light.png",
            "gizmo/sphere-light.png", "gizmo/spot-light.png",
            "gizmo/particle-system.png", "gizmo/light-probe.png",
            "gizmo/reflection-probe.png", "gizmo/webview.png",
            "gizmo/x.png", "gizmo/y.png", "gizmo/z.png",
            // dependencies
            "dependencies/textures/lut/original-color-grading.png",
            "dependencies/textures/preintegrated-skin.png",
            // effects json
            "builtin-effects.json",
        };

        for (auto *f : files) {
            ccstd::string path(f);
            auto full = fu->fullPathForFilename(path);
            bool exists = !full.empty() && fu->isFileExist(full);
            _entries.push_back({path, exists});
        }
    }
    void update(float dt) override {
        auto *dr = DebugRenderer::getInstance();
        if (!dr) return;

        int total = static_cast<int>(_entries.size());
        int loaded = 0;
        for (auto &e : _entries) if (e.loaded) loaded++;

        DebugTextInfo title;
        title.color = {1, 1, 0, 1};
        title.bold = true;
        title.scale = 1.5f;
        char buf[128];
        snprintf(buf, sizeof(buf), "Asset Inventory: %d / %d found", loaded, total);
        dr->addText(buf, Vec2(40, 40), title);

        float y = 70.0f;
        for (auto &e : _entries) {
            DebugTextInfo info;
            info.scale = 0.85f;
            info.color = e.loaded ? gfx::Color{0.4f, 1, 0.4f, 1} : gfx::Color{1, 0.3f, 0.3f, 1};
            ccstd::string s = (e.loaded ? "[OK]   " : "[MISS] ") + e.path;
            dr->addText(s, Vec2(40, y), info);
            y += 15.0f;
            if (y > 700) break; // avoid overflow
        }
    }
};
REGISTER_TEST("Builtin Assets", "Full Inventory", TestAssetInventory);
