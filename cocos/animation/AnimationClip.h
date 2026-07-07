/****************************************************************************
 AnimationClip — reusable node-transform animation data (P6).

 A clip is a named set of NodeTracks. Each NodeTrack addresses one node by
 relative path from the playback root ("" = the root itself, "a/b" resolved
 via Node::getChildByPath) and can animate position / rotation / eulerAngles
 / scale independently. Rotation and eulerAngles are mutually exclusive per
 track (eulerAngles wins if both are present — matches how most authoring
 exports pick one representation).

 Clips are RefCounted and shared: playback state (time, wrap, speed) lives
 in AnimationState, never here.
****************************************************************************/

#pragma once

#include "animation/AnimationCurve.h"
#include "base/RefCounted.h"
#include "base/std/container/string.h"
#include "base/std/container/vector.h"

namespace cc {

class AnimationClip : public RefCounted {
public:
    struct NodeTrack {
        ccstd::string  path; // relative to playback root; "" targets the root
        anim::Vec3Track position;
        anim::QuatTrack rotation;
        anim::Vec3Track eulerAngles;
        anim::Vec3Track scale;

        bool hasAnyCurve() const {
            return !position.empty() || !rotation.empty() ||
                   !eulerAngles.empty() || !scale.empty();
        }
        float duration() const {
            float d = position.duration();
            d = std::max(d, rotation.duration());
            d = std::max(d, eulerAngles.duration());
            return std::max(d, scale.duration());
        }
    };

    explicit AnimationClip(ccstd::string name = "") : _name(std::move(name)) {}

    const ccstd::string &getName() const { return _name; }
    void setName(const ccstd::string &v) { _name = v; }

    // Returns the track for `path`, creating it on first use.
    NodeTrack &track(const ccstd::string &path);

    const ccstd::vector<NodeTrack> &tracks() const { return _tracks; }

    // Explicit duration wins when set (>0); otherwise the max key time
    // across all tracks.
    float getDuration() const;
    void  setDuration(float v) { _duration = v; }

private:
    ccstd::string           _name;
    ccstd::vector<NodeTrack> _tracks;
    float                   _duration{-1.F};
};

} // namespace cc
