#pragma once
#include "core/assets/Texture2D.h"

namespace cc::game {

class TextureLoader {
public:
    static Texture2D* loadFromFile(const ccstd::string& path);
    static Texture2D* createFromRGBA(const uint8_t* data, uint32_t width, uint32_t height);
    static Texture2D* createSolidColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
};

} // namespace cc::game
