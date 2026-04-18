#include "cocos/ui/components/SafeArea.h"

#include "cocos/ui/components/Widget.h"
#include "core/scene-graph/Node.h"

namespace cc {

CC_IMPLEMENT_CLASS(SafeArea, "cc.SafeArea", Component)
    .property("_symmetric", &SafeArea::_symmetric, true)
CC_END_CLASS(SafeArea);

void SafeArea::updateArea() {
    // Desktop builds have no notch — the full window is usable. When
    // the mobile ISafeAreaManager lands, this is where we'd query
    // insets and push them into the Widget. For now we simply force
    // the paired Widget to re-run its alignment so the API exercises
    // the right code path in tests.
    auto *node = getNode();
    if (!node) return;
    auto *w = node->getComponent<Widget>();
    if (!w) return;
    w->updateAlignment();
}

}  // namespace cc
