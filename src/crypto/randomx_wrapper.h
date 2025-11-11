// Copyright (c) 2025 Juno Cash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CRYPTO_RANDOMX_WRAPPER_H
#define BITCOIN_CRYPTO_RANDOMX_WRAPPER_H

#include "uint256.h"
#include <vector>
#include <cstddef>

/**
 * RandomX wrapper for Juno Cash
 *
 * RandomX is a proof-of-work algorithm optimized for general-purpose CPUs.
 * This wrapper provides a simple interface for using RandomX in Juno Cash.
 */

// RandomX epoch configuration
static const uint64_t RANDOMX_SEEDHASH_EPOCH_BLOCKS = 2048;  // Power of 2 for efficient bitmask operations
static const uint64_t RANDOMX_SEEDHASH_EPOCH_LAG = 96;

/**
 * Calculate the seed height for a given block height.
 * The seed changes every 2048 blocks with a 96-block lag.
 * First epoch transition occurs at block 2144 (2048 + 96).
 *
 * @param height Block height
 * @return Seed height to use for RandomX key
 */
uint64_t RandomX_SeedHeight(uint64_t height);

/**
 * Set the main seed hash for RandomX.
 * This should be called when mining or when the epoch changes.
 *
 * @param seedhash Pointer to seed hash (32 bytes, typically a block hash)
 * @param size Size of seed hash
 */
void RandomX_SetMainSeedHash(const void* seedhash, size_t size);

/**
 * Calculate a RandomX hash with a specific seed.
 *
 * @param seedhash Pointer to seed hash (32 bytes)
 * @param seedhashSize Size of seed hash
 * @param input Pointer to input data
 * @param inputSize Size of input data in bytes
 * @param output Pointer to output buffer (must be at least 32 bytes)
 * @return true if successful, false otherwise
 */
bool RandomX_Hash_WithSeed(const void* seedhash, size_t seedhashSize,
                           const void* input, size_t inputSize, void* output);

/**
 * Calculate a RandomX hash (uses current main seed).
 *
 * @param input Pointer to input data
 * @param inputSize Size of input data in bytes
 * @param output Pointer to output buffer (must be at least 32 bytes)
 * @return true if successful, false otherwise
 */
bool RandomX_Hash(const void* input, size_t inputSize, void* output);

/**
 * Calculate a RandomX hash from a block header (uses current main seed).
 * This is optimized for mining use case.
 *
 * @param input Pointer to input data
 * @param inputSize Size of input data in bytes
 * @param hash Output hash (32 bytes)
 * @return true if successful, false otherwise
 */
bool RandomX_Hash_Block(const void* input, size_t inputSize, uint256& hash);

/**
 * Verify a RandomX proof of work with a specific seed.
 *
 * @param seedhash Pointer to seed hash (32 bytes)
 * @param seedhashSize Size of seed hash
 * @param input Pointer to input data
 * @param inputSize Size of input data in bytes
 * @param expectedHash The expected hash value
 * @return true if the hash matches, false otherwise
 */
bool RandomX_Verify_WithSeed(const void* seedhash, size_t seedhashSize,
                             const void* input, size_t inputSize, const uint256& expectedHash);

/**
 * Verify a RandomX proof of work (uses current main seed).
 *
 * @param input Pointer to input data
 * @param inputSize Size of input data in bytes
 * @param expectedHash The expected hash value
 * @return true if the hash matches, false otherwise
 */
bool RandomX_Verify(const void* input, size_t inputSize, const uint256& expectedHash);

/**
 * Initialize RandomX (call once at startup).
 * This prepares the RandomX cache and VM.
 */
void RandomX_Init();

/**
 * Cleanup RandomX (call at shutdown).
 */
void RandomX_Shutdown();

#endif // BITCOIN_CRYPTO_RANDOMX_WRAPPER_H
