#include "game/TextureLoader.h"
#include "core/assets/ImageAsset.h"
#include "platform/Image.h"
#include "base/Log.h"
#include "base/Ptr.h"
#include "renderer/gfx-base/GFXDef-common.h"
#include <cstdlib>
#include <cstring>

namespace cc::game {

namespace {

// Convert image raw data to RGBA8 format (fallback for unsupported texture formats).
// Returns a newly allocated buffer the caller must free, or nullptr on failure.
static uint8_t *expandToRGBA8(gfx::Format src, const uint8_t *srcData, uint32_t width, uint32_t height) {
    const uint32_t pixelCount = width * height;
    uint8_t *out = static_cast<uint8_t *>(malloc(pixelCount * 4));
    if (!out) return nullptr;

    switch (src) {
        case gfx::Format::L8:
            // Single channel grayscale → RGBA8 (replicate L to RGB, A=255)
            for (uint32_t i = 0; i < pixelCount; ++i) {
                uint8_t v = srcData[i];
                out[i * 4 + 0] = v;
                out[i * 4 + 1] = v;
                out[i * 4 + 2] = v;
                out[i * 4 + 3] = 255;
            }
            break;
        case gfx::Format::LA8:
            for (uint32_t i = 0; i < pixelCount; ++i) {
                uint8_t v = srcData[i * 2 + 0];
                uint8_t a = srcData[i * 2 + 1];
                out[i * 4 + 0] = v;
                out[i * 4 + 1] = v;
                out[i * 4 + 2] = v;
                out[i * 4 + 3] = a;
            }
            break;
        case gfx::Format::RGB8:
            // Expand 24-bit RGB → 32-bit RGBA (A=255)
            for (uint32_t i = 0; i < pixelCount; ++i) {
                out[i * 4 + 0] = srcData[i * 3 + 0];
                out[i * 4 + 1] = srcData[i * 3 + 1];
                out[i * 4 + 2] = srcData[i * 3 + 2];
                out[i * 4 + 3] = 255;
            }
            break;
        case gfx::Format::RGBA8:
            memcpy(out, srcData, pixelCount * 4);
            break;
        default:
            free(out);
            return nullptr;
    }
    return out;
}

} // namespace

Texture2D* TextureLoader::loadFromFile(const ccstd::string& path) {
    IntrusivePtr<Image> image = ccnew Image();
    if (!image->initWithImageFile(path)) {
        CC_LOG_ERROR("TextureLoader: failed to load image: %s", path.c_str());
        return nullptr;
    }

    gfx::Format fmt = image->getRenderFormat();
    uint32_t width = static_cast<uint32_t>(image->getWidth());
    uint32_t height = static_cast<uint32_t>(image->getHeight());

    // Always convert to RGBA8 for maximum compatibility.
    // Many GLES3 devices don't support RGB8 or L8 as sampled textures.
    uint8_t *rgba = expandToRGBA8(fmt, image->getData(), width, height);
    if (!rgba) {
        CC_LOG_ERROR("TextureLoader: unsupported format %d for: %s",
                     static_cast<int>(fmt), path.c_str());
        return nullptr;
    }

    auto *texture = createFromRGBA(rgba, width, height);
    free(rgba);

    if (texture && !texture->getGFXTexture()) {
        CC_LOG_WARNING("TextureLoader: GPU texture creation failed for: %s", path.c_str());
    }
    return texture;
}

Texture2D* TextureLoader::createFromRGBA(const uint8_t* data, uint32_t width, uint32_t height) {
    IntrusivePtr<Image> image = ccnew Image();
    image->initWithRawData(data, width * height * 4, width, height, 32);

    auto* imageAsset = ccnew ImageAsset();
    imageAsset->setNativeAsset(image.get());

    auto* texture = ccnew Texture2D();
    texture->setImage(imageAsset);
    return texture;
}

Texture2D* TextureLoader::createSolidColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    uint8_t data[4] = {r, g, b, a};
    return createFromRGBA(data, 1, 1);
}

} // namespace cc::game
