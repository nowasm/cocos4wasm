#include "cocos/2d/text/FontAtlas.h"

#include <algorithm>
#include <cstring>

#include "base/Log.h"
#include "core/assets/ImageAsset.h"
#include "platform/Image.h"

namespace cc {

FontAtlas::FontAtlas(uint16_t width, uint16_t height)
: _width(width), _height(height) {
    // Allocate the CPU shadow as fully-white + fully-transparent.
    // White RGB keeps the `o.rgb *= texColor.rgb` branch of
    // builtin-unlit a no-op tint; alpha=0 means "no glyph here".
    _pixels.assign(static_cast<size_t>(_width) * _height * 4, 0);
    for (size_t i = 0; i < _pixels.size(); i += 4) {
        _pixels[i + 0] = 255;
        _pixels[i + 1] = 255;
        _pixels[i + 2] = 255;
        // _pixels[i + 3] already 0
    }

    // Build an ImageAsset → Texture2D pair. We go through ImageAsset
    // so the first uploadData has a valid gfx::Texture on hand.
    auto *image = ccnew Image();
    image->initWithRawData(_pixels.data(),
                            static_cast<uint32_t>(_pixels.size()),
                            _width, _height, /*bpp*/ 32);

    auto *asset = ccnew ImageAsset();
    asset->setNativeAsset(image);

    _texture = IntrusivePtr<Texture2D>(ccnew Texture2D());
    _texture->setImage(asset);
    _texture->onLoaded();  // realise the gfx::Texture
}

FontAtlas::Slot FontAtlas::insert(uint16_t w, uint16_t h,
                                    const uint8_t *mask, uint32_t maskPitch) {
    Slot s;
    if (w == 0 || h == 0) {
        // Zero-sized glyph (space etc.) — still "inserted" at the
        // current shelf cursor; caller records zero-sized UVs.
        s.x = _shelfX;
        s.y = _shelfY;
        s.ok = true;
        return s;
    }

    // Wrap to a new shelf if this rect won't fit in the current row.
    const uint16_t needW = w + kPadding;
    const uint16_t needH = h + kPadding;
    if (_shelfX + needW > _width) {
        _shelfY = _shelfY + _shelfMaxH + kPadding;
        _shelfX = 0;
        _shelfMaxH = 0;
    }
    if (_shelfY + needH > _height) {
        // Atlas is full. Log once and give up — the glyph will render
        // as an empty rect. Multi-page support is a later milestone.
        static bool warned = false;
        if (!warned) {
            CC_LOG_WARNING("[FontAtlas] full (%dx%d) — new glyphs will be dropped",
                           _width, _height);
            warned = true;
        }
        return s;  // ok=false
    }

    s.x = _shelfX;
    s.y = _shelfY;
    s.ok = true;
    _shelfX += needW;
    if (h > _shelfMaxH) _shelfMaxH = h;

    writePixels(s.x, s.y, w, h, mask, maskPitch);
    uploadToGPU();
    return s;
}

void FontAtlas::writePixels(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                              const uint8_t *mask, uint32_t maskPitch) {
    if (!mask) return;
    const uint16_t clampedW = (x + w > _width) ? static_cast<uint16_t>(_width - x) : w;
    const uint16_t clampedH = (y + h > _height) ? static_cast<uint16_t>(_height - y) : h;
    for (uint16_t row = 0; row < clampedH; ++row) {
        const uint8_t *src = mask + static_cast<size_t>(row) * maskPitch;
        uint8_t *dst = _pixels.data()
                     + (static_cast<size_t>(y + row) * _width + x) * 4;
        for (uint16_t col = 0; col < clampedW; ++col) {
            // RGB stays white (pre-filled in ctor); only alpha updates.
            dst[col * 4 + 3] = src[col];
        }
    }
}

void FontAtlas::uploadToGPU() {
    if (!_texture) return;
    _texture->uploadData(_pixels.data());
}

}  // namespace cc
