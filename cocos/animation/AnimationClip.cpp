#include "animation/AnimationClip.h"

namespace cc {

AnimationClip::NodeTrack &AnimationClip::track(const ccstd::string &path) {
    for (auto &t : _tracks) {
        if (t.path == path) return t;
    }
    _tracks.emplace_back();
    _tracks.back().path = path;
    return _tracks.back();
}

float AnimationClip::getDuration() const {
    if (_duration > 0.F) return _duration;
    float d = 0.F;
    for (const auto &t : _tracks) d = std::max(d, t.duration());
    return d;
}

} // namespace cc
