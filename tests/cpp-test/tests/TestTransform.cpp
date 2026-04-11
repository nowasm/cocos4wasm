#include "../TestScene.h"

using namespace cc;
using namespace cc::game;

// ---- Parent-Child Hierarchy ----
class TestHierarchy : public TestScene {
    Node *_parent{nullptr};
    float _time{0};
public:
    std::string name() override { return "Parent-Child"; }
    std::string category() override { return "Transform"; }
    void setup(scene::RenderScene *rs, Root *root) override {
        // Parent: large red box at center
        _parent = createNode("Parent");
        _parent->setScale(Vec3(2, 2, 2));
        addMeshRenderer(_parent, PrimitiveFactory::createBox(),
                        MaterialFactory::createStandard(Color(200, 60, 60, 255)));

        // Child 1: small green sphere orbiting
        auto *child1 = createNode("Child1");
        child1->setPosition(Vec3(3, 0, 0));
        child1->setScale(Vec3(0.5f, 0.5f, 0.5f));
        _parent->addChild(child1);
        addMeshRenderer(child1, PrimitiveFactory::createSphere(0.5f, 24),
                        MaterialFactory::createStandard(Color(60, 200, 60, 255)));

        // Child 2: small blue box orbiting at different axis
        auto *child2 = createNode("Child2");
        child2->setPosition(Vec3(0, 0, 3));
        child2->setScale(Vec3(0.4f, 0.4f, 0.4f));
        _parent->addChild(child2);
        addMeshRenderer(child2, PrimitiveFactory::createBox(),
                        MaterialFactory::createStandard(Color(60, 60, 200, 255)));

        // Grandchild: tiny yellow sphere orbiting child1
        auto *grand = createNode("Grandchild");
        grand->setPosition(Vec3(2, 0, 0));
        grand->setScale(Vec3(0.5f, 0.5f, 0.5f));
        child1->addChild(grand);
        addMeshRenderer(grand, PrimitiveFactory::createSphere(0.5f, 16),
                        MaterialFactory::createStandard(Color(240, 220, 40, 255)));
    }
    void update(float dt) override {
        _time += dt;
        if (_parent) {
            _parent->setRotationFromEuler(0, _time * 50.0f, _time * 15.0f);
        }
    }
};
REGISTER_TEST("Transform", "Parent-Child", TestHierarchy);

// ---- Scale & Rotate ----
class TestScaleRotate : public TestScene {
    float _time{0};
public:
    std::string name() override { return "Scale & Rotate"; }
    std::string category() override { return "Transform"; }
    void setup(scene::RenderScene *rs, Root *root) override {
        Color colors[] = {
            Color(220, 60, 60, 255),
            Color(60, 220, 60, 255),
            Color(60, 60, 220, 255),
            Color(220, 220, 60, 255),
        };
        for (int i = 0; i < 4; ++i) {
            auto *n = createNode("Obj_" + std::to_string(i));
            float angle = i * 90.0f * 3.14159f / 180.0f;
            n->setPosition(Vec3(cosf(angle) * 4, 0, sinf(angle) * 4));
            addMeshRenderer(n, PrimitiveFactory::createBox(),
                            MaterialFactory::createStandard(colors[i]));
        }
    }
    void update(float dt) override {
        _time += dt;
        int i = 0;
        for (auto *n : _nodes) {
            float s = 1.0f + 0.5f * sinf(_time * 2.0f + i * 1.5f);
            n->setScale(Vec3(s, s, s));
            n->setRotationFromEuler(_time * 60.0f * (i + 1), _time * 40.0f, 0);
            ++i;
        }
    }
};
REGISTER_TEST("Transform", "Scale & Rotate", TestScaleRotate);
