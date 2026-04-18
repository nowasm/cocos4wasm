#pragma once

#include <functional>
#include "base/Ptr.h"
#include "base/std/container/string.h"
#include "base/std/container/unordered_map.h"
#include "core/assets/Asset.h"

namespace cc {

// Central registry for UUID-referenced assets. Scene / prefab / material
// JSONs contain `{"__uuid__":"..."}` values; the deserializer resolves
// them via AssetManager::load<T>(uuid) to fetch (or cache-hit) a typed
// IntrusivePtr<T>.
//
// MVP design:
//   - loadUuidMap() reads the uuid-map.json emitted by gen-uuid-map.js.
//   - load() picks a loader by file extension.
//   - Every loaded asset lands in the cache; subsequent load() calls for
//     the same UUID return the cached IntrusivePtr without re-reading.
//   - Synchronous only; async + worker-thread loading deferred.
class AssetManager {
public:
    static AssetManager &get();

    // Load uuid → path map. Call once at startup. Returns false on
    // parse/I/O failure (errors logged).
    bool loadUuidMap(const ccstd::string &uuidMapPath);

    // If the uuid resolver can't locate an asset by its map entry alone
    // (e.g. the map was built with paths relative to one root), the
    // base-dir supplied here is prepended to each entry before the
    // loader is invoked.
    void setAssetRoot(const ccstd::string &rootDir);
    const ccstd::string &getAssetRoot() const { return _assetRoot; }

    // Load an asset by UUID. Returns nullptr on failure.
    template <typename T>
    IntrusivePtr<T> load(const ccstd::string &uuid) {
        IntrusivePtr<Asset> a = loadAsset(uuid);
        return IntrusivePtr<T>(static_cast<T *>(a.get()));
    }

    IntrusivePtr<Asset> loadAsset(const ccstd::string &uuid);

    // Loader registry. A loader builds an Asset given the resolved file
    // path + its UUID. Register one per file extension (e.g. ".png",
    // ".mtl"). Extension matching is case-sensitive for now.
    using LoaderFn = std::function<IntrusivePtr<Asset>(const ccstd::string &absPath,
                                                        const ccstd::string &uuid)>;
    void registerLoader(const char *extension, LoaderFn fn);

    // Testing / diagnostic helpers.
    size_t getCachedCount() const { return _cache.size(); }
    size_t getUuidMapSize() const { return _uuidToPath.size(); }

    // Drop a cached entry; callers rarely need this — IntrusivePtr drop
    // does NOT evict (cache owns a strong ref to keep hot assets alive).
    void evict(const ccstd::string &uuid);

    // Nuke the whole cache. Used between demos / on shutdown.
    void clearCache();

private:
    AssetManager() = default;

    // Picks the loader by looking at the last '.' in `path`.
    LoaderFn findLoader(const ccstd::string &path) const;

    ccstd::string _assetRoot;
    ccstd::unordered_map<ccstd::string, ccstd::string>      _uuidToPath;
    ccstd::unordered_map<ccstd::string, IntrusivePtr<Asset>> _cache;
    ccstd::unordered_map<ccstd::string, LoaderFn>            _loaders;
};

}  // namespace cc
