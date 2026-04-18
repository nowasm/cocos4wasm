#pragma once

#include "core/component/Component.h"
#include "core/reflection/Reflection.h"

namespace cc {

class Widget;

// Mirrors cocos/ui/safe-area.ts. Pairs with a Widget on the same node
// and adjusts its edge-margins to match the device's safe-area
// rectangle (iPhone notch, Android cutouts, etc.).
//
// On desktop the safe area equals the viewport, so updateArea() is
// effectively a no-op — but the component exists so scene JSON that
// uses it can round-trip unchanged.
class SafeArea : public Component {
    CC_CLASS_DECL(SafeArea, Component)
public:
    SafeArea() = default;
    ~SafeArea() override = default;

    void onEnable() override { updateArea(); }

    bool isSymmetric() const { return _symmetric; }
    void setSymmetric(bool v) { _symmetric = v; updateArea(); }

    // Apply the current safe-area rectangle to the paired Widget.
    // Desktop: no-op (nothing to inset). Mobile/native: pull from
    // platform SDK (TODO hook).
    void updateArea();

private:
    bool _symmetric{true};
};

}  // namespace cc
