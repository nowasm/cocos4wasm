#include "cocos/core/reflection/Reflection.h"
#include "cocos/serialization/JsonDeserializer.h"
#include "gtest/gtest.h"

// ─── Fixtures: plain reflected types ──────────────────────────────────────
// These are POD-ish test classes (no RefCounted base) used to exercise
// scalar/nested decoding paths without involving the engine's refcounted
// lifecycle. Test names use a "jd." prefix to avoid clashing with any
// real engine classes registered in the same process.

enum class Mood : int32_t { HAPPY = 0, SAD = 1, FURIOUS = 2 };

class Scalars {
    CC_CLASS_DECL(Scalars, void)
public:
    Scalars() = default;
    virtual ~Scalars() = default;

    bool          _bool{false};
    int32_t       _int{0};
    uint32_t      _uint{0};
    float         _float{0.0f};
    double        _double{0.0};
    ccstd::string _string;
    Mood          _mood{Mood::HAPPY};
};

CC_IMPLEMENT_ROOT_CLASS(Scalars, "jd.Scalars")
    .property("b",  &Scalars::_bool)
    .property("i",  &Scalars::_int)
    .property("u",  &Scalars::_uint)
    .property("f",  &Scalars::_float)
    .property("d",  &Scalars::_double)
    .property("s",  &Scalars::_string)
    .property("m",  &Scalars::_mood)
CC_END_CLASS(Scalars);

class Nested {
    CC_CLASS_DECL(Nested, void)
public:
    Nested() = default;
    virtual ~Nested() = default;

    cc::Vec2       _v2;
    cc::Vec3       _v3;
    cc::Vec4       _v4;
    cc::Quaternion _q;
    cc::Color      _color;
};

CC_IMPLEMENT_ROOT_CLASS(Nested, "jd.Nested")
    .property("v2",    &Nested::_v2)
    .property("v3",    &Nested::_v3)
    .property("v4",    &Nested::_v4)
    .property("quat",  &Nested::_q)
    .property("color", &Nested::_color)
CC_END_CLASS(Nested);

// ─── Tests: scalar and nested-object deserialization ──────────────────────

using cc::serialization::JsonDeserializer;

TEST(JsonDeserializer, ScalarsRoundTrip) {
    JsonDeserializer d;
    auto *obj = d.deserializeAs<Scalars>(R"([
        {
            "__type__": "jd.Scalars",
            "b": true,
            "i": -42,
            "u": 1000,
            "f": 1.5,
            "d": 3.141592653589793,
            "s": "hello",
            "m": 2
        }
    ])");
    ASSERT_NE(obj, nullptr);
    EXPECT_TRUE(obj->_bool);
    EXPECT_EQ(obj->_int, -42);
    EXPECT_EQ(obj->_uint, 1000u);
    EXPECT_FLOAT_EQ(obj->_float, 1.5f);
    EXPECT_DOUBLE_EQ(obj->_double, 3.141592653589793);
    EXPECT_EQ(obj->_string, "hello");
    EXPECT_EQ(obj->_mood, Mood::FURIOUS);
    delete obj;
}

TEST(JsonDeserializer, MissingFieldsKeepDefaults) {
    JsonDeserializer d;
    auto *obj = d.deserializeAs<Scalars>(R"([
        {"__type__": "jd.Scalars", "i": 7}
    ])");
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->_int, 7);
    EXPECT_FALSE(obj->_bool);
    EXPECT_EQ(obj->_string, "");  // default-constructed
    delete obj;
}

TEST(JsonDeserializer, UnknownFieldsIgnored) {
    // Forward-compat: fields the reflection system doesn't know about should
    // be silently skipped rather than failing the whole load.
    JsonDeserializer d;
    auto *obj = d.deserializeAs<Scalars>(R"([
        {"__type__": "jd.Scalars", "i": 1, "not_a_field": 999, "b": true}
    ])");
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->_int, 1);
    EXPECT_TRUE(obj->_bool);
    delete obj;
}

TEST(JsonDeserializer, NestedVec3) {
    JsonDeserializer d;
    auto *obj = d.deserializeAs<Nested>(R"([
        {"__type__": "jd.Nested", "v3": {"x": 1, "y": 2, "z": 3}}
    ])");
    ASSERT_NE(obj, nullptr);
    EXPECT_FLOAT_EQ(obj->_v3.x, 1.0f);
    EXPECT_FLOAT_EQ(obj->_v3.y, 2.0f);
    EXPECT_FLOAT_EQ(obj->_v3.z, 3.0f);
    delete obj;
}

TEST(JsonDeserializer, NestedVec3PartialComponents) {
    // Missing components default to 0; partial objects still deserialize.
    JsonDeserializer d;
    auto *obj = d.deserializeAs<Nested>(R"([
        {"__type__": "jd.Nested", "v3": {"x": 7.5}}
    ])");
    ASSERT_NE(obj, nullptr);
    EXPECT_FLOAT_EQ(obj->_v3.x, 7.5f);
    EXPECT_FLOAT_EQ(obj->_v3.y, 0.0f);
    EXPECT_FLOAT_EQ(obj->_v3.z, 0.0f);
    delete obj;
}

TEST(JsonDeserializer, NestedQuatDefaultsWTo1) {
    // w defaults to 1 (identity quaternion) when missing.
    JsonDeserializer d;
    auto *obj = d.deserializeAs<Nested>(R"([
        {"__type__": "jd.Nested", "quat": {"x": 0, "y": 0, "z": 0}}
    ])");
    ASSERT_NE(obj, nullptr);
    EXPECT_FLOAT_EQ(obj->_q.w, 1.0f);
    delete obj;
}

TEST(JsonDeserializer, NestedColor) {
    JsonDeserializer d;
    auto *obj = d.deserializeAs<Nested>(R"([
        {"__type__": "jd.Nested", "color": {"r": 200, "g": 100, "b": 50, "a": 255}}
    ])");
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->_color.r, 200);
    EXPECT_EQ(obj->_color.g, 100);
    EXPECT_EQ(obj->_color.b, 50);
    EXPECT_EQ(obj->_color.a, 255);
    delete obj;
}

TEST(JsonDeserializer, MalformedJsonReturnsNull) {
    JsonDeserializer d;
    auto *obj = d.deserializeAs<Scalars>("not valid json at all");
    EXPECT_EQ(obj, nullptr);
}

TEST(JsonDeserializer, NonArrayTopLevelReturnsNull) {
    JsonDeserializer d;
    auto *obj = d.deserializeAs<Scalars>(R"({"__type__": "jd.Scalars"})");
    EXPECT_EQ(obj, nullptr);
}

TEST(JsonDeserializer, UnknownTypeReturnsNull) {
    JsonDeserializer d;
    auto *obj = d.deserializeAs<Scalars>(R"([
        {"__type__": "jd.DoesNotExist"}
    ])");
    EXPECT_EQ(obj, nullptr);
}

TEST(JsonDeserializer, EmptyArrayReturnsNull) {
    JsonDeserializer d;
    auto *obj = d.deserializeAs<Scalars>("[]");
    EXPECT_EQ(obj, nullptr);
}

TEST(JsonDeserializer, MultipleTopLevelObjects) {
    // Objects after index 0 should still be allocated but only the root
    // is returned. Verify objectCount reflects all slots.
    JsonDeserializer d;
    auto *obj = d.deserializeAs<Scalars>(R"([
        {"__type__": "jd.Scalars", "i": 1},
        {"__type__": "jd.Scalars", "i": 2},
        {"__type__": "jd.Scalars", "i": 3}
    ])");
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(d.objectCount(), 3u);
    EXPECT_EQ(obj->_int, 1);
    // Other slots are owned by the deserializer and leak unless a parent
    // object references them — which non-graph scalars can't. Release.
    delete static_cast<Scalars *>(d.objectAt(1));
    delete static_cast<Scalars *>(d.objectAt(2));
    delete obj;
}
