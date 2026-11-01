#pragma once

/**
 * @file hyperv_cmd.hpp
 * @brief Canonical Win32 hyperv.exe / vmms.exe CLI Management Utility for Hyper-V.
 *
 * Clean-room implementation of the Microsoft Windows Server Hyper-V &
 * Virtual Machine Management Service (VMMS) CLI under 17 U.S.C. § 1201(f) &
 * Google LLC v. Oracle America, Inc. (141 S. Ct. 1183, 2021).
 *
 * Provides complete CLI management for:
 *   - hyperv status: global virtualization subsystem health & topology
 *   - hyperv list / vms: tabular inventory of defined virtual machines
 *   - hyperv create <name> [/vcpu:<n>] [/ram:<MB>] [/vhdx:<path>]: provision VM
 *   - hyperv start <id|name>: power on virtual machine
 *   - hyperv stop <id|name> [/force]: power off virtual machine
 *   - hyperv pause <id|name>: pause execution
 *   - hyperv resume <id|name>: resume execution
 *   - hyperv save <id|name>: save VM state to disk
 *   - hyperv balloon <id|name> <ram_mb>: dynamic memory allocation adjustment
 *   - hyperv switch / vswitch: inspect extensible virtual switch ports & VLANs
 *   - hyperv vhdx <path>: inspect virtual hard disk container & differential chain
 *   - hyperv migrate <id|name> [destHost]: execute live migration pre-copy & brownout
 *   - hyperv dump: declarative script configuration export
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

#include "micant/um/servers/vmms.hpp"

namespace micant::hyperv_cmd::um {

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
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    if (bytes >= (1ULL << 40)) oss << (static_cast<double>(bytes) / (1ULL << 40)) << " TB";
    else if (bytes >= (1ULL << 30)) oss << (static_cast<double>(bytes) / (1ULL << 30)) << " GB";
    else if (bytes >= (1ULL << 20)) oss << (static_cast<double>(bytes) / (1ULL << 20)) << " MB";
    else if (bytes >= (1ULL << 10)) oss << (static_cast<double>(bytes) / (1ULL << 10)) << " KB";
    else oss << bytes << " B";
    return oss.str();
}

inline int ExecuteHyperV(std::span<std::string_view> args, std::string& output) {
    std::ostringstream out;
    auto& vmms = micant::vmms::VmmsSubsystem::get();
    vmms.initialize();

    if (args.empty()) {
        out << "Microsoft Windows [Version 10.0.26100.1]\n"
            << "Hyper-V Virtual Machine Management Utility (hyperv.exe / vmms.exe)\n\n"
            << "Usage: hyperv <command> [arguments]\n\n"
            << "Commands:\n"
            << "  status                          Display Hyper-V subsystem health & resource vitals\n"
            << "  list | vms                      Display inventory of defined virtual machines\n"
            << "  create <name> [/vcpu:N] [/ram:MB] [/vhdx:PATH]  Provision a new virtual machine\n"
            << "  start <id|name>                 Power on virtual machine\n"
            << "  stop <id|name> [/force]         Power off virtual machine\n"
            << "  pause <id|name>                 Pause running virtual machine\n"
            << "  resume <id|name>                Resume paused virtual machine\n"
            << "  save <id|name>                  Save VM memory state to disk\n"
            << "  balloon <id|name> <ram_mb>      Adjust dynamic memory allocation\n"
            << "  switch | vswitch                Inspect Hyper-V Extensible Virtual Switch\n"
            << "  vhdx <path>                     Inspect VHDX virtual hard disk container\n"
            << "  migrate <id|name> [destHost]    Execute live migration pre-copy & brownout\n"
            << "  dump                            Export declarative hypervisor configuration script\n";
        output = out.str();
        return 0;
    }

    std::string verb(args[0]);
    std::transform(verb.begin(), verb.end(), verb.begin(), ::tolower);

    // ------------------------------------------------------------------------
    // hyperv status
    // ------------------------------------------------------------------------
    if (verb == "status") {
        out << "Hyper-V Virtual Machine Management Subsystem Status:\n"
            << "================================================================================\n"
            << "  Subsystem State:             ACTIVE (TitanHyperCore / AegisVMM)\n"
            << "  Hypervisor Version:          10.0.26100.1 (Enlightened VMCS / Nested VT-x/SVM)\n"
            << "  Total Defined VMs:           " << vmms.getVirtualMachineCount() << "\n"
            << "  Running Virtual Machines:    " << vmms.getRunningVirtualMachineCount() << "\n"
            << "  Extensible Virtual Switches: " << vmms.getAllSwitches().size() << "\n"
            << "  Mounted VHDX Containers:     " << vmms.getVhdxCount() << "\n"
            << "  Active Memory Allocated:     " << vmms.getTotalAllocatedRamMb() << " MB\n"
            << "  Virtual Infrastructure:      C:\\Windows\\System32\\drivers\\vid.sys\n"
            << "  Virtual Switch Filter:       C:\\Windows\\System32\\drivers\\vmswitch.sys\n";
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // hyperv list | vms
    // ------------------------------------------------------------------------
    if (verb == "list" || verb == "vms") {
        auto vms = vmms.getAllVirtualMachines();
        out << "Hyper-V Virtual Machines Inventory (" << vms.size() << " VMs):\n"
            << "--------------------------------------------------------------------------------\n"
            << std::left << std::setw(15) << "VM ID"
            << std::setw(18) << "Name"
            << std::setw(12) << "State"
            << std::setw(8)  << "vCPUs"
            << std::setw(14) << "Memory (MB)"
            << "VHDX Attachments\n"
            << "--------------------------------------------------------------------------------\n";
        for (const auto& vm : vms) {
            std::string vhdxStr;
            for (const auto& p : vm.getAttachedVhdx()) {
                if (!vhdxStr.empty()) vhdxStr += ", ";
                vhdxStr += p;
            }
            if (vhdxStr.empty()) vhdxStr = "None";

            std::string memStr = std::to_string(vm.getMemory().currentAllocatedMb) + " MB";

            out << std::left << std::setw(15) << vm.getId()
                << std::setw(18) << vm.getName()
                << std::setw(12) << micant::vmms::VmStateToString(vm.getState())
                << std::setw(8)  << vm.getTopology().vCpuCount
                << std::setw(14) << memStr
                << vhdxStr << "\n";
        }
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // hyperv create <name> [/vcpu:N] [/ram:MB] [/vhdx:PATH]
    // ------------------------------------------------------------------------
    if (verb == "create") {
        if (args.size() < 2) {
            out << "Error: VM name is required. Usage: hyperv create <name> [/vcpu:N] [/ram:MB] [/vhdx:PATH]\n";
            output = out.str();
            return 1;
        }

        std::string name(args[1]);
        uint32_t vcpus = 4;
        uint32_t ramMb = 4096;
        std::string vhdxPath;

        for (size_t i = 2; i < args.size(); ++i) {
            std::string opt(args[i]);
            if (opt.starts_with("/vcpu:") || opt.starts_with("-vcpu:")) {
                vcpus = static_cast<uint32_t>(std::strtoul(opt.substr(6).c_str(), nullptr, 10));
                if (vcpus == 0) vcpus = 1;
            } else if (opt.starts_with("/ram:") || opt.starts_with("-ram:")) {
                ramMb = static_cast<uint32_t>(std::strtoul(opt.substr(5).c_str(), nullptr, 10));
                if (ramMb == 0) ramMb = 1024;
            } else if (opt.starts_with("/vhdx:") || opt.starts_with("-vhdx:")) {
                vhdxPath = opt.substr(6);
            }
        }

        std::string vmId = "VM-UUID-" + std::to_string(1000 + vmms.getVirtualMachineCount() + 1);
        micant::vmms::VirtualMachine newVm(vmId, name, vcpus, ramMb);
        if (!vhdxPath.empty()) {
            newVm.attachVhdx(vhdxPath);
        }

        if (vmms.addVirtualMachine(newVm)) {
            out << "Virtual machine '" << name << "' created successfully.\n"
                << "  VM ID:          " << vmId << "\n"
                << "  vCPUs:          " << vcpus << "\n"
                << "  Startup Memory: " << ramMb << " MB\n"
                << "  VHDX Disk:      " << (vhdxPath.empty() ? "None" : vhdxPath) << "\n"
                << "  State:          Off\n";
            output = out.str();
            return 0;
        } else {
            out << "Error: Failed to create virtual machine '" << name << "' (duplicate ID or name).\n";
            output = out.str();
            return 1;
        }
    }

    // Helper lambda to find VM by ID or Name
    auto findVm = [&](std::string_view target) -> micant::vmms::VirtualMachine* {
        std::string t(target);
        auto* vm = vmms.getVirtualMachine(t);
        if (!vm) vm = vmms.getVirtualMachineByName(t);
        return vm;
    };

    // ------------------------------------------------------------------------
    // hyperv start <id|name>
    // ------------------------------------------------------------------------
    if (verb == "start") {
        if (args.size() < 2) {
            out << "Error: VM ID or name is required. Usage: hyperv start <id|name>\n";
            output = out.str();
            return 1;
        }

        auto* vm = findVm(args[1]);
        if (!vm) {
            out << "Error: Virtual machine '" << args[1] << "' not found.\n";
            output = out.str();
            return 1;
        }

        if (vm->start()) {
            out << "Virtual machine '" << vm->getName() << "' (" << vm->getId() << ") started successfully.\n"
                << "  State: Running\n";
            output = out.str();
            return 0;
        } else {
            out << "Error: Could not start VM '" << vm->getName() << "' (Current state: "
                << micant::vmms::VmStateToString(vm->getState()) << ").\n";
            output = out.str();
            return 1;
        }
    }

    // ------------------------------------------------------------------------
    // hyperv stop <id|name> [/force]
    // ------------------------------------------------------------------------
    if (verb == "stop") {
        if (args.size() < 2) {
            out << "Error: VM ID or name is required. Usage: hyperv stop <id|name> [/force]\n";
            output = out.str();
            return 1;
        }

        bool force = false;
        if (args.size() >= 3 && (args[2] == "/force" || args[2] == "-force")) force = true;

        auto* vm = findVm(args[1]);
        if (!vm) {
            out << "Error: Virtual machine '" << args[1] << "' not found.\n";
            output = out.str();
            return 1;
        }

        if (vm->stop(force)) {
            out << "Virtual machine '" << vm->getName() << "' (" << vm->getId() << ") stopped successfully.\n"
                << "  State: Off\n";
            output = out.str();
            return 0;
        } else {
            out << "Error: Could not stop VM '" << vm->getName() << "' (Already Off).\n";
            output = out.str();
            return 1;
        }
    }

    // ------------------------------------------------------------------------
    // hyperv pause <id|name>
    // ------------------------------------------------------------------------
    if (verb == "pause") {
        if (args.size() < 2) {
            out << "Error: VM ID or name is required. Usage: hyperv pause <id|name>\n";
            output = out.str();
            return 1;
        }

        auto* vm = findVm(args[1]);
        if (!vm) {
            out << "Error: Virtual machine '" << args[1] << "' not found.\n";
            output = out.str();
            return 1;
        }

        if (vm->pause()) {
            out << "Virtual machine '" << vm->getName() << "' (" << vm->getId() << ") paused successfully.\n"
                << "  State: Paused\n";
            output = out.str();
            return 0;
        } else {
            out << "Error: Could not pause VM '" << vm->getName() << "' (Current state: "
                << micant::vmms::VmStateToString(vm->getState()) << ").\n";
            output = out.str();
            return 1;
        }
    }

    // ------------------------------------------------------------------------
    // hyperv resume <id|name>
    // ------------------------------------------------------------------------
    if (verb == "resume") {
        if (args.size() < 2) {
            out << "Error: VM ID or name is required. Usage: hyperv resume <id|name>\n";
            output = out.str();
            return 1;
        }

        auto* vm = findVm(args[1]);
        if (!vm) {
            out << "Error: Virtual machine '" << args[1] << "' not found.\n";
            output = out.str();
            return 1;
        }

        if (vm->resume()) {
            out << "Virtual machine '" << vm->getName() << "' (" << vm->getId() << ") resumed successfully.\n"
                << "  State: Running\n";
            output = out.str();
            return 0;
        } else {
            out << "Error: Could not resume VM '" << vm->getName() << "' (Current state: "
                << micant::vmms::VmStateToString(vm->getState()) << ").\n";
            output = out.str();
            return 1;
        }
    }

    // ------------------------------------------------------------------------
    // hyperv save <id|name>
    // ------------------------------------------------------------------------
    if (verb == "save") {
        if (args.size() < 2) {
            out << "Error: VM ID or name is required. Usage: hyperv save <id|name>\n";
            output = out.str();
            return 1;
        }

        auto* vm = findVm(args[1]);
        if (!vm) {
            out << "Error: Virtual machine '" << args[1] << "' not found.\n";
            output = out.str();
            return 1;
        }

        if (vm->save()) {
            out << "Virtual machine '" << vm->getName() << "' (" << vm->getId() << ") state saved to disk.\n"
                << "  State: Saved\n";
            output = out.str();
            return 0;
        } else {
            out << "Error: Could not save VM '" << vm->getName() << "' (Current state: "
                << micant::vmms::VmStateToString(vm->getState()) << ").\n";
            output = out.str();
            return 1;
        }
    }

    // ------------------------------------------------------------------------
    // hyperv balloon <id|name> <ram_mb>
    // ------------------------------------------------------------------------
    if (verb == "balloon") {
        if (args.size() < 3) {
            out << "Error: VM ID and target RAM (MB) required. Usage: hyperv balloon <id|name> <ram_mb>\n";
            output = out.str();
            return 1;
        }

        auto* vm = findVm(args[1]);
        if (!vm) {
            out << "Error: Virtual machine '" << args[1] << "' not found.\n";
            output = out.str();
            return 1;
        }

        uint32_t targetMb = static_cast<uint32_t>(std::strtoul(std::string(args[2]).c_str(), nullptr, 10));
        if (vm->adjustBalloonMemory(targetMb)) {
            out << "Dynamic memory ballooning adjusted for '" << vm->getName() << "':\n"
                << "  Allocated Memory: " << vm->getMemory().currentAllocatedMb << " MB\n"
                << "  Current Demand:   " << vm->getMemory().currentDemandMb << " MB\n"
                << "  Min / Max Limits: " << vm->getMemory().minRamMb << " MB / " << vm->getMemory().maxRamMb << " MB\n";
            output = out.str();
            return 0;
        } else {
            out << "Error: Memory allocation of " << targetMb << " MB exceeds limits ("
                << vm->getMemory().minRamMb << " MB - " << vm->getMemory().maxRamMb << " MB).\n";
            output = out.str();
            return 1;
        }
    }

    // ------------------------------------------------------------------------
    // hyperv switch | vswitch
    // ------------------------------------------------------------------------
    if (verb == "switch" || verb == "vswitch") {
        auto switches = vmms.getAllSwitches();
        out << "Hyper-V Extensible Virtual Switch Topology (" << switches.size() << " Switch(es)):\n"
            << "================================================================================\n";
        for (const auto& sw : switches) {
            out << "Switch Name:      " << sw.getName() << "\n"
                << "Switch Type:      " << (sw.getType() == micant::vmms::VSwitchPortType::Internal ? "Internal" : "External") << "\n"
                << "Ports Connected:  " << sw.getPortCount() << "\n"
                << "Frames Switched:  " << sw.getTotalFramesSwitched() << "\n"
                << "Frames Dropped:   " << sw.getTotalFramesDropped() << " (VLAN Isolation / ACL)\n"
                << "--------------------------------------------------------------------------------\n"
                << std::left << std::setw(16) << "Port ID"
                << std::setw(20) << "Port Name"
                << std::setw(20) << "MAC Address"
                << std::setw(10) << "802.1Q"
                << "Pkts (Rx/Tx)\n"
                << "--------------------------------------------------------------------------------\n";
            for (const auto& p : sw.getAllPorts()) {
                std::string vlanStr = (p.vlanId == 0) ? "Untagged" : ("VLAN " + std::to_string(p.vlanId));
                std::string pkts = std::to_string(p.packetsReceived) + " / " + std::to_string(p.packetsSent);
                out << std::left << std::setw(16) << p.portId
                    << std::setw(20) << p.portName
                    << std::setw(20) << p.macAddress
                    << std::setw(10) << vlanStr
                    << pkts << "\n";
            }
        }
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // hyperv vhdx <path>
    // ------------------------------------------------------------------------
    if (verb == "vhdx") {
        if (args.size() < 2) {
            out << "Error: VHXD file path required. Usage: hyperv vhdx <path>\n";
            output = out.str();
            return 1;
        }

        std::string path(args[1]);
        auto* vhdx = vmms.getVhdx(path);
        if (!vhdx) {
            out << "Error: VHDX container '" << path << "' not registered in VMMS.\n";
            output = out.str();
            return 1;
        }

        std::string typeStr;
        switch (vhdx->getDiskType()) {
            case micant::vmms::VhdxDiskType::Fixed:         typeStr = "Fixed"; break;
            case micant::vmms::VhdxDiskType::Dynamic:       typeStr = "Dynamic"; break;
            case micant::vmms::VhdxDiskType::Differencing:  typeStr = "Differencing (Snapshot Child)"; break;
        }

        out << "Hyper-V Virtual Hard Disk (VHDX v1.00) Metadata:\n"
            << "--------------------------------------------------------------------------------\n"
            << "  Container Path:      " << vhdx->getPath() << "\n"
            << "  Disk Format:         VHDX ('vhdxfile' 0x656C696678646876)\n"
            << "  Disk Type:           " << typeStr << "\n"
            << "  Virtual Capacity:    " << FormatBytes(vhdx->getVirtualSizeBytes()) << "\n"
            << "  Payload Block Size:  " << FormatBytes(vhdx->getBlockSize()) << " (1 MB Native BAT)\n"
            << "  Parent VHDX Locator: " << (vhdx->getParentPath().empty() ? "None (Base Image)" : vhdx->getParentPath()) << "\n"
            << "  Allocated Blocks:    " << vhdx->getAllocatedBlocks() << " slabs\n";
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // hyperv migrate <id|name> [destHost]
    // ------------------------------------------------------------------------
    if (verb == "migrate") {
        if (args.size() < 2) {
            out << "Error: VM ID or name required. Usage: hyperv migrate <id|name> [destHost]\n";
            output = out.str();
            return 1;
        }

        std::string destHost = (args.size() >= 3) ? std::string(args[2]) : "HYPERV-NODE-02.corp.contoso.com";
        auto* vm = findVm(args[1]);
        if (!vm) {
            out << "Error: Virtual machine '" << args[1] << "' not found.\n";
            output = out.str();
            return 1;
        }

        if (vm->getState() != micant::vmms::VmState::Running) {
            out << "Error: Live Migration requires VM to be in 'Running' state (Current: "
                << micant::vmms::VmStateToString(vm->getState()) << ").\n";
            output = out.str();
            return 1;
        }

        if (vm->simulateLiveMigration(3)) {
            out << "Hyper-V Live Migration Completed Successfully:\n"
                << "--------------------------------------------------------------------------------\n"
                << "  Virtual Machine:     " << vm->getName() << " (" << vm->getId() << ")\n"
                << "  Destination Node:    " << destHost << "\n"
                << "  Pre-Copy Iterations: 3 iterative passes\n"
                << "  Memory Pages Synced: " << vm->getMigrationPagesCopied() << " pages (4 KB)\n"
                << "  Brownout Downtime:   " << vm->getMigrationBrownoutMs() << " ms (Zero Dropped Packets)\n"
                << "  Final Status:        Running on " << destHost << "\n";
            output = out.str();
            return 0;
        } else {
            out << "Error: Live Migration failed during dirty page pre-copy phase.\n";
            output = out.str();
            return 1;
        }
    }

    // ------------------------------------------------------------------------
    // hyperv dump
    // ------------------------------------------------------------------------
    if (verb == "dump") {
        out << "# Hyper-V Virtual Machine Management Configuration Script\n"
            << "# Generated by MicaNT hyperv.exe / vmms.exe @ 10.0.26100.1\n\n";
        for (const auto& sw : vmms.getAllSwitches()) {
            out << "New-VMSwitch -Name \"" << sw.getName() << "\" -SwitchType "
                << (sw.getType() == micant::vmms::VSwitchPortType::Internal ? "Internal" : "External") << "\n";
        }
        for (const auto& vm : vmms.getAllVirtualMachines()) {
            out << "New-VM -Name \"" << vm.getName() << "\" -MemoryStartupBytes "
                << (static_cast<uint64_t>(vm.getMemory().startupRamMb) * 1024 * 1024)
                << " -Generation 2\n";
            out << "Set-VMProcessor -VMName \"" << vm.getName() << "\" -Count "
                << vm.getTopology().vCpuCount << "\n";
            for (const auto& d : vm.getAttachedVhdx()) {
                out << "Add-VMHardDiskDrive -VMName \"" << vm.getName() << "\" -Path \"" << d << "\"\n";
            }
            if (vm.getState() == micant::vmms::VmState::Running) {
                out << "Start-VM -Name \"" << vm.getName() << "\"\n";
            }
        }
        output = out.str();
        return 0;
    }

    out << "Unknown hyperv command: '" << verb << "'. Type 'hyperv' for help.\n";
    output = out.str();
    return 1;
}

inline int ExecuteHyperVString(std::string_view cmdLine, std::string& output) {
    auto tokens = Tokenize(cmdLine);
    if (tokens.empty()) {
        std::span<std::string_view> emptyArgs;
        return ExecuteHyperV(emptyArgs, output);
    }

    // Strip leading "hyperv" or "vmms" if present
    if (tokens[0] == "hyperv" || tokens[0] == "hyperv.exe" ||
        tokens[0] == "vmms"   || tokens[0] == "vmms.exe") {
        tokens.erase(tokens.begin());
    }

    std::span<std::string_view> args(tokens.data(), tokens.size());
    return ExecuteHyperV(args, output);
}

inline int Main(int argc, char* argv[]) {
    std::vector<std::string_view> args;
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    std::string outStr;
    int rc = ExecuteHyperV(args, outStr);
    std::cout << outStr;
    return rc;
}

} // namespace micant::hyperv_cmd::um
