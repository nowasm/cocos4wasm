#include "cocos/asset/EffectLoader.h"

#include "base/Log.h"
#include "platform/FileUtils.h"
#include "rapidjson/document.h"

namespace cc {

EffectAsset *EffectLoader::loadFromFile(const ccstd::string &absPath) {
    auto *fu = FileUtils::getInstance();
    ccstd::string text = fu->getStringFromFile(absPath);
    if (text.empty()) {
        CC_LOG_ERROR("[EffectLoader] empty / missing file: %s", absPath.c_str());
        return nullptr;
    }

    rapidjson::Document doc;
    doc.Parse(text.c_str(), text.size());
    if (doc.HasParseError()) {
        CC_LOG_ERROR("[EffectLoader] JSON parse error in %s at offset %zu",
                     absPath.c_str(), (size_t)doc.GetErrorOffset());
        return nullptr;
    }

    // Accept either a bare object or a Cocos-array with a single cc.EffectAsset
    // entry — both are valid authoring shapes depending on whether the file
    // came through the Editor's serialiser.
    const rapidjson::Value *obj = nullptr;
    if (doc.IsObject()) {
        obj = &doc;
    } else if (doc.IsArray() && doc.Size() > 0 && doc[0].IsObject()) {
        obj = &doc[0];
    }
    if (!obj) {
        CC_LOG_ERROR("[EffectLoader] unexpected top-level JSON shape in %s",
                     absPath.c_str());
        return nullptr;
    }

    // Pull out the effect name. Creator stores it on either `name` (legacy) or
    // `_name` (CCObject-style serialised form) — accept both.
    const char *name = nullptr;
    if (obj->HasMember("_name") && (*obj)["_name"].IsString()) {
        name = (*obj)["_name"].GetString();
    } else if (obj->HasMember("name") && (*obj)["name"].IsString()) {
        name = (*obj)["name"].GetString();
    }
    if (!name || !name[0]) {
        CC_LOG_ERROR("[EffectLoader] no (_)name in %s — cannot resolve effect",
                     absPath.c_str());
        return nullptr;
    }

    EffectAsset *pre = EffectAsset::get(name);
    if (pre) {
        return pre;
    }

    CC_LOG_WARNING("[EffectLoader] '%s' not in the builtin registry (%s) — "
                   "full .effect compilation isn't wired yet; add it to "
                   "builtin-effects.json or author the shader as a builtin.",
                   name, absPath.c_str());
    return nullptr;
}

}  // namespace cc
