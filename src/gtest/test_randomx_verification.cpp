#include <gtest/gtest.h>

#include "chain.h"
#include "chainparams.h"
#include "consensus/params.h"
#include "crypto/randomx_wrapper.h"
#include "pow.h"
#include "primitives/block.h"
#include "streams.h"
#include "uint256.h"
#include "util/test.h"

#include <vector>

// Helper: Create a mock blockchain for testing
class MockBlockchain {
public:
    std::vector<CBlockIndex> blocks;
    std::vector<uint256> blockHashes;

    MockBlockchain(size_t numBlocks) {
        blocks.resize(numBlocks);
        blockHashes.resize(numBlocks);

        for (size_t i = 0; i < numBlocks; i++) {
            blocks[i].pprev = (i > 0) ? &blocks[i-1] : nullptr;
            blocks[i].nHeight = i;
            blocks[i].nTime = 1231006505 + (i * 120);  // 2 minutes per block
            blocks[i].nBits = 0x207fffff;

            // Generate a unique hash for each block
            uint256 hash = ArithToUint256(arith_uint256(i + 1) * 100);
            blockHashes[i] = hash;
            blocks[i].phashBlock = &blockHashes[i];
        }
    }

    CBlockIndex* GetBlock(int height) {
        if (height < 0 || height >= (int)blocks.size()) return nullptr;
        return &blocks[height];
    }
};

// Test that RandomX verification works with correct seed
TEST(RandomXVerification, BasicVerification) {
    // Initialize RandomX with genesis seed
    RandomX_Init();

    // Create a simple block header
    CBlockHeader header;
    header.nVersion = 4;
    header.hashPrevBlock.SetNull();
    header.hashMerkleRoot.SetNull();
    header.nTime = 1231006505;
    header.nBits = 0x207fffff;
    header.nNonce.SetNull();

    // Serialize the header for hashing (minus nonce and solution)
    CEquihashInput I{header};
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << I;
    ss << header.nNonce;

    // Calculate RandomX hash
    uint256 hash;
    ASSERT_TRUE(RandomX_Hash_Block(ss.data(), ss.size(), hash));

    // Store hash as solution
    header.nSolution.resize(32);
    memcpy(header.nSolution.data(), hash.begin(), 32);

    // Verification should succeed (no pindexPrev, uses current main seed)
    SelectParams(CBaseChainParams::REGTEST);
    EXPECT_TRUE(CheckRandomXSolution(&header, Params().GetConsensus()));
}

// Test that verification fails with wrong solution
TEST(RandomXVerification, InvalidSolution) {
    RandomX_Init();

    CBlockHeader header;
    header.nVersion = 4;
    header.hashPrevBlock.SetNull();
    header.hashMerkleRoot.SetNull();
    header.nTime = 1231006505;
    header.nBits = 0x207fffff;
    header.nNonce.SetNull();

    // Create an invalid solution (all zeros)
    header.nSolution.resize(32);
    memset(header.nSolution.data(), 0, 32);

    SelectParams(CBaseChainParams::REGTEST);
    EXPECT_FALSE(CheckRandomXSolution(&header, Params().GetConsensus()));
}

// Test that verification works with epoch-specific seeds
TEST(RandomXVerification, EpochBasedVerification) {
    // Create mock blockchain
    MockBlockchain chain(5000);

    // Set seed for epoch 0 (all zeros)
    uint256 genesis_seed;
    genesis_seed.SetNull();
    RandomX_SetMainSeedHash(genesis_seed.begin(), 32);

    // Create a block at height 100 (epoch 0)
    CBlockHeader header1;
    header1.nVersion = 4;
    header1.hashPrevBlock = *chain.GetBlock(99)->phashBlock;
    header1.hashMerkleRoot.SetNull();
    header1.nTime = chain.GetBlock(99)->nTime + 120;
    header1.nBits = 0x207fffff;
    header1.nNonce = ArithToUint256(arith_uint256(1000));

    // Calculate hash for block 100
    CEquihashInput I1{header1};
    CDataStream ss1(SER_NETWORK, PROTOCOL_VERSION);
    ss1 << I1;
    ss1 << header1.nNonce;

    uint256 hash1;
    ASSERT_TRUE(RandomX_Hash_Block(ss1.data(), ss1.size(), hash1));

    header1.nSolution.resize(32);
    memcpy(header1.nSolution.data(), hash1.begin(), 32);

    // Should verify with epoch 0 seed
    SelectParams(CBaseChainParams::REGTEST);
    EXPECT_TRUE(CheckRandomXSolution(&header1, Params().GetConsensus(), chain.GetBlock(99)));
}

// Test seed hash retrieval with block index
TEST(RandomXVerification, SeedHashWithBlockIndex) {
    // Create mock blockchain with enough blocks to cross epoch boundary
    MockBlockchain chain(3000);

    SelectParams(CBaseChainParams::REGTEST);
    const Consensus::Params& params = Params().GetConsensus();

    // Test block at height 100 (epoch 0)
    {
        uint64_t seedHeight = RandomX_SeedHeight(100);
        EXPECT_EQ(seedHeight, 0);

        // Seed hash should be used from GetAncestor(0), which is genesis
        CBlockIndex* seed_block = chain.GetBlock(100)->GetAncestor(seedHeight);
        ASSERT_NE(seed_block, nullptr);
        EXPECT_EQ(seed_block->nHeight, 0);
    }

    // Test block at height 2113 (epoch 1, first block with new seed)
    {
        uint64_t seedHeight = RandomX_SeedHeight(2113);
        EXPECT_EQ(seedHeight, 2048);

        // Seed hash should be from block 2048
        CBlockIndex* seed_block = chain.GetBlock(2113)->GetAncestor(seedHeight);
        ASSERT_NE(seed_block, nullptr);
        EXPECT_EQ(seed_block->nHeight, 2048);
    }

    // Test block at height 2500 (epoch 1)
    {
        uint64_t seedHeight = RandomX_SeedHeight(2500);
        EXPECT_EQ(seedHeight, 2048);

        CBlockIndex* seed_block = chain.GetBlock(2500)->GetAncestor(seedHeight);
        ASSERT_NE(seed_block, nullptr);
        EXPECT_EQ(seed_block->nHeight, 2048);
    }
}

// Test verification across epoch transition
TEST(RandomXVerification, EpochTransition) {
    // Create blockchain crossing epoch boundary
    MockBlockchain chain(2200);

    SelectParams(CBaseChainParams::REGTEST);
    const Consensus::Params& params = Params().GetConsensus();

    // Block 2111 - last block of epoch 0
    {
        uint64_t seedHeight = RandomX_SeedHeight(2111);
        EXPECT_EQ(seedHeight, 0);  // Should use genesis seed

        // Set genesis seed
        uint256 genesis_seed;
        genesis_seed.SetNull();
        RandomX_SetMainSeedHash(genesis_seed.begin(), 32);

        // Create and verify block
        CBlockHeader header;
        header.nVersion = 4;
        header.hashPrevBlock = *chain.GetBlock(2110)->phashBlock;
        header.hashMerkleRoot.SetNull();
        header.nTime = chain.GetBlock(2110)->nTime + 120;
        header.nBits = 0x207fffff;
        header.nNonce = ArithToUint256(arith_uint256(2111));

        CEquihashInput I{header};
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << I;
        ss << header.nNonce;

        uint256 hash;
        ASSERT_TRUE(RandomX_Hash_Block(ss.data(), ss.size(), hash));

        header.nSolution.resize(32);
        memcpy(header.nSolution.data(), hash.begin(), 32);

        EXPECT_TRUE(CheckRandomXSolution(&header, params, chain.GetBlock(2110)));
    }

    // Block 2112 - transition block, still uses epoch 0 seed
    {
        uint64_t seedHeight = RandomX_SeedHeight(2112);
        EXPECT_EQ(seedHeight, 0);  // Still genesis seed
    }

    // Block 2113 - first block with epoch 1 seed
    {
        uint64_t seedHeight = RandomX_SeedHeight(2113);
        EXPECT_EQ(seedHeight, 2048);  // Now uses block 2048 hash

        // Set epoch 1 seed (block 2048 hash)
        uint256 epoch1_seed = *chain.GetBlock(2048)->phashBlock;
        RandomX_SetMainSeedHash(epoch1_seed.begin(), 32);

        // Create and verify block
        CBlockHeader header;
        header.nVersion = 4;
        header.hashPrevBlock = *chain.GetBlock(2112)->phashBlock;
        header.hashMerkleRoot.SetNull();
        header.nTime = chain.GetBlock(2112)->nTime + 120;
        header.nBits = 0x207fffff;
        header.nNonce = ArithToUint256(arith_uint256(2113));

        CEquihashInput I{header};
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << I;
        ss << header.nNonce;

        uint256 hash;
        ASSERT_TRUE(RandomX_Hash_Block(ss.data(), ss.size(), hash));

        header.nSolution.resize(32);
        memcpy(header.nSolution.data(), hash.begin(), 32);

        EXPECT_TRUE(CheckRandomXSolution(&header, params, chain.GetBlock(2112)));
    }
}

// Test that different seeds produce different hashes
TEST(RandomXVerification, DifferentSeedsProduceDifferentHashes) {
    // Create two different seeds
    uint256 seed1, seed2;
    seed1.SetNull();  // All zeros
    memset(seed2.begin(), 0xFF, 32);  // All FFs

    // Same input data
    uint8_t input[100];
    memset(input, 0x42, sizeof(input));

    // Hash with first seed
    RandomX_SetMainSeedHash(seed1.begin(), 32);
    uint256 hash1;
    ASSERT_TRUE(RandomX_Hash(input, sizeof(input), hash1.begin()));

    // Hash with second seed
    RandomX_SetMainSeedHash(seed2.begin(), 32);
    uint256 hash2;
    ASSERT_TRUE(RandomX_Hash(input, sizeof(input), hash2.begin()));

    // Hashes must be different
    EXPECT_NE(hash1, hash2);
}

// Test RandomX_Hash_WithSeed directly
TEST(RandomXVerification, HashWithSeedDirect) {
    uint256 seed;
    seed.SetNull();

    uint8_t input[100];
    memset(input, 0x55, sizeof(input));

    uint256 output1, output2;

    // Hash with seed directly
    ASSERT_TRUE(RandomX_Hash_WithSeed(seed.begin(), 32, input, sizeof(input), output1.begin()));

    // Set main seed and hash
    RandomX_SetMainSeedHash(seed.begin(), 32);
    ASSERT_TRUE(RandomX_Hash(input, sizeof(input), output2.begin()));

    // Both methods should produce the same result
    EXPECT_EQ(output1, output2);
}
