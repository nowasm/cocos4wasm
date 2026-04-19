#pragma once
#include "core/assets/Texture2D.h"

namespace cc::game {

class TextureLoader {
public:
    // Controls how L8 / LA8 source images map to the final RGBA8 texture.
    enum class Interpret {
        // Color image. L8 → RGBA(L,L,L,255); LA8 → RGBA(L,L,L,A). Default.
        COLOR,
        // Alpha mask (font atlas etc.). L8 → RGBA(255,255,255,L);
        // LA8 → RGBA(255,255,255,A). RGB/RGBA sources pass through.
        ALPHA_MASK,
    };

    static Texture2D* loadFromFile(const ccstd::string& path,
                                    Interpret interpret = Interpret::COLOR);
    static Texture2D* createFromRGBA(const uint8_t* data, uint32_t width, uint32_t height);
    static Texture2D* createSolidColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
};

} // namespace cc::game
