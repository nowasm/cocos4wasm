#pragma once

#include <functional>

#include "base/std/container/vector.h"
#include "core/component/Component.h"
#include "core/reflection/Reflection.h"

namespace cc {

class Node;
class Sprite;
class UITransform;

// Mirrors cocos/ui/slider.ts. A handle Sprite slides along a track
// (the owning node's UITransform defines the bar). Drag updates the
// 0..1 progress value and fires `slide` events.
class Slider : public Component {
    CC_CLASS_DECL(Slider, Component)
public:
    enum class Direction : uint32_t {
        Horizontal = 0,
        Vertical   = 1,
    };

    using Handler = std::function<void(Slider *)>;

    Slider() = default;
    ~Slider() override = default;

    void onEnable() override;
    void onDisable() override;

    Sprite *getHandle() const { return _handle; }
    void    setHandle(Sprite *h);

    Direction getDirection() const { return _direction; }
    void      setDirection(Direction d) { _direction = d; _applyProgress(); }

    float getProgress() const { return _progress; }
    void  setProgress(float v);  // clamps to [0,1]

    // slideEvents handler vector — fires on every progress change from
    // drag (and programmatic setProgress). Mirrors TS `slideEvents[]`.
    void addSlideListener(Handler fn) { _slideEvents.push_back(std::move(fn)); }
    void clearSlideListeners() { _slideEvents.clear(); }

private:
    void _applyProgress();
    void _emitSlide();

    Sprite   *_handle{nullptr};
    Direction _direction{Direction::Horizontal};
    float     _progress{0.1f};

    ccstd::vector<Handler> _slideEvents;

    struct Hooks;
    Hooks *_hooks{nullptr};

    // Track drag state — whether the handle is currently being pointed at.
    bool _dragging{false};
};

}  // namespace cc
