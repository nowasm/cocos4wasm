/****************************************************************************
 Tween — implementation (P7). See Tween.h for the design summary.
****************************************************************************/

#include "tween/Tween.h"

#include <utility>
#include "base/memory/Memory.h"
#include "core/scene-graph/Node.h"
#include "tween/TweenSystem.h"

namespace cc {

namespace {
inline Vec3 lerp(const Vec3 &a, const Vec3 &b, float k) {
    return {a.x + (b.x - a.x) * k,
            a.y + (b.y - a.y) * k,
            a.z + (b.z - a.z) * k};
}
} // namespace

Tween::Tween(Node *target) : _target(target) {}

Tween::~Tween() = default;

Tween *Tween::create(Node *target) {
    return ccnew Tween(target);
}

Tween &Tween::to(float duration, const TweenProps &props, TweenEasing easing) {
    Step step;
    step.kind = StepKind::TO;
    step.duration = duration;
    step.props = props;
    step.easing = easing;
    _steps.emplace_back(std::move(step));
    return *this;
}

Tween &Tween::by(float duration, const TweenProps &props, TweenEasing easing) {
    Step step;
    step.kind = StepKind::BY;
    step.duration = duration;
    step.props = props;
    step.easing = easing;
    _steps.emplace_back(std::move(step));
    return *this;
}

Tween &Tween::delay(float seconds) {
    Step step;
    step.kind = StepKind::DELAY;
    step.duration = seconds;
    _steps.emplace_back(std::move(step));
    return *this;
}

Tween &Tween::call(std::function<void()> cb) {
    Step step;
    step.kind = StepKind::CALL;
    step.cb = std::move(cb);
    _steps.emplace_back(std::move(step));
    return *this;
}

Tween &Tween::repeat(int32_t times) {
    _repeat = times;
    return *this;
}

Tween &Tween::repeatForever() {
    _repeat = -1;
    return *this;
}

void Tween::start() {
    _cycles = 0;
    enterStep(0);
    if (!_running) {
        _running = true;
        TweenSystem::getInstance().add(this);
    }
}

void Tween::stop() {
    if (!_running) {
        return;
    }
    _running = false;
    // May destroy `this` if the system held the last reference — nothing
    // touches members after this line.
    TweenSystem::getInstance().remove(this);
}

void Tween::enterStep(size_t index) {
    _stepIndex = index;
    _stepTime = 0.F;
    _stepResolved = false;
}

void Tween::resolve(const Step &step) {
    const bool relative = step.kind == StepKind::BY;
    if (step.props.hasPosition) {
        _fromPos = _target->getPosition();
        _toPos = relative ? _fromPos + step.props.position : step.props.position;
    }
    if (step.props.hasEuler) {
        _fromEuler = _target->getEulerAngles();
        _toEuler = relative ? _fromEuler + step.props.eulerAngles : step.props.eulerAngles;
    }
    if (step.props.hasScale) {
        _fromScale = _target->getScale();
        _toScale = relative ? _fromScale + step.props.scale : step.props.scale;
    }
    _stepResolved = true;
}

void Tween::apply(const Step &step, float k) {
    if (step.props.hasPosition) {
        _target->setPosition(lerp(_fromPos, _toPos, k));
    }
    if (step.props.hasEuler) {
        const Vec3 e = lerp(_fromEuler, _toEuler, k);
        _target->setRotationFromEuler(e.x, e.y, e.z);
    }
    if (step.props.hasScale) {
        _target->setScale(lerp(_fromScale, _toScale, k));
    }
}

bool Tween::update(float dt) {
    if (_steps.empty() || !_target) {
        return true;
    }
    // Guard against zero-total-duration sequences with repeatForever: if a
    // whole cycle consumed no time, run it at most once per update.
    float dtAtLastWrap = -1.F;
    for (;;) {
        if (_stepIndex >= _steps.size()) {
            ++_cycles;
            if (_repeat >= 0 && _cycles >= _repeat) {
                return true;
            }
            if (dt == dtAtLastWrap) {
                return false;
            }
            dtAtLastWrap = dt;
            enterStep(0);
        }
        const Step &step = _steps[_stepIndex];
        switch (step.kind) {
            case StepKind::CALL:
                if (step.cb) {
                    step.cb();
                }
                if (!_running) { // callback stopped this tween
                    return false;
                }
                enterStep(_stepIndex + 1);
                break;
            case StepKind::DELAY:
                _stepTime += dt;
                if (_stepTime < step.duration) {
                    return false;
                }
                dt = _stepTime - step.duration;
                enterStep(_stepIndex + 1);
                break;
            case StepKind::TO:
            case StepKind::BY:
                if (!_stepResolved) {
                    resolve(step);
                }
                _stepTime += dt;
                if (step.duration <= 0.F || _stepTime >= step.duration) {
                    apply(step, 1.F);
                    dt = step.duration > 0.F ? _stepTime - step.duration : dt;
                    enterStep(_stepIndex + 1);
                    break;
                }
                apply(step, ease(step.easing, _stepTime / step.duration));
                return false;
        }
    }
}

} // namespace cc
