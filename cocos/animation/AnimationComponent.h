/****************************************************************************
 AnimationComponent — the authoring-layer `cc.Animation` component (P6).

 Mirrors the TS API shape: a set of named clips, an optional default clip,
 playOnLoad, and play()/pause()/resume()/stop()/getState(). Playback drives
 node transforms through AnimationState on the component's own node as the
 binding root, ticked from ComponentScheduler::update.

 Clips are added programmatically today (addClip). The reflected fields are
 limited to the scalars (`playOnLoad`) until AnimationClip becomes a real
 loadable asset — at that point `_clips` / `_defaultClip` join the
 serialization surface the same way Sprite's SpriteFrame reference did.
****************************************************************************/

#pragma once

#include <memory>
#include "animation/AnimationState.h"
#include "core/component/Component.h"
#include "core/reflection/Reflection.h"

namespace cc {

class AnimationComponent : public Component {
    CC_CLASS_DECL(AnimationComponent, Component)
public:
    AnimationComponent() { _wantsUpdate = true; }
    ~AnimationComponent() override = default;

    // Registers a clip under clip->getName() (or `name` when non-empty).
    // The first clip added becomes the default when none is set.
    void addClip(AnimationClip *clip, const ccstd::string &name = "");
    AnimationClip *getClip(const ccstd::string &name) const;

    void setDefaultClipName(const ccstd::string &name) { _defaultClipName = name; }
    const ccstd::string &getDefaultClipName() const { return _defaultClipName; }

    bool playOnLoad{false};

    // Play `name`, or the default clip when empty. Rebinds lazily so clips
    // added before the node had its final children still resolve.
    AnimationState *play(const ccstd::string &name = "");
    void pause();
    void resume();
    void stop();

    // Playback state for a clip (nullptr if the clip was never played).
    AnimationState *getState(const ccstd::string &name);

    void onLoad() override;
    void update(float dt) override;

    static int forceLink();

private:
    struct Entry {
        ccstd::string name;
        IntrusivePtr<AnimationClip> clip;
        std::unique_ptr<AnimationState> state; // created on first play
    };
    Entry *findEntry(const ccstd::string &name);

    ccstd::vector<Entry> _entries;
    ccstd::string _defaultClipName;
    AnimationState *_current{nullptr};
};

} // namespace cc
