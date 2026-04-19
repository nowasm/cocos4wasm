#include "cocos/asset/SpriteFrameLoader.h"

#include <cstring>
#include <fstream>
#include <sstream>

#include "base/Log.h"
#include "base/memory/Memory.h"
#include "cocos/asset/AssetManager.h"
#include "platform/FileUtils.h"
#include "rapidjson/document.h"

namespace cc {

namespace {

// Helper: resolve the "payload" JSON object. Editor sometimes wraps the
// real data as `{"__type__":"cc.SpriteFrame","content":{...}}`, sometimes
// as a top-level array `[{"__type__":"cc.SpriteFrame"}, {...}]`,
// sometimes just the flat content object. Return the object to read
// fields from, or null if the shape isn't recognized.
const rapidjson::Value *resolvePayload(const rapidjson::Document &doc) {
    // Case 1: top-level object
    if (doc.IsObject()) {
        // 1a: wrapper object with content sub-field
        if (doc.HasMember("__type__") && doc.HasMember("content") &&
            doc["content"].IsObject()) {
            return &doc["content"];
        }
        // 1b: flat object (no wrapper) — payload is the doc itself
        return &static_cast<const rapidjson::Value &>(doc);
    }
    // Case 2: top-level array — `[{"__type__":"cc.SpriteFrame"}, {fields}]`
    if (doc.IsArray() && doc.Size() >= 2 &&
        doc[0].IsObject() && doc[1].IsObject()) {
        return &doc[1];
    }
    return nullptr;
}

bool readFloat(const rapidjson::Value &obj, const char *key, float &out) {
    if (!obj.HasMember(key)) return false;
    const auto &v = obj[key];
    if (v.IsNumber()) { out = v.GetFloat(); return true; }
    return false;
}

// {"x":..,"y":..} → Vec2
bool readVec2Object(const rapidjson::Value &obj, const char *key, Vec2 &out) {
    if (!obj.HasMember(key) || !obj[key].IsObject()) return false;
    const auto &v = obj[key];
    float x = 0.f, y = 0.f;
    readFloat(v, "x", x);
    readFloat(v, "y", y);
    out.set(x, y);
    return true;
}

// {"width":..,"height":..} → Vec2(width, height)
bool readSizeObject(const rapidjson::Value &obj, const char *key, Vec2 &out) {
    if (!obj.HasMember(key) || !obj[key].IsObject()) return false;
    const auto &v = obj[key];
    float w = 0.f, h = 0.f;
    readFloat(v, "width",  w);
    readFloat(v, "height", h);
    out.set(w, h);
    return true;
}

// {"x":..,"y":..,"width":..,"height":..} → Vec4(x, y, width, height)
bool readRectObject(const rapidjson::Value &obj, const char *key, Vec4 &out) {
    if (!obj.HasMember(key) || !obj[key].IsObject()) return false;
    const auto &v = obj[key];
    float x = 0.f, y = 0.f, w = 0.f, h = 0.f;
    readFloat(v, "x",      x);
    readFloat(v, "y",      y);
    readFloat(v, "width",  w);
    readFloat(v, "height", h);
    out.set(x, y, w, h);
    return true;
}

// [l, t, r, b] → Vec4(left, top, right, bottom)
bool readCapInsetsArray(const rapidjson::Value &obj, const char *key, Vec4 &out) {
    if (!obj.HasMember(key) || !obj[key].IsArray()) return false;
    const auto &a = obj[key];
    if (a.Size() < 4) return false;
    out.set(
        a[0].IsNumber() ? a[0].GetFloat() : 0.f,
        a[1].IsNumber() ? a[1].GetFloat() : 0.f,
        a[2].IsNumber() ? a[2].GetFloat() : 0.f,
        a[3].IsNumber() ? a[3].GetFloat() : 0.f);
    return true;
}

}  // namespace

SpriteFrame *SpriteFrameLoader::loadFromFile(const ccstd::string &absPath) {
    // Prefer FileUtils when the engine has booted (handles Android APK /
    // Emscripten MEMFS paths), but fall back to stdio when it's absent —
    // keeps the loader usable from unit tests and tooling that don't
    // stand up a full platform layer.
    ccstd::string text;
    if (auto *fu = FileUtils::getInstance()) {
        text = fu->getStringFromFile(absPath);
    } else {
        std::ifstream in(absPath.c_str(), std::ios::binary);
        if (in) {
            std::ostringstream ss;
            ss << in.rdbuf();
            text = ss.str().c_str();
        }
    }
    if (text.empty()) {
        CC_LOG_ERROR("[SpriteFrameLoader] empty / missing file: %s", absPath.c_str());
        return nullptr;
    }

    rapidjson::Document doc;
    doc.Parse(text.c_str(), text.size());
    if (doc.HasParseError()) {
        CC_LOG_ERROR("[SpriteFrameLoader] JSON parse error in %s at offset %zu",
                     absPath.c_str(), (size_t)doc.GetErrorOffset());
        return nullptr;
    }

    const rapidjson::Value *payload = resolvePayload(doc);
    if (!payload) {
        CC_LOG_ERROR("[SpriteFrameLoader] unrecognised JSON shape in %s", absPath.c_str());
        return nullptr;
    }

    auto *sf = ccnew SpriteFrame();

    // Simple value fields — all optional; defaults on SpriteFrame hold if
    // absent.
    if (payload->HasMember("name") && (*payload)["name"].IsString()) {
        sf->setName((*payload)["name"].GetString());
    }
    if (payload->HasMember("rotated") && (*payload)["rotated"].IsBool()) {
        sf->setRotated((*payload)["rotated"].GetBool());
    }
    if (payload->HasMember("packable") && (*payload)["packable"].IsBool()) {
        sf->setPackable((*payload)["packable"].GetBool());
    }
    {
        float f = 0.f;
        if (readFloat(*payload, "pixelsToUnit", f)) sf->setPixelsToUnit(f);
    }

    // Structured fields.
    Vec4 rect; if (readRectObject(*payload, "rect", rect))  sf->setRect(rect);
    Vec2 v2;
    if (readVec2Object (*payload, "offset", v2)) sf->setOffset(v2);
    if (readVec2Object (*payload, "pivot",  v2)) sf->setPivot(v2);
    if (readSizeObject (*payload, "originalSize", v2)) sf->setOriginalSize(v2);
    Vec4 insets; if (readCapInsetsArray(*payload, "capInsets", insets)) sf->setCapInsets(insets);

    // Texture ref — upstream stores either a raw UUID string or an
    // `{__uuid__: "..."}` wrapper. Both shapes route through AssetManager.
    if (payload->HasMember("texture")) {
        const auto &tex = (*payload)["texture"];
        ccstd::string uuid;
        if (tex.IsString()) {
            uuid = tex.GetString();
        } else if (tex.IsObject() && tex.HasMember("__uuid__") && tex["__uuid__"].IsString()) {
            uuid = tex["__uuid__"].GetString();
        }
        if (!uuid.empty()) {
            auto texture = AssetManager::get().load<Texture2D>(uuid);
            if (texture) sf->setTexture(texture.get());
        }
    }

    return sf;
}

}  // namespace cc
