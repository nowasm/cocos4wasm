#include "cocos/asset/AssetManager.h"

#include <fstream>
#include "base/Log.h"
#include "platform/FileUtils.h"
#include "rapidjson/document.h"

namespace cc {

AssetManager &AssetManager::get() {
    static AssetManager instance;
    return instance;
}

AssetManager::~AssetManager() {
    shutdown();
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

void AssetManager::registerUuid(const ccstd::string &uuid, const ccstd::string &relPath) {
    if (uuid.empty() || relPath.empty()) return;
    _uuidToPath[uuid] = relPath;
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

// ─── Synchronous path ─────────────────────────────────────────────────────

IntrusivePtr<Asset> AssetManager::loadAsset(const ccstd::string &uuid) {
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto cached = _cache.find(uuid);
        if (cached != _cache.end()) return cached->second;
    }

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

    // Loader runs unlocked: it may create GFX resources and may re-enter
    // load() for dependencies.
    IntrusivePtr<Asset> asset = loader(absPath, uuid);
    if (!asset) {
        CC_LOG_ERROR("[AssetManager] loader returned null for %s (uuid %s)",
                     absPath.c_str(), uuid.c_str());
        return nullptr;
    }

    size_t cachedNow = 0;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        // Another (re-entrant / async-pumped) load may have won the race;
        // keep the first cached instance so all callers share one object.
        auto it = _cache.find(uuid);
        if (it != _cache.end()) {
            asset = it->second;
        } else if (_cacheEnabled) {
            _cache[uuid] = asset;
        }
        cachedNow = _cache.size();
    }
    CC_LOG_INFO("[AssetManager] loaded %s (uuid %s, cached=%zu)",
                relPath.c_str(), uuid.c_str(), cachedNow);
    return asset;
}

// ─── Async path ───────────────────────────────────────────────────────────
// Split of work per loader (all current loaders take a *path*, not bytes,
// so v1 keeps the loader body on the main thread for every asset type):
//   worker thread : file existence check + full raw byte read via
//                   std::ifstream (result discarded — warms the OS file
//                   cache; FileUtils is not documented thread-safe, so the
//                   worker avoids it entirely).
//   main thread   : the registered LoaderFn (Texture2D/.png .jpg .webp,
//                   Material/.mtl, Prefab/.prefab, EffectAsset/.effect,
//                   SpriteFrame/.json .sf, ...) runs inside update() —
//                   GFX resource creation, Node building and reflection
//                   never leave the main thread.

void AssetManager::loadAsync(const ccstd::string &uuid, AsyncCallback onDone) {
    if (!onDone) return;

    {
        std::lock_guard<std::mutex> lock(_mutex);

        // Cache hit → complete on the next update() pump, never inline.
        auto cached = _cache.find(uuid);
        if (cached != _cache.end()) {
            auto entry = std::make_shared<PendingLoad>();
            entry->uuid = uuid;
            entry->asset = cached->second;
            entry->callbacks.emplace_back(std::move(onDone));
            _ioDone.emplace_back(std::move(entry));
            return;
        }

        // In-flight dedup: piggy-back on the existing request.
        auto pending = _pending.find(uuid);
        if (pending != _pending.end()) {
            pending->second->callbacks.emplace_back(std::move(onDone));
            return;
        }
    }

    // Resolve uuid → path + loader (main-thread-only tables, no lock
    // needed). Failures still complete asynchronously via update().
    auto pathIt = _uuidToPath.find(uuid);
    const auto at = uuid.find('@');
    if (pathIt == _uuidToPath.end() && at != ccstd::string::npos) {
        pathIt = _uuidToPath.find(uuid.substr(0, at));
    }

    auto entry = std::make_shared<PendingLoad>();
    entry->uuid = uuid;
    entry->callbacks.emplace_back(std::move(onDone));

    if (pathIt == _uuidToPath.end()) {
        CC_LOG_ERROR("[AssetManager] loadAsync: unknown uuid: %s", uuid.c_str());
    } else {
        entry->loader = findLoader(pathIt->second);
        if (!entry->loader) {
            CC_LOG_ERROR("[AssetManager] loadAsync: no loader registered for '%s' (uuid %s)",
                         pathIt->second.c_str(), uuid.c_str());
        } else {
            entry->absPath = _assetRoot + pathIt->second;
            entry->needLoad = true;
        }
    }

    std::lock_guard<std::mutex> lock(_mutex);
    if (entry->needLoad) {
        _pending.emplace(uuid, entry);
        _ioQueue.emplace_back(std::move(entry));
        ensureWorkerLocked();
        _cv.notify_one();
    } else {
        // Unresolvable request: report failure on the next pump.
        _ioDone.emplace_back(std::move(entry));
    }
}

void AssetManager::ensureWorkerLocked() {
    if (_workerRunning) return;
    if (_worker.joinable()) _worker.join(); // stale thread from a prior shutdown()
    _workerQuit = false;
    _workerRunning = true;
    _worker = std::thread([this] { workerLoop(); });
}

void AssetManager::workerLoop() {
    std::unique_lock<std::mutex> lock(_mutex);
    while (!_workerQuit) {
        if (_ioQueue.empty()) {
            _cv.wait(lock, [this] { return _workerQuit || !_ioQueue.empty(); });
            continue;
        }
        PendingPtr job = _ioQueue.front();
        _ioQueue.erase(_ioQueue.begin());
        lock.unlock();

        // Filesystem work only — no engine calls off the main thread.
        // Reading the bytes (and discarding them) verifies readability and
        // warms the OS file cache so the main-thread loader's own read is
        // cheap.
        bool ok = false;
        {
            std::ifstream in(job->absPath.c_str(), std::ios::binary);
            if (in.good()) {
                char buf[64 * 1024];
                while (in.read(buf, sizeof(buf)) || in.gcount() > 0) {}
                ok = true;
            }
        }

        lock.lock();
        job->ioOk = ok;
        _ioDone.emplace_back(std::move(job));
    }
    _workerRunning = false;
}

void AssetManager::update() {
    ccstd::vector<PendingPtr> done;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_ioDone.empty()) return;
        done.swap(_ioDone);
    }

    for (auto &entry : done) {
        IntrusivePtr<Asset> result = entry->asset;

        if (entry->needLoad) {
            // A sync load may have populated the cache while I/O ran.
            {
                std::lock_guard<std::mutex> lock(_mutex);
                auto it = _cache.find(entry->uuid);
                if (it != _cache.end()) {
                    result = it->second;
                    entry->needLoad = false;
                }
            }

            if (entry->needLoad) {
                if (entry->ioOk) {
                    // Loader runs unlocked on the main thread; may re-enter
                    // load()/loadAsync().
                    result = entry->loader(entry->absPath, entry->uuid);
                    if (!result) {
                        CC_LOG_ERROR("[AssetManager] loadAsync: loader returned null for %s (uuid %s)",
                                     entry->absPath.c_str(), entry->uuid.c_str());
                    }
                } else {
                    CC_LOG_ERROR("[AssetManager] loadAsync: cannot read file %s (uuid %s)",
                                 entry->absPath.c_str(), entry->uuid.c_str());
                }
            }
        }

        // Publish + retire the pending entry atomically, then fire the
        // callbacks unlocked (they may schedule further loads).
        ccstd::vector<AsyncCallback> callbacks;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (result) {
                auto it = _cache.find(entry->uuid);
                if (it != _cache.end()) {
                    result = it->second;
                } else if (_cacheEnabled) {
                    _cache[entry->uuid] = result;
                }
            }
            _pending.erase(entry->uuid);
            callbacks.swap(entry->callbacks);
        }

        for (auto &cb : callbacks) {
            cb(result.get());
        }
    }
}

void AssetManager::shutdown() {
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _workerQuit = true;
    }
    _cv.notify_all();
    if (_worker.joinable()) _worker.join();
    std::lock_guard<std::mutex> lock(_mutex);
    _ioQueue.clear();
}

// ─── Cache lifetime ───────────────────────────────────────────────────────

size_t AssetManager::cachedCount() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _cache.size();
}

size_t AssetManager::evictUnused() {
    // Move the doomed refs out first and destroy them unlocked — an asset
    // destructor could conceivably call back into the manager.
    ccstd::vector<IntrusivePtr<Asset>> doomed;
    size_t kept = 0;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        for (auto it = _cache.begin(); it != _cache.end();) {
            // refcount == 1 → the cache holds the only reference.
            if (it->second && it->second->getRefCount() == 1) {
                doomed.emplace_back(std::move(it->second));
                it = _cache.erase(it);
            } else {
                ++kept;
                ++it;
            }
        }
    }
    CC_LOG_INFO("[AssetManager] evictUnused: evicted=%zu kept=%zu",
                doomed.size(), kept);
    return doomed.size();
}

void AssetManager::evict(const ccstd::string &uuid) {
    std::lock_guard<std::mutex> lock(_mutex);
    _cache.erase(uuid);
}

void AssetManager::clearCache() {
    // `doomed` is declared before the guard, so it destructs *after* the
    // guard releases the mutex — the cached refs die unlocked.
    ccstd::unordered_map<ccstd::string, IntrusivePtr<Asset>> doomed;
    std::lock_guard<std::mutex> lock(_mutex);
    doomed.swap(_cache);
}

}  // namespace cc
