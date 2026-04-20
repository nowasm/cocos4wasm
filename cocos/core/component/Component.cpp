#include "Component.h"
#include "NodeActivator.h"
#include "cocos/asset/PrefabInfo.h"
#include "core/scene-graph/Node.h"

namespace cc {

CC_IMPLEMENT_ROOT_CLASS(Component, "cc.Component")
    .property("enabled",  &Component::_enabled, true)
    .property("__prefab", &Component::__prefab)
CC_END_CLASS(Component);

Component::Component()  = default;
Component::~Component() = default;

void Component::setEnabled(bool v) {
    if (_enabled == v) return;
    _enabled = v;
    if (_node && _node->isActiveInHierarchy()) {
        if (v) {
            NodeActivator::get().activateComp(this);
        } else {
            NodeActivator::get().deactivateComp(this);
        }
    }
}

bool Component::isEnabledInHierarchy() const {
    return _enabled && _node && _node->isActiveInHierarchy();
}

}  // namespace cc
