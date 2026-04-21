#pragma once

#include "base/Ptr.h"
#include "core/component/Component.h"
#include "core/reflection/Reflection.h"
#include "math/Vec2.h"

namespace cc {

class Sprite;

// Mirrors cocos/ui/progress-bar.ts. Drives a Sprite's size (HORIZONTAL /
// VERTICAL modes) to visualise a 0..1 progress value.
//
// FILLED mode in Creator uses Sprite.Type.FILLED with radial / stretch
// fill modes — our Sprite doesn't have that asset-frame machinery yet,
// so FILLED is accepted as a value for API parity but currently behaves
// as HORIZONTAL.
class ProgressBar : public Component {
    CC_CLASS_DECL(ProgressBar, Component)
public:
    enum class Mode : uint32_t {
        HORIZONTAL = 0,
        VERTICAL   = 1,
        FILLED     = 2,  // accepted but falls back to HORIZONTAL behaviour
    };

    ProgressBar() = default;
    ~ProgressBar() override = default;

    // Drag the translation unit out of the static-lib's dead-code strip.
    // MSVC drops any object whose symbols nothing imports; without this
    // DemoGame hook the ProgressBar reflection registration never runs
    // and scene JSON fails with "unknown class 'cc.ProgressBar'".
    static int forceLink();

    void onLoad() override { _applyProgress(); }
    void onEnable() override { _applyProgress(); }

    Sprite *getBarSprite() const { return _barSprite; }
    void    setBarSprite(Sprite *s);

    Mode getMode() const { return _mode; }
    void setMode(Mode m) { _mode = m; _applyProgress(); }

    float getTotalLength() const { return _totalLength; }
    void  setTotalLength(float v) { _totalLength = v; _applyProgress(); }

    float getProgress() const { return _progress; }
    void  setProgress(float v);  // clamps to [0,1]

    bool isReverse() const { return _reverse; }
    void setReverse(bool v) { _reverse = v; _applyProgress(); }

private:
    void _applyProgress();
    void _captureOriginalSize();

    Sprite *_barSprite{nullptr};
    Mode    _mode{Mode::HORIZONTAL};
    float   _totalLength{1.f};
    float   _progress{0.1f};
    bool    _reverse{false};

    // Captured original bar-sprite size on first apply so we can rebuild
    // the geometry proportionally each progress change.
    Vec2 _barOriginalSize{0.f, 0.f};
    bool _originalSizeCaptured{false};
};

}  // namespace cc
