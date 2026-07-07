#include "base/Ptr.h"
#include "cocos/asset/Prefab.h"
#include "cocos/asset/PrefabInfo.h"
#include "cocos/asset/prefab_utils.h"
#include "core/component/Component.h"
#include "core/reflection/Reflection.h"
#include "core/scene-graph/Node.h"
#include "gtest/gtest.h"

using namespace cc;
using namespace cc::prefab_utils;

// ─── Test component (reflected) ───────────────────────────────────────────
namespace {

class ApplyTestComp : public Component {
    CC_CLASS_DECL(ApplyTestComp, Component)
public:
    int32_t       _score{0};
    ccstd::string _label;
    Color         _tint{255, 255, 255, 255};
    Vec3          _pos;
    Node         *_ref{nullptr};
};

}  // namespace

CC_IMPLEMENT_CLASS(ApplyTestComp, "prefab_utils_test.ApplyTestComp", Component)
    .property("_score", &ApplyTestComp::_score)
    .property("_label", &ApplyTestComp::_label)
    .property("_tint",  &ApplyTestComp::_tint)
    .property("_pos",   &ApplyTestComp::_pos)
    .property("_ref",   &ApplyTestComp::_ref)
CC_END_CLASS(ApplyTestComp);

// ─── Fixture builder ──────────────────────────────────────────────────────
// Builds a synthetic cloned prefab tree:
//   root    (fileId="root")
//     ├── child1  (fileId="child1")
//     │     └── compA on child1  (fileId="compA")
//     └── child2  (fileId="child2")

namespace {

struct Fixture {
    IntrusivePtr<Node> root;
    Node *child1{nullptr};
    Node *child2{nullptr};
    ApplyTestComp *compA{nullptr};

    static IntrusivePtr<PrefabInfo> makePI(const char *fileId) {
        auto pi = IntrusivePtr<PrefabInfo>(ccnew PrefabInfo());
        pi->fileId = fileId;
        return pi;
    }
    static IntrusivePtr<CompPrefabInfo> makeCPI(const char *fileId) {
        auto cpi = IntrusivePtr<CompPrefabInfo>(ccnew CompPrefabInfo());
        cpi->fileId = fileId;
        return cpi;
    }
};

Fixture makeTree() {
    Fixture fx;
    fx.root = ccnew Node();
    fx.root->_prefab = Fixture::makePI("root");

    auto *c1 = ccnew Node();
    c1->_prefab = Fixture::makePI("child1");
    fx.root->addChild(c1);
    fx.child1 = c1;

    auto *c2 = ccnew Node();
    c2->_prefab = Fixture::makePI("child2");
    fx.root->addChild(c2);
    fx.child2 = c2;

    auto *comp = c1->addComponent<ApplyTestComp>();
    comp->setPrefabInfo(Fixture::makeCPI("compA").get());
    fx.compA = comp;

    return fx;
}

}  // namespace

// ─── buildTargetMap / getTarget ─────────────────────────────────────────────

TEST(PrefabUtils, BuildMapFlatTree) {
    auto fx = makeTree();
    TargetMap map;
    buildTargetMap(fx.root.get(), map, true);

    // Nodes and components at their respective fileIds
    EXPECT_EQ(map.nodes["root"],   fx.root.get());
    EXPECT_EQ(map.nodes["child1"], fx.child1);
    EXPECT_EQ(map.nodes["child2"], fx.child2);
    EXPECT_EQ(map.components["compA"], fx.compA);
}

TEST(PrefabUtils, GetTargetNode) {
    auto fx = makeTree();
    TargetMap map;
    buildTargetMap(fx.root.get(), map, true);

    EXPECT_EQ(getTarget({"child1"}, map), fx.child1);
    EXPECT_EQ(getTarget({"child2"}, map), fx.child2);
    EXPECT_EQ(getTarget({"nothere"}, map), nullptr);
    EXPECT_EQ(getTarget({}, map), nullptr);  // empty path
}

TEST(PrefabUtils, GetTargetComponent) {
    auto fx = makeTree();
    TargetMap map;
    buildTargetMap(fx.root.get(), map, true);
    EXPECT_EQ(getTarget({"compA"}, map), fx.compA);
}

// ─── applyMountedChildren ─────────────────────────────────────────────────

TEST(PrefabUtils, ApplyMountedChildrenAddsUnderTarget) {
    auto fx = makeTree();
    TargetMap map;
    buildTargetMap(fx.root.get(), map, true);

    IntrusivePtr<Node> extra = ccnew Node();
    extra->setName("extra");
    extra->_prefab = Fixture::makePI("extra");

    auto mci = IntrusivePtr<MountedChildrenInfo>(ccnew MountedChildrenInfo());
    mci->targetInfo = IntrusivePtr<TargetInfo>(ccnew TargetInfo());
    mci->targetInfo->localID = {"child1"};
    mci->nodes.push_back(extra);

    ccstd::vector<IntrusivePtr<MountedChildrenInfo>> vec;
    vec.push_back(mci);
    applyMountedChildren(vec, map);

    // Was extra reparented under child1?
    ASSERT_EQ(fx.child1->getChildren().size(), 1u);
    EXPECT_EQ(fx.child1->getChildren()[0].get(), extra.get());

    // Was it added to the targetMap for subsequent overrides?
    EXPECT_EQ(map.nodes["extra"], extra.get());
}

// ─── applyMountedComponents ───────────────────────────────────────────────

TEST(PrefabUtils, ApplyMountedComponentsAttachesToTarget) {
    auto fx = makeTree();
    TargetMap map;
    buildTargetMap(fx.root.get(), map, true);

    auto *newComp = ccnew ApplyTestComp();
    newComp->setPrefabInfo(Fixture::makeCPI("newComp").get());
    newComp->_score = 99;

    auto mci = IntrusivePtr<MountedComponentsInfo>(ccnew MountedComponentsInfo());
    mci->targetInfo = IntrusivePtr<TargetInfo>(ccnew TargetInfo());
    mci->targetInfo->localID = {"child2"};
    mci->components.push_back(IntrusivePtr<Component>(newComp));

    ccstd::vector<IntrusivePtr<MountedComponentsInfo>> vec;
    vec.push_back(mci);
    applyMountedComponents(vec, map);

    // child2 should now own newComp
    auto *attached = fx.child2->getComponent<ApplyTestComp>();
    EXPECT_EQ(attached, newComp);
    EXPECT_EQ(attached->_score, 99);
    // Registered in map
    EXPECT_EQ(map.components["newComp"], newComp);
}

// ─── applyRemovedComponents ───────────────────────────────────────────────

TEST(PrefabUtils, ApplyRemovedComponentsRemovesByFileId) {
    auto fx = makeTree();
    TargetMap map;
    buildTargetMap(fx.root.get(), map, true);
    ASSERT_NE(fx.child1->getComponent<ApplyTestComp>(), nullptr);

    auto ti = IntrusivePtr<TargetInfo>(ccnew TargetInfo());
    ti->localID = {"compA"};

    ccstd::vector<IntrusivePtr<TargetInfo>> vec;
    vec.push_back(ti);
    applyRemovedComponents(vec, map);

    EXPECT_EQ(fx.child1->getComponent<ApplyTestComp>(), nullptr);
}

// ─── applyPropertyOverrides ───────────────────────────────────────────────

TEST(PrefabUtils, ApplyPropertyOverridesDepth1Int) {
    auto fx = makeTree();
    TargetMap map;
    buildTargetMap(fx.root.get(), map, true);

    auto po = IntrusivePtr<PropertyOverrideInfo>(ccnew PropertyOverrideInfo());
    po->targetInfo = IntrusivePtr<TargetInfo>(ccnew TargetInfo());
    po->targetInfo->localID = {"compA"};
    po->propertyPath = {"_score"};
    po->valueJson = "42";

    ccstd::vector<IntrusivePtr<PropertyOverrideInfo>> vec;
    vec.push_back(po);
    applyPropertyOverrides(vec, map);

    EXPECT_EQ(fx.compA->_score, 42);
}

TEST(PrefabUtils, ApplyPropertyOverridesDepth1String) {
    auto fx = makeTree();
    TargetMap map;
    buildTargetMap(fx.root.get(), map, true);

    auto po = IntrusivePtr<PropertyOverrideInfo>(ccnew PropertyOverrideInfo());
    po->targetInfo = IntrusivePtr<TargetInfo>(ccnew TargetInfo());
    po->targetInfo->localID = {"compA"};
    po->propertyPath = {"_label"};
    po->valueJson = R"("hello")";

    ccstd::vector<IntrusivePtr<PropertyOverrideInfo>> vec;
    vec.push_back(po);
    applyPropertyOverrides(vec, map);

    EXPECT_EQ(fx.compA->_label, "hello");
}

TEST(PrefabUtils, ApplyPropertyOverridesDepth2Vec3Component) {
    auto fx = makeTree();
    TargetMap map;
    buildTargetMap(fx.root.get(), map, true);

    fx.compA->_pos.set(1.f, 2.f, 3.f);  // starting value
    auto po = IntrusivePtr<PropertyOverrideInfo>(ccnew PropertyOverrideInfo());
    po->targetInfo = IntrusivePtr<TargetInfo>(ccnew TargetInfo());
    po->targetInfo->localID = {"compA"};
    po->propertyPath = {"_pos", "y"};
    po->valueJson = "7.5";

    ccstd::vector<IntrusivePtr<PropertyOverrideInfo>> vec;
    vec.push_back(po);
    applyPropertyOverrides(vec, map);

    EXPECT_FLOAT_EQ(fx.compA->_pos.x, 1.f);    // unchanged
    EXPECT_FLOAT_EQ(fx.compA->_pos.y, 7.5f);   // overridden
    EXPECT_FLOAT_EQ(fx.compA->_pos.z, 3.f);    // unchanged
}

TEST(PrefabUtils, ApplyPropertyOverridesDepth2ColorComponent) {
    auto fx = makeTree();
    TargetMap map;
    buildTargetMap(fx.root.get(), map, true);

    auto po = IntrusivePtr<PropertyOverrideInfo>(ccnew PropertyOverrideInfo());
    po->targetInfo = IntrusivePtr<TargetInfo>(ccnew TargetInfo());
    po->targetInfo->localID = {"compA"};
    po->propertyPath = {"_tint", "r"};
    po->valueJson = "128";

    ccstd::vector<IntrusivePtr<PropertyOverrideInfo>> vec;
    vec.push_back(po);
    applyPropertyOverrides(vec, map);

    EXPECT_EQ(fx.compA->_tint.r, 128);
    EXPECT_EQ(fx.compA->_tint.g, 255);  // unchanged
}

TEST(PrefabUtils, ApplyPropertyOverridesUnknownTargetIsNoOp) {
    auto fx = makeTree();
    TargetMap map;
    buildTargetMap(fx.root.get(), map, true);

    auto po = IntrusivePtr<PropertyOverrideInfo>(ccnew PropertyOverrideInfo());
    po->targetInfo = IntrusivePtr<TargetInfo>(ccnew TargetInfo());
    po->targetInfo->localID = {"nonexistent"};  // won't resolve
    po->propertyPath = {"_score"};
    po->valueJson = "999";

    ccstd::vector<IntrusivePtr<PropertyOverrideInfo>> vec;
    vec.push_back(po);
    EXPECT_NO_FATAL_FAILURE(applyPropertyOverrides(vec, map));
    EXPECT_EQ(fx.compA->_score, 0);  // untouched
}

// ─── applyTargetOverrides ────────────────────────────────────────────────

TEST(PrefabUtils, ApplyTargetOverridesPatchesPointer) {
    auto fx = makeTree();
    TargetMap map;
    buildTargetMap(fx.root.get(), map, true);

    // Make compA._ref point at child2 via a target override.
    auto to = IntrusivePtr<TargetOverrideInfo>(ccnew TargetOverrideInfo());
    to->source       = fx.compA;          // direct pointer
    to->target       = fx.child2;         // direct pointer
    to->propertyPath = {"_ref"};

    ccstd::vector<IntrusivePtr<TargetOverrideInfo>> vec;
    vec.push_back(to);
    applyTargetOverrides(vec, map);

    EXPECT_EQ(fx.compA->_ref, fx.child2);
}

// ─── End-to-end: Prefab::instantiate(PrefabInstance) ──────────────────────
//
// Builds a synthetic master .prefab JSON (a Node with one ApplyTestComp
// child-component, each with their own fileId) and exercises three
// variant instances that together touch four of the five override
// categories: propertyOverride, mountedChildren, mountedComponents,
// removedComponents. (applyTargetOverrides is covered above.)

namespace {

// Minimal master prefab JSON:
//   root (fileId=rootId, component compA (fileId=compAId, _score=0))
const char *kMasterPrefab = R"([
    {
        "__type__": "cc.Node",
        "_name": "Master",
        "_children":  [],
        "_components": [{"__id__": 1}],
        "_prefab":    {"__id__": 2}
    },
    {
        "__type__": "prefab_utils_test.ApplyTestComp",
        "_score":   0,
        "_label":   "master",
        "__prefab": {"__id__": 3}
    },
    {
        "__type__": "cc.PrefabInfo",
        "fileId":   "rootId"
    },
    {
        "__type__": "cc.CompPrefabInfo",
        "fileId":   "compAId"
    }
])";

IntrusivePtr<Prefab> makeMaster() {
    IntrusivePtr<Prefab> p = ccnew Prefab();
    p->setData(kMasterPrefab);
    p->setRootIndex(0);
    return p;
}

IntrusivePtr<PrefabInstance> makeInstanceWithScoreOverride(int value) {
    auto inst = IntrusivePtr<PrefabInstance>(ccnew PrefabInstance());
    inst->fileId = "inst-1";

    auto po = IntrusivePtr<PropertyOverrideInfo>(ccnew PropertyOverrideInfo());
    po->targetInfo = IntrusivePtr<TargetInfo>(ccnew TargetInfo());
    po->targetInfo->localID = {"compAId"};
    po->propertyPath = {"_score"};
    po->valueJson = std::to_string(value).c_str();
    inst->propertyOverrides.push_back(po);
    return inst;
}

}  // namespace

TEST(PrefabUtils, InstantiateWithPropertyOverride) {
    auto master = makeMaster();
    auto inst   = makeInstanceWithScoreOverride(100);

    IntrusivePtr<Node> root(master->instantiate(inst.get()));
    ASSERT_NE(root.get(), nullptr);

    auto *comp = root->getComponent<ApplyTestComp>();
    ASSERT_NE(comp, nullptr);
    EXPECT_EQ(comp->_score, 100);           // overridden
    EXPECT_EQ(comp->_label, "master");      // unchanged from master
}

TEST(PrefabUtils, InstantiateWithoutInstanceYieldsPlainClone) {
    auto master = makeMaster();
    IntrusivePtr<Node> root(master->instantiate());   // no overrides
    ASSERT_NE(root.get(), nullptr);
    auto *comp = root->getComponent<ApplyTestComp>();
    ASSERT_NE(comp, nullptr);
    EXPECT_EQ(comp->_score, 0);             // master default
    EXPECT_EQ(comp->_label, "master");
}

TEST(PrefabUtils, InstantiateTwiceProducesIndependentInstances) {
    auto master = makeMaster();
    auto inst1  = makeInstanceWithScoreOverride(10);
    auto inst2  = makeInstanceWithScoreOverride(20);

    IntrusivePtr<Node> r1(master->instantiate(inst1.get()));
    IntrusivePtr<Node> r2(master->instantiate(inst2.get()));
    ASSERT_NE(r1.get(), r2.get());

    auto *c1 = r1->getComponent<ApplyTestComp>();
    auto *c2 = r2->getComponent<ApplyTestComp>();
    ASSERT_NE(c1, nullptr);
    ASSERT_NE(c2, nullptr);
    ASSERT_NE(c1, c2);   // cloned components, not aliased
    EXPECT_EQ(c1->_score, 10);
    EXPECT_EQ(c2->_score, 20);
}

TEST(PrefabUtils, InstantiateMarksInstanceExpanded) {
    auto master = makeMaster();
    auto inst   = makeInstanceWithScoreOverride(42);
    EXPECT_FALSE(inst->expanded);
    IntrusivePtr<Node> root(master->instantiate(inst.get()));
    EXPECT_TRUE(inst->expanded);
}

// ─── Scene-load integration: expandPrefabInstanceNode ─────────────────────
//
// Simulates what happens when a scene JSON is deserialized and one of
// its nodes carries a populated `_prefab.asset + _prefab.instance`
// combo — the expand helper fetches the master and transplants it into
// the empty shell, then applies overrides.

TEST(PrefabUtils, ExpandPrefabInstanceNodeTransplantsMasterAndApplies) {
    auto master = makeMaster();

    // "Scene" node: empty shell with a _prefab pointing at master + a
    // PrefabInstance that overrides _score to 777.
    IntrusivePtr<Node> sceneNode = ccnew Node();
    sceneNode->setName("InstanceInScene");

    auto pi = IntrusivePtr<PrefabInfo>(ccnew PrefabInfo());
    pi->asset    = master;
    pi->instance = makeInstanceWithScoreOverride(777);
    sceneNode->_prefab = pi;

    // Before expansion: the shell has no children / components.
    EXPECT_EQ(sceneNode->getChildren().size(), 0u);
    EXPECT_EQ(sceneNode->getComponentList().size(), 0u);

    const int expanded = expandPrefabInstanceNode(sceneNode.get());
    EXPECT_EQ(expanded, 1);

    // After expansion: master's ApplyTestComp is now on sceneNode, and
    // the override has bumped _score to 777.
    auto *comp = sceneNode->getComponent<ApplyTestComp>();
    ASSERT_NE(comp, nullptr);
    EXPECT_EQ(comp->_score, 777);
    EXPECT_EQ(comp->_label, "master");   // unchanged
    EXPECT_TRUE(pi->instance->expanded);
}

TEST(PrefabUtils, ExpandPrefabInstanceNodeIsIdempotent) {
    auto master = makeMaster();
    IntrusivePtr<Node> sceneNode = ccnew Node();
    auto pi = IntrusivePtr<PrefabInfo>(ccnew PrefabInfo());
    pi->asset    = master;
    pi->instance = makeInstanceWithScoreOverride(5);
    sceneNode->_prefab = pi;

    EXPECT_EQ(expandPrefabInstanceNode(sceneNode.get()), 1);
    // Second call: instance.expanded is true, helper returns 0.
    EXPECT_EQ(expandPrefabInstanceNode(sceneNode.get()), 0);
    // And hasn't duplicated components.
    int comps = 0;
    for (const auto &c : sceneNode->getComponentList()) if (c) ++comps;
    EXPECT_EQ(comps, 1);
}

TEST(PrefabUtils, ExpandPrefabInstanceNodeSkipsPlainNodes) {
    IntrusivePtr<Node> plain = ccnew Node();
    plain->setName("plain");
    EXPECT_EQ(expandPrefabInstanceNode(plain.get()), 0);
}

TEST(PrefabUtils, ApplyTargetOverridesViaSourceInfo) {
    auto fx = makeTree();
    TargetMap map;
    buildTargetMap(fx.root.get(), map, true);

    // source is resolved by sourceInfo path instead of direct pointer.
    auto to = IntrusivePtr<TargetOverrideInfo>(ccnew TargetOverrideInfo());
    to->sourceInfo = IntrusivePtr<TargetInfo>(ccnew TargetInfo());
    to->sourceInfo->localID = {"compA"};
    to->target       = fx.child1;
    to->propertyPath = {"_ref"};

    ccstd::vector<IntrusivePtr<TargetOverrideInfo>> vec;
    vec.push_back(to);
    applyTargetOverrides(vec, map);

    EXPECT_EQ(fx.compA->_ref, fx.child1);
}
