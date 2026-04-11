#include "../TestScene.h"

using namespace cc;
using namespace cc::game;

// ---- Single Box ----
class TestBox : public TestScene {
public:
    std::string name() override { return "Box"; }
    std::string category() override { return "Shapes"; }
    void setup(scene::RenderScene *rs, Root *root) override {
        auto *node = createNode("Box");
        node->setScale(Vec3(3, 3, 3));
        auto *mat = MaterialFactory::createStandard(Color(30, 200, 60, 255));
        addMeshRenderer(node, PrimitiveFactory::createBox(), mat);
    }
};
REGISTER_TEST("Shapes", "Box", TestBox);

// ---- Sphere ----
class TestSphere : public TestScene {
public:
    std::string name() override { return "Sphere"; }
    std::string category() override { return "Shapes"; }
    void setup(scene::RenderScene *rs, Root *root) override {
        auto *node = createNode("Sphere");
        node->setScale(Vec3(4, 4, 4));
        auto *mat = MaterialFactory::createStandard(Color(60, 120, 220, 255));
        addMeshRenderer(node, PrimitiveFactory::createSphere(0.5f, 32), mat);
    }
};
REGISTER_TEST("Shapes", "Sphere", TestSphere);

// ---- All Shapes ----
class TestMultiShapes : public TestScene {
    float _time{0};
public:
    std::string name() override { return "All Shapes"; }
    std::string category() override { return "Shapes"; }
    void setup(scene::RenderScene *rs, Root *root) override {
        struct ShapeInfo { const char *name; Mesh *mesh; Color color; Vec3 pos; };
        ShapeInfo shapes[] = {
            {"Box",      PrimitiveFactory::createBox(),                Color(220, 60, 60, 255),   Vec3(-6, 0, 0)},
            {"Sphere",   PrimitiveFactory::createSphere(0.5f, 32),    Color(60, 200, 60, 255),   Vec3(-3, 0, 0)},
            {"Cylinder", PrimitiveFactory::createCylinder(),           Color(60, 60, 220, 255),   Vec3(0, 0, 0)},
            {"Cone",     PrimitiveFactory::createCone(),               Color(220, 200, 40, 255),  Vec3(3, 0, 0)},
            {"Torus",    PrimitiveFactory::createTorus(),              Color(200, 60, 200, 255),  Vec3(6, 0, 0)},
            {"Plane",    PrimitiveFactory::createPlane(2, 2),          Color(100, 200, 200, 255), Vec3(0, -2, 0)},
        };
        for (auto &s : shapes) {
            auto *n = createNode(s.name);
            n->setPosition(s.pos);
            n->setScale(Vec3(1.5f, 1.5f, 1.5f));
            addMeshRenderer(n, s.mesh, MaterialFactory::createStandard(s.color));
        }
    }
    void update(float dt) override {
        _time += dt;
        int i = 0;
        for (auto *n : _nodes) {
            n->setRotationFromEuler(0, _time * 30.0f + i * 60.0f, 0);
            ++i;
        }
    }
};
REGISTER_TEST("Shapes", "All Shapes", TestMultiShapes);
