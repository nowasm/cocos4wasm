#pragma once

#include <memory>

#include "base/std/container/string.h"
#include "base/std/container/unordered_map.h"
#include "cocos/2d/text/FontAtlas.h"
#include "cocos/2d/text/TextFont.h"

namespace cc {

// Runtime TTF rasteriser built on FreeType. One instance = one
// (.ttf file, fontSize) pair — a different size of the same file is
// a separate TtfFont. (FontAtlasCache-style sharing between multiple
// Labels asking for the same (path, size) is a follow-up; for now
// each TtfFont owns its own atlas.)
//
// Glyph flow:
//   setFontSize(32) + load("OpenSans.ttf")
//     → FT_New_Memory_Face, FT_Set_Pixel_Sizes(32)
//   Label::updateGeometry iterates codepoints → calls getGlyph('A')
//     → first call: FT_Load_Char + FT_Render_Glyph → FontAtlas::insert
//       → bitmap written to atlas, glyph metrics cached, returned
//     → later calls: pure cache hit
//
// TextFont::getAtlas returns the internal FontAtlas's Texture2D.
// Label picks up dynamic UVs from the atlas regardless of whether
// the glyph existed at load time or was added this frame.
class TtfFont : public TextFont {
public:
    TtfFont();
    ~TtfFont() override;

    TtfFont(const TtfFont &) = delete;
    TtfFont &operator=(const TtfFont &) = delete;

    // Loads the .ttf and sets its rasterisation size. Returns false
    // on parse / I/O error (errors logged internally). The file data
    // is held for the lifetime of this TtfFont — FT_Face references
    // it zero-copy.
    bool load(const ccstd::string &ttfPath, uint16_t fontSize);

    // --- TextFont interface ---------------------------------------------

    const FontLetterDef *getGlyph(uint32_t codepoint) override;

    Texture2D *getAtlas() const override { return _atlas ? _atlas->getTexture() : nullptr; }
    uint16_t getAtlasWidth()  const override { return _atlas ? _atlas->getWidth()  : 0; }
    uint16_t getAtlasHeight() const override { return _atlas ? _atlas->getHeight() : 0; }

    uint16_t getLineHeight()   const override { return _lineHeight; }
    uint16_t getBaseFontSize() const override { return _fontSize; }

private:
    // Lazy-rasterise a single glyph into the atlas, cache the metrics.
    // Returns nullptr if FreeType can't produce the glyph or the
    // atlas is full.
    const FontLetterDef *rasterizeGlyph(uint32_t codepoint);

    // FreeType handles are raw pointers hidden behind an impl PIMPL —
    // keeps this header free of <ft2build.h> machinery.
    struct Impl;
    std::unique_ptr<Impl> _impl;

    // Owned file bytes — FreeType's FT_Face holds a zero-copy pointer
    // into this buffer for the lifetime of the face.
    ccstd::string _fileData;

    std::unique_ptr<FontAtlas> _atlas;
    ccstd::unordered_map<uint32_t, FontLetterDef> _glyphs;

    uint16_t _fontSize{0};
    uint16_t _lineHeight{0};
};

}  // namespace cc
