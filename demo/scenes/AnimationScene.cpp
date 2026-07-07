// P6 — AnimationClip / AnimationState / cc.Animation component demo.
//
// Three animated bodies, one per wrap mode:
//   - orbiter (LOOP):     torus orbiting the centre via a position track on a
//                          child node + euler spin on the pivot
//   - bouncer (PING_PONG): sphere bouncing with squash on landing
//   - popper (NORMAL):     cone that scales up once on scene enter, then stops
// Press SPACE to replay the one-shot popper clip.

#include "SceneRegistry.h"

#include "animation/AnimationComponent.h"
#include "base/Log.h"
#include "cocos/game/CocosGame.h"
#include "core/component/NodeActivator.h"
#include "engine/EngineEvents.h"

using namespace cc;

namespace {

struct Body {
    Node *node{nullptr};
    game::MeshRenderer *renderer{nullptr};
};

} // namespace

class AnimationScene : public DemoScene {
public:
    const char *name() const override { return "P6 — Animation"; }

    void onEnter(scene::RenderScene * /*rs*/, Root * /*root*/) override {
        _sceneRoot = ccnew Node("anim-root");
        _sceneRoot->addRef();

        makeGround();
        makeOrbiter();
        makeBouncer();
        makePopper();

        NodeActivator::get().activateNode(_sceneRoot, true);

        // Replay the one-shot clip on SPACE.
        _keyListener.bind([this](const KeyboardEvent &ev) {
            if (ev.action == KeyboardEvent::Action::RELEASE &&
                ev.key == static_cast<int>(KeyCode::SPACE) && _popperAnim) {
                _popperAnim->play("pop");
            }
        });

        CC_LOG_INFO("[AnimationScene] LOOP orbit / PING_PONG bounce / NORMAL pop (SPACE replays)");
    }

    void onUpdate(float /*dt*/) override {
        // Animation runs inside ComponentScheduler; here we only push the
        // animated node transforms to the render models (game::MeshRenderer
        // is not yet a Component — see P6 follow-up).
        for (auto &b : _bodies) b.renderer->updateTransform();
    }

    void onExit() override {
        _keyListener.reset();
        for (auto &b : _bodies) delete b.renderer;
        _bodies.clear();
        _popperAnim = nullptr;
        if (_sceneRoot) {
            NodeActivator::get().activateNode(_sceneRoot, false);
            _sceneRoot->release();
            _sceneRoot = nullptr;
        }
    }

private:
    Body &makeBody(const char *nodeName, Mesh *mesh, Material *material, Node *parent = nullptr) {
        auto *node = ccnew Node(nodeName);
        (parent ? parent : _sceneRoot)->addChild(node);
        auto *renderer = ccnew game::MeshRenderer(node);
        renderer->setMesh(mesh);
        renderer->setMaterial(material);
        _bodies.push_back({node, renderer});
        return _bodies.back();
    }

    void makeGround() {
        game::PBRParams p;
        p.albedo = Color(70, 70, 78, 255);
        p.roughness = 0.95F;
        auto &ground = makeBody("ground", game::PrimitiveFactory::createBox(9.F, 0.2F, 5.F),
                                game::MaterialFactory::createStandard(p));
        ground.node->setPosition(0.F, -1.2F, 0.F);
    }

    void makeOrbiter() {
        // Pivot spins (euler track, LOOP); torus rides on a child offset node,
        // exercising the child-path binding ("carrier").
        auto *pivot = ccnew Node("orbit-pivot");
        _sceneRoot->addChild(pivot);
        auto *carrier = ccnew Node("carrier");
        pivot->addChild(carrier);

        game::PBRParams p;
        p.albedo = Color(220, 60, 60, 255);
        p.roughness = 0.35F;
        p.metallic = 0.6F;
        auto &torus = makeBody("torus", game::PrimitiveFactory::createTorus(0.45F, 0.14F),
                               game::MaterialFactory::createStandard(p), carrier);
        (void)torus;

        auto *clip = ccnew AnimationClip("orbit");
        auto &pivotTrack = clip->track("");
        pivotTrack.eulerAngles.addKey(0.F, Vec3(0.F, 0.F, 0.F));
        pivotTrack.eulerAngles.addKey(4.F, Vec3(0.F, 360.F, 0.F));
        auto &carrierTrack = clip->track("carrier");
        carrierTrack.position.addKey(0.F, Vec3(2.6F, 0.2F, 0.F));
        carrierTrack.position.addKey(2.F, Vec3(2.6F, 0.9F, 0.F));
        carrierTrack.position.addKey(4.F, Vec3(2.6F, 0.2F, 0.F));

        auto *animComp = pivot->addComponent<AnimationComponent>();
        animComp->addClip(clip);
        auto *state = animComp->play();
        if (state) state->wrapMode = AnimationWrapMode::LOOP;
    }

    void makeBouncer() {
        game::PBRParams p;
        p.albedo = Color(70, 190, 90, 255);
        p.roughness = 0.5F;
        auto &ball = makeBody("ball", game::PrimitiveFactory::createSphere(0.5F),
                              game::MaterialFactory::createStandard(p));

        auto *clip = ccnew AnimationClip("bounce");
        auto &t = clip->track("");
        t.position.addKey(0.F, Vec3(0.F, -0.6F, 0.F));
        t.position.addKey(0.8F, Vec3(0.F, 1.4F, 0.F));
        // Squash at the bottom, round at the top.
        t.scale.addKey(0.F, Vec3(1.25F, 0.7F, 1.25F));
        t.scale.addKey(0.25F, Vec3(1.F, 1.F, 1.F));
        t.scale.addKey(0.8F, Vec3(1.F, 1.F, 1.F));

        auto *animComp = ball.node->addComponent<AnimationComponent>();
        animComp->addClip(clip);
        auto *state = animComp->play();
        if (state) state->wrapMode = AnimationWrapMode::PING_PONG;
    }

    void makePopper() {
        game::PBRParams p;
        p.albedo = Color(240, 190, 60, 255);
        p.roughness = 0.25F;
        p.metallic = 0.8F;
        auto &cone = makeBody("cone", game::PrimitiveFactory::createCone(0.5F, 1.1F),
                              game::MaterialFactory::createStandard(p));
        cone.node->setPosition(-2.6F, -0.5F, 0.F);

        auto *clip = ccnew AnimationClip("pop");
        auto &t = clip->track("");
        t.scale.addKey(0.F, Vec3(0.05F, 0.05F, 0.05F));
        t.scale.addKey(0.5F, Vec3(1.35F, 1.35F, 1.35F));
        t.scale.addKey(0.7F, Vec3(1.F, 1.F, 1.F));
        t.eulerAngles.addKey(0.F, Vec3(0.F, 0.F, 0.F));
        t.eulerAngles.addKey(0.7F, Vec3(0.F, 180.F, 0.F));

        _popperAnim = cone.node->addComponent<AnimationComponent>();
        _popperAnim->addClip(clip);
        _popperAnim->play(); // NORMAL: plays once, then AnimationState stops itself
    }

    Node *_sceneRoot{nullptr};
    ccstd::vector<Body> _bodies;
    AnimationComponent *_popperAnim{nullptr};
    events::Keyboard::Listener _keyListener;
};

REGISTER_DEMO_SCENE("P6 — Animation", AnimationScene);
