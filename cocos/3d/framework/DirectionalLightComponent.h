/****************************************************************************
 DirectionalLightComponent — authoring-layer `cc.DirectionalLight` (P6).

 Wraps a scene::DirectionalLight: direction comes from the node's rotation
 (the light forward is the node's -Z, same as upstream), colour/illuminance
 are component fields. On enable the light registers with the render scene
 and — by default — becomes the main light; on disable it detaches.

 Only one main light is honoured by the forward pipeline; when several
 DirectionalLightComponents are enabled the last one to enable wins.
****************************************************************************/

#pragma once

#include "core/component/Component.h"
#include "core/reflection/Reflection.h"
#include "math/Color.h"

namespace cc {

namespace scene {
class DirectionalLight;
class RenderScene;
} // namespace scene

class DirectionalLightComponent : public Component {
    CC_CLASS_DECL(DirectionalLightComponent, Component)
public:
    DirectionalLightComponent() = default;
    ~DirectionalLightComponent() override = default;

    const Color &getColor() const { return _color; }
    void setColor(const Color &c);

    float getIlluminance() const { return _illuminance; }
    void setIlluminance(float lux);

    bool isMainLight() const { return _asMainLight; }
    void setAsMainLight(bool v) { _asMainLight = v; }

    scene::DirectionalLight *getLight() const { return _light; }

    void onEnable() override;
    void onDisable() override;
    void onDestroy() override;

    static int forceLink();

private:
    void createLightIfNeeded();
    void releaseLight();

    Color _color{255, 255, 255, 255};
    float _illuminance{65000.F}; // upstream daylight default (lux)
    bool  _asMainLight{true};

    scene::DirectionalLight *_light{nullptr};
    scene::RenderScene *_renderScene{nullptr};
};

} // namespace cc
