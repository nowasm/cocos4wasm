#include "NodeActivator.h"
#include <algorithm>
#include "Component.h"
#include "ComponentScheduler.h"
#include "core/scene-graph/Node.h"

namespace cc {

using Flags = CCObject::Flags;

NodeActivator &NodeActivator::get() {
    static NodeActivator instance;
    return instance;
}

void NodeActivator::activateNode(Node *node, bool shouldActive) {
    if (!node) return;
    if (node->isActiveInHierarchy() == shouldActive) return;
    if (shouldActive) {
        activateNodeRecursive(node);
    } else {
        deactivateNodeRecursive(node);
    }
}

void NodeActivator::activateNodeRecursive(Node *node) {
    node->setActiveInHierarchy(true);
    // activate this node's components (snapshot-iterate — activateComp may
    // mutate _startQueue but must not mutate the node's component list)
    const auto &comps = node->getComponentList();
    for (size_t i = 0, n = comps.size(); i < n; ++i) {
        if (comps[i]) activateComp(comps[i].get());
    }
    // recurse into locally-active children
    const auto &children = node->getChildren();
    for (size_t i = 0, n = children.size(); i < n; ++i) {
        Node *child = children[i].get();
        if (child && child->isActive()) {
            activateNodeRecursive(child);
        }
    }
}

void NodeActivator::deactivateNodeRecursive(Node *node) {
    node->_objFlags |= Flags::DEACTIVATING;
    // children go disabled before parent notifies, so teardown propagates leaf-first
    const auto &children = node->getChildren();
    for (size_t i = 0, n = children.size(); i < n; ++i) {
        Node *child = children[i].get();
        if (child && child->isActiveInHierarchy()) {
            deactivateNodeRecursive(child);
        }
    }
    const auto &comps = node->getComponentList();
    for (size_t i = 0, n = comps.size(); i < n; ++i) {
        if (comps[i]) deactivateComp(comps[i].get());
    }
    node->setActiveInHierarchy(false);
    node->_objFlags &= ~Flags::DEACTIVATING;
}

void NodeActivator::activateComp(Component *comp) {
    if (!comp) return;

    // onLoad — once per lifetime, on first activation
    if (!hasFlag(comp->_objFlags, Flags::IS_ON_LOAD_CALLED)) {
        comp->onLoad();
        comp->_objFlags |= Flags::IS_ON_LOAD_CALLED;
    }

    // onEnable — skip if already enabled, or if component is disabled
    if (!comp->_enabled) return;
    if (hasFlag(comp->_objFlags, Flags::IS_ON_ENABLE_CALLED)) return;

    comp->onEnable();
    comp->_objFlags |= Flags::IS_ON_ENABLE_CALLED;

    if (comp->_wantsUpdate)     ComponentScheduler::get().scheduleUpdate(comp);
    if (comp->_wantsLateUpdate) ComponentScheduler::get().scheduleLateUpdate(comp);

    // queue start() for next tick (if not already started)
    if (!hasFlag(comp->_objFlags, Flags::IS_START_CALLED)) {
        if (std::find(_startQueue.begin(), _startQueue.end(), comp) == _startQueue.end()) {
            _startQueue.push_back(comp);
        }
    }
}

void NodeActivator::deactivateComp(Component *comp) {
    if (!comp) return;
    if (!hasFlag(comp->_objFlags, Flags::IS_ON_ENABLE_CALLED)) return;

    // unschedule first so no update() fires after onDisable
    if (comp->_wantsUpdate)     ComponentScheduler::get().unscheduleUpdate(comp);
    if (comp->_wantsLateUpdate) ComponentScheduler::get().unscheduleLateUpdate(comp);

    // drop from start queue if still waiting
    _startQueue.erase(std::remove(_startQueue.begin(), _startQueue.end(), comp), _startQueue.end());

    comp->onDisable();
    comp->_objFlags &= ~Flags::IS_ON_ENABLE_CALLED;
}

void NodeActivator::destroyComp(Component *comp) {
    if (!comp) return;
    if (hasFlag(comp->_objFlags, Flags::IS_ON_ENABLE_CALLED)) {
        deactivateComp(comp);
    }
    comp->onDestroy();
}

void NodeActivator::invokePendingStarts() {
    if (_startQueue.empty()) return;
    // drain via swap so start() implementations can safely queue new work
    ccstd::vector<Component *> batch;
    batch.swap(_startQueue);
    for (auto *c : batch) {
        if (!c) continue;
        if (hasFlag(c->_objFlags, Flags::IS_START_CALLED)) continue;
        if (!c->isEnabledInHierarchy()) continue;
        c->start();
        c->_objFlags |= Flags::IS_START_CALLED;
    }
}

}  // namespace cc
