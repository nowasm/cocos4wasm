#include "cocos/asset/SpriteFrame.h"

#include "core/assets/ImageAsset.h"

namespace cc {

// Minimal reflection: SpriteFrame is recognized by `cc.SpriteFrame` so the
// JSON deserializer can factor-construct one when resolving __uuid__ refs,
// and Sprite / Button / ProgressBar properties typed `IntrusivePtr<SpriteFrame>`
// can serialize their pointee class. All fields populate via setters from
// SpriteFrameLoader — no @serializable decorators on the C++ side.
CC_IMPLEMENT_ROOT_CLASS(SpriteFrame, "cc.SpriteFrame")
CC_END_CLASS(SpriteFrame);

void SpriteFrame::setTextureAndResetRect(Texture2D *t) {
    _texture = t;
    if (t) {
        if (auto *img = t->getImage()) {
            const float w = static_cast<float>(img->getWidth());
            const float h = static_cast<float>(img->getHeight());
            _rect.set(0.f, 0.f, w, h);
            _originalSize.set(w, h);
        } else {
            _rect.set(0.f, 0.f, 0.f, 0.f);
            _originalSize.set(0.f, 0.f);
        }
    } else {
        _rect.set(0.f, 0.f, 0.f, 0.f);
        _originalSize.set(0.f, 0.f);
    }
    _offset.set(0.f, 0.f);
    _rotated = false;
}

}  // namespace cc
