#include "cocos/2d/components/Label.h"

#include "base/Log.h"
#include "base/std/container/unordered_map.h"
#include "cocos/2d/text/BmfFont.h"
#include "core/assets/EffectAsset.h"
#include "core/assets/Material.h"
#include "core/assets/Texture2D.h"
#include "renderer/gfx-base/GFXTexture.h"

namespace cc {

CC_IMPLEMENT_CLASS(Label, "cc.Label", UIRenderer)
    .property("text",  &Label::_text)
    .property("color", &Label::_color, Color{255, 255, 255, 255})
CC_END_CLASS(Label);

namespace {
// Shared per-atlas Material cache. Two Labels using the same BmfFont
// share the same atlas Texture2D → same Material → same batch key.
ccstd::unordered_map<Texture2D *, IntrusivePtr<Material>> g_labelMatCache;

IntrusivePtr<Material> buildLabelMat(Texture2D *atlas) {
    auto *effect = EffectAsset::get("builtin-unlit");
    if (!effect) return nullptr;

    MacroRecord defines{
        {"USE_TEXTURE",      true},
        {"USE_VERTEX_COLOR", true},
    };
    IMaterialInfo info;
    info.effectAsset = effect;
    info.technique   = 1u;  // transparent
    info.defines     = IMaterialInfo::DefinesType{defines};

    auto *mat = ccnew Material();
    mat->initialize(info);
    mat->setPropertyTextureBase("mainTexture", atlas);
    // mainColor stays white — per-label tint rides on vertex colour.
    return IntrusivePtr<Material>(mat);
}
}  // namespace

Label::Label() {
    _vertexStrideFloats = 9;  // position(3) + uv(2) + colour(4)
}

Label::~Label() = default;

void Label::setFont(BmfFont *font) {
    if (_font == font) return;
    _font = font;
    markDirty();
}

void Label::setText(const ccstd::string &text) {
    if (_text == text) return;
    _text = text;
    markDirty();
}

void Label::setColor(const Color &c) {
    _color = c;
    markDirty();
}

ccstd::vector<gfx::Attribute> Label::vertexAttributes() const {
    return {
        gfx::Attribute{gfx::ATTR_NAME_POSITION,  gfx::Format::RGB32F},
        gfx::Attribute{gfx::ATTR_NAME_TEX_COORD, gfx::Format::RG32F},
        gfx::Attribute{gfx::ATTR_NAME_COLOR,     gfx::Format::RGBA32F},
    };
}

gfx::Texture *Label::resolveBatchTexture() const {
    if (!_font) return nullptr;
    auto *atlas = _font->getAtlas();
    return (atlas && atlas->getGFXTexture()) ? atlas->getGFXTexture() : nullptr;
}

void Label::updateGeometry() {
    _vertexStrideFloats = 9;
    _vertexData.clear();
    _indexData.clear();
    _vertexCount = 0;
    _indexCount  = 0;

    if (!_font || _text.empty()) return;

    const auto atlasW = static_cast<float>(_font->getAtlasWidth());
    const auto atlasH = static_cast<float>(_font->getAtlasHeight());
    if (atlasW <= 0.0f || atlasH <= 0.0f) return;

    const float r = _color.r / 255.0f;
    const float g = _color.g / 255.0f;
    const float b = _color.b / 255.0f;
    const float a = _color.a / 255.0f;

    // First pass: total advance to centre horizontally.
    float totalAdvance = 0.0f;
    for (unsigned char c : _text) {
        const auto *gl = _font->getGlyph(static_cast<uint32_t>(c));
        if (!gl) continue;
        totalAdvance += static_cast<float>(gl->xadvance);
    }
    float penX = -totalAdvance * 0.5f;

    _vertexData.reserve(_text.size() * 4 * _vertexStrideFloats);
    _indexData.reserve(_text.size() * 6);

    for (unsigned char c : _text) {
        const auto *gl = _font->getGlyph(static_cast<uint32_t>(c));
        if (!gl) continue;

        if (gl->w > 0 && gl->h > 0) {
            const float x0 = penX + static_cast<float>(gl->xoffset);
            const float x1 = x0 + static_cast<float>(gl->w);
            const float y0 = -static_cast<float>(gl->yoffset);        // top in world-Y-up
            const float y1 = y0 - static_cast<float>(gl->h);          // bottom

            const float u0 = static_cast<float>(gl->x) / atlasW;
            const float u1 = static_cast<float>(gl->x + gl->w) / atlasW;
            const float v0 = static_cast<float>(gl->y) / atlasH;
            const float v1 = static_cast<float>(gl->y + gl->h) / atlasH;

            const auto base = static_cast<uint16_t>(_vertexCount);
            _vertexData.insert(_vertexData.end(), {x0, y1, 0.0f, u0, v1, r, g, b, a});
            _vertexData.insert(_vertexData.end(), {x1, y1, 0.0f, u1, v1, r, g, b, a});
            _vertexData.insert(_vertexData.end(), {x1, y0, 0.0f, u1, v0, r, g, b, a});
            _vertexData.insert(_vertexData.end(), {x0, y0, 0.0f, u0, v0, r, g, b, a});

            _indexData.insert(_indexData.end(), {
                base,
                static_cast<uint16_t>(base + 1),
                static_cast<uint16_t>(base + 2),
                base,
                static_cast<uint16_t>(base + 2),
                static_cast<uint16_t>(base + 3),
            });
            _vertexCount += 4;
            _indexCount  += 6;
        }

        penX += static_cast<float>(gl->xadvance);
    }
}

IntrusivePtr<Material> Label::resolveMaterial() {
    if (!_font || !_font->getAtlas() || !_font->getAtlas()->getGFXTexture()) {
        CC_LOG_WARNING("[Label] font/atlas not ready — no material built");
        return nullptr;
    }

    auto *atlas = _font->getAtlas();
    auto it = g_labelMatCache.find(atlas);
    if (it != g_labelMatCache.end()) return it->second;

    auto mat = buildLabelMat(atlas);
    if (mat) g_labelMatCache[atlas] = mat;
    return mat;
}

}  // namespace cc
