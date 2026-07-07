/****************************************************************************
 TweenSystem — global registry that drives every running Tween (P7).

 Singleton, ticked externally: TweenSystem::getInstance().update(dt). Holds
 an IntrusivePtr per running tween (so unowned tweens survive until they
 finish) and iterates a snapshot each update, so call() callbacks may freely
 start or stop other tweens — new tweens begin ticking on the next update.
****************************************************************************/

#pragma once

#include "base/Ptr.h"
#include "base/std/container/vector.h"

namespace cc {

class Tween;

class TweenSystem {
public:
    static TweenSystem &getInstance();

    void update(float dt);
    void stopAll();
    size_t runningCount() const { return _tweens.size(); }

private:
    friend class Tween;

    TweenSystem() = default;

    void add(Tween *tween);    // no-op if already registered
    void remove(Tween *tween); // no-op if not registered

    ccstd::vector<IntrusivePtr<Tween>> _tweens;
};

} // namespace cc
