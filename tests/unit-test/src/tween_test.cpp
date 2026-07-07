// P7 tween chain — step interpolation, overshoot carry-over, by/repeat
// accumulation, delay, call ordering, and TweenSystem lifecycle. Driven by
// manual TweenSystem::update(dt) calls with fixed dt; no engine bootstrap.

#include "base/Ptr.h"
#include "core/scene-graph/Node.h"
#include "tween/Tween.h"
#include "tween/TweenSystem.h"
#include "gtest/gtest.h"

using namespace cc;

namespace {

TweenSystem &sys() { return TweenSystem::getInstance(); }

void tick(float dt, int times = 1) {
    for (int i = 0; i < times; ++i) {
        sys().update(dt);
    }
}

class TweenTest : public ::testing::Test {
protected:
    void SetUp() override { sys().stopAll(); }
    void TearDown() override { sys().stopAll(); }
};

} // namespace

TEST_F(TweenTest, linearToMidpoint) {
    IntrusivePtr<Node> n = ccnew Node("t");
    Tween::create(n)->to(1.F, TweenProps().setPosition(Vec3(10.F, 20.F, 30.F))).start();

    tick(0.5F);
    const Vec3 &p = n->getPosition();
    EXPECT_FLOAT_EQ(p.x, 5.F);
    EXPECT_FLOAT_EQ(p.y, 10.F);
    EXPECT_FLOAT_EQ(p.z, 15.F);

    tick(0.5F);
    EXPECT_FLOAT_EQ(n->getPosition().x, 10.F);
    EXPECT_EQ(sys().runningCount(), 0U); // released on completion
}

TEST_F(TweenTest, multiChannelSingleStep) {
    IntrusivePtr<Node> n = ccnew Node("t");
    Tween::create(n)
        ->to(1.F, TweenProps()
                      .setPosition(Vec3(8.F, 0.F, 0.F))
                      .setScale(Vec3(3.F, 3.F, 3.F)))
        .start();

    tick(0.5F);
    EXPECT_FLOAT_EQ(n->getPosition().x, 4.F);
    EXPECT_FLOAT_EQ(n->getScale().x, 2.F); // scale starts at 1
    EXPECT_FLOAT_EQ(n->getScale().y, 2.F);
    // Euler channel untouched (flag not set).
    EXPECT_FLOAT_EQ(n->getEulerAngles().y, 0.F);
}

TEST_F(TweenTest, overshootCarriesIntoNextStep) {
    IntrusivePtr<Node> n = ccnew Node("t");
    Tween::create(n)
        ->to(0.4F, TweenProps().setPosition(Vec3(4.F, 0.F, 0.F)))
        .to(0.6F, TweenProps().setPosition(Vec3(10.F, 0.F, 0.F)))
        .start();

    // dt = 0.5 finishes step 1 (0.4s) and advances 0.1s into step 2:
    // x = 4 + (10 - 4) * (0.1 / 0.6) = 5.
    tick(0.5F);
    EXPECT_NEAR(n->getPosition().x, 5.F, 1e-5F);

    tick(0.5F);
    EXPECT_FLOAT_EQ(n->getPosition().x, 10.F);
    EXPECT_EQ(sys().runningCount(), 0U);
}

TEST_F(TweenTest, byAccumulatesAcrossRepeat) {
    IntrusivePtr<Node> n = ccnew Node("t");
    Tween::create(n)
        ->by(1.F, TweenProps().setPosition(Vec3(1.F, 0.F, 0.F)))
        .repeat(2)
        .start();

    tick(1.F);
    EXPECT_FLOAT_EQ(n->getPosition().x, 1.F); // cycle 1 done
    tick(1.F);
    EXPECT_FLOAT_EQ(n->getPosition().x, 2.F); // by re-resolved: accumulates
    EXPECT_EQ(sys().runningCount(), 0U);

    tick(1.F);
    EXPECT_FLOAT_EQ(n->getPosition().x, 2.F); // stays put after completion
}

TEST_F(TweenTest, delayDefersMovement) {
    IntrusivePtr<Node> n = ccnew Node("t");
    Tween::create(n)
        ->delay(0.5F)
        .to(0.5F, TweenProps().setPosition(Vec3(1.F, 0.F, 0.F)))
        .start();

    tick(0.4F);
    EXPECT_FLOAT_EQ(n->getPosition().x, 0.F); // still delayed

    tick(0.4F); // 0.3s into the move
    EXPECT_NEAR(n->getPosition().x, 0.6F, 1e-5F);

    tick(0.4F);
    EXPECT_FLOAT_EQ(n->getPosition().x, 1.F);
}

TEST_F(TweenTest, callFiresOnceInSequenceOrder) {
    IntrusivePtr<Node> n = ccnew Node("t");
    int calls = 0;
    float xAtCall = -1.F;
    Tween::create(n)
        ->to(0.5F, TweenProps().setPosition(Vec3(2.F, 0.F, 0.F)))
        .call([&]() {
            ++calls;
            xAtCall = n->getPosition().x;
        })
        .to(0.5F, TweenProps().setPosition(Vec3(4.F, 0.F, 0.F)))
        .start();

    tick(0.25F);
    EXPECT_EQ(calls, 0); // first step not finished yet

    tick(0.5F); // finishes step 1, fires call, advances into step 2
    EXPECT_EQ(calls, 1);
    EXPECT_FLOAT_EQ(xAtCall, 2.F); // fired after step 1 wrote its end value

    tick(1.F);
    EXPECT_EQ(calls, 1); // never fires again
    EXPECT_FLOAT_EQ(n->getPosition().x, 4.F);
}

TEST_F(TweenTest, repeatForeverKeepsRunning) {
    IntrusivePtr<Node> n = ccnew Node("t");
    Tween::create(n)
        ->by(0.1F, TweenProps().setPosition(Vec3(1.F, 0.F, 0.F)))
        .repeatForever()
        .start();

    tick(0.25F, 100);
    EXPECT_EQ(sys().runningCount(), 1U);
    EXPECT_GT(n->getPosition().x, 200.F); // 100 * 0.25s / 0.1s = 250 cycles

    sys().stopAll();
    EXPECT_EQ(sys().runningCount(), 0U);
}

TEST_F(TweenTest, stopRemovesFromSystem) {
    IntrusivePtr<Node> n = ccnew Node("t");
    IntrusivePtr<Tween> tw = Tween::create(n);
    tw->to(1.F, TweenProps().setPosition(Vec3(1.F, 0.F, 0.F)));
    tw->start();
    EXPECT_TRUE(tw->isRunning());
    EXPECT_EQ(sys().runningCount(), 1U);

    tick(0.25F);
    tw->stop();
    EXPECT_FALSE(tw->isRunning());
    EXPECT_EQ(sys().runningCount(), 0U);

    const float x = n->getPosition().x;
    tick(1.F);
    EXPECT_FLOAT_EQ(n->getPosition().x, x); // no further writes
}

TEST_F(TweenTest, startTweenFromInsideCallback) {
    IntrusivePtr<Node> a = ccnew Node("a");
    IntrusivePtr<Node> b = ccnew Node("b");

    Tween::create(a)
        ->to(0.5F, TweenProps().setPosition(Vec3(1.F, 0.F, 0.F)))
        .call([&]() {
            Tween::create(b)->to(1.F, TweenProps().setPosition(Vec3(10.F, 0.F, 0.F))).start();
        })
        .start();

    tick(0.5F); // finishes A's move, callback starts B mid-update
    EXPECT_EQ(sys().runningCount(), 1U); // A done+released, B registered
    EXPECT_FLOAT_EQ(b->getPosition().x, 0.F); // B ticks from the next update

    tick(0.5F);
    EXPECT_FLOAT_EQ(b->getPosition().x, 5.F);
    tick(0.5F);
    EXPECT_FLOAT_EQ(b->getPosition().x, 10.F);
    EXPECT_EQ(sys().runningCount(), 0U);
}
