// Copyright (c) 2025 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "randomx_wrapper.h"
#include "randomx/randomx.h"
#include "util/system.h"

#include <mutex>
#include <memory>
#include <map>
#include <chrono>
#include <thread>

// Multi-cache system to support concurrent access to multiple seeds
// This is essential for reindex and sync scenarios where background threads
// need to validate old blocks while the tip processes new blocks
struct CacheEntry {
    randomx_cache* cache;
    uint256 seedhash;
    uint64_t last_used;  // Timestamp for LRU cleanup

    CacheEntry() : cache(nullptr), last_used(0) {}
    ~CacheEntry() {
        if (cache) {
            randomx_release_cache(cache);
            cache = nullptr;
        }
    }
};

// Map of seed hash → cache entry
static std::map<uint256, std::shared_ptr<CacheEntry>> seed_caches;
static std::mutex cache_map_mutex;
static bool rx_shutting_down = false;

// Main seed for mining (set via RandomX_SetMainSeedHash)
static uint256 main_seed;
static bool main_seed_set = false;
static std::mutex main_seed_mutex;

// Thread-local VMs mapped by seed hash
struct ThreadLocalVM {
    std::map<uint256, randomx_vm*> vms;

    ~ThreadLocalVM() {
        for (auto& pair : vms) {
            if (pair.second) {
                randomx_destroy_vm(pair.second);
            }
        }
        vms.clear();
    }
};

thread_local ThreadLocalVM rxVM_thread;

// Calculate seed height for a given block height
uint64_t RandomX_SeedHeight(uint64_t height)
{
    if (height <= RANDOMX_SEEDHASH_EPOCH_BLOCKS + RANDOMX_SEEDHASH_EPOCH_LAG) {
        return 0;
    }
    return (height - RANDOMX_SEEDHASH_EPOCH_LAG - 1) & ~(RANDOMX_SEEDHASH_EPOCH_BLOCKS - 1);
}

// Get or create a cache for a specific seed
static std::shared_ptr<CacheEntry> GetOrCreateCache(const uint256& seedhash)
{
    std::lock_guard<std::mutex> lock(cache_map_mutex);

    // Check if cache already exists
    auto it = seed_caches.find(seedhash);
    if (it != seed_caches.end()) {
        it->second->last_used = GetTime();
        return it->second;
    }

    // Create new cache
    LogPrintf("RandomX: Creating new cache for seed %s\n", seedhash.GetHex().substr(0, 16));

    randomx_flags flags = randomx_get_flags();
    flags |= RANDOMX_FLAG_JIT;

    auto entry = std::make_shared<CacheEntry>();
    entry->cache = randomx_alloc_cache(flags);
    if (!entry->cache) {
        LogPrintf("RandomX: ERROR - Failed to allocate cache\n");
        return nullptr;
    }

    randomx_init_cache(entry->cache, seedhash.begin(), 32);
    entry->seedhash = seedhash;
    entry->last_used = GetTime();

    seed_caches[seedhash] = entry;

    // Cleanup old caches (keep most recent 5, which covers ±2 epochs around current)
    if (seed_caches.size() > 5) {
        // Find oldest cache
        auto oldest = seed_caches.begin();
        for (auto it = seed_caches.begin(); it != seed_caches.end(); ++it) {
            if (it->second->last_used < oldest->second->last_used) {
                oldest = it;
            }
        }
        LogPrintf("RandomX: Evicting old cache for seed %s\n", oldest->first.GetHex().substr(0, 16));
        seed_caches.erase(oldest);
    }

    return entry;
}

// Initialize RandomX
void RandomX_Init()
{
    LogPrintf("RandomX: Initializing multi-cache system\n");

    // Set genesis seed as default main seed
    uint256 genesisSeed;
    genesisSeed.SetNull();
    *genesisSeed.begin() = 0x08;

    {
        std::lock_guard<std::mutex> lock(main_seed_mutex);
        main_seed = genesisSeed;
        main_seed_set = true;
    }

    // Pre-create genesis cache
    GetOrCreateCache(genesisSeed);

    LogPrintf("RandomX: Initialization complete\n");
}

void RandomX_Shutdown()
{
    LogPrintf("RandomX: Starting shutdown...\n");

    rx_shutting_down = true;

    // Clean up thread-local VMs
    rxVM_thread.vms.clear();

    // Give other threads time to finish
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Clean up all caches
    {
        std::lock_guard<std::mutex> lock(cache_map_mutex);
        seed_caches.clear();
    }

    LogPrintf("RandomX: Shutdown complete\n");
}

// Set main seed hash (for mining - pre-caches the seed)
void RandomX_SetMainSeedHash(const void* seedhash, size_t size)
{
    if (!seedhash || size != 32) {
        LogPrintf("RandomX: ERROR - Invalid seedhash\n");
        return;
    }

    uint256 seed;
    memcpy(seed.begin(), seedhash, 32);

    // Update main seed
    {
        std::lock_guard<std::mutex> lock(main_seed_mutex);
        main_seed = seed;
        main_seed_set = true;
    }

    // Pre-cache this seed
    GetOrCreateCache(seed);

    LogPrintf("RandomX: Main seed set to %s\n", seed.GetHex().substr(0, 16));
}

// Hash with a specific seed
bool RandomX_Hash_WithSeed(const void* seedhash, size_t seedhashSize,
                           const void* input, size_t inputSize, void* output)
{
    if (!seedhash || seedhashSize != 32 || !input || !output) {
        return false;
    }

    if (rx_shutting_down) {
        return false;
    }

    uint256 seed;
    memcpy(seed.begin(), seedhash, 32);

    // Get or create cache for this seed
    auto cache_entry = GetOrCreateCache(seed);
    if (!cache_entry || !cache_entry->cache) {
        LogPrintf("RandomX: ERROR - Failed to get cache for seed\n");
        return false;
    }

    // Get or create thread-local VM for this seed
    auto vm_it = rxVM_thread.vms.find(seed);
    if (vm_it == rxVM_thread.vms.end()) {
        // Create new VM
        randomx_flags flags = randomx_get_flags();
        flags |= RANDOMX_FLAG_JIT;

        randomx_vm* vm = randomx_create_vm(flags, cache_entry->cache, nullptr);
        if (!vm) {
            LogPrintf("RandomX: ERROR - Failed to create VM\n");
            return false;
        }

        rxVM_thread.vms[seed] = vm;
        vm_it = rxVM_thread.vms.find(seed);
    }

    // Calculate hash
    randomx_calculate_hash(vm_it->second, input, inputSize, output);
    return true;
}

// Hash with current main seed
bool RandomX_Hash(const void* input, size_t inputSize, void* output)
{
    if (!input || !output) {
        return false;
    }

    if (rx_shutting_down) {
        return false;
    }

    // Get current main seed (and auto-initialize if needed)
    uint256 seed;
    {
        std::unique_lock<std::mutex> lock(main_seed_mutex);

        // Auto-initialize if needed (check inside lock to avoid race)
        if (!main_seed_set) {
            // Release lock before calling Init() to avoid deadlock
            lock.unlock();
            RandomX_Init();
            lock.lock();
        }

        seed = main_seed;
    }

    return RandomX_Hash_WithSeed(seed.begin(), 32, input, inputSize, output);
}

bool RandomX_Hash_Block(const void* input, size_t inputSize, uint256& hash)
{
    return RandomX_Hash(input, inputSize, hash.begin());
}

bool RandomX_Verify_WithSeed(const void* seedhash, size_t seedhashSize,
                             const void* input, size_t inputSize, const uint256& expectedHash)
{
    uint256 computedHash;
    if (!RandomX_Hash_WithSeed(seedhash, seedhashSize, input, inputSize, computedHash.begin())) {
        return false;
    }

    return computedHash == expectedHash;
}

bool RandomX_Verify(const void* input, size_t inputSize, const uint256& expectedHash)
{
    uint256 computedHash;
    if (!RandomX_Hash_Block(input, inputSize, computedHash)) {
        return false;
    }

    return computedHash == expectedHash;
}
