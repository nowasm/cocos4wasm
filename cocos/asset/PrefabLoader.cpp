#include "cocos/asset/PrefabLoader.h"

#include <cstring>

#include "base/Log.h"
#include "base/memory/Memory.h"
#include "platform/FileUtils.h"
#include "rapidjson/document.h"

namespace cc {

Prefab *PrefabLoader::loadFromFile(const ccstd::string &absPath) {
    auto *fu = FileUtils::getInstance();
    ccstd::string text = fu->getStringFromFile(absPath);
    if (text.empty()) {
        CC_LOG_ERROR("[PrefabLoader] empty / missing file: %s", absPath.c_str());
        return nullptr;
    }

    rapidjson::Document doc;
    doc.Parse(text.c_str(), text.size());
    if (doc.HasParseError()) {
        CC_LOG_ERROR("[PrefabLoader] JSON parse error in %s at offset %zu",
                     absPath.c_str(), (size_t)doc.GetErrorOffset());
        return nullptr;
    }
    if (!doc.IsArray() || doc.Size() == 0) {
        CC_LOG_ERROR("[PrefabLoader] expected non-empty JSON array in %s",
                     absPath.c_str());
        return nullptr;
    }

    // Detect the Creator wrapper: `cc.Prefab` at slot 0 with `data.__id__`
    // pointing at the real Node root.
    size_t rootIndex = 0;
    const auto &first = doc[0];
    if (first.IsObject() &&
        first.HasMember("__type__") && first["__type__"].IsString() &&
        std::strcmp(first["__type__"].GetString(), "cc.Prefab") == 0) {
        if (first.HasMember("data") && first["data"].IsObject() &&
            first["data"].HasMember("__id__") && first["data"]["__id__"].IsInt()) {
            int id = first["data"]["__id__"].GetInt();
            if (id <= 0 || id >= static_cast<int>(doc.Size())) {
                CC_LOG_ERROR("[PrefabLoader] wrapper.data.__id__=%d out of range (size=%u)",
                             id, doc.Size());
                return nullptr;
            }
            rootIndex = static_cast<size_t>(id);
        } else {
            CC_LOG_ERROR("[PrefabLoader] cc.Prefab wrapper missing data.__id__ in %s",
                         absPath.c_str());
            return nullptr;
        }
    }

    auto *prefab = ccnew Prefab();
    prefab->setData(text);
    prefab->setRootIndex(rootIndex);
    return prefab;
}

}  // namespace cc
