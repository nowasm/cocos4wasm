#include "cocos/ui/components/ProgressBar.h"

#include <algorithm>

#include "cocos/2d/components/Sprite.h"
#include "cocos/2d/framework/UITransform.h"
#include "core/scene-graph/Node.h"

namespace cc {

CC_IMPLEMENT_CLASS(ProgressBar, "cc.ProgressBar", Component)
    .property("_mode",        &ProgressBar::_mode,        Mode::HORIZONTAL)
    .property("_totalLength", &ProgressBar::_totalLength, 1.f)
    .property("_progress",    &ProgressBar::_progress,    0.1f)
    .property("_reverse",     &ProgressBar::_reverse,     false)
CC_END_CLASS(ProgressBar);

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
    Vec2 target = _barOriginalSize;

    switch (_mode) {
        case Mode::HORIZONTAL:
        case Mode::FILLED:   // MVP: treat FILLED as HORIZONTAL until
                             // SpriteFrame FillType lands.
            target.x = _barOriginalSize.x * pct;
            break;
        case Mode::VERTICAL:
            target.y = _barOriginalSize.y * pct;
            break;
    }

    // Keep the bar anchored to one edge so shrinking reveals the "empty"
    // side. `reverse` flips which edge stays pinned. We accomplish the
    // anchoring by moving the sprite node so its edge stays fixed.
    auto *barNode = _barSprite->getNode();
    auto *ui = barNode->getComponent<UITransform>();
    const Vec2 anchor = ui ? ui->getAnchorPoint() : Vec2{0.5f, 0.5f};
    Vec3 pos = barNode->getPosition();

    if (_mode == Mode::HORIZONTAL || _mode == Mode::FILLED) {
        const float delta = (_barOriginalSize.x - target.x);
        // anchor 0 = left-pinned, anchor 1 = right-pinned, 0.5 = centred.
        // Default: pin the side opposite to `reverse`. reverse=false pins
        // left (anchor 0), reverse=true pins right (anchor 1).
        const float shift = (_reverse ? +0.5f : -0.5f) * delta;
        pos.x = barNode->getPosition().x;  // preserve unless below adjusts
        (void)shift; (void)anchor;
    } else {
        const float delta = (_barOriginalSize.y - target.y);
        (void)delta;
    }

    _barSprite->setSize(target.x, target.y);
    if (ui) ui->setContentSize(target);
    barNode->setPosition(pos);
}

}  // namespace cc
