#include "cocos/2d/components/Sprite.h"

#include "core/assets/Material.h"
#include "game/MaterialFactory.h"

namespace cc {

CC_IMPLEMENT_CLASS(Sprite, "cc.Sprite", UIRenderer)
    .property("color", &Sprite::_color, Color{255, 255, 255, 255})
    .property("size",  &Sprite::_size,  Vec2{100.0f, 100.0f})
CC_END_CLASS(Sprite);

Sprite::Sprite() {
    _vertexStrideFloats = 5;  // position(3) + uv(2)
}

Sprite::~Sprite() = default;

void Sprite::setTexture(Texture2D *tex) {
    if (_texture.get() == tex) return;
    _texture = tex;
    markDirty();
}

void Sprite::setSize(float w, float h) {
    _size.set(w, h);
    markDirty();
}

void Sprite::setColor(const Color &c) {
    _color = c;
    markDirty();
}

ccstd::vector<gfx::Attribute> Sprite::vertexAttributes() const {
    return {
        gfx::Attribute{gfx::ATTR_NAME_POSITION,  gfx::Format::RGB32F},
        gfx::Attribute{gfx::ATTR_NAME_TEX_COORD, gfx::Format::RG32F},
    };
}

void Sprite::updateGeometry() {
    _vertexStrideFloats = 5;
    const float hx = _size.x * 0.5f;
    const float hy = _size.y * 0.5f;

    // UV convention: (0,0) top-left of image; texture Y is pre-flipped on
    // load by the engine's image pipeline, so standard image orientation
    // maps naturally.
    _vertexData = {
        -hx, -hy, 0.0f,   0.0f, 1.0f,  // bottom-left
         hx, -hy, 0.0f,   1.0f, 1.0f,  // bottom-right
         hx,  hy, 0.0f,   1.0f, 0.0f,  // top-right
        -hx,  hy, 0.0f,   0.0f, 0.0f,  // top-left
    };
    _indexData = {0, 1, 2, 0, 2, 3};
    _vertexCount = 4;
    _indexCount  = 6;
}

IntrusivePtr<Material> Sprite::resolveMaterial() {
    IntrusivePtr<Material> mat;
    if (_texture && _texture->getGFXTexture()) {
        mat = game::MaterialFactory::createUnlitTextured(_texture.get());
        if (mat) {
            mat->setPropertyColor("mainColor", _color);
        }
    } else {
        // No texture assigned — fall back to a flat-coloured quad so the
        // component is still visible for debugging.
        mat = game::MaterialFactory::createUnlit(_color);
    }
    return mat;
}

}  // namespace cc
