#include "cocos/2d/text/TtfFont.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include "base/Log.h"
#include "platform/FileUtils.h"

namespace cc {

// PIMPL for FreeType handles so TtfFont.h doesn't leak ft2build.h
// onto every translation unit that includes a font pointer.
struct TtfFont::Impl {
    FT_Face face{nullptr};

    // Lazily-allocated process-wide FT_Library. Released at program
    // exit via the static wrapper below; FreeType's own lifetime is
    // refcounted internally so this is safe against repeated init.
    static FT_Library library() {
        static FT_Library lib = nullptr;
        if (!lib) {
            FT_Error err = FT_Init_FreeType(&lib);
            if (err) {
                CC_LOG_ERROR("[TtfFont] FT_Init_FreeType failed (err=%d)", err);
                lib = nullptr;
            }
        }
        return lib;
    }
};

TtfFont::TtfFont() : _impl(std::make_unique<Impl>()) {}

TtfFont::~TtfFont() {
    if (_impl && _impl->face) {
        FT_Done_Face(_impl->face);
        _impl->face = nullptr;
    }
}

bool TtfFont::load(const ccstd::string &ttfPath, uint16_t fontSize) {
    if (fontSize == 0) {
        CC_LOG_ERROR("[TtfFont] fontSize=0 not allowed");
        return false;
    }

    auto *fu = FileUtils::getInstance();
    ccstd::string resolved = fu->isFileExist(ttfPath) ? ttfPath
                                                       : fu->fullPathForFilename(ttfPath);
    if (resolved.empty() || !fu->isFileExist(resolved)) {
        CC_LOG_ERROR("[TtfFont] .ttf not found: %s", ttfPath.c_str());
        return false;
    }

    _fileData = fu->getStringFromFile(resolved);
    if (_fileData.empty()) {
        CC_LOG_ERROR("[TtfFont] .ttf empty: %s", resolved.c_str());
        return false;
    }

    FT_Library lib = Impl::library();
    if (!lib) return false;

    // FT_New_Memory_Face holds a zero-copy pointer into _fileData.
    FT_Error err = FT_New_Memory_Face(
        lib,
        reinterpret_cast<const FT_Byte *>(_fileData.data()),
        static_cast<FT_Long>(_fileData.size()),
        /*faceIndex*/ 0,
        &_impl->face);
    if (err) {
        CC_LOG_ERROR("[TtfFont] FT_New_Memory_Face failed (err=%d) for %s",
                     err, resolved.c_str());
        _fileData.clear();
        return false;
    }

    err = FT_Set_Pixel_Sizes(_impl->face, /*width*/ 0, fontSize);
    if (err) {
        CC_LOG_ERROR("[TtfFont] FT_Set_Pixel_Sizes(%u) failed (err=%d)",
                     fontSize, err);
        FT_Done_Face(_impl->face);
        _impl->face = nullptr;
        _fileData.clear();
        return false;
    }

    _fontSize = fontSize;
    // FT `metrics.height` is in 26.6 fixed point; shift right by 6 for pixels.
    _lineHeight = static_cast<uint16_t>(_impl->face->size->metrics.height >> 6);
    _ascender   = static_cast<uint16_t>(_impl->face->size->metrics.ascender >> 6);

    _atlas = std::make_unique<FontAtlas>(/*w*/ 1024, /*h*/ 1024);

    CC_LOG_INFO("[TtfFont] loaded '%s' @ %upx  lineHeight=%u",
                resolved.c_str(), _fontSize, _lineHeight);
    return true;
}

const FontLetterDef *TtfFont::getGlyph(uint32_t codepoint) {
    auto it = _glyphs.find(codepoint);
    if (it != _glyphs.end()) return &it->second;
    return rasterizeGlyph(codepoint);
}

const FontLetterDef *TtfFont::rasterizeGlyph(uint32_t codepoint) {
    if (!_impl || !_impl->face || !_atlas) return nullptr;

    FT_Face face = _impl->face;
    FT_Error err = FT_Load_Char(face, codepoint, FT_LOAD_RENDER);
    if (err) {
        // Log-spamming a whole string could get noisy; keep at WARNING
        // level only — the caller falls back gracefully on nullptr.
        CC_LOG_WARNING("[TtfFont] FT_Load_Char(%u) failed (err=%d)",
                       codepoint, err);
        return nullptr;
    }

    const FT_GlyphSlot slot = face->glyph;
    const FT_Bitmap &bmp = slot->bitmap;

    FontLetterDef def;
    def.w = static_cast<uint16_t>(bmp.width);
    def.h = static_cast<uint16_t>(bmp.rows);
    // bitmap_left is bearingX (pen → glyph left edge); same sign as
    // BMFont's xoffset.
    def.xoffset = static_cast<int16_t>(slot->bitmap_left);
    // FreeType's bitmap_top is the baseline→glyph-top distance (Y up);
    // BMFont's yoffset is line-top→glyph-top (Y down). Convert by
    // subtracting from ascender: yoffset = ascender - bitmap_top.
    // `face->size->metrics.ascender` is 26.6 fixed.
    const int32_t ascenderPx = face->size->metrics.ascender >> 6;
    def.yoffset = static_cast<int16_t>(ascenderPx - slot->bitmap_top);
    def.xadvance = static_cast<int16_t>(slot->advance.x >> 6);

    // Some glyphs (e.g. space) have 0×0 bitmaps but still advance.
    // FontAtlas::insert handles that gracefully (returns valid slot
    // without writing pixels).
    auto alloc = _atlas->insert(def.w, def.h, bmp.buffer, bmp.pitch);
    if (!alloc.ok) {
        CC_LOG_WARNING("[TtfFont] atlas full, dropping glyph %u", codepoint);
        return nullptr;
    }
    def.x = alloc.x;
    def.y = alloc.y;

    auto [ins, inserted] = _glyphs.emplace(codepoint, def);
    return &ins->second;
}

}  // namespace cc
