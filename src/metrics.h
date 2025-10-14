// Copyright (c) 2016-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_METRICS_H
#define ZCASH_METRICS_H

#include "uint256.h"
#include "consensus/params.h"

#include <atomic>
#include <mutex>
#include <optional>
#include <string>

struct AtomicCounter {
    std::atomic<uint64_t> value;

    AtomicCounter() : value {0} { }

    void increment(){
        ++value;
    }

    void decrement(){
        --value;
    }

    int get() const {
        return value.load();
    }
};

class AtomicTimer {
private:
    std::mutex mtx;
    uint64_t threads;
    int64_t start_time;
    int64_t total_time;

public:
    AtomicTimer() : threads(0), start_time(0), total_time(0) {}

    /**
     * Starts timing on first call, and counts the number of calls.
     */
    void start();

    /**
     * Counts number of calls, and stops timing after it has been called as
     * many times as start().
     */
    void stop();

    void zeroize();

    bool running();

    uint64_t threadCount();

    double rate(const AtomicCounter& count);
};

enum DurationFormat {
    FULL,
    REDUCED
};

extern AtomicCounter transactionsValidated;
extern AtomicCounter ehSolverRuns;
extern AtomicCounter solutionTargetChecks;
extern AtomicTimer miningTimer;
extern std::atomic<size_t> nSizeReindexed; // valid only during reindex
extern std::atomic<size_t> nFullSizeToReindex; // valid only during reindex

void TrackMinedBlock(uint256 hash);

void MarkStartTime();
double GetLocalSolPS();
int EstimateNetHeight(const Consensus::Params& params, int currentBlockHeight, int64_t currentBlockTime);
std::optional<int64_t> SecondsLeftToNextEpoch(const Consensus::Params& params, int currentHeight);
std::string DisplayDuration(int64_t time, DurationFormat format);
std::string DisplaySize(size_t value);
std::string DisplayHashRate(double value);

void TriggerRefresh();

void ConnectMetricsScreen();
void ThreadShowMetricsScreen();

/**
 * Juno Moneta - Roman goddess of money and mint
 * ASCII art representing wealth and prosperity
 */
const std::string METRICS_ART =
"                                        \n"
"          [1;33m___[0m      [1;37mJUNO[0m      [1;33m___[0m          \n"
"        [1;33m.'   `.[0m  [1;37mMONETA[0m  [1;33m.'   `.[0m        \n"
"       [1;33m/       \\[0m          [1;33m/       \\[0m       \n"
"      [1;33m|    [1;37mO[1;33m    |[0m        [1;33m|    [1;37mO[1;33m    |[0m      \n"
"      [1;33m|   [1;37mJMR[1;33m   |[0m        [1;33m|   [1;37mJMR[1;33m   |[0m      \n"
"       [1;33m\\       /[0m          [1;33m\\       /[0m       \n"
"        [1;33m`.___.´[0m            [1;33m`.___.´[0m        \n"
"                                        \n"
"           [1;36m____  ____[0m                  \n"
"          [1;36m/    \\/    \\[0m                 \n"
"         [1;36m|    [1;37mO[1;36m      |[0m                \n"
"         [1;36m|   [1;37mJMR[1;36m    |[0m                \n"
"         [1;36m|          |[0m   [1;33mWealth[0m         \n"
"          [1;36m\\        /[0m   [1;33mfor All[0m         \n"
"           [1;36m`.____.´[0m                    \n"
"                                        \n"
"      [1;90m~[0m [1;35mRandomX[0m [1;32mPrivacy[0m [1;33mMoney[0m [1;90m~[0m      \n"
"                                        ";


#endif // ZCASH_METRICS_H
