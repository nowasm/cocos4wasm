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
        material->setPropertyColor("mainColor", params.albedo);
        // shader reads packed vec4: x=occlusion y=roughness z=metallic w=specularIntensity
        Vec4 pbr(0.0f, params.roughness, params.metallic, params.specularIntensity);
        material->setPropertyVec4("pbrParams", pbr);
        material->setPropertyColor("emissive", params.emissive);
    }
    return material;
}

Material* MaterialFactory::createStandardTextured(Texture2D* texture) {
    auto* material = createMaterialWithEffect("builtin-standard");
    if (material && texture && texture->getGFXTexture()) {
        material->setPropertyTextureBase("mainTexture", texture);
    }
    return material;
}

} // namespace cc::game
