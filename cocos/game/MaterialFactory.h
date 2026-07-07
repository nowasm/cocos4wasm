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

// Optional texture set for builtin-standard. Channel conventions follow the
// effect (glTF-compatible): pbr = packed ORM (r=occlusion, g=roughness,
// b=metallic, a=specularIntensity), occlusion = separate AO (r), normal =
// tangent-space normal map (mesh must carry tangents), emissive = sRGB.
struct StandardTextures {
    Texture2D* albedo{nullptr};
    Texture2D* normal{nullptr};
    Texture2D* pbr{nullptr};
    Texture2D* occlusion{nullptr};
    Texture2D* emissive{nullptr};
};

class MaterialFactory {
public:
    static Material* createUnlit(const Color& color = Color(255, 255, 255, 255));
    static Material* createUnlitTextured(Texture2D* texture);
    static Material* createStandard(const Color& albedo = Color(255, 255, 255, 255));
    static Material* createStandard(const PBRParams& params);
    static Material* createStandardTextured(Texture2D* texture);
    static Material* createStandardPBR(const PBRParams& params, const StandardTextures& textures);
};

} // namespace cc::game
