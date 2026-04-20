#pragma once

#include <cstdint>
#include "base/Ptr.h"
#include "base/std/container/string.h"
#include "base/std/container/unordered_map.h"
#include "cocos/2d/text/TextFont.h"
#include "core/assets/Texture2D.h"

namespace cc {

// Back-compat alias — the original glyph struct was called BmfGlyph
// and is referenced by downstream code (EditBoxImpl's measureAdvance,
// various scenes). The unified type now lives in Font.h; the alias
// keeps source compatibility.
using BmfGlyph = FontLetterDef;

// AngelCode BMFont loader — parses a `.fnt` XML file and its companion
// atlas PNG into a Texture2D plus a flat glyph table. Implements the
// shared `TextFont` interface so Label / EditBox can treat BmfFont
// and TtfFont uniformly.
//
// Single-page atlases only (matches all OpenSans defaults we ship).
// No kerning parsing yet; add when a layout scene demands it.
class BmfFont : public TextFont {
public:
    BmfFont() = default;
    ~BmfFont() override = default;

    // Loads .fnt (XML) plus atlas PNG (resolved via the `<page file=...>`
    // entry, relative to the .fnt's directory). Returns false on any
    // parse / I/O failure; errors are logged internally.
    bool load(const ccstd::string &fntPath);

    // --- TextFont interface ---------------------------------------------

    const FontLetterDef *getGlyph(uint32_t code) override {
        auto it = _glyphs.find(code);
        return it == _glyphs.end() ? nullptr : &it->second;
    }
    Texture2D *getAtlas() const override { return _atlas.get(); }
    uint16_t   getAtlasWidth() const override { return _atlasW; }
    uint16_t   getAtlasHeight() const override { return _atlasH; }
    uint16_t   getLineHeight() const override { return _lineHeight; }
    uint16_t   getAscender()   const override {
        // Fallback to a conservative 0.8 × lineHeight if the .fnt omitted
        // the `base` attribute (rare, but some tools drop it).
        return _ascender > 0 ? _ascender
                             : static_cast<uint16_t>(_lineHeight * 4 / 5);
    }
    uint16_t   getBaseFontSize() const override { return _baseFontSize; }

private:
    IntrusivePtr<Texture2D> _atlas;
    uint16_t _atlasW{0};
    uint16_t _atlasH{0};
    uint16_t _lineHeight{0};
    uint16_t _ascender{0};
    uint16_t _baseFontSize{0};  // from <info size=...>
    ccstd::unordered_map<uint32_t, FontLetterDef> _glyphs;
};

}  // namespace cc
