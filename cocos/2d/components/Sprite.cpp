#include "cocos/2d/components/Sprite.h"

#include "base/Log.h"
#include "base/std/container/unordered_map.h"
#include "cocos/2d/framework/UITransform.h"
#include "core/assets/EffectAsset.h"
#include "core/assets/Material.h"
#include "core/scene-graph/Node.h"
#include "game/MaterialFactory.h"
#include "renderer/core/PassUtils.h"
#include "renderer/gfx-base/GFXTexture.h"

namespace cc {

CC_IMPLEMENT_CLASS(Sprite, "cc.Sprite", UIRenderer)
    .property("color",    &Sprite::_color,    Color{255, 255, 255, 255})
    .property("size",     &Sprite::_size,     Vec2{100.0f, 100.0f})
    .property("_texture", &Sprite::_texture)
CC_END_CLASS(Sprite);

namespace {
// Material cache keyed by Texture2D pointer. Same texture → same Material
// pointer → UIRenderer's batch key matches → batches fold together. Tint
// now comes from per-vertex colour, so the Material stays per-texture
// only (not per-colour).
ccstd::unordered_map<Texture2D *, IntrusivePtr<Material>> g_spriteTexturedCache;
IntrusivePtr<Material> g_spriteUntexturedFallback;

IntrusivePtr<Material> buildTexturedMat(Texture2D *tex) {
    auto *effect = EffectAsset::get("builtin-unlit");
    if (!effect) return nullptr;

    MacroRecord defines{
        {"USE_TEXTURE",       true},
        {"USE_VERTEX_COLOR",  true},
        // Disable scene fog on UI — see Label.cpp for the full note.
        {"CC_USE_FOG",        4},
    };
    IMaterialInfo info;
    info.effectAsset = effect;
    info.technique   = 1u;  // transparent
    info.defines     = IMaterialInfo::DefinesType{defines};

    auto *mat = ccnew Material();
    mat->initialize(info);
    mat->setPropertyTextureBase("mainTexture", tex);
    // mainColor stays white — per-sprite tint rides on vertex colour.
    return IntrusivePtr<Material>(mat);
}

// Shared untextured material — same USE_VERTEX_COLOR path as the textured
// build, just without USE_TEXTURE. All tint-only sprites can batch together
// because the Material pointer matches.
IntrusivePtr<Material> buildUntexturedVertexColorMat() {
    auto *effect = EffectAsset::get("builtin-unlit");
    if (!effect) return nullptr;

    MacroRecord defines{
        {"USE_VERTEX_COLOR",  true},
        {"CC_USE_FOG",        4},
    };
    IMaterialInfo info;
    info.effectAsset = effect;
    info.technique   = 1u;  // transparent
    info.defines     = IMaterialInfo::DefinesType{defines};

    auto *mat = ccnew Material();
    mat->initialize(info);
    return IntrusivePtr<Material>(mat);
}
}  // namespace

struct Sprite::Hooks {
    cc::event::TargetEventID<Node::SizeChanged> sizeId;
};

Sprite::Sprite() {
    _vertexStrideFloats = 9;  // position(3) + uv(2) + colour(4)
}

Sprite::~Sprite() = default;

void Sprite::onEnable() {
    UIRenderer::onEnable();
    syncSizeFromUITransform();
    if (!_hooks) {
        _hooks = new Hooks();
        if (auto *n = getNode()) {
            _hooks->sizeId = n->on<Node::SizeChanged>([this](Node * /*self*/) {
                syncSizeFromUITransform();
            });
        }
    }
}

void Sprite::onDisable() {
    if (_hooks) {
        if (auto *n = getNode()) {
            n->off<Node::SizeChanged>(_hooks->sizeId);
        }
        delete _hooks;
        _hooks = nullptr;
    }
    UIRenderer::onDisable();
}

void Sprite::syncSizeFromUITransform() {
    auto *n = getNode();
    if (!n) return;
    auto *ui = n->getComponent<UITransform>();
    if (!ui) return;
    const Vec2 &sz = ui->getContentSize();
    if (_size.x == sz.x && _size.y == sz.y) return;
    _size = sz;
    markDirty();
}

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
        gfx::Attribute{gfx::ATTR_NAME_COLOR,     gfx::Format::RGBA32F},
    };
}

gfx::Texture *Sprite::resolveBatchTexture() const {
    return (_texture && _texture->getGFXTexture()) ? _texture->getGFXTexture() : nullptr;
}

void Sprite::updateGeometry() {
    _vertexStrideFloats = 9;
    const float hx = _size.x * 0.5f;
    const float hy = _size.y * 0.5f;
    const float r  = _color.r / 255.0f;
    const float g  = _color.g / 255.0f;
    const float b  = _color.b / 255.0f;
    const float a  = _color.a / 255.0f;

    // UV convention: (0,0) top-left of image; engine pre-flips Y on load.
    _vertexData = {
        -hx, -hy, 0.0f,   0.0f, 1.0f,   r, g, b, a,   // bottom-left
         hx, -hy, 0.0f,   1.0f, 1.0f,   r, g, b, a,   // bottom-right
         hx,  hy, 0.0f,   1.0f, 0.0f,   r, g, b, a,   // top-right
        -hx,  hy, 0.0f,   0.0f, 0.0f,   r, g, b, a,   // top-left
    };
    _indexData = {0, 1, 2, 0, 2, 3};
    _vertexCount = 4;
    _indexCount  = 6;
}

IntrusivePtr<Material> Sprite::resolveMaterial() {
    if (!_texture || !_texture->getGFXTexture()) {
        // Shared un-textured material — USE_VERTEX_COLOR=1 so each sprite's
        // setColor still shows up via the baked per-corner vertex tint.
        if (!g_spriteUntexturedFallback) {
            g_spriteUntexturedFallback = buildUntexturedVertexColorMat();
        }
        return g_spriteUntexturedFallback;
    }

    auto it = g_spriteTexturedCache.find(_texture.get());
    if (it != g_spriteTexturedCache.end()) return it->second;

    auto mat = buildTexturedMat(_texture.get());
    if (mat) g_spriteTexturedCache[_texture.get()] = mat;
    return mat;
}

}  // namespace cc
