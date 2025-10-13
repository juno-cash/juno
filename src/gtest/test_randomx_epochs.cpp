#include <gtest/gtest.h>

#include "crypto/randomx_wrapper.h"
#include "uint256.h"

// Test RandomX seed height calculation at various block heights
// Updated for 1536-block epochs with 96-block lag (first transition at 1632)
TEST(RandomXEpochs, SeedHeightCalculation) {
    // Epoch 0: blocks 0 - 1631 should use seed height 0
    EXPECT_EQ(RandomX_SeedHeight(0), 0);
    EXPECT_EQ(RandomX_SeedHeight(1), 0);
    EXPECT_EQ(RandomX_SeedHeight(100), 0);
    EXPECT_EQ(RandomX_SeedHeight(1000), 0);
    EXPECT_EQ(RandomX_SeedHeight(1535), 0);
    EXPECT_EQ(RandomX_SeedHeight(1536), 0);  // Seed block itself
    EXPECT_EQ(RandomX_SeedHeight(1600), 0);
    EXPECT_EQ(RandomX_SeedHeight(1631), 0);  // Boundary: still epoch 0

    // Epoch 1: blocks 1632 - 3167 should use seed height 1536
    EXPECT_EQ(RandomX_SeedHeight(1632), 1536);
    EXPECT_EQ(RandomX_SeedHeight(1633), 1536);
    EXPECT_EQ(RandomX_SeedHeight(2000), 1536);
    EXPECT_EQ(RandomX_SeedHeight(3167), 1536);

    // Epoch 2: blocks 3168 - 4703 should use seed height 3072
    EXPECT_EQ(RandomX_SeedHeight(3168), 3072);
    EXPECT_EQ(RandomX_SeedHeight(3169), 3072);
    EXPECT_EQ(RandomX_SeedHeight(4000), 3072);
    EXPECT_EQ(RandomX_SeedHeight(4703), 3072);

    // Epoch 3: blocks 4704 - 6239 should use seed height 4608
    EXPECT_EQ(RandomX_SeedHeight(4704), 4608);
    EXPECT_EQ(RandomX_SeedHeight(4705), 4608);
    EXPECT_EQ(RandomX_SeedHeight(5000), 4608);
    EXPECT_EQ(RandomX_SeedHeight(6239), 4608);

    // Epoch 4: blocks 6240+ should use seed height 6144
    EXPECT_EQ(RandomX_SeedHeight(6240), 6144);
    EXPECT_EQ(RandomX_SeedHeight(6241), 6144);
    EXPECT_EQ(RandomX_SeedHeight(10000), 9216);
}

// Test that epoch boundaries align with the expected formula
TEST(RandomXEpochs, EpochBoundaries) {
    const uint64_t EPOCH_BLOCKS = 1536;
    const uint64_t LAG = 96;

    // Test the formula: seed_height = (height - LAG - 1) & ~(EPOCH_BLOCKS - 1)
    // for heights where it should not be 0

    // First transition at block 1632
    uint64_t height = 1632;
    uint64_t expected = (height - LAG - 1) & ~(EPOCH_BLOCKS - 1);
    EXPECT_EQ(expected, 1536);
    EXPECT_EQ(RandomX_SeedHeight(height), expected);

    // Second transition at block 3168
    height = 3168;
    expected = (height - LAG - 1) & ~(EPOCH_BLOCKS - 1);
    EXPECT_EQ(expected, 3072);
    EXPECT_EQ(RandomX_SeedHeight(height), expected);

    // Third transition at block 4704
    height = 4704;
    expected = (height - LAG - 1) & ~(EPOCH_BLOCKS - 1);
    EXPECT_EQ(expected, 4608);
    EXPECT_EQ(RandomX_SeedHeight(height), expected);
}

// Test the 96-block lag is correctly applied
TEST(RandomXEpochs, LagPeriod) {
    // Block 1536 is mined (epoch boundary block)
    // Blocks 1536-1631 still use old seed (96 blocks of lag)
    // Block 1631 is the last block with old seed
    // Block 1632 is the first block with new seed from block 1536

    EXPECT_EQ(RandomX_SeedHeight(1536), 0);  // Seed block itself uses old seed
    EXPECT_EQ(RandomX_SeedHeight(1537), 0);
    EXPECT_EQ(RandomX_SeedHeight(1600), 0);
    EXPECT_EQ(RandomX_SeedHeight(1631), 0);  // Last block with genesis seed
    EXPECT_EQ(RandomX_SeedHeight(1632), 1536);  // First block with block 1536 seed

    // Same pattern for next epoch
    EXPECT_EQ(RandomX_SeedHeight(3072), 1536);  // Seed block uses current seed
    EXPECT_EQ(RandomX_SeedHeight(3100), 1536);
    EXPECT_EQ(RandomX_SeedHeight(3167), 1536);  // Last block with old seed
    EXPECT_EQ(RandomX_SeedHeight(3168), 3072);  // First block with block 3072 seed
}

// Test seed height calculation for large block heights
TEST(RandomXEpochs, LargeBlockHeights) {
    // Test some large heights to ensure no overflow
    EXPECT_EQ(RandomX_SeedHeight(100000), 98304);
    EXPECT_EQ(RandomX_SeedHeight(1000000), 998400);

    // Verify the pattern holds
    uint64_t height = 100000;
    uint64_t seed_height = RandomX_SeedHeight(height);

    // Seed height should be aligned to 1536-block boundaries
    EXPECT_EQ(seed_height % 1536, 0);

    // Seed height should be less than current height
    EXPECT_LT(seed_height, height);

    // Seed height should be within one epoch of (height - lag)
    uint64_t diff = height - seed_height;
    EXPECT_GE(diff, 96);
    EXPECT_LT(diff, 1536 + 96);
}

// Test initialization with genesis seed
TEST(RandomXEpochs, GenesisInitialization) {
    // Initialize RandomX (should use 0x08... seed for genesis)
    RandomX_Init();

    // Test hash calculation works (without verifying specific output)
    uint8_t input[100];
    memset(input, 0, sizeof(input));

    uint256 output;
    EXPECT_TRUE(RandomX_Hash(input, sizeof(input), output.begin()));

    // Output should not be all zeros (extremely unlikely with RandomX)
    bool all_zeros = true;
    for (int i = 0; i < 32; i++) {
        if (output.begin()[i] != 0) {
            all_zeros = false;
            break;
        }
    }
    EXPECT_FALSE(all_zeros);
}

// Test setting different seed hashes
TEST(RandomXEpochs, SeedHashSwitching) {
    // Create two different seed hashes
    uint256 seed1, seed2;
    memset(seed1.begin(), 0xAA, 32);
    memset(seed2.begin(), 0xBB, 32);

    // Set first seed
    RandomX_SetMainSeedHash(seed1.begin(), 32);

    // Calculate hash with first seed
    uint8_t input[100];
    memset(input, 0x01, sizeof(input));

    uint256 hash1;
    EXPECT_TRUE(RandomX_Hash(input, sizeof(input), hash1.begin()));

    // Set second seed
    RandomX_SetMainSeedHash(seed2.begin(), 32);

    // Calculate hash with second seed - should be different
    uint256 hash2;
    EXPECT_TRUE(RandomX_Hash(input, sizeof(input), hash2.begin()));

    // Hashes should be different (different seeds produce different outputs)
    EXPECT_NE(hash1, hash2);

    // Set first seed again
    RandomX_SetMainSeedHash(seed1.begin(), 32);

    // Should get the same hash as before
    uint256 hash3;
    EXPECT_TRUE(RandomX_Hash(input, sizeof(input), hash3.begin()));
    EXPECT_EQ(hash1, hash3);
}
