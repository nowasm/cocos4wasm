#include <atomic>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <thread>

#include "base/Ptr.h"
#include "cocos/asset/AssetManager.h"
#include "core/assets/Asset.h"
#include "gtest/gtest.h"

using namespace cc;

// ─── Fake asset + loader ──────────────────────────────────────────────────
// The unit-test tree builds the engine without a GFX device, so these tests
// use a trivial Asset subclass loaded from `.blob` temp files instead of
// real textures/materials. AssetManager never inspects the concrete type,
// so no reflection registration is needed.

namespace {

class BlobAsset : public Asset {
public:
    ccstd::string payload;
};

std::atomic<int> gLoaderRuns{0};

constexpr int kDeadlineMs = 5000;

// Writes <name> under the test's working directory and returns the relative
// path used in the uuid map (asset root is left empty).
ccstd::string writeBlobFile(const ccstd::string &name, const ccstd::string &content) {
    std::ofstream out(name.c_str(), std::ios::binary | std::ios::trunc);
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    return name;
}

void setUpManager() {
    auto &am = AssetManager::get();
    am.setAssetRoot("");
    am.setCacheEnabled(true);
    am.clearCache();
    am.registerLoader(".blob", [](const ccstd::string &absPath, const ccstd::string &uuid) {
        gLoaderRuns.fetch_add(1);
        std::ifstream in(absPath.c_str(), std::ios::binary);
        if (!in.good()) return IntrusivePtr<Asset>{};
        ccstd::string content((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
        auto *blob = ccnew BlobAsset();
        blob->payload = content;
        blob->setUuid(uuid);
        return IntrusivePtr<Asset>(blob);
    });
    gLoaderRuns.store(0);
}

// Pumps AssetManager::update() until `pred` is true or the deadline hits.
template <typename Pred>
bool pumpUntil(Pred pred, int deadlineMs = kDeadlineMs) {
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(deadlineMs);
    for (;;) {
        AssetManager::get().update();
        if (pred()) return true;
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

} // namespace

// ─── Tests ────────────────────────────────────────────────────────────────

TEST(AssetManagerAsync, cacheMissCompletesViaUpdate) {
    setUpManager();
    auto &am = AssetManager::get();
    const ccstd::string uuid = "async-miss-01";
    am.registerUuid(uuid, writeBlobFile("am_miss_01.blob", "hello"));

    bool done = false;
    Asset *got = nullptr;
    am.loadAsync(uuid, [&](Asset *a) { done = true; got = a; });

    // Callback must not fire before update(), even after the worker had
    // ample time to finish the I/O.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(done);

    ASSERT_TRUE(pumpUntil([&] { return done; }));
    ASSERT_NE(got, nullptr);
    EXPECT_EQ(static_cast<BlobAsset *>(got)->payload, "hello");
    EXPECT_EQ(gLoaderRuns.load(), 1);
    EXPECT_EQ(am.cachedCount(), 1U);
}

TEST(AssetManagerAsync, cacheHitCompletesViaUpdateSamePointer) {
    setUpManager();
    auto &am = AssetManager::get();
    const ccstd::string uuid = "async-hit-01";
    am.registerUuid(uuid, writeBlobFile("am_hit_01.blob", "cached"));

    Asset *first = nullptr;
    am.loadAsync(uuid, [&](Asset *a) { first = a; });
    ASSERT_TRUE(pumpUntil([&] { return first != nullptr; }));

    // Second request hits the cache — still completes via update(), not
    // inline, and yields the same instance.
    bool done = false;
    Asset *second = nullptr;
    am.loadAsync(uuid, [&](Asset *a) { done = true; second = a; });
    EXPECT_FALSE(done); // not inline
    ASSERT_TRUE(pumpUntil([&] { return done; }));
    EXPECT_EQ(second, first);
    EXPECT_EQ(gLoaderRuns.load(), 1); // loader did not run again
}

TEST(AssetManagerAsync, inFlightRequestsAreDeduplicated) {
    setUpManager();
    auto &am = AssetManager::get();
    const ccstd::string uuid = "async-dedup-01";
    am.registerUuid(uuid, writeBlobFile("am_dedup_01.blob", "once"));

    Asset *a1 = nullptr;
    Asset *a2 = nullptr;
    am.loadAsync(uuid, [&](Asset *a) { a1 = a; });
    am.loadAsync(uuid, [&](Asset *a) { a2 = a; });

    ASSERT_TRUE(pumpUntil([&] { return a1 != nullptr && a2 != nullptr; }));
    EXPECT_EQ(a1, a2);
    EXPECT_EQ(gLoaderRuns.load(), 1);
}

TEST(AssetManagerAsync, unknownUuidCallsBackWithNull) {
    setUpManager();
    auto &am = AssetManager::get();

    bool done = false;
    Asset *got = reinterpret_cast<Asset *>(0x1); // sentinel
    am.loadAsync("no-such-uuid-xyz", [&](Asset *a) { done = true; got = a; });
    EXPECT_FALSE(done); // failure is not reported inline either

    ASSERT_TRUE(pumpUntil([&] { return done; }));
    EXPECT_EQ(got, nullptr);
    EXPECT_EQ(gLoaderRuns.load(), 0);
}

TEST(AssetManagerAsync, evictUnusedDropsCacheOnlyEntries) {
    setUpManager();
    auto &am = AssetManager::get();
    const ccstd::string uuid = "evict-01";
    am.registerUuid(uuid, writeBlobFile("am_evict_01.blob", "v1"));

    {
        IntrusivePtr<Asset> held = am.loadAsset(uuid);
        ASSERT_TRUE(held);
        EXPECT_EQ(am.cachedCount(), 1U);
    } // caller ref dropped → cache is now the sole owner

    EXPECT_EQ(am.evictUnused(), 1U);
    EXPECT_EQ(am.cachedCount(), 0U);

    // Loading again re-reads from disk (loader runs a second time).
    const int runsBefore = gLoaderRuns.load();
    IntrusivePtr<Asset> again = am.loadAsset(uuid);
    ASSERT_TRUE(again);
    EXPECT_EQ(gLoaderRuns.load(), runsBefore + 1);
}

TEST(AssetManagerAsync, heldRefSurvivesEvictUnused) {
    setUpManager();
    auto &am = AssetManager::get();
    const ccstd::string uuid = "evict-held-01";
    am.registerUuid(uuid, writeBlobFile("am_evict_held_01.blob", "keepme"));

    IntrusivePtr<Asset> held = am.loadAsset(uuid);
    ASSERT_TRUE(held);
    EXPECT_EQ(held->getRefCount(), 2U); // caller + cache

    // evictUnused() must NOT drop an entry someone else still references.
    EXPECT_EQ(am.evictUnused(), 0U);
    EXPECT_EQ(am.cachedCount(), 1U);

    // Unconditional evict() drops the *cache's* reference only — it never
    // invalidates outstanding IntrusivePtrs; the held asset stays valid.
    am.evict(uuid);
    EXPECT_EQ(am.cachedCount(), 0U);
    EXPECT_EQ(held->getRefCount(), 1U);
    EXPECT_EQ(static_cast<BlobAsset *>(held.get())->payload, "keepme");
}

TEST(AssetManagerAsync, concurrencySmoke20RequestsOver5Uuids) {
    setUpManager();
    auto &am = AssetManager::get();

    constexpr int kUuids = 5;
    constexpr int kRequests = 20;
    ccstd::vector<ccstd::string> uuids;
    for (int i = 0; i < kUuids; ++i) {
        ccstd::string uuid = "smoke-" + std::to_string(i);
        char fname[64];
        std::snprintf(fname, sizeof(fname), "am_smoke_%02d.blob", i);
        am.registerUuid(uuid, writeBlobFile(fname, "payload-" + std::to_string(i)));
        uuids.emplace_back(std::move(uuid));
    }

    std::atomic<int> succeeded{0};
    std::atomic<int> completed{0};
    for (int i = 0; i < kRequests; ++i) {
        const int idx = i % kUuids;
        am.loadAsync(uuids[idx], [&, idx](Asset *a) {
            completed.fetch_add(1);
            if (a && static_cast<BlobAsset *>(a)->payload ==
                         "payload-" + std::to_string(idx)) {
                succeeded.fetch_add(1);
            }
        });
    }

    ASSERT_TRUE(pumpUntil([&] { return completed.load() == kRequests; }));
    EXPECT_EQ(succeeded.load(), kRequests);
    EXPECT_EQ(gLoaderRuns.load(), kUuids); // dedup: one loader run per uuid
    EXPECT_EQ(am.cachedCount(), static_cast<size_t>(kUuids));
}
