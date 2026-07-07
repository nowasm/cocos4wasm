/****************************************************************************
 TweenSystem — implementation (P7). See TweenSystem.h for the design.
****************************************************************************/

#include "tween/TweenSystem.h"

#include "tween/Tween.h"

namespace cc {

TweenSystem &TweenSystem::getInstance() {
    static TweenSystem instance;
    return instance;
}

void TweenSystem::add(Tween *tween) {
    for (const auto &t : _tweens) {
        if (t.get() == tween) {
            return;
        }
    }
    _tweens.emplace_back(tween);
}

void TweenSystem::remove(Tween *tween) {
    for (auto it = _tweens.begin(); it != _tweens.end(); ++it) {
        if (it->get() == tween) {
            _tweens.erase(it);
            return;
        }
    }
}

void TweenSystem::update(float dt) {
    // Snapshot: callbacks may add/remove tweens while we iterate. Tweens
    // stopped by an earlier callback in this pass are skipped; tweens
    // started during this pass tick from the next update.
    ccstd::vector<IntrusivePtr<Tween>> snapshot = _tweens;
    for (const auto &t : snapshot) {
        if (!t->isRunning()) {
            continue;
        }
        if (t->update(dt)) {
            t->stop();
        }
    }
}

void TweenSystem::stopAll() {
    ccstd::vector<IntrusivePtr<Tween>> snapshot;
    snapshot.swap(_tweens);
    for (const auto &t : snapshot) {
        t->stop(); // remove() is now a no-op; just clears the running flag
    }
}

} // namespace cc
