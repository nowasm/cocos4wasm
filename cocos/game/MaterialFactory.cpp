#include "game/MaterialFactory.h"
#include "core/assets/EffectAsset.h"
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
    if (material && texture) {
        material->setPropertyTextureBase("mainTexture", texture);
    }
    return material;
}

Material* MaterialFactory::createStandard(const Color& albedo) {
    auto* material = createMaterialWithEffect("builtin-standard");
    if (material) {
        material->setPropertyColor("mainColor", albedo);
    }
    return material;
}

Material* MaterialFactory::createStandardTextured(Texture2D* texture) {
    auto* material = createMaterialWithEffect("builtin-standard");
    if (material && texture) {
        material->setPropertyTextureBase("mainTexture", texture);
    }
    return material;
}

} // namespace cc::game
