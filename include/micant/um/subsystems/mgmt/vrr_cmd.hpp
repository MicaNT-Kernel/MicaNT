#pragma once

/**
 * @file vrr_cmd.hpp
 * @brief Canonical Win32 vrrcli.exe / displaysync.exe CLI Management Utilities for Variable Refresh Rate & Adaptive Sync.
 *
 * Clean-room implementation of Adaptive Display Sync & Variable Refresh Rate CLI tools
 * under 17 U.S.C. § 1201(f) & Google LLC v. Oracle America, Inc. (141 S. Ct. 1183, 2021).
 *
 * Provides complete CLI management for:
 *   - vrr status: Introspect display sync vitals, active refresh rate, VRR limits, and LFC events
 *   - vrr list: Tabular inventory of connected display endpoints, resolutions, and HDR capabilities
 *   - vrr set-rate <id> <hz>: Set active display refresh frequency
 *   - vrr set-vrr <id> <enable|disable>: Toggle VESA Adaptive-Sync / VRR presentation
 *   - vrr pace <id> <render_us>: Simulate frame pacing calculation and VBlank modulation
 *   - vrr autohdr <id> <enable|disable> [paper_white] [peak_nits]: Configure Auto HDR tone mapping
 *   - vrr dump: Export declarative PowerShell / Registry display synchronization configuration
 *   - vrr help: Command usage and syntax
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

#include "micant/km/drivers/video/vrr.hpp"

namespace micant::vrr_cmd::um {

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

class VrrCommandInterpreter {
public:
    static VrrCommandInterpreter& get() {
        static VrrCommandInterpreter instance;
        return instance;
    }

    void execute(std::span<const std::string_view> args, std::string& outResult) {
        std::ostringstream ss;

        if (args.empty() || args[0] == "help" || args[0] == "-h" || args[0] == "--help" || args[0] == "/?") {
            printHelp(ss);
            outResult = ss.str();
            return;
        }

        auto& sys = micant::vrr::VrrSubsystem::get();
        std::string_view subCmd = args[0];

        if (subCmd == "status") {
            auto v = sys.getVitals();
            ss << "===============================================================================\n"
               << "      MicaNT Sovereign Adaptive Display Sync & Variable Refresh Rate Subsystem \n"
               << "===============================================================================\n"
               << "  Active Display Endpoints             : " << v.activeDisplaysCount << " endpoints\n"
               << "  Current Refresh Frequency            : " << std::fixed << std::setprecision(1) << v.currentRefreshHz << " Hz\n"
               << "  Variable Refresh Rate Range          : " << v.minRefreshHz << " Hz - " << v.maxRefreshHz << " Hz\n"
               << "  Adaptive-Sync / VRR State            : " << (v.vrrEnabled ? "ACTIVE (Tear-Free)" : "DISABLED") << "\n"
               << "  Low Framerate Compensation (LFC)     : " << (v.lfcEnabled ? "ENABLED (Frame Doubling)" : "DISABLED") << "\n"
               << "  Total Paced Frames Synchronized      : " << v.totalPacedFrames << " frames\n"
               << "  LFC Frame Doubling Events            : " << v.totalLfcEvents << " events\n"
               << "  SCM Driver Service Status            : DisplaySyncDriver (displaysync.sys, BOOT_START)\n"
               << "  Display Broker Service Status        : DisplaySyncBroker (syncbroker.exe, PID 1780)\n"
               << "===============================================================================\n";
        }
        else if (subCmd == "list") {
            auto displays = sys.getAllDisplays();
            ss << "----------------------------------------------------------------------------------------------------\n"
               << " ID  Display Endpoint Name                    Resolution   VRR Range     Current  Gamut     Auto HDR\n"
               << "----------------------------------------------------------------------------------------------------\n";
            for (const auto& d : displays) {
                std::string name(d->getName().begin(), d->getName().end());
                std::string res = std::to_string(d->getWidth()) + "x" + std::to_string(d->getHeight());
                std::string vrrRange = std::to_string(static_cast<int>(d->getVrrCaps().minRefreshHz)) + "-" +
                                      std::to_string(static_cast<int>(d->getVrrCaps().maxRefreshHz)) + "Hz";
                ss << " " << std::left << std::setw(3) << d->getId() << " "
                   << std::setw(40) << name << " "
                   << std::setw(12) << res << " "
                   << std::setw(13) << vrrRange << " "
                   << std::right << std::setw(5) << static_cast<int>(d->getVrrCaps().currentRefreshHz) << "Hz  "
                   << std::left << std::setw(9) << micant::vrr::DisplayColorGamutToString(d->getColorGamut()) << " "
                   << (d->getHdrCaps().autoHdrEnabled ? "ENABLED" : "OFF") << "\n";
            }
            ss << "----------------------------------------------------------------------------------------------------\n"
               << " Total Display Endpoints: " << displays.size() << "\n";
        }
        else if (subCmd == "set-rate") {
            if (args.size() < 3) {
                ss << "[-] Usage: vrr set-rate <display_id> <hz>\n";
            } else {
                uint32_t id = 1;
                float hz = 120.0f;
                try {
                    id = static_cast<uint32_t>(std::stoul(std::string(args[1])));
                    hz = std::stof(std::string(args[2]));
                } catch (...) {
                    ss << "[-] Invalid parameters.\n";
                    outResult = ss.str();
                    return;
                }
                auto d = sys.getDisplay(id);
                if (!d) {
                    ss << "[-] Display endpoint " << id << " not found.\n";
                } else {
                    d->setRefreshRate(hz);
                    ss << "[+] Display " << id << " refresh rate set to " << std::fixed << std::setprecision(1) << d->getVrrCaps().currentRefreshHz << " Hz.\n";
                }
            }
        }
        else if (subCmd == "set-vrr") {
            if (args.size() < 3) {
                ss << "[-] Usage: vrr set-vrr <display_id> <enable|disable>\n";
            } else {
                uint32_t id = 1;
                try {
                    id = static_cast<uint32_t>(std::stoul(std::string(args[1])));
                } catch (...) {}
                auto d = sys.getDisplay(id);
                if (!d) {
                    ss << "[-] Display endpoint " << id << " not found.\n";
                } else {
                    std::string state = std::string(args[2]);
                    std::transform(state.begin(), state.end(), state.begin(), ::tolower);
                    auto caps = d->getVrrCaps();
                    caps.vrrSupported = (state == "enable" || state == "1" || state == "true");
                    d->setVrrCaps(caps);
                    ss << "[+] Display " << id << " Variable Refresh Rate set to " << (caps.vrrSupported ? "ENABLED" : "DISABLED") << ".\n";
                }
            }
        }
        else if (subCmd == "pace") {
            uint32_t id = 1;
            uint32_t renderTimeUs = 6944; // ~144 Hz frame (6.94 ms)
            if (args.size() >= 2) {
                try { id = static_cast<uint32_t>(std::stoul(std::string(args[1]))); } catch (...) {}
            }
            if (args.size() >= 3) {
                try { renderTimeUs = static_cast<uint32_t>(std::stoul(std::string(args[2]))); } catch (...) {}
            }

            micant::vrr::DisplayPacingInfo pInfo{};
            bool ok = sys.paceFrame(id, renderTimeUs, pInfo);
            if (!ok) {
                ss << "[-] Failed to calculate frame pacing for display " << id << ".\n";
            } else {
                ss << "[+] Frame Pacing Calculation (Display " << id << "):\n"
                   << "    GPU Render Duration : " << pInfo.renderTimeUs << " us (" << std::fixed << std::setprecision(2) << (pInfo.renderTimeUs / 1000.0f) << " ms)\n"
                   << "    Paced VBlank Period : " << pInfo.calculatedVBlankUs << " us\n"
                   << "    Effective Refresh   : " << std::setprecision(1) << pInfo.effectiveRefreshHz << " Hz\n"
                   << "    LFC Frame Doubling  : " << (pInfo.frameDoubled ? "ACTIVE (Below Floor)" : "NO (Within VRR Window)") << "\n";
            }
        }
        else if (subCmd == "autohdr") {
            if (args.size() < 3) {
                ss << "[-] Usage: vrr autohdr <display_id> <enable|disable> [paper_white] [peak_nits]\n";
            } else {
                uint32_t id = 1;
                try { id = static_cast<uint32_t>(std::stoul(std::string(args[1]))); } catch (...) {}
                auto d = sys.getDisplay(id);
                if (!d) {
                    ss << "[-] Display endpoint " << id << " not found.\n";
                } else {
                    std::string state = std::string(args[2]);
                    std::transform(state.begin(), state.end(), state.begin(), ::tolower);
                    auto hdr = d->getHdrCaps();
                    hdr.autoHdrEnabled = (state == "enable" || state == "1" || state == "true");
                    if (args.size() >= 4) {
                        try { hdr.paperWhiteNits = std::stof(std::string(args[3])); } catch (...) {}
                    }
                    if (args.size() >= 5) {
                        try { hdr.maxPeakLuminanceNits = std::stof(std::string(args[4])); } catch (...) {}
                    }
                    d->setHdrCaps(hdr);
                    ss << "[+] Auto HDR for display " << id << " set to " << (hdr.autoHdrEnabled ? "ENABLED" : "DISABLED") << " ("
                       << hdr.paperWhiteNits << " nits reference white, " << hdr.maxPeakLuminanceNits << " nits peak).\n";
                }
            }
        }
        else if (subCmd == "dump") {
            ss << "# -----------------------------------------------------------------------------\n"
               << "# MicaNT Sovereign Adaptive Display Sync & VRR Configuration\n"
               << "# Generated automatically by vrr dump\n"
               << "# -----------------------------------------------------------------------------\n\n"
               << "# Enable Variable Refresh Rate Globally in Registry\n"
               << "Set-ItemProperty -Path \"HKCU:\\Control Panel\\Desktop\" -Name \"EnableVRR\" -Value 1\n\n"
               << "# Enable Display Sync Broker Service\n"
               << "Set-Service -Name \"DisplaySyncBroker\" -StartupType Automatic\n"
               << "Start-Service -Name \"DisplaySyncBroker\"\n\n"
               << "# Enable Auto HDR Feature Globally\n"
               << "Set-ItemProperty -Path \"HKLM:\\SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers\" -Name \"AutoHDREnabled\" -Value 1\n\n"
               << "# End of Display Sync Configuration Export\n";
        }
        else {
            ss << "[-] Unknown vrr command: " << subCmd << "\n"
               << "    Type 'vrr help' for a list of available commands.\n";
        }

        outResult = ss.str();
    }

private:
    VrrCommandInterpreter() = default;

    void printHelp(std::ostream& os) const {
        os << "MicaNT Display Synchronization Tool (vrrcli / displaysync)\n"
           << "Copyright (C) 2026 MicaNT Authors.\n"
           << "Provides sovereign administration of Variable Refresh Rate (VRR), Adaptive-Sync, and Auto HDR.\n\n"
           << "Usage:\n"
           << "  vrr status                                Display display sync vitals, refresh rate, and LFC counts\n"
           << "  vrr list                                  List connected display endpoints and capabilities\n"
           << "  vrr set-rate <id> <hz>                    Set display target refresh frequency\n"
           << "  vrr set-vrr <id> <enable|disable>         Toggle Variable Refresh Rate / Adaptive-Sync\n"
           << "  vrr pace <id> <render_us>                 Calculate frame pacing and VBlank modulation\n"
           << "  vrr autohdr <id> <en|dis> [white] [peak]  Configure Auto HDR highlight expansion\n"
           << "  vrr dump                                  Export PowerShell / Registry display configuration\n"
           << "  vrr help                                  Display this help message\n";
    }
};

inline void ExecuteVrrString(std::string_view commandLine, std::string& outResult) {
    auto tokens = Tokenize(commandLine);
    if (tokens.empty()) {
        outResult = "[-] No command specified.\n";
        return;
    }

    std::span<const std::string_view> args(tokens.data() + 1, tokens.size() - 1);
    VrrCommandInterpreter::get().execute(args, outResult);
}

} // namespace micant::vrr_cmd::um
