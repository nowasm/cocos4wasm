/****************************************************************************
 Cocos4wasm — port of Cocos Creator's cc.SpriteFrame asset.

 Wraps a Texture2D with atlas metadata: a sub-rect, a pre-trim original
 size, a pivot, an optional 90°-CW rotation flag, and 9-slice border
 insets. Exactly the metadata Editor produces per image slice.

 Serialization shape upstream is custom (not standard @serializable) —
 fields arrive as nested JSON objects (rect = {x,y,width,height}, pivot
 = {x,y}, capInsets = [l,t,r,b]). The reflection registration here is
 just enough for the type-id resolver (`cc.SpriteFrame` → factory); the
 actual JSON parsing lives in SpriteFrameLoader, which writes via the
 public setters.
****************************************************************************/

#pragma once

#include "base/Ptr.h"
#include "base/std/container/string.h"
#include "core/assets/Asset.h"
#include "core/assets/Texture2D.h"
#include "core/reflection/Reflection.h"
#include "math/Vec2.h"
#include "math/Vec4.h"

namespace cc {

class SpriteFrame : public Asset {
    CC_CLASS_DECL(SpriteFrame, void)
public:
    SpriteFrame() = default;
    ~SpriteFrame() override = default;

    // ── Texture binding ─────────────────────────────────────────────────
    Texture2D *getTexture() const { return _texture.get(); }
    void       setTexture(Texture2D *t) { _texture = t; }

    // Convenience: bind texture AND reset rect/originalSize to match the
    // full texture bounds. Used when an unatlased image is loaded directly.
    void setTextureAndResetRect(Texture2D *t);

    // ── Sub-rect in texture pixels ──────────────────────────────────────
    // Packed as {x, y, width, height} — x/y are the bottom-left origin
    // of the slice inside the texture, matching upstream cc.Rect.
    const Vec4 &getRect() const { return _rect; }
    void        setRect(const Vec4 &r) { _rect = r; }
    float       getWidth()  const { return _rotated ? _rect.w : _rect.z; }
    float       getHeight() const { return _rotated ? _rect.z : _rect.w; }

    // ── Pre-trim ("original") size ─────────────────────────────────────
    // Size before transparent-edge trimming during atlas packing.
    // Used to position the visible slice correctly within the unpacked
    // content box.
    const Vec2 &getOriginalSize() const { return _originalSize; }
    void        setOriginalSize(const Vec2 &v) { _originalSize = v; }

    // ── Trim offset ────────────────────────────────────────────────────
    // Pixel offset from the center of originalSize to the center of the
    // trimmed rect. Zero for untrimmed frames.
    const Vec2 &getOffset() const { return _offset; }
    void        setOffset(const Vec2 &v) { _offset = v; }

    // ── Rotation flag (atlas packing optimization) ─────────────────────
    // When true the slice was rotated 90° CW in the texture to pack
    // tighter — renderer swaps UV/vert layout accordingly.
    bool isRotated() const { return _rotated; }
    void setRotated(bool v) { _rotated = v; }

    // ── 9-slice cap insets [left, top, right, bottom] in pixels ────────
    // A zero vector means the frame is not 9-sliced. Non-zero triggers
    // the 9-slice draw path in the sprite renderer.
    const Vec4 &getCapInsets() const { return _capInsets; }
    void        setCapInsets(const Vec4 &v) { _capInsets = v; }
    float       getInsetLeft()   const { return _capInsets.x; }
    float       getInsetTop()    const { return _capInsets.y; }
    float       getInsetRight()  const { return _capInsets.z; }
    float       getInsetBottom() const { return _capInsets.w; }
    void setInsetLeft  (float v) { _capInsets.x = v; }
    void setInsetTop   (float v) { _capInsets.y = v; }
    void setInsetRight (float v) { _capInsets.z = v; }
    void setInsetBottom(float v) { _capInsets.w = v; }

    // ── Pivot in normalized [0,1] coordinates ──────────────────────────
    // Default (0.5, 0.5) = centred. Matches UITransform's anchorPoint
    // default, so most Editor-exported sprites render at their authored
    // position without needing an explicit pivot override.
    const Vec2 &getPivot() const { return _pivot; }
    void        setPivot(const Vec2 &v) { _pivot = v; }

    // ── Packable (editor-only hint; runtime no-op) ─────────────────────
    bool isPackable() const { return _packable; }
    void setPackable(bool v) { _packable = v; }

    // ── pixelsToUnit (physics scaling; runtime tracks but doesn't use
    //    yet) ──────────────────────────────────────────────────────────
    float getPixelsToUnit() const { return _pixelsToUnit; }
    void  setPixelsToUnit(float v) { _pixelsToUnit = v; }

private:
    IntrusivePtr<Texture2D> _texture;

    Vec4 _rect{0.f, 0.f, 0.f, 0.f};          // x, y, width, height (pixel space)
    Vec2 _offset{0.f, 0.f};                  // trim offset
    Vec2 _originalSize{0.f, 0.f};            // pre-trim size
    bool _rotated{false};                    // 90° CW in atlas
    Vec4 _capInsets{0.f, 0.f, 0.f, 0.f};     // left, top, right, bottom
    Vec2 _pivot{0.5f, 0.5f};                 // normalized anchor
    bool _packable{true};                    // editor hint
    float _pixelsToUnit{100.f};              // physics scaling
};

}  // namespace cc
