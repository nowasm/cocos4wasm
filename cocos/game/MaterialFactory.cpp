#include "game/MaterialFactory.h"
#include "core/assets/EffectAsset.h"
#include "core/builtin/BuiltinResMgr.h"
#include "base/Log.h"

namespace cc::game {

namespace {

Material* createMaterialWithEffect(const ccstd::string& effectName) {
    auto* effect = EffectAsset::get(effectName);
    if (!effect) {
        CC_LOG_ERROR("MaterialFactory: effect '%s' not found. Are builtin effects loaded?", effectName.c_str());
        return nullptr;
    }

    IMaterialInfo info;
    info.effectAsset = effect;

    auto* material = ccnew Material();
    material->initialize(info);
    return material;
}

} // namespace

Material* MaterialFactory::createUnlit(const Color& color) {
    auto* material = createMaterialWithEffect("builtin-unlit");
    if (material) {
        material->setPropertyColor("mainColor", color);
    }
    return material;
}

Material* MaterialFactory::createUnlitTextured(Texture2D* texture) {
    auto* material = createMaterialWithEffect("builtin-unlit");
    if (material && texture && texture->getGFXTexture()) {
        material->setPropertyTextureBase("mainTexture", texture);
    }
    return material;
}

Material* MaterialFactory::createStandard(const Color& albedo) {
    PBRParams params;
    params.albedo = albedo;
    return createStandard(params);
}

Material* MaterialFactory::createStandard(const PBRParams& params) {
    auto* material = createMaterialWithEffect("builtin-standard");
    if (material) {
        // builtin-standard exposes the albedo uniform directly; the friendly
        // "mainColor" alias is not resolvable when effects are loaded from
        // builtin-effects.json (pure-C++ mode).
        material->setPropertyColor("albedo", params.albedo);
        // shader reads packed vec4: x=occlusion y=roughness z=metallic w=specularIntensity
        Vec4 pbr(0.0f, params.roughness, params.metallic, params.specularIntensity);
        material->setPropertyVec4("pbrParams", pbr);
        material->setPropertyColor("emissive", params.emissive);
    }
    return material;
}

Material* MaterialFactory::createStandardTextured(Texture2D* texture) {
    StandardTextures textures;
    textures.albedo = texture;
    return createStandardPBR(PBRParams{}, textures);
}

Material* MaterialFactory::createStandardPBR(const PBRParams& params,
                                             const StandardTextures& textures) {
    auto* effect = EffectAsset::get("builtin-standard");
    if (!effect) {
        CC_LOG_ERROR("MaterialFactory: effect 'builtin-standard' not found");
        return nullptr;
    }

    const auto usable = [](Texture2D* t) { return t && t->getGFXTexture(); };
    const bool hasAlbedo    = usable(textures.albedo);
    const bool hasNormal    = usable(textures.normal);
    const bool hasPBR       = usable(textures.pbr);
    const bool hasOcclusion = usable(textures.occlusion);
    const bool hasEmissive  = usable(textures.emissive);

    IMaterialInfo info;
    info.effectAsset = effect;
    // Each sampler is compiled out unless its macro is defined.
    MacroRecord defines;
    if (hasAlbedo)    defines["USE_ALBEDO_MAP"] = true;
    if (hasNormal)    defines["USE_NORMAL_MAP"] = true;
    if (hasPBR)       defines["USE_PBR_MAP"] = true;
    if (hasOcclusion) defines["USE_OCCLUSION_MAP"] = true;
    if (hasEmissive)  defines["USE_EMISSIVE_MAP"] = true;
    if (!defines.empty()) info.defines = defines;

    auto* material = ccnew Material();
    material->initialize(info);

    // Raw uniform names throughout — friendly aliases ("mainTexture",
    // "mainColor") are not resolvable in pure-C++ mode.
    material->setPropertyColor("albedo", params.albedo);
    // pbrParams: x=occlusion strength, y=roughness, z=metallic, w=specular.
    // With USE_PBR_MAP the shader multiplies y/z by the map, so the factors
    // must be 1 to pass the texture through unscaled.
    Vec4 pbr((hasPBR || hasOcclusion) ? 1.0f : 0.0f,
             hasPBR ? 1.0f : params.roughness,
             hasPBR ? 1.0f : params.metallic,
             params.specularIntensity);
    material->setPropertyVec4("pbrParams", pbr);
    material->setPropertyColor("emissive", params.emissive);

    if (hasAlbedo)    material->setPropertyTextureBase("albedoMap", textures.albedo);
    if (hasNormal)    material->setPropertyTextureBase("normalMap", textures.normal);
    if (hasPBR)       material->setPropertyTextureBase("pbrMap", textures.pbr);
    if (hasOcclusion) material->setPropertyTextureBase("occlusionMap", textures.occlusion);
    if (hasEmissive)  material->setPropertyTextureBase("emissiveMap", textures.emissive);
    return material;
}

} // namespace cc::game
