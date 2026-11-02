#pragma once

/**
 * @file mpio_cmd.hpp
 * @brief Canonical Win32 mpiocli.exe / mpio.exe CLI Management Utilities for Multipath I/O.
 *
 * Clean-room implementation of Multipath I/O (MPIO) Subsystem & Device-Specific Module (DSM)
 * CLI management tools under 17 U.S.C. § 1201(f) & Google LLC v. Oracle America, Inc.
 * (141 S. Ct. 1183, 2021).
 *
 * Provides complete CLI management for:
 *   - mpio status / mpiocli status: Introspect MPIO subsystem vitals, active paths, and DSM status
 *   - mpio disks: Tabular inventory of claimed MPIO multi-path virtual disks
 *   - mpio paths [disk_id]: Detailed path matrix, ALUA states, queue depths, and metrics
 *   - mpio set-policy <disk_id> <policy>: Configure load balance policy (RR, FOO, LQD, LB, WP, RRWS)
 *   - mpio claim <hwid>: Register hardware ID for automatic multipath claiming
 *   - mpio unclaim <hwid>: Unregister hardware ID
 *   - mpio claim-list: List registered hardware IDs
 *   - mpio test-failover <disk_id> <path_id>: Simulate path failure and trigger automatic failover
 *   - mpio test-failback <disk_id> <path_id>: Simulate path recovery and trigger failback
 *   - mpio pr-status <disk_id>: Inspect SCSI-3 Persistent Reservation status and registered keys
 *   - mpio dump: Export declarative PowerShell multipath configuration script
 *   - mpio help: Command usage and syntax
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

#include "micant/km/drivers/storage/mpio.hpp"

namespace micant::mpio_cmd::um {

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

class MpioCommandInterpreter {
public:
    static MpioCommandInterpreter& get() {
        static MpioCommandInterpreter instance;
        return instance;
    }

    void execute(std::span<const std::string_view> args, std::string& outResult) {
        std::ostringstream ss;

        if (args.empty() || args[0] == "help" || args[0] == "-h" || args[0] == "--help" || args[0] == "/?") {
            printHelp(ss);
            outResult = ss.str();
            return;
        }

        auto& mgr = micant::mpio::MpioManager::get();
        std::string_view subCmd = args[0];

        if (subCmd == "status") {
            auto vitals = mgr.getVitals();
            ss << "===============================================================================\n"
               << "          MicaNT Sovereign Multipath I/O (MPIO) Subsystem Vitals               \n"
               << "===============================================================================\n"
               << "  Claimed MPIO Disks       : " << vitals.claimedDevicesCount << "\n"
               << "  Total Configured Paths   : " << vitals.totalPathsCount << "\n"
               << "  Active / Healthy Paths   : " << vitals.activePathsCount << "\n"
               << "  Standby Paths            : " << vitals.standbyPathsCount << "\n"
               << "  Failed / Degraded Paths  : " << vitals.failedPathsCount << "\n"
               << "  Total Dispatched I/O     : " << vitals.totalIosDispatched << " operations\n"
               << "  Total Failover Events    : " << vitals.totalFailoverEvents << "\n"
               << "  Total Failback Events    : " << vitals.totalFailbackEvents << "\n"
               << "  Active Storage DSM       : " << vitals.dsmModuleName << " (" << vitals.dsmVendorName << ")\n"
               << "  Path Verification Timer  : " << vitals.pathVerifyPeriodSeconds << " seconds (SCSI TUR Heartbeat)\n"
               << "  SCM Driver Service       : MPIO (SERVICE_RUNNING, BOOT_START)\n"
               << "  DSM Driver Service       : msdsm (SERVICE_RUNNING, BOOT_START)\n"
               << "===============================================================================\n";
        }
        else if (subCmd == "disks") {
            auto devs = mgr.getAllDevices();
            ss << "===============================================================================\n"
               << "                          Claimed MPIO Virtual Disks                           \n"
               << "===============================================================================\n"
               << std::left << std::setw(6)  << "Disk"
               << std::left << std::setw(18) << "Device Name"
               << std::left << std::setw(24) << "Hardware ID"
               << std::left << std::setw(14) << "Bus Type"
               << std::left << std::setw(10) << "Capacity"
               << std::left << std::setw(18) << "Policy"
               << std::left << std::setw(8)  << "Paths" << "\n"
               << "-------------------------------------------------------------------------------\n";

            for (const auto& d : devs) {
                uint32_t activePaths = 0;
                for (const auto& p : d.paths) {
                    if (p.state == micant::mpio::MpioPathState::ActiveOptimized ||
                        p.state == micant::mpio::MpioPathState::ActiveUnoptimized) {
                        activePaths++;
                    }
                }
                std::ostringstream pathStr;
                pathStr << activePaths << "/" << d.paths.size();

                double capGb = static_cast<double>(d.capacityBytes) / (1024.0 * 1024.0 * 1024.0);
                std::ostringstream capStr;
                if (capGb >= 1024.0) {
                    capStr << std::fixed << std::setprecision(1) << (capGb / 1024.0) << " TB";
                } else {
                    capStr << std::fixed << std::setprecision(0) << capGb << " GB";
                }

                ss << std::left << std::setw(6)  << d.diskIndex
                   << std::left << std::setw(18) << d.deviceName
                   << std::left << std::setw(24) << d.hardwareId
                   << std::left << std::setw(14) << micant::mpio::MpioBusTypeToString(d.busType)
                   << std::left << std::setw(10) << capStr.str()
                   << std::left << std::setw(18) << micant::mpio::MpioPolicyToString(d.policy)
                   << std::left << std::setw(8)  << pathStr.str() << "\n";
            }
            ss << "===============================================================================\n";
        }
        else if (subCmd == "paths") {
            int filterDisk = -1;
            if (args.size() >= 2) {
                filterDisk = std::stoi(std::string(args[1]));
            }

            auto devs = mgr.getAllDevices();
            ss << "===============================================================================\n"
               << "                            MPIO Path Topology Matrix                          \n"
               << "===============================================================================\n";

            for (const auto& d : devs) {
                if (filterDisk >= 0 && static_cast<int>(d.diskIndex) != filterDisk) continue;

                ss << "Disk " << d.diskIndex << " (" << d.deviceName << ") - Policy: "
                   << micant::mpio::MpioPolicyToString(d.policy) << "\n"
                   << "-------------------------------------------------------------------------------\n"
                   << std::left << std::setw(12) << "Path ID"
                   << std::left << std::setw(8)  << "Adapter"
                   << std::left << std::setw(10) << "B:T:L"
                   << std::left << std::setw(14) << "TPG"
                   << std::left << std::setw(20) << "State"
                   << std::left << std::setw(8)  << "Weight"
                   << std::left << std::setw(8)  << "Queue"
                   << std::left << std::setw(12) << "I/O Count" << "\n"
                   << "-------------------------------------------------------------------------------\n";

                for (const auto& p : d.paths) {
                    std::ostringstream pid;
                    pid << "0x" << std::hex << std::setw(8) << std::setfill('0') << p.pathId;

                    std::ostringstream btl;
                    btl << p.busNumber << ":" << p.targetId << ":" << p.lunId;

                    ss << std::left << std::setw(12) << pid.str()
                       << std::left << std::setw(8)  << p.adapterIndex
                       << std::left << std::setw(10) << btl.str()
                       << std::left << std::setw(14) << p.targetPortGroup
                       << std::left << std::setw(20) << micant::mpio::MpioPathStateToString(p.state)
                       << std::left << std::setw(8)  << p.weight
                       << std::left << std::setw(8)  << p.queueDepth
                       << std::left << std::setw(12) << p.totalIoRequests << "\n";
                }
                ss << "\n";
            }
        }
        else if (subCmd == "set-policy") {
            if (args.size() < 3) {
                ss << "[-] Syntax: mpio set-policy <disk_id> <policy>\n"
                   << "    Policies: FOO, RR, RRWS, LQD, WP, LB\n";
                outResult = ss.str();
                return;
            }

            uint32_t diskId = static_cast<uint32_t>(std::stoul(std::string(args[1])));
            std::string polStr(args[2]);
            std::transform(polStr.begin(), polStr.end(), polStr.begin(), ::toupper);

            micant::mpio::MpioLoadBalancePolicy pol = micant::mpio::MpioLoadBalancePolicy::RoundRobin;
            if (polStr == "FOO" || polStr == "FAILOVERONLY") {
                pol = micant::mpio::MpioLoadBalancePolicy::FailoverOnly;
            } else if (polStr == "RR" || polStr == "ROUNDROBIN") {
                pol = micant::mpio::MpioLoadBalancePolicy::RoundRobin;
            } else if (polStr == "RRWS" || polStr == "ROUNDROBINWITHSUBSET") {
                pol = micant::mpio::MpioLoadBalancePolicy::RoundRobinWithSubset;
            } else if (polStr == "LQD" || polStr == "LEASTQUEUEDEPTH") {
                pol = micant::mpio::MpioLoadBalancePolicy::LeastQueueDepth;
            } else if (polStr == "WP" || polStr == "WEIGHTEDPATHS") {
                pol = micant::mpio::MpioLoadBalancePolicy::WeightedPaths;
            } else if (polStr == "LB" || polStr == "LEASTBLOCKS") {
                pol = micant::mpio::MpioLoadBalancePolicy::LeastBlocks;
            } else {
                ss << "[-] Unknown policy '" << args[2] << "'. Valid options: FOO, RR, RRWS, LQD, WP, LB.\n";
                outResult = ss.str();
                return;
            }

            if (mgr.setLoadBalancePolicy(diskId, pol)) {
                ss << "[+] Successfully updated Disk " << diskId << " load balance policy to: "
                   << micant::mpio::MpioPolicyToString(pol) << "\n";
            } else {
                ss << "[-] Failed to set load balance policy for Disk " << diskId << " (disk not found).\n";
            }
        }
        else if (subCmd == "claim") {
            if (args.size() < 2) {
                ss << "[-] Syntax: mpio claim <hardware_id>\n";
                outResult = ss.str();
                return;
            }
            std::string_view hwid = args[1];
            if (mgr.claimHardwareId(hwid)) {
                ss << "[+] Successfully added hardware ID to MPIO claim list: \"" << hwid << "\"\n";
            } else {
                ss << "[-] Hardware ID is already registered or invalid: \"" << hwid << "\"\n";
            }
        }
        else if (subCmd == "unclaim") {
            if (args.size() < 2) {
                ss << "[-] Syntax: mpio unclaim <hardware_id>\n";
                outResult = ss.str();
                return;
            }
            std::string_view hwid = args[1];
            if (mgr.unclaimHardwareId(hwid)) {
                ss << "[+] Successfully removed hardware ID from MPIO claim list: \"" << hwid << "\"\n";
            } else {
                ss << "[-] Hardware ID not found in claim list: \"" << hwid << "\"\n";
            }
        }
        else if (subCmd == "claim-list") {
            auto list = mgr.getClaimedHardwareIds();
            ss << "===============================================================================\n"
               << "                   MPIO Registered Hardware Claim Identifiers                  \n"
               << "===============================================================================\n";
            for (size_t i = 0; i < list.size(); ++i) {
                ss << "  [" << (i + 1) << "] \"" << list[i] << "\"\n";
            }
            ss << "===============================================================================\n";
        }
        else if (subCmd == "test-failover") {
            if (args.size() < 3) {
                ss << "[-] Syntax: mpio test-failover <disk_id> <path_id_hex_or_dec>\n";
                outResult = ss.str();
                return;
            }
            uint32_t diskId = static_cast<uint32_t>(std::stoul(std::string(args[1])));
            uint32_t pathId = 0;
            std::string pathStr(args[2]);
            if (pathStr.starts_with("0x") || pathStr.starts_with("0X")) {
                pathId = static_cast<uint32_t>(std::stoul(pathStr, nullptr, 16));
            } else {
                pathId = static_cast<uint32_t>(std::stoul(pathStr));
            }

            if (mgr.failPath(diskId, pathId, "CLI Simulated Fault")) {
                ss << "[+] Path 0x" << std::hex << pathId << std::dec
                   << " on Disk " << diskId << " transitioned to FAILED.\n"
                   << "    Automatic failover engaged. Upper storage stack I/O routed to surviving paths.\n";
            } else {
                ss << "[-] Failed to simulate path failure: Disk or Path ID not found.\n";
            }
        }
        else if (subCmd == "test-failback") {
            if (args.size() < 3) {
                ss << "[-] Syntax: mpio test-failback <disk_id> <path_id_hex_or_dec>\n";
                outResult = ss.str();
                return;
            }
            uint32_t diskId = static_cast<uint32_t>(std::stoul(std::string(args[1])));
            uint32_t pathId = 0;
            std::string pathStr(args[2]);
            if (pathStr.starts_with("0x") || pathStr.starts_with("0X")) {
                pathId = static_cast<uint32_t>(std::stoul(pathStr, nullptr, 16));
            } else {
                pathId = static_cast<uint32_t>(std::stoul(pathStr));
            }

            if (mgr.recoverPath(diskId, pathId)) {
                ss << "[+] Path 0x" << std::hex << pathId << std::dec
                   << " on Disk " << diskId << " restored to healthy ACTIVE state.\n"
                   << "    Automatic failback completed. Path restored to active load balancing pool.\n";
            } else {
                ss << "[-] Failed to simulate path recovery: Disk or Path ID not found.\n";
            }
        }
        else if (subCmd == "pr-status") {
            if (args.size() < 2) {
                ss << "[-] Syntax: mpio pr-status <disk_id>\n";
                outResult = ss.str();
                return;
            }
            uint32_t diskId = static_cast<uint32_t>(std::stoul(std::string(args[1])));
            micant::mpio::MpioScsiReservation res{};
            if (mgr.getReservationStatus(diskId, res)) {
                ss << "===============================================================================\n"
                   << "          SCSI-3 Persistent Reservation Status for Disk " << diskId << "\n"
                   << "===============================================================================\n"
                   << "  Reservation Active    : " << (res.hasReservation ? "YES" : "NO") << "\n";
                if (res.hasReservation) {
                    ss << "  Reservation Type      : " << micant::mpio::ScsiPrTypeToString(res.reservationType) << "\n"
                       << "  Reservation Key       : 0x" << std::hex << res.reservationKey << std::dec << "\n"
                       << "  Holder Path ID        : 0x" << std::hex << res.reservationHolderPathId << std::dec << "\n";
                }
                ss << "  Registered Keys Count : " << res.registeredKeys.size() << "\n";
                for (const auto& [k, pid] : res.registeredKeys) {
                    ss << "    Key: 0x" << std::hex << k << " (Registered on Path 0x" << pid << ")\n" << std::dec;
                }
                ss << "===============================================================================\n";
            } else {
                ss << "[-] Disk " << diskId << " not found.\n";
            }
        }
        else if (subCmd == "dump") {
            ss << "# -----------------------------------------------------------------------------\n"
               << "# MicaNT Multipath I/O (MPIO) Declarative Configuration Export\n"
               << "# Generated automatically by mpiocli / mpio dump\n"
               << "# -----------------------------------------------------------------------------\n\n"
               << "# Enable Multipath I/O Feature\n"
               << "Enable-WindowsOptionalFeature -Online -FeatureName MultipathIO\n\n"
               << "# Set Global MSDSM Default Load Balancing Policy\n"
               << "Set-MSDSMGlobalDefaultLoadBalancePolicy -Policy RR\n\n"
               << "# Claim Supported Hardware IDs\n";
            auto list = mgr.getClaimedHardwareIds();
            for (const auto& hw : list) {
                ss << "New-MSDSMSupportedHW -VendorId \"" << hw.substr(0, std::min<size_t>(hw.size(), 8)) << "\" "
                   << "-ProductId \"" << (hw.size() > 8 ? hw.substr(8) : "") << "\"\n";
            }
            ss << "\n# Configure Individual MPIO Disks\n";
            auto devs = mgr.getAllDevices();
            for (const auto& d : devs) {
                std::string polCode = "RR";
                if (d.policy == micant::mpio::MpioLoadBalancePolicy::FailoverOnly) polCode = "FOO";
                else if (d.policy == micant::mpio::MpioLoadBalancePolicy::LeastQueueDepth) polCode = "LQD";
                else if (d.policy == micant::mpio::MpioLoadBalancePolicy::WeightedPaths) polCode = "WP";
                else if (d.policy == micant::mpio::MpioLoadBalancePolicy::LeastBlocks) polCode = "LB";
                else if (d.policy == micant::mpio::MpioLoadBalancePolicy::RoundRobinWithSubset) polCode = "RRWS";

                ss << "Set-MPIOLoadBalancePolicy -DiskNumber " << d.diskIndex << " -Policy " << polCode << "\n";
            }
            ss << "\n# End of Multipath I/O Configuration Export\n";
        }
        else {
            ss << "[-] Unknown mpio command: " << subCmd << "\n"
               << "    Type 'mpio help' for a list of available commands.\n";
        }

        outResult = ss.str();
    }

private:
    MpioCommandInterpreter() = default;

    void printHelp(std::ostream& os) const {
        os << "MicaNT Multipath I/O Administration Tool (mpiocli / mpio)\n"
           << "Copyright (C) 2026 MicaNT Authors.\n"
           << "Provides sovereign administration of storage multipath paths, ALUA states, and policies.\n\n"
           << "Usage:\n"
           << "  mpio status                               Display MPIO subsystem vitals and path summary\n"
           << "  mpio disks                                List all claimed MPIO virtual disks\n"
           << "  mpio paths [disk_id]                      Display detailed path matrix, ALUA states, and queue depths\n"
           << "  mpio set-policy <disk_id> <policy>        Configure load balance policy (RR, FOO, LQD, LB, WP, RRWS)\n"
           << "  mpio claim <hwid>                         Register hardware ID for automatic claiming\n"
           << "  mpio unclaim <hwid>                       Unregister hardware ID\n"
           << "  mpio claim-list                           List registered hardware claim IDs\n"
           << "  mpio test-failover <disk_id> <path_id>    Simulate path failure and trigger automatic failover\n"
           << "  mpio test-failback <disk_id> <path_id>    Simulate path recovery and trigger failback\n"
           << "  mpio pr-status <disk_id>                  Display SCSI-3 Persistent Reservation status and keys\n"
           << "  mpio dump                                 Export declarative PowerShell multipath script\n"
           << "  mpio help                                 Display this help message\n";
    }
};

inline void ExecuteMpioString(std::string_view commandLine, std::string& outResult) {
    auto tokens = Tokenize(commandLine);
    if (tokens.empty()) {
        outResult = "[-] No command specified.\n";
        return;
    }

    std::span<const std::string_view> args(tokens.data() + 1, tokens.size() - 1);
    MpioCommandInterpreter::get().execute(args, outResult);
}

} // namespace micant::mpio_cmd::um
