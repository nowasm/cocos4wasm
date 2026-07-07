// P8 — Physics framework demo.
//
// A static ground slab (BoxCollider WITHOUT RigidBody = static geometry)
// plus six dynamic bodies (boxes + spheres) dropped from varying heights;
// they fall under gravity, land on the slab and stack/settle. Press SPACE
// to spawn another sphere from above.
//
// The scene logs each dynamic body's world-Y at ~1s intervals so the
// fall → settle behaviour is verifiable from stdout.

#include "SceneRegistry.h"

#include <cstdio>

#include "3d/framework/MeshRendererComponent.h"
#include "base/Log.h"
#include "cocos/game/CocosGame.h"
#include "core/component/NodeActivator.h"
#include "engine/EngineEvents.h"
#include "physics/framework/BoxCollider.h"
#include "physics/framework/RigidBody.h"
#include "physics/framework/SphereCollider.h"

using namespace cc;

class PhysicsScene : public DemoScene {
public:
    const char *name() const override { return "P8 — Physics"; }

    void onEnter(scene::RenderScene * /*rs*/, Root * /*root*/) override {
        _sceneRoot = ccnew Node("physics-root");
        _sceneRoot->addRef();

        makeGround();

        // Six dynamic bodies from varying heights: a loose stack of boxes
        // in the middle, spheres off to the sides.
        makeDynamicBox("box-0", Vec3(-0.10F, 1.5F, 0.0F), Color(220, 80, 60, 255));
        makeDynamicBox("box-1", Vec3(0.10F, 3.0F, 0.05F), Color(230, 150, 60, 255));
        makeDynamicBox("box-2", Vec3(-0.05F, 4.5F, -0.05F), Color(240, 210, 80, 255));
        makeDynamicSphere("ball-0", Vec3(-2.2F, 2.0F, 0.0F), Color(80, 190, 110, 255));
        makeDynamicSphere("ball-1", Vec3(2.2F, 3.5F, 0.2F), Color(70, 150, 230, 255));
        makeDynamicSphere("ball-2", Vec3(1.6F, 5.0F, -0.2F), Color(170, 90, 220, 255));

        NodeActivator::get().activateNode(_sceneRoot, true);

        _keyListener.bind([this](const KeyboardEvent &ev) {
            if (ev.action == KeyboardEvent::Action::RELEASE &&
                ev.key == static_cast<int>(KeyCode::SPACE)) {
                spawnSphere();
            }
        });

        CC_LOG_INFO("[PhysicsScene] 1 static ground + %zu dynamic bodies (SPACE spawns a sphere)",
                    _bodies.size());
    }

    void onUpdate(float dt) override {
        _elapsed += dt;
        if (_elapsed - _lastLogTime < 1.0F) return;
        _lastLogTime = _elapsed;

        char line[512];
        int n = std::snprintf(line, sizeof(line), "[PhysicsScene] t=%4.1fs |", _elapsed);
        for (auto *node : _bodies) {
            if (n < 0 || n >= static_cast<int>(sizeof(line))) break;
            n += std::snprintf(line + n, sizeof(line) - n, " %s y=%6.2f",
                               node->getName().c_str(), node->getWorldPosition().y);
        }
        CC_LOG_INFO("%s", line);
    }

    void onExit() override {
        _keyListener.reset();
        _bodies.clear();
        if (_sceneRoot) {
            NodeActivator::get().activateNode(_sceneRoot, false);
            _sceneRoot->release();
            _sceneRoot = nullptr;
        }
    }

private:
    Node *makeBody(const char *nodeName, Mesh *mesh, Material *material) {
        auto *node = ccnew Node(nodeName);
        _sceneRoot->addChild(node);
        auto *renderer = node->addComponent<MeshRendererComponent>();
        renderer->setMaterial(material);
        renderer->setMesh(mesh);
        return node;
    }

    Material *makeMat(const Color &albedo, float roughness = 0.6F, float metallic = 0.1F) {
        game::PBRParams p;
        p.albedo = albedo;
        p.roughness = roughness;
        p.metallic = metallic;
        return game::MaterialFactory::createStandard(p);
    }

    void makeGround() {
        Node *ground = makeBody("ground",
                                game::PrimitiveFactory::createBox(12.F, 0.4F, 8.F),
                                makeMat(Color(70, 70, 78, 255), 0.95F, 0.0F));
        ground->setPosition(0.F, -2.2F, 0.F);
        // BoxCollider with no RigidBody on the node ⇒ static world geometry.
        auto *col = ground->addComponent<BoxCollider>();
        col->setSize(Vec3(12.F, 0.4F, 8.F));
    }

    void makeDynamicBox(const char *nodeName, const Vec3 &pos, const Color &color) {
        Node *node = makeBody(nodeName, game::PrimitiveFactory::createBox(0.8F, 0.8F, 0.8F),
                              makeMat(color));
        node->setPosition(pos);
        auto *col = node->addComponent<BoxCollider>();
        col->setSize(Vec3(0.8F, 0.8F, 0.8F));
        auto *rb = node->addComponent<RigidBody>();
        rb->setMass(1.F);
        _bodies.push_back(node);
    }

    void makeDynamicSphere(const char *nodeName, const Vec3 &pos, const Color &color) {
        Node *node = makeBody(nodeName, game::PrimitiveFactory::createSphere(0.4F),
                              makeMat(color, 0.35F, 0.3F));
        node->setPosition(pos);
        auto *col = node->addComponent<SphereCollider>();
        col->setRadius(0.4F);
        auto *rb = node->addComponent<RigidBody>();
        rb->setMass(1.F);
        _bodies.push_back(node);
    }

    void spawnSphere() {
        char nodeName[32];
        std::snprintf(nodeName, sizeof(nodeName), "spawn-%d", _spawnCount);
        // Deterministic pseudo-random drop column so repeated spawns pile up
        // in different spots.
        const float x = static_cast<float>((_spawnCount * 37) % 40) / 10.F - 2.F;
        ++_spawnCount;
        Node *node = makeBody(nodeName, game::PrimitiveFactory::createSphere(0.4F),
                              makeMat(Color(240, 240, 245, 255), 0.3F, 0.5F));
        node->setPosition(x, 5.5F, 0.F);
        auto *col = node->addComponent<SphereCollider>();
        col->setRadius(0.4F);
        auto *rb = node->addComponent<RigidBody>();
        rb->setMass(1.F);
        NodeActivator::get().activateNode(node, true);
        _bodies.push_back(node);
        CC_LOG_INFO("[PhysicsScene] spawned %s at x=%.1f", nodeName, x);
    }

    Node *_sceneRoot{nullptr};
    std::vector<Node *> _bodies;
    float _elapsed{0.F};
    float _lastLogTime{0.F};
    int _spawnCount{0};
    events::Keyboard::Listener _keyListener;
};

REGISTER_DEMO_SCENE("P8 — Physics", PhysicsScene);
