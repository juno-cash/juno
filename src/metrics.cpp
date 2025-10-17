// Copyright (c) 2016-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "metrics.h"

#include "chainparams.h"
#include "checkpoints.h"
#include "main.h"
#include "miner.h"
#include "rpc/server.h"
#include "timedata.h"
#include "ui_interface.h"
#include "util/system.h"
#include "util/time.h"
#include "util/moneystr.h"
#include "util/strencodings.h"
#include "wallet/wallet.h"

#include <boost/range/irange.hpp>
#include <boost/thread.hpp>
#include <boost/thread/synchronized_value.hpp>

#include <optional>
#include <string>
#include <iostream>
#include <limits>
#ifdef WIN32
#include <io.h>
#include <wincon.h>
#include <conio.h>
#else
#include <sys/ioctl.h>
#include <poll.h>
#include <termios.h>
#endif
#include <unistd.h>

void AtomicTimer::start()
{
    std::unique_lock<std::mutex> lock(mtx);
    if (threads < 1) {
        start_time = GetTime();
    }
    ++threads;
}

void AtomicTimer::stop()
{
    std::unique_lock<std::mutex> lock(mtx);
    // Ignore excess calls to stop()
    if (threads > 0) {
        --threads;
        if (threads < 1) {
            int64_t time_span = GetTime() - start_time;
            total_time += time_span;
        }
    }
}


void AtomicTimer::zeroize()
{
    std::unique_lock<std::mutex> lock(mtx);
    // only zeroize it if there's no more threads (same semantics as start())
    if (threads < 1) {
        start_time = 0;
        total_time = 0;
    }
}

bool AtomicTimer::running()
{
    std::unique_lock<std::mutex> lock(mtx);
    return threads > 0;
}

uint64_t AtomicTimer::threadCount()
{
    std::unique_lock<std::mutex> lock(mtx);
    return threads;
}

double AtomicTimer::rate(const AtomicCounter& count)
{
    std::unique_lock<std::mutex> lock(mtx);
    int64_t duration = total_time;
    if (threads > 0) {
        // Timer is running, so get the latest count
        duration += GetTime() - start_time;
    }
    return duration > 0 ? (double)count.get() / duration : 0;
}

static CCriticalSection cs_metrics;

static boost::synchronized_value<int64_t> nNodeStartTime;
static boost::synchronized_value<int64_t> nNextRefresh;
AtomicCounter transactionsValidated;
AtomicCounter ehSolverRuns;
AtomicCounter solutionTargetChecks;
static AtomicCounter minedBlocks;
AtomicTimer miningTimer;
std::atomic<size_t> nSizeReindexed(0);   // valid only during reindex
std::atomic<size_t> nFullSizeToReindex(1);   // valid only during reindex

static boost::synchronized_value<std::list<uint256>> trackedBlocks;

static boost::synchronized_value<std::list<std::string>> messageBox;
static boost::synchronized_value<std::string> initMessage;
static bool loaded = false;

extern int64_t GetNetworkHashPS(int lookup, int height);

void TrackMinedBlock(uint256 hash)
{
    LOCK(cs_metrics);
    minedBlocks.increment();
    trackedBlocks->push_back(hash);
}

void MarkStartTime()
{
    *nNodeStartTime = GetTime();
}

int64_t GetUptime()
{
    return GetTime() - *nNodeStartTime;
}

double GetLocalSolPS()
{
    return miningTimer.rate(solutionTargetChecks);
}

std::string WhichNetwork()
{
    if (GetBoolArg("-regtest", false))
        return "regtest";
    if (GetBoolArg("-testnet", false))
        return "testnet";
    return "mainnet";
}

int EstimateNetHeight(const Consensus::Params& params, int currentHeadersHeight, int64_t currentHeadersTime)
{
    int64_t now = GetTime();
    if (currentHeadersTime >= now) {
        return currentHeadersHeight;
    }

    int estimatedHeight = currentHeadersHeight + (now - currentHeadersTime) / params.PoWTargetSpacing(currentHeadersHeight);

    int blossomActivationHeight = params.vUpgrades[Consensus::UPGRADE_BLOSSOM].nActivationHeight;
    if (currentHeadersHeight >= blossomActivationHeight || estimatedHeight <= blossomActivationHeight) {
        return ((estimatedHeight + 5) / 10) * 10;
    }

    int numPreBlossomBlocks = blossomActivationHeight - currentHeadersHeight;
    int64_t preBlossomTime = numPreBlossomBlocks * params.PoWTargetSpacing(blossomActivationHeight - 1);
    int64_t blossomActivationTime = currentHeadersTime + preBlossomTime;
    if (blossomActivationTime >= now) {
        return blossomActivationHeight;
    }

    int netheight =  blossomActivationHeight + (now - blossomActivationTime) / params.PoWTargetSpacing(blossomActivationHeight);
    return ((netheight + 5) / 10) * 10;
}

void TriggerRefresh()
{
    *nNextRefresh = GetTime();
    // Ensure that the refresh has started before we return
    MilliSleep(200);
}

static bool metrics_ThreadSafeMessageBox(const std::string& message,
                                      const std::string& caption,
                                      unsigned int style)
{
    // The SECURE flag has no effect in the metrics UI.
    style &= ~CClientUIInterface::SECURE;

    std::string strCaption;
    // Check for usage of predefined caption
    switch (style) {
    case CClientUIInterface::MSG_ERROR:
        strCaption += _("Error");
        break;
    case CClientUIInterface::MSG_WARNING:
        strCaption += _("Warning");
        break;
    case CClientUIInterface::MSG_INFORMATION:
        strCaption += _("Information");
        break;
    default:
        strCaption += caption; // Use supplied caption (can be empty)
    }

    boost::strict_lock_ptr<std::list<std::string>> u = messageBox.synchronize();
    u->push_back(strCaption + ": " + message);
    if (u->size() > 5) {
        u->pop_back();
    }

    TriggerRefresh();
    return false;
}

static bool metrics_ThreadSafeQuestion(const std::string& /* ignored interactive message */, const std::string& message, const std::string& caption, unsigned int style)
{
    return metrics_ThreadSafeMessageBox(message, caption, style);
}

static void metrics_InitMessage(const std::string& message)
{
    *initMessage = message;
}

void ConnectMetricsScreen()
{
    uiInterface.ThreadSafeMessageBox.disconnect_all_slots();
    uiInterface.ThreadSafeMessageBox.connect(metrics_ThreadSafeMessageBox);
    uiInterface.ThreadSafeQuestion.disconnect_all_slots();
    uiInterface.ThreadSafeQuestion.connect(metrics_ThreadSafeQuestion);
    uiInterface.InitMessage.disconnect_all_slots();
    uiInterface.InitMessage.connect(metrics_InitMessage);
}

std::string DisplayDuration(int64_t duration, DurationFormat format)
{
    int64_t days =  duration / (24 * 60 * 60);
    int64_t hours = (duration - (days * 24 * 60 * 60)) / (60 * 60);
    int64_t minutes = (duration - (((days * 24) + hours) * 60 * 60)) / 60;
    int64_t seconds = duration - (((((days * 24) + hours) * 60) + minutes) * 60);

    std::string strDuration;
    if (format == DurationFormat::REDUCED) {
        if (days > 0) {
            strDuration = strprintf(_("%d days"), days);
        } else if (hours > 0) {
            strDuration = strprintf(_("%d hours"), hours);
        } else if (minutes > 0) {
            strDuration = strprintf(_("%d minutes"), minutes);
        } else {
            strDuration = strprintf(_("%d seconds"), seconds);
        }
    } else {
        if (days > 0) {
            strDuration = strprintf(_("%d days, %d hours, %d minutes, %d seconds"), days, hours, minutes, seconds);
        } else if (hours > 0) {
            strDuration = strprintf(_("%d hours, %d minutes, %d seconds"), hours, minutes, seconds);
        } else if (minutes > 0) {
            strDuration = strprintf(_("%d minutes, %d seconds"), minutes, seconds);
        } else {
            strDuration = strprintf(_("%d seconds"), seconds);
        }
    }
    return strDuration;
}

std::string DisplaySize(size_t value)
{
    double coef = 1.0;
    if (value < 1024.0 * coef)
        return strprintf(_("%d Bytes"), value);
    coef *= 1024.0;
    if (value < 1024.0 * coef)
        return strprintf(_("%.2f KiB"), value / coef);
    coef *= 1024.0;
    if (value < 1024.0 * coef)
        return strprintf(_("%.2f MiB"), value / coef);
    coef *= 1024.0;
    if (value < 1024.0 * coef)
        return strprintf(_("%.2f GiB"), value / coef);
    coef *= 1024.0;
    return strprintf(_("%.2f TiB"), value / coef);
}

std::string DisplayHashRate(double value)
{
    double coef = 1.0;
    if (value < 1000.0 * coef)
        return strprintf(_("%.3f H/s"), value);
    coef *= 1000.0;
    if (value < 1000.0 * coef)
        return strprintf(_("%.3f kH/s"), value / coef);
    coef *= 1000.0;
    if (value < 1000.0 * coef)
        return strprintf(_("%.3f MH/s"), value / coef);
    coef *= 1000.0;
    if (value < 1000.0 * coef)
        return strprintf(_("%.3f GH/s"), value / coef);
    coef *= 1000.0;
    return strprintf(_("%.3f TH/s"), value / coef);
}

std::optional<int64_t> SecondsLeftToNextEpoch(const Consensus::Params& params, int currentHeight)
{
    auto nextHeight = NextActivationHeight(currentHeight, params);
    if (nextHeight) {
        return (nextHeight.value() - currentHeight) * params.PoWTargetSpacing(nextHeight.value() - 1);
    } else {
        return std::nullopt;
    }
}

struct MetricsStats {
    int height;
    int64_t currentHeadersHeight;
    int64_t currentHeadersTime;
    size_t connections;
    int64_t netsolps;
};

MetricsStats loadStats()
{
    int height;
    int64_t currentHeadersHeight;
    int64_t currentHeadersTime;
    size_t connections;
    int64_t netsolps;

    {
        LOCK(cs_main);
        height = chainActive.Height();
        currentHeadersHeight = pindexBestHeader ? pindexBestHeader->nHeight: -1;
        currentHeadersTime = pindexBestHeader ? pindexBestHeader->nTime : 0;
        netsolps = GetNetworkHashPS(120, -1);
    }
    {
        LOCK(cs_vNodes);
        connections = vNodes.size();
    }

    return MetricsStats {
        height,
        currentHeadersHeight,
        currentHeadersTime,
        connections,
        netsolps
    };
}

// ============================================================================
// Beautiful UI Helper Functions
// ============================================================================

// Calculate visible length of string (excluding ANSI escape codes)
// Counts UTF-8 characters, not bytes
static size_t visibleLength(const std::string& str) {
    size_t len = 0;
    bool inEscape = false;
    for (size_t i = 0; i < str.length(); ) {
        unsigned char c = str[i];

        if (c == '\e') {
            inEscape = true;
            i++;
        } else if (inEscape && c == 'm') {
            inEscape = false;
            i++;
        } else if (!inEscape) {
            // Count this as one character and skip UTF-8 continuation bytes
            len++;
            // UTF-8: if byte starts with 11xxxxxx, count following 10xxxxxx bytes
            if ((c & 0x80) == 0) {
                i++; // ASCII (0xxxxxxx)
            } else if ((c & 0xE0) == 0xC0) {
                i += 2; // 2-byte UTF-8 (110xxxxx 10xxxxxx)
            } else if ((c & 0xF0) == 0xE0) {
                i += 3; // 3-byte UTF-8 (1110xxxx 10xxxxxx 10xxxxxx)
            } else if ((c & 0xF8) == 0xF0) {
                i += 4; // 4-byte UTF-8 (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
            } else {
                i++; // Invalid UTF-8, just skip
            }
        } else {
            i++;
        }
    }
    return len;
}

// Draw a horizontal line with optional title
static void drawLine(const std::string& title = "", const std::string& left = "├", const std::string& right = "┤", const std::string& fill = "─", int width = 72) {
    std::cout << left;
    if (!title.empty()) {
        int titleLen = title.length() + 2; // +2 for spaces
        int leftPad = (width - titleLen) / 2;
        int rightPad = width - titleLen - leftPad;
        for (int i = 0; i < leftPad; i++) std::cout << fill;
        std::cout << " \e[1;37m" << title << "\e[0m ";
        for (int i = 0; i < rightPad; i++) std::cout << fill;
    } else {
        for (int i = 0; i < width; i++) std::cout << fill;
    }
    std::cout << right << std::endl;
}

// Draw top border of box
static void drawBoxTop(const std::string& title = "", int width = 72) {
    drawLine(title, "┌", "┐", "─", width);
}

// Draw bottom border of box
static void drawBoxBottom(int width = 72) {
    drawLine("", "└", "┘", "─", width);
}

// Draw a data row inside a box with label and value
static void drawRow(const std::string& label, const std::string& value, int width = 72) {
    int labelLen = visibleLength(label);
    int valueLen = visibleLength(value);
    int padding = width - labelLen - valueLen - 2; // -2 for the two spaces (after | and before |)

    std::cout << "│ \e[1;36m" << label << "\e[0m";
    for (int i = 0; i < padding; i++) std::cout << " ";
    std::cout << "\e[1;33m" << value << "\e[0m │" << std::endl;
}

// Draw a centered text line in a box
static void drawCentered(const std::string& text, const std::string& color = "", int width = 72) {
    int textLen = visibleLength(text);
    int padding = (width - textLen) / 2;
    int rightPad = width - textLen - padding;

    std::cout << "│";
    for (int i = 0; i < padding; i++) std::cout << " ";
    if (!color.empty()) std::cout << color;
    std::cout << text;
    if (!color.empty()) std::cout << "\e[0m";
    for (int i = 0; i < rightPad; i++) std::cout << " ";
    std::cout << "│" << std::endl;
}

// Draw a progress bar
static void drawProgressBar(int percent, int width = 68) {
    int filled = (percent * width) / 100;
    std::cout << "│ \e[1;32m";
    for (int i = 0; i < filled; i++) std::cout << "█";
    std::cout << "\e[0;32m";
    for (int i = filled; i < width; i++) std::cout << "░";
    std::cout << "\e[0m │" << std::endl;
}

int printStats(MetricsStats stats, bool isScreen, bool mining)
{
    int lines = 0;
    const Consensus::Params& params = Params().GetConsensus();
    auto localsolps = GetLocalSolPS();

    // Network Status Box
    drawBoxTop("NETWORK STATUS");
    lines++;

    // Syncing or synced status
    if (IsInitialBlockDownload(Params().GetConsensus())) {
        if (fReindex) {
            int downloadPercent = nSizeReindexed * 100 / nFullSizeToReindex;
            drawRow("Status", strprintf("Reindexing (%d%%)", downloadPercent));
            lines++;
            drawRow("Progress", strprintf("%s / %s",
                DisplaySize(nSizeReindexed), DisplaySize(nFullSizeToReindex)));
            lines++;
            drawRow("Blocks", strprintf("%d", stats.height));
            lines++;

            if (isScreen) {
                drawProgressBar(downloadPercent);
                lines++;
            }
        } else {
            int nHeaders = stats.currentHeadersHeight < 0 ? 0 : stats.currentHeadersHeight;
            int netheight = stats.currentHeadersHeight == -1 || stats.currentHeadersTime == 0 ?
                0 : EstimateNetHeight(params, stats.currentHeadersHeight, stats.currentHeadersTime);
            if (netheight < nHeaders) netheight = nHeaders;
            if (netheight <= 0) netheight = 1;
            int downloadPercent = stats.height * 100 / netheight;

            drawRow("Status", strprintf("\e[1;33mSYNCING\e[0m (%d%%)", downloadPercent));
            lines++;
            drawRow("Block Height", strprintf("%d / %d", stats.height, netheight));
            lines++;

            // Network Difficulty
            double difficulty = GetNetworkDifficulty(chainActive.Tip());
            drawRow("Network Difficulty", strprintf("%.6f", difficulty));
            lines++;

            if (isScreen) {
                drawProgressBar(downloadPercent);
                lines++;
            }
        }
    } else {
        drawRow("Status", "\e[1;32m● SYNCHRONIZED\e[0m");
        lines++;
        drawRow("Block Height", strprintf("%d", stats.height));
        lines++;
    }

    // Network Difficulty
    double difficulty = GetNetworkDifficulty(chainActive.Tip());
    drawRow("Network Difficulty", strprintf("%.6f", difficulty));
    lines++;

    // Network info
    auto secondsLeft = SecondsLeftToNextEpoch(params, stats.height);
    if (secondsLeft) {
        auto nextHeight = NextActivationHeight(stats.height, params).value();
        auto nextBranch = NextEpoch(stats.height, params).value();
        drawRow("Next Upgrade", strprintf("%s at %d (~%s)",
            NetworkUpgradeInfo[nextBranch].strName, nextHeight,
            DisplayDuration(secondsLeft.value(), DurationFormat::REDUCED)));
    } else {
        drawRow("Next Upgrade", "None scheduled");
    }
    lines++;

    drawRow("Connections", strprintf("%d", stats.connections));
    lines++;
    drawRow("Network Hash", DisplayHashRate(stats.netsolps));
    lines++;

    if (mining && miningTimer.running()) {
        drawRow("Your Hash Rate", DisplayHashRate(localsolps));
        lines++;
    }

    drawBoxBottom();
    lines++;
    std::cout << std::endl;
    lines++;

    return lines;
}


// Forward declarations for donation helper functions
static int getCurrentDonationPercentage();
static std::string getCurrentDonationAddress();

int printMiningStatus(bool mining)
{
#ifdef ENABLE_MINING
    int lines = 0;

    // Mining Status Box
    drawBoxTop("MINING");
    lines++;

    if (mining) {
        auto nThreads = miningTimer.threadCount();
        if (nThreads > 0) {
            drawRow("Status", strprintf("\e[1;32m● ACTIVE\e[0m - %d threads", nThreads));
            lines++;

            // Show block reward
            int nHeight = chainActive.Height() + 1; // Next block to be mined
            CAmount blockReward = Params().GetConsensus().GetBlockSubsidy(nHeight);
            drawRow("Block Reward", FormatMoney(blockReward));
            lines++;

            // Show blocks mined
            int blocksMined = minedBlocks.get();
            if (blocksMined > 0) {
                drawRow("Blocks Mined", strprintf("%d", blocksMined));
                lines++;
            }
        } else {
            bool fvNodesEmpty;
            {
                LOCK(cs_vNodes);
                fvNodesEmpty = vNodes.empty();
            }
            if (fvNodesEmpty) {
                drawRow("Status", "\e[1;33m○ PAUSED\e[0m - Waiting for connections");
            } else if (IsInitialBlockDownload(Params().GetConsensus())) {
                drawRow("Status", "\e[1;33m○ PAUSED\e[0m - Downloading blocks");
            } else {
                drawRow("Status", "\e[1;33m○ PAUSED\e[0m - Processing");
            }
            lines++;
        }

        // Show donation status if active
        int donationPct = getCurrentDonationPercentage();
        if (donationPct > 0) {
            std::string donationAddr = getCurrentDonationAddress();
            std::string shortAddr = donationAddr.substr(0, 10) + "..." + donationAddr.substr(donationAddr.length() - 6);
            drawRow("Donations", strprintf("\e[1;35m%d%%\e[0m → %s", donationPct, shortAddr.c_str()));
            lines++;
        }
    } else {
        drawRow("Status", "\e[1;31m○ INACTIVE\e[0m");
        lines++;
    }

    drawBoxBottom();
    lines++;
    std::cout << std::endl;
    lines++;

    // Controls Box
    drawBoxTop("CONTROLS");
    lines++;

    if (mining) {
        // Get current thread count
        int nThreads = GetArg("-genproclimit", 1);

        std::string controls = strprintf("\e[1;37m[M]\e[0m Mining: \e[1;32mON\e[0m  \e[1;37m[T]\e[0m Threads: %d", nThreads);

        int donationPct = getCurrentDonationPercentage();
        if (donationPct > 0) {
            // Donations are ON, show current state and controls
            controls += strprintf("  \e[1;37m[D]\e[0m Donations: \e[1;35mON (%d%%)\e[0m  \e[1;37m[P]\e[0m Change %%", donationPct);
        } else {
            // Donations are OFF, show current state
            controls += "  \e[1;37m[D]\e[0m Donations: \e[1;31mOFF\e[0m";
        }
        drawCentered(controls);
    } else {
        drawCentered("\e[1;37m[M]\e[0m Mining: \e[1;31mOFF\e[0m");
    }
    lines++;

    drawBoxBottom();
    lines++;

    return lines;
#else // ENABLE_MINING
    return 0;
#endif // !ENABLE_MINING
}


int printMetrics(size_t cols, bool mining)
{
    // Number of lines that are always displayed
    int lines = 2;

    // Calculate and display uptime
    std::string duration = DisplayDuration(GetUptime(), DurationFormat::FULL);

    std::string strDuration = strprintf(_("Uptime: %s"), duration);
    std::cout << strDuration << std::endl;
    lines += (strDuration.size() / cols);

    if (mining && loaded) {
        std::cout << "- " << strprintf(_("You have completed %d RandomX hashes."), ehSolverRuns.get()) << std::endl;
        lines++;

        int mined = 0;
        int orphaned = 0;
        CAmount immature {0};
        CAmount mature {0};
        {
            LOCK2(cs_main, cs_metrics);
            boost::strict_lock_ptr<std::list<uint256>> u = trackedBlocks.synchronize();

            // Update orphaned block count
            std::list<uint256>::iterator it = u->begin();
            while (it != u->end()) {
                auto hash = *it;
                if (mapBlockIndex.count(hash) > 0 &&
                        chainActive.Contains(mapBlockIndex[hash])) {
                    it++;
                } else {
                    it = u->erase(it);
                }
            }

            mined = minedBlocks.get();
            orphaned = mined - u->size();
        }

        // Get balance from wallet API
        if (pwalletMain) {
            immature = pwalletMain->GetImmatureBalance(std::nullopt);
            mature = pwalletMain->GetBalance(std::nullopt);
        }

        if (mined > 0) {
            std::string units = Params().CurrencyUnits();
            std::cout << "- " << strprintf(_("You have mined %d blocks!"), mined) << std::endl;
            std::cout << "  "
                      << strprintf(_("Orphaned: %d blocks, Immature: %u %s, Mature: %u %s"),
                                     orphaned,
                                     FormatMoney(immature), units,
                                     FormatMoney(mature), units)
                      << std::endl;
            lines += 2;
        }
    }
    std::cout << std::endl;

    return lines;
}

int printMessageBox(size_t cols)
{
    boost::strict_lock_ptr<std::list<std::string>> u = messageBox.synchronize();

    if (u->size() == 0) {
        return 0;
    }

    int lines = 2 + u->size();
    std::cout << _("Messages:") << std::endl;
    for (auto it = u->cbegin(); it != u->cend(); ++it) {
        auto msg = FormatParagraph(*it, cols, 2);
        std::cout << "- " << msg << std::endl;
        // Handle newlines and wrapped lines
        size_t i = 0;
        size_t j = 0;
        while (j < msg.size()) {
            i = msg.find('\n', j);
            if (i == std::string::npos) {
                i = msg.size();
            } else {
                // Newline
                lines++;
            }
            j = i + 1;
        }
    }
    std::cout << std::endl;
    return lines;
}

int printInitMessage()
{
    if (loaded) {
        return 0;
    }

    std::string msg = *initMessage;
    std::cout << _("Node is starting up:") << " " << msg << std::endl;
    std::cout << std::endl;

    if (msg == _("Done loading")) {
        loaded = true;
    }

    return 2;
}

#ifdef WIN32
bool enableVTMode()
{
    // Set output mode to handle virtual terminal sequences
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) {
        return false;
    }

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hOut, dwMode)) {
        return false;
    }
    return true;
}
#endif

// Helper function to check for keyboard input without blocking
static int checkKeyPress()
{
#ifdef WIN32
    if (_kbhit()) {
        return _getch();
    }
    return 0;
#else
    struct pollfd fds;
    fds.fd = STDIN_FILENO;
    fds.events = POLLIN;

    int ret = poll(&fds, 1, 0);  // 0 timeout = non-blocking
    if (ret > 0 && (fds.revents & POLLIN)) {
        char c;
        if (read(STDIN_FILENO, &c, 1) == 1) {
            return c;
        }
    }
    return 0;
#endif
}

// Terminal mode management for input prompts
#ifndef WIN32
static struct termios orig_termios;
static bool termios_saved = false;

static void disableRawMode()
{
    if (termios_saved) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    }
}

static void enableRawMode()
{
    if (!termios_saved) {
        tcgetattr(STDIN_FILENO, &orig_termios);
        termios_saved = true;
        atexit(disableRawMode);
    }

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);  // Disable canonical mode and echo
    raw.c_cc[VMIN] = 0;   // Non-blocking read
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void enableCanonicalMode()
{
    if (termios_saved) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    }
}
#endif

// Get current donation percentage
static int getCurrentDonationPercentage()
{
    return GetArg("-donationpercentage", 0);
}

// Get current donation address
static std::string getCurrentDonationAddress()
{
    std::string addr = GetArg("-donationaddress", "");
    if (addr.empty()) {
        addr = Params().GetDefaultDeveloperAddress();
    }
    return addr;
}

// Update donation percentage
static void updateDonationPercentage(int percentage)
{
    if (percentage < 0 || percentage > 100) {
        return;  // Invalid range
    }

    mapArgs["-donationpercentage"] = itostr(percentage);

    if (percentage > 0) {
        std::string devAddress = getCurrentDonationAddress();
        LogPrintf("User set donation to %d%% (address: %s)\n", percentage, devAddress);
    } else {
        LogPrintf("User disabled donations\n");
    }
}

// Toggle donation on/off
static void toggleDonation()
{
    int current = getCurrentDonationPercentage();
    if (current > 0) {
        // Turn off
        updateDonationPercentage(0);
    } else {
        // Turn on with default 5%
        updateDonationPercentage(5);
    }
}

// Prompt user for donation percentage
static void promptForPercentage()
{
#ifndef WIN32
    enableCanonicalMode();
#endif

    // Clear the input area and show prompt
    std::cout << "\n\e[K";  // Clear line
    std::cout << "Enter donation percentage (0-100): " << std::flush;

    std::string input;
    std::getline(std::cin, input);

    try {
        int percentage = std::stoi(input);
        if (percentage >= 0 && percentage <= 100) {
            updateDonationPercentage(percentage);
            if (percentage == 0) {
                std::cout << "Donations disabled." << std::endl;
            } else {
                std::cout << "Donation set to " << percentage << "%" << std::endl;
            }
        } else {
            std::cout << "Invalid percentage. Must be between 0 and 100." << std::endl;
        }
    } catch (...) {
        std::cout << "Invalid input. Please enter a number." << std::endl;
    }

    MilliSleep(1500);  // Give user time to see the message

#ifndef WIN32
    enableRawMode();
#endif
}

// Toggle mining on/off
static void toggleMining()
{
    bool currentlyMining = GetBoolArg("-gen", false);
    mapArgs["-gen"] = currentlyMining ? "0" : "1";

    int nThreads = GetArg("-genproclimit", 1);
    GenerateBitcoins(!currentlyMining, nThreads, Params());

    if (!currentlyMining) {
        LogPrintf("User enabled mining with %d threads\n", nThreads);
    } else {
        LogPrintf("User disabled mining\n");
    }
}

// Prompt user for number of mining threads
static void promptForThreads()
{
#ifndef WIN32
    enableCanonicalMode();
#endif

    // Clear the input area and show prompt
    std::cout << "\n\e[K";  // Clear line
    std::cout << "Enter number of mining threads (1-" << boost::thread::hardware_concurrency() << ", or -1 for all cores): " << std::flush;

    std::string input;
    std::getline(std::cin, input);

    try {
        int threads = std::stoi(input);
        int maxThreads = boost::thread::hardware_concurrency();

        if (threads == -1) {
            threads = maxThreads;
        }

        if (threads >= 1 && threads <= maxThreads) {
            mapArgs["-genproclimit"] = itostr(threads);

            // Restart mining with new thread count if currently mining
            bool currentlyMining = GetBoolArg("-gen", false);
            if (currentlyMining) {
                GenerateBitcoins(true, threads, Params());
                LogPrintf("User set mining threads to %d (mining restarted)\n", threads);
            } else {
                LogPrintf("User set mining threads to %d (will apply when mining starts)\n", threads);
            }
            std::cout << "Mining threads set to " << threads << std::endl;
        } else {
            std::cout << "Invalid thread count. Must be between 1 and " << maxThreads << " (or -1 for all cores)." << std::endl;
        }
    } catch (...) {
        std::cout << "Invalid input. Please enter a number." << std::endl;
    }

    MilliSleep(1500);  // Give user time to see the message

#ifndef WIN32
    enableRawMode();
#endif
}

void ThreadShowMetricsScreen()
{
    // Determine whether we should render a persistent UI or rolling metrics
    bool isTTY = isatty(STDOUT_FILENO);
    bool isScreen = GetBoolArg("-metricsui", isTTY);
    int64_t nRefresh = GetArg("-metricsrefreshtime", isTTY ? 1 : 600);

    if (isScreen) {
#ifdef WIN32
        enableVTMode();
#else
        // Enable raw mode for non-blocking keyboard input
        if (isTTY) {
            enableRawMode();
        }
#endif

        // Clear screen
        std::cout << "\e[2J";

        // Beautiful Header
        drawBoxTop("");
        drawCentered("JUNO MONETA", "\e[1;33m");
        drawCentered("Privacy Money for All", "\e[1;36m");
        drawCentered(FormatFullVersion() + " - " + WhichNetwork() + " - RandomX", "\e[0;37m");
        drawBoxBottom();
        std::cout << std::endl;
    }

    while (true) {
        // Number of lines that are always displayed
        int lines = 0;
        int cols = 80;

        // Get current window size
        if (isTTY) {
#ifdef WIN32
            CONSOLE_SCREEN_BUFFER_INFO csbi;
            if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi) != 0) {
                cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
            }
#else
            struct winsize w;
            w.ws_col = 0;
            if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) != -1 && w.ws_col != 0) {
                cols = w.ws_col;
            }
#endif
        }

        // Lock and fetch stats before erasing the screen, in case we block.
        std::optional<MetricsStats> metricsStats;
        if (loaded) {
            metricsStats = loadStats();
        }

        if (isScreen) {
            // Erase below current position
            std::cout << "\e[J" << std::flush;
        }

        // Miner status
#ifdef ENABLE_MINING
        bool mining = GetBoolArg("-gen", false);
#else
        bool mining = false;
#endif

        if (loaded) {
            lines += printStats(metricsStats.value(), isScreen, mining);
            lines += printMiningStatus(mining);
        }
        lines += printMetrics(cols, mining);
        lines += printMessageBox(cols);
        lines += printInitMessage();

        if (isScreen) {
            // Explain how to exit
            std::cout << "[";
#ifdef WIN32
            std::cout << _("'juno-cli.exe stop' to exit");
#else
            std::cout << _("Press Ctrl+C to exit");
#endif
            std::cout << "] [" << _("Set 'showmetrics=0' to hide") << "]" << std::endl;
            lines++; // Count the exit message line
        } else {
            // Print delineator
            std::cout << "----------------------------------------" << std::endl;
        }

        *nNextRefresh = GetTime() + nRefresh;
        while (GetTime() < *nNextRefresh) {
            boost::this_thread::interruption_point();

            // Check for keyboard input
            if (isScreen && isTTY) {
                int key = checkKeyPress();
                if (key == 'M' || key == 'm') {
                    toggleMining();
                    break;  // Force screen refresh
                } else if (key == 'T' || key == 't') {
                    // Only allow changing threads if mining or on non-main network
                    if (mining || Params().NetworkIDString() != "main") {
                        promptForThreads();
                        break;  // Force screen refresh
                    }
                } else if (mining) {
                    // Donation controls only available when mining
                    if (key == 'D' || key == 'd') {
                        toggleDonation();
                        break;  // Force screen refresh
                    } else if (key == 'P' || key == 'p') {
                        // Only allow changing percentage if donations are active
                        int currentPct = getCurrentDonationPercentage();
                        if (currentPct > 0) {
                            promptForPercentage();
                            break;  // Force screen refresh
                        }
                    }
                }
            }

            MilliSleep(200);
        }

        if (isScreen) {
            // Return to the top of the updating section
            std::cout << "\e[" << lines << "A" << std::flush;
        }
    }
}
