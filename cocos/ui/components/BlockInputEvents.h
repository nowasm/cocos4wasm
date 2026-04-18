#pragma once

#include "core/component/Component.h"
#include "core/reflection/Reflection.h"

namespace cc {

// Mirrors cocos/ui/block-input-events.ts. Attach to a UI node that sits
// above something you don't want to receive events — Touch/Mouse hits
// land on it and propagation is stopped, so nodes below never see the
// event.
//
// Minimum surface: no properties, no methods. onEnable/onDisable register
// a set of typed Node event handlers that call stopPropagation().
class BlockInputEvents : public Component {
    CC_CLASS_DECL(BlockInputEvents, Component)
public:
    BlockInputEvents() = default;
    ~BlockInputEvents() override = default;

    void onEnable() override;
    void onDisable() override;

private:
    struct Hooks;
    Hooks *_hooks{nullptr};
};

}  // namespace cc
