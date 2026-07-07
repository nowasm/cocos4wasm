/****************************************************************************
 Easing — header-only easing curves for the tween module (P7).

 ease() maps a normalized progress t in [0, 1] onto the eased curve. All
 formulas are the standard Robert Penner variants; BACK_OUT uses the
 canonical overshoot s = 1.70158, BOUNCE_OUT the standard 4-segment fit.
****************************************************************************/

#pragma once

#include <cmath>

namespace cc {

enum class TweenEasing {
    LINEAR,
    QUAD_IN,
    QUAD_OUT,
    QUAD_IN_OUT,
    CUBIC_OUT,
    SINE_IN_OUT,
    BACK_OUT,
    BOUNCE_OUT,
};

inline float ease(TweenEasing easing, float t) {
    switch (easing) {
        case TweenEasing::LINEAR:
            return t;
        case TweenEasing::QUAD_IN:
            return t * t;
        case TweenEasing::QUAD_OUT:
            return t * (2.F - t);
        case TweenEasing::QUAD_IN_OUT:
            return t < 0.5F ? 2.F * t * t : -1.F + (4.F - 2.F * t) * t;
        case TweenEasing::CUBIC_OUT: {
            const float u = t - 1.F;
            return u * u * u + 1.F;
        }
        case TweenEasing::SINE_IN_OUT:
            return -0.5F * (std::cos(3.14159265358979323846F * t) - 1.F);
        case TweenEasing::BACK_OUT: {
            const float s = 1.70158F;
            const float u = t - 1.F;
            return u * u * ((s + 1.F) * u + s) + 1.F;
        }
        case TweenEasing::BOUNCE_OUT: {
            if (t < 1.F / 2.75F) {
                return 7.5625F * t * t;
            }
            if (t < 2.F / 2.75F) {
                t -= 1.5F / 2.75F;
                return 7.5625F * t * t + 0.75F;
            }
            if (t < 2.5F / 2.75F) {
                t -= 2.25F / 2.75F;
                return 7.5625F * t * t + 0.9375F;
            }
            t -= 2.625F / 2.75F;
            return 7.5625F * t * t + 0.984375F;
        }
    }
    return t;
}

} // namespace cc
