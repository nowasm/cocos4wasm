#include "cocos/2d/components/Sprite.h"

#include "base/Log.h"
#include "core/assets/EffectAsset.h"
#include "core/assets/Material.h"
#include "game/MaterialFactory.h"
#include "renderer/core/PassUtils.h"

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
    if (!_texture || !_texture->getGFXTexture()) {
        // No texture assigned — fall back to a flat-coloured quad so the
        // component is still visible for debugging.
        return game::MaterialFactory::createUnlit(_color);
    }

    // Build the material with USE_TEXTURE pre-baked into the defines so the
    // shader compiles with the texture-sampling branch enabled. Setting
    // defines only at initialize() time avoids the runtime recompileShaders
    // path that deadlocks in this configuration.
    auto *effect = EffectAsset::get("builtin-unlit");
    if (!effect) {
        CC_LOG_ERROR("[Sprite] builtin-unlit effect missing; builtin effects loaded?");
        return nullptr;
    }

    MacroRecord defines{{"USE_TEXTURE", true}};
    IMaterialInfo info;
    info.effectAsset = effect;
    info.defines = IMaterialInfo::DefinesType{defines};

    auto *mat = ccnew Material();
    mat->initialize(info);
    mat->setPropertyTextureBase("mainTexture", _texture.get());
    mat->setPropertyColor("mainColor", _color);
    return mat;
}

}  // namespace cc
