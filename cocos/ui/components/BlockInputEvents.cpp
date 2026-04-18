#include "cocos/ui/components/BlockInputEvents.h"

#include "core/event/EventTarget.h"
#include "core/scene-graph/Node.h"

namespace cc {

CC_IMPLEMENT_CLASS(BlockInputEvents, "cc.BlockInputEvents", Component)
CC_END_CLASS(BlockInputEvents);

struct BlockInputEvents::Hooks {
    cc::event::TargetEventID<Node::TouchStart>  touchStart;
    cc::event::TargetEventID<Node::TouchMove>   touchMove;
    cc::event::TargetEventID<Node::TouchEnd>    touchEnd;
    cc::event::TargetEventID<Node::TouchCancel> touchCancel;
    cc::event::TargetEventID<Node::MouseDown>   mouseDown;
    cc::event::TargetEventID<Node::MouseMove>   mouseMove;
    cc::event::TargetEventID<Node::MouseUp>     mouseUp;
    cc::event::TargetEventID<Node::MouseWheel>  mouseWheel;
};

void BlockInputEvents::onEnable() {
    auto *node = getNode();
    if (!node || _hooks) return;
    _hooks = new Hooks();

    // Each handler explicitly does nothing — the dispatcher fires us
    // because we're the first hit-test match, which is precisely what
    // "block events from propagating" means in our stack. Nothing below
    // receives the event.
    _hooks->touchStart  = node->on<Node::TouchStart> ([](Node *, const NodeTouchEventArg &) {});
    _hooks->touchMove   = node->on<Node::TouchMove>  ([](Node *, const NodeTouchEventArg &) {});
    _hooks->touchEnd    = node->on<Node::TouchEnd>   ([](Node *, const NodeTouchEventArg &) {});
    _hooks->touchCancel = node->on<Node::TouchCancel>([](Node *, const NodeTouchEventArg &) {});
    _hooks->mouseDown   = node->on<Node::MouseDown>  ([](Node *, const NodeMouseEventArg &) {});
    _hooks->mouseMove   = node->on<Node::MouseMove>  ([](Node *, const NodeMouseEventArg &) {});
    _hooks->mouseUp     = node->on<Node::MouseUp>    ([](Node *, const NodeMouseEventArg &) {});
    _hooks->mouseWheel  = node->on<Node::MouseWheel> ([](Node *, const NodeMouseEventArg &) {});
}

void BlockInputEvents::onDisable() {
    if (!_hooks) return;
    auto *node = getNode();
    if (node) {
        node->off<Node::TouchStart> (_hooks->touchStart);
        node->off<Node::TouchMove>  (_hooks->touchMove);
        node->off<Node::TouchEnd>   (_hooks->touchEnd);
        node->off<Node::TouchCancel>(_hooks->touchCancel);
        node->off<Node::MouseDown>  (_hooks->mouseDown);
        node->off<Node::MouseMove>  (_hooks->mouseMove);
        node->off<Node::MouseUp>    (_hooks->mouseUp);
        node->off<Node::MouseWheel> (_hooks->mouseWheel);
    }
    delete _hooks;
    _hooks = nullptr;
}

}  // namespace cc
