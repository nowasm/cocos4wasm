#pragma once

#include "base/Ptr.h"
#include "cocos/2d/framework/UIRenderer.h"
#include "cocos/asset/SpriteFrame.h"
#include "core/assets/Texture2D.h"
#include "math/Color.h"
#include "math/Vec2.h"

namespace cc {

// Textured quad. First authoring-layer UIRenderer that samples a Texture2D.
//
// Storage is authoritative on `_spriteFrame` (matches upstream sprite.ts);
// Texture2D access goes through `spriteFrame->getTexture()`. `setTexture()`
// is a convenience shim that wraps a raw Texture2D in a default SpriteFrame
// so existing callers (scene demos, Button/Indicator transition code) keep
// compiling while the SpriteFrame asset becomes the canonical authoring
// handle.
//
// P2c scope (still):
//   - SIMPLE type (whole frame stretched to the quad, no 9-slice)
//   - Full-size UV (0,0..1,1); ignore SpriteFrame.rect sub-regions for now
//     (atlas sub-rects land when the UV path is generalized)
//   - mainColor uniform acts as a tint multiplier on the sampled texel
//
// Deferred to later phases:
//   - Atlas UV sub-rect + rotation (Sprite::updateGeometry reads from
//     spriteFrame->getRect() and spriteFrame->isRotated())
//   - 9-slice / tiled / filled render modes
//   - Automatic sizing from UITransform
class Sprite : public UIRenderer {
    CC_CLASS_DECL(Sprite, UIRenderer)
public:
    Sprite();
    ~Sprite() override;

    void onEnable() override;
    void onDisable() override;

    // ── SpriteFrame (authoritative) ─────────────────────────────────────
    void         setSpriteFrame(SpriteFrame *sf);
    SpriteFrame *getSpriteFrame() const { return _spriteFrame.get(); }

    // ── Texture convenience ─────────────────────────────────────────────
    // Wraps `tex` in a default SpriteFrame (full-rect, no rotation) and
    // stores it as the spriteFrame. Existing code that talks to Sprite
    // in texture terms keeps working.
    void setTexture(Texture2D *tex);
    Texture2D *getTexture() const {
        return _spriteFrame ? _spriteFrame->getTexture() : nullptr;
    }

    void setSize(float w, float h);
    const Vec2 &getSize() const { return _size; }

    // Tint multiplied into the sampled texel via mainColor.
    void setColor(const Color &c);
    const Color &getColor() const { return _color; }

protected:
    void updateGeometry() override;
    IntrusivePtr<Material> resolveMaterial() override;
    ccstd::vector<gfx::Attribute> vertexAttributes() const override;
    gfx::Texture *resolveBatchTexture() const override;

private:
    // Pull _size from the owning node's UITransform if present. Called at
    // onEnable and whenever the node emits SizeChanged (e.g. from Widget's
    // stretch layout).
    void syncSizeFromUITransform();

    // Authoritative texture binding — SpriteFrame owns the Texture2D.
    IntrusivePtr<SpriteFrame> _spriteFrame;
    Vec2  _size{100.0f, 100.0f};
    Color _color{255, 255, 255, 255};

    // Event-subscription id. Opaque — see Node.h for the typed event.
    struct Hooks;
    Hooks *_hooks{nullptr};
};

}  // namespace cc
