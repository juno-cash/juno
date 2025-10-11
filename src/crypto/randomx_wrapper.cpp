// Copyright (c) 2025 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "randomx_wrapper.h"
#include "randomx/randomx.h"
#include "util/system.h"

#include <mutex>
#include <memory>
#include <chrono>
#include <thread>

// Main cache (for current epoch)
static randomx_cache* main_cache = nullptr;
static std::mutex main_cache_mutex;
static unsigned char main_seedhash[32];
static bool main_seedhash_set = false;

// Secondary cache (for previous/next epoch during transitions)
static randomx_cache* secondary_cache = nullptr;
static std::mutex secondary_cache_mutex;
static unsigned char secondary_seedhash[32];
static bool secondary_seedhash_set = false;

static bool rx_shutting_down = false;  // Flag to prevent new VM creation during shutdown

// Thread-local VM wrapper with automatic cleanup
struct ThreadLocalVM {
    randomx_vm* main_vm = nullptr;
    randomx_vm* secondary_vm = nullptr;

    ~ThreadLocalVM() {
        if (main_vm) {
            randomx_destroy_vm(main_vm);
            main_vm = nullptr;
        }
        if (secondary_vm) {
            randomx_destroy_vm(secondary_vm);
            secondary_vm = nullptr;
        }
    }
};

// Thread-local RandomX VM instances
// Each thread gets its own VMs to avoid race conditions
thread_local ThreadLocalVM rxVM_thread;

// Helper: Check if seedhash matches main cache
static bool is_main(const unsigned char* seedhash) {
    return main_seedhash_set && (memcmp(seedhash, main_seedhash, 32) == 0);
}

// Helper: Check if seedhash matches secondary cache
static bool is_secondary(const unsigned char* seedhash) {
    return secondary_seedhash_set && (memcmp(seedhash, secondary_seedhash, 32) == 0);
}

// Calculate seed height for a given block height
uint64_t RandomX_SeedHeight(uint64_t height)
{
    if (height <= RANDOMX_SEEDHASH_EPOCH_BLOCKS + RANDOMX_SEEDHASH_EPOCH_LAG) {
        return 0;
    }
    // Bitmask operation works efficiently since RANDOMX_SEEDHASH_EPOCH_BLOCKS (2048) is a power of 2
    // This rounds down to the nearest multiple of epochBlocks
    return (height - RANDOMX_SEEDHASH_EPOCH_LAG - 1) & ~(RANDOMX_SEEDHASH_EPOCH_BLOCKS - 1);
}

// Initialize shared cache (call once at startup)
void RandomX_Init()
{
    std::lock_guard<std::mutex> lock(main_cache_mutex);

    if (main_seedhash_set) {
        return;
    }

    LogPrintf("RandomX: Initializing with genesis seed (0x08...)...\n");

    // Get recommended flags
    randomx_flags flags = randomx_get_flags();

    // For mining, we want JIT compilation if available
    flags |= RANDOMX_FLAG_JIT;

    // For production, consider using RANDOMX_FLAG_FULL_MEM for better performance
    // but it requires 2GB+ of memory. For now, use light mode.

    // Create main cache
    main_cache = randomx_alloc_cache(flags);
    if (!main_cache) {
        LogPrintf("RandomX: ERROR - Failed to allocate main cache\n");
        return;
    }

    // Initialize with genesis seed (0x08 followed by zeros, matching epoch 0)
    // This makes it obvious we're using a real seed value, not just null/zero
    unsigned char genesis_seed[32];
    memset(genesis_seed, 0, 32);
    genesis_seed[0] = 0x08;  // First byte is 0x08 to distinguish from null
    randomx_init_cache(main_cache, genesis_seed, 32);
    memcpy(main_seedhash, genesis_seed, 32);
    main_seedhash_set = true;

    LogPrintf("RandomX: Main cache initialized with genesis seed (0x08...)\n");
}

void RandomX_Shutdown()
{
    LogPrintf("RandomX: Starting shutdown...\n");

    // Set shutdown flag to prevent new VMs from being created
    rx_shutting_down = true;

    // Clean up thread-local VMs for this thread
    // Other threads will clean up their VMs via thread_local destructor
    if (rxVM_thread.main_vm) {
        randomx_destroy_vm(rxVM_thread.main_vm);
        rxVM_thread.main_vm = nullptr;
    }
    if (rxVM_thread.secondary_vm) {
        randomx_destroy_vm(rxVM_thread.secondary_vm);
        rxVM_thread.secondary_vm = nullptr;
    }

    // Give other threads time to finish their hash calculations
    // This is a bit crude but prevents race conditions during shutdown
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Clean up shared caches
    {
        std::lock_guard<std::mutex> lock1(main_cache_mutex);
        if (main_cache) {
            randomx_release_cache(main_cache);
            main_cache = nullptr;
        }
        main_seedhash_set = false;
    }

    {
        std::lock_guard<std::mutex> lock2(secondary_cache_mutex);
        if (secondary_cache) {
            randomx_release_cache(secondary_cache);
            secondary_cache = nullptr;
        }
        secondary_seedhash_set = false;
    }

    LogPrintf("RandomX: Shut down complete\n");
}

// Set main seed hash for mining
void RandomX_SetMainSeedHash(const void* seedhash, size_t size)
{
    if (!seedhash || size != 32) {
        LogPrintf("RandomX: ERROR - Invalid seedhash in SetMainSeedHash\n");
        return;
    }

    const unsigned char* hash = static_cast<const unsigned char*>(seedhash);

    // Early exit if seedhash didn't change
    if (is_main(hash)) {
        return;
    }

    std::lock_guard<std::mutex> lock(main_cache_mutex);

    // Double check after acquiring lock
    if (is_main(hash)) {
        return;
    }

    LogPrintf("RandomX: Setting new main seed hash\n");

    randomx_flags flags = randomx_get_flags();
    flags |= RANDOMX_FLAG_JIT;

    // Allocate cache if needed
    if (!main_cache) {
        main_cache = randomx_alloc_cache(flags);
        if (!main_cache) {
            LogPrintf("RandomX: ERROR - Failed to allocate main cache\n");
            return;
        }
    }

    // Initialize cache with new seed
    randomx_init_cache(main_cache, seedhash, size);
    memcpy(main_seedhash, seedhash, 32);
    main_seedhash_set = true;

    LogPrintf("RandomX: Main cache updated with new seed\n");
}

// Hash with a specific seed (for verification)
bool RandomX_Hash_WithSeed(const void* seedhash, size_t seedhashSize,
                           const void* input, size_t inputSize, void* output)
{
    if (!seedhash || seedhashSize != 32 || !input || !output) {
        return false;
    }

    if (rx_shutting_down) {
        LogPrintf("RandomX: Hash calculation skipped - shutting down\n");
        return false;
    }

    const unsigned char* hash = static_cast<const unsigned char*>(seedhash);
    randomx_flags flags = randomx_get_flags();
    flags |= RANDOMX_FLAG_JIT;

    // Try main cache first (fast path)
    if (is_main(hash)) {
        std::lock_guard<std::mutex> lock(main_cache_mutex);
        if (is_main(hash) && main_cache) {
            // Create or reuse thread-local VM
            if (!rxVM_thread.main_vm) {
                rxVM_thread.main_vm = randomx_create_vm(flags, main_cache, nullptr);
                if (!rxVM_thread.main_vm) {
                    LogPrintf("RandomX: ERROR - Failed to create main VM\n");
                    return false;
                }
            }
            randomx_calculate_hash(rxVM_thread.main_vm, input, inputSize, output);
            return true;
        }
    }

    // Try secondary cache (for epoch transitions)
    if (is_secondary(hash)) {
        std::lock_guard<std::mutex> lock(secondary_cache_mutex);
        if (is_secondary(hash) && secondary_cache) {
            if (!rxVM_thread.secondary_vm) {
                rxVM_thread.secondary_vm = randomx_create_vm(flags, secondary_cache, nullptr);
                if (!rxVM_thread.secondary_vm) {
                    LogPrintf("RandomX: ERROR - Failed to create secondary VM\n");
                    return false;
                }
            }
            randomx_calculate_hash(rxVM_thread.secondary_vm, input, inputSize, output);
            return true;
        }
    }

    // Slow path: use or create secondary cache
    {
        std::lock_guard<std::mutex> lock(secondary_cache_mutex);

        // Initialize secondary cache if needed
        if (!is_secondary(hash)) {
            LogPrintf("RandomX: Updating secondary cache with new seed\n");

            if (!secondary_cache) {
                secondary_cache = randomx_alloc_cache(flags);
                if (!secondary_cache) {
                    LogPrintf("RandomX: ERROR - Failed to allocate secondary cache\n");
                    return false;
                }
            }

            randomx_init_cache(secondary_cache, seedhash, seedhashSize);
            memcpy(secondary_seedhash, seedhash, 32);
            secondary_seedhash_set = true;
        }

        // Create VM if needed
        if (rxVM_thread.secondary_vm) {
            randomx_destroy_vm(rxVM_thread.secondary_vm);
        }
        rxVM_thread.secondary_vm = randomx_create_vm(flags, secondary_cache, nullptr);
        if (!rxVM_thread.secondary_vm) {
            LogPrintf("RandomX: ERROR - Failed to create secondary VM\n");
            return false;
        }

        randomx_calculate_hash(rxVM_thread.secondary_vm, input, inputSize, output);
        return true;
    }
}

bool RandomX_Hash(const void* input, size_t inputSize, void* output)
{
    if (!input || !output) {
        return false;
    }

    // Don't create new VMs during shutdown
    if (rx_shutting_down) {
        LogPrintf("RandomX: Hash calculation skipped - shutting down\n");
        return false;
    }

    // Auto-initialize if needed
    if (!main_seedhash_set) {
        RandomX_Init();
    }

    // Use main cache
    std::lock_guard<std::mutex> lock(main_cache_mutex);

    if (!main_cache) {
        LogPrintf("RandomX: ERROR - Main cache not initialized\n");
        return false;
    }

    // Create thread-local VM if it doesn't exist for this thread
    if (!rxVM_thread.main_vm) {
        randomx_flags flags = randomx_get_flags();
        flags |= RANDOMX_FLAG_JIT;

        rxVM_thread.main_vm = randomx_create_vm(flags, main_cache, nullptr);
        if (!rxVM_thread.main_vm) {
            LogPrintf("RandomX: ERROR - Failed to create thread-local VM\n");
            return false;
        }
    }

    // Calculate hash using thread-local VM
    randomx_calculate_hash(rxVM_thread.main_vm, input, inputSize, output);

    return true;
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
