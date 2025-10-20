// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2016-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "pow.h"

#include "arith_uint256.h"
#include "chain.h"
#include "chainparams.h"
#include "crypto/equihash.h"
#include "crypto/randomx_wrapper.h"
#include "primitives/block.h"
#include "streams.h"
#include "uint256.h"
#include "util/system.h"

#include <librustzcash.h>
#include <rust/equihash.h>

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

    // Genesis block
    if (pindexLast == NULL)
        return nProofOfWorkLimit;

    // Regtest
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    {
        // Comparing to pindexLast->nHeight with >= because this function
        // returns the work required for the block after pindexLast.
        if (params.nPowAllowMinDifficultyBlocksAfterHeight != std::nullopt &&
            pindexLast->nHeight >= params.nPowAllowMinDifficultyBlocksAfterHeight.value())
        {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 6 * block interval minutes
            // then allow mining of a min-difficulty block.
            if (pblock && pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.PoWTargetSpacing(pindexLast->nHeight + 1) * 6)
                return nProofOfWorkLimit;
        }
    }

    // Find the first block in the averaging interval
    const CBlockIndex* pindexFirst = pindexLast;
    arith_uint256 bnTot {0};
    for (int i = 0; pindexFirst && i < params.nPowAveragingWindow; i++) {
        arith_uint256 bnTmp;
        bnTmp.SetCompact(pindexFirst->nBits);
        bnTot += bnTmp;
        pindexFirst = pindexFirst->pprev;
    }

    // Check we have enough blocks
    if (pindexFirst == NULL)
        return nProofOfWorkLimit;

    // The protocol specification leaves MeanTarget(height) as a rational, and takes the floor
    // only after dividing by AveragingWindowTimespan in the computation of Threshold(height):
    // <https://zips.z.cash/protocol/protocol.pdf#diffadjustment>
    //
    // Here we take the floor of MeanTarget(height) immediately, but that is equivalent to doing
    // so only after a further division, as proven in <https://math.stackexchange.com/a/147832/185422>.
    arith_uint256 bnAvg {bnTot / params.nPowAveragingWindow};

    return CalculateNextWorkRequired(bnAvg,
                                     pindexLast->GetMedianTimePast(), pindexFirst->GetMedianTimePast(),
                                     params,
                                     pindexLast->nHeight + 1);
}

unsigned int CalculateNextWorkRequired(arith_uint256 bnAvg,
                                       int64_t nLastBlockTime, int64_t nFirstBlockTime,
                                       const Consensus::Params& params,
                                       int nextHeight)
{
    int64_t averagingWindowTimespan = params.AveragingWindowTimespan(nextHeight);
    int64_t minActualTimespan = params.MinActualTimespan(nextHeight);
    int64_t maxActualTimespan = params.MaxActualTimespan(nextHeight);
    // Limit adjustment step
    // Use medians to prevent time-warp attacks
    int64_t nActualTimespan = nLastBlockTime - nFirstBlockTime;
    nActualTimespan = averagingWindowTimespan + (nActualTimespan - averagingWindowTimespan)/4;

    if (nActualTimespan < minActualTimespan) {
        nActualTimespan = minActualTimespan;
    }
    if (nActualTimespan > maxActualTimespan) {
        nActualTimespan = maxActualTimespan;
    }

    // Retarget
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    arith_uint256 bnNew {bnAvg};
    bnNew /= averagingWindowTimespan;
    bnNew *= nActualTimespan;

    if (bnNew > bnPowLimit) {
        bnNew = bnPowLimit;
    }

    return bnNew.GetCompact();
}

bool CheckEquihashSolution(const CBlockHeader *pblock, const Consensus::Params& params)
{
    unsigned int n = params.nEquihashN;
    unsigned int k = params.nEquihashK;

    // I = the block header minus nonce and solution.
    CEquihashInput I{*pblock};
    // I||V
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << I;

    return equihash::is_valid(
        n, k,
        {(const unsigned char*)ss.data(), ss.size()},
        {pblock->nNonce.begin(), pblock->nNonce.size()},
        {pblock->nSolution.data(), pblock->nSolution.size()});
}

bool CheckRandomXSolution(const CBlockHeader *pblock, const Consensus::Params& params, const CBlockIndex* pindexPrev)
{
    // For RandomX, we hash the block header (minus solution) with the nonce
    // The nSolution field stores the RandomX hash result for verification
    CEquihashInput I{*pblock};
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << I;
    ss << pblock->nNonce;

    // Determine the block height and seed hash
    uint64_t blockHeight = 0;
    uint256 seedHash;

    if (pindexPrev != nullptr) {
        blockHeight = pindexPrev->nHeight + 1;

        // Calculate seed height for this block
        uint64_t seedHeight = RandomX_SeedHeight(blockHeight);

        // Get the seed block hash
        if (seedHeight == 0) {
            // Genesis epoch: use 0x08 followed by zeros (matching Monero's style but with visible value)
            seedHash.SetNull();
            // Set first byte to 0x08 (at beginning of internal storage to match hex serialization)
            *seedHash.begin() = 0x08;
        } else {
            // Get the block at seed height
            const CBlockIndex* pindexSeed = pindexPrev->GetAncestor(seedHeight);
            if (pindexSeed == nullptr) {
                LogPrintf("RandomX: ERROR - Could not find seed block at height %d\n", seedHeight);
                return false;
            }
            seedHash = pindexSeed->GetBlockHash();
        }

        LogPrint("pow", "CheckRandomXSolution: Validating block at height %d with seed from block %d (hash: %s)\n",
                 blockHeight, seedHeight, seedHash.GetHex());

        // Calculate RandomX hash with the specific seed
        // Use internal byte order directly (little-endian) as Monero does
        uint256 hash;
        if (!RandomX_Hash_WithSeed(seedHash.begin(), 32, ss.data(), ss.size(), hash.begin())) {
            LogPrintf("CheckRandomXSolution: RandomX_Hash_WithSeed failed\n");
            return false;
        }

        // Verify the stored solution matches the calculated hash
        if (pblock->nSolution.size() != 32) {
            LogPrintf("CheckRandomXSolution: Invalid solution size: %d\n", pblock->nSolution.size());
            return false;
        }

        uint256 storedHash;
        memcpy(storedHash.begin(), pblock->nSolution.data(), 32);

        bool result = (hash == storedHash);
        if (!result) {
            LogPrintf("CheckRandomXSolution: Hash mismatch! Computed: %s, Stored: %s\n",
                     hash.GetHex(), storedHash.GetHex());
        }

        return result;
    } else {
        // No block index available - use current main seed (for mining/mempool)
        // This is less secure but necessary for stateless validation
        uint256 hash;
        if (!RandomX_Hash_Block(ss.data(), ss.size(), hash)) {
            return false;
        }

        // Verify the stored solution matches the calculated hash
        if (pblock->nSolution.size() != 32) {
            return false;
        }

        uint256 storedHash;
        memcpy(storedHash.begin(), pblock->nSolution.data(), 32);

        return hash == storedHash;
    }
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return false;

    return true;
}

arith_uint256 GetBlockProof(const CBlockIndex& block)
{
    arith_uint256 bnTarget;
    bool fNegative;
    bool fOverflow;
    bnTarget.SetCompact(block.nBits, &fNegative, &fOverflow);
    if (fNegative || fOverflow || bnTarget == 0)
        return 0;
    // We need to compute 2**256 / (bnTarget+1), but we can't represent 2**256
    // as it's too large for an arith_uint256. However, as 2**256 is at least as large
    // as bnTarget+1, it is equal to ((2**256 - bnTarget - 1) / (bnTarget+1)) + 1,
    // or ~bnTarget / (bnTarget+1) + 1.
    return (~bnTarget / (bnTarget + 1)) + 1;
}

int64_t GetBlockProofEquivalentTime(const CBlockIndex& to, const CBlockIndex& from, const CBlockIndex& tip, const Consensus::Params& params)
{
    arith_uint256 r;
    int sign = 1;
    if (to.nChainWork > from.nChainWork) {
        r = to.nChainWork - from.nChainWork;
    } else {
        r = from.nChainWork - to.nChainWork;
        sign = -1;
    }
    r = r * arith_uint256(params.PoWTargetSpacing(tip.nHeight)) / GetBlockProof(tip);
    if (r.bits() > 63) {
        return sign * std::numeric_limits<int64_t>::max();
    }
    return sign * r.GetLow64();
}
