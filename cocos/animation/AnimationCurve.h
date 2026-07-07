/****************************************************************************
 Minimal C++ keyframe tracks for the P6 animation chain.

 A Track<T> is a time-sorted keyframe list with LINEAR or STEP sampling.
 Vec3 lerps componentwise, Quaternion uses shortest-arc slerp. This is the
 evaluation kernel shared by node-transform clips today and (later) the
 skeletal path; it deliberately has no notion of targets — AnimationClip
 owns the "which node / which property" mapping.
****************************************************************************/

#pragma once

#include <algorithm>
#include "base/Assertf.h"
#include "base/std/container/vector.h"
#include "math/Quaternion.h"
#include "math/Vec3.h"

namespace cc::anim {

enum class Interpolation {
    LINEAR,
    STEP,
};

template <typename T>
struct Keyframe {
    float time{0.F};
    T     value{};
};

namespace detail {
inline float interpolate(const float &a, const float &b, float t) {
    return a + (b - a) * t;
}
inline Vec3 interpolate(const Vec3 &a, const Vec3 &b, float t) {
    return Vec3{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
}
inline Quaternion interpolate(const Quaternion &a, const Quaternion &b, float t) {
    Quaternion out;
    Quaternion::slerp(a, b, t, &out);
    return out;
}
} // namespace detail

template <typename T>
class Track {
public:
    // Keys must be appended in non-decreasing time order; addKey enforces
    // ordering by insertion sort so callers may be sloppy.
    void addKey(float time, const T &value) {
        Keyframe<T> key{time, value};
        auto it = std::upper_bound(_keys.begin(), _keys.end(), time,
                                   [](float t, const Keyframe<T> &k) { return t < k.time; });
        _keys.insert(it, key);
    }

    void clear() { _keys.clear(); }
    bool empty() const { return _keys.empty(); }
    size_t size() const { return _keys.size(); }
    const ccstd::vector<Keyframe<T>> &keys() const { return _keys; }

    float duration() const { return _keys.empty() ? 0.F : _keys.back().time; }

    Interpolation interpolation{Interpolation::LINEAR};

    // Sample at absolute clip time. Clamps outside the key range.
    T sample(float time) const {
        CC_ASSERT(!_keys.empty());
        if (time <= _keys.front().time) return _keys.front().value;
        if (time >= _keys.back().time) return _keys.back().value;

        auto it = std::upper_bound(_keys.begin(), _keys.end(), time,
                                   [](float t, const Keyframe<T> &k) { return t < k.time; });
        const Keyframe<T> &next = *it;
        const Keyframe<T> &prev = *(it - 1);
        if (interpolation == Interpolation::STEP) return prev.value;

        const float span = next.time - prev.time;
        const float t = span > 0.F ? (time - prev.time) / span : 0.F;
        return detail::interpolate(prev.value, next.value, t);
    }

private:
    ccstd::vector<Keyframe<T>> _keys;
};

using FloatTrack = Track<float>;
using Vec3Track = Track<Vec3>;
using QuatTrack = Track<Quaternion>;

} // namespace cc::anim
