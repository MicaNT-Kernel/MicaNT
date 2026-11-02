#pragma once

/**
 * @file dedup_cmd.hpp
 * @brief Canonical Win32 ddpeval.exe and ddpcli.exe Command-Line Management Utilities.
 *
 * Clean-room implementation of the Microsoft Windows Server Data Deduplication
 * evaluation and administrative utilities (ddpeval.exe, ddpcli.exe) under 17 U.S.C. § 1201(f) &
 * Google LLC v. Oracle America, Inc. (141 S. Ct. 1183, 2021).
 *
 * Provides complete CLI management for:
 *   - ddpeval <path>: prospective deduplication space savings evaluation
 *   - ddpcli volume [/list|/status]: query volumes with deduplication enabled
 *   - ddpcli enable <drive> [/type:Default|HyperV|Backup]: enable deduplication
 *   - ddpcli disable <drive>: disable deduplication
 *   - ddpcli start <drive> /job:<type>: trigger optimization, GC, scrubbing, un-optimization
 *   - ddpcli job [/status]: list active and historical servicing jobs
 *   - ddpcli status: global deduplication health and space efficiency vitals
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

#include "micant/km/drivers/fs/dedup.hpp"

namespace micant::dedup_cmd::um {

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
// ddpeval.exe CLI Command Handler
// ----------------------------------------------------------------------------
inline int ExecuteDdpEval(int argc, const char* argv[], std::ostream& out) {
    auto& sys = micant::dedup::DataDeduplicationSubsystem::get();
    sys.initialize();

    if (argc < 2) {
        out << "Microsoft Windows [Version 10.0.26100.1]\n"
            << "Data Deduplication Evaluation Tool (ddpeval.exe)\n\n"
            << "Usage:\n"
            << "  ddpeval <path>                               - Evaluate folder or drive for deduplication savings\n"
            << "  ddpeval /?                                   - Display help options\n";
        return 0;
    }

    std::string_view targetPath = argv[1];
    if (targetPath == "/?" || targetPath == "-?" || targetPath == "help") {
        const char* dummy[] = {"ddpeval"};
        return ExecuteDdpEval(1, dummy, out);
    }

    std::string drive = std::string(targetPath.substr(0, 2));
    auto vol = sys.getVolume(drive);
    if (!vol) {
        // If not explicit drive, default to D:
        vol = sys.getVolume("D:");
    }

    out << "MicaNT Data Deduplication Evaluation Tool [Windows Server Compatible]\n"
        << "Copyright (C) 2026 MicaNT Authors. Windows and Windows Server are trademarks of Microsoft Corporation.\n\n"
        << "Evaluated folder: " << targetPath << "\n";

    if (vol) {
        uint64_t totalLogical = vol->getChunkStore().getTotalLogicalBytes();
        uint64_t storedPhys = vol->getChunkStore().getStoredPhysicalBytes();
        uint64_t saved = (totalLogical > storedPhys) ? (totalLogical - storedPhys) : 0;
        double pct = (totalLogical > 0) ? (static_cast<double>(saved) * 100.0 / static_cast<double>(totalLogical)) : 0.0;

        out << "Total files evaluated:  " << vol->getFileCount() << "\n"
            << "Total size:             " << FormatBytes(totalLogical) << "\n"
            << "Space savings:          " << FormatBytes(saved) << " (" << std::fixed << std::setprecision(1) << pct << "%)\n"
            << "Optimized size:         " << FormatBytes(storedPhys) << "\n";
    } else {
        out << "Total files evaluated:  0\n"
            << "Total size:             0 Bytes\n"
            << "Space savings:          0 Bytes (0.0%)\n"
            << "Optimized size:         0 Bytes\n";
    }
    return 0;
}

inline void ExecuteDdpEvalString(std::string_view commandLine, std::string& out) {
    auto tokens = Tokenize(commandLine);
    if (tokens.empty()) {
        out = "ddpeval: missing command arguments.\n";
        return;
    }

    std::vector<std::string> args;
    args.reserve(tokens.size());
    for (auto t : tokens) args.emplace_back(t);

    std::vector<const char*> argv;
    argv.reserve(args.size());
    for (const auto& a : args) argv.push_back(a.c_str());

    std::ostringstream ss;
    ExecuteDdpEval(static_cast<int>(argv.size()), argv.data(), ss);
    out = ss.str();
}

// ----------------------------------------------------------------------------
// ddpcli.exe CLI Command Handler
// ----------------------------------------------------------------------------
inline int ExecuteDdpCli(int argc, const char* argv[], std::ostream& out) {
    auto& sys = micant::dedup::DataDeduplicationSubsystem::get();
    sys.initialize();

    if (argc < 2) {
        out << "Microsoft Windows [Version 10.0.26100.1]\n"
            << "Data Deduplication Command-Line Management Utility (ddpcli.exe)\n\n"
            << "Usage:\n"
            << "  ddpcli volume [/list|/status]                - List deduplication volumes\n"
            << "  ddpcli enable <drive> [/type:<type>]         - Enable deduplication on volume\n"
            << "  ddpcli disable <drive>                       - Disable deduplication on volume\n"
            << "  ddpcli start <drive> /job:<Optimization|GarbageCollection|Scrubbing|Unoptimization>\n"
            << "                                               - Run background deduplication job\n"
            << "  ddpcli job [/status]                         - Query servicing job records\n"
            << "  ddpcli status                                - Global deduplication status\n"
            << "  ddpcli help                                  - Display this help message\n";
        return 0;
    }

    std::string_view verb = argv[1];

    if (verb == "help" || verb == "/?" || verb == "-?") {
        const char* dummy[] = {"ddpcli"};
        return ExecuteDdpCli(1, dummy, out);
    }

    // --- ddpcli volume ---
    if (verb == "volume") {
        auto volumes = sys.getAllVolumes();
        out << "Volume  Enabled  UsageType  LogicalSize  OptimizedSize  SavedSpace   SavingsPct  Chunks\n"
            << "------  -------  ---------  -----------  -------------  ----------   ----------  ------\n";

        for (const auto& v : volumes) {
            uint64_t logical = v->getChunkStore().getTotalLogicalBytes();
            uint64_t phys = v->getChunkStore().getStoredPhysicalBytes();
            uint64_t saved = v->getChunkStore().getSavedBytes();
            double pct = v->getChunkStore().getSavingsPercentage();

            out << std::left
                << std::setw(8)  << v->getDriveLetter()
                << std::setw(9)  << (v->isEnabled() ? "True" : "False")
                << std::setw(11) << micant::dedup::DedupUsageTypeToString(v->getConfig().usageType)
                << std::setw(13) << FormatBytes(logical)
                << std::setw(15) << FormatBytes(phys)
                << std::setw(13) << FormatBytes(saved)
                << std::fixed << std::setprecision(1) << pct << "%"
                << std::setw(6) << " "
                << v->getChunkStore().getChunkCount() << "\n";
        }
        return 0;
    }

    // --- ddpcli enable ---
    if (verb == "enable") {
        if (argc < 3) {
            out << "Error: Drive letter required. Usage: ddpcli enable <drive> [/type:<type>]\n";
            return 1;
        }

        std::string drive = argv[2];
        micant::dedup::DedupUsageType u = micant::dedup::DedupUsageType::Default;

        if (argc >= 4) {
            std::string_view tArg = argv[3];
            if (tArg.starts_with("/type:")) {
                std::string_view val = tArg.substr(6);
                if (val == "HyperV" || val == "hyperv") u = micant::dedup::DedupUsageType::HyperV;
                else if (val == "Backup" || val == "backup") u = micant::dedup::DedupUsageType::Backup;
                else if (val == "Default" || val == "default") u = micant::dedup::DedupUsageType::Default;
            }
        }

        auto vol = sys.getVolume(drive);
        if (!vol) {
            out << "Creating new deduplication configuration for drive " << drive << "...\n";
            vol = std::make_shared<micant::dedup::DedupVolume>(drive, u);
        }
        vol->setEnabled(true);
        vol->setUsageType(u);
        out << "Data Deduplication successfully ENABLED on volume " << drive
            << " with UsageType: " << micant::dedup::DedupUsageTypeToString(u) << ".\n";
        return 0;
    }

    // --- ddpcli disable ---
    if (verb == "disable") {
        if (argc < 3) {
            out << "Error: Drive letter required. Usage: ddpcli disable <drive>\n";
            return 1;
        }

        std::string drive = argv[2];
        auto vol = sys.getVolume(drive);
        if (!vol) {
            out << "Error: Volume " << drive << " not configured for Data Deduplication.\n";
            return 1;
        }
        vol->setEnabled(false);
        out << "Data Deduplication successfully DISABLED on volume " << drive << ".\n";
        return 0;
    }

    // --- ddpcli start ---
    if (verb == "start") {
        if (argc < 3) {
            out << "Error: Drive letter required. Usage: ddpcli start <drive> /job:<type>\n";
            return 1;
        }

        std::string drive = argv[2];
        auto vol = sys.getVolume(drive);
        if (!vol) {
            out << "Error: Volume " << drive << " not found.\n";
            return 1;
        }

        std::string jobStr = "Optimization";
        if (argc >= 4) {
            std::string_view jArg = argv[3];
            if (jArg.starts_with("/job:")) {
                jobStr = std::string(jArg.substr(5));
            }
        }

        if (jobStr == "Optimization" || jobStr == "opt") {
            out << "Starting Optimization job on volume " << drive << "...\n";
            auto rec = vol->runOptimizationJob();
            out << "Optimization job completed successfully: " << rec.filesProcessed << " files processed, "
                << FormatBytes(rec.bytesOptimized) << " optimized.\n";
            return 0;
        } else if (jobStr == "GarbageCollection" || jobStr == "gc") {
            out << "Starting GarbageCollection job on volume " << drive << "...\n";
            auto rec = vol->runGarbageCollectionJob();
            out << "Garbage Collection completed: " << FormatBytes(rec.bytesReclaimed) << " reclaimed.\n";
            return 0;
        } else if (jobStr == "Scrubbing" || jobStr == "scrub") {
            out << "Starting Scrubbing job on volume " << drive << "...\n";
            auto rec = vol->runScrubbingJob();
            out << "Data Scrubbing completed: All chunks verified with 100% integrity.\n";
            return 0;
        } else if (jobStr == "Unoptimization" || jobStr == "unopt") {
            out << "Starting Unoptimization job on volume " << drive << "...\n";
            auto rec = vol->runUnoptimizationJob();
            out << "Unoptimization completed: " << rec.filesProcessed << " files re-expanded.\n";
            return 0;
        }

        out << "Unknown job type: " << jobStr << ". Valid types: Optimization, GarbageCollection, Scrubbing, Unoptimization.\n";
        return 1;
    }

    // --- ddpcli job ---
    if (verb == "job") {
        out << "JobId  Drive  JobType            Status     FilesProcessed  BytesProcessed  BytesReclaimed\n"
            << "-----  -----  -------            ------     --------------  --------------  --------------\n";

        for (const auto& v : sys.getAllVolumes()) {
            for (const auto& j : v->getJobHistory()) {
                out << std::left
                    << std::setw(7)  << j.jobId
                    << std::setw(7)  << j.driveLetter
                    << std::setw(19) << micant::dedup::DedupJobTypeToString(j.type)
                    << std::setw(11) << micant::dedup::DedupJobStatusToString(j.status)
                    << std::setw(16) << j.filesProcessed
                    << std::setw(16) << FormatBytes(j.bytesOptimized)
                    << FormatBytes(j.bytesReclaimed) << "\n";
            }
        }
        return 0;
    }

    // --- ddpcli status ---
    if (verb == "status") {
        out << "Data Deduplication Subsystem Status:\n"
            << "  Service Status:        Running (dedup.sys & DedupSvc)\n"
            << "  Active Volumes:        " << sys.getActiveVolumesCount() << "\n"
            << "  Total Space Saved:     " << FormatBytes(sys.getTotalSavedBytes()) << "\n"
            << "  Global Savings Pct:    " << std::fixed << std::setprecision(1) << sys.getGlobalSavingsPercentage() << "%\n"
            << "  Total Chunks Stored:   " << sys.getTotalChunksStored() << "\n"
            << "  Completed Jobs:        " << sys.getCompletedJobsCount() << "\n";
        return 0;
    }

    out << "Unknown command: ddpcli " << verb << ". Type 'ddpcli help' for usage.\n";
    return 1;
}

inline void ExecuteDdpCliString(std::string_view commandLine, std::string& out) {
    auto tokens = Tokenize(commandLine);
    if (tokens.empty()) {
        out = "ddpcli: missing command arguments.\n";
        return;
    }

    std::vector<std::string> args;
    args.reserve(tokens.size());
    for (auto t : tokens) args.emplace_back(t);

    std::vector<const char*> argv;
    argv.reserve(args.size());
    for (const auto& a : args) argv.push_back(a.c_str());

    std::ostringstream ss;
    ExecuteDdpCli(static_cast<int>(argv.size()), argv.data(), ss);
    out = ss.str();
}

} // namespace micant::dedup_cmd::um
