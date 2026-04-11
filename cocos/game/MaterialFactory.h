#pragma once
#include "core/assets/Material.h"
#include "core/assets/Texture2D.h"
#include "math/Color.h"

namespace cc::game {

class MaterialFactory {
public:
    static Material* createUnlit(const Color& color = Color(255, 255, 255, 255));
    static Material* createUnlitTextured(Texture2D* texture);
    static Material* createStandard(const Color& albedo = Color(255, 255, 255, 255));
    static Material* createStandardTextured(Texture2D* texture);
};

} // namespace cc::game
