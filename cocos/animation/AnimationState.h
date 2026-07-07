/****************************************************************************
 AnimationState — playback of one AnimationClip against one node tree (P6).

 Owns the mutable side of playback: current time, wrap mode, speed, and the
 clip-track → Node* binding cache. bind() resolves every track path once;
 re-bind after reparenting animated nodes. update(dt) advances time, applies
 wrapping, samples each bound track and writes local TRS to the nodes.
****************************************************************************/

#pragma once

#include "animation/AnimationClip.h"
#include "base/Ptr.h"

namespace cc {

class Node;

enum class AnimationWrapMode {
    NORMAL,   // play once, clamp at the last frame, stop
    LOOP,     // wrap around
    PING_PONG // forward, then backward, repeat
};

class AnimationState {
public:
    AnimationState() = default;
    AnimationState(AnimationClip *clip, Node *root) { bind(clip, root); }

    // Resolves every track path relative to `root`. Unresolvable paths are
    // logged once and skipped (the rest of the clip still plays).
    void bind(AnimationClip *clip, Node *root);

    AnimationClip *getClip() const { return _clip.get(); }

    void play();               // (re)start from t = 0
    void stop();               // halt and rewind; leaves node transforms as-is
    void pause() { _playing = false; }
    void resume() { _playing = _clip != nullptr; }
    bool isPlaying() const { return _playing; }

    float getTime() const { return _time; }
    void  setTime(float t);

    float getSpeed() const { return _speed; }
    void  setSpeed(float v) { _speed = v; }

    AnimationWrapMode wrapMode{AnimationWrapMode::NORMAL};

    // Advance and write transforms. No-op unless playing.
    void update(float dt);

    // Evaluate the clip at the current time and write node transforms
    // regardless of the playing flag (used by setTime and the last NORMAL
    // frame).
    void sample();

private:
    struct Binding {
        const AnimationClip::NodeTrack *track{nullptr};
        Node *target{nullptr};
    };

    // Maps raw playback time into [0, duration] per wrap mode. Returns
    // false when a NORMAL-mode playback has run past the end (time clamps).
    bool wrapTime(float duration, float rawTime, float *outTime) const;

    IntrusivePtr<AnimationClip> _clip;
    Node *_root{nullptr};
    ccstd::vector<Binding> _bindings;
    float _time{0.F};
    float _speed{1.F};
    bool  _playing{false};
};

} // namespace cc
