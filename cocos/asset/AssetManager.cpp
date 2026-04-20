#include "cocos/asset/AssetManager.h"

#include "base/Log.h"
#include "platform/FileUtils.h"
#include "rapidjson/document.h"

namespace cc {

AssetManager &AssetManager::get() {
    static AssetManager instance;
    return instance;
}

bool AssetManager::loadUuidMap(const ccstd::string &uuidMapPath) {
    auto *fu = FileUtils::getInstance();
    ccstd::string resolved = fu->isFileExist(uuidMapPath) ? uuidMapPath
                                                          : fu->fullPathForFilename(uuidMapPath);
    if (resolved.empty() || !fu->isFileExist(resolved)) {
        CC_LOG_ERROR("[AssetManager] uuid-map not found: %s", uuidMapPath.c_str());
        return false;
    }

    ccstd::string text = fu->getStringFromFile(resolved);
    if (text.empty()) {
        CC_LOG_ERROR("[AssetManager] uuid-map empty: %s", resolved.c_str());
        return false;
    }

    rapidjson::Document doc;
    doc.Parse(text.data(), text.size());
    if (doc.HasParseError() || !doc.IsObject()) {
        CC_LOG_ERROR("[AssetManager] uuid-map JSON parse error: %s", resolved.c_str());
        return false;
    }

    _uuidToPath.clear();
    for (auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it) {
        if (!it->name.IsString() || !it->value.IsString()) continue;
        _uuidToPath.emplace(it->name.GetString(), it->value.GetString());
    }

    CC_LOG_INFO("[AssetManager] loaded uuid-map: %zu entries from %s",
                _uuidToPath.size(), resolved.c_str());
    return true;
}

void AssetManager::setAssetRoot(const ccstd::string &rootDir) {
    _assetRoot = rootDir;
    if (!_assetRoot.empty() && _assetRoot.back() != '/' && _assetRoot.back() != '\\') {
        _assetRoot.push_back('/');
    }
}

void AssetManager::registerLoader(const char *extension, LoaderFn fn) {
    if (!extension || !*extension) return;
    _loaders[extension] = std::move(fn);
}

AssetManager::LoaderFn AssetManager::findLoader(const ccstd::string &path) const {
    auto pos = path.find_last_of('.');
    if (pos == ccstd::string::npos) return nullptr;
    const ccstd::string ext = path.substr(pos);
    auto it = _loaders.find(ext);
    if (it == _loaders.end()) return nullptr;
    return it->second;
}

IntrusivePtr<Asset> AssetManager::loadAsset(const ccstd::string &uuid) {
    auto cached = _cache.find(uuid);
    if (cached != _cache.end()) return cached->second;

    // Editor-exported JSONs reference sub-assets via `<main-uuid>@<sub-id>`
    // syntax (e.g. a SpriteFrame nested inside a texture bundle). We
    // try the full uuid first — projects that ship explicit sub-asset
    // JSON files register them that way. If that misses, strip the
    // `@<sub-id>` and retry with the base uuid so a main-texture entry
    // still satisfies sprite references that forgot their SpriteFrame
    // sidecar.
    auto pathIt = _uuidToPath.find(uuid);
    const auto at = uuid.find('@');
    ccstd::string baseKey;
    if (pathIt == _uuidToPath.end() && at != ccstd::string::npos) {
        baseKey = uuid.substr(0, at);
        pathIt = _uuidToPath.find(baseKey);
    }

    if (pathIt == _uuidToPath.end()) {
        if (at != ccstd::string::npos) {
            CC_LOG_WARNING("[AssetManager] sub-asset uuid unresolved: %s "
                           "(base '%s' not in uuid-map — built-in or unshipped asset?)",
                           uuid.c_str(), baseKey.c_str());
        } else {
            CC_LOG_ERROR("[AssetManager] unknown uuid: %s", uuid.c_str());
        }
        return nullptr;
    }

    const ccstd::string relPath = pathIt->second;
    const ccstd::string absPath = _assetRoot + relPath;

    LoaderFn loader = findLoader(relPath);
    if (!loader) {
        CC_LOG_ERROR("[AssetManager] no loader registered for '%s' (uuid %s)",
                     relPath.c_str(), uuid.c_str());
        return nullptr;
    }

    IntrusivePtr<Asset> asset = loader(absPath, uuid);
    if (!asset) {
        CC_LOG_ERROR("[AssetManager] loader returned null for %s (uuid %s)",
                     absPath.c_str(), uuid.c_str());
        return nullptr;
    }

    _cache[uuid] = asset;
    CC_LOG_INFO("[AssetManager] loaded %s (uuid %s, cached=%zu)",
                relPath.c_str(), uuid.c_str(), _cache.size());
    return asset;
}

void AssetManager::evict(const ccstd::string &uuid) {
    _cache.erase(uuid);
}

void AssetManager::clearCache() {
    _cache.clear();
}

}  // namespace cc
