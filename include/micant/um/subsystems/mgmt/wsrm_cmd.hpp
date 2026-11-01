#pragma once

/**
 * @file wsrm_cmd.hpp
 * @brief Canonical Win32 wsrmcli.exe / wsrm.exe CLI Management Utility for WSRM.
 *
 * Clean-room implementation of the Microsoft Windows Server System Resource
 * Manager (WSRM) administrative CLI under 17 U.S.C. § 1201(f) &
 * Google LLC v. Oracle America, Inc. (141 S. Ct. 1183, 2021).
 *
 * Provides complete CLI management for:
 *   - wsrm status: global subsystem state, active policy & rebalance vitals
 *   - wsrm policies / criteria: tabular process matching criteria (PMC)
 *   - wsrm setpolicy <process|user|session|custom>: change active allocation policy
 *   - wsrm procs / list: tabular active managed processes & CPU rate limits
 *   - wsrm addcriteria <name> <pattern> <cpu%> [options]: define PMC rule
 *   - wsrm dfss <activeSessions>: trigger Dynamic Fair Share Scheduling
 *   - wsrm account <userOrApp>: query tenant resource accounting & I/O
 *   - wsrm history: query historical accounting log records
 *   - wsrm dump: export declarative PowerShell configuration script
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

#include "micant/um/servers/wsrm.hpp"

namespace micant::wsrm_cmd::um {

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
    if (bytes == 0) return "Unlimited";
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    if (bytes >= (1ULL << 40)) oss << (static_cast<double>(bytes) / (1ULL << 40)) << " TB";
    else if (bytes >= (1ULL << 30)) oss << (static_cast<double>(bytes) / (1ULL << 30)) << " GB";
    else if (bytes >= (1ULL << 20)) oss << (static_cast<double>(bytes) / (1ULL << 20)) << " MB";
    else if (bytes >= (1ULL << 10)) oss << (static_cast<double>(bytes) / (1ULL << 10)) << " KB";
    else oss << bytes << " B";
    return oss.str();
}

inline int ExecuteWsrm(std::span<std::string_view> args, std::string& output) {
    std::ostringstream out;
    auto& wsrm = micant::wsrm::SystemResourceManager::instance();
    wsrm.initialize();

    if (args.empty()) {
        out << "Microsoft Windows [Version 10.0.26100.1]\n"
            << "Windows System Resource Manager Command-Line Tool (wsrmcli.exe / wsrm.exe)\n\n"
            << "Usage: wsrm <command> [arguments]\n\n"
            << "Commands:\n"
            << "  status                          Display WSRM subsystem status & metrics\n"
            << "  policies | criteria             Display Process Matching Criteria (PMC) rules\n"
            << "  setpolicy <proc|user|dfss|custom> Change active resource allocation policy\n"
            << "  procs | list                    Display actively managed processes & CPU rates\n"
            << "  addcriteria <name> <pat> <cpu%> Create or update a Process Matching Criteria\n"
            << "  dfss <activeSessions>           Trigger Dynamic Fair Share Scheduling for RDS\n"
            << "  account <userOrApp>             Query tenant CPU, memory & I/O usage\n"
            << "  history                         Display historical accounting records\n"
            << "  dump                            Export declarative configuration script\n";
        output = out.str();
        return 0;
    }

    std::string verb(args[0]);
    std::transform(verb.begin(), verb.end(), verb.begin(), ::tolower);

    // ------------------------------------------------------------------------
    // wsrm status
    // ------------------------------------------------------------------------
    if (verb == "status") {
        out << "Windows System Resource Manager (WSRM) Subsystem Status:\n"
            << "================================================================================\n"
            << "  Service Status:              ONLINE (wsrm.exe / TitanQuota)\n"
            << "  Active Allocation Policy:    " << micant::wsrm::AllocationPolicyTypeToString(wsrm.getAllocationPolicy()) << "\n"
            << "  Managed Active Processes:    " << wsrm.getAllManagedProcesses().size() << "\n"
            << "  Total Registered Processes:  " << wsrm.getTotalRegisteredProcesses() << "\n"
            << "  Throttling Violations:       " << wsrm.getTotalThrottlingEvents() << "\n"
            << "  DFSS Rebalance Cycles:       " << wsrm.getRebalanceCycles() << "\n"
            << "  Process Matching Rules:      " << wsrm.getAllCriteria().size() << "\n"
            << "  Kernel Driver:               C:\\Windows\\System32\\drivers\\wsrmcore.sys\n";
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // wsrm policies | criteria
    // ------------------------------------------------------------------------
    if (verb == "policies" || verb == "criteria") {
        auto crits = wsrm.getAllCriteria();
        out << "WSRM Process Matching Criteria (PMC) (" << crits.size() << " Rules):\n"
            << "--------------------------------------------------------------------------------\n"
            << std::left << std::setw(24) << "Criteria Name"
            << std::setw(16) << "App Pattern"
            << std::setw(14) << "User/Group"
            << std::setw(10) << "CPU Cap"
            << std::setw(12) << "Mem Cap"
            << "Type\n"
            << "--------------------------------------------------------------------------------\n";
        for (const auto& c : crits) {
            std::string cpuStr = std::to_string(c.targetCpuPercent) + "%";
            std::string memStr = (c.memoryLimitBytes == 0) ? "Unlimited" : (std::to_string(c.memoryLimitBytes / (1024 * 1024)) + " MB");
            std::string modeStr = c.hardCap ? "Hard Cap" : ("Weight (" + std::to_string(c.weight) + ")");
            out << std::left << std::setw(24) << c.criteriaName
                << std::setw(16) << c.appPattern
                << std::setw(14) << c.userOrGroup
                << std::setw(10) << cpuStr
                << std::setw(12) << memStr
                << modeStr << "\n";
        }
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // wsrm setpolicy <proc|user|dfss|custom>
    // ------------------------------------------------------------------------
    if (verb == "setpolicy") {
        if (args.size() < 2) {
            out << "Error: Policy type required (process|user|session|custom). Usage: wsrm setpolicy <type>\n";
            output = out.str();
            return 1;
        }

        std::string pol(args[1]);
        std::transform(pol.begin(), pol.end(), pol.begin(), ::tolower);

        if (pol == "proc" || pol == "process" || pol == "equalperprocess") {
            wsrm.setAllocationPolicy(micant::wsrm::AllocationPolicyType::EqualPerProcess);
            out << "Active allocation policy set to Equal_Per_Process.\n";
            output = out.str();
            return 0;
        } else if (pol == "user" || pol == "equalperuser") {
            wsrm.setAllocationPolicy(micant::wsrm::AllocationPolicyType::EqualPerUser);
            out << "Active allocation policy set to Equal_Per_User.\n";
            output = out.str();
            return 0;
        } else if (pol == "dfss" || pol == "session" || pol == "equalpersession") {
            wsrm.setAllocationPolicy(micant::wsrm::AllocationPolicyType::EqualPerSession);
            out << "Active allocation policy set to Equal_Per_Session (DFSS).\n";
            output = out.str();
            return 0;
        } else if (pol == "custom" || pol == "weighted" || pol == "customweighted") {
            wsrm.setAllocationPolicy(micant::wsrm::AllocationPolicyType::CustomWeighted);
            out << "Active allocation policy set to Custom_Weighted.\n";
            output = out.str();
            return 0;
        } else {
            out << "Error: Unknown policy type '" << args[1] << "'. Valid options: process, user, session, custom.\n";
            output = out.str();
            return 1;
        }
    }

    // ------------------------------------------------------------------------
    // wsrm procs | list
    // ------------------------------------------------------------------------
    if (verb == "procs" || verb == "list") {
        auto procs = wsrm.getAllManagedProcesses();
        out << "WSRM Managed Processes Inventory (" << procs.size() << " Processes):\n"
            << "--------------------------------------------------------------------------------\n"
            << std::left << std::setw(8)  << "PID"
            << std::setw(18) << "Process Name"
            << std::setw(14) << "User Name"
            << std::setw(6)  << "Sess"
            << std::setw(10) << "CPU %"
            << std::setw(12) << "Mem Cap"
            << "Matched Criteria\n"
            << "--------------------------------------------------------------------------------\n";
        for (const auto& p : procs) {
            std::ostringstream cpuStream;
            cpuStream << std::fixed << std::setprecision(1) << p.allocatedCpuPercent << "%";
            out << std::left << std::setw(8)  << p.processId
                << std::setw(18) << p.processName
                << std::setw(14) << p.userName
                << std::setw(6)  << p.sessionId
                << std::setw(10) << cpuStream.str()
                << std::setw(12) << FormatBytes(p.memoryLimitBytes)
                << p.matchedCriteria << "\n";
        }
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // wsrm addcriteria <name> <pattern> <cpu%> [options]
    // ------------------------------------------------------------------------
    if (verb == "addcriteria") {
        if (args.size() < 4) {
            out << "Error: Name, app pattern, and CPU percentage required.\n"
                << "Usage: wsrm addcriteria <name> <pattern> <cpu%> [/user:<u|*>] [/session:<id>] [/mem:<MB>] [/weight:<1-9>] [/hard:<on|off>]\n";
            output = out.str();
            return 1;
        }

        micant::wsrm::ProcessMatchingCriteria pmc{};
        pmc.criteriaName = std::string(args[1]);
        pmc.appPattern = std::string(args[2]);
        pmc.targetCpuPercent = static_cast<uint32_t>(std::strtoul(std::string(args[3]).c_str(), nullptr, 10));
        pmc.userOrGroup = "*";
        pmc.sessionId = -1;
        pmc.hardCap = true;
        pmc.weight = 5;
        pmc.memoryLimitBytes = 0;

        for (size_t i = 4; i < args.size(); ++i) {
            std::string opt(args[i]);
            if (opt.starts_with("/user:") || opt.starts_with("-user:")) {
                pmc.userOrGroup = opt.substr(6);
            } else if (opt.starts_with("/session:") || opt.starts_with("-session:")) {
                pmc.sessionId = std::atoi(opt.substr(9).c_str());
            } else if (opt.starts_with("/mem:") || opt.starts_with("-mem:")) {
                uint64_t mb = std::strtoull(opt.substr(5).c_str(), nullptr, 10);
                pmc.memoryLimitBytes = mb * 1024 * 1024;
            } else if (opt.starts_with("/weight:") || opt.starts_with("-weight:")) {
                pmc.weight = static_cast<uint32_t>(std::strtoul(opt.substr(8).c_str(), nullptr, 10));
            } else if (opt.starts_with("/hard:") || opt.starts_with("-hard:")) {
                std::string h = opt.substr(6);
                std::transform(h.begin(), h.end(), h.begin(), ::tolower);
                pmc.hardCap = (h == "on" || h == "true" || h == "1");
            }
        }

        if (wsrm.addMatchingCriteria(pmc)) {
            out << "Process Matching Criteria '" << pmc.criteriaName << "' created successfully:\n"
                << "  App Pattern:    " << pmc.appPattern << "\n"
                << "  Target CPU:     " << pmc.targetCpuPercent << "%\n"
                << "  User/Group:     " << pmc.userOrGroup << "\n"
                << "  Session ID:     " << (pmc.sessionId == -1 ? "All" : std::to_string(pmc.sessionId)) << "\n"
                << "  Memory Limit:   " << FormatBytes(pmc.memoryLimitBytes) << "\n"
                << "  Rate Cap Mode:  " << (pmc.hardCap ? "Hard Cap" : "Weight-Based") << "\n";
            output = out.str();
            return 0;
        } else {
            out << "Error: Failed to add Process Matching Criteria.\n";
            output = out.str();
            return 1;
        }
    }

    // ------------------------------------------------------------------------
    // wsrm dfss <activeSessions>
    // ------------------------------------------------------------------------
    if (verb == "dfss") {
        uint32_t sessions = 2;
        if (args.size() >= 2) {
            sessions = static_cast<uint32_t>(std::strtoul(std::string(args[1]).c_str(), nullptr, 10));
            if (sessions == 0) sessions = 1;
        }

        wsrm.applyFairShare(sessions);
        double targetPerSession = 100.0 / static_cast<double>(sessions);
        out << "Dynamic Fair Share Scheduling (DFSS) applied across " << sessions << " active session(s):\n"
            << "  Target CPU per Session: " << std::fixed << std::setprecision(1) << targetPerSession << "%\n"
            << "  Rebalance Cycles:       " << wsrm.getRebalanceCycles() << "\n";
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // wsrm account <userOrApp>
    // ------------------------------------------------------------------------
    if (verb == "account") {
        if (args.size() < 2) {
            out << "Error: User or application name required. Usage: wsrm account <userOrApp>\n";
            output = out.str();
            return 1;
        }

        std::string target(args[1]);
        uint64_t cpuUs = 0, peakMem = 0, ioBytes = 0;
        if (wsrm.getTenantAccounting(target, &cpuUs, &peakMem, &ioBytes)) {
            out << "Resource Accounting Telemetry for '" << target << "':\n"
                << "--------------------------------------------------------------------------------\n"
                << "  Total CPU Time:      " << (cpuUs / 1000) << " ms (" << cpuUs << " us)\n"
                << "  Peak Working Set:    " << FormatBytes(peakMem) << "\n"
                << "  Total I/O Transfer:  " << FormatBytes(ioBytes) << "\n";
            output = out.str();
            return 0;
        } else {
            out << "Error: No accounting records found for tenant or process '" << target << "'.\n";
            output = out.str();
            return 1;
        }
    }

    // ------------------------------------------------------------------------
    // wsrm history
    // ------------------------------------------------------------------------
    if (verb == "history") {
        auto hist = wsrm.getAccountingHistory();
        out << "WSRM Historical Resource Accounting Log (" << hist.size() << " Records):\n"
            << "--------------------------------------------------------------------------------\n"
            << std::left << std::setw(16) << "Tenant/User"
            << std::setw(18) << "Process"
            << std::setw(8)  << "PID"
            << std::setw(12) << "CPU (ms)"
            << std::setw(12) << "Peak Mem"
            << "I/O Transfer\n"
            << "--------------------------------------------------------------------------------\n";
        for (const auto& h : hist) {
            out << std::left << std::setw(16) << h.tenantName
                << std::setw(18) << h.processName
                << std::setw(8)  << h.processId
                << std::setw(12) << (h.cpuTimeUs / 1000)
                << std::setw(12) << FormatBytes(h.peakWorkingSetBytes)
                << FormatBytes(h.totalIoBytes) << "\n";
        }
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // wsrm dump
    // ------------------------------------------------------------------------
    if (verb == "dump") {
        out << "# Windows System Resource Manager (WSRM) Policy Configuration Script\n"
            << "# Generated by MicaNT wsrmcli.exe / wsrm.exe @ 10.0.26100.1\n\n";
        out << "Set-WsrmAllocationPolicy -Policy "
            << micant::wsrm::AllocationPolicyTypeToString(wsrm.getAllocationPolicy()) << "\n\n";
        for (const auto& c : wsrm.getAllCriteria()) {
            out << "New-WsrmProcessMatchingCriteria -Name \"" << c.criteriaName
                << "\" -Pattern \"" << c.appPattern
                << "\" -CpuPercentage " << c.targetCpuPercent;
            if (c.memoryLimitBytes > 0) {
                out << " -MemoryLimitBytes " << c.memoryLimitBytes;
            }
            if (c.hardCap) {
                out << " -HardCap $true";
            }
            out << "\n";
        }
        output = out.str();
        return 0;
    }

    out << "Unknown wsrm command: '" << verb << "'. Type 'wsrm' for help.\n";
    output = out.str();
    return 1;
}

inline int ExecuteWsrmString(std::string_view cmdLine, std::string& output) {
    auto tokens = Tokenize(cmdLine);
    if (tokens.empty()) {
        std::span<std::string_view> emptyArgs;
        return ExecuteWsrm(emptyArgs, output);
    }

    // Strip leading "wsrm", "wsrmcli", or "wsrmadmin" if present
    if (tokens[0] == "wsrm"      || tokens[0] == "wsrm.exe" ||
        tokens[0] == "wsrmcli"   || tokens[0] == "wsrmcli.exe" ||
        tokens[0] == "wsrmadmin" || tokens[0] == "wsrmadmin.exe") {
        tokens.erase(tokens.begin());
    }

    std::span<std::string_view> args(tokens.data(), tokens.size());
    return ExecuteWsrm(args, output);
}

inline int Main(int argc, char* argv[]) {
    std::vector<std::string_view> args;
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    std::string outStr;
    int rc = ExecuteWsrm(args, outStr);
    std::cout << outStr;
    return rc;
}

} // namespace micant::wsrm_cmd::um
