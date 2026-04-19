#pragma once

#include <cstdint>

namespace cc {

class Texture2D;

// Atlas-addressed glyph metrics, shared by every TextFont backend.
//
// Fields match AngelCode BMFont semantics so the existing BmfFont
// loader and Label rendering path keep working unchanged. TtfFont
// produces the same structure — the only difference is that TtfFont
// fills it in on demand via FreeType, while BmfFont reads it out of a
// pre-baked .fnt file.
//
// All measurements are in atlas pixels. `xoffset/yoffset` is the pen
// offset (bearing); `xadvance` is how far the pen moves after drawing
// this glyph; `x,y,w,h` is the glyph's rectangle in the source atlas.
struct FontLetterDef {
    uint16_t x{0};      // atlas rect top-left (atlas has y-down origin)
    uint16_t y{0};
    uint16_t w{0};
    uint16_t h{0};
    int16_t  xoffset{0};  // bearing X (pen → glyph left edge)
    int16_t  yoffset{0};  // bearing Y (line-top → glyph top, BMFont convention)
    int16_t  xadvance{0}; // pen advance after drawing
};

// Abstract text-source interface consumed by Label. Two backends ship:
//
//   • BmfFont — pre-rasterised AngelCode BMFont atlas. Glyphs and
//     their metrics are fully known at load time; getGlyph is a pure
//     lookup.
//   • TtfFont — runtime FreeType rasterisation into a dynamic atlas.
//     getGlyph rasterises and inserts on first miss; subsequent calls
//     hit the cache.
//
// Named `TextFont` rather than plain `Font` to avoid colliding with
// the legacy `cc::Font` asset wrapper in core/assets/Font.h (still
// used by the profiler / debug renderer). `TextFont` lives in the 2d
// text-rendering layer and has no asset-lifecycle machinery.
class TextFont {
public:
    virtual ~TextFont();  // defined in TextFont.cpp — vtable anchor

    // Returns the letter definition for `codepoint`, or nullptr if
    // the font can't supply one. For TTF this may trigger a lazy
    // rasterisation on the first call for a given codepoint — which
    // can upload texture data, so it's a non-const operation.
    virtual const FontLetterDef *getGlyph(uint32_t codepoint) = 0;

    // GPU texture Label binds when rendering glyphs from this font.
    // For BmfFont this is fixed at load time; for TtfFont this is the
    // dynamic atlas which may have had new glyphs written to it since
    // the last call.
    virtual Texture2D *getAtlas() const = 0;

    virtual uint16_t getAtlasWidth() const = 0;
    virtual uint16_t getAtlasHeight() const = 0;

    // Pen advance between successive baselines in atlas pixels.
    virtual uint16_t getLineHeight() const = 0;

    // Reference point size the glyph metrics are quoted in, used by
    // Label::fontSize to compute a scale factor. For TtfFont this is
    // the size passed to load(); for BmfFont this is the
    // <info size=...> attribute.
    virtual uint16_t getBaseFontSize() const = 0;
};

}  // namespace cc
