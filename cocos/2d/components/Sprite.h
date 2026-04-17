#pragma once

#include "base/Ptr.h"
#include "cocos/2d/framework/UIRenderer.h"
#include "core/assets/Texture2D.h"
#include "math/Color.h"
#include "math/Vec2.h"

namespace cc {

// Textured quad. First authoring-layer UIRenderer that samples a Texture2D.
//
// P2c scope:
//   - SIMPLE type (whole texture stretched to the quad, no 9-slice / no UV
//     rects for atlas sub-regions)
//   - Full-size UV (0,0..1,1); no trim, no rotation
//   - mainColor uniform acts as a tint multiplier on the sampled texel
//
// Deferred to later phases:
//   - SpriteFrame asset wrapper (atlas UV rect, trim, rotation)
//   - 9-slice / tiled / filled render modes
//   - Automatic sizing from UITransform
class Sprite : public UIRenderer {
    CC_CLASS_DECL(Sprite, UIRenderer)
public:
    Sprite();
    ~Sprite() override;

    void setTexture(Texture2D *tex);
    Texture2D *getTexture() const { return _texture.get(); }

    void setSize(float w, float h);
    const Vec2 &getSize() const { return _size; }

    // Tint multiplied into the sampled texel via mainColor.
    void setColor(const Color &c);
    const Color &getColor() const { return _color; }

protected:
    void updateGeometry() override;
    IntrusivePtr<Material> resolveMaterial() override;
    ccstd::vector<gfx::Attribute> vertexAttributes() const override;

private:
    IntrusivePtr<Texture2D> _texture;
    Vec2  _size{100.0f, 100.0f};
    Color _color{255, 255, 255, 255};
};

}  // namespace cc
