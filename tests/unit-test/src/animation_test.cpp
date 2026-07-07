// P6 animation chain — Track sampling, clip duration, AnimationState wrap
// modes and node binding, AnimationComponent playback control.

#include <cmath>
#include "animation/AnimationComponent.h"
#include "animation/AnimationCurve.h"
#include "animation/AnimationState.h"
#include "base/Ptr.h"
#include "core/scene-graph/Node.h"
#include "math/Math.h"
#include "gtest/gtest.h"

using namespace cc;

// ─── Track sampling ────────────────────────────────────────────────────────

TEST(AnimationCurve, linearSamplingAndClamp) {
    anim::FloatTrack t;
    t.addKey(1.F, 10.F);
    t.addKey(3.F, 30.F);

    EXPECT_FLOAT_EQ(t.sample(0.F), 10.F);   // clamp before first key
    EXPECT_FLOAT_EQ(t.sample(1.F), 10.F);
    EXPECT_FLOAT_EQ(t.sample(2.F), 20.F);   // midpoint
    EXPECT_FLOAT_EQ(t.sample(3.F), 30.F);
    EXPECT_FLOAT_EQ(t.sample(99.F), 30.F);  // clamp after last key
    EXPECT_FLOAT_EQ(t.duration(), 3.F);
}

TEST(AnimationCurve, stepSampling) {
    anim::FloatTrack t;
    t.interpolation = anim::Interpolation::STEP;
    t.addKey(0.F, 1.F);
    t.addKey(1.F, 2.F);

    EXPECT_FLOAT_EQ(t.sample(0.99F), 1.F);
    EXPECT_FLOAT_EQ(t.sample(1.F), 2.F);
}

TEST(AnimationCurve, outOfOrderInsertionIsSorted) {
    anim::FloatTrack t;
    t.addKey(2.F, 20.F);
    t.addKey(0.F, 0.F);
    t.addKey(1.F, 10.F);

    EXPECT_FLOAT_EQ(t.sample(0.5F), 5.F);
    EXPECT_FLOAT_EQ(t.sample(1.5F), 15.F);
}

TEST(AnimationCurve, vec3AndQuatInterpolate) {
    anim::Vec3Track v;
    v.addKey(0.F, Vec3(0.F, 0.F, 0.F));
    v.addKey(2.F, Vec3(2.F, 4.F, 6.F));
    const Vec3 mid = v.sample(1.F);
    EXPECT_FLOAT_EQ(mid.x, 1.F);
    EXPECT_FLOAT_EQ(mid.y, 2.F);
    EXPECT_FLOAT_EQ(mid.z, 3.F);

    // 0° → 90° about Y; halfway must be 45°.
    Quaternion a;
    Quaternion::createFromAxisAngle(Vec3(0.F, 1.F, 0.F), 0.F, &a);
    Quaternion b;
    Quaternion::createFromAxisAngle(Vec3(0.F, 1.F, 0.F), math::PI / 2.F, &b);
    anim::QuatTrack q;
    q.addKey(0.F, a);
    q.addKey(1.F, b);
    Quaternion expected;
    Quaternion::createFromAxisAngle(Vec3(0.F, 1.F, 0.F), math::PI / 4.F, &expected);
    const Quaternion mid2 = q.sample(0.5F);
    EXPECT_NEAR(std::abs(mid2.x * expected.x + mid2.y * expected.y +
                         mid2.z * expected.z + mid2.w * expected.w),
                1.F, 1e-5F);
}

// ─── Clip ──────────────────────────────────────────────────────────────────

TEST(AnimationClip, durationFromTracksOrExplicit) {
    IntrusivePtr<AnimationClip> clip = ccnew AnimationClip("c");
    auto &root = clip->track("");
    root.position.addKey(0.F, Vec3());
    root.position.addKey(1.5F, Vec3(1.F, 0.F, 0.F));
    auto &child = clip->track("child");
    child.scale.addKey(2.5F, Vec3(2.F, 2.F, 2.F));

    EXPECT_FLOAT_EQ(clip->getDuration(), 2.5F);
    clip->setDuration(4.F);
    EXPECT_FLOAT_EQ(clip->getDuration(), 4.F);

    // track() returns the same NodeTrack for the same path
    EXPECT_EQ(&clip->track("child"), &child);
    EXPECT_EQ(clip->tracks().size(), 2u);
}

// ─── State: binding + wrap modes ───────────────────────────────────────────

namespace {
struct TreeFixture {
    IntrusivePtr<Node> root;
    Node *child{nullptr};

    TreeFixture() {
        root = ccnew Node("root");
        child = ccnew Node("child");
        root->addChild(child);
    }
};
} // namespace

TEST(AnimationState, bindsRootAndChildPath) {
    TreeFixture f;
    IntrusivePtr<AnimationClip> clip = ccnew AnimationClip("move");
    auto &rt = clip->track("");
    rt.position.addKey(0.F, Vec3(0.F, 0.F, 0.F));
    rt.position.addKey(1.F, Vec3(10.F, 0.F, 0.F));
    auto &ct = clip->track("child");
    ct.position.addKey(0.F, Vec3(0.F, 0.F, 0.F));
    ct.position.addKey(1.F, Vec3(0.F, 6.F, 0.F));

    AnimationState state(clip, f.root);
    state.play();
    state.update(0.5F);

    EXPECT_NEAR(f.root->getPosition().x, 5.F, 1e-5F);
    EXPECT_NEAR(f.child->getPosition().y, 3.F, 1e-5F);
}

TEST(AnimationState, unresolvablePathIsSkipped) {
    TreeFixture f;
    IntrusivePtr<AnimationClip> clip = ccnew AnimationClip("bad");
    auto &missing = clip->track("no/such/node");
    missing.position.addKey(0.F, Vec3());
    missing.position.addKey(1.F, Vec3(1.F, 1.F, 1.F));
    auto &ok = clip->track("");
    ok.position.addKey(0.F, Vec3());
    ok.position.addKey(1.F, Vec3(2.F, 0.F, 0.F));

    AnimationState state(clip, f.root);
    state.play();
    state.update(1.F); // must not crash; root track still applies
    EXPECT_NEAR(f.root->getPosition().x, 2.F, 1e-5F);
}

TEST(AnimationState, normalStopsAtEnd) {
    TreeFixture f;
    IntrusivePtr<AnimationClip> clip = ccnew AnimationClip("once");
    auto &t = clip->track("");
    t.position.addKey(0.F, Vec3(0.F, 0.F, 0.F));
    t.position.addKey(1.F, Vec3(4.F, 0.F, 0.F));

    AnimationState state(clip, f.root);
    state.wrapMode = AnimationWrapMode::NORMAL;
    state.play();
    state.update(2.F);

    EXPECT_FALSE(state.isPlaying());
    EXPECT_NEAR(f.root->getPosition().x, 4.F, 1e-5F); // clamped last frame
}

TEST(AnimationState, loopWraps) {
    TreeFixture f;
    IntrusivePtr<AnimationClip> clip = ccnew AnimationClip("loop");
    auto &t = clip->track("");
    t.position.addKey(0.F, Vec3(0.F, 0.F, 0.F));
    t.position.addKey(1.F, Vec3(4.F, 0.F, 0.F));

    AnimationState state(clip, f.root);
    state.wrapMode = AnimationWrapMode::LOOP;
    state.play();
    state.update(1.25F); // wraps to t=0.25

    EXPECT_TRUE(state.isPlaying());
    EXPECT_NEAR(f.root->getPosition().x, 1.F, 1e-5F);
}

TEST(AnimationState, pingPongReflects) {
    TreeFixture f;
    IntrusivePtr<AnimationClip> clip = ccnew AnimationClip("pp");
    auto &t = clip->track("");
    t.position.addKey(0.F, Vec3(0.F, 0.F, 0.F));
    t.position.addKey(1.F, Vec3(4.F, 0.F, 0.F));

    AnimationState state(clip, f.root);
    state.wrapMode = AnimationWrapMode::PING_PONG;
    state.play();
    state.update(1.5F); // reflected: effective t=0.5

    EXPECT_TRUE(state.isPlaying());
    EXPECT_NEAR(f.root->getPosition().x, 2.F, 1e-5F);
}

TEST(AnimationState, speedScalesTime) {
    TreeFixture f;
    IntrusivePtr<AnimationClip> clip = ccnew AnimationClip("fast");
    auto &t = clip->track("");
    t.position.addKey(0.F, Vec3(0.F, 0.F, 0.F));
    t.position.addKey(2.F, Vec3(8.F, 0.F, 0.F));

    AnimationState state(clip, f.root);
    state.setSpeed(2.F);
    state.play();
    state.update(0.5F); // effective t = 1.0

    EXPECT_NEAR(f.root->getPosition().x, 4.F, 1e-5F);
}

TEST(AnimationState, eulerTrackDrivesRotation) {
    TreeFixture f;
    IntrusivePtr<AnimationClip> clip = ccnew AnimationClip("spin");
    auto &t = clip->track("");
    t.eulerAngles.addKey(0.F, Vec3(0.F, 0.F, 0.F));
    t.eulerAngles.addKey(1.F, Vec3(0.F, 90.F, 0.F));

    AnimationState state(clip, f.root);
    state.play();
    state.update(0.5F);

    EXPECT_NEAR(f.root->getEulerAngles().y, 45.F, 1e-3F);
}

// ─── Component ─────────────────────────────────────────────────────────────

TEST(AnimationComponent, playPauseResumeStop) {
    TreeFixture f;
    auto *comp = f.root->addComponent<AnimationComponent>();

    IntrusivePtr<AnimationClip> clip = ccnew AnimationClip("walk");
    auto &t = clip->track("");
    t.position.addKey(0.F, Vec3(0.F, 0.F, 0.F));
    t.position.addKey(1.F, Vec3(10.F, 0.F, 0.F));
    comp->addClip(clip);

    EXPECT_EQ(comp->getClip("walk"), clip.get());
    EXPECT_EQ(comp->getDefaultClipName(), "walk"); // first clip becomes default

    AnimationState *state = comp->play();
    ASSERT_NE(state, nullptr);
    state->wrapMode = AnimationWrapMode::LOOP;

    comp->update(0.25F);
    EXPECT_NEAR(f.root->getPosition().x, 2.5F, 1e-5F);

    comp->pause();
    comp->update(0.25F); // paused — no movement
    EXPECT_NEAR(f.root->getPosition().x, 2.5F, 1e-5F);

    comp->resume();
    comp->update(0.25F);
    EXPECT_NEAR(f.root->getPosition().x, 5.F, 1e-5F);

    comp->stop();
    EXPECT_FALSE(state->isPlaying());
    EXPECT_EQ(comp->getState("walk"), state);
    EXPECT_EQ(comp->getState("nope"), nullptr);
}

TEST(AnimationComponent, playUnknownClipReturnsNull) {
    TreeFixture f;
    auto *comp = f.root->addComponent<AnimationComponent>();
    EXPECT_EQ(comp->play("missing"), nullptr);
}
