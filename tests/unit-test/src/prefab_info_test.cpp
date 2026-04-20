#include "base/Ptr.h"
#include "cocos/asset/PrefabInfo.h"
#include "cocos/serialization/JsonDeserializer.h"
#include "core/reflection/Reflection.h"
#include "gtest/gtest.h"

using namespace cc;
using namespace cc::reflection;

// ─── Reflection registration sanity ───────────────────────────────────────

TEST(PrefabInfo, TypeNamesMatchUpstream) {
    // Critical: these strings are the exact __type__ values Editor emits
    // — a typo here means Editor JSON silently won't load.
    EXPECT_STREQ(TargetInfo::getStaticClass()->name,             "cc.TargetInfo");
    EXPECT_STREQ(TargetOverrideInfo::getStaticClass()->name,     "cc.TargetOverrideInfo");
    EXPECT_STREQ(CompPrefabInfo::getStaticClass()->name,         "cc.CompPrefabInfo");
    EXPECT_STREQ(PropertyOverrideInfo::getStaticClass()->name,   "CCPropertyOverrideInfo");
    EXPECT_STREQ(MountedChildrenInfo::getStaticClass()->name,    "cc.MountedChildrenInfo");
    EXPECT_STREQ(MountedComponentsInfo::getStaticClass()->name,  "cc.MountedComponentsInfo");
    EXPECT_STREQ(PrefabInstance::getStaticClass()->name,         "cc.PrefabInstance");
    EXPECT_STREQ(PrefabInfo::getStaticClass()->name,             "cc.PrefabInfo");
}

TEST(PrefabInfo, TargetInfoPropertiesRegistered) {
    auto *meta = TargetInfo::getStaticClass();
    auto *p = meta->findProperty("localID");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->typeId, TypeId::ARRAY);
    EXPECT_EQ(p->elementTypeId, TypeId::STRING);
}

TEST(PrefabInfo, PrefabInstancePropertiesRegistered) {
    auto *meta = PrefabInstance::getStaticClass();
    EXPECT_NE(meta->findProperty("fileId"),            nullptr);
    EXPECT_NE(meta->findProperty("mountedChildren"),   nullptr);
    EXPECT_NE(meta->findProperty("mountedComponents"), nullptr);
    EXPECT_NE(meta->findProperty("propertyOverrides"), nullptr);
    EXPECT_NE(meta->findProperty("removedComponents"), nullptr);
}

TEST(PrefabInfo, PrefabInfoPropertiesRegistered) {
    auto *meta = PrefabInfo::getStaticClass();
    EXPECT_NE(meta->findProperty("root"),            nullptr);
    EXPECT_NE(meta->findProperty("asset"),           nullptr);
    EXPECT_NE(meta->findProperty("fileId"),          nullptr);
    EXPECT_NE(meta->findProperty("instance"),        nullptr);
    EXPECT_NE(meta->findProperty("targetOverrides"), nullptr);
}

// ─── JSON round-trip: vector<string> array decode ────────────────────────
// Verifies the reflection extension for `vector<ccstd::string>` works end-to-end.

TEST(PrefabInfo, TargetInfoLocalIDRoundTrip) {
    const char *json = R"([
        {
            "__type__": "cc.TargetInfo",
            "localID": ["abc-123", "def-456", "ghi-789"]
        }
    ])";

    serialization::JsonDeserializer d;
    TargetInfo *ti = d.deserializeAs<TargetInfo>(json, 0);
    ASSERT_NE(ti, nullptr);
    IntrusivePtr<TargetInfo> owner(ti);

    ASSERT_EQ(ti->localID.size(), 3u);
    EXPECT_EQ(ti->localID[0], "abc-123");
    EXPECT_EQ(ti->localID[1], "def-456");
    EXPECT_EQ(ti->localID[2], "ghi-789");
}

TEST(PrefabInfo, TargetInfoEmptyLocalID) {
    const char *json = R"([
        {
            "__type__": "cc.TargetInfo",
            "localID": []
        }
    ])";

    serialization::JsonDeserializer d;
    IntrusivePtr<TargetInfo> ti(d.deserializeAs<TargetInfo>(json, 0));
    ASSERT_NE(ti.get(), nullptr);
    EXPECT_EQ(ti->localID.size(), 0u);
}

// ─── CompPrefabInfo ────────────────────────────────────────────────────────

TEST(PrefabInfo, CompPrefabInfoFileIdRoundTrip) {
    const char *json = R"([
        {
            "__type__": "cc.CompPrefabInfo",
            "fileId": "compfile-42"
        }
    ])";

    serialization::JsonDeserializer d;
    IntrusivePtr<CompPrefabInfo> info(d.deserializeAs<CompPrefabInfo>(json, 0));
    ASSERT_NE(info.get(), nullptr);
    EXPECT_EQ(info->fileId, "compfile-42");
}

// ─── PrefabInstance (no overrides, just structure) ─────────────────────────

TEST(PrefabInfo, PrefabInstanceEmptyBundleRoundTrip) {
    const char *json = R"([
        {
            "__type__": "cc.PrefabInstance",
            "fileId": "inst-root-1",
            "mountedChildren":   [],
            "mountedComponents": [],
            "propertyOverrides": [],
            "removedComponents": []
        }
    ])";

    serialization::JsonDeserializer d;
    IntrusivePtr<PrefabInstance> inst(d.deserializeAs<PrefabInstance>(json, 0));
    ASSERT_NE(inst.get(), nullptr);
    EXPECT_EQ(inst->fileId, "inst-root-1");
    EXPECT_EQ(inst->mountedChildren.size(), 0u);
    EXPECT_EQ(inst->mountedComponents.size(), 0u);
    EXPECT_EQ(inst->propertyOverrides.size(), 0u);
    EXPECT_EQ(inst->removedComponents.size(), 0u);
    EXPECT_FALSE(inst->expanded);
}

// ─── Full wire: PrefabInstance with mountedChildren ─────────────────────────
// Exercises nested array-of-pointer resolution for MountedChildrenInfo.

TEST(PrefabInfo, PrefabInstanceWithMountedChildren) {
    // Slot 0: the PrefabInstance root
    // Slot 1: one MountedChildrenInfo entry
    // Slot 2: the TargetInfo it points at
    const char *json = R"([
        {
            "__type__": "cc.PrefabInstance",
            "fileId": "inst-xyz",
            "mountedChildren":   [{"__id__": 1}],
            "mountedComponents": [],
            "propertyOverrides": [],
            "removedComponents": []
        },
        {
            "__type__": "cc.MountedChildrenInfo",
            "targetInfo": {"__id__": 2},
            "nodes": []
        },
        {
            "__type__": "cc.TargetInfo",
            "localID": ["parent-node-id"]
        }
    ])";

    serialization::JsonDeserializer d;
    IntrusivePtr<PrefabInstance> inst(d.deserializeAs<PrefabInstance>(json, 0));
    ASSERT_NE(inst.get(), nullptr);
    ASSERT_EQ(inst->mountedChildren.size(), 1u);

    MountedChildrenInfo *mci = inst->mountedChildren[0].get();
    ASSERT_NE(mci, nullptr);
    ASSERT_NE(mci->targetInfo.get(), nullptr);
    ASSERT_EQ(mci->targetInfo->localID.size(), 1u);
    EXPECT_EQ(mci->targetInfo->localID[0], "parent-node-id");
}

// ─── PropertyOverrideInfo (value field captured later — for now just
//     targetInfo + propertyPath) ─────────────────────────────────────────

TEST(PrefabInfo, PropertyOverrideInfoPathRoundTrip) {
    const char *json = R"([
        {
            "__type__": "CCPropertyOverrideInfo",
            "targetInfo":   {"__id__": 1},
            "propertyPath": ["_color", "r"],
            "value": 255
        },
        {
            "__type__": "cc.TargetInfo",
            "localID": ["button-root"]
        }
    ])";

    serialization::JsonDeserializer d;
    IntrusivePtr<PropertyOverrideInfo> po(d.deserializeAs<PropertyOverrideInfo>(json, 0));
    ASSERT_NE(po.get(), nullptr);
    ASSERT_NE(po->targetInfo.get(), nullptr);
    EXPECT_EQ(po->targetInfo->localID[0], "button-root");
    ASSERT_EQ(po->propertyPath.size(), 2u);
    EXPECT_EQ(po->propertyPath[0], "_color");
    EXPECT_EQ(po->propertyPath[1], "r");
    EXPECT_EQ(po->valueJson, "255");  // captured verbatim by the hook
}

TEST(PrefabInfo, PropertyOverrideInfoValueJsonCapturesAllShapes) {
    // The `value` hook should stringify arbitrary JSON fragments
    // verbatim — scalars, objects, arrays all round-trip as the
    // source text.
    struct Case {
        const char *source;
        const char *expected;
    };
    Case cases[] = {
        { R"(42)",                        "42"                  },
        { R"("hello world")",             "\"hello world\""     },
        { R"(true)",                      "true"                },
        { R"(null)",                      "null"                },
        { R"({"x":1,"y":2,"z":3})",       "{\"x\":1,\"y\":2,\"z\":3}" },
        { R"([1,2,3])",                   "[1,2,3]"             },
    };
    for (const auto &c : cases) {
        ccstd::string jsonText = ccstd::string("[{\"__type__\":\"CCPropertyOverrideInfo\",\"value\":") +
                                  c.source + "}]";
        serialization::JsonDeserializer d;
        IntrusivePtr<PropertyOverrideInfo> po(d.deserializeAs<PropertyOverrideInfo>(jsonText, 0));
        ASSERT_NE(po.get(), nullptr);
        EXPECT_EQ(po->valueJson, c.expected) << "source was: " << c.source;
    }
}

// ─── decodeJsonValueToProperty (apply-time decode) ────────────────────────

namespace {

class PropTarget {
    CC_CLASS_DECL(PropTarget, void)
public:
    int32_t       _i{0};
    float         _f{0.f};
    bool          _b{false};
    ccstd::string _s;
    cc::Vec3      _pos;
    cc::Color     _tint;
};

}  // namespace

CC_IMPLEMENT_ROOT_CLASS(PropTarget, "prefab_test.PropTarget")
    .property("i",    &PropTarget::_i)
    .property("f",    &PropTarget::_f)
    .property("b",    &PropTarget::_b)
    .property("s",    &PropTarget::_s)
    .property("pos",  &PropTarget::_pos)
    .property("tint", &PropTarget::_tint)
CC_END_CLASS(PropTarget);

TEST(PrefabInfo, DecodeJsonValueScalarInt) {
    PropTarget t;
    auto *meta = PropTarget::getStaticClass();
    auto *p = meta->findProperty("i");
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(serialization::decodeJsonValueToProperty(&t, *p, "42"));
    EXPECT_EQ(t._i, 42);
}

TEST(PrefabInfo, DecodeJsonValueScalarFloat) {
    PropTarget t;
    auto *p = PropTarget::getStaticClass()->findProperty("f");
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(serialization::decodeJsonValueToProperty(&t, *p, "3.75"));
    EXPECT_FLOAT_EQ(t._f, 3.75f);
}

TEST(PrefabInfo, DecodeJsonValueScalarBool) {
    PropTarget t;
    auto *p = PropTarget::getStaticClass()->findProperty("b");
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(serialization::decodeJsonValueToProperty(&t, *p, "true"));
    EXPECT_TRUE(t._b);
}

TEST(PrefabInfo, DecodeJsonValueScalarString) {
    PropTarget t;
    auto *p = PropTarget::getStaticClass()->findProperty("s");
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(serialization::decodeJsonValueToProperty(&t, *p, R"("hello")"));
    EXPECT_EQ(t._s, "hello");
}

TEST(PrefabInfo, DecodeJsonValueVec3) {
    PropTarget t;
    auto *p = PropTarget::getStaticClass()->findProperty("pos");
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(serialization::decodeJsonValueToProperty(&t, *p, R"({"x":1,"y":2,"z":3})"));
    EXPECT_FLOAT_EQ(t._pos.x, 1.f);
    EXPECT_FLOAT_EQ(t._pos.y, 2.f);
    EXPECT_FLOAT_EQ(t._pos.z, 3.f);
}

TEST(PrefabInfo, DecodeJsonValueColor) {
    PropTarget t;
    auto *p = PropTarget::getStaticClass()->findProperty("tint");
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(serialization::decodeJsonValueToProperty(
        &t, *p, R"({"__type__":"cc.Color","r":200,"g":100,"b":50,"a":255})"));
    EXPECT_EQ(t._tint.r, 200);
    EXPECT_EQ(t._tint.g, 100);
    EXPECT_EQ(t._tint.b, 50);
    EXPECT_EQ(t._tint.a, 255);
}

TEST(PrefabInfo, DecodeJsonValueBadJsonReturnsFalse) {
    PropTarget t;
    auto *p = PropTarget::getStaticClass()->findProperty("i");
    EXPECT_FALSE(serialization::decodeJsonValueToProperty(&t, *p, "{not json"));
    EXPECT_EQ(t._i, 0);  // untouched
}

TEST(PrefabInfo, DecodeJsonValueEmptyTextReturnsFalse) {
    PropTarget t;
    auto *p = PropTarget::getStaticClass()->findProperty("i");
    EXPECT_FALSE(serialization::decodeJsonValueToProperty(&t, *p, ""));
}
