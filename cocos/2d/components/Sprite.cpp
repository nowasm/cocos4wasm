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

// All fields carry the upstream underscore-prefixed JSON names. `_size`
// is our own internal geometry cache — not serialised by Editor, so it
// stays unreflected (UITransform._contentSize drives the visible quad).
CC_IMPLEMENT_CLASS(Sprite, "cc.Sprite", UIRenderer)
    .property("_color",       &Sprite::_color, Color{255, 255, 255, 255})
    .property("_spriteFrame", &Sprite::_spriteFrame)
    .property("_type",        &Sprite::_type,  Sprite::Type::SIMPLE)
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

void Sprite::setType(Type t) {
    if (_type == t) return;
    _type = t;
    markDirty();
}

void Sprite::setSpriteFrame(SpriteFrame *sf) {
    if (_spriteFrame.get() == sf) return;
    _spriteFrame = sf;
    markDirty();
}

void Sprite::setTexture(Texture2D *tex) {
    // Reuse an existing SpriteFrame if one is already attached — swapping
    // out the backing texture is a common transition pattern (Button's
    // state sprites, PageViewIndicator dot retint). Otherwise wrap the
    // texture in a fresh full-rect SpriteFrame.
    if (_spriteFrame) {
        if (_spriteFrame->getTexture() == tex) return;
        _spriteFrame->setTextureAndResetRect(tex);
        markDirty();
        return;
    }
    if (tex) {
        auto *sf = ccnew SpriteFrame();
        sf->setTextureAndResetRect(tex);
        _spriteFrame = sf;
    }
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
    Texture2D *tex = _spriteFrame ? _spriteFrame->getTexture() : nullptr;
    return (tex && tex->getGFXTexture()) ? tex->getGFXTexture() : nullptr;
}

void Sprite::updateGeometry() {
    _vertexStrideFloats = 9;
    const float r  = _color.r / 255.0f;
    const float g  = _color.g / 255.0f;
    const float b  = _color.b / 255.0f;
    const float a  = _color.a / 255.0f;

    // Resolve atlas UV sub-rect. Fallback to full-texture (0..1) when no
    // SpriteFrame / texture is available yet. SpriteFrame::_rect is
    // (x, y, w, h) in atlas pixel space.
    float u0 = 0.f, u1 = 1.f, v0 = 0.f, v1 = 1.f;
    float rectL = 0.f, rectT = 0.f, rectW = _size.x, rectH = _size.y;
    if (_spriteFrame) {
        const Vec4 &rc = _spriteFrame->getRect();
        rectL = rc.x;
        rectT = rc.y;
        rectW = rc.z;
        rectH = rc.w;
        if (auto *tex = _spriteFrame->getTexture()) {
            const float tw = static_cast<float>(tex->getWidth());
            const float th = static_cast<float>(tex->getHeight());
            if (tw > 0.f && th > 0.f) {
                u0 = rectL / tw;
                u1 = (rectL + rectW) / tw;
                v0 = rectT / th;
                v1 = (rectT + rectH) / th;
            }
        }
    }

    // Resolve anchor from UITransform so the sprite rect aligns the same
    // way the engine places any other UI content. `anchorPoint` default
    // is (0.5, 0.5), which reduces the general form below to the
    // centred rect the MVP used previously. Non-centred anchors
    // (e.g. ProgressBar's Bar uses (0, 0.5) so its left edge pins to
    // the node position) now render correctly.
    float ax = 0.5f, ay = 0.5f;
    if (auto *n = getNode()) {
        if (auto *ui = n->getComponent<UITransform>()) {
            const Vec2 ap = ui->getAnchorPoint();
            ax = ap.x;
            ay = ap.y;
        }
    }
    const float xLo = -ax * _size.x;
    const float xHi = (1.f - ax) * _size.x;
    const float yLo = -ay * _size.y;
    const float yHi = (1.f - ay) * _size.y;

    const bool sliced =
        (_type == Type::SLICED) && _spriteFrame &&
        (_spriteFrame->getInsetLeft() + _spriteFrame->getInsetRight() +
         _spriteFrame->getInsetTop()  + _spriteFrame->getInsetBottom()) > 0.f;

    if (!sliced) {
        // SIMPLE: whole (possibly sub-rect) frame stretched to the quad.
        // UV (0,0) is top-left; engine pre-flips Y on load.
        _vertexData = {
            xLo, yLo, 0.0f,   u0, v1,   r, g, b, a,   // bottom-left
            xHi, yLo, 0.0f,   u1, v1,   r, g, b, a,   // bottom-right
            xHi, yHi, 0.0f,   u1, v0,   r, g, b, a,   // top-right
            xLo, yHi, 0.0f,   u0, v0,   r, g, b, a,   // top-left
        };
        _indexData = {0, 1, 2, 0, 2, 3};
        _vertexCount = 4;
        _indexCount  = 6;
        return;
    }

    // 9-slice: 4×4 vertex grid → 9 sub-quads. Corners keep their atlas
    // size; edges stretch in one axis; centre stretches in both. If the
    // target size is smaller than the cap sum on an axis, the caps
    // collapse proportionally so neither side overshoots the middle.
    float capL = _spriteFrame->getInsetLeft();
    float capR = _spriteFrame->getInsetRight();
    float capT = _spriteFrame->getInsetTop();
    float capB = _spriteFrame->getInsetBottom();

    const float capSumX = capL + capR;
    if (capSumX > 0.f && capSumX > _size.x) {
        const float s = _size.x / capSumX;
        capL *= s; capR *= s;
    }
    const float capSumY = capT + capB;
    if (capSumY > 0.f && capSumY > _size.y) {
        const float s = _size.y / capSumY;
        capT *= s; capB *= s;
    }

    // X bands (0=left quad edge, 3=right quad edge), anchor-aware.
    const float xs[4] = {xLo, xLo + capL, xHi - capR, xHi};
    // Y bands (0=bottom, 3=top).
    const float ys[4] = {yLo, yLo + capB, yHi - capT, yHi};

    // UV bands: u grows L→R, v grows T→B (y-flipped relative to local y).
    // Texture-pixel widths are computed in ATLAS space and divided by
    // atlas dims, not by rect — that's how the caller-specified
    // capInsets in pixel units map to the atlas.
    float texW = 1.f, texH = 1.f;
    if (_spriteFrame) {
        if (auto *tex = _spriteFrame->getTexture()) {
            if (tex->getWidth())  texW = static_cast<float>(tex->getWidth());
            if (tex->getHeight()) texH = static_cast<float>(tex->getHeight());
        }
    }
    const float us[4] = {u0,
                         u0 + _spriteFrame->getInsetLeft()  / texW,
                         u1 - _spriteFrame->getInsetRight() / texW,
                         u1};
    // vs: row 0 (bottom-of-quad) gets v1 (bottom-of-atlas); row 3 (top)
    // gets v0 (top-of-atlas) — matches SIMPLE's bottom/top UV pairing.
    const float vs[4] = {v1,
                         v1 - _spriteFrame->getInsetBottom() / texH,
                         v0 + _spriteFrame->getInsetTop()    / texH,
                         v0};

    _vertexData.clear();
    _vertexData.reserve(16 * _vertexStrideFloats);
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            _vertexData.insert(_vertexData.end(),
                {xs[col], ys[row], 0.0f, us[col], vs[row], r, g, b, a});
        }
    }

    _indexData.clear();
    _indexData.reserve(9 * 6);
    auto vi = [](int row, int col) { return static_cast<uint16_t>(row * 4 + col); };
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            // Two triangles per cell, same winding as SIMPLE's quad.
            _indexData.push_back(vi(row,     col));
            _indexData.push_back(vi(row,     col + 1));
            _indexData.push_back(vi(row + 1, col + 1));
            _indexData.push_back(vi(row,     col));
            _indexData.push_back(vi(row + 1, col + 1));
            _indexData.push_back(vi(row + 1, col));
        }
    }
    _vertexCount = 16;
    _indexCount  = 54;
}

IntrusivePtr<Material> Sprite::resolveMaterial() {
    Texture2D *tex = _spriteFrame ? _spriteFrame->getTexture() : nullptr;
    if (!tex || !tex->getGFXTexture()) {
        // Shared un-textured material — USE_VERTEX_COLOR=1 so each sprite's
        // setColor still shows up via the baked per-corner vertex tint.
        if (!g_spriteUntexturedFallback) {
            g_spriteUntexturedFallback = buildUntexturedVertexColorMat();
        }
        return g_spriteUntexturedFallback;
    }

    auto it = g_spriteTexturedCache.find(tex);
    if (it != g_spriteTexturedCache.end()) return it->second;

    auto mat = buildTexturedMat(tex);
    if (mat) g_spriteTexturedCache[tex] = mat;
    return mat;
}

}  // namespace cc
