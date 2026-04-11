#include "game/TextureLoader.h"
#include "core/assets/ImageAsset.h"
#include "platform/Image.h"
#include "base/Log.h"

namespace cc::game {

Texture2D* TextureLoader::loadFromFile(const ccstd::string& path) {
    auto* image = ccnew Image();
    if (!image->initWithImageFile(path)) {
        CC_LOG_ERROR("TextureLoader: failed to load image: %s", path.c_str());
        delete image;
        return nullptr;
    }

    auto* imageAsset = ccnew ImageAsset();
    imageAsset->setNativeAsset(image);

    auto* texture = ccnew Texture2D();
    texture->setImage(imageAsset);
    return texture;
}

Texture2D* TextureLoader::createFromRGBA(const uint8_t* data, uint32_t width, uint32_t height) {
    auto* image = ccnew Image();
    image->initWithRawData(data, width * height * 4, width, height, 32);

    auto* imageAsset = ccnew ImageAsset();
    imageAsset->setNativeAsset(image);

    auto* texture = ccnew Texture2D();
    texture->setImage(imageAsset);
    return texture;
}

Texture2D* TextureLoader::createSolidColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    uint8_t data[4] = {r, g, b, a};
    return createFromRGBA(data, 1, 1);
}

} // namespace cc::game
