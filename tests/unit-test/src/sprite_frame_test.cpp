#include <cstdio>
#include <filesystem>
#include <fstream>

#include "base/Ptr.h"
#include "cocos/asset/SpriteFrame.h"
#include "cocos/asset/SpriteFrameLoader.h"
#include "core/assets/Texture2D.h"
#include "core/reflection/Reflection.h"
#include "gtest/gtest.h"

using namespace cc;

// ─── Reflection registration ──────────────────────────────────────────────

TEST(SpriteFrame, ClassRegistered) {
    auto *meta = SpriteFrame::getStaticClass();
    ASSERT_NE(meta, nullptr);
    EXPECT_STREQ(meta->name, "cc.SpriteFrame");
    // No fields registered — data comes in through the loader, not the
    // standard @serializable path.
    EXPECT_EQ(meta->properties.size(), 0u);
}

TEST(SpriteFrame, DefaultsMatchUpstream) {
    IntrusivePtr<SpriteFrame> sf = ccnew SpriteFrame();
    EXPECT_EQ(sf->getTexture(), nullptr);
    EXPECT_FLOAT_EQ(sf->getRect().x, 0.f);
    EXPECT_FLOAT_EQ(sf->getRect().y, 0.f);
    EXPECT_FLOAT_EQ(sf->getRect().z, 0.f);
    EXPECT_FLOAT_EQ(sf->getRect().w, 0.f);
    EXPECT_FLOAT_EQ(sf->getOffset().x, 0.f);
    EXPECT_FLOAT_EQ(sf->getOffset().y, 0.f);
    EXPECT_FALSE(sf->isRotated());
    EXPECT_FLOAT_EQ(sf->getPivot().x, 0.5f);  // centred default
    EXPECT_FLOAT_EQ(sf->getPivot().y, 0.5f);
    EXPECT_TRUE(sf->isPackable());
    EXPECT_FLOAT_EQ(sf->getPixelsToUnit(), 100.f);
}

TEST(SpriteFrame, RotationSwapsWidthHeight) {
    IntrusivePtr<SpriteFrame> sf = ccnew SpriteFrame();
    sf->setRect({0.f, 0.f, 64.f, 32.f});
    EXPECT_FLOAT_EQ(sf->getWidth(),  64.f);
    EXPECT_FLOAT_EQ(sf->getHeight(), 32.f);

    sf->setRotated(true);
    // Rotated-90° frame: the slice's effective dimensions swap.
    EXPECT_FLOAT_EQ(sf->getWidth(),  32.f);
    EXPECT_FLOAT_EQ(sf->getHeight(), 64.f);
}

TEST(SpriteFrame, CapInsetsComponentAccessors) {
    IntrusivePtr<SpriteFrame> sf = ccnew SpriteFrame();
    sf->setCapInsets({1.f, 2.f, 3.f, 4.f});
    EXPECT_FLOAT_EQ(sf->getInsetLeft(),   1.f);
    EXPECT_FLOAT_EQ(sf->getInsetTop(),    2.f);
    EXPECT_FLOAT_EQ(sf->getInsetRight(),  3.f);
    EXPECT_FLOAT_EQ(sf->getInsetBottom(), 4.f);

    sf->setInsetLeft(10.f);
    sf->setInsetBottom(40.f);
    EXPECT_FLOAT_EQ(sf->getCapInsets().x, 10.f);
    EXPECT_FLOAT_EQ(sf->getCapInsets().w, 40.f);
}

// ─── Loader: wrapper & flat-object JSON shapes ────────────────────────────

namespace {

// Write `content` to a temp file in the system temp dir. Returns the path.
ccstd::string writeTempJson(const char *tag, const char *content) {
    auto dir = std::filesystem::temp_directory_path();
    auto path = dir / (ccstd::string("ccos_sf_test_") + tag + ".json").c_str();
    std::ofstream f(path);
    f << content;
    f.close();
    return ccstd::string(path.string().c_str());
}

}  // namespace

TEST(SpriteFrameLoader, FlatObjectShape) {
    // Root is the payload — no wrapper. All optional fields present.
    const char *json = R"({
        "name": "frame01",
        "rect": { "x": 10, "y": 20, "width": 128, "height": 64 },
        "offset": { "x": 1, "y": -2 },
        "originalSize": { "width": 130, "height": 66 },
        "rotated": true,
        "capInsets": [4, 5, 6, 7],
        "pivot": { "x": 0.25, "y": 0.75 },
        "packable": false,
        "pixelsToUnit": 50
    })";
    auto path = writeTempJson("flat", json);

    SpriteFrame *sf = SpriteFrameLoader::loadFromFile(path);
    ASSERT_NE(sf, nullptr);
    IntrusivePtr<SpriteFrame> owner(sf);

    EXPECT_EQ(sf->getName(), "frame01");
    EXPECT_FLOAT_EQ(sf->getRect().x, 10.f);
    EXPECT_FLOAT_EQ(sf->getRect().y, 20.f);
    EXPECT_FLOAT_EQ(sf->getRect().z, 128.f);
    EXPECT_FLOAT_EQ(sf->getRect().w, 64.f);
    EXPECT_FLOAT_EQ(sf->getOffset().x,  1.f);
    EXPECT_FLOAT_EQ(sf->getOffset().y, -2.f);
    EXPECT_FLOAT_EQ(sf->getOriginalSize().x, 130.f);
    EXPECT_FLOAT_EQ(sf->getOriginalSize().y,  66.f);
    EXPECT_TRUE(sf->isRotated());
    EXPECT_FLOAT_EQ(sf->getCapInsets().x, 4.f);
    EXPECT_FLOAT_EQ(sf->getCapInsets().y, 5.f);
    EXPECT_FLOAT_EQ(sf->getCapInsets().z, 6.f);
    EXPECT_FLOAT_EQ(sf->getCapInsets().w, 7.f);
    EXPECT_FLOAT_EQ(sf->getPivot().x, 0.25f);
    EXPECT_FLOAT_EQ(sf->getPivot().y, 0.75f);
    EXPECT_FALSE(sf->isPackable());
    EXPECT_FLOAT_EQ(sf->getPixelsToUnit(), 50.f);
}

TEST(SpriteFrameLoader, WrapperShape) {
    // Editor-typical: outer object with __type__ + content sub-field.
    const char *json = R"({
        "__type__": "cc.SpriteFrame",
        "content": {
            "name": "frame02",
            "rect": { "x": 0, "y": 0, "width": 32, "height": 32 }
        }
    })";
    auto path = writeTempJson("wrapper", json);

    IntrusivePtr<SpriteFrame> sf(SpriteFrameLoader::loadFromFile(path));
    ASSERT_NE(sf.get(), nullptr);
    EXPECT_EQ(sf->getName(), "frame02");
    EXPECT_FLOAT_EQ(sf->getRect().z, 32.f);
    EXPECT_FLOAT_EQ(sf->getRect().w, 32.f);
}

TEST(SpriteFrameLoader, ArrayShape) {
    // Second-entry-holds-the-payload shape — legacy export variant.
    const char *json = R"([
        {"__type__": "cc.SpriteFrame"},
        {
            "name": "frame03",
            "rect": { "x": 5, "y": 5, "width": 64, "height": 64 },
            "rotated": false
        }
    ])";
    auto path = writeTempJson("array", json);

    IntrusivePtr<SpriteFrame> sf(SpriteFrameLoader::loadFromFile(path));
    ASSERT_NE(sf.get(), nullptr);
    EXPECT_EQ(sf->getName(), "frame03");
    EXPECT_FLOAT_EQ(sf->getRect().x, 5.f);
    EXPECT_FALSE(sf->isRotated());
}

TEST(SpriteFrameLoader, DefaultsWhenFieldsAbsent) {
    // Minimum-viable JSON: just name + rect. All other fields fall back
    // to SpriteFrame defaults.
    const char *json = R"({
        "name": "minimal",
        "rect": { "x": 0, "y": 0, "width": 16, "height": 16 }
    })";
    auto path = writeTempJson("minimal", json);

    IntrusivePtr<SpriteFrame> sf(SpriteFrameLoader::loadFromFile(path));
    ASSERT_NE(sf.get(), nullptr);
    EXPECT_EQ(sf->getName(), "minimal");
    EXPECT_FLOAT_EQ(sf->getPivot().x, 0.5f);  // unchanged from default
    EXPECT_TRUE(sf->isPackable());
    EXPECT_FALSE(sf->isRotated());
}

TEST(SpriteFrameLoader, CapInsetsArrayShorterThanFourIsIgnored) {
    // Defensive: a malformed 3-element capInsets shouldn't crash; it
    // should just leave the default [0,0,0,0] untouched.
    const char *json = R"({
        "name": "bad",
        "capInsets": [1, 2, 3]
    })";
    auto path = writeTempJson("short_insets", json);

    IntrusivePtr<SpriteFrame> sf(SpriteFrameLoader::loadFromFile(path));
    ASSERT_NE(sf.get(), nullptr);
    EXPECT_FLOAT_EQ(sf->getCapInsets().x, 0.f);
    EXPECT_FLOAT_EQ(sf->getCapInsets().y, 0.f);
    EXPECT_FLOAT_EQ(sf->getCapInsets().z, 0.f);
    EXPECT_FLOAT_EQ(sf->getCapInsets().w, 0.f);
}

TEST(SpriteFrameLoader, ReturnsNullOnBadJson) {
    const char *json = "{not json";
    auto path = writeTempJson("bad", json);

    SpriteFrame *sf = SpriteFrameLoader::loadFromFile(path);
    EXPECT_EQ(sf, nullptr);
}

TEST(SpriteFrameLoader, ReturnsNullOnMissingFile) {
    SpriteFrame *sf = SpriteFrameLoader::loadFromFile(
        "C:/definitely/does/not/exist/spriteframe.json");
    EXPECT_EQ(sf, nullptr);
}
