#pragma once

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include "base/Ptr.h"
#include "base/std/container/string.h"
#include "base/std/container/unordered_map.h"
#include "base/std/container/vector.h"
#include "core/assets/Asset.h"

namespace cc {

// Central registry for UUID-referenced assets. Scene / prefab / material
// JSONs contain `{"__uuid__":"..."}` values; the deserializer resolves
// them via AssetManager::load<T>(uuid) to fetch (or cache-hit) a typed
// IntrusivePtr<T>.
//
// Design:
//   - loadUuidMap() reads the uuid-map.json emitted by gen-uuid-map.js.
//   - load() picks a loader by file extension (synchronous path).
//   - loadAsync() queues a request on a lazily-started background worker.
//     The worker only touches the filesystem (existence check + raw byte
//     prefetch to warm the OS cache); the registered loader itself — which
//     may create GFX resources, Nodes or reflected objects — always runs
//     on the main thread inside update(). Callbacks therefore always fire
//     on the main thread, during update(), never inline (consistent
//     reentrancy even on cache hits). Concurrent requests for the same
//     in-flight uuid are deduplicated: the loader runs once, every
//     callback fires.
//   - update() must be pumped once per frame from the main loop.
//   - Every loaded asset lands in a strong-ref cache; subsequent loads for
//     the same UUID return the cached IntrusivePtr without re-reading.
//     evictUnused() drops entries the cache alone keeps alive; evict()/
//     clearCache() drop unconditionally (outstanding IntrusivePtrs stay
//     valid — eviction only releases the cache's own reference).
//   - Threading: one std::mutex guards the cache, the pending-load table
//     and the worker queues. Loaders and callbacks are always invoked with
//     the mutex released, so they may re-enter load()/loadAsync().
//     uuid-map and loader registration are main-thread-only setup.
class AssetManager {
public:
    static AssetManager &get();
    ~AssetManager();

    // Load uuid → path map. Call once at startup. Returns false on
    // parse/I/O failure (errors logged).
    bool loadUuidMap(const ccstd::string &uuidMapPath);

    // Add / override a single uuid → relative-path entry (tests, dynamic
    // content). Main-thread only, like loadUuidMap().
    void registerUuid(const ccstd::string &uuid, const ccstd::string &relPath);

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

    // ── Async loading ─────────────────────────────────────────────────
    // Queue an asynchronous load. `onDone` receives the asset (nullptr on
    // failure) on the main thread during a later update() call — never
    // inline, even on a cache hit.
    using AsyncCallback = std::function<void(Asset *)>;
    void loadAsync(const ccstd::string &uuid, AsyncCallback onDone);

    // Typed convenience wrapper; the callback gets a T* (nullptr on
    // failure). No type verification is performed (same as load<T>).
    template <typename T>
    void loadAsync(const ccstd::string &uuid, std::function<void(T *)> onDone) {
        loadAsync(uuid, AsyncCallback([cb = std::move(onDone)](Asset *a) {
                      cb(static_cast<T *>(a));
                  }));
    }

    // Main-thread pump: runs loaders for I/O-complete requests and fires
    // their callbacks. Call once per frame from the game loop.
    void update();

    // Stops and joins the worker thread; drops queued (not-yet-completed)
    // requests without firing their callbacks. Safe to call repeatedly;
    // also invoked from the destructor. loadAsync() after shutdown()
    // restarts the worker lazily.
    void shutdown();

    // Loader registry. A loader builds an Asset given the resolved file
    // path + its UUID. Register one per file extension (e.g. ".png",
    // ".mtl"). Extension matching is case-sensitive for now.
    using LoaderFn = std::function<IntrusivePtr<Asset>(const ccstd::string &absPath,
                                                        const ccstd::string &uuid)>;
    void registerLoader(const char *extension, LoaderFn fn);

    // ── Cache lifetime ────────────────────────────────────────────────
    // Drop every cache entry whose only remaining owner is the cache
    // itself (refcount == 1). Assets still referenced elsewhere are kept.
    // Returns the number of entries evicted.
    size_t evictUnused();

    // When disabled, freshly loaded assets are not retained in the cache
    // (existing entries are untouched — evict/clear them explicitly).
    void setCacheEnabled(bool enabled) { _cacheEnabled = enabled; }
    bool isCacheEnabled() const { return _cacheEnabled; }

    // Testing / diagnostic helpers.
    size_t cachedCount() const;
    size_t getCachedCount() const { return cachedCount(); }
    size_t getUuidMapSize() const { return _uuidToPath.size(); }

    // Drop a cached entry; callers rarely need this — IntrusivePtr drop
    // does NOT evict (cache owns a strong ref to keep hot assets alive).
    void evict(const ccstd::string &uuid);

    // Nuke the whole cache. Used between demos / on shutdown.
    void clearCache();

private:
    AssetManager() = default;

    // One queued async load. Shared between the pending table (keyed by
    // uuid, for dedup), the worker queue and the done queue. `callbacks`
    // and the io* flags are guarded by _mutex; the path/loader fields are
    // immutable after construction.
    struct PendingLoad {
        ccstd::string uuid;
        ccstd::string absPath;
        LoaderFn loader;                       // main-thread loader to run in update()
        ccstd::vector<AsyncCallback> callbacks;
        IntrusivePtr<Asset> asset;             // pre-resolved (cache hit) or failure (null)
        bool needLoad{false};                  // true → update() must run `loader`
        bool ioOk{false};                      // worker: file existed & bytes readable
    };
    using PendingPtr = std::shared_ptr<PendingLoad>;

    // Picks the loader by looking at the last '.' in `path`.
    LoaderFn findLoader(const ccstd::string &path) const;

    // Starts the worker thread if it isn't running. Caller holds _mutex.
    void ensureWorkerLocked();
    void workerLoop();

    ccstd::string _assetRoot;
    ccstd::unordered_map<ccstd::string, ccstd::string>       _uuidToPath;
    ccstd::unordered_map<ccstd::string, IntrusivePtr<Asset>> _cache;
    ccstd::unordered_map<ccstd::string, LoaderFn>            _loaders;
    bool _cacheEnabled{true};

    // Async machinery. _mutex guards _cache, _pending, _ioQueue, _ioDone
    // and _workerQuit; _cv wakes the worker.
    mutable std::mutex _mutex;
    std::condition_variable _cv;
    std::thread _worker;
    bool _workerRunning{false};
    bool _workerQuit{false};
    ccstd::unordered_map<ccstd::string, PendingPtr> _pending; // in-flight, keyed by uuid
    ccstd::vector<PendingPtr> _ioQueue;                       // waiting for the worker
    ccstd::vector<PendingPtr> _ioDone;                        // drained by update()
};

}  // namespace cc
