#include "cocos/input/InputEventDispatcher.h"

#include <algorithm>

#include "application/ApplicationManager.h"
#include "base/Log.h"
#include "cocos/2d/framework/Canvas.h"
#include "cocos/2d/framework/UITransform.h"
#include "core/scene-graph/Node.h"
#include "engine/EngineEvents.h"
#include "platform/interfaces/modules/ISystemWindow.h"
#include "platform/interfaces/modules/ISystemWindowManager.h"

namespace cc {

struct InputEventDispatcher::Impl {
    events::Mouse::Listener mouseListener;
    events::Touch::Listener touchListener;
};

InputEventDispatcher &InputEventDispatcher::get() {
    static InputEventDispatcher inst;
    return inst;
}

void InputEventDispatcher::registerCanvas(Canvas *c) {
    if (!c) return;
    if (std::find(_canvases.begin(), _canvases.end(), c) == _canvases.end()) {
        _canvases.push_back(c);
    }
}

void InputEventDispatcher::unregisterCanvas(Canvas *c) {
    _canvases.erase(std::remove(_canvases.begin(), _canvases.end(), c),
                    _canvases.end());
}

namespace {

// Pull the current window (width, height) for screen → world conversion.
// Returns false if the platform interface isn't ready (engine hasn't
// finished booting), in which case the dispatch is skipped.
bool getWindowViewSize(float &w, float &h) {
    auto *win = CC_GET_MAIN_SYSTEM_WINDOW();
    if (!win) return false;
    const auto sz = win->getViewSize();
    w = static_cast<float>(sz.width);
    h = static_cast<float>(sz.height);
    return w > 0 && h > 0;
}

// Map window-space event coordinates to Cocos UI world coordinates.
// SDL delivers origin at top-left with y growing downward; the ortho UI
// camera lives at world (0,0,0) with y growing upward, ortho height =
// window height / 2 (see Canvas::createCamera). Pixel-perfect 1:1 means
// we only need to recentre and flip y.
void windowToWorld(float winX, float winY, float winW, float winH,
                   float &outX, float &outY) {
    outX = winX - winW * 0.5f;
    outY = winH * 0.5f - winY;
}

// Depth-first hit-test a node sub-tree. Returns the deepest node whose
// UITransform contains the world point, visiting children in reverse
// sibling order so the topmost-drawn sprite wins. `outLocal` carries the
// hit node's local-space coords back to the caller.
Node *hitTestDFS(Node *node, float wx, float wy, Vec2 &outLocal) {
    if (!node || !node->isActiveInHierarchy()) return nullptr;

    // Recurse into children first — deeper / later-sibling nodes render
    // on top, so they must be tested first to win overlap ties.
    const auto &children = node->getChildren();
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        Node *child = it->get();
        if (Node *hit = hitTestDFS(child, wx, wy, outLocal)) {
            return hit;
        }
    }

    // No child claimed the point — test this node's own UITransform.
    UITransform *ui = node->getComponent<UITransform>();
    if (ui && ui->hitTestWorld(wx, wy, &outLocal)) {
        return node;
    }
    return nullptr;
}

// Map cc::MouseEvent::Type → the TgtEvent dispatch and emit it with an
// already-populated NodeMouseEventArg.
void dispatchMouse(Node *hit, const MouseEvent &ev, const NodeMouseEventArg &arg) {
    switch (ev.type) {
        case MouseEvent::Type::DOWN:  hit->dispatchEvent<Node::MouseDown>(arg); break;
        case MouseEvent::Type::UP:    hit->dispatchEvent<Node::MouseUp>(arg);   break;
        case MouseEvent::Type::MOVE:  hit->dispatchEvent<Node::MouseMove>(arg); break;
        case MouseEvent::Type::WHEEL: hit->dispatchEvent<Node::MouseWheel>(arg); break;
        default: break;
    }
}

}  // namespace

void InputEventDispatcher::start() {
    if (_running) return;
    _impl = new Impl();

    _impl->mouseListener.bind([this](const MouseEvent &ev) {
        if (_canvases.empty()) return;

        float winW = 0, winH = 0;
        if (!getWindowViewSize(winW, winH)) return;

        float wx = 0, wy = 0;
        windowToWorld(ev.x, ev.y, winW, winH, wx, wy);

        // Iterate canvases in descending priority — the topmost layer
        // gets first crack at the hit.
        ccstd::vector<Canvas *> sorted = _canvases;
        std::sort(sorted.begin(), sorted.end(), [](Canvas *a, Canvas *b) {
            return a->getPriority() > b->getPriority();
        });

        Vec2 local;
        for (auto *c : sorted) {
            Node *root = c->getNode();
            Node *hit = hitTestDFS(root, wx, wy, local);
            if (!hit) continue;

            NodeMouseEventArg arg;
            arg.x = ev.x;
            arg.y = ev.y;
            arg.localX = local.x;
            arg.localY = local.y;
            arg.button = ev.button;
            if (ev.type == MouseEvent::Type::WHEEL) {
                arg.wheelDx = ev.xDelta;
                arg.wheelDy = ev.yDelta;
            }
            dispatchMouse(hit, ev, arg);
            return;  // first hit wins — don't fall through to lower canvases
        }
    });

    _impl->touchListener.bind([this](const TouchEvent &ev) {
        if (_canvases.empty() || ev.touches.empty()) return;
        const auto &t0 = ev.touches[0];  // MVP: single-touch; multi-touch later

        float winW = 0, winH = 0;
        if (!getWindowViewSize(winW, winH)) return;

        float wx = 0, wy = 0;
        windowToWorld(t0.x, t0.y, winW, winH, wx, wy);

        ccstd::vector<Canvas *> sorted = _canvases;
        std::sort(sorted.begin(), sorted.end(), [](Canvas *a, Canvas *b) {
            return a->getPriority() > b->getPriority();
        });

        Vec2 local;
        for (auto *c : sorted) {
            Node *root = c->getNode();
            Node *hit = hitTestDFS(root, wx, wy, local);
            if (!hit) continue;

            NodeTouchEventArg arg;
            arg.x = t0.x;
            arg.y = t0.y;
            arg.localX = local.x;
            arg.localY = local.y;
            arg.touchId = static_cast<uint32_t>(t0.index);

            switch (ev.type) {
                case TouchEvent::Type::BEGAN:     hit->dispatchEvent<Node::TouchStart>(arg);  break;
                case TouchEvent::Type::MOVED:     hit->dispatchEvent<Node::TouchMove>(arg);   break;
                case TouchEvent::Type::ENDED:     hit->dispatchEvent<Node::TouchEnd>(arg);    break;
                case TouchEvent::Type::CANCELLED: hit->dispatchEvent<Node::TouchCancel>(arg); break;
                default: break;
            }
            return;
        }
    });

    _running = true;
}

void InputEventDispatcher::stop() {
    if (!_running) return;
    delete _impl;
    _impl = nullptr;
    _running = false;
}

}  // namespace cc
