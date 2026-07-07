#include "animation/AnimationComponent.h"

#include "base/Log.h"
#include "core/scene-graph/Node.h"

namespace cc {

CC_IMPLEMENT_CLASS(AnimationComponent, "cc.Animation", Component)
    .property("playOnLoad", &AnimationComponent::playOnLoad)
CC_END_CLASS(AnimationComponent);

int AnimationComponent::forceLink() {
    return getStaticClass() != nullptr ? 1 : 0;
}

void AnimationComponent::addClip(AnimationClip *clip, const ccstd::string &name) {
    if (!clip) return;
    const ccstd::string &key = name.empty() ? clip->getName() : name;
    if (Entry *existing = findEntry(key)) {
        existing->clip = clip;
        existing->state.reset(); // stale binding — rebuilt on next play
        if (_current && _current->getClip() == nullptr) _current = nullptr;
        return;
    }
    Entry e;
    e.name = key;
    e.clip = clip;
    _entries.push_back(std::move(e));
    if (_defaultClipName.empty()) _defaultClipName = key;
}

AnimationClip *AnimationComponent::getClip(const ccstd::string &name) const {
    for (const auto &e : _entries) {
        if (e.name == name) return e.clip.get();
    }
    return nullptr;
}

AnimationComponent::Entry *AnimationComponent::findEntry(const ccstd::string &name) {
    for (auto &e : _entries) {
        if (e.name == name) return &e;
    }
    return nullptr;
}

AnimationState *AnimationComponent::play(const ccstd::string &name) {
    const ccstd::string &key = name.empty() ? _defaultClipName : name;
    Entry *entry = findEntry(key);
    if (!entry) {
        CC_LOG_WARNING("[Animation] no clip named '%s' on node '%s'",
                       key.c_str(), _node ? _node->getName().c_str() : "?");
        return nullptr;
    }
    if (!entry->state) {
        entry->state = std::make_unique<AnimationState>();
        entry->state->bind(entry->clip.get(), _node);
    }
    _current = entry->state.get();
    _current->play();
    return _current;
}

void AnimationComponent::pause() {
    if (_current) _current->pause();
}

void AnimationComponent::resume() {
    if (_current) _current->resume();
}

void AnimationComponent::stop() {
    if (_current) _current->stop();
}

AnimationState *AnimationComponent::getState(const ccstd::string &name) {
    Entry *entry = findEntry(name.empty() ? _defaultClipName : name);
    return entry ? entry->state.get() : nullptr;
}

void AnimationComponent::onLoad() {
    if (playOnLoad && !_defaultClipName.empty()) {
        play();
    }
}

void AnimationComponent::update(float dt) {
    if (_current) _current->update(dt);
}

} // namespace cc
