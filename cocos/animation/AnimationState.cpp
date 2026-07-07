#include "animation/AnimationState.h"

#include <cmath>
#include "base/Log.h"
#include "core/scene-graph/Node.h"

namespace cc {

void AnimationState::bind(AnimationClip *clip, Node *root) {
    _clip = clip;
    _root = root;
    _bindings.clear();
    _time = 0.F;
    _playing = false;
    if (!clip || !root) return;

    for (const auto &track : clip->tracks()) {
        if (!track.hasAnyCurve()) continue;
        Node *target = track.path.empty() ? root : root->getChildByPath(track.path);
        if (!target) {
            CC_LOG_WARNING("[AnimationState] clip '%s': track path '%s' not found under '%s'",
                           clip->getName().c_str(), track.path.c_str(), root->getName().c_str());
            continue;
        }
        _bindings.push_back({&track, target});
    }
}

void AnimationState::play() {
    if (!_clip) return;
    _time = 0.F;
    _playing = true;
    sample();
}

void AnimationState::stop() {
    _playing = false;
    _time = 0.F;
}

void AnimationState::setTime(float t) {
    _time = t;
    if (_clip) sample();
}

bool AnimationState::wrapTime(float duration, float rawTime, float *outTime) const {
    if (duration <= 0.F) {
        *outTime = 0.F;
        return wrapMode != AnimationWrapMode::NORMAL;
    }
    switch (wrapMode) {
        case AnimationWrapMode::NORMAL:
            if (rawTime >= duration) {
                *outTime = duration;
                return false;
            }
            *outTime = std::max(rawTime, 0.F);
            return true;
        case AnimationWrapMode::LOOP:
            *outTime = std::fmod(std::fmod(rawTime, duration) + duration, duration);
            return true;
        case AnimationWrapMode::PING_PONG: {
            const float cycle = 2.F * duration;
            float t = std::fmod(std::fmod(rawTime, cycle) + cycle, cycle);
            *outTime = t <= duration ? t : cycle - t;
            return true;
        }
    }
    *outTime = 0.F;
    return false;
}

void AnimationState::update(float dt) {
    if (!_playing || !_clip) return;
    _time += dt * _speed;
    sample();
    float wrapped = 0.F;
    if (!wrapTime(_clip->getDuration(), _time, &wrapped)) {
        // NORMAL playback ran off the end: sample() above already wrote the
        // clamped last frame; halt here.
        _playing = false;
    }
}

void AnimationState::sample() {
    if (!_clip) return;
    float t = 0.F;
    wrapTime(_clip->getDuration(), _time, &t);

    for (const auto &b : _bindings) {
        const auto &track = *b.track;
        Node *node = b.target;
        if (!track.position.empty()) node->setPosition(track.position.sample(t));
        if (!track.eulerAngles.empty()) {
            const Vec3 e = track.eulerAngles.sample(t);
            node->setRotationFromEuler(e.x, e.y, e.z);
        } else if (!track.rotation.empty()) {
            node->setRotation(track.rotation.sample(t));
        }
        if (!track.scale.empty()) node->setScale(track.scale.sample(t));
    }
}

} // namespace cc
