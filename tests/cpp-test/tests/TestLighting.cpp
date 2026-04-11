#include "../TestScene.h"

using namespace cc;
using namespace cc::game;
using namespace cc::scene;

// ---- Directional Light Rotating ----
class TestDirectionalLight : public TestScene {
    Node *_lightNode{nullptr};
    float _time{0};
public:
    std::string name() override { return "Directional Light"; }
    std::string category() override { return "Lighting"; }
    void setup(RenderScene *rs, Root *root) override {
        // Ground plane
        auto *ground = createNode("Ground");
        ground->setPosition(Vec3(0, -1.5f, 0));
        ground->setScale(Vec3(3, 3, 3));
        addMeshRenderer(ground, PrimitiveFactory::createPlane(4, 4),
                        MaterialFactory::createStandard(Color(180, 180, 180, 255)));

        // Central sphere
        auto *sphere = createNode("Sphere");
        sphere->setScale(Vec3(2.5f, 2.5f, 2.5f));
        addMeshRenderer(sphere, PrimitiveFactory::createSphere(0.5f, 32),
                        MaterialFactory::createStandard(Color(200, 200, 200, 255)));

        // Rotating directional light
        _lightNode = createNode("DirLight");
        auto *light = root->createLight<DirectionalLight>();
        light->setNode(_lightNode);
        light->setColor(Vec3(1, 0.95f, 0.8f));
        light->setIlluminance(100000.0f);
        rs->setMainLight(light);
    }
    void update(float dt) override {
        _time += dt;
        if (_lightNode) {
            _lightNode->setRotationFromEuler(-45.0f, _time * 40.0f, 0);
        }
    }
};
REGISTER_TEST("Lighting", "Directional Light", TestDirectionalLight);

// ---- Multiple Lights ----
class TestMultipleLights : public TestScene {
    float _time{0};
public:
    std::string name() override { return "Multiple Lights"; }
    std::string category() override { return "Lighting"; }
    void setup(RenderScene *rs, Root *root) override {
        // Ground
        auto *ground = createNode("Ground");
        ground->setPosition(Vec3(0, -1.5f, 0));
        ground->setScale(Vec3(3, 3, 3));
        addMeshRenderer(ground, PrimitiveFactory::createPlane(6, 6),
                        MaterialFactory::createStandard(Color(160, 160, 160, 255)));

        // Several objects
        struct ObjInfo { const char *name; Vec3 pos; Color color; };
        ObjInfo objs[] = {
            {"Box1",   Vec3(-3, 0, -2), Color(220, 80, 80, 255)},
            {"Sphere", Vec3(0, 0, 0),    Color(80, 220, 80, 255)},
            {"Box2",   Vec3(3, 0, -2),  Color(80, 80, 220, 255)},
            {"Cyl",    Vec3(0, 0, 3),    Color(220, 220, 80, 255)},
        };
        Mesh *meshes[] = {
            PrimitiveFactory::createBox(),
            PrimitiveFactory::createSphere(0.5f, 32),
            PrimitiveFactory::createBox(),
            PrimitiveFactory::createCylinder(),
        };
        for (int i = 0; i < 4; ++i) {
            auto *n = createNode(objs[i].name);
            n->setPosition(objs[i].pos);
            n->setScale(Vec3(1.5f, 1.5f, 1.5f));
            addMeshRenderer(n, meshes[i], MaterialFactory::createStandard(objs[i].color));
        }

        // Directional (sun)
        auto *sunNode = createNode("Sun");
        sunNode->setRotationFromEuler(-50.0f, 30.0f, 0);
        auto *sun = root->createLight<DirectionalLight>();
        sun->setNode(sunNode);
        sun->setColor(Vec3(1, 0.95f, 0.9f));
        sun->setIlluminance(80000.0f);
        rs->setMainLight(sun);

        // Sphere light (point-like)
        auto *ptNode = createNode("PointLight");
        ptNode->setPosition(Vec3(0, 3, 0));
        auto *ptLight = root->createLight<SphereLight>();
        ptLight->setNode(ptNode);
        ptLight->setColor(Vec3(0.2f, 0.5f, 1.0f));
        ptLight->setLuminance(500000.0f);
        ptLight->setRange(15.0f);
        ptLight->setSize(0.1f);
        rs->addSphereLight(ptLight);

        // Spot light
        auto *spotNode = createNode("SpotLight");
        spotNode->setPosition(Vec3(4, 5, 4));
        spotNode->lookAt(Vec3(0, 0, 0));
        auto *spotLight = root->createLight<SpotLight>();
        spotLight->setNode(spotNode);
        spotLight->setColor(Vec3(1.0f, 0.4f, 0.1f));
        spotLight->setLuminance(500000.0f);
        spotLight->setRange(20.0f);
        spotLight->setSize(0.1f);
        spotLight->setSpotAngle(0.6f);
        rs->addSpotLight(spotLight);
    }
    void update(float dt) override {
        _time += dt;
    }
};
REGISTER_TEST("Lighting", "Multiple Lights", TestMultipleLights);
