#include "cocos/2d/components/Label.h"

#include "base/Log.h"
#include "base/std/container/unordered_map.h"
#include "cocos/2d/framework/UITransform.h"
#include "cocos/2d/text/TextFont.h"
#include "core/scene-graph/Node.h"
#include "core/assets/EffectAsset.h"
#include "core/assets/Material.h"
#include "core/assets/Texture2D.h"
#include "renderer/gfx-base/GFXTexture.h"

namespace cc {

// Upstream label.ts field naming verbatim — every serialised field is
// underscore-prefixed (protected). `_string` is the label text; our C++
// field is still `_text` but the reflection key tracks upstream.
CC_IMPLEMENT_CLASS(Label, "cc.Label", UIRenderer)
    .property("_string",         &Label::_text)
    .property("_color",          &Label::_color, Color{255, 255, 255, 255})
    .property("_horizontalAlign",&Label::_hAlign, Label::HorizontalAlign::CENTER)
    .property("_verticalAlign",  &Label::_vAlign, Label::VerticalAlign::CENTER)
    .property("_fontSize",       &Label::_fontSize,   -1.f)
    .property("_lineHeight",     &Label::_lineHeight, -1.f)
CC_END_CLASS(Label);

namespace {
// Shared per-atlas Material cache. Two Labels using the same TextFont
// share the same atlas Texture2D → same Material → same batch key.
ccstd::unordered_map<Texture2D *, IntrusivePtr<Material>> g_labelMatCache;

IntrusivePtr<Material> buildLabelMat(Texture2D *atlas) {
    auto *effect = EffectAsset::get("builtin-unlit");
    if (!effect) return nullptr;

    MacroRecord defines{
        {"USE_TEXTURE",      true},
        {"USE_VERTEX_COLOR", true},
        // Disable fog. builtin-unlit's CC_USE_FOG defaults to 0
        // (CC_FOG_LINEAR); with `cc_fogBase` uniforms left at their
        // scene defaults that produces a visible centre→edge radial
        // darken that makes UI text look washed out / tinted. Setting
        // 4 (CC_FOG_NONE) makes the vertex-side TRANSFER_FOG emit
        // factor=1.0, and CC_APPLY_FOG's mix becomes a no-op.
        {"CC_USE_FOG",       4},
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

// Size-change subscription bundle. Heap-allocated so Label.h doesn't
// need to pull in EventTarget.h template definitions just to declare a
// listener ID.
struct Label::SizeHook {
    cc::event::TargetEventID<Node::SizeChanged> id;
};

Label::Label() {
    _vertexStrideFloats = 9;  // position(3) + uv(2) + colour(4)
}

Label::~Label() {
    if (_sizeHook) {
        delete _sizeHook;
        _sizeHook = nullptr;
    }
}

void Label::onEnable() {
    UIRenderer::onEnable();
    if (auto *node = getNode(); node && !_sizeHook) {
        _sizeHook = new SizeHook();
        _sizeHook->id = node->on<Node::SizeChanged>(
            [this](Node *) { markDirty(); });
    }
}

void Label::onDisable() {
    if (_sizeHook) {
        if (auto *node = getNode()) {
            node->off<Node::SizeChanged>(_sizeHook->id);
        }
        delete _sizeHook;
        _sizeHook = nullptr;
    }
    UIRenderer::onDisable();
}

void Label::setFont(TextFont *font) {
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

void Label::setHorizontalAlign(HorizontalAlign v) {
    if (_hAlign == v) return;
    _hAlign = v;
    markDirty();
}

void Label::setVerticalAlign(VerticalAlign v) {
    if (_vAlign == v) return;
    _vAlign = v;
    markDirty();
}

void Label::setFontSize(float size) {
    if (_fontSize == size) return;
    _fontSize = size;
    markDirty();
}

void Label::setLineHeight(float h) {
    if (_lineHeight == h) return;
    _lineHeight = h;
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

    // Resolve the font-size scale. Glyph metrics in the atlas are
    // relative to `baseFontSize`; `_fontSize<=0` means "stay at
    // native" (scale 1.0). Fallback to native lineHeight if the .fnt
    // omitted <info size=>.
    const float baseSize = static_cast<float>(_font->getBaseFontSize());
    const float scale = (_fontSize > 0.f && baseSize > 0.f)
                            ? (_fontSize / baseSize)
                            : 1.f;

    const float nativeLineH = static_cast<float>(_font->getLineHeight()) * scale;
    const float lineH = (_lineHeight > 0.f) ? _lineHeight : nativeLineH;

    const float r = _color.r / 255.0f;
    const float g = _color.g / 255.0f;
    const float b = _color.b / 255.0f;
    const float a = _color.a / 255.0f;

    // Split on explicit '\n'. Each segment is one baseline; auto-wrap
    // is deferred.
    ccstd::vector<std::pair<size_t, size_t>> lines;  // (startByte, byteLen)
    {
        size_t start = 0;
        for (size_t i = 0; i <= _text.size(); ++i) {
            if (i == _text.size() || _text[i] == '\n') {
                lines.emplace_back(start, i - start);
                start = i + 1;
            }
        }
    }
    const int numLines = static_cast<int>(lines.size());

    // Content box for alignment — read from the Label's own Node
    // UITransform. If it's missing (Label created without a UI parent),
    // fall back to a (0,0) box which collapses alignment to CENTER —
    // visually matches the previous MVP behaviour.
    Vec2 contentSize{0.f, 0.f};
    if (auto *node = getNode()) {
        if (auto *ui = node->getComponent<UITransform>()) {
            contentSize = ui->getContentSize();
        }
    }

    // Pre-measure each line's advance so horizontal alignment can be
    // applied in one pass. Each entry is the total pen-advance the
    // line occupies *at the target scale*.
    ccstd::vector<float> lineAdvances;
    lineAdvances.reserve(numLines);
    for (int li = 0; li < numLines; ++li) {
        const size_t startByte = lines[li].first;
        const size_t byteLen   = lines[li].second;
        float adv = 0.f;
        for (size_t i = 0; i < byteLen; ++i) {
            const auto *gl = _font->getGlyph(
                static_cast<uint32_t>(
                    static_cast<unsigned char>(_text[startByte + i])));
            if (gl) adv += gl->xadvance * scale;
        }
        lineAdvances.push_back(adv);
    }

    // Vertical: resolve the baseline Y of the TOP line. Subsequent
    // lines march down by lineH.
    //
    // The coordinate frame is centred on the Label's node origin
    // (standard UI local-space), so the content box spans
    // [-h/2, h/2] × [-w/2, w/2]. "TOP aligned" means the first line's
    // BASELINE sits lineH below the top edge (so ascenders stay
    // inside the box); "BOTTOM" mirrors on the other side.
    //
    // For CENTER we mirror the upstream BMFont formula — expressed in
    // upstream's bottom-left-origin space:
    //   letterOffsetY = contentH/2 + actualFontSize/2   (for 1 line)
    // In our centred-origin Y-up frame that's
    //   baselineY = actualFontSize/2
    // The ascender metric would be a few pixels larger (FT includes
    // internal leading), and using it pushed the visible block above
    // the centre. Using `actualFontSize` — the rendered em height —
    // reproduces Cocos Creator's visual centring exactly: tall glyphs
    // like `b` span ±fontSize/2 around Y=0, lowercase x-height glyphs
    // ride through the middle.
    const float actualFontSize = (_fontSize > 0.f)
                                     ? _fontSize
                                     : static_cast<float>(_font->getBaseFontSize());
    float topBaselineY;
    switch (_vAlign) {
        case VerticalAlign::TOP:
            topBaselineY = contentSize.y * 0.5f - lineH * 0.5f;
            break;
        case VerticalAlign::BOTTOM:
            topBaselineY = -contentSize.y * 0.5f
                         + (static_cast<float>(numLines) - 0.5f) * lineH;
            break;
        case VerticalAlign::CENTER:
        default:
            topBaselineY = (static_cast<float>(numLines) - 1.f) * 0.5f * lineH
                         + actualFontSize * 0.5f;
            break;
    }

    _vertexData.reserve(_text.size() * 4 * _vertexStrideFloats);
    _indexData.reserve(_text.size() * 6);

    for (int li = 0; li < numLines; ++li) {
        const size_t startByte = lines[li].first;
        const size_t byteLen   = lines[li].second;
        const float lineAdvance = lineAdvances[li];

        // Horizontal: pen-start X.
        float penX;
        switch (_hAlign) {
            case HorizontalAlign::LEFT:
                penX = -contentSize.x * 0.5f;
                break;
            case HorizontalAlign::RIGHT:
                penX = contentSize.x * 0.5f - lineAdvance;
                break;
            case HorizontalAlign::CENTER:
            default:
                penX = -lineAdvance * 0.5f;
                break;
        }
        const float baselineY = topBaselineY - li * lineH;

        for (size_t i = 0; i < byteLen; ++i) {
            const auto *gl = _font->getGlyph(
                static_cast<uint32_t>(static_cast<unsigned char>(_text[startByte + i])));
            if (!gl) continue;

            if (gl->w > 0 && gl->h > 0) {
                const float x0 = penX + static_cast<float>(gl->xoffset) * scale;
                const float x1 = x0 + static_cast<float>(gl->w) * scale;
                const float y0 = baselineY - static_cast<float>(gl->yoffset) * scale;
                const float y1 = y0 - static_cast<float>(gl->h) * scale;

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

            penX += static_cast<float>(gl->xadvance) * scale;
        }
    }
}

IntrusivePtr<Material> Label::resolveMaterial() {
    if (!_font || !_font->getAtlas() || !_font->getAtlas()->getGFXTexture()) {
        // Font's atlas GFX texture may not be ready yet during the Label's
        // very first onEnable (EditBox auto-creates a Label child, whose
        // addComponent triggers onEnable before the caller gets a chance
        // to pass the font in). The next lateUpdate rebuilds once the
        // parent has called setFont, so this is informational only.
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
