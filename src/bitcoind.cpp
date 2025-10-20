// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin Core developers
// Copyright (c) 2016-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "clientversion.h"
#include "deprecation.h"
#include "fs.h"
#include "rpc/server.h"
#include "init.h"
#include "main.h"
#include "noui.h"
#include "scheduler.h"
#include "util/system.h"
#include "httpserver.h"
#include "httprpc.h"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/thread.hpp>

#include <stdio.h>
#include <fstream>

const std::function<std::string(const char*)> G_TRANSLATION_FUN = nullptr;

/* Introduction text for doxygen: */

/*! \mainpage Developer documentation
 *
 * \section intro_sec Introduction
 *
 * This is the developer documentation of the reference client for an experimental new digital currency called Bitcoin (https://www.bitcoin.org/),
 * which enables instant payments to anyone, anywhere in the world. Bitcoin uses peer-to-peer technology to operate
 * with no central authority: managing transactions and issuing money are carried out collectively by the network.
 *
 * The software is a community-driven open source project, released under the MIT license.
 *
 * \section Navigation
 * Use the buttons <code>Namespaces</code>, <code>Classes</code> or <code>Files</code> at the top of the page to start navigating the code.
 */

static bool fDaemon;

void WaitForShutdown(boost::thread_group* threadGroup)
{
    bool fShutdown = ShutdownRequested();
    // Tell the main threads to shutdown.
    while (!fShutdown)
    {
        MilliSleep(200);
        fShutdown = ShutdownRequested();
    }
    if (threadGroup)
    {
        Interrupt(*threadGroup);
        threadGroup->join_all();
    }
}

static std::string GetDefaultConfigContent()
{
    return
        "# Juno Cash configuration file\n"
        "# Generated automatically on first run\n"
        "# Lines beginning with # are comments\n"
        "\n"
        "# Enable CPU mining (uncomment to enable)\n"
        "#gen=1\n"
        "\n"
        "# Number of CPU threads for mining (-1 = all cores)\n"
        "#genproclimit=-1\n"
        "\n"
        "# RPC server (enabled by default)\n"
        "# Authentication uses a random cookie by default\n"
        "# To use password auth instead, uncomment and set:\n"
        "#rpcuser=yourusername\n"
        "#rpcpassword=yourpassword\n"
        "\n"
        "# Restrict RPC to localhost only (default behavior)\n"
        "# To allow other IPs, uncomment and specify:\n"
        "#rpcallowip=127.0.0.1\n"
        "\n"
        "# Optional developer donation (0-100 percent of mining rewards, default=0)\n"
        "#donationpercentage=5\n"
        "\n";
}

static bool CreateDefaultConfigFile(const fs::path& confPath)
{
    try {
        // Create parent directory if it doesn't exist
        fs::path dir = confPath.parent_path();
        if (!fs::exists(dir)) {
            fs::create_directories(dir);
        }

        // Write default config content
        std::ofstream file;
        file.open(confPath.string());
        if (!file.is_open()) {
            return false;
        }
        file << GetDefaultConfigContent();
        file.close();

        fprintf(stdout, _("Created configuration file: %s\n").c_str(), confPath.string().c_str());
        return true;
    } catch (const std::exception& e) {
        fprintf(stderr, _("Error creating configuration file: %s\n").c_str(), e.what());
        return false;
    }
}

//////////////////////////////////////////////////////////////////////////////
//
// Start
//
bool AppInit(int argc, char* argv[])
{
    boost::thread_group threadGroup;
    CScheduler scheduler;

    bool fRet = false;

    //
    // Parameters
    //
    ParseParameters(argc, argv);

    // Process help and version before taking care about datadir
    if (mapArgs.count("-?") || mapArgs.count("-h") ||  mapArgs.count("-help") || mapArgs.count("-version"))
    {
        std::string strUsage = _("Juno Cash Daemon") + " " + _("version") + " " + FormatFullVersion() + "\n" + PrivacyInfo();

        if (mapArgs.count("-version"))
        {
            strUsage += LicenseInfo();
        }
        else
        {
            strUsage += "\n" + _("Usage:") + "\n" +
                  "  junocashd [options]                     " + _("Start Juno Cash Daemon") + "\n";

            strUsage += "\n" + HelpMessage(HMM_BITCOIND);
        }

        fprintf(stdout, "%s", strUsage.c_str());
        return true;
    }

    try
    {
        if (!fs::is_directory(GetDataDir(false)))
        {
            fprintf(stderr, "Error: Specified data directory \"%s\" does not exist.\n", mapArgs["-datadir"].c_str());
            return false;
        }
        try
        {
            ReadConfigFile(GetArg("-conf", BITCOIN_CONF_FILENAME), mapArgs, mapMultiArgs);
        } catch (const missing_zcash_conf& e) {
            auto confFilename = GetArg("-conf", BITCOIN_CONF_FILENAME);
            auto confPath = GetConfigFile(confFilename);

            // Auto-create config file with helpful defaults
            fprintf(stdout, _("Configuration file not found. Creating default configuration at:\n%s\n").c_str(),
                    confPath.string().c_str());

            if (!CreateDefaultConfigFile(confPath)) {
                fprintf(stderr, _("Failed to create configuration file. Please create it manually.\n").c_str());
                return false;
            }

            // Try reading again after creation
            try {
                ReadConfigFile(confFilename, mapArgs, mapMultiArgs);
            } catch (const std::exception& e2) {
                fprintf(stderr, _("Error reading newly created configuration file: %s\n").c_str(), e2.what());
                return false;
            }
        } catch (const std::exception& e) {
            fprintf(stderr,"Error reading configuration file: %s\n", e.what());
            return false;
        }

        // Check for -testnet or -regtest parameter (Params() calls are only valid after this clause)
        try {
            SelectParams(ChainNameFromCommandLine());
        } catch(std::exception &e) {
            fprintf(stderr, "Error: %s\n", e.what());
            return false;
        }

        // Handle setting of allowed-deprecated features as early as possible
        // so that it's possible for other initialization steps to respect them.
        auto deprecationError = LoadAllowedDeprecatedFeatures();
        if (deprecationError.has_value()) {
            fprintf(stderr, "%s", deprecationError.value().c_str());
            return false;
        }

        // Command-line RPC
        bool fCommandLine = false;
        for (int i = 1; i < argc; i++)
            if (!IsSwitchChar(argv[i][0]) && !boost::algorithm::istarts_with(argv[i], "zcash:"))
                fCommandLine = true;

        if (fCommandLine)
        {
            fprintf(stderr, "Error: There is no RPC client functionality in zcashd. Use the zcash-cli utility instead.\n");
            exit(EXIT_FAILURE);
        }
#ifndef WIN32
        fDaemon = GetBoolArg("-daemon", false);
        if (fDaemon)
        {
            fprintf(stdout, "JunoMoneta server starting\n");

            // Daemonize
            pid_t pid = fork();
            if (pid < 0)
            {
                fprintf(stderr, "Error: fork() returned %d errno %d\n", pid, errno);
                return false;
            }
            if (pid > 0) // Parent process, pid is child process id
            {
                return true;
            }
            // Child process falls through to rest of initialization

            pid_t sid = setsid();
            if (sid < 0)
                fprintf(stderr, "Error: setsid() returned %d errno %d\n", sid, errno);
        }
#endif
        SoftSetBoolArg("-server", true);

        // Set this early so that parameter interactions go to console
        InitLogging();

        // Now that we have logging set up, start the initialization span.
        auto span = TracingSpan("info", "main", "Init");
        auto spanGuard = span.Enter();

        InitParameterInteraction();
        fRet = AppInit2(threadGroup, scheduler);
    }
    catch (const std::exception& e) {
        PrintExceptionContinue(&e, "AppInit()");
    } catch (...) {
        PrintExceptionContinue(NULL, "AppInit()");
    }

    if (!fRet)
    {
        Interrupt(threadGroup);
        // threadGroup.join_all(); was left out intentionally here, because we didn't re-test all of
        // the startup-failure cases to make sure they don't result in a hang due to some
        // thread-blocking-waiting-for-another-thread-during-startup case
    } else {
        WaitForShutdown(&threadGroup);
    }
    Shutdown();

    return fRet;
}
#ifdef ZCASH_FUZZ
#warning BUILDING A FUZZER, NOT THE REAL MAIN
#include "fuzz.cpp"
#else
int main(int argc, char* argv[])
{
    SetupEnvironment();

    // Connect bitcoind signal handlers
    noui_connect();

    return (AppInit(argc, argv) ? EXIT_SUCCESS : EXIT_FAILURE);
}
#endif
