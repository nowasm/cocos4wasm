#include "cocos/misc/CameraComponent.h"

namespace cc {

// All upstream fields are protected backing fields — prefixed with "_"
// in JSON. We reflect the subset useful for load-verification (priority,
// clear state, ortho height, near/far, fov). Less-used authoring fields
// are kept as silent defaults for now.
CC_IMPLEMENT_CLASS(CameraComponent, "cc.Camera", Component)
    .property("_projection",  &CameraComponent::_projection)
    .property("_priority",    &CameraComponent::_priority)
    .property("_fov",         &CameraComponent::_fov)
    .property("_fovAxis",     &CameraComponent::_fovAxis)
    .property("_orthoHeight", &CameraComponent::_orthoHeight)
    .property("_near",        &CameraComponent::_near)
    .property("_far",         &CameraComponent::_far)
    .property("_color",       &CameraComponent::_color)
    .property("_clearFlags",  &CameraComponent::_clearFlags)
    .property("_visibility",  &CameraComponent::_visibility)
CC_END_CLASS(CameraComponent);

int CameraComponent::forceLink() {
    // Touching getStaticClass() is enough to pull the auto-reg static in.
    return getStaticClass() != nullptr ? 1 : 0;
}

}  // namespace cc
