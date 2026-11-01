#pragma once

/**
 * @file sr_cmd.hpp
 * @brief Canonical Win32 srcli.exe Command-Line Management Utility for Storage Replica.
 *
 * Clean-room implementation of the Microsoft Windows Server Storage Replica
 * administrative CLI utility (srcli.exe) under 17 U.S.C. § 1201(f) &
 * Google LLC v. Oracle America, Inc. (141 S. Ct. 1183, 2021).
 *
 * Provides complete CLI management for:
 *   - srcli partnership [/list|/status]: query replication partnerships
 *   - srcli create <id> /source:<vol> /dest:<vol> [/mode:sync|async]: establish partnership
 *   - srcli remove <id>: teardown replication partnership
 *   - srcli sync <id> [/delta]: trigger delta resynchronization / flush
 *   - srcli failover <id> [/forced]: execute role reversal / failover
 *   - srcli suspend <id>: pause replication
 *   - srcli resume <id>: resume replication
 *   - srcli log <id>: query write-ahead log & dirty block telemetry
 *   - srcli status: global Storage Replica subsystem health and metrics
 */

#include <string>
#include <string_view>
#include <vector>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <cstdint>
#include <span>

#include "micant/km/drivers/storage/storage_replica.hpp"

namespace micant::sr_cmd::um {

inline std::vector<std::string_view> Tokenize(std::string_view input) {
    std::vector<std::string_view> tokens;
    size_t pos = 0;
    while (pos < input.size()) {
        while (pos < input.size() && (input[pos] == ' ' || input[pos] == '\t' || input[pos] == '\r' || input[pos] == '\n')) {
            pos++;
        }
        if (pos >= input.size()) break;
        size_t start = pos;
        if (input[pos] == '\"') {
            start = ++pos;
            while (pos < input.size() && input[pos] != '\"') pos++;
            tokens.push_back(input.substr(start, pos - start));
            if (pos < input.size()) pos++;
        } else {
            while (pos < input.size() && input[pos] != ' ' && input[pos] != '\t' && input[pos] != '\r' && input[pos] != '\n') {
                pos++;
            }
            tokens.push_back(input.substr(start, pos - start));
        }
    }
    return tokens;
}

inline std::string FormatBytes(uint64_t bytes) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    if (bytes >= 1024ULL * 1024 * 1024 * 1024) {
        double tb = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0 * 1024.0);
        ss << tb << " TB";
    } else if (bytes >= 1024ULL * 1024 * 1024) {
        double gb = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
        ss << gb << " GB";
    } else if (bytes >= 1024ULL * 1024) {
        double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
        ss << mb << " MB";
    } else if (bytes >= 1024ULL) {
        double kb = static_cast<double>(bytes) / 1024.0;
        ss << kb << " KB";
    } else {
        ss << bytes << " Bytes";
    }
    return ss.str();
}

// ----------------------------------------------------------------------------
// srcli.exe CLI Command Handler
// ----------------------------------------------------------------------------
inline int ExecuteSrCli(int argc, const char* argv[], std::ostream& out) {
    auto& sys = micant::sr::StorageReplicaSubsystem::get();
    sys.initialize();

    if (argc < 2) {
        out << "Microsoft Windows [Version 10.0.26100.1]\n"
            << "Storage Replica Command-Line Management Utility (srcli.exe)\n\n"
            << "Usage:\n"
            << "  srcli partnership [/list|/status]            - List active replication partnerships\n"
            << "  srcli create <id> /source:<vol> /dest:<vol> [/mode:<sync|async>]\n"
            << "                                               - Create a new replication partnership\n"
            << "  srcli remove <id>                            - Delete an existing partnership\n"
            << "  srcli sync <id> [/delta]                     - Trigger log flush or delta resync\n"
            << "  srcli failover <id> [/forced]                - Execute direction reversal / takeover\n"
            << "  srcli suspend <id>                           - Suspend replication\n"
            << "  srcli resume <id>                            - Resume suspended replication\n"
            << "  srcli log <id>                               - Query WAL queue & dirty block status\n"
            << "  srcli status                                 - Global Storage Replica vitals\n"
            << "  srcli help                                   - Display this help message\n";
        return 0;
    }

    std::string_view verb = argv[1];

    if (verb == "help" || verb == "/?" || verb == "-?") {
        const char* dummy[] = {"srcli"};
        return ExecuteSrCli(1, dummy, out);
    }

    // --- srcli partnership ---
    if (verb == "partnership") {
        auto parts = sys.getAllPartnerships();
        out << "PartnershipId       Source  Dest   Mode         State                     Role                    Replicated\n"
            << "------------------  ------  ----   ----         -----                     ----                    ----------\n";

        for (const auto& p : parts) {
            const auto& cfg = p->getConfig();
            auto t = p->getTelemetry();

            std::string modeStr = (cfg.mode == micant::sr::ReplicationMode::Synchronous) ? "Sync" : "Async";
            out << std::left
                << std::setw(20) << cfg.partnershipId
                << std::setw(8)  << cfg.sourceVolume
                << std::setw(7)  << cfg.destinationVolume
                << std::setw(13) << modeStr
                << std::setw(26) << micant::sr::ReplicationStateToString(p->getState())
                << std::setw(24) << micant::sr::ReplicationRoleToString(p->getRole())
                << FormatBytes(t.bytesReplicated) << "\n";
        }
        return 0;
    }

    // --- srcli create <id> /source:<vol> /dest:<vol> [/mode:sync|async] ---
    if (verb == "create") {
        if (argc < 3) {
            out << "Error: Missing partnership ID. Syntax: srcli create <id> /source:<vol> /dest:<vol> [/mode:<sync|async>]\n";
            return 1;
        }
        std::string partId = argv[2];
        std::string srcVol = "D:";
        std::string dstVol = "E:";
        std::string srcLog = "L:";
        std::string dstLog = "M:";
        micant::sr::ReplicationMode mode = micant::sr::ReplicationMode::Synchronous;

        for (int i = 3; i < argc; ++i) {
            std::string_view arg = argv[i];
            if (arg.starts_with("/source:")) srcVol = std::string(arg.substr(8));
            else if (arg.starts_with("/dest:")) dstVol = std::string(arg.substr(6));
            else if (arg.starts_with("/sourcelog:")) srcLog = std::string(arg.substr(11));
            else if (arg.starts_with("/destlog:")) dstLog = std::string(arg.substr(9));
            else if (arg.starts_with("/mode:")) {
                std::string_view mVal = arg.substr(6);
                if (mVal == "async" || mVal == "asynchronous") {
                    mode = micant::sr::ReplicationMode::Asynchronous;
                }
            }
        }

        micant::sr::SrPartnershipConfig cfg{};
        cfg.partnershipId = partId;
        cfg.sourceServer = "SRV-PRIMARY";
        cfg.sourceVolume = srcVol;
        cfg.sourceLogVolume = srcLog;
        cfg.destinationServer = "SRV-SECONDARY";
        cfg.destinationVolume = dstVol;
        cfg.destinationLogVolume = dstLog;
        cfg.mode = mode;
        cfg.localRole = micant::sr::ReplicationRole::Source;

        bool ok = sys.createPartnership(cfg);
        if (ok) {
            out << "Replication partnership '" << partId << "' created successfully ("
                << ((mode == micant::sr::ReplicationMode::Synchronous) ? "Synchronous" : "Asynchronous") << ").\n";
            return 0;
        } else {
            out << "Error: Failed to create partnership '" << partId << "'. It may already exist.\n";
            return 1;
        }
    }

    // --- srcli remove <id> ---
    if (verb == "remove" || verb == "delete") {
        if (argc < 3) {
            out << "Error: Missing partnership ID. Syntax: srcli remove <id>\n";
            return 1;
        }
        std::string partId = argv[2];
        bool ok = sys.removePartnership(partId);
        if (ok) {
            out << "Replication partnership '" << partId << "' removed successfully.\n";
            return 0;
        } else {
            out << "Error: Partnership '" << partId << "' not found.\n";
            return 1;
        }
    }

    // --- srcli sync <id> [/delta] ---
    if (verb == "sync") {
        if (argc < 3) {
            out << "Error: Missing partnership ID. Syntax: srcli sync <id> [/delta]\n";
            return 1;
        }
        std::string partId = argv[2];
        auto part = sys.getPartnership(partId);
        if (!part) {
            out << "Error: Partnership '" << partId << "' not found.\n";
            return 1;
        }

        bool delta = false;
        if (argc >= 4 && (std::string_view(argv[3]) == "/delta" || std::string_view(argv[3]) == "-delta")) {
            delta = true;
        }

        if (delta || part->getDirtyBlockCount() > 0) {
            uint32_t synced = part->performDeltaSync();
            out << "Delta resynchronization completed for '" << partId << "': " << synced << " dirty blocks synchronized.\n";
        } else if (part->getMode() == micant::sr::ReplicationMode::Asynchronous) {
            size_t flushed = part->flushAsyncLog();
            out << "Asynchronous log flush completed for '" << partId << "': " << flushed << " records replicated.\n";
        } else {
            out << "Partnership '" << partId << "' is already in continuous synchronization (0 dirty blocks).\n";
        }
        return 0;
    }

    // --- srcli failover <id> [/forced] ---
    if (verb == "failover" || verb == "reverse") {
        if (argc < 3) {
            out << "Error: Missing partnership ID. Syntax: srcli failover <id> [/forced]\n";
            return 1;
        }
        std::string partId = argv[2];
        auto part = sys.getPartnership(partId);
        if (!part) {
            out << "Error: Partnership '" << partId << "' not found.\n";
            return 1;
        }

        micant::sr::FailoverType ft = micant::sr::FailoverType::Graceful;
        if (argc >= 4 && (std::string_view(argv[3]) == "/forced" || std::string_view(argv[3]) == "-forced")) {
            ft = micant::sr::FailoverType::Forced;
        }

        bool ok = part->reverseDirection(ft);
        if (ok) {
            out << "Failover direction reversal completed for '" << partId << "':\n"
                << "  New Role: " << micant::sr::ReplicationRoleToString(part->getRole()) << "\n"
                << "  New Epoch: " << part->getEpoch() << "\n"
                << "  Failover Mode: " << ((ft == micant::sr::FailoverType::Graceful) ? "Graceful" : "Forced") << "\n";
            return 0;
        } else {
            out << "Error: Failover direction reversal failed for '" << partId << "'.\n";
            return 1;
        }
    }

    // --- srcli suspend <id> ---
    if (verb == "suspend" || verb == "pause") {
        if (argc < 3) {
            out << "Error: Missing partnership ID. Syntax: srcli suspend <id>\n";
            return 1;
        }
        std::string partId = argv[2];
        auto part = sys.getPartnership(partId);
        if (!part) {
            out << "Error: Partnership '" << partId << "' not found.\n";
            return 1;
        }
        part->suspend();
        out << "Replication partnership '" << partId << "' suspended successfully.\n";
        return 0;
    }

    // --- srcli resume <id> ---
    if (verb == "resume") {
        if (argc < 3) {
            out << "Error: Missing partnership ID. Syntax: srcli resume <id>\n";
            return 1;
        }
        std::string partId = argv[2];
        auto part = sys.getPartnership(partId);
        if (!part) {
            out << "Error: Partnership '" << partId << "' not found.\n";
            return 1;
        }
        part->resume();
        out << "Replication partnership '" << partId << "' resumed successfully.\n";
        return 0;
    }

    // --- srcli log <id> ---
    if (verb == "log") {
        if (argc < 3) {
            out << "Error: Missing partnership ID. Syntax: srcli log <id>\n";
            return 1;
        }
        std::string partId = argv[2];
        auto part = sys.getPartnership(partId);
        if (!part) {
            out << "Error: Partnership '" << partId << "' not found.\n";
            return 1;
        }
        auto t = part->getTelemetry();
        out << "Storage Replica WAL & Queue Telemetry for '" << partId << "':\n"
            << "  Current LSN:           " << t.currentLsn << "\n"
            << "  Last Flushed LSN:      " << t.lastFlushedLsn << "\n"
            << "  Partnership Epoch:     " << t.epoch << "\n"
            << "  Dirty Blocks Pending:  " << t.dirtyBlockCount << "\n"
            << "  Async Queue Depth:     " << part->getAsyncQueueDepth() << "\n"
            << "  Average Latency:       " << t.averageLatencyUs << " us\n"
            << "  Replicated Bytes:      " << FormatBytes(t.bytesReplicated) << "\n";
        return 0;
    }

    // --- srcli status ---
    if (verb == "status") {
        auto parts = sys.getAllPartnerships();
        uint64_t totalReplicated = 0;
        uint64_t totalSyncWrites = 0;
        uint64_t totalAsyncWrites = 0;
        uint32_t totalDirty = 0;

        for (const auto& p : parts) {
            auto t = p->getTelemetry();
            totalReplicated += t.bytesReplicated;
            totalSyncWrites += t.syncWrites;
            totalAsyncWrites += t.asyncWrites;
            totalDirty += t.dirtyBlockCount;
        }

        out << "Storage Replica Subsystem Status:\n"
            << "  Driver Status:         Running (storrepl.sys & srsys.sys)\n"
            << "  Service Status:        Running (SrSvc in svchost.exe -k StorageReplica)\n"
            << "  Active Partnerships:   " << parts.size() << "\n"
            << "  Total Bytes Replicated:" << FormatBytes(totalReplicated) << "\n"
            << "  Total Sync Writes:     " << totalSyncWrites << "\n"
            << "  Total Async Writes:    " << totalAsyncWrites << "\n"
            << "  Total Dirty Blocks:    " << totalDirty << "\n";
        return 0;
    }

    out << "Unknown command: srcli " << verb << ". Type 'srcli help' for usage.\n";
    return 1;
}

inline void ExecuteSrCliString(std::string_view commandLine, std::string& out) {
    auto tokens = Tokenize(commandLine);
    if (tokens.empty()) {
        out = "srcli: missing command arguments.\n";
        return;
    }

    std::vector<std::string> args;
    args.reserve(tokens.size());
    for (auto t : tokens) args.emplace_back(t);

    std::vector<const char*> argv;
    argv.reserve(args.size());
    for (const auto& a : args) argv.push_back(a.c_str());

    std::ostringstream ss;
    ExecuteSrCli(static_cast<int>(argv.size()), argv.data(), ss);
    out = ss.str();
}

} // namespace micant::sr_cmd::um
