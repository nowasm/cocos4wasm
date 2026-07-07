#include "3d/framework/DirectionalLightComponent.h"

#include "core/Root.h"
#include "core/scene-graph/Node.h"
#include "scene/DirectionalLight.h"
#include "scene/RenderScene.h"

namespace cc {

CC_IMPLEMENT_CLASS(DirectionalLightComponent, "cc.DirectionalLight", Component)
    .property("_color", &DirectionalLightComponent::_color)
    .property("_illuminance", &DirectionalLightComponent::_illuminance)
CC_END_CLASS(DirectionalLightComponent);

int DirectionalLightComponent::forceLink() {
    return getStaticClass() != nullptr ? 1 : 0;
}

void DirectionalLightComponent::setColor(const Color &c) {
    _color = c;
    if (_light) {
        _light->setColor(Vec3(_color.r / 255.F, _color.g / 255.F, _color.b / 255.F));
    }
}

void DirectionalLightComponent::setIlluminance(float lux) {
    _illuminance = lux;
    if (_light) _light->setIlluminance(lux);
}

void DirectionalLightComponent::createLightIfNeeded() {
    if (_light) return;
    auto *root = Root::getInstance();
    if (!root) return;
    _light = root->createLight<scene::DirectionalLight>();
    _light->setNode(_node);
    _light->setColor(Vec3(_color.r / 255.F, _color.g / 255.F, _color.b / 255.F));
    _light->setIlluminance(_illuminance);
}

void DirectionalLightComponent::releaseLight() {
    if (!_light) return;
    auto *root = Root::getInstance();
    if (root) root->destroyLight(_light);
    _light = nullptr;
}

void DirectionalLightComponent::onEnable() {
    createLightIfNeeded();
    if (!_light) return;
    auto *root = Root::getInstance();
    if (root && !root->getScenes().empty()) {
        _renderScene = root->getScenes()[0].get();
        if (_asMainLight) {
            _renderScene->setMainLight(_light);
        } else {
            _renderScene->addDirectionalLight(_light);
        }
    }
}

void DirectionalLightComponent::onDisable() {
    if (_light && _renderScene) {
        if (_asMainLight) {
            _renderScene->unsetMainLight(_light);
        } else {
            _renderScene->removeDirectionalLight(_light);
        }
        _renderScene = nullptr;
    }
}

void DirectionalLightComponent::onDestroy() {
    onDisable();
    releaseLight();
}

} // namespace cc
