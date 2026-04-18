#include "SceneRegistry.h"

#include <algorithm>
#include <memory>

#include "base/Log.h"
#include "cocos/2d/components/Sprite.h"
#include "cocos/2d/framework/Canvas.h"
#include "cocos/2d/framework/UITransform.h"
#include "core/component/NodeActivator.h"
#include "core/scene-graph/Node.h"
#include "game/TextureLoader.h"

// ─── P4-4 — clickable sprites ─────────────────────────────────────────────
//
// Three coloured sprites arranged horizontally. Each has a UITransform
// (so hit-test bounds exist) and a MouseDown handler that toggles the
// tint between normal and bright. A parent listener on the root catches
// whatever bubbles up.
//
// What this verifies:
//   • InputEventDispatcher wires mouse → Canvas → DFS → UITransform hit.
//   • NodeMouseEventArg carries screen + local coords into handlers.
//   • Bubble phase works — root logger reports the same click the leaf
//     already handled (unless a handler called e.propagationStopped).

class ClickableScene : public DemoScene {
public:
    const char *name() const override { return "P4 — clickable sprites"; }

    void onEnter(cc::scene::RenderScene * /*rs*/, cc::Root * /*root*/) override {
        _root = ccnew cc::Node("clickable-root");
        _root->addRef();

        auto *canvas = _root->addComponent<cc::Canvas>();
        canvas->setClearColor(cc::Color{40, 40, 60, 255});

        // Bubble-phase root listener. Fires after a leaf sprite's own
        // handler; useful for global logging / analytics.
        _rootHandlerId = _root->on<cc::Node::MouseDown>(
            [](cc::Node *self, const cc::NodeMouseEventArg &arg) {
                CC_LOG_INFO("[Click] root saw bubble from '%s' at local=(%.1f, %.1f)",
                            self->getName().c_str(), arg.localX, arg.localY);
            });

        const struct { const char *name; cc::Color color; float x; } cfg[] = {
            {"red-btn",   cc::Color(230,  80,  80, 255), -260.0f},
            {"green-btn", cc::Color( 80, 210, 120, 255),    0.0f},
            {"blue-btn",  cc::Color( 90, 160, 240, 255),  260.0f},
        };

        for (const auto &c : cfg) {
            auto *node = ccnew cc::Node(c.name);
            node->setPosition(cc::Vec3(c.x, 0, 0));

            auto *ui = node->addComponent<cc::UITransform>();
            ui->setContentSize(180.0f, 180.0f);
            ui->setAnchorPoint(0.5f, 0.5f);

            auto *sprite = node->addComponent<cc::Sprite>();
            sprite->setSize(180.0f, 180.0f);
            sprite->setColor(c.color);

            // Per-node state captured by reference inside the lambda — each
            // sprite toggles independently between the authored tint and a
            // brightened version.
            const cc::Color baseColor = c.color;
            auto toggled = std::make_shared<bool>(false);
            node->on<cc::Node::MouseDown>(
                [sprite, baseColor, toggled](cc::Node *self,
                                              const cc::NodeMouseEventArg &arg) {
                    *toggled = !*toggled;
                    cc::Color tinted = baseColor;
                    if (*toggled) {
                        tinted.r = static_cast<uint8_t>(std::min(255, baseColor.r + 60));
                        tinted.g = static_cast<uint8_t>(std::min(255, baseColor.g + 60));
                        tinted.b = static_cast<uint8_t>(std::min(255, baseColor.b + 60));
                    }
                    sprite->setColor(tinted);
                    CC_LOG_INFO("[Click] '%s' button=%u local=(%.1f, %.1f) -> %s",
                                self->getName().c_str(), arg.button,
                                arg.localX, arg.localY, *toggled ? "bright" : "normal");
                });

            _root->addChild(node);
        }

        cc::NodeActivator::get().activateNode(_root, true);
        CC_LOG_INFO("[ClickableScene] ready — 3 buttons armed");
    }

    void onExit() override {
        if (_root) {
            _root->off<cc::Node::MouseDown>(_rootHandlerId);
            cc::NodeActivator::get().activateNode(_root, false);
            _root->release();
            _root = nullptr;
        }
    }

private:
    cc::Node *_root{nullptr};
    cc::event::TargetEventID<cc::Node::MouseDown> _rootHandlerId;
};

REGISTER_DEMO_SCENE("ClickableScene", ClickableScene);
