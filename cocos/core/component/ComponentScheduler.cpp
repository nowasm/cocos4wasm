#include "ComponentScheduler.h"
#include <algorithm>
#include "Component.h"

namespace cc {

ComponentScheduler &ComponentScheduler::get() {
    static ComponentScheduler instance;
    return instance;
}

void ComponentScheduler::addUnique(ccstd::vector<Component *> &dst, Component *c) {
    if (std::find(dst.begin(), dst.end(), c) == dst.end()) {
        dst.push_back(c);
    }
}

void ComponentScheduler::removeFrom(ccstd::vector<Component *> &dst, Component *c) {
    dst.erase(std::remove(dst.begin(), dst.end(), c), dst.end());
}

void ComponentScheduler::scheduleUpdate(Component *c) {
    if (_iterDepth > 0) {
        addUnique(_pendingAdd, c);
        removeFrom(_pendingRemove, c);
    } else {
        addUnique(_updatable, c);
    }
}

void ComponentScheduler::unscheduleUpdate(Component *c) {
    if (_iterDepth > 0) {
        addUnique(_pendingRemove, c);
        removeFrom(_pendingAdd, c);
    } else {
        removeFrom(_updatable, c);
    }
}

void ComponentScheduler::scheduleLateUpdate(Component *c) {
    if (_iterDepth > 0) {
        addUnique(_pendingAddLate, c);
        removeFrom(_pendingRemoveLate, c);
    } else {
        addUnique(_lateUpdatable, c);
    }
}

void ComponentScheduler::unscheduleLateUpdate(Component *c) {
    if (_iterDepth > 0) {
        addUnique(_pendingRemoveLate, c);
        removeFrom(_pendingAddLate, c);
    } else {
        removeFrom(_lateUpdatable, c);
    }
}

void ComponentScheduler::applyPending() {
    for (auto *c : _pendingAdd)    addUnique(_updatable, c);
    for (auto *c : _pendingRemove) removeFrom(_updatable, c);
    for (auto *c : _pendingAddLate)    addUnique(_lateUpdatable, c);
    for (auto *c : _pendingRemoveLate) removeFrom(_lateUpdatable, c);
    _pendingAdd.clear();
    _pendingRemove.clear();
    _pendingAddLate.clear();
    _pendingRemoveLate.clear();
}

void ComponentScheduler::update(float dt) {
    ++_iterDepth;
    // snapshot size — new entries during iteration go to pending
    const size_t n = _updatable.size();
    for (size_t i = 0; i < n; ++i) {
        Component *c = _updatable[i];
        if (c && c->isEnabledInHierarchy()) {
            c->update(dt);
        }
    }
    --_iterDepth;
    if (_iterDepth == 0) applyPending();
}

void ComponentScheduler::lateUpdate(float dt) {
    ++_iterDepth;
    const size_t n = _lateUpdatable.size();
    for (size_t i = 0; i < n; ++i) {
        Component *c = _lateUpdatable[i];
        if (c && c->isEnabledInHierarchy()) {
            c->lateUpdate(dt);
        }
    }
    --_iterDepth;
    if (_iterDepth == 0) applyPending();
}

}  // namespace cc
