#pragma once

#include "base/std/container/vector.h"

namespace cc {

class EditBoxImplBase;

// Mirrors `cocos/ui/editbox/tabIndexUtil.ts`. Maintains the registry of
// focus-eligible EditBoxImpl instances so `Tab` cycles between them in
// ascending tabIndex order.
//
// Ownership: impls register themselves in their init() and deregister in
// clear(). The registry holds raw pointers — lifetime is tied to the
// impl's existence, and an impl that's been cleared must remove itself
// before destruction.
class TabIndexUtil {
public:
    // Register / unregister an impl. Idempotent — repeated adds of the
    // same impl are ignored.
    static void add(EditBoxImplBase *impl);
    static void remove(EditBoxImplBase *impl);

    // Re-sort by the delegate's tabIndex. Called after any impl's
    // tabIndex changes. `next()` assumes the list is sorted.
    static void resort();

    // Blur `from` and focus the next impl in tab order (tabIndex >= 0).
    // If `from` isn't registered or there's no valid next entry, the
    // call still blurs `from` (matching TS behavior where setFocus(false)
    // fires unconditionally).
    static void next(EditBoxImplBase *from);

private:
    // Lazy-initialised Meyers list — avoids a static-initialisation
    // fiasco when EditBoxImpl instances register at global dtor time.
    static ccstd::vector<EditBoxImplBase *> &list();
};

}  // namespace cc
