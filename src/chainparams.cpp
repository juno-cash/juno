// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2015-2025 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "chainparams.h"
#include "consensus/merkle.h"
#include "key_io.h"
#include "main.h"
#include "crypto/equihash.h"

#include "tinyformat.h"
#include "util/system.h"
#include "util/strencodings.h"

#include <assert.h>
#include <optional>
#include <variant>

#include <boost/assign/list_of.hpp>

#include "chainparamsseeds.h"

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, const uint256& nNonce, const std::vector<unsigned char>& nSolution, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    // To create a genesis block for a new chain which is Overwintered:
    //   txNew.nVersion = OVERWINTER_TX_VERSION
    //   txNew.fOverwintered = true
    //   txNew.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID
    //   txNew.nExpiryHeight = <default value>
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 520617983 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nSolution = nSolution;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(txNew);
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database (and is in any case of zero value).
 *
 * >>> from hashlib import blake2s
 * >>> 'Zcash' + blake2s(b'The Economist 2016-10-29 Known unknown: Another crypto-currency is born. BTC#436254 0000000000000000044f321997f336d2908cf8c8d6893e88dbf067e2d949487d ETH#2521903 483039a6b6bd8bd05f0584f9a078d075e454925eb71c1f13eaff59b405a721bb DJIA close on 27 Oct 2016: 18,169.68').hexdigest()
 *
 * CBlock(hash=00040fe8, ver=4, hashPrevBlock=00000000000000, hashMerkleRoot=c4eaa5, nTime=1477641360, nBits=1f07ffff, nNonce=4695, vtx=1)
 *   CTransaction(hash=c4eaa5, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(000000, -1), coinbase 04ffff071f0104455a6361736830623963346565663862376363343137656535303031653335303039383462366665613335363833613763616331343161303433633432303634383335643334)
 *     CTxOut(nValue=0.00000000, scriptPubKey=0x5F1DF16B2B704C8A578D0B)
 *   vMerkleTree: c4eaa5
 */
static CBlock CreateGenesisBlock(const char* pszTimestamp, uint32_t nTime, const uint256& nNonce, const std::vector<unsigned char>& nSolution, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const CScript genesisOutputScript = CScript() << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f") << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nSolution, nBits, nVersion, genesisReward);
}

/**
 * Main network
 */
/**
 * What makes a good checkpoint block?
 * + Is surrounded by blocks with reasonable timestamps
 *   (no blocks before with a timestamp after, none after with
 *    timestamp before)
 * + Contains no strange transactions
 */

const arith_uint256 maxUint = UintToArith256(uint256S("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"));

class CMainParams : public CChainParams {
public:
    CMainParams() {
        keyConstants.strNetworkID = "main";
        strCurrencyUnits = "JMR";
        keyConstants.bip44CoinType = 8133; // Juno Moneta coin type
        consensus.fCoinbaseMustBeShielded = true;
        consensus.nSubsidySlowStartInterval = 20000;
        consensus.nPreBlossomSubsidyHalvingInterval = Consensus::PRE_BLOSSOM_HALVING_INTERVAL;
        consensus.nPostBlossomSubsidyHalvingInterval = POST_BLOSSOM_HALVING_INTERVAL(Consensus::PRE_BLOSSOM_HALVING_INTERVAL);
        consensus.nMajorityEnforceBlockUpgrade = 750;
        consensus.nMajorityRejectBlockOutdated = 950;
        consensus.nMajorityWindow = 4000;
        const size_t N = 200, K = 9;
        static_assert(equihash_parameters_acceptable(N, K));
        consensus.nEquihashN = N;
        consensus.nEquihashK = K;
        // RandomX powLimit: targeting ~500 H/s @ 60s blocks = ~100k difficulty
        // This is a very easy difficulty suitable for CPU mining on laptops
        consensus.powLimit = uint256S("00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowAveragingWindow = 100;
        assert(maxUint/UintToArith256(consensus.powLimit) >= consensus.nPowAveragingWindow);
        consensus.nPowMaxAdjustDown = 32; // 32% adjustment down
        consensus.nPowMaxAdjustUp = 16; // 16% adjustment up
        consensus.nPreBlossomPowTargetSpacing = Consensus::PRE_BLOSSOM_POW_TARGET_SPACING;
        consensus.nPostBlossomPowTargetSpacing = Consensus::POST_BLOSSOM_POW_TARGET_SPACING;
        consensus.nPowAllowMinDifficultyBlocksAfterHeight = std::nullopt;
        consensus.fPowNoRetargeting = false;
        // Simplified consensus upgrade activation for this fork
        // All upgrades activate sequentially at blocks 1-8
        consensus.vUpgrades[Consensus::BASE_SPROUT].nProtocolVersion = 170002;
        consensus.vUpgrades[Consensus::BASE_SPROUT].nActivationHeight =
            Consensus::NetworkUpgrade::ALWAYS_ACTIVE;
        consensus.vUpgrades[Consensus::UPGRADE_TESTDUMMY].nProtocolVersion = 170002;
        consensus.vUpgrades[Consensus::UPGRADE_TESTDUMMY].nActivationHeight =
            Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT;
        consensus.vUpgrades[Consensus::UPGRADE_OVERWINTER].nProtocolVersion = 170005;
        consensus.vUpgrades[Consensus::UPGRADE_OVERWINTER].nActivationHeight = 1;
        consensus.vUpgrades[Consensus::UPGRADE_SAPLING].nProtocolVersion = 170007;
        consensus.vUpgrades[Consensus::UPGRADE_SAPLING].nActivationHeight = 2;
        consensus.vUpgrades[Consensus::UPGRADE_BLOSSOM].nProtocolVersion = 170009;
        consensus.vUpgrades[Consensus::UPGRADE_BLOSSOM].nActivationHeight = 3;
        consensus.vUpgrades[Consensus::UPGRADE_HEARTWOOD].nProtocolVersion = 170011;
        consensus.vUpgrades[Consensus::UPGRADE_HEARTWOOD].nActivationHeight = 4;
        consensus.vUpgrades[Consensus::UPGRADE_CANOPY].nProtocolVersion = 170013;
        consensus.vUpgrades[Consensus::UPGRADE_CANOPY].nActivationHeight = 5;
        consensus.vUpgrades[Consensus::UPGRADE_NU5].nProtocolVersion = 170100;
        consensus.vUpgrades[Consensus::UPGRADE_NU5].nActivationHeight = 6;
        consensus.vUpgrades[Consensus::UPGRADE_NU6].nProtocolVersion = 170120;
        consensus.vUpgrades[Consensus::UPGRADE_NU6].nActivationHeight = 7;
        consensus.vUpgrades[Consensus::UPGRADE_NU6_1].nProtocolVersion = 170140;
        consensus.vUpgrades[Consensus::UPGRADE_NU6_1].nActivationHeight = 8;
        consensus.vUpgrades[Consensus::UPGRADE_ZFUTURE].nProtocolVersion = 0x7FFFFFFF;
        consensus.vUpgrades[Consensus::UPGRADE_ZFUTURE].nActivationHeight =
            Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT;

        consensus.nFundingPeriodLength = consensus.nPostBlossomSubsidyHalvingInterval / 48;

        // guarantees the first 2 characters, when base58 encoded, are "t1"
        keyConstants.base58Prefixes[PUBKEY_ADDRESS]     = {0x1C,0xB8};
        // guarantees the first 2 characters, when base58 encoded, are "t3"
        keyConstants.base58Prefixes[SCRIPT_ADDRESS]     = {0x1C,0xBD};
        // the first character, when base58 encoded, is "5" or "K" or "L" (as in Bitcoin)
        keyConstants.base58Prefixes[SECRET_KEY]         = {0x80};
        // do not rely on these BIP32 prefixes; they are not specified and may change
        keyConstants.base58Prefixes[EXT_PUBLIC_KEY]     = {0x04,0x88,0xB2,0x1E};
        keyConstants.base58Prefixes[EXT_SECRET_KEY]     = {0x04,0x88,0xAD,0xE4};
        // guarantees the first 2 characters, when base58 encoded, are "zc"
        keyConstants.base58Prefixes[ZCPAYMENT_ADDRESS]  = {0x16,0x9A};
        // guarantees the first 4 characters, when base58 encoded, are "ZiVK"
        keyConstants.base58Prefixes[ZCVIEWING_KEY]      = {0xA8,0xAB,0xD3};
        // guarantees the first 2 characters, when base58 encoded, are "SK"
        keyConstants.base58Prefixes[ZCSPENDING_KEY]     = {0xAB,0x36};

        keyConstants.bech32HRPs[SAPLING_PAYMENT_ADDRESS]      = "zs";
        keyConstants.bech32HRPs[SAPLING_FULL_VIEWING_KEY]     = "zviews";
        keyConstants.bech32HRPs[SAPLING_INCOMING_VIEWING_KEY] = "zivks";
        keyConstants.bech32HRPs[SAPLING_EXTENDED_SPEND_KEY]   = "secret-extended-key-main";
        keyConstants.bech32HRPs[SAPLING_EXTENDED_FVK]         = "zxviews";

        keyConstants.bech32mHRPs[TEX_ADDRESS]                 = "tex";

        // Mainnet funding streams removed for this fork - 100% to miners
        /*
        {
            auto canopyActivation = consensus.vUpgrades[Consensus::UPGRADE_CANOPY].nActivationHeight;
            auto nu6Activation = consensus.vUpgrades[Consensus::UPGRADE_NU6].nActivationHeight;
            auto nu6_1Activation = consensus.vUpgrades[Consensus::UPGRADE_NU6_1].nActivationHeight;

            // ZIP 214 Revision 0
            std::vector<std::string> bp_addresses = {
                "t3LmX1cxWPPPqL4TZHx42HU3U5ghbFjRiif",
                "t3Toxk1vJQ6UjWQ42tUJz2rV2feUWkpbTDs",
                "t3ZBdBe4iokmsjdhMuwkxEdqMCFN16YxKe6",
                "t3ZuaJziLM8xZ32rjDUzVjVtyYdDSz8GLWB",
                "t3bAtYWa4bi8VrtvqySxnbr5uqcG9czQGTZ",
                "t3dktADfb5Rmxncpe1HS5BRS5Gcj7MZWYBi",
                "t3hgskquvKKoCtvxw86yN7q8bzwRxNgUZmc",
                "t3R1VrLzwcxAZzkX4mX3KGbWpNsgtYtMntj",
                "t3ff6fhemqPMVujD3AQurxRxTdvS1pPSaa2",
                "t3cEUQFG3KYnFG6qYhPxSNgGi3HDjUPwC3J",
                "t3WR9F5U4QvUFqqx9zFmwT6xFqduqRRXnaa",
                "t3PYc1LWngrdUrJJbHkYPCKvJuvJjcm85Ch",
                "t3bgkjiUeatWNkhxY3cWyLbTxKksAfk561R",
                "t3Z5rrR8zahxUpZ8itmCKhMSfxiKjUp5Dk5",
                "t3PU1j7YW3fJ67jUbkGhSRto8qK2qXCUiW3",
                "t3S3yaT7EwNLaFZCamfsxxKwamQW2aRGEkh",
                "t3eutXKJ9tEaPSxZpmowhzKhPfJvmtwTEZK",
                "t3gbTb7brxLdVVghSPSd3ycGxzHbUpukeDm",
                "t3UCKW2LrHFqPMQFEbZn6FpjqnhAAbfpMYR",
                "t3NyHsrnYbqaySoQqEQRyTWkjvM2PLkU7Uu",
                "t3QEFL6acxuZwiXtW3YvV6njDVGjJ1qeaRo",
                "t3PdBRr2S1XTDzrV8bnZkXF3SJcrzHWe1wj",
                "t3ZWyRPpWRo23pKxTLtWsnfEKeq9T4XPxKM",
                "t3he6QytKCTydhpztykFsSsb9PmBT5JBZLi",
                "t3VWxWDsLb2TURNEP6tA1ZSeQzUmPKFNxRY",
                "t3NmWLvZkbciNAipauzsFRMxoZGqmtJksbz",
                "t3cKr4YxVPvPBG1mCvzaoTTdBNokohsRJ8n",
                "t3T3smGZn6BoSFXWWXa1RaoQdcyaFjMfuYK",
                "t3gkDUe9Gm4GGpjMk86TiJZqhztBVMiUSSA",
                "t3eretuBeBXFHe5jAqeSpUS1cpxVh51fAeb",
                "t3dN8g9zi2UGJdixGe9txeSxeofLS9t3yFQ",
                "t3S799pq9sYBFwccRecoTJ3SvQXRHPrHqvx",
                "t3fhYnv1S5dXwau7GED3c1XErzt4n4vDxmf",
                "t3cmE3vsBc5xfDJKXXZdpydCPSdZqt6AcNi",
                "t3h5fPdjJVHaH4HwynYDM5BB3J7uQaoUwKi",
                "t3Ma35c68BgRX8sdLDJ6WR1PCrKiWHG4Da9",
                "t3LokMKPL1J8rkJZvVpfuH7dLu6oUWqZKQK",
                "t3WFFGbEbhJWnASZxVLw2iTJBZfJGGX73mM",
                "t3L8GLEsUn4QHNaRYcX3EGyXmQ8kjpT1zTa",
                "t3PgfByBhaBSkH8uq4nYJ9ZBX4NhGCJBVYm",
                "t3WecsqKDhWXD4JAgBVcnaCC2itzyNZhJrv",
                "t3ZG9cSfopnsMQupKW5v9sTotjcP5P6RTbn",
                "t3hC1Ywb5zDwUYYV8LwhvF5rZ6m49jxXSG5",
                "t3VgMqDL15ZcyQDeqBsBW3W6rzfftrWP2yB",
                "t3LC94Y6BwLoDtBoK2NuewaEbnko1zvR9rm",
                "t3cWCUZJR3GtALaTcatrrpNJ3MGbMFVLRwQ",
                "t3YYF4rPLVxDcF9hHFsXyc5Yq1TFfbojCY6",
                "t3XHAGxRP2FNfhAjxGjxbrQPYtQQjc3RCQD",
            };

            // ZF and MG each use a single address repeated 48 times,
            // once for each funding period.
            std::vector<std::string> zf_addresses(48, "t3dvVE3SQEi7kqNzwrfNePxZ1d4hUyztBA1");
            std::vector<std::string> mg_addresses(48, "t3XyYW8yBFRuMnfvm5KLGFbEVz25kckZXym");

            consensus.AddZIP207FundingStream(
                keyConstants,
                Consensus::FS_ZIP214_BP,
                canopyActivation,
                nu6Activation,
                bp_addresses);
            consensus.AddZIP207FundingStream(
                keyConstants,
                Consensus::FS_ZIP214_ZF,
                canopyActivation,
                nu6Activation,
                zf_addresses);
            consensus.AddZIP207FundingStream(
                keyConstants,
                Consensus::FS_ZIP214_MG,
                canopyActivation,
                nu6Activation,
                mg_addresses);

            // ZIP 214 Revision 1
            // FPF uses a single address repeated 12 times, once for each funding period.
            std::vector<std::string> fpf_addresses(12, "t3cFfPt1Bcvgez9ZbMBFWeZsskxTkPzGCow");

            consensus.AddZIP207FundingStream(
                keyConstants,
                Consensus::FS_FPF_ZCG,
                nu6Activation,
                nu6_1Activation,
                fpf_addresses);
            consensus.AddZIP207LockboxStream(
                keyConstants,
                Consensus::FS_DEFERRED,
                nu6Activation,
                nu6_1Activation);

            // ZIP 214 Revision 2
            // FPF uses a single address repeated 36 times, once for each funding period.
            std::vector<std::string> fpf_addresses_h3(36, "t3cFfPt1Bcvgez9ZbMBFWeZsskxTkPzGCow");
            consensus.AddZIP207FundingStream(
                keyConstants,
                Consensus::FS_FPF_ZCG_H3,
                nu6_1Activation,
                4406400,
                fpf_addresses_h3);
            consensus.AddZIP207LockboxStream(
                keyConstants,
                Consensus::FS_CCF_H3,
                nu6_1Activation,
                4406400);

            // ZIP 271
            // For convenience of distribution, we split the lockbox contents into 10 equal chunks.
            std::string nu6_1_kho_address = "t3ev37Q2uL1sfTsiJQJiWJoFzQpDhmnUwYo";
            static const CAmount nu6_1_disbursement_amount = 78750 * COIN;
            static const CAmount nu6_1_chunk_amount = 7875 * COIN;
            static constexpr auto nu6_1_chunks = {
                Consensus::LD_ZIP271_NU6_1_CHUNK_1,
                Consensus::LD_ZIP271_NU6_1_CHUNK_2,
                Consensus::LD_ZIP271_NU6_1_CHUNK_3,
                Consensus::LD_ZIP271_NU6_1_CHUNK_4,
                Consensus::LD_ZIP271_NU6_1_CHUNK_5,
                Consensus::LD_ZIP271_NU6_1_CHUNK_6,
                Consensus::LD_ZIP271_NU6_1_CHUNK_7,
                Consensus::LD_ZIP271_NU6_1_CHUNK_8,
                Consensus::LD_ZIP271_NU6_1_CHUNK_9,
                Consensus::LD_ZIP271_NU6_1_CHUNK_10,
            };
            static_assert(nu6_1_chunk_amount * nu6_1_chunks.size() == nu6_1_disbursement_amount);
            for (auto idx : nu6_1_chunks) {
                consensus.AddZIP271LockboxDisbursement(
                    keyConstants,
                    idx,
                    Consensus::UPGRADE_NU6_1,
                    nu6_1_chunk_amount,
                    nu6_1_kho_address);
            }
        }
        */

        // The best chain should have at least this much work.
        // Set to zero for new fork to allow mining from genesis without IBD
        consensus.nMinimumChainWork = uint256S("0x00");

        /**
         * New magic bytes for this fork - derived from Bitcoin block 919123 hash
         */
        pchMessageStart[0] = 0xb5;
        pchMessageStart[1] = 0x0c;
        pchMessageStart[2] = 0x07;
        pchMessageStart[3] = 0x02;
        vAlertPubKey = ParseHex("04b7ecf0baa90495ceb4e4090f6b2fd37eec1e9c85fac68a487f3ce11589692e4a317479316ee814e066638e1db54e37a10689b70286e6315b1087b6615d179264");
        // New port for this fork
        nDefaultPort = 8234;
        nPruneAfterHeight = 100000;

        // Mainnet genesis timestamp - Bitcoin block 919123 as proof of post-mining
        const char* pszTimestamp = "bitcoin:919123:000000000000000000011124a15b43fc430a28d5c50d15a5edffdbdcb50c0702";

        genesis = CreateGenesisBlock(
            pszTimestamp,
            1760195960,
            uint256S("0x5400000000000000000000000000000000000000000000000000000000000000"),
            ParseHex("7a35116c47fc0c0bdf951ee4687dafd20b8f6bf68d93c0709eb40b58d384f500"),
            0x2000ffff, 4, 0);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(genesis.hashMerkleRoot == uint256S("0x0f9faf242c95f27c1eaafa666d60274b80bf65163599656a67486d7ed3426c5a"));

        vFixedSeeds.clear();
        // Remove all Zcash DNS seeds - this is a separate network
        vSeeds.clear();
        vSeeds.push_back(CDNSSeedData("junomoneta.io", "mainseeds.junomoneta.io"));

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        fMiningRequiresPeers = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;
        fTestnetToBeDeprecatedFieldRPC = false;

        checkpointData = (CCheckpointData) {
            boost::assign::map_list_of
            (      0, consensus.hashGenesisBlock)
            (   2500, uint256S("0x00000006dc968f600be11a86cbfbf7feb61c7577f45caced2e82b6d261d19744"))
            (  15000, uint256S("0x00000000b6bc56656812a5b8dcad69d6ad4446dec23b5ec456c18641fb5381ba"))
            (  67500, uint256S("0x000000006b366d2c1649a6ebb4787ac2b39c422f451880bc922e3a6fbd723616"))
            ( 100000, uint256S("0x000000001c5c82cd6baccfc0879e3830fd50d5ede17fa2c37a9a253c610eb285"))
            ( 133337, uint256S("0x0000000002776ccfaf06cc19857accf3e20c01965282f916b8a886e3e4a05be9"))
            ( 180000, uint256S("0x000000001205b742eac4a1b3959635bdf8aeada078d6a996df89740f7b54351d"))
            ( 222222, uint256S("0x000000000cafb9e56445a6cabc8057b57ee6fcc709e7adbfa195e5c7fac61343"))
            ( 270000, uint256S("0x00000000025c1cfa0258e33ab050aaa9338a3d4aaa3eb41defefc887779a9729"))
            ( 304600, uint256S("0x00000000028324e022a45014c4a4dc51e95d41e6bceb6ad554c5b65d5cea3ea5"))
            ( 410100, uint256S("0x0000000002c565958f783a24a4ac17cde898ff525e75ed9baf66861b0b9fcada"))
            ( 497000, uint256S("0x0000000000abd333f0acca6ffdf78a167699686d6a7d25c33fca5f295061ffff"))
            ( 525000, uint256S("0x0000000001a36c500378be8862d9bf1bea8f1616da6e155971b608139cc7e39b"))
            ( 650000, uint256S("0x0000000000a0a3fbbd739fb4fcbbfefff44efffc2064ca69a59d5284a2da26e2"))
            ( 800000, uint256S("0x00000000013f1f4e5634e896ebdbe63dec115547c1480de0d83c64426f913c27"))
            (1000000, uint256S("0x000000000062eff9ae053020017bfef24e521a2704c5ec9ead2a4608ac70fc7a"))
            (1200000, uint256S("0x0000000000347d5011108fdcf667c93e622e8635c94e586556898e41db18d192"))
            (1400000, uint256S("0x0000000001155ecec0ad3924d47ad476c0a5ed7527b8776f53cbda1a780b9f76"))
            (1600000, uint256S("0x0000000000aae69fb228f90e77f34c24b7920667eaca726c3a3939536f03dcfc"))
            (1860000, uint256S("0x000000000043a968c78af5fb8133e00e6fe340051c19dd969e53ab62bf3dc22a"))
            (2000000, uint256S("0x00000000010accaf2f87934765dc2e0bf4823a2b1ae2c1395b334acfce52ad68"))
            (2200000, uint256S("0x0000000001a0139c4c4d0e8f68cc562227c6003f4b1b640a3d921aeb8c3d2e3d"))
            (2400000, uint256S("0x0000000000294d1c8d87a1b6566d302aa983691bc3cab0583a245389bbb9d285"))
            (2600000, uint256S("0x0000000000b5ad92fcec0069d590f674d05ec7d96b1ff727863ea390950c4e49"))
            (2800000, uint256S("0x00000000011a226fb25d778d65b055605a82da016989b7788e0ce83c4f8d64f7"))
            (3000000, uint256S("0x0000000000573729e4db33678233e5dc0cc721c9c09977c64dcaa3f6344de8e9")),
            1752983473,     // * UNIX timestamp of last checkpoint block
            15537904,       // * total number of transactions between genesis and last checkpoint
            5967            // * estimated number of transactions per day after checkpoint
                            //   (total number of tx * 48 * 24) / checkpoint block height
        };

        // Hardcoded fallback value for the Sprout shielded value pool balance
        // for nodes that have not reindexed since the introduction of monitoring
        // in #2795.
        nSproutValuePoolCheckpointHeight = 520633;
        nSproutValuePoolCheckpointBalance = 22145062442933;
        fZIP209Enabled = true;
        hashSproutValuePoolCheckpointBlock = uint256S("0000000000c7b46b6bc04b4cbf87d8bb08722aebd51232619b214f7273f8460e");

        // Founders reward script expects a vector of 2-of-3 multisig addresses
        vFoundersRewardAddress = {
            "t3Vz22vK5z2LcKEdg16Yv4FFneEL1zg9ojd", /* main-index: 0*/
            "t3cL9AucCajm3HXDhb5jBnJK2vapVoXsop3", /* main-index: 1*/
            "t3fqvkzrrNaMcamkQMwAyHRjfDdM2xQvDTR", /* main-index: 2*/
            "t3TgZ9ZT2CTSK44AnUPi6qeNaHa2eC7pUyF", /* main-index: 3*/
            "t3SpkcPQPfuRYHsP5vz3Pv86PgKo5m9KVmx", /* main-index: 4*/
            "t3Xt4oQMRPagwbpQqkgAViQgtST4VoSWR6S", /* main-index: 5*/
            "t3ayBkZ4w6kKXynwoHZFUSSgXRKtogTXNgb", /* main-index: 6*/
            "t3adJBQuaa21u7NxbR8YMzp3km3TbSZ4MGB", /* main-index: 7*/
            "t3K4aLYagSSBySdrfAGGeUd5H9z5Qvz88t2", /* main-index: 8*/
            "t3RYnsc5nhEvKiva3ZPhfRSk7eyh1CrA6Rk", /* main-index: 9*/
            "t3Ut4KUq2ZSMTPNE67pBU5LqYCi2q36KpXQ", /* main-index: 10*/
            "t3ZnCNAvgu6CSyHm1vWtrx3aiN98dSAGpnD", /* main-index: 11*/
            "t3fB9cB3eSYim64BS9xfwAHQUKLgQQroBDG", /* main-index: 12*/
            "t3cwZfKNNj2vXMAHBQeewm6pXhKFdhk18kD", /* main-index: 13*/
            "t3YcoujXfspWy7rbNUsGKxFEWZqNstGpeG4", /* main-index: 14*/
            "t3bLvCLigc6rbNrUTS5NwkgyVrZcZumTRa4", /* main-index: 15*/
            "t3VvHWa7r3oy67YtU4LZKGCWa2J6eGHvShi", /* main-index: 16*/
            "t3eF9X6X2dSo7MCvTjfZEzwWrVzquxRLNeY", /* main-index: 17*/
            "t3esCNwwmcyc8i9qQfyTbYhTqmYXZ9AwK3X", /* main-index: 18*/
            "t3M4jN7hYE2e27yLsuQPPjuVek81WV3VbBj", /* main-index: 19*/
            "t3gGWxdC67CYNoBbPjNvrrWLAWxPqZLxrVY", /* main-index: 20*/
            "t3LTWeoxeWPbmdkUD3NWBquk4WkazhFBmvU", /* main-index: 21*/
            "t3P5KKX97gXYFSaSjJPiruQEX84yF5z3Tjq", /* main-index: 22*/
            "t3f3T3nCWsEpzmD35VK62JgQfFig74dV8C9", /* main-index: 23*/
            "t3Rqonuzz7afkF7156ZA4vi4iimRSEn41hj", /* main-index: 24*/
            "t3fJZ5jYsyxDtvNrWBeoMbvJaQCj4JJgbgX", /* main-index: 25*/
            "t3Pnbg7XjP7FGPBUuz75H65aczphHgkpoJW", /* main-index: 26*/
            "t3WeKQDxCijL5X7rwFem1MTL9ZwVJkUFhpF", /* main-index: 27*/
            "t3Y9FNi26J7UtAUC4moaETLbMo8KS1Be6ME", /* main-index: 28*/
            "t3aNRLLsL2y8xcjPheZZwFy3Pcv7CsTwBec", /* main-index: 29*/
            "t3gQDEavk5VzAAHK8TrQu2BWDLxEiF1unBm", /* main-index: 30*/
            "t3Rbykhx1TUFrgXrmBYrAJe2STxRKFL7G9r", /* main-index: 31*/
            "t3aaW4aTdP7a8d1VTE1Bod2yhbeggHgMajR", /* main-index: 32*/
            "t3YEiAa6uEjXwFL2v5ztU1fn3yKgzMQqNyo", /* main-index: 33*/
            "t3g1yUUwt2PbmDvMDevTCPWUcbDatL2iQGP", /* main-index: 34*/
            "t3dPWnep6YqGPuY1CecgbeZrY9iUwH8Yd4z", /* main-index: 35*/
            "t3QRZXHDPh2hwU46iQs2776kRuuWfwFp4dV", /* main-index: 36*/
            "t3enhACRxi1ZD7e8ePomVGKn7wp7N9fFJ3r", /* main-index: 37*/
            "t3PkLgT71TnF112nSwBToXsD77yNbx2gJJY", /* main-index: 38*/
            "t3LQtHUDoe7ZhhvddRv4vnaoNAhCr2f4oFN", /* main-index: 39*/
            "t3fNcdBUbycvbCtsD2n9q3LuxG7jVPvFB8L", /* main-index: 40*/
            "t3dKojUU2EMjs28nHV84TvkVEUDu1M1FaEx", /* main-index: 41*/
            "t3aKH6NiWN1ofGd8c19rZiqgYpkJ3n679ME", /* main-index: 42*/
            "t3MEXDF9Wsi63KwpPuQdD6by32Mw2bNTbEa", /* main-index: 43*/
            "t3WDhPfik343yNmPTqtkZAoQZeqA83K7Y3f", /* main-index: 44*/
            "t3PSn5TbMMAEw7Eu36DYctFezRzpX1hzf3M", /* main-index: 45*/
            "t3R3Y5vnBLrEn8L6wFjPjBLnxSUQsKnmFpv", /* main-index: 46*/
            "t3Pcm737EsVkGTbhsu2NekKtJeG92mvYyoN", /* main-index: 47*/
        };

        assert(vFoundersRewardAddress.size() <= consensus.GetLastFoundersRewardBlockHeight(0));

        // Developer fund address for optional donations
        strDeveloperFundAddress = "t1HwfuDqt2oAVexgpjDHg9yB7UpCKSmEES7";
    }
};
static CMainParams mainParams;

/**
 * Testnet (v3)
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        keyConstants.strNetworkID = "test";
        strCurrencyUnits = "TJMR";
        keyConstants.bip44CoinType = 1;
        consensus.fCoinbaseMustBeShielded = true;
        consensus.nSubsidySlowStartInterval = 20000;
        consensus.nPreBlossomSubsidyHalvingInterval = Consensus::PRE_BLOSSOM_HALVING_INTERVAL;
        consensus.nPostBlossomSubsidyHalvingInterval = POST_BLOSSOM_HALVING_INTERVAL(Consensus::PRE_BLOSSOM_HALVING_INTERVAL);
        consensus.nMajorityEnforceBlockUpgrade = 51;
        consensus.nMajorityRejectBlockOutdated = 75;
        consensus.nMajorityWindow = 400;
        const size_t N = 200, K = 9;
        static_assert(equihash_parameters_acceptable(N, K));
        consensus.nEquihashN = N;
        consensus.nEquihashK = K;
        // RandomX powLimit: targeting ~500 H/s @ 60s blocks = ~100k difficulty
        // Same as mainnet for testnet consistency
        consensus.powLimit = uint256S("00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowAveragingWindow = 100;
        assert(maxUint/UintToArith256(consensus.powLimit) >= consensus.nPowAveragingWindow);
        consensus.nPowMaxAdjustDown = 32; // 32% adjustment down
        consensus.nPowMaxAdjustUp = 16; // 16% adjustment up
        consensus.nPreBlossomPowTargetSpacing = Consensus::PRE_BLOSSOM_POW_TARGET_SPACING;
        consensus.nPostBlossomPowTargetSpacing = Consensus::POST_BLOSSOM_POW_TARGET_SPACING;
        consensus.nPowAllowMinDifficultyBlocksAfterHeight = 299187;
        consensus.fPowNoRetargeting = false;
        // Simplified consensus upgrade activation for this fork (same as mainnet)
        // All upgrades activate sequentially at blocks 1-8
        consensus.vUpgrades[Consensus::BASE_SPROUT].nProtocolVersion = 170002;
        consensus.vUpgrades[Consensus::BASE_SPROUT].nActivationHeight =
            Consensus::NetworkUpgrade::ALWAYS_ACTIVE;
        consensus.vUpgrades[Consensus::UPGRADE_TESTDUMMY].nProtocolVersion = 170002;
        consensus.vUpgrades[Consensus::UPGRADE_TESTDUMMY].nActivationHeight =
            Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT;
        consensus.vUpgrades[Consensus::UPGRADE_OVERWINTER].nProtocolVersion = 170003;
        consensus.vUpgrades[Consensus::UPGRADE_OVERWINTER].nActivationHeight = 1;
        consensus.vUpgrades[Consensus::UPGRADE_SAPLING].nProtocolVersion = 170007;
        consensus.vUpgrades[Consensus::UPGRADE_SAPLING].nActivationHeight = 2;
        consensus.vUpgrades[Consensus::UPGRADE_BLOSSOM].nProtocolVersion = 170008;
        consensus.vUpgrades[Consensus::UPGRADE_BLOSSOM].nActivationHeight = 3;
        consensus.vUpgrades[Consensus::UPGRADE_HEARTWOOD].nProtocolVersion = 170010;
        consensus.vUpgrades[Consensus::UPGRADE_HEARTWOOD].nActivationHeight = 4;
        consensus.vUpgrades[Consensus::UPGRADE_CANOPY].nProtocolVersion = 170012;
        consensus.vUpgrades[Consensus::UPGRADE_CANOPY].nActivationHeight = 5;
        consensus.vUpgrades[Consensus::UPGRADE_NU5].nProtocolVersion = 170050;
        consensus.vUpgrades[Consensus::UPGRADE_NU5].nActivationHeight = 6;
        consensus.vUpgrades[Consensus::UPGRADE_NU6].nProtocolVersion = 170110;
        consensus.vUpgrades[Consensus::UPGRADE_NU6].nActivationHeight = 7;
        consensus.vUpgrades[Consensus::UPGRADE_NU6_1].nProtocolVersion = 170130;
        consensus.vUpgrades[Consensus::UPGRADE_NU6_1].nActivationHeight = 8;
        consensus.vUpgrades[Consensus::UPGRADE_ZFUTURE].nProtocolVersion = 0x7FFFFFFF;
        consensus.vUpgrades[Consensus::UPGRADE_ZFUTURE].nActivationHeight =
            Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT;

        consensus.nFundingPeriodLength = consensus.nPostBlossomSubsidyHalvingInterval / 48;

        // guarantees the first 2 characters, when base58 encoded, are "tm"
        keyConstants.base58Prefixes[PUBKEY_ADDRESS]     = {0x1D,0x25};
        // guarantees the first 2 characters, when base58 encoded, are "t2"
        keyConstants.base58Prefixes[SCRIPT_ADDRESS]     = {0x1C,0xBA};
        // the first character, when base58 encoded, is "9" or "c" (as in Bitcoin)
        keyConstants.base58Prefixes[SECRET_KEY]         = {0xEF};
        // do not rely on these BIP32 prefixes; they are not specified and may change
        keyConstants.base58Prefixes[EXT_PUBLIC_KEY]     = {0x04,0x35,0x87,0xCF};
        keyConstants.base58Prefixes[EXT_SECRET_KEY]     = {0x04,0x35,0x83,0x94};
        // guarantees the first 2 characters, when base58 encoded, are "zt"
        keyConstants.base58Prefixes[ZCPAYMENT_ADDRESS]  = {0x16,0xB6};
        // guarantees the first 4 characters, when base58 encoded, are "ZiVt"
        keyConstants.base58Prefixes[ZCVIEWING_KEY]      = {0xA8,0xAC,0x0C};
        // guarantees the first 2 characters, when base58 encoded, are "ST"
        keyConstants.base58Prefixes[ZCSPENDING_KEY]     = {0xAC,0x08};

        keyConstants.bech32HRPs[SAPLING_PAYMENT_ADDRESS]      = "ztestsapling";
        keyConstants.bech32HRPs[SAPLING_FULL_VIEWING_KEY]     = "zviewtestsapling";
        keyConstants.bech32HRPs[SAPLING_INCOMING_VIEWING_KEY] = "zivktestsapling";
        keyConstants.bech32HRPs[SAPLING_EXTENDED_SPEND_KEY]   = "secret-extended-key-test";
        keyConstants.bech32HRPs[SAPLING_EXTENDED_FVK]         = "zxviewtestsapling";

        keyConstants.bech32mHRPs[TEX_ADDRESS]                 = "textest";

        // Testnet funding streams removed for this fork - 100% to miners
        /*
        {
            auto canopyActivation = consensus.vUpgrades[Consensus::UPGRADE_CANOPY].nActivationHeight;
            auto nu6Activation = consensus.vUpgrades[Consensus::UPGRADE_NU6].nActivationHeight;
            auto nu6_1Activation = consensus.vUpgrades[Consensus::UPGRADE_NU6_1].nActivationHeight;

            // ZIP 214 Revision 0
            std::vector<std::string> bp_addresses = {
                "t26ovBdKAJLtrvBsE2QGF4nqBkEuptuPFZz",
                "t26ovBdKAJLtrvBsE2QGF4nqBkEuptuPFZz",
                "t26ovBdKAJLtrvBsE2QGF4nqBkEuptuPFZz",
                "t26ovBdKAJLtrvBsE2QGF4nqBkEuptuPFZz",
                "t2NNHrgPpE388atmWSF4DxAb3xAoW5Yp45M",
                "t2VMN28itPyMeMHBEd9Z1hm6YLkQcGA1Wwe",
                "t2CHa1TtdfUV8UYhNm7oxbzRyfr8616BYh2",
                "t2F77xtr28U96Z2bC53ZEdTnQSUAyDuoa67",
                "t2ARrzhbgcpoVBDPivUuj6PzXzDkTBPqfcT",
                "t278aQ8XbvFR15mecRguiJDQQVRNnkU8kJw",
                "t2Dp1BGnZsrTXZoEWLyjHmg3EPvmwBnPDGB",
                "t2KzeqXgf4ju33hiSqCuKDb8iHjPCjMq9iL",
                "t2Nyxqv1BiWY1eUSiuxVw36oveawYuo18tr",
                "t2DKFk5JRsVoiuinK8Ti6eM4Yp7v8BbfTyH",
                "t2CUaBca4k1x36SC4q8Nc8eBoqkMpF3CaLg",
                "t296SiKL7L5wvFmEdMxVLz1oYgd6fTfcbZj",
                "t29fBCFbhgsjL3XYEZ1yk1TUh7eTusB6dPg",
                "t2FGofLJXa419A76Gpf5ncxQB4gQXiQMXjK",
                "t2ExfrnRVnRiXDvxerQ8nZbcUQvNvAJA6Qu",
                "t28JUffLp47eKPRHKvwSPzX27i9ow8LSXHx",
                "t2JXWPtrtyL861rFWMZVtm3yfgxAf4H7uPA",
                "t2QdgbJoWfYHgyvEDEZBjHmgkr9yNJff3Hi",
                "t2QW43nkco8r32ZGRN6iw6eSzyDjkMwCV3n",
                "t2DgYDXMJTYLwNcxighQ9RCgPxMVATRcUdC",
                "t2Bop7dg33HGZx3wunnQzi2R2ntfpjuti3M",
                "t2HVeEwovcLq9RstAbYkqngXNEsCe2vjJh9",
                "t2HxbP5keQSx7p592zWQ5bJ5GrMmGDsV2Xa",
                "t2TJzUg2matao3mztBRJoWnJY6ekUau6tPD",
                "t29pMzxmo6wod25YhswcjKv3AFRNiBZHuhj",
                "t2QBQMRiJKYjshJpE6RhbF7GLo51yE6d4wZ",
                "t2F5RqnqguzZeiLtYHFx4yYfy6pDnut7tw5",
                "t2CHvyZANE7XCtg8AhZnrcHCC7Ys1jJhK13",
                "t2BRzpMdrGWZJ2upsaNQv6fSbkbTy7EitLo",
                "t2BFixHGQMAWDY67LyTN514xRAB94iEjXp3",
                "t2Uvz1iVPzBEWfQBH1p7NZJsFhD74tKaG8V",
                "t2CmFDj5q6rJSRZeHf1SdrowinyMNcj438n",
                "t2ErNvWEReTfPDBaNizjMPVssz66aVZh1hZ",
                "t2GeJQ8wBUiHKDVzVM5ZtKfY5reCg7CnASs",
                "t2L2eFtkKv1G6j55kLytKXTGuir4raAy3yr",
                "t2EK2b87dpPazb7VvmEGc8iR6SJ289RywGL",
                "t2DJ7RKeZJxdA4nZn8hRGXE8NUyTzjujph9",
                "t2K1pXo4eByuWpKLkssyMLe8QKUbxnfFC3H",
                "t2TB4mbSpuAcCWkH94Leb27FnRxo16AEHDg",
                "t2Phx4gVL4YRnNsH3jM1M7jE4Fo329E66Na",
                "t2VQZGmeNomN8c3USefeLL9nmU6M8x8CVzC",
                "t2RicCvTVTY5y9JkreSRv3Xs8q2K67YxHLi",
                "t2JrSLxTGc8wtPDe9hwbaeUjCrCfc4iZnDD",
                "t2Uh9Au1PDDSw117sAbGivKREkmMxVC5tZo",
                "t2FDwoJKLeEBMTy3oP7RLQ1Fihhvz49a3Bv",
                "t2FY18mrgtb7QLeHA8ShnxLXuW8cNQ2n1v8",
                "t2L15TkDYum7dnQRBqfvWdRe8Yw3jVy9z7g",
            };

            // ZF and MG use the same address for each funding period
            std::vector<std::string> zf_addresses(51, "t27eWDgjFYJGVXmzrXeVjnb5J3uXDM9xH9v");
            std::vector<std::string> mg_addresses(51, "t2Gvxv2uNM7hbbACjNox4H6DjByoKZ2Fa3P");

            consensus.AddZIP207FundingStream(
                keyConstants,
                Consensus::FS_ZIP214_BP,
                canopyActivation,
                2796000, // *not* the NU6 activation height
                bp_addresses);
            consensus.AddZIP207FundingStream(
                keyConstants,
                Consensus::FS_ZIP214_ZF,
                canopyActivation,
                2796000, // *not* the NU6 activation height
                zf_addresses);
            consensus.AddZIP207FundingStream(
                keyConstants,
                Consensus::FS_ZIP214_MG,
                canopyActivation,
                2796000, // *not* the NU6 activation height
                mg_addresses);

            // ZIP 214 Revision 1
            // FPF uses a single address repeated 13 times, once for each funding period.
            // There are 13 periods because the start height does not align with a period boundary.
            std::vector<std::string> fpf_addresses(13, "t2HifwjUj9uyxr9bknR8LFuQbc98c3vkXtu");
            consensus.AddZIP207FundingStream(
                keyConstants,
                Consensus::FS_FPF_ZCG,
                nu6Activation,
                3396000,
                fpf_addresses);
            consensus.AddZIP207LockboxStream(
                keyConstants,
                Consensus::FS_DEFERRED,
                nu6Activation,
                3396000);

            // ZIP 214 Revision 2
            // FPF uses a single address repeated 27 times, once for each funding period.
            // There are 27 periods because the start height is after the second halving
            // on testnet and does not align with a period boundary.
            std::vector<std::string> fpf_addresses_h3(27, "t2HifwjUj9uyxr9bknR8LFuQbc98c3vkXtu");
            consensus.AddZIP207FundingStream(
                keyConstants,
                Consensus::FS_FPF_ZCG_H3,
                nu6_1Activation,
                4476000,
                fpf_addresses_h3);
            consensus.AddZIP207LockboxStream(
                keyConstants,
                Consensus::FS_CCF_H3,
                nu6_1Activation,
                4476000);

            // ZIP 271
            // For testing purposes, we split the lockbox contents into 10 equal chunks.
            std::string nu6_1_kho_address = "t2RnBRiqrN1nW4ecZs1Fj3WWjNdnSs4kiX8";
            static const CAmount nu6_1_disbursement_amount = 78750 * COIN;
            static const CAmount nu6_1_chunk_amount = 7875 * COIN;
            static constexpr auto nu6_1_chunks = {
                Consensus::LD_ZIP271_NU6_1_CHUNK_1,
                Consensus::LD_ZIP271_NU6_1_CHUNK_2,
                Consensus::LD_ZIP271_NU6_1_CHUNK_3,
                Consensus::LD_ZIP271_NU6_1_CHUNK_4,
                Consensus::LD_ZIP271_NU6_1_CHUNK_5,
                Consensus::LD_ZIP271_NU6_1_CHUNK_6,
                Consensus::LD_ZIP271_NU6_1_CHUNK_7,
                Consensus::LD_ZIP271_NU6_1_CHUNK_8,
                Consensus::LD_ZIP271_NU6_1_CHUNK_9,
                Consensus::LD_ZIP271_NU6_1_CHUNK_10,
            };
            static_assert(nu6_1_chunk_amount * nu6_1_chunks.size() == nu6_1_disbursement_amount);
            for (auto idx : nu6_1_chunks) {
                consensus.AddZIP271LockboxDisbursement(
                    keyConstants,
                    idx,
                    Consensus::UPGRADE_NU6_1,
                    nu6_1_chunk_amount,
                    nu6_1_kho_address);
            }
        }
        */

        // On testnet we activate this rule 6 blocks after Blossom activation. From block 299188 and
        // prior to Blossom activation, the testnet minimum-difficulty threshold was 15 minutes (i.e.
        // a minimum difficulty block can be mined if no block is mined normally within 15 minutes):
        // <https://zips.z.cash/zip-0205#change-to-difficulty-adjustment-on-testnet>
        // However the median-time-past is 6 blocks behind, and the worst-case time for 7 blocks at a
        // 15-minute spacing is ~105 minutes, which exceeds the limit imposed by the soft fork of
        // 90 minutes.
        //
        // After Blossom, the minimum difficulty threshold time is changed to 6 times the block target
        // spacing, which is 7.5 minutes:
        // <https://zips.z.cash/zip-0208#minimum-difficulty-blocks-on-the-test-network>
        // 7 times that is 52.5 minutes which is well within the limit imposed by the soft fork.

        static_assert(6 * Consensus::POST_BLOSSOM_POW_TARGET_SPACING * 7 < MAX_FUTURE_BLOCK_TIME_MTP - 60,
                      "MAX_FUTURE_BLOCK_TIME_MTP is too low given block target spacing");
        consensus.nFutureTimestampSoftForkHeight = consensus.vUpgrades[Consensus::UPGRADE_BLOSSOM].nActivationHeight + 6;

        // The best chain should have at least this much work.
        // Set to zero for new fork to allow mining from genesis without IBD
        consensus.nMinimumChainWork = uint256S("0x00");

        // New magic bytes for this fork testnet - derived from Bitcoin block 919122 hash
        pchMessageStart[0] = 0xa7;
        pchMessageStart[1] = 0x23;
        pchMessageStart[2] = 0xe1;
        pchMessageStart[3] = 0x6c;
        vAlertPubKey = ParseHex("044e7a1553392325c871c5ace5d6ad73501c66f4c185d6b0453cf45dec5a1322e705c672ac1a27ef7cdaf588c10effdf50ed5f95f85f2f54a5f6159fca394ed0c6");
        // New port for this fork testnet
        nDefaultPort = 18234;
        nPruneAfterHeight = 1000;

        // Testnet genesis timestamp - Bitcoin block 919122 as proof of post-mining
        const char* pszTimestamp = "bitcoin:919122:00000000000000000000de554a907650308b22427efb3735744099dea723e16c";

        genesis = CreateGenesisBlock(
            pszTimestamp,
            1760195959,
            uint256S("0x4f00000000000000000000000000000000000000000000000000000000000000"),
            ParseHex("64085e066034cdfb75d9c027943ea72dc7b6570be5b79fcca9b693be38609c00"),
            0x2000ffff, 4, 0);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(genesis.hashMerkleRoot == uint256S("0xd6ae25b7520285c517befe1931399d49436d323cf063cd94b067988266a539c9"));

        vFixedSeeds.clear();
        // Remove all Zcash DNS seeds - this is a separate network
        vSeeds.clear();
        vSeeds.push_back(CDNSSeedData("junomoneta.io", "testseeds.junomoneta.io"));

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        fMiningRequiresPeers = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;
        fTestnetToBeDeprecatedFieldRPC = true;


        checkpointData = (CCheckpointData) {
            boost::assign::map_list_of
            (0, consensus.hashGenesisBlock)
            (38000, uint256S("0x001e9a2d2e2892b88e9998cf7b079b41d59dd085423a921fe8386cecc42287b8")),
            1486897419,  // * UNIX timestamp of last checkpoint block
            47163,       // * total number of transactions between genesis and last checkpoint
            715          //   total number of tx / (checkpoint block height / (24 * 24))
        };

        // Hardcoded fallback value for the Sprout shielded value pool balance
        // for nodes that have not reindexed since the introduction of monitoring
        // in #2795.
        nSproutValuePoolCheckpointHeight = 440329;
        nSproutValuePoolCheckpointBalance = 40000029096803;
        fZIP209Enabled = true;
        hashSproutValuePoolCheckpointBlock = uint256S("000a95d08ba5dcbabe881fc6471d11807bcca7df5f1795c99f3ec4580db4279b");

        // Founders reward script expects a vector of 2-of-3 multisig addresses
        vFoundersRewardAddress = {
            "t2UNzUUx8mWBCRYPRezvA363EYXyEpHokyi", "t2N9PH9Wk9xjqYg9iin1Ua3aekJqfAtE543", "t2NGQjYMQhFndDHguvUw4wZdNdsssA6K7x2", "t2ENg7hHVqqs9JwU5cgjvSbxnT2a9USNfhy",
            "t2BkYdVCHzvTJJUTx4yZB8qeegD8QsPx8bo", "t2J8q1xH1EuigJ52MfExyyjYtN3VgvshKDf", "t2Crq9mydTm37kZokC68HzT6yez3t2FBnFj", "t2EaMPUiQ1kthqcP5UEkF42CAFKJqXCkXC9",
            "t2F9dtQc63JDDyrhnfpzvVYTJcr57MkqA12", "t2LPirmnfYSZc481GgZBa6xUGcoovfytBnC", "t26xfxoSw2UV9Pe5o3C8V4YybQD4SESfxtp", "t2D3k4fNdErd66YxtvXEdft9xuLoKD7CcVo",
            "t2DWYBkxKNivdmsMiivNJzutaQGqmoRjRnL", "t2C3kFF9iQRxfc4B9zgbWo4dQLLqzqjpuGQ", "t2MnT5tzu9HSKcppRyUNwoTp8MUueuSGNaB", "t2AREsWdoW1F8EQYsScsjkgqobmgrkKeUkK",
            "t2Vf4wKcJ3ZFtLj4jezUUKkwYR92BLHn5UT", "t2K3fdViH6R5tRuXLphKyoYXyZhyWGghDNY", "t2VEn3KiKyHSGyzd3nDw6ESWtaCQHwuv9WC", "t2F8XouqdNMq6zzEvxQXHV1TjwZRHwRg8gC",
            "t2BS7Mrbaef3fA4xrmkvDisFVXVrRBnZ6Qj", "t2FuSwoLCdBVPwdZuYoHrEzxAb9qy4qjbnL", "t2SX3U8NtrT6gz5Db1AtQCSGjrpptr8JC6h", "t2V51gZNSoJ5kRL74bf9YTtbZuv8Fcqx2FH",
            "t2FyTsLjjdm4jeVwir4xzj7FAkUidbr1b4R", "t2EYbGLekmpqHyn8UBF6kqpahrYm7D6N1Le", "t2NQTrStZHtJECNFT3dUBLYA9AErxPCmkka", "t2GSWZZJzoesYxfPTWXkFn5UaxjiYxGBU2a",
            "t2RpffkzyLRevGM3w9aWdqMX6bd8uuAK3vn", "t2JzjoQqnuXtTGSN7k7yk5keURBGvYofh1d", "t2AEefc72ieTnsXKmgK2bZNckiwvZe3oPNL", "t2NNs3ZGZFsNj2wvmVd8BSwSfvETgiLrD8J",
            "t2ECCQPVcxUCSSQopdNquguEPE14HsVfcUn", "t2JabDUkG8TaqVKYfqDJ3rqkVdHKp6hwXvG", "t2FGzW5Zdc8Cy98ZKmRygsVGi6oKcmYir9n", "t2DUD8a21FtEFn42oVLp5NGbogY13uyjy9t",
            "t2UjVSd3zheHPgAkuX8WQW2CiC9xHQ8EvWp", "t2TBUAhELyHUn8i6SXYsXz5Lmy7kDzA1uT5", "t2Tz3uCyhP6eizUWDc3bGH7XUC9GQsEyQNc", "t2NysJSZtLwMLWEJ6MH3BsxRh6h27mNcsSy",
            "t2KXJVVyyrjVxxSeazbY9ksGyft4qsXUNm9", "t2J9YYtH31cveiLZzjaE4AcuwVho6qjTNzp", "t2QgvW4sP9zaGpPMH1GRzy7cpydmuRfB4AZ", "t2NDTJP9MosKpyFPHJmfjc5pGCvAU58XGa4",
            "t29pHDBWq7qN4EjwSEHg8wEqYe9pkmVrtRP", "t2Ez9KM8VJLuArcxuEkNRAkhNvidKkzXcjJ", "t2D5y7J5fpXajLbGrMBQkFg2mFN8fo3n8cX", "t2UV2wr1PTaUiybpkV3FdSdGxUJeZdZztyt",
            };
        assert(vFoundersRewardAddress.size() <= consensus.GetLastFoundersRewardBlockHeight(0));

        // Developer fund address for optional donations (testnet)
        strDeveloperFundAddress = "t2UNzUUx8mWBCRYPRezvA363EYXyEpHokyi";
    }
};
static CTestNetParams testNetParams;

/**
 * Regression test
 */
class CRegTestParams : public CChainParams {
public:
    CRegTestParams() {
        keyConstants.strNetworkID = "regtest";
        strCurrencyUnits = "RJMR";
        keyConstants.bip44CoinType = 1;
        consensus.fCoinbaseMustBeShielded = false;
        consensus.nSubsidySlowStartInterval = 0;
        consensus.nPreBlossomSubsidyHalvingInterval = Consensus::PRE_BLOSSOM_REGTEST_HALVING_INTERVAL;
        consensus.nPostBlossomSubsidyHalvingInterval = POST_BLOSSOM_HALVING_INTERVAL(Consensus::PRE_BLOSSOM_REGTEST_HALVING_INTERVAL);
        consensus.nMajorityEnforceBlockUpgrade = 750;
        consensus.nMajorityRejectBlockOutdated = 950;
        consensus.nMajorityWindow = 1000;
        const size_t N = 48, K = 5;
        static_assert(equihash_parameters_acceptable(N, K));
        consensus.nEquihashN = N;
        consensus.nEquihashK = K;
        // Regtest: easiest powLimit that satisfies avgWindow=17 constraint
        // maxUint/powLimit must be >= 17, so powLimit <= maxUint/17
        // Maximum valid powLimit is 0x0f0f0f0f... (maxUint/17)
        consensus.powLimit = uint256S("0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f");
        consensus.nPowAveragingWindow = 17;
        assert(maxUint/UintToArith256(consensus.powLimit) >= consensus.nPowAveragingWindow);
        consensus.nPowMaxAdjustDown = 0; // Turn off adjustment down
        consensus.nPowMaxAdjustUp = 0; // Turn off adjustment up
        consensus.nPreBlossomPowTargetSpacing = Consensus::PRE_BLOSSOM_POW_TARGET_SPACING;
        consensus.nPostBlossomPowTargetSpacing = Consensus::POST_BLOSSOM_POW_TARGET_SPACING;
        consensus.nPowAllowMinDifficultyBlocksAfterHeight = 0;
        consensus.fPowNoRetargeting = true;
        // Simplified consensus upgrade activation for this fork (same as mainnet/testnet)
        // All upgrades activate sequentially at blocks 1-8
        consensus.vUpgrades[Consensus::BASE_SPROUT].nProtocolVersion = 170002;
        consensus.vUpgrades[Consensus::BASE_SPROUT].nActivationHeight =
            Consensus::NetworkUpgrade::ALWAYS_ACTIVE;
        consensus.vUpgrades[Consensus::UPGRADE_TESTDUMMY].nProtocolVersion = 170002;
        consensus.vUpgrades[Consensus::UPGRADE_TESTDUMMY].nActivationHeight =
            Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT;
        consensus.vUpgrades[Consensus::UPGRADE_OVERWINTER].nProtocolVersion = 170003;
        consensus.vUpgrades[Consensus::UPGRADE_OVERWINTER].nActivationHeight = 1;
        consensus.vUpgrades[Consensus::UPGRADE_SAPLING].nProtocolVersion = 170006;
        consensus.vUpgrades[Consensus::UPGRADE_SAPLING].nActivationHeight = 2;
        consensus.vUpgrades[Consensus::UPGRADE_BLOSSOM].nProtocolVersion = 170008;
        consensus.vUpgrades[Consensus::UPGRADE_BLOSSOM].nActivationHeight = 3;
        consensus.vUpgrades[Consensus::UPGRADE_HEARTWOOD].nProtocolVersion = 170010;
        consensus.vUpgrades[Consensus::UPGRADE_HEARTWOOD].nActivationHeight = 4;
        consensus.vUpgrades[Consensus::UPGRADE_CANOPY].nProtocolVersion = 170012;
        consensus.vUpgrades[Consensus::UPGRADE_CANOPY].nActivationHeight = 5;
        consensus.vUpgrades[Consensus::UPGRADE_NU5].nProtocolVersion = 170050;
        consensus.vUpgrades[Consensus::UPGRADE_NU5].nActivationHeight = 6;
        consensus.vUpgrades[Consensus::UPGRADE_NU6].nProtocolVersion = 170110;
        consensus.vUpgrades[Consensus::UPGRADE_NU6].nActivationHeight = 7;
        consensus.vUpgrades[Consensus::UPGRADE_NU6_1].nProtocolVersion = 170130;
        consensus.vUpgrades[Consensus::UPGRADE_NU6_1].nActivationHeight = 8;
        consensus.vUpgrades[Consensus::UPGRADE_ZFUTURE].nProtocolVersion = 0x7FFFFFFF;
        consensus.vUpgrades[Consensus::UPGRADE_ZFUTURE].nActivationHeight =
            Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT;

        consensus.nFundingPeriodLength = consensus.nPostBlossomSubsidyHalvingInterval / 48;
        // Defined funding streams can be enabled with node config flags.

        // These prefixes are the same as the testnet prefixes
        keyConstants.base58Prefixes[PUBKEY_ADDRESS]     = {0x1D,0x25};
        keyConstants.base58Prefixes[SCRIPT_ADDRESS]     = {0x1C,0xBA};
        keyConstants.base58Prefixes[SECRET_KEY]         = {0xEF};
        // do not rely on these BIP32 prefixes; they are not specified and may change
        keyConstants.base58Prefixes[EXT_PUBLIC_KEY]     = {0x04,0x35,0x87,0xCF};
        keyConstants.base58Prefixes[EXT_SECRET_KEY]     = {0x04,0x35,0x83,0x94};
        keyConstants.base58Prefixes[ZCPAYMENT_ADDRESS]  = {0x16,0xB6};
        keyConstants.base58Prefixes[ZCVIEWING_KEY]      = {0xA8,0xAC,0x0C};
        keyConstants.base58Prefixes[ZCSPENDING_KEY]     = {0xAC,0x08};

        keyConstants.bech32HRPs[SAPLING_PAYMENT_ADDRESS]      = "zregtestsapling";
        keyConstants.bech32HRPs[SAPLING_FULL_VIEWING_KEY]     = "zviewregtestsapling";
        keyConstants.bech32HRPs[SAPLING_INCOMING_VIEWING_KEY] = "zivkregtestsapling";
        keyConstants.bech32HRPs[SAPLING_EXTENDED_SPEND_KEY]   = "secret-extended-key-regtest";
        keyConstants.bech32HRPs[SAPLING_EXTENDED_FVK]         = "zxviewregtestsapling";

        keyConstants.bech32mHRPs[TEX_ADDRESS]                 = "texregtest";

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");

        // New magic bytes for this fork regtest - distinct from Zcash regtest
        pchMessageStart[0] = 0x81;
        pchMessageStart[1] = 0x1d;
        pchMessageStart[2] = 0x21;
        pchMessageStart[3] = 0xf6;
        // New port for this fork regtest
        nDefaultPort = 18345;
        nPruneAfterHeight = 1000;

        // Regtest genesis timestamp - simple timestamp for testing
        const char* pszTimestamp = "regtest";

        genesis = CreateGenesisBlock(
            pszTimestamp,
            1296688602,
            uint256S("0x0000000000000000000000000000000000000000000000000000000000000000"),
            ParseHex("37dcb7703915a625b7f1bfcdc82cf5422718f84ff0c0188513ff325cac9dd803"),
            0x200f0f0f, 4, 0);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(genesis.hashMerkleRoot == uint256S("0x7c60450f854b877c579f48710bce88f858adee1585a8574af45092b47144d024"));

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.

        fMiningRequiresPeers = false;
        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;
        fTestnetToBeDeprecatedFieldRPC = false;

        checkpointData = (CCheckpointData){
            boost::assign::map_list_of
            ( 0, uint256S("0f9188f13cb7b2c71f2a335e3a4fc328bf5beb436012afca590b1a11466e2206")),
            0,
            0,
            0
        };

        // Founders reward script expects a vector of 2-of-3 multisig addresses
        vFoundersRewardAddress = { "t2FwcEhFdNXuFMv1tcYwaBJtYVtMj8b1uTg" };
        assert(vFoundersRewardAddress.size() <= consensus.GetLastFoundersRewardBlockHeight(0));

        // Developer fund address for optional donations (regtest)
        strDeveloperFundAddress = "t2FwcEhFdNXuFMv1tcYwaBJtYVtMj8b1uTg";

        // do not require the wallet backup to be confirmed in regtest mode
        fRequireWalletBackup = false;
    }

    void UpdateNetworkUpgradeParameters(Consensus::UpgradeIndex idx, int nActivationHeight)
    {
        assert(idx > Consensus::BASE_SPROUT && idx < Consensus::MAX_NETWORK_UPGRADES);
        consensus.vUpgrades[idx].nActivationHeight = nActivationHeight;
    }

    void UpdateFundingStreamParameters(Consensus::FundingStreamIndex idx, Consensus::FundingStream fs)
    {
        assert(idx >= Consensus::FIRST_FUNDING_STREAM && idx < Consensus::MAX_FUNDING_STREAMS);
        consensus.vFundingStreams[idx] = fs;
    }

    void UpdateOnetimeLockboxDisbursementParameters(
        Consensus::OnetimeLockboxDisbursementIndex idx,
        Consensus::OnetimeLockboxDisbursement ld)
    {
        assert(idx >= Consensus::FIRST_ONETIME_LOCKBOX_DISBURSEMENT && idx < Consensus::MAX_ONETIME_LOCKBOX_DISBURSEMENTS);
        consensus.vOnetimeLockboxDisbursements[idx] = ld;
    }

    void UpdateRegtestPow(
        int64_t nPowMaxAdjustDown,
        int64_t nPowMaxAdjustUp,
        uint256 powLimit,
        bool noRetargeting)
    {
        consensus.nPowMaxAdjustDown = nPowMaxAdjustDown;
        consensus.nPowMaxAdjustUp = nPowMaxAdjustUp;
        consensus.powLimit = powLimit;
        consensus.fPowNoRetargeting = noRetargeting;
    }

    void SetRegTestZIP209Enabled() {
        fZIP209Enabled = true;
    }
};
static CRegTestParams regTestParams;

static const CChainParams* pCurrentParams = nullptr;

const CChainParams& Params() {
    assert(pCurrentParams);
    return *pCurrentParams;
}

const CChainParams& Params(const std::string& chain)
{
    if (chain == CBaseChainParams::MAIN)
            return mainParams;
    else if (chain == CBaseChainParams::TESTNET)
            return testNetParams;
    else if (chain == CBaseChainParams::REGTEST)
            return regTestParams;
    else
        throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
    SelectBaseParams(network);
    pCurrentParams = &Params(network);

    // Some python qa rpc tests need to enforce the coinbase consensus rule
    if (network == CBaseChainParams::REGTEST && mapArgs.count("-regtestshieldcoinbase")) {
        regTestParams.SetRegTestCoinbaseMustBeShielded();
    }

    // When a developer is debugging turnstile violations in regtest mode, enable ZIP209
    if (network == CBaseChainParams::REGTEST && mapArgs.count("-developersetpoolsizezero")) {
        regTestParams.SetRegTestZIP209Enabled();
    }
}


// Block height must be >0 and <=last founders reward block height
// Index variable i ranges from 0 - (vFoundersRewardAddress.size()-1)
std::string CChainParams::GetFoundersRewardAddressAtHeight(int nHeight) const {
    int preBlossomMaxHeight = consensus.GetLastFoundersRewardBlockHeight(0);
    // zip208
    // FounderAddressAdjustedHeight(height) :=
    // height, if not IsBlossomActivated(height)
    // BlossomActivationHeight + floor((height - BlossomActivationHeight) / BlossomPoWTargetSpacingRatio), otherwise
    bool blossomActive = consensus.NetworkUpgradeActive(nHeight, Consensus::UPGRADE_BLOSSOM);
    if (blossomActive) {
        int blossomActivationHeight = consensus.vUpgrades[Consensus::UPGRADE_BLOSSOM].nActivationHeight;
        nHeight = blossomActivationHeight + ((nHeight - blossomActivationHeight) / Consensus::BLOSSOM_POW_TARGET_SPACING_RATIO);
    }
    assert(nHeight > 0 && nHeight <= preBlossomMaxHeight);
    size_t addressChangeInterval = (preBlossomMaxHeight + vFoundersRewardAddress.size()) / vFoundersRewardAddress.size();
    size_t i = nHeight / addressChangeInterval;
    return vFoundersRewardAddress[i];
}

// Block height must be >0 and <=last founders reward block height
// The founders reward address is expected to be a multisig (P2SH) address
CScript CChainParams::GetFoundersRewardScriptAtHeight(int nHeight) const {
    assert(nHeight > 0 && nHeight <= consensus.GetLastFoundersRewardBlockHeight(nHeight));

    KeyIO keyIO(*this);
    auto address = keyIO.DecodePaymentAddress(GetFoundersRewardAddressAtHeight(nHeight).c_str());
    assert(address.has_value());
    assert(std::holds_alternative<CScriptID>(address.value()));
    CScriptID scriptID = std::get<CScriptID>(address.value());
    CScript script = CScript() << OP_HASH160 << ToByteVector(scriptID) << OP_EQUAL;
    return script;
}

std::string CChainParams::GetFoundersRewardAddressAtIndex(int i) const {
    assert(i >= 0 && i < vFoundersRewardAddress.size());
    return vFoundersRewardAddress[i];
}

std::string CChainParams::GetDefaultDeveloperAddress() const {
    return strDeveloperFundAddress;
}

CScript CChainParams::GetDeveloperFundScript() const {
    KeyIO keyIO(*this);
    auto address = keyIO.DecodePaymentAddress(strDeveloperFundAddress.c_str());
    assert(address.has_value());
    assert(std::holds_alternative<CScriptID>(address.value()));
    CScriptID scriptID = std::get<CScriptID>(address.value());
    CScript script = CScript() << OP_HASH160 << ToByteVector(scriptID) << OP_EQUAL;
    return script;
}

void UpdateNetworkUpgradeParameters(Consensus::UpgradeIndex idx, int nActivationHeight)
{
    regTestParams.UpdateNetworkUpgradeParameters(idx, nActivationHeight);
}

void UpdateFundingStreamParameters(Consensus::FundingStreamIndex idx, Consensus::FundingStream fs)
{
    regTestParams.UpdateFundingStreamParameters(idx, fs);
}

void UpdateOnetimeLockboxDisbursementParameters(
    Consensus::OnetimeLockboxDisbursementIndex idx,
    Consensus::OnetimeLockboxDisbursement ld)
{
    regTestParams.UpdateOnetimeLockboxDisbursementParameters(idx, ld);
}

void UpdateRegtestPow(
    int64_t nPowMaxAdjustDown,
    int64_t nPowMaxAdjustUp,
    uint256 powLimit,
    bool noRetargeting)
{
    regTestParams.UpdateRegtestPow(nPowMaxAdjustDown, nPowMaxAdjustUp, powLimit, noRetargeting);
}
