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
// Render modes:
//   - SIMPLE: whole frame stretched across the quad (atlas sub-rect honoured)
//   - SLICED: 9-slice using SpriteFrame capInsets (L,T,R,B). The four
//     corner quads stay at native pixel size, the four edges stretch in
//     one axis, the centre stretches in both. When the render size is
//     smaller than the cap sum on an axis, the cap bands collapse
//     proportionally (matches upstream Cocos behaviour).
//
// TILED / FILLED render modes and trim / rotation are deferred to later
// phases. Tint is per-vertex (mainColor uniform stays white) so sprites
// with the same texture can share a batch regardless of colour.
class Sprite : public UIRenderer {
    CC_CLASS_DECL(Sprite, UIRenderer)
public:
    // Matches upstream cc.Sprite.Type enum integer values so the prefab
    // `_type` field round-trips without remapping.
    enum class Type : uint32_t {
        SIMPLE = 0,
        SLICED = 1,
        // TILED = 2, FILLED = 3  — not yet implemented, will fall back to SIMPLE
    };

    Sprite();
    ~Sprite() override;

    void onEnable() override;
    void onDisable() override;

    Type getType() const { return _type; }
    void setType(Type t);

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
    Type  _type{Type::SIMPLE};
    Vec2  _size{100.0f, 100.0f};
    Color _color{255, 255, 255, 255};

    // Event-subscription id. Opaque — see Node.h for the typed event.
    struct Hooks;
    Hooks *_hooks{nullptr};
};

}  // namespace cc
