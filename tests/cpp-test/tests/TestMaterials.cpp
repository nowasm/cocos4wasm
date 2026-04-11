#include "../TestScene.h"

using namespace cc;
using namespace cc::game;

// ---- Unlit Colors Grid ----
class TestUnlitColors : public TestScene {
public:
    std::string name() override { return "Unlit Colors"; }
    std::string category() override { return "Materials"; }
    void setup(scene::RenderScene *rs, Root *root) override {
        Color colors[] = {
            Color(255, 0, 0, 255),   Color(0, 255, 0, 255),   Color(0, 0, 255, 255),
            Color(255, 255, 0, 255), Color(255, 0, 255, 255), Color(0, 255, 255, 255),
            Color(255, 128, 0, 255), Color(128, 0, 255, 255), Color(255, 255, 255, 255),
        };
        for (int i = 0; i < 9; ++i) {
            auto *n = createNode("Color_" + std::to_string(i));
            n->setPosition(Vec3((i % 3 - 1) * 3.0f, (1 - i / 3) * 3.0f, 0));
            n->setScale(Vec3(1.2f, 1.2f, 1.2f));
            addMeshRenderer(n, PrimitiveFactory::createBox(), MaterialFactory::createUnlit(colors[i]));
        }
    }
};
REGISTER_TEST("Materials", "Unlit Colors", TestUnlitColors);

// ---- PBR Color Palette ----
class TestPBRColors : public TestScene {
    float _time{0};
public:
    std::string name() override { return "PBR Color Palette"; }
    std::string category() override { return "Materials"; }
    void setup(scene::RenderScene *rs, Root *root) override {
        Color colors[] = {
            Color(200, 50, 50, 255), Color(50, 200, 50, 255), Color(50, 50, 200, 255),
            Color(200, 200, 50, 255), Color(200, 50, 200, 255),
        };
        for (int i = 0; i < 5; ++i) {
            auto *n = createNode("PBR_" + std::to_string(i));
            n->setPosition(Vec3((i - 2) * 3.0f, 0, 0));
            n->setScale(Vec3(2, 2, 2));
            addMeshRenderer(n, PrimitiveFactory::createSphere(0.5f, 32),
                            MaterialFactory::createStandard(colors[i]));
        }
    }
    void update(float dt) override {
        _time += dt;
        int i = 0;
        for (auto *n : _nodes) {
            float bounce = sinf(_time * 2.0f + i * 0.5f) * 0.5f;
            n->setPosition(Vec3((i - 2) * 3.0f, bounce, 0));
            ++i;
        }
    }
};
REGISTER_TEST("Materials", "PBR Color Palette", TestPBRColors);

// ---- Roughness Gradient ----
class TestRoughnessGradient : public TestScene {
public:
    std::string name() override { return "Roughness Gradient"; }
    std::string category() override { return "Materials"; }
    void setup(scene::RenderScene *rs, Root *root) override {
        // 7 spheres: roughness from 0 (mirror) to 1 (matte)
        constexpr int COUNT = 7;
        for (int i = 0; i < COUNT; ++i) {
            auto *n = createNode("Rough_" + std::to_string(i));
            n->setPosition(Vec3((i - COUNT / 2) * 2.5f, 0, 0));
            n->setScale(Vec3(2, 2, 2));

            PBRParams p;
            p.albedo = Color(220, 220, 220, 255);
            p.roughness = static_cast<float>(i) / (COUNT - 1);
            p.metallic = 0.0f;
            addMeshRenderer(n, PrimitiveFactory::createSphere(0.5f, 32),
                            MaterialFactory::createStandard(p));
        }
    }
};
REGISTER_TEST("Materials", "Roughness Gradient", TestRoughnessGradient);

// ---- Metallic Gradient ----
class TestMetallicGradient : public TestScene {
public:
    std::string name() override { return "Metallic Gradient"; }
    std::string category() override { return "Materials"; }
    void setup(scene::RenderScene *rs, Root *root) override {
        // 7 spheres: metallic from 0 (dielectric) to 1 (full metal)
        constexpr int COUNT = 7;
        for (int i = 0; i < COUNT; ++i) {
            auto *n = createNode("Metal_" + std::to_string(i));
            n->setPosition(Vec3((i - COUNT / 2) * 2.5f, 0, 0));
            n->setScale(Vec3(2, 2, 2));

            PBRParams p;
            p.albedo = Color(200, 180, 160, 255);
            p.roughness = 0.3f;
            p.metallic = static_cast<float>(i) / (COUNT - 1);
            addMeshRenderer(n, PrimitiveFactory::createSphere(0.5f, 32),
                            MaterialFactory::createStandard(p));
        }
    }
};
REGISTER_TEST("Materials", "Metallic Gradient", TestMetallicGradient);

// ---- Roughness x Metallic Grid ----
class TestRoughnessMetallicGrid : public TestScene {
public:
    std::string name() override { return "Roughness x Metallic"; }
    std::string category() override { return "Materials"; }
    void setup(scene::RenderScene *rs, Root *root) override {
        // 5x5 grid: X = roughness, Y = metallic
        constexpr int N = 5;
        for (int row = 0; row < N; ++row) {
            for (int col = 0; col < N; ++col) {
                auto *n = createNode("RM_" + std::to_string(row) + "_" + std::to_string(col));
                n->setPosition(Vec3((col - N / 2) * 2.2f, (row - N / 2) * 2.2f, 0));
                n->setScale(Vec3(1.0f, 1.0f, 1.0f));

                PBRParams p;
                p.albedo = Color(230, 200, 170, 255);  // warm gold-ish
                p.roughness = static_cast<float>(col) / (N - 1);
                p.metallic = static_cast<float>(row) / (N - 1);
                addMeshRenderer(n, PrimitiveFactory::createSphere(0.5f, 24),
                                MaterialFactory::createStandard(p));
            }
        }
    }
};
REGISTER_TEST("Materials", "Roughness x Metallic", TestRoughnessMetallicGrid);

// ---- Emissive Materials ----
class TestEmissive : public TestScene {
    float _time{0};
public:
    std::string name() override { return "Emissive"; }
    std::string category() override { return "Materials"; }
    void setup(scene::RenderScene *rs, Root *root) override {
        // dark spheres with different emissive colors
        struct EmInfo { Color emissive; Vec3 pos; };
        EmInfo items[] = {
            {Color(255, 40, 40, 255),  Vec3(-4, 0, 0)},
            {Color(40, 255, 40, 255),  Vec3(-1.5f, 0, 0)},
            {Color(40, 40, 255, 255),  Vec3(1.5f, 0, 0)},
            {Color(255, 200, 40, 255), Vec3(4, 0, 0)},
        };
        for (auto &item : items) {
            auto *n = createNode("Emit");
            n->setPosition(item.pos);
            n->setScale(Vec3(2, 2, 2));

            PBRParams p;
            p.albedo = Color(20, 20, 20, 255);  // very dark surface
            p.roughness = 0.8f;
            p.metallic = 0.0f;
            p.emissive = item.emissive;
            addMeshRenderer(n, PrimitiveFactory::createSphere(0.5f, 32),
                            MaterialFactory::createStandard(p));
        }
    }
    void update(float dt) override {
        _time += dt;
        int i = 0;
        for (auto *n : _nodes) {
            float s = 1.8f + 0.3f * sinf(_time * 3.0f + i * 1.5f);
            n->setScale(Vec3(s, s, s));
            ++i;
        }
    }
};
REGISTER_TEST("Materials", "Emissive", TestEmissive);

// ---- Material Comparison (shapes) ----
class TestMaterialComparison : public TestScene {
    float _time{0};
public:
    std::string name() override { return "Shape Comparison"; }
    std::string category() override { return "Materials"; }
    void setup(scene::RenderScene *rs, Root *root) override {
        // Same PBR material on different shapes
        PBRParams shiny;
        shiny.albedo = Color(255, 215, 0, 255);  // gold
        shiny.roughness = 0.15f;
        shiny.metallic = 1.0f;

        struct Info { const char *name; Mesh *mesh; Vec3 pos; };
        Info items[] = {
            {"Box",      PrimitiveFactory::createBox(),              Vec3(-5, 0, 0)},
            {"Sphere",   PrimitiveFactory::createSphere(0.5f, 32),  Vec3(-2.5f, 0, 0)},
            {"Cylinder", PrimitiveFactory::createCylinder(),         Vec3(0, 0, 0)},
            {"Torus",    PrimitiveFactory::createTorus(),            Vec3(2.5f, 0, 0)},
            {"Cone",     PrimitiveFactory::createCone(),             Vec3(5, 0, 0)},
        };
        for (auto &item : items) {
            auto *n = createNode(item.name);
            n->setPosition(item.pos);
            n->setScale(Vec3(2, 2, 2));
            addMeshRenderer(n, item.mesh, MaterialFactory::createStandard(shiny));
        }
    }
    void update(float dt) override {
        _time += dt;
        int i = 0;
        for (auto *n : _nodes) {
            n->setRotationFromEuler(0, _time * 30.0f + i * 72.0f, 0);
            ++i;
        }
    }
};
REGISTER_TEST("Materials", "Shape Comparison", TestMaterialComparison);
