#pragma once

#include <cstdint>
#include "base/Ptr.h"
#include "base/std/container/string.h"
#include "base/std/container/unordered_map.h"
#include "core/assets/Texture2D.h"

namespace cc {

// AngelCode BMFont loader — parses a `.fnt` XML file and its companion
// atlas PNG into a Texture2D plus a flat glyph table. Sidesteps the
// engine's BitmapFontFace, which exposes gfx::Texture* and can't be
// plugged into a Material's mainTexture slot on path A.
//
// Single-page atlases only (matches all OpenSans defaults we ship).
// No kerning parsing yet; add when a layout scene demands it.
struct BmfGlyph {
    uint16_t x{0};      // pixel rect in atlas (top-left origin)
    uint16_t y{0};
    uint16_t w{0};
    uint16_t h{0};
    int16_t  xoffset{0};  // bearing from pen-x to glyph left edge
    int16_t  yoffset{0};  // distance from line top to glyph top
    int16_t  xadvance{0}; // how far to advance pen-x after drawing
};

class BmfFont {
public:
    BmfFont() = default;
    ~BmfFont() = default;

    // Loads .fnt (XML) plus atlas PNG (resolved via the `<page file=...>`
    // entry, relative to the .fnt's directory). Returns false on any
    // parse / I/O failure; errors are logged internally.
    bool load(const ccstd::string &fntPath);

    Texture2D *getAtlas() const { return _atlas.get(); }
    uint16_t   getAtlasWidth() const { return _atlasW; }
    uint16_t   getAtlasHeight() const { return _atlasH; }
    uint16_t   getLineHeight() const { return _lineHeight; }

    const BmfGlyph *getGlyph(uint32_t code) const {
        auto it = _glyphs.find(code);
        return it == _glyphs.end() ? nullptr : &it->second;
    }

private:
    IntrusivePtr<Texture2D> _atlas;
    uint16_t _atlasW{0};
    uint16_t _atlasH{0};
    uint16_t _lineHeight{0};
    ccstd::unordered_map<uint32_t, BmfGlyph> _glyphs;
};

}  // namespace cc
