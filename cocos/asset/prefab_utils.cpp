#include "cocos/asset/prefab_utils.h"

#include "base/Log.h"
#include "cocos/serialization/JsonDeserializer.h"
#include "core/component/Component.h"
#include "core/reflection/Reflection.h"
#include "core/scene-graph/Node.h"
#include "math/Color.h"
#include "math/Quaternion.h"
#include "math/Vec2.h"
#include "math/Vec3.h"
#include "math/Vec4.h"

namespace cc {
namespace prefab_utils {

using reflection::ClassMeta;
using reflection::PropertyMeta;
using reflection::TypeId;

namespace {

// CCObject itself has no virtual getClass() — Node and Component each
// declare their own via CC_CLASS_DECL. Discriminate by a single dynamic
// cast at each apply site; the path only executes during scene load so
// the RTTI cost is negligible.
const ClassMeta *classOf(CCObject *obj) {
    if (!obj) return nullptr;
    if (auto *n = dynamic_cast<Node *>(obj)) return n->getClass();
    if (auto *c = dynamic_cast<Component *>(obj)) return c->getClass();
    return nullptr;
}

}  // namespace

// ─── Target map construction + lookup ─────────────────────────────────────

void buildTargetMap(Node *node, TargetMap &targetMap, bool isRoot) {
    if (!node) return;

    TargetMap *cur = &targetMap;

    // Non-root nodes that are themselves prefab instances get a fresh
    // sub-map under their PrefabInstance fileId — subsequent descendants
    // live under that namespace. The root's descendants live at the top
    // level, keyed by the root's own prefabInfo.fileId + direct descendant
    // fileIds.
    if (!isRoot && node->_prefab && node->_prefab->instance) {
        auto &bucket = targetMap.subMaps[node->_prefab->instance->fileId];
        if (!bucket) bucket = std::make_unique<TargetMap>();
        cur = bucket.get();
    }

    if (node->_prefab && !node->_prefab->fileId.empty()) {
        cur->nodes[node->_prefab->fileId] = node;
    }

    for (const auto &comp : node->getComponentList()) {
        if (!comp) continue;
        if (comp->getPrefabInfo() && !comp->getPrefabInfo()->fileId.empty()) {
            cur->components[comp->getPrefabInfo()->fileId] = comp.get();
        }
    }

    for (const auto &child : node->getChildren()) {
        buildTargetMap(child.get(), *cur, false);
    }
}

CCObject *getTarget(const ccstd::vector<ccstd::string> &localID,
                    const TargetMap &targetMap) {
    if (localID.empty()) return nullptr;

    const TargetMap *cur = &targetMap;
    for (size_t i = 0; i < localID.size(); ++i) {
        const ccstd::string &key = localID[i];
        const bool isLast = (i + 1 == localID.size());

        auto subIt = cur->subMaps.find(key);
        if (subIt != cur->subMaps.end()) {
            if (isLast) return nullptr;  // sub-map isn't a leaf target
            cur = subIt->second.get();
            continue;
        }

        if (isLast) {
            auto nIt = cur->nodes.find(key);
            if (nIt != cur->nodes.end()) return static_cast<CCObject *>(nIt->second);
            auto cIt = cur->components.find(key);
            if (cIt != cur->components.end()) return static_cast<CCObject *>(cIt->second);
            return nullptr;
        }

        // Mid-path key missing from sub-maps — can't continue.
        return nullptr;
    }
    return nullptr;
}

// ─── applyMountedChildren ─────────────────────────────────────────────────
//
// Reparent every extra child node under its recorded target. Newly
// reparented nodes are also walked into the targetMap so subsequent
// override passes can reference them by fileId.

void applyMountedChildren(
    const ccstd::vector<IntrusivePtr<MountedChildrenInfo>> &mountedChildren,
    TargetMap &targetMap) {
    for (const auto &mcPtr : mountedChildren) {
        MountedChildrenInfo *mc = mcPtr.get();
        if (!mc || !mc->targetInfo) continue;
        CCObject *t = getTarget(mc->targetInfo->localID, targetMap);
        auto *targetNode = t ? dynamic_cast<Node *>(t) : nullptr;
        if (!targetNode) continue;

        for (const auto &childPtr : mc->nodes) {
            Node *child = childPtr.get();
            if (!child) continue;
            targetNode->addChild(child);
            // Mounted children can themselves be prefab-authored — add to
            // the same local map so their nested overrides still resolve.
            buildTargetMap(child, targetMap, false);
        }
    }
}

// ─── applyMountedComponents ───────────────────────────────────────────────
//
// Attach extra components to their target node. Registered into the
// targetMap by fileId for subsequent property-override lookups.

void applyMountedComponents(
    const ccstd::vector<IntrusivePtr<MountedComponentsInfo>> &mountedComponents,
    TargetMap &targetMap) {
    for (const auto &mcPtr : mountedComponents) {
        MountedComponentsInfo *mc = mcPtr.get();
        if (!mc || !mc->targetInfo) continue;
        CCObject *t = getTarget(mc->targetInfo->localID, targetMap);
        auto *targetNode = t ? dynamic_cast<Node *>(t) : nullptr;
        if (!targetNode) continue;

        for (const auto &compPtr : mc->components) {
            Component *comp = compPtr.get();
            if (!comp) continue;
            targetNode->addComponent(comp);
            if (comp->getPrefabInfo() && !comp->getPrefabInfo()->fileId.empty()) {
                targetMap.components[comp->getPrefabInfo()->fileId] = comp;
            }
        }
    }
}

// ─── applyRemovedComponents ───────────────────────────────────────────────
//
// Each TargetInfo here resolves to a Component in the cloned master
// tree; remove it from its parent node's component list.

void applyRemovedComponents(
    const ccstd::vector<IntrusivePtr<TargetInfo>> &removedComponents,
    TargetMap &targetMap) {
    for (const auto &tiPtr : removedComponents) {
        TargetInfo *ti = tiPtr.get();
        if (!ti) continue;
        CCObject *t = getTarget(ti->localID, targetMap);
        auto *comp = t ? dynamic_cast<Component *>(t) : nullptr;
        if (!comp || !comp->getNode()) continue;
        comp->getNode()->removeComponent(comp);
    }
}

// ─── applyPropertyOverrides ───────────────────────────────────────────────
//
// Path walk + leaf set. Single-step paths decode directly into the
// reflected property's C++ type; two-step paths whose first step lands
// on a Vec2/3/4/Color/Quat field do a read-modify-write on the named
// sub-component (x/y/z/w or r/g/b/a). Deeper paths are not supported
// yet — they're rare in practice (Editor nearly always inlines nested
// objects as the full value).

namespace {

// Write a single float component into a Vec2/Vec3/Vec4/Color/Quat-typed
// property. Returns true on success; false if the TypeId or component
// name is unsupported.
bool writeVectorComponent(void *instance, const PropertyMeta &prop,
                          const ccstd::string &componentName, float v) {
    const char *c = componentName.c_str();
    auto isOneOf = [&](const char *a, const char *b = nullptr,
                        const char *d = nullptr, const char *e = nullptr) {
        return (a && std::strcmp(c, a) == 0) ||
               (b && std::strcmp(c, b) == 0) ||
               (d && std::strcmp(c, d) == 0) ||
               (e && std::strcmp(c, e) == 0);
    };
    switch (prop.typeId) {
        case TypeId::VEC2: {
            Vec2 x;
            prop.getter(instance, &x);
            if      (isOneOf("x")) x.x = v;
            else if (isOneOf("y")) x.y = v;
            else return false;
            prop.setter(instance, &x);
            return true;
        }
        case TypeId::VEC3: {
            Vec3 x;
            prop.getter(instance, &x);
            if      (isOneOf("x")) x.x = v;
            else if (isOneOf("y")) x.y = v;
            else if (isOneOf("z")) x.z = v;
            else return false;
            prop.setter(instance, &x);
            return true;
        }
        case TypeId::VEC4: {
            Vec4 x;
            prop.getter(instance, &x);
            if      (isOneOf("x")) x.x = v;
            else if (isOneOf("y")) x.y = v;
            else if (isOneOf("z")) x.z = v;
            else if (isOneOf("w")) x.w = v;
            else return false;
            prop.setter(instance, &x);
            return true;
        }
        case TypeId::QUAT: {
            Quaternion x;
            prop.getter(instance, &x);
            if      (isOneOf("x")) x.x = v;
            else if (isOneOf("y")) x.y = v;
            else if (isOneOf("z")) x.z = v;
            else if (isOneOf("w")) x.w = v;
            else return false;
            prop.setter(instance, &x);
            return true;
        }
        case TypeId::COLOR: {
            Color x;
            prop.getter(instance, &x);
            // Colour components are stored as 0–255 ints but override
            // values arrive as floats per upstream convention (editor
            // normalises at the ValueType.set() layer).
            const auto clamp = [](float f) {
                int i = static_cast<int>(f);
                return static_cast<uint8_t>(i < 0 ? 0 : (i > 255 ? 255 : i));
            };
            if      (isOneOf("r")) x.r = clamp(v);
            else if (isOneOf("g")) x.g = clamp(v);
            else if (isOneOf("b")) x.b = clamp(v);
            else if (isOneOf("a")) x.a = clamp(v);
            else return false;
            prop.setter(instance, &x);
            return true;
        }
        default:
            return false;
    }
}

}  // namespace

void applyPropertyOverrides(
    const ccstd::vector<IntrusivePtr<PropertyOverrideInfo>> &propertyOverrides,
    TargetMap &targetMap) {
    for (const auto &poPtr : propertyOverrides) {
        PropertyOverrideInfo *po = poPtr.get();
        if (!po || !po->targetInfo) continue;
        CCObject *target = getTarget(po->targetInfo->localID, targetMap);
        if (!target) continue;

        const auto *meta = classOf(target);
        if (!meta) continue;

        const auto &path = po->propertyPath;
        if (path.empty()) {
            CC_LOG_WARNING("[prefab] property override with empty path skipped");
            continue;
        }

        // Depth 1 — direct field set.
        if (path.size() == 1) {
            const PropertyMeta *prop = meta->findProperty(path[0].c_str());
            if (!prop) continue;
            serialization::decodeJsonValueToProperty(target, *prop, po->valueJson);
            continue;
        }

        // Depth 2 — assume first step lands on a Vec/Color/Quat-typed
        // field, second step is the component name. Value is a single
        // numeric scalar.
        if (path.size() == 2) {
            const PropertyMeta *prop = meta->findProperty(path[0].c_str());
            if (!prop) continue;
            float v = 0.f;
            // Tolerate both numeric JSON ("123") and object JSON for
            // single-component writes. Object form is uncommon here —
            // Editor emits scalars.
            try {
                v = std::stof(po->valueJson.c_str());
            } catch (...) {
                continue;
            }
            writeVectorComponent(target, *prop, path[1], v);
            continue;
        }

        CC_LOG_WARNING("[prefab] property override path depth %zu > 2 unsupported (target=%s)",
                       path.size(), meta->name ? meta->name : "?");
    }
}

// ─── applyTargetOverrides ────────────────────────────────────────────────
//
// Cross-instance pointer patch. Writes `source.<propertyPath> = target`
// where both source and target can be either direct pointers or resolved
// through a nested prefab instance's targetMap (the `sourceInfo` /
// `targetInfo` branches). Only the leaf write needs reflection — we
// look up the last path element as a reflected pointer-typed property
// on the source and assign.
//
// MVP scope: handles direct `source`/`target` pointers and depth-1
// propertyPath; nested-prefab sourceInfo/targetInfo deeper than one level
// fall through the direct path.

void applyTargetOverrides(
    const ccstd::vector<IntrusivePtr<TargetOverrideInfo>> &targetOverrides,
    TargetMap &targetMap) {
    for (const auto &toPtr : targetOverrides) {
        TargetOverrideInfo *to = toPtr.get();
        if (!to) continue;

        CCObject *source = to->source;
        if (!source && to->sourceInfo) {
            source = getTarget(to->sourceInfo->localID, targetMap);
        }
        if (!source) continue;

        Node *target = to->target;
        if (!target && to->targetInfo) {
            auto *obj = getTarget(to->targetInfo->localID, targetMap);
            target = obj ? dynamic_cast<Node *>(obj) : nullptr;
        }
        if (!target) continue;

        if (to->propertyPath.empty()) continue;

        const auto *meta = classOf(source);
        if (!meta) continue;

        // Only depth-1 pointer writes supported. Deeper paths (e.g. into
        // a nested vector slot) would need Variant-style addressing —
        // deferred until a real scene actually produces one.
        if (to->propertyPath.size() != 1) {
            CC_LOG_WARNING("[prefab] target override depth %zu != 1 unsupported",
                           to->propertyPath.size());
            continue;
        }
        const PropertyMeta *prop = meta->findProperty(to->propertyPath[0].c_str());
        if (!prop || prop->typeId != TypeId::POINTER || !prop->setter) continue;

        void *targetVoid = target;
        prop->setter(source, &targetVoid);
    }
}

}  // namespace prefab_utils
}  // namespace cc
