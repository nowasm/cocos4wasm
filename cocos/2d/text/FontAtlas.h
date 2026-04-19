#pragma once

#include <cstdint>

#include "base/Ptr.h"
#include "base/std/container/vector.h"
#include "core/assets/Texture2D.h"

namespace cc {

// Dynamic font atlas that packs newly-rasterised glyph bitmaps into a
// single RGBA8 Texture2D using a simple shelf packer. Consumed by
// TtfFont (and eventually any other runtime-rasterising backend).
//
// Pixel layout: RGBA8 with R=G=B=255 and A=luminance. Matches the
// BMFont alpha-mask convention TextureLoader already uses, so the
// shared `builtin-unlit` material draws text correctly without a
// dedicated shader variant.
//
// Strategy for v1:
//   * Single page, default 1024×1024. When full, `insert` returns
//     `{ok=false}` and the caller (TtfFont) drops the glyph —
//     multi-page / LRU eviction is a later improvement.
//   * Shelf packing: rows of glyphs left-to-right, wrap to the next
//     shelf when the row fills. Fast, never fragments, wastes a bit
//     of space at shelf-bottom tails.
//   * Upload strategy: hold a CPU shadow buffer; every `insert` pushes
//     the *full* texture back to the GPU via
//     `SimpleTexture::uploadData`. 1 KB / new glyph is cheap; the
//     work stops as soon as the in-use glyph set stabilises. Can
//     revisit with per-rect `copyBuffersToTexture` if profiling
//     flags it.
class FontAtlas {
public:
    struct Slot {
        uint16_t x{0};
        uint16_t y{0};
        bool     ok{false};   // false = atlas full, bitmap dropped
    };

    explicit FontAtlas(uint16_t width = 1024, uint16_t height = 1024);
    ~FontAtlas() = default;

    FontAtlas(const FontAtlas &) = delete;
    FontAtlas &operator=(const FontAtlas &) = delete;

    // Reserve a `w × h` rect on the current shelf (advancing to a new
    // shelf if it doesn't fit), write the 8-bit alpha mask into the
    // shadow buffer at the reserved rect, and re-upload the whole
    // texture. Returns the atlas coords so the caller can record
    // UVs into its glyph map.
    //
    // `maskPitch` is the source stride in bytes — typically equals
    // `w` but FreeType bitmaps can have row padding.
    Slot insert(uint16_t w, uint16_t h, const uint8_t *mask, uint32_t maskPitch);

    Texture2D *getTexture() const { return _texture.get(); }
    uint16_t getWidth()  const { return _width; }
    uint16_t getHeight() const { return _height; }

private:
    // Expand the shadow buffer for a rect — writes RGBA where
    // R=G=B=255 and A=mask[i]. Safe against out-of-bounds (clips to
    // atlas edges).
    void writePixels(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                     const uint8_t *mask, uint32_t maskPitch);

    // Flush the entire shadow buffer to the GPU. Called after every
    // insert — cheap enough for the initial glyph-population burst,
    // free once the working set stabilises.
    void uploadToGPU();

    IntrusivePtr<Texture2D> _texture;
    ccstd::vector<uint8_t>  _pixels;  // RGBA8 shadow, size = w*h*4
    uint16_t _width{0};
    uint16_t _height{0};

    // Shelf packer state. A 1-pixel gutter between glyphs prevents
    // bilinear bleeding from neighbouring glyphs when the atlas is
    // sampled with a mip filter.
    static constexpr uint16_t kPadding = 1;
    uint16_t _shelfX{0};     // next X on the current shelf
    uint16_t _shelfY{0};     // top Y of the current shelf
    uint16_t _shelfMaxH{0};  // tallest glyph on the current shelf
};

}  // namespace cc
