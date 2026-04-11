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
            Color(255, 0, 0, 255),     // red
            Color(0, 255, 0, 255),     // green
            Color(0, 0, 255, 255),     // blue
            Color(255, 255, 0, 255),   // yellow
            Color(255, 0, 255, 255),   // magenta
            Color(0, 255, 255, 255),   // cyan
            Color(255, 128, 0, 255),   // orange
            Color(128, 0, 255, 255),   // purple
            Color(255, 255, 255, 255), // white
        };
        int count = sizeof(colors) / sizeof(colors[0]);
        for (int i = 0; i < count; ++i) {
            auto *n = createNode("Color_" + std::to_string(i));
            float x = (i % 3 - 1) * 3.0f;
            float y = (1 - i / 3) * 3.0f;
            n->setPosition(Vec3(x, y, 0));
            n->setScale(Vec3(1.2f, 1.2f, 1.2f));
            addMeshRenderer(n, PrimitiveFactory::createBox(), MaterialFactory::createUnlit(colors[i]));
        }
    }
};
REGISTER_TEST("Materials", "Unlit Colors", TestUnlitColors);

// ---- Standard PBR ----
class TestStandardPBR : public TestScene {
    float _time{0};
public:
    std::string name() override { return "Standard PBR"; }
    std::string category() override { return "Materials"; }
    void setup(scene::RenderScene *rs, Root *root) override {
        // Row of spheres with different colors under PBR lighting
        Color colors[] = {
            Color(200, 50, 50, 255),
            Color(50, 200, 50, 255),
            Color(50, 50, 200, 255),
            Color(200, 200, 50, 255),
            Color(200, 50, 200, 255),
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
            auto pos = n->getPosition();
            n->setPosition(Vec3(pos.x, bounce, pos.z));
            ++i;
        }
    }
};
REGISTER_TEST("Materials", "Standard PBR", TestStandardPBR);
