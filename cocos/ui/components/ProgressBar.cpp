#include "cocos/ui/components/ProgressBar.h"

#include <algorithm>

#include "cocos/2d/components/Sprite.h"
#include "cocos/2d/framework/UITransform.h"
#include "core/scene-graph/Node.h"

namespace cc {

CC_IMPLEMENT_CLASS(ProgressBar, "cc.ProgressBar", Component)
    .property("_barSprite",   &ProgressBar::_barSprite)
    .property("_mode",        &ProgressBar::_mode,        Mode::HORIZONTAL)
    .property("_totalLength", &ProgressBar::_totalLength, 1.f)
    .property("_progress",    &ProgressBar::_progress,    0.1f)
    .property("_reverse",     &ProgressBar::_reverse,     false)
CC_END_CLASS(ProgressBar);

int ProgressBar::forceLink() { return 0; }

void ProgressBar::setBarSprite(Sprite *s) {
    _barSprite = s;
    _originalSizeCaptured = false;
    _applyProgress();
}

void ProgressBar::setProgress(float v) {
    _progress = std::clamp(v, 0.f, 1.f);
    _applyProgress();
}

void ProgressBar::_captureOriginalSize() {
    if (_originalSizeCaptured) return;
    if (!_barSprite) return;
    // Prefer UITransform size; fall back to the sprite's own _size.
    if (auto *ui = _barSprite->getNode()->getComponent<UITransform>()) {
        _barOriginalSize = ui->getContentSize();
    } else {
        _barOriginalSize = _barSprite->getSize();
    }
    if (_barOriginalSize.x <= 0.f && _barOriginalSize.y <= 0.f) return;
    _originalSizeCaptured = true;
}

void ProgressBar::_applyProgress() {
    if (!_barSprite) return;
    _captureOriginalSize();
    if (!_originalSizeCaptured) return;

    const float pct = std::clamp(_progress, 0.f, 1.f);
    // Upstream uses `totalLength * progress` for the filled dimension —
    // not `originalSize * progress`. The original bar size on the
    // authoring node only fixes the cross-axis thickness; the primary
    // axis follows _totalLength directly, which is why Editor previews
    // a 300-pixel-wide bar at 50 % as 150 px wide even when the bar
    // sprite's own UITransform content-size is 150.
    Vec2 target = _barOriginalSize;
    switch (_mode) {
        case Mode::HORIZONTAL:
        case Mode::FILLED:  // FILLED falls back to HORIZONTAL until Sprite
                            // gains Type::FILLED rendering.
            target.x = _totalLength * pct;
            break;
        case Mode::VERTICAL:
            target.y = _totalLength * pct;
            break;
    }

    // Pin the bar's filling edge — the non-shrinking side stays fixed so
    // the "empty" area grows from the opposite edge. Cocos Creator's
    // default authoring gives the bar node an anchor that already pins
    // the desired edge (e.g. (0, 0.5) for a HORIZONTAL bar that grows
    // right), and our Sprite honours that anchor, so sizing the sprite
    // is enough — no position shuffle needed. `reverse` flips the anchor
    // we expect, which we enforce here so prefabs that ship with the
    // "wrong" anchor still behave correctly.
    auto *barNode = _barSprite->getNode();
    auto *ui = barNode->getComponent<UITransform>();
    if (ui) {
        Vec2 ap = ui->getAnchorPoint();
        switch (_mode) {
            case Mode::HORIZONTAL:
            case Mode::FILLED:
                ap.x = _reverse ? 1.f : 0.f;
                break;
            case Mode::VERTICAL:
                ap.y = _reverse ? 1.f : 0.f;
                break;
        }
        ui->setAnchorPoint(ap);
        ui->setContentSize(target);
    }
    _barSprite->setSize(target.x, target.y);
}

}  // namespace cc
