#pragma once

/**
 * @file hyperv_hypercall_cmd.hpp
 * @brief Canonical Win32 hvc.exe / hvconfig.exe CLI Management Utility for Hyper-V Hypercall & Nested Virtualization.
 *
 * Clean-room implementation of the Microsoft Windows Server Hyper-V Hypercall &
 * Nested Hardware Virtualization CLI under 17 U.S.C. § 1201(f) &
 * Google LLC v. Oracle America, Inc. (141 S. Ct. 1183, 2021).
 *
 * Provides complete CLI management for:
 *   - hvc status / query: hypervisor code page GPA, TSC invariant calibration, telemetry
 *   - hvc partitions / partition list: tabular inventory of guest partitions and nested states
 *   - hvc partition show <id>: detailed partition inspection with per-VP eVMCS state
 *   - hvc nested <enable|disable> <id>: toggle nested virtualization on guest partition
 *   - hvc call <code_hex> [--fast] [--param <hex>]: dispatch hypercall to hypervisor engine
 *   - hvc evmcs sync <part_id> <vp_id> <dirty_mask_hex>: synchronize eVMCS clean fields
 *   - hvc inject-exit <part_id> <vp_id> <reason> [qual_hex]: inject reflective nested VM-Exit
 *   - hvc tsc set <seq> <scale_hex> <offset>: calibrate invariant Reference TSC page
 *   - hvc dump: export declarative PowerShell configuration script
 *   - hvc help: syntax and synopsis
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

#include "micant/um/servers/hyperv.hpp"

namespace micant::hvc_cmd::um {

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

inline std::string FormatHex64(uint64_t val) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(16) << val;
    return oss.str();
}

inline std::string FormatHex32(uint32_t val) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << val;
    return oss.str();
}

inline std::string FormatHex16(uint16_t val) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << val;
    return oss.str();
}

inline int ExecuteHvc(std::span<std::string_view> args, std::string& output) {
    std::ostringstream out;
    auto& hv = micant::hyperv::HypervSubsystem::get();
    hv.initialize();

    if (args.empty() || args[0] == "help" || args[0] == "/?" || args[0] == "-h" || args[0] == "--help") {
        out << "MicaNT Hyper-V Hypercall & Nested Virtualization CLI (hvc.exe) [Windows Server Compatible]\n"
            << "Copyright (C) 2026 MicaNT Authors. Windows, Windows Server, and Hyper-V are trademarks of Microsoft Corporation.\n\n"
            << "Usage: hvc <command> [parameters...]\n\n"
            << "Commands:\n"
            << "  status | query               Display hypervisor status, hypercall page GPA & telemetry\n"
            << "  partitions | list            List all active guest partitions\n"
            << "  partition show <id>          Show detailed partition and Virtual Processor (VP) state\n"
            << "  nested enable <id>           Enable nested hardware virtualization for partition\n"
            << "  nested disable <id>          Disable nested hardware virtualization for partition\n"
            << "  call <code> [--fast] [--param <hex>]  Dispatch hypercall to hypervisor engine\n"
            << "  evmcs sync <part> <vp> <mask> Synchronize Enlightened VMCS clean field mask\n"
            << "  inject-exit <part> <vp> <reason> [qual] Inject reflective nested VM-Exit\n"
            << "  tsc set <seq> <scale> <offset> Calibrate Reference TSC invariant page\n"
            << "  dump                         Export declarative PowerShell configuration\n";
        output = out.str();
        return 0;
    }

    std::string_view verb = args[0];

    // ------------------------------------------------------------------------
    // status | query
    // ------------------------------------------------------------------------
    if (verb == "status" || verb == "query") {
        const auto& tsc = hv.getReferenceTsc();
        out << "Hyper-V Hypervisor Subsystem Status:\n"
            << "========================================================================\n"
            << "  Operational State:         ACTIVE (TitanHypervisor / AegisNestedVM)\n"
            << "  Core Hypervisor Driver:    hvix64.sys [Version 10.0.26100.1]\n"
            << "  Platform Runtime Driver:   winhvr.sys [Version 10.0.26100.1]\n"
            << "  Specification Parity:      Hyper-V TLFS v6.0b / Enlightened VMCS v1\n"
            << "  Hypercall Page GPA:        " << FormatHex64(hv.getHypercallPageGpa()) << "\n"
            << "  Hypercall Page Enabled:    " << (hv.isHypercallPageEnabled() ? "TRUE" : "FALSE") << "\n"
            << "  Reference TSC Sequence:    " << tsc.tscSequence << "\n"
            << "  Reference TSC Scale:       " << FormatHex64(tsc.tscScale) << " (1.0 Fixed-Point)\n"
            << "  Reference TSC Offset:      " << tsc.tscOffset << " ticks\n"
            << "  Active Guest Partitions:   " << hv.getPartitionCount() << "\n"
            << "------------------------------------------------------------------------\n"
            << "Telemetry Counters:\n"
            << "  Total Hypercalls:          " << hv.getTotalHypercalls() << "\n"
            << "  Fast Register Hypercalls:  " << hv.getFastHypercalls() << "\n"
            << "  Nested VM-Exits:           " << hv.getNestedVmExits() << "\n"
            << "  eVMCS Clean Field Flushes: " << hv.getEvmcsFlushes() << "\n"
            << "========================================================================\n";
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // partitions | list
    // ------------------------------------------------------------------------
    if (verb == "partitions" || verb == "list" || (verb == "partition" && args.size() > 1 && args[1] == "list")) {
        auto parts = hv.getAllPartitions();
        out << "Hyper-V Guest Partitions (" << parts.size() << " partitions):\n"
            << "--------------------------------------------------------------------------------\n"
            << std::left << std::setw(6) << "ID"
            << std::setw(30) << "Name"
            << std::setw(12) << "Memory (MB)"
            << std::setw(12) << "Nested VMM"
            << std::setw(8)  << "VPs" << "\n"
            << "--------------------------------------------------------------------------------\n";

        for (const auto& p : parts) {
            out << std::left << std::setw(6) << p->getId()
                << std::setw(30) << p->getName()
                << std::setw(12) << p->getMemoryMb()
                << std::setw(12) << (p->isNestedEnabled() ? "ENABLED" : "DISABLED")
                << std::setw(8)  << p->getVpCount() << "\n";
        }
        out << "--------------------------------------------------------------------------------\n";
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // partition show <id>
    // ------------------------------------------------------------------------
    if ((verb == "partition" && args.size() > 2 && args[1] == "show") || (verb == "show" && args.size() > 1)) {
        std::string_view idStr = (verb == "partition") ? args[2] : args[1];
        uint32_t partId = 0;
        try {
            partId = static_cast<uint32_t>(std::stoul(std::string(idStr)));
        } catch (...) {
            output = "Error: Invalid partition ID format.\n";
            return 1;
        }

        auto p = hv.getPartition(partId);
        if (!p) {
            out << "Error: Partition " << partId << " not found.\n";
            output = out.str();
            return 1;
        }

        out << "Partition Details for ID " << p->getId() << " (" << p->getName() << "):\n"
            << "========================================================================\n"
            << "  Assigned Memory:           " << p->getMemoryMb() << " MB\n"
            << "  Nested Hardware VMM:       " << (p->isNestedEnabled() ? "ENABLED (L1/L2 Active)" : "DISABLED") << "\n"
            << "  Virtual Processor Count:   " << p->getVpCount() << "\n"
            << "------------------------------------------------------------------------\n"
            << "Virtual Processors:\n";

        auto vps = p->getAllVirtualProcessors();
        for (const auto& vp : vps) {
            const auto& evmcs = vp->getEnlightenedVmcs();
            const auto& vpap = vp->getVpap();
            out << "  [VP " << vp->getVpIndex() << "]\n"
                << "    Execution Level:         L" << vp->getExecutionLevel() << (vp->isNestedActive() ? " (Nested Guest)" : " (Guest Hypervisor)") << "\n"
                << "    eVMCS GPA:               " << FormatHex64(vpap.enlightenedVmcsGpa) << "\n"
                << "    eVMCS Revision:          " << evmcs.revisionId << "\n"
                << "    Clean Fields Mask:       " << FormatHex32(evmcs.cleanFieldsMask) << "\n"
                << "    Guest RIP:               " << FormatHex64(evmcs.guestRip) << "\n"
                << "    Guest RSP:               " << FormatHex64(evmcs.guestRsp) << "\n"
                << "    Guest CR3:               " << FormatHex64(evmcs.guestCr3) << "\n"
                << "    Total VM-Exits:          " << vp->getVmExits() << "\n"
                << "    Nested VM-Exits:         " << vp->getNestedVmExits() << "\n";
        }
        out << "========================================================================\n";
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // nested <enable|disable> <id>
    // ------------------------------------------------------------------------
    if (verb == "nested" && args.size() > 2) {
        std::string_view action = args[1];
        std::string_view idStr = args[2];
        uint32_t partId = 0;
        try {
            partId = static_cast<uint32_t>(std::stoul(std::string(idStr)));
        } catch (...) {
            output = "Error: Invalid partition ID format.\n";
            return 1;
        }

        auto p = hv.getPartition(partId);
        if (!p) {
            out << "Error: Partition " << partId << " not found.\n";
            output = out.str();
            return 1;
        }

        if (action == "enable") {
            p->enableNestedVirtualization(true);
            out << "SUCCESS: Nested hardware virtualization ENABLED for partition " << partId << " (" << p->getName() << ").\n";
        } else if (action == "disable") {
            p->enableNestedVirtualization(false);
            out << "SUCCESS: Nested hardware virtualization DISABLED for partition " << partId << " (" << p->getName() << ").\n";
        } else {
            out << "Error: Unknown action '" << action << "'. Use 'enable' or 'disable'.\n";
            output = out.str();
            return 1;
        }
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // call <code> [--fast] [--param <hex>]
    // ------------------------------------------------------------------------
    if (verb == "call" && args.size() > 1) {
        uint16_t callCode = 0;
        try {
            callCode = static_cast<uint16_t>(std::stoul(std::string(args[1]), nullptr, 16));
        } catch (...) {
            output = "Error: Invalid hypercall code hex format.\n";
            return 1;
        }

        bool isFast = false;
        uint64_t inParam = 0;
        for (size_t i = 2; i < args.size(); ++i) {
            if (args[i] == "--fast" || args[i] == "/fast") {
                isFast = true;
            } else if ((args[i] == "--param" || args[i] == "/param") && i + 1 < args.size()) {
                try {
                    inParam = std::stoull(std::string(args[++i]), nullptr, 16);
                } catch (...) {}
            }
        }

        uint64_t outVal = 0;
        uint16_t status = hv.invokeHypercall(callCode, isFast, inParam, &outVal);

        out << "Hypercall Invocation Result:\n"
            << "------------------------------------------------------------------------\n"
            << "  Call Code:                 " << FormatHex16(callCode) << "\n"
            << "  Calling Convention:        " << (isFast ? "Fast (RCX/RDX Registers)" : "Standard (Memory Buffered)") << "\n"
            << "  Input Parameter:           " << FormatHex64(inParam) << "\n"
            << "  TLFS Status:               " << FormatHex16(status) << " (" << ((status == micant::hyperv::HV_STATUS_SUCCESS) ? "HV_STATUS_SUCCESS" : "ERROR") << ")\n"
            << "  Output Value:              " << FormatHex64(outVal) << "\n"
            << "------------------------------------------------------------------------\n";
        output = out.str();
        return (status == micant::hyperv::HV_STATUS_SUCCESS) ? 0 : 1;
    }

    // ------------------------------------------------------------------------
    // evmcs sync <part> <vp> <mask>
    // ------------------------------------------------------------------------
    if (verb == "evmcs" && args.size() > 4 && args[1] == "sync") {
        uint32_t partId = 0, vpIndex = 0, dirtyMask = 0;
        try {
            partId = static_cast<uint32_t>(std::stoul(std::string(args[2])));
            vpIndex = static_cast<uint32_t>(std::stoul(std::string(args[3])));
            dirtyMask = static_cast<uint32_t>(std::stoul(std::string(args[4]), nullptr, 16));
        } catch (...) {
            output = "Error: Invalid numeric parameters for evmcs sync.\n";
            return 1;
        }

        bool ok = hv.syncEnlightenedVmcs(partId, vpIndex, dirtyMask);
        if (!ok) {
            out << "Error: Failed to sync eVMCS for Partition " << partId << " VP " << vpIndex << ".\n";
            output = out.str();
            return 1;
        }

        out << "SUCCESS: Synchronized eVMCS for Partition " << partId << " VP " << vpIndex
            << " with dirty mask " << FormatHex32(dirtyMask) << ".\n";
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // inject-exit <part> <vp> <reason> [qual]
    // ------------------------------------------------------------------------
    if (verb == "inject-exit" && args.size() > 3) {
        uint32_t partId = 0, vpIndex = 0;
        try {
            partId = static_cast<uint32_t>(std::stoul(std::string(args[1])));
            vpIndex = static_cast<uint32_t>(std::stoul(std::string(args[2])));
        } catch (...) {
            output = "Error: Invalid numeric parameters for inject-exit.\n";
            return 1;
        }

        std::string_view rStr = args[3];
        micant::hyperv::NestedExitReason reason = micant::hyperv::NestedExitReason::Cpuid;
        if (rStr == "cpuid" || rStr == "CPUID") {
            reason = micant::hyperv::NestedExitReason::Cpuid;
        } else if (rStr == "vmcall" || rStr == "VMCALL") {
            reason = micant::hyperv::NestedExitReason::Vmcall;
        } else if (rStr == "cr_access" || rStr == "CR_ACCESS") {
            reason = micant::hyperv::NestedExitReason::CrAccess;
        } else if (rStr == "msr_read" || rStr == "MSR_READ") {
            reason = micant::hyperv::NestedExitReason::MsrRead;
        } else if (rStr == "msr_write" || rStr == "MSR_WRITE") {
            reason = micant::hyperv::NestedExitReason::MsrWrite;
        } else if (rStr == "ept_violation" || rStr == "EPT_VIOLATION") {
            reason = micant::hyperv::NestedExitReason::EptViolation;
        } else if (rStr == "ept_misconfig" || rStr == "EPT_MISCONFIG") {
            reason = micant::hyperv::NestedExitReason::EptMisconfig;
        }

        uint64_t qual = 0;
        if (args.size() > 4) {
            try {
                qual = std::stoull(std::string(args[4]), nullptr, 16);
            } catch (...) {}
        }

        bool ok = hv.injectNestedVmExit(partId, vpIndex, reason, qual);
        if (!ok) {
            out << "Error: Failed to inject nested VM-Exit for Partition " << partId << " VP " << vpIndex << ".\n";
            output = out.str();
            return 1;
        }

        out << "SUCCESS: Injected reflective nested VM-Exit (" << micant::hyperv::NestedExitReasonToString(reason)
            << ") on Partition " << partId << " VP " << vpIndex
            << " (Exit Qualification: " << FormatHex64(qual) << "). Transitioned back to L1.\n";
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // tsc set <seq> <scale> <offset>
    // ------------------------------------------------------------------------
    if (verb == "tsc" && args.size() > 4 && args[1] == "set") {
        uint32_t seq = 0;
        uint64_t scale = 0;
        int64_t offset = 0;
        try {
            seq = static_cast<uint32_t>(std::stoul(std::string(args[2])));
            scale = std::stoull(std::string(args[3]), nullptr, 16);
            offset = std::stoll(std::string(args[4]));
        } catch (...) {
            output = "Error: Invalid numeric parameters for tsc set.\n";
            return 1;
        }

        hv.setReferenceTsc(seq, scale, offset);
        out << "SUCCESS: Reference TSC invariant page updated: Sequence=" << seq
            << ", Scale=" << FormatHex64(scale)
            << ", Offset=" << offset << ".\n";
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // dump
    // ------------------------------------------------------------------------
    if (verb == "dump") {
        out << "# MicaNT Sovereign Hyper-V Hypercall & Nested Virtualization Export\n"
            << "# Generated: 2026-11-01 [TLFS v6.0b / Enlightened VMCS v1]\n\n"
            << "# Core Drivers\n"
            << "sc.exe config HypervService start= auto\n"
            << "net start HypervService\n\n"
            << "# Nested Virtualization Configuration\n";

        for (const auto& p : hv.getAllPartitions()) {
            if (p->getId() != 0) { // Skip host
                out << "Set-VMProcessor -VMName \"" << p->getName() << "\" -ExposeVirtualizationExtensions $"
                    << (p->isNestedEnabled() ? "true" : "false") << "\n";
            }
        }
        output = out.str();
        return 0;
    }

    out << "Error: Unknown hvc command '" << verb << "'. Type 'hvc help' for usage.\n";
    output = out.str();
    return 1;
}

inline int ExecuteHvcString(std::string_view commandLine, std::string& output) {
    auto tokens = Tokenize(commandLine);
    if (tokens.empty()) {
        output.clear();
        return 0;
    }
    // Skip executable token if provided (hvc or hvconfig)
    if (tokens[0] == "hvc" || tokens[0] == "hvc.exe" || tokens[0] == "hvconfig" || tokens[0] == "hvconfig.exe") {
        tokens.erase(tokens.begin());
    }
    return ExecuteHvc(tokens, output);
}

} // namespace micant::hvc_cmd::um
