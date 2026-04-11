#pragma once
#include "core/assets/Material.h"
#include "core/assets/Texture2D.h"
#include "math/Color.h"

namespace cc::game {

struct PBRParams {
    Color albedo{255, 255, 255, 255};
    float roughness{0.5f};
    float metallic{0.0f};
    float specularIntensity{0.5f};
    Color emissive{0, 0, 0, 255};
};

class MaterialFactory {
public:
    static Material* createUnlit(const Color& color = Color(255, 255, 255, 255));
    static Material* createUnlitTextured(Texture2D* texture);
    static Material* createStandard(const Color& albedo = Color(255, 255, 255, 255));
    static Material* createStandard(const PBRParams& params);
    static Material* createStandardTextured(Texture2D* texture);
};

} // namespace cc::game
