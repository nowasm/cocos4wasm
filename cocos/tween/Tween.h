/****************************************************************************
 Tween — minimal C++ port of the TS tween() builder (P7).

 A Tween is a sequential list of steps (to / by / delay / call) against one
 Node. Builders only record steps; start() registers the tween with
 TweenSystem, which addRefs it and drives update(dt). A to/by step captures
 the node's CURRENT transform as its start value when the step begins, so
 by-steps re-resolved on repeat accumulate. Overshoot within one update
 carries into the following step. The system releases the tween when the
 sequence (times repeat budget) completes or stop() is called; the tween
 keeps its target alive via IntrusivePtr while it lives.
****************************************************************************/

#pragma once

#include <cstdint>
#include <functional>
#include "base/Ptr.h"
#include "base/RefCounted.h"
#include "base/std/container/vector.h"
#include "math/Vec3.h"
#include "tween/Easing.h"

namespace cc {

class Node;

// Absolute (to) or relative (by) channel targets. Flags mark which channels
// a step writes; unflagged channels are left untouched.
struct TweenProps {
    Vec3 position;
    bool hasPosition{false};
    Vec3 eulerAngles;
    bool hasEuler{false};
    Vec3 scale;
    bool hasScale{false};

    TweenProps &setPosition(const Vec3 &v) {
        position = v;
        hasPosition = true;
        return *this;
    }
    TweenProps &setEulerAngles(const Vec3 &v) {
        eulerAngles = v;
        hasEuler = true;
        return *this;
    }
    TweenProps &setScale(const Vec3 &v) {
        scale = v;
        hasScale = true;
        return *this;
    }
};

class Tween : public RefCounted {
public:
    // ccnew'd with refcount 0; NOT auto-started. Hold it in an IntrusivePtr
    // if you need to touch it after stop()/completion.
    static Tween *create(Node *target);

    ~Tween() override;

    Tween &to(float duration, const TweenProps &props, TweenEasing easing = TweenEasing::LINEAR);
    Tween &by(float duration, const TweenProps &props, TweenEasing easing = TweenEasing::LINEAR);
    Tween &delay(float seconds);
    Tween &call(std::function<void()> cb);

    // Whole-sequence repeat count (default 1 = play once).
    Tween &repeat(int32_t times);
    Tween &repeatForever();

    void start(); // registers with TweenSystem (which addRefs)
    void stop();  // unregisters (system releases its ref)
    bool isRunning() const { return _running; }

private:
    friend class TweenSystem;

    enum class StepKind {
        TO,
        BY,
        DELAY,
        CALL,
    };

    struct Step {
        StepKind kind{StepKind::DELAY};
        float duration{0.F};
        TweenProps props;
        TweenEasing easing{TweenEasing::LINEAR};
        std::function<void()> cb;
    };

    explicit Tween(Node *target);

    // Advances the sequence; returns true when the repeat budget is spent.
    // Returns false immediately if a call() callback stopped this tween.
    bool update(float dt);

    void enterStep(size_t index);
    void resolve(const Step &step); // capture start values, compute targets
    void apply(const Step &step, float k);

    IntrusivePtr<Node> _target;
    ccstd::vector<Step> _steps;
    int32_t _repeat{1}; // -1 = forever
    int32_t _cycles{0};
    size_t _stepIndex{0};
    float _stepTime{0.F};
    bool _stepResolved{false};
    bool _running{false};

    // Resolved endpoints of the current to/by step.
    Vec3 _fromPos, _toPos;
    Vec3 _fromEuler, _toEuler;
    Vec3 _fromScale, _toScale;
};

} // namespace cc
