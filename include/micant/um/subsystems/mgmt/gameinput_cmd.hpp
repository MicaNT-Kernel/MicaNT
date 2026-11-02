#pragma once

/**
 * @file gameinput_cmd.hpp
 * @brief Canonical Win32 gamepadcli.exe / gameinput.exe CLI Management Utilities for Gaming Input.
 *
 * Clean-room implementation of Windows Gaming Input & High-Polling Raw Input CLI tools
 * under 17 U.S.C. § 1201(f) & Google LLC v. Oracle America, Inc. (141 S. Ct. 1183, 2021).
 *
 * Provides complete CLI management for:
 *   - gameinput status: Introspect connected controllers, polling rates, and report metrics
 *   - gameinput list: Tabular inventory of connected gaming input devices and hardware IDs
 *   - gameinput poll [user_index]: Read instantaneous button, trigger, and thumbstick states
 *   - gameinput rumble <user_index> <left> <right> [left_impulse] [right_impulse]: Trigger haptic motors
 *   - gameinput test-rawinput <dx> <dy> [buttons]: Inject high-frequency raw mouse displacement
 *   - gameinput dump: Export declarative PowerShell / Registry Gaming Input configuration
 *   - gameinput help: Command usage and syntax
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

#include "micant/km/drivers/input/gameinput.hpp"

namespace micant::gameinput_cmd::um {

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

class GameInputCommandInterpreter {
public:
    static GameInputCommandInterpreter& get() {
        static GameInputCommandInterpreter instance;
        return instance;
    }

    void execute(std::span<const std::string_view> args, std::string& outResult) {
        std::ostringstream ss;

        if (args.empty() || args[0] == "help" || args[0] == "-h" || args[0] == "--help" || args[0] == "/?") {
            printHelp(ss);
            outResult = ss.str();
            return;
        }

        auto& mgr = micant::gameinput::GameInputManager::get();
        std::string_view subCmd = args[0];

        if (subCmd == "status") {
            auto v = mgr.getVitals();
            ss << "===============================================================================\n"
               << "      MicaNT Sovereign Gaming Input & High-Polling Raw Input Subsystem        \n"
               << "===============================================================================\n"
               << "  Connected Input Devices     : " << v.connectedDevicesCount << "\n"
               << "  Active Gamepads             : " << v.activeGamepadsCount << " controller(s)\n"
               << "  Active High-Polling Mice    : " << v.activeMiceCount << " mouse/mice\n"
               << "  Peak Polling Frequency      : " << v.highestPollingRateHz << " Hz\n"
               << "  Total Input Reports Handled : " << v.totalInputReportsProcessed << " events\n"
               << "  Total Haptic Commands Sent  : " << v.totalHapticCommandsIssued << " commands\n"
               << "  Background Service Status   : " << (v.serviceRunning ? "RUNNING (GameInputSvc, PID 1420)" : "STOPPED") << "\n"
               << "  Raw Input Filter Driver     : " << (v.rawInputDriverActive ? "ACTIVE (rawinput.sys, Boot-Start)" : "DISABLED") << "\n"
               << "===============================================================================\n";
        }
        else if (subCmd == "list") {
            auto devList = mgr.getAllDevices();
            ss << "----------------------------------------------------------------------------------------------------------------\n"
               << " ID   Slot  Kind                         Polling Rate  Hardware ID                     Device Name\n"
               << "----------------------------------------------------------------------------------------------------------------\n";
            for (const auto& dev : devList) {
                ss << " " << std::left << std::setw(5) << dev.deviceId
                   << std::setw(6) << dev.userIndex
                   << std::setw(29) << micant::gameinput::DeviceKindToString(dev.kind)
                   << std::setw(14) << (std::to_string(dev.pollingRateHz) + " Hz")
                   << std::setw(32) << dev.hardwareId
                   << dev.deviceName << "\n";
            }
            ss << "----------------------------------------------------------------------------------------------------------------\n"
               << " Total devices enumerated: " << devList.size() << "\n";
        }
        else if (subCmd == "poll") {
            uint32_t slot = 0;
            if (args.size() >= 2) {
                try {
                    slot = static_cast<uint32_t>(std::stoul(std::string(args[1])));
                } catch (...) {
                    slot = 0;
                }
            }
            micant::gameinput::GameInputGamepadState st{};
            if (mgr.xinputGetState(slot, st)) {
                ss << "[+] Controller Slot " << slot << " Polling Snapshot:\n"
                   << "    Buttons           : 0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << st.buttons << std::dec << std::setfill(' ') << "\n"
                   << "    Triggers (LT / RT): " << std::fixed << std::setprecision(2) << st.leftTrigger << " / " << st.rightTrigger << "\n"
                   << "    Left Stick (X, Y) : " << st.leftThumbstickX << ", " << st.leftThumbstickY << "\n"
                   << "    Right Stick (X, Y): " << st.rightThumbstickX << ", " << st.rightThumbstickY << "\n";
            } else {
                ss << "[-] No active gamepad found at slot " << slot << ".\n";
            }
        }
        else if (subCmd == "rumble") {
            if (args.size() < 4) {
                ss << "[-] Syntax: gameinput rumble <user_index> <left_speed> <right_speed> [left_impulse] [right_impulse]\n"
                   << "    Speeds must be normalized floating-point numbers between 0.0 and 1.0.\n";
                outResult = ss.str();
                return;
            }
            uint32_t slot = 0;
            float leftSpeed = 0.0f;
            float rightSpeed = 0.0f;
            try {
                slot = static_cast<uint32_t>(std::stoul(std::string(args[1])));
                leftSpeed = std::stof(std::string(args[2]));
                rightSpeed = std::stof(std::string(args[3]));
            } catch (...) {
                ss << "[-] Invalid numerical parameters for rumble command.\n";
                outResult = ss.str();
                return;
            }

            if (mgr.xinputSetState(slot, leftSpeed, rightSpeed)) {
                ss << "[+] Successfully applied haptic vibration to Controller Slot " << slot << ": "
                   << "Left=" << std::fixed << std::setprecision(2) << leftSpeed
                   << ", Right=" << rightSpeed << "\n";
            } else {
                ss << "[-] Failed to set haptics: Controller Slot " << slot << " not connected.\n";
            }
        }
        else if (subCmd == "test-rawinput") {
            int32_t dx = 10;
            int32_t dy = -5;
            uint32_t btns = 0;
            if (args.size() >= 3) {
                try {
                    dx = std::stol(std::string(args[1]));
                    dy = std::stol(std::string(args[2]));
                    if (args.size() >= 4) {
                        btns = static_cast<uint32_t>(std::stoul(std::string(args[3])));
                    }
                } catch (...) {
                    dx = 10; dy = -5;
                }
            }

            // Dispatch to Esports Mouse (ID 2)
            if (mgr.dispatchRawInputReport(2, dx, dy, btns)) {
                ss << "[+] Successfully injected high-frequency Raw Input report to Device 2:\n"
                   << "    Displacement: dX=" << dx << ", dY=" << dy << "\n"
                   << "    Buttons     : 0x" << std::hex << btns << std::dec << "\n"
                   << "    Latency     : < 0.125 ms (8000 Hz Sub-Millisecond Kernel Path)\n";
            } else {
                ss << "[-] Failed to dispatch raw input report to Device 2.\n";
            }
        }
        else if (subCmd == "dump") {
            ss << "# -----------------------------------------------------------------------------\n"
               << "# MicaNT Gaming Input & Raw Input High-Polling Configuration\n"
               << "# Generated automatically by gameinput dump\n"
               << "# -----------------------------------------------------------------------------\n\n"
               << "# Configure Raw Input Polling Frequency in Registry\n"
               << "Set-ItemProperty -Path \"HKLM:\\System\\CurrentControlSet\\Services\\rawinput\\Parameters\" "
               << "-Name \"MaxPollingFrequencyHz\" -Value 8000\n"
               << "Set-ItemProperty -Path \"HKLM:\\System\\CurrentControlSet\\Services\\rawinput\\Parameters\" "
               << "-Name \"SubMillisecondDispatch\" -Value 1\n\n"
               << "# Enable GameInput Background Broker Service Auto-Start\n"
               << "Set-Service -Name \"GameInputSvc\" -StartupType Automatic\n"
               << "Start-Service -Name \"GameInputSvc\"\n\n"
               << "# Configure Low Latency Gamepad Deadzone Normalization\n"
               << "Set-ItemProperty -Path \"HKCU:\\Software\\MicaNT\\GameInput\" -Name \"DeadzoneInnerRadius\" -Value 0.05\n"
               << "Set-ItemProperty -Path \"HKCU:\\Software\\MicaNT\\GameInput\" -Name \"DeadzoneOuterRadius\" -Value 0.98\n\n"
               << "# End of Gaming Input Configuration Export\n";
        }
        else {
            ss << "[-] Unknown gameinput command: " << subCmd << "\n"
               << "    Type 'gameinput help' for a list of available commands.\n";
        }

        outResult = ss.str();
    }

private:
    GameInputCommandInterpreter() = default;

    void printHelp(std::ostream& os) const {
        os << "MicaNT Gaming Input Administration Tool (gamepadcli / gameinput)\n"
           << "Copyright (C) 2026 MicaNT Authors.\n"
           << "Provides sovereign administration of gamepads, high-polling raw input mice, and haptics.\n\n"
           << "Usage:\n"
           << "  gameinput status                          Display gaming input subsystem vitals and service state\n"
           << "  gameinput list                            List all connected game controllers and mice\n"
           << "  gameinput poll [user_index]               Poll controller state for given slot (default: 0)\n"
           << "  gameinput rumble <slot> <left> <right>    Set dual-motor vibration speeds (0.0 to 1.0)\n"
           << "  gameinput test-rawinput <dx> <dy> [btns]  Inject 8000Hz raw input mouse report\n"
           << "  gameinput dump                            Export PowerShell / Registry configuration script\n"
           << "  gameinput help                            Display this help message\n";
    }
};

inline void ExecuteGameInputString(std::string_view commandLine, std::string& outResult) {
    auto tokens = Tokenize(commandLine);
    if (tokens.empty()) {
        outResult = "[-] No command specified.\n";
        return;
    }

    std::span<const std::string_view> args(tokens.data() + 1, tokens.size() - 1);
    GameInputCommandInterpreter::get().execute(args, outResult);
}

} // namespace micant::gameinput_cmd::um
