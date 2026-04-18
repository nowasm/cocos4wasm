#include "cocos/2d/components/Label.h"

#include "base/Log.h"
#include "cocos/2d/text/BmfFont.h"
#include "core/assets/EffectAsset.h"
#include "core/assets/Material.h"

namespace cc {

CC_IMPLEMENT_CLASS(Label, "cc.Label", UIRenderer)
    .property("text",  &Label::_text)
    .property("color", &Label::_color, Color{255, 255, 255, 255})
CC_END_CLASS(Label);

Label::Label() {
    _vertexStrideFloats = 5;  // position(3) + uv(2)
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
    };
}

void Label::updateGeometry() {
    _vertexStrideFloats = 5;
    _vertexData.clear();
    _indexData.clear();
    _vertexCount = 0;
    _indexCount  = 0;

    if (!_font || _text.empty()) return;

    const auto atlasW = static_cast<float>(_font->getAtlasWidth());
    const auto atlasH = static_cast<float>(_font->getAtlasHeight());
    if (atlasW <= 0.0f || atlasH <= 0.0f) return;

    // First pass: compute total width to centre the label horizontally.
    float totalAdvance = 0.0f;
    for (unsigned char c : _text) {
        const auto *g = _font->getGlyph(static_cast<uint32_t>(c));
        if (!g) continue;
        totalAdvance += static_cast<float>(g->xadvance);
    }

    // Pen origin at (-totalAdvance/2, 0). BMFont yoffset is measured from
    // the top of the line downward; our world Y points up, so glyph top
    // is -yoffset (i.e. yoffset 5 means glyph starts 5 units BELOW 0).
    float penX = -totalAdvance * 0.5f;

    _vertexData.reserve(_text.size() * 4 * _vertexStrideFloats);
    _indexData.reserve(_text.size() * 6);

    for (unsigned char c : _text) {
        const auto *g = _font->getGlyph(static_cast<uint32_t>(c));
        if (!g) {
            continue;
        }

        if (g->w > 0 && g->h > 0) {
            const float x0 = penX + static_cast<float>(g->xoffset);
            const float x1 = x0 + static_cast<float>(g->w);
            const float y0 = -static_cast<float>(g->yoffset);        // top in world-Y-up
            const float y1 = y0 - static_cast<float>(g->h);          // bottom

            const float u0 = static_cast<float>(g->x) / atlasW;
            const float u1 = static_cast<float>(g->x + g->w) / atlasW;
            const float v0 = static_cast<float>(g->y) / atlasH;
            const float v1 = static_cast<float>(g->y + g->h) / atlasH;

            const auto base = static_cast<uint16_t>(_vertexCount);
            // quad: BL, BR, TR, TL
            _vertexData.insert(_vertexData.end(), {x0, y1, 0.0f, u0, v1});
            _vertexData.insert(_vertexData.end(), {x1, y1, 0.0f, u1, v1});
            _vertexData.insert(_vertexData.end(), {x1, y0, 0.0f, u1, v0});
            _vertexData.insert(_vertexData.end(), {x0, y0, 0.0f, u0, v0});

            _indexData.insert(_indexData.end(), {
                base, static_cast<uint16_t>(base + 1), static_cast<uint16_t>(base + 2),
                base, static_cast<uint16_t>(base + 2), static_cast<uint16_t>(base + 3),
            });
            _vertexCount += 4;
            _indexCount  += 6;
        }

        penX += static_cast<float>(g->xadvance);
    }
}

IntrusivePtr<Material> Label::resolveMaterial() {
    if (!_font || !_font->getAtlas() || !_font->getAtlas()->getGFXTexture()) {
        CC_LOG_WARNING("[Label] font/atlas not ready — no material built");
        return nullptr;
    }

    auto *effect = EffectAsset::get("builtin-unlit");
    if (!effect) {
        CC_LOG_ERROR("[Label] builtin-unlit effect missing");
        return nullptr;
    }

    MacroRecord defines{{"USE_TEXTURE", true}};
    IMaterialInfo info;
    info.effectAsset = effect;
    info.technique   = 1u;  // transparent — src_alpha / one_minus_src_alpha
    info.defines     = IMaterialInfo::DefinesType{defines};

    auto *mat = ccnew Material();
    mat->initialize(info);
    mat->setPropertyTextureBase("mainTexture", _font->getAtlas());
    mat->setPropertyColor("mainColor", _color);
    return mat;
}

}  // namespace cc
