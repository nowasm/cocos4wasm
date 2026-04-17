#include "Component.h"
#include "NodeActivator.h"
#include "core/scene-graph/Node.h"

namespace cc {

CC_IMPLEMENT_ROOT_CLASS(Component, "cc.Component")
    .property("enabled", &Component::_enabled, true)
CC_END_CLASS(Component);

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
