#include "cocos/2d/components/Mask.h"

#include "base/Log.h"
#include "core/assets/EffectAsset.h"
#include "core/assets/Material.h"

namespace cc {

CC_IMPLEMENT_CLASS(Mask, "cc.Mask", UIRenderer)
    .property("size",     &Mask::_size,     Vec2{200.0f, 200.0f})
    .property("inverted", &Mask::_inverted)
CC_END_CLASS(Mask);

Mask::Mask() {
    _vertexStrideFloats = 3;  // position-only; colour never reaches the framebuffer anyway
}

Mask::~Mask() = default;

void Mask::setType(Type t) {
    _type = t;
    markDirty();
}

void Mask::setSize(float w, float h) {
    _size.set(w, h);
    markDirty();
}

void Mask::setInverted(bool v) {
    _inverted = v;
    markDirty();
}

ccstd::vector<gfx::Attribute> Mask::vertexAttributes() const {
    return {
        gfx::Attribute{gfx::ATTR_NAME_POSITION, gfx::Format::RGB32F},
    };
}

void Mask::updateGeometry() {
    _vertexStrideFloats = 3;
    _vertexData.clear();
    _indexData.clear();
    _vertexCount = 0;
    _indexCount  = 0;

    const float hx = _size.x * 0.5f;
    const float hy = _size.y * 0.5f;

    switch (_type) {
        case Type::RECT: {
            _vertexData = {
                -hx, -hy, 0.0f,   // 0 BL
                 hx, -hy, 0.0f,   // 1 BR
                 hx,  hy, 0.0f,   // 2 TR
                -hx,  hy, 0.0f,   // 3 TL
            };
            _indexData = {0, 1, 2, 0, 2, 3};
            _vertexCount = 4;
            _indexCount  = 6;
            break;
        }
        // ELLIPSE / GRAPHICS land in P2f-b
    }
}

IntrusivePtr<Material> Mask::resolveMaterial() {
    // The mask's own shader never writes colour (the ENTER_LEVEL stencil
    // stage forces every fragment to fail the stencil test, which cancels
    // the colour write but still lets failOp run to stamp our stencil bit).
    // Any pass-through shader works; we keep builtin-unlit for uniformity
    // with other UIRenderers in this scene.
    auto *effect = EffectAsset::get("builtin-unlit");
    if (!effect) {
        CC_LOG_ERROR("[Mask] builtin-unlit effect missing");
        return nullptr;
    }

    IMaterialInfo info;
    info.effectAsset = effect;
    info.technique   = 1u;  // transparent (blend state harmless since fragments are discarded)

    auto *mat = ccnew Material();
    mat->initialize(info);
    return mat;
}

}  // namespace cc
