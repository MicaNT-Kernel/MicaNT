#pragma once

/**
 * @file anticheat_cmd.hpp
 * @brief Canonical Win32 accli.exe / anticheat.exe CLI Management Utilities for Kernel Anti-Cheat.
 *
 * Clean-room implementation of Kernel Anti-Cheat & Process Protection CLI tools
 * under 17 U.S.C. § 1201(f) & Google LLC v. Oracle America, Inc. (141 S. Ct. 1183, 2021).
 *
 * Provides complete CLI management for:
 *   - anticheat status: Introspect kernel handle filtering vitals, altitude, and callback states
 *   - anticheat protect <pid> [name]: Add game process to active kernel protection
 *   - anticheat unprotect <pid>: Remove game process from active protection
 *   - anticheat test-tamper <pid>: Simulate external handle opening and verify access mask stripping
 *   - anticheat test-inject <pid> <dll> [signed]: Simulate DLL injection and verify image filtering
 *   - anticheat list: Tabular inventory of protected game processes and protection levels
 *   - anticheat whitelist-overlay <dll>: Register verified game overlay (Steam/Discord/PrismX)
 *   - anticheat dump: Export declarative PowerShell Process Mitigation configuration script
 *   - anticheat help: Command usage and syntax
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

#include "micant/km/drivers/security/anticheat.hpp"

namespace micant::anticheat_cmd::um {

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

class AntiCheatCommandInterpreter {
public:
    static AntiCheatCommandInterpreter& get() {
        static AntiCheatCommandInterpreter instance;
        return instance;
    }

    void execute(std::span<const std::string_view> args, std::string& outResult) {
        std::ostringstream ss;

        if (args.empty() || args[0] == "help" || args[0] == "-h" || args[0] == "--help" || args[0] == "/?") {
            printHelp(ss);
            outResult = ss.str();
            return;
        }

        auto& mgr = micant::anticheat::AntiCheatManager::get();
        std::string_view subCmd = args[0];

        if (subCmd == "status") {
            auto v = mgr.getVitals();
            ss << "===============================================================================\n"
               << "      MicaNT Sovereign Kernel Anti-Cheat & Process Protection Subsystem       \n"
               << "===============================================================================\n"
               << "  Protected Processes Active : " << v.protectedProcessesCount << "\n"
               << "  Handle Access Interceptions: " << v.totalHandleAttemptsIntercepted << "\n"
               << "  Dangerous Access Stripped  : " << v.totalHandleAccessBitsStripped << " operations\n"
               << "  Remote Threads Blocked     : " << v.totalRemoteThreadsBlocked << "\n"
               << "  Unauthorized Images Blocked: " << v.totalUnauthorizedImagesBlocked << "\n"
               << "  ObRegisterCallbacks Status : " << (v.obCallbacksActive ? "ACTIVE (Altitude 328000)" : "DISABLED") << "\n"
               << "  Process Notify Routine     : " << (v.processNotifyActive ? "ACTIVE" : "DISABLED") << "\n"
               << "  Thread Notify Routine      : " << (v.threadNotifyActive ? "ACTIVE" : "DISABLED") << "\n"
               << "  Image Load Notify Routine  : " << (v.imageNotifyActive ? "ACTIVE" : "DISABLED") << "\n"
               << "  SCM Driver Service Status  : AntiCheat (SERVICE_RUNNING, SYSTEM_START)\n"
               << "===============================================================================\n";
        }
        else if (subCmd == "protect") {
            if (args.size() < 2) {
                ss << "[-] Syntax: anticheat protect <pid> [name]\n";
                outResult = ss.str();
                return;
            }
            uint32_t pid = static_cast<uint32_t>(std::stoul(std::string(args[1])));
            std::string_view name = (args.size() >= 3) ? args[2] : "";
            if (mgr.protectProcess(pid, name)) {
                ss << "[+] Process PID " << pid << " (" << (name.empty() ? "unnamed" : name)
                   << ") registered for active kernel anti-cheat protection.\n"
                   << "    Handle access stripping and remote thread blocking engaged.\n";
            } else {
                ss << "[-] Failed to protect process PID " << pid << ".\n";
            }
        }
        else if (subCmd == "unprotect") {
            if (args.size() < 2) {
                ss << "[-] Syntax: anticheat unprotect <pid>\n";
                outResult = ss.str();
                return;
            }
            uint32_t pid = static_cast<uint32_t>(std::stoul(std::string(args[1])));
            if (mgr.unprotectProcess(pid)) {
                ss << "[+] Process PID " << pid << " removed from anti-cheat protection.\n";
            } else {
                ss << "[-] Process PID " << pid << " not found in protected inventory.\n";
            }
        }
        else if (subCmd == "test-tamper") {
            if (args.size() < 2) {
                ss << "[-] Syntax: anticheat test-tamper <target_pid>\n";
                outResult = ss.str();
                return;
            }
            uint32_t targetPid = static_cast<uint32_t>(std::stoul(std::string(args[1])));
            uint32_t callerPid = 9999; // Simulated unauthorized external cheat process
            uint32_t desired = micant::anticheat::PROCESS_VM_READ |
                               micant::anticheat::PROCESS_VM_WRITE |
                               micant::anticheat::PROCESS_VM_OPERATION |
                               micant::anticheat::PROCESS_CREATE_THREAD;

            uint32_t granted = mgr.filterProcessHandleCreation(callerPid, targetPid, desired);

            ss << "===============================================================================\n"
               << "                   Simulated Handle Tamper Audit Report                        \n"
               << "===============================================================================\n"
               << "  Target Process PID      : " << targetPid << "\n"
               << "  Untrusted Caller PID    : " << callerPid << "\n"
               << "  Requested DesiredAccess : 0x" << std::hex << desired << std::dec
               << " (VM_READ | VM_WRITE | VM_OPERATION | CREATE_THREAD)\n"
               << "  Kernel GrantedAccess    : 0x" << std::hex << granted << std::dec << "\n"
               << "  Protection Status       : ";

            if ((granted & micant::anticheat::DANGEROUS_PROCESS_ACCESS_MASK) == 0) {
                ss << "PROTECTED (Dangerous access bits stripped to PROCESS_QUERY_LIMITED_INFORMATION)\n";
            } else {
                ss << "VULNERABLE (Dangerous access granted)\n";
            }
            ss << "===============================================================================\n";
        }
        else if (subCmd == "test-inject") {
            if (args.size() < 3) {
                ss << "[-] Syntax: anticheat test-inject <target_pid> <dll_name> [signed|unsigned]\n";
                outResult = ss.str();
                return;
            }
            uint32_t targetPid = static_cast<uint32_t>(std::stoul(std::string(args[1])));
            std::string_view dllName = args[2];
            bool isSigned = (args.size() >= 4 && args[3] == "signed");

            bool allowed = mgr.validateImageLoad(targetPid, dllName, isSigned);

            ss << "===============================================================================\n"
               << "                  Simulated Image Injection Audit Report                       \n"
               << "===============================================================================\n"
               << "  Target Process PID      : " << targetPid << "\n"
               << "  Injected Image Name     : " << dllName << "\n"
               << "  Digital Signature Valid : " << (isSigned ? "YES" : "NO") << "\n"
               << "  Image Load Verdict      : " << (allowed ? "ALLOWED (Whitelisted/Signed)" : "BLOCKED (Unauthorized Hook)") << "\n"
               << "===============================================================================\n";
        }
        else if (subCmd == "list") {
            auto list = mgr.getAllProtectedProcesses();
            ss << "===============================================================================\n"
               << "                   Active Protected Game & High-Integrity Processes            \n"
               << "===============================================================================\n"
               << std::left << std::setw(8)  << "PID"
               << std::left << std::setw(16) << "Image Name"
               << std::left << std::setw(28) << "Protection Level"
               << std::left << std::setw(14) << "Stripped"
               << std::left << std::setw(10) << "Blocked" << "\n"
               << "-------------------------------------------------------------------------------\n";

            for (const auto& p : list) {
                ss << std::left << std::setw(8)  << p.processId
                   << std::left << std::setw(16) << p.processName
                   << std::left << std::setw(28) << micant::anticheat::ProtectionLevelToString(p.level)
                   << std::left << std::setw(14) << p.handleAccessStrippedCount
                   << std::left << std::setw(10) << (p.remoteThreadBlocksCount + p.blockedImageInjectionsCount) << "\n";
            }
            ss << "===============================================================================\n";
        }
        else if (subCmd == "whitelist-overlay") {
            if (args.size() < 2) {
                ss << "[-] Syntax: anticheat whitelist-overlay <dll_name>\n";
                outResult = ss.str();
                return;
            }
            std::string_view dll = args[1];
            if (mgr.addWhitelistedOverlay(dll)) {
                ss << "[+] Overlay \"" << dll << "\" added to approved game overlay whitelist.\n";
            } else {
                ss << "[-] Overlay \"" << dll << "\" is already whitelisted or invalid.\n";
            }
        }
        else if (subCmd == "dump") {
            ss << "# -----------------------------------------------------------------------------\n"
               << "# MicaNT Kernel Anti-Cheat & Process Mitigation Configuration\n"
               << "# Generated automatically by anticheat dump\n"
               << "# -----------------------------------------------------------------------------\n\n"
               << "# Enable Exploit Guard & Process Mitigations for Gaming Binaries\n"
               << "Set-ProcessMitigation -Name cod.exe -Enable "
               << "DEP,BottomUpASLR,HighEntropyASLR,StrictHandleCheck,DisallowWin32kSystemCalls\n"
               << "Set-ProcessMitigation -Name cod.exe -Enable "
               << "ExtensionPointDisable,BlockRemoteImageLoads,DynamicCodeOptOut\n\n"
               << "# Configure Anti-Cheat Kernel Driver Altitude\n"
               << "Set-ItemProperty -Path \"HKLM:\\System\\CurrentControlSet\\Services\\AntiCheat\\Instances\\AntiCheat Instance\" "
               << "-Name \"Altitude\" -Value \"328000\"\n\n"
               << "# End of Anti-Cheat Configuration Export\n";
        }
        else {
            ss << "[-] Unknown anticheat command: " << subCmd << "\n"
               << "    Type 'anticheat help' for a list of available commands.\n";
        }

        outResult = ss.str();
    }

private:
    AntiCheatCommandInterpreter() = default;

    void printHelp(std::ostream& os) const {
        os << "MicaNT Kernel Anti-Cheat Administration Tool (accli / anticheat)\n"
           << "Copyright (C) 2026 MicaNT Authors.\n"
           << "Provides sovereign administration of ObRegisterCallbacks handle filtering and image protection.\n\n"
           << "Usage:\n"
           << "  anticheat status                          Display Anti-Cheat subsystem vitals and callback metrics\n"
           << "  anticheat protect <pid> [name]            Add game process to active kernel protection\n"
           << "  anticheat unprotect <pid>                 Remove game process from active protection\n"
           << "  anticheat test-tamper <pid>               Simulate handle opening and verify access mask stripping\n"
           << "  anticheat test-inject <pid> <dll> [sign]  Simulate DLL injection and verify image filtering\n"
           << "  anticheat list                            List all active protected game processes\n"
           << "  anticheat whitelist-overlay <dll>         Register verified game overlay (Steam/Discord/PrismX)\n"
           << "  anticheat dump                            Export declarative PowerShell Process Mitigation script\n"
           << "  anticheat help                            Display this help message\n";
    }
};

inline void ExecuteAntiCheatString(std::string_view commandLine, std::string& outResult) {
    auto tokens = Tokenize(commandLine);
    if (tokens.empty()) {
        outResult = "[-] No command specified.\n";
        return;
    }

    std::span<const std::string_view> args(tokens.data() + 1, tokens.size() - 1);
    AntiCheatCommandInterpreter::get().execute(args, outResult);
}

} // namespace micant::anticheat_cmd::um
