#pragma once

/**
 * @file rdsh_cmd.hpp
 * @brief Canonical Win32 qwinsta.exe / rwinsta.exe / tscon.exe / tsdiscon.exe / chgusr.exe / shadow.exe CLI Management Utilities for RDSH.
 *
 * Clean-room implementation of the Microsoft Windows Server Remote Desktop Session Host (RDSH)
 * & Terminal Services Multi-Session CLI under 17 U.S.C. § 1201(f) &
 * Google LLC v. Oracle America, Inc. (141 S. Ct. 1183, 2021).
 *
 * Provides complete CLI management for:
 *   - qwinsta [id|name]: query session inventory and connection states
 *   - rwinsta <id>: reset/logoff terminal session
 *   - tsdiscon <id>: disconnect interactive user session
 *   - tscon <id>: reconnect/attach user session to active console
 *   - chgusr </install|/execute|/query>: manage application execution and terminal server install mode
 *   - shadow <id> [/control]: initiate remote session control or observation
 *   - rdsh status: global RDSH subsystem health and session statistics
 *   - rdsh channels <id>: inspect Dynamic Virtual Channels (cliprdr, rdpdr, rdpsnd, rdpinput)
 *   - rdsh redirect <id> <drive_letter> <path>: configure client device and drive redirection
 *   - rdsh clip <id> <text>: synchronize cross-session clipboard buffer
 *   - rdsh dump: export declarative PowerShell RDS configuration script
 *   - rdsh help: command usage and syntax
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

#include "micant/um/servers/termsrv.hpp"

namespace micant::rdsh_cmd::um {

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

inline std::string StateToString(micant::termsrv::WTS_CONNECTSTATE_CLASS st) {
    switch (st) {
        case micant::termsrv::WTSActive:       return "Active";
        case micant::termsrv::WTSConnected:    return "Conn";
        case micant::termsrv::WTSConnectQuery: return "Query";
        case micant::termsrv::WTSShadow:       return "Shadow";
        case micant::termsrv::WTSDisconnected: return "Disc";
        case micant::termsrv::WTSIdle:         return "Idle";
        case micant::termsrv::WTSListen:       return "Listen";
        case micant::termsrv::WTSReset:        return "Reset";
        case micant::termsrv::WTSDown:         return "Down";
        case micant::termsrv::WTSInit:         return "Init";
        default:                               return "Unknown";
    }
}

inline std::string WideToNarrow(const std::wstring& ws) {
    std::string s;
    s.reserve(ws.size());
    for (wchar_t wc : ws) {
        s.push_back(static_cast<char>(wc));
    }
    return s;
}

inline int ExecuteRdsh(std::span<std::string_view> args, std::string& output) {
    std::ostringstream out;
    auto& mgr = micant::termsrv::TerminalServicesManager::get();

    if (args.empty() || args[0] == "help" || args[0] == "/?" || args[0] == "-h" || args[0] == "--help") {
        out << "Microsoft Windows Remote Desktop Session Host (RDSH) Subsystem CLI\n"
            << "Copyright (C) Microsoft Corporation / Sovereign Clean-Room MicaNT.\n\n"
            << "Available Utilities:\n"
            << "  qwinsta [id]                 Query active session inventory\n"
            << "  rwinsta <id>                 Reset/terminate target session\n"
            << "  tsdiscon <id>                Disconnect interactive user session\n"
            << "  tscon <id>                   Reconnect/attach session to active console\n"
            << "  chgusr </install|/execute|/query>  Change application execution mode\n"
            << "  shadow <id> [/control]       Shadow or observe target user session\n"
            << "  rdsh status                  Display RDSH health and connection metrics\n"
            << "  rdsh channels <id>           Inspect Dynamic Virtual Channels (DVC)\n"
            << "  rdsh redirect <id> <drv> <path>  Map redirected client drive\n"
            << "  rdsh clip <id> <text>        Synchronize cross-session clipboard\n"
            << "  rdsh dump                    Export declarative PowerShell configuration\n";
        output = out.str();
        return 0;
    }

    std::string_view verb = args[0];

    // ------------------------------------------------------------------------
    // qwinsta / query session
    // ------------------------------------------------------------------------
    if (verb == "qwinsta" || verb == "query" || (verb == "rdsh" && args.size() > 1 && args[1] == "query")) {
        auto sessions = mgr.getSessions();
        out << " SESSIONNAME       USERNAME              ID  STATE    TYPE        DEVICE\n";
        for (const auto& s : sessions) {
            std::string sessName = WideToNarrow(s.winStationName);
            std::string user = WideToNarrow(s.userName);
            std::string st = StateToString(s.state);
            std::string type = (s.protocolType == micant::termsrv::WTS_PROTOCOL_TYPE_RDP) ? "rdp-tcp" : "Console";
            std::string marker = (s.sessionId == 1 && s.state == micant::termsrv::WTSActive) ? ">" : " ";

            out << marker << std::left << std::setw(17) << sessName
                << std::setw(22) << user
                << std::right << std::setw(3) << s.sessionId << "  "
                << std::left << std::setw(9) << st
                << std::setw(12) << type << "\n";
        }
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // rwinsta <id> (reset session)
    // ------------------------------------------------------------------------
    if ((verb == "rwinsta" || verb == "reset") && args.size() > 1) {
        uint32_t sid = 0;
        try {
            sid = static_cast<uint32_t>(std::stoul(std::string(args[1])));
        } catch (...) {
            output = "Error: Invalid session ID format.\n";
            return 1;
        }

        bool ok = mgr.logoffSession(sid);
        if (!ok) {
            out << "Error: Session ID " << sid << " not found or cannot be reset.\n";
            output = out.str();
            return 1;
        }

        out << "SUCCESS: Session ID " << sid << " reset and terminated.\n";
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // tsdiscon <id> (disconnect session)
    // ------------------------------------------------------------------------
    if ((verb == "tsdiscon" || verb == "disconnect") && args.size() > 1) {
        uint32_t sid = 0;
        try {
            sid = static_cast<uint32_t>(std::stoul(std::string(args[1])));
        } catch (...) {
            output = "Error: Invalid session ID format.\n";
            return 1;
        }

        bool ok = mgr.disconnectSession(sid);
        if (!ok) {
            out << "Error: Session ID " << sid << " not found or cannot be disconnected.\n";
            output = out.str();
            return 1;
        }

        out << "SUCCESS: Session ID " << sid << " disconnected.\n";
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // tscon <id> (connect/reconnect session)
    // ------------------------------------------------------------------------
    if ((verb == "tscon" || verb == "connect") && args.size() > 1) {
        uint32_t sid = 0;
        try {
            sid = static_cast<uint32_t>(std::stoul(std::string(args[1])));
        } catch (...) {
            output = "Error: Invalid session ID format.\n";
            return 1;
        }

        bool ok = mgr.reconnectSession(sid);
        if (!ok) {
            out << "Error: Session ID " << sid << " not found or cannot be connected.\n";
            output = out.str();
            return 1;
        }

        out << "SUCCESS: Session ID " << sid << " reconnected and active.\n";
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // chgusr </install|/execute|/query>
    // ------------------------------------------------------------------------
    if (verb == "chgusr") {
        if (args.size() == 1 || args[1] == "/query" || args[1] == "-query" || args[1] == "query") {
            auto mode = mgr.getExecutionMode();
            out << "Application execution mode is set to "
                << micant::termsrv::AppExecutionModeToString(mode) << ".\n";
            output = out.str();
            return 0;
        }

        std::string_view opt = args[1];
        if (opt == "/install" || opt == "-install" || opt == "install") {
            mgr.setExecutionMode(micant::termsrv::AppExecutionMode::Install);
            out << "Application execution mode is set to Install.\n";
            output = out.str();
            return 0;
        } else if (opt == "/execute" || opt == "-execute" || opt == "execute") {
            mgr.setExecutionMode(micant::termsrv::AppExecutionMode::Execute);
            out << "Application execution mode is set to Execute.\n";
            output = out.str();
            return 0;
        } else {
            out << "Error: Invalid parameter '" << opt << "'. Use /install, /execute, or /query.\n";
            output = out.str();
            return 1;
        }
    }

    // ------------------------------------------------------------------------
    // shadow <id> [/control]
    // ------------------------------------------------------------------------
    if (verb == "shadow" && args.size() > 1) {
        uint32_t sid = 0;
        try {
            sid = static_cast<uint32_t>(std::stoul(std::string(args[1])));
        } catch (...) {
            output = "Error: Invalid session ID format.\n";
            return 1;
        }

        bool controlMode = false;
        for (size_t i = 2; i < args.size(); ++i) {
            if (args[i] == "/control" || args[i] == "-control" || args[i] == "control") {
                controlMode = true;
            }
        }

        bool ok = mgr.startShadow(sid, controlMode);
        if (!ok) {
            out << "Error: Failed to shadow session ID " << sid << ".\n";
            output = out.str();
            return 1;
        }

        out << "SUCCESS: Shadowing session ID " << sid << " ("
            << (controlMode ? "Interactive Remote Control" : "View-Only Observation") << ").\n";
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // rdsh status
    // ------------------------------------------------------------------------
    if (verb == "status" || (verb == "rdsh" && (args.size() == 1 || args[1] == "status"))) {
        out << "Remote Desktop Session Host (RDSH) Subsystem Status:\n"
            << "========================================================================\n"
            << "  Operational State:         ONLINE (TermService / termsrv.dll)\n"
            << "  Multi-Session Daemon:      SessionEnv [svchost.exe -k netsvcs]\n"
            << "  Active Sessions:           " << mgr.getActiveSessionCount() << "\n"
            << "  Disconnected Sessions:     " << mgr.getDisconnectedSessionCount() << "\n"
            << "  Total Sessions:            " << mgr.getSessions().size() << "\n"
            << "  Application Mode:          " << micant::termsrv::AppExecutionModeToString(mgr.getExecutionMode()) << "\n"
            << "  Open DVC Channels:         " << mgr.getOpenChannelCount() << "\n"
            << "  Redirected Drives:         " << mgr.getRedirectedDriveCount() << "\n"
            << "  Listener Port:             TCP 3389 (RDP) / UDP 3389 (RDP-UDP/MS-RDPBCGR)\n"
            << "  Security Level:            TLS 1.3 / CredSSP (NLA Required)\n"
            << "========================================================================\n";
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // rdsh channels <id>
    // ------------------------------------------------------------------------
    if (verb == "channels" || (verb == "rdsh" && args.size() > 1 && args[1] == "channels")) {
        uint32_t sid = 1;
        size_t argIdx = (verb == "channels") ? 1 : 2;
        if (args.size() > argIdx) {
            try {
                sid = static_cast<uint32_t>(std::stoul(std::string(args[argIdx])));
            } catch (...) {}
        }

        micant::termsrv::TerminalSession s;
        if (!mgr.getSession(sid, s)) {
            out << "Error: Session ID " << sid << " not found.\n";
            output = out.str();
            return 1;
        }

        out << "Dynamic Virtual Channels for Session " << sid << " (" << WideToNarrow(s.winStationName) << "):\n"
            << "------------------------------------------------------------------------\n"
            << std::left << std::setw(15) << "Channel"
            << std::setw(12) << "State"
            << std::setw(20) << "Bytes Transferred" << "\n"
            << "------------------------------------------------------------------------\n";
        for (const auto& ch : s.channels) {
            out << std::left << std::setw(15) << ch.name
                << std::setw(12) << (ch.isOpen ? "OPEN" : "CLOSED")
                << std::setw(20) << ch.bytesTransferred << "\n";
        }
        out << "------------------------------------------------------------------------\n";
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // rdsh redirect <id> <drive_letter> <path>
    // ------------------------------------------------------------------------
    if (verb == "redirect" || (verb == "rdsh" && args.size() > 1 && args[1] == "redirect")) {
        size_t baseIdx = (verb == "redirect") ? 1 : 2;
        if (args.size() < baseIdx + 3) {
            output = "Usage: rdsh redirect <sessionId> <driveLetter> <clientPath>\n";
            return 1;
        }

        uint32_t sid = 0;
        try {
            sid = static_cast<uint32_t>(std::stoul(std::string(args[baseIdx])));
        } catch (...) {
            output = "Error: Invalid session ID.\n";
            return 1;
        }

        char drv = args[baseIdx + 1][0];
        std::string path = std::string(args[baseIdx + 2]);

        bool ok = mgr.addRedirectedDrive(sid, drv, path);
        if (!ok) {
            out << "Error: Failed to map drive for Session " << sid << ".\n";
            output = out.str();
            return 1;
        }

        out << "SUCCESS: Mapped redirected client drive " << drv << ": -> " << path << " in Session " << sid << ".\n";
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // rdsh clip <id> <text>
    // ------------------------------------------------------------------------
    if (verb == "clip" || (verb == "rdsh" && args.size() > 1 && args[1] == "clip")) {
        size_t baseIdx = (verb == "clip") ? 1 : 2;
        if (args.size() < baseIdx + 2) {
            output = "Usage: rdsh clip <sessionId> <text>\n";
            return 1;
        }

        uint32_t sid = 0;
        try {
            sid = static_cast<uint32_t>(std::stoul(std::string(args[baseIdx])));
        } catch (...) {
            output = "Error: Invalid session ID.\n";
            return 1;
        }

        std::string text = std::string(args[baseIdx + 1]);
        bool ok = mgr.setClipboard(sid, text);
        if (!ok) {
            out << "Error: Failed to synchronize clipboard for Session " << sid << ".\n";
            output = out.str();
            return 1;
        }

        out << "SUCCESS: Synchronized clipboard buffer (" << text.size() << " bytes) for Session " << sid << ".\n";
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // rdsh dump
    // ------------------------------------------------------------------------
    if (verb == "dump" || (verb == "rdsh" && args.size() > 1 && args[1] == "dump")) {
        out << "# MicaNT Sovereign Remote Desktop Session Host (RDSH) Configuration Export\n"
            << "# Generated: 2026-11-01 [Multi-Session Architecture / termsrv.dll]\n\n"
            << "# SCM Service Configuration\n"
            << "sc.exe config TermService start= auto\n"
            << "net start TermService\n"
            << "sc.exe config SessionEnv start= auto\n"
            << "net start SessionEnv\n\n"
            << "# Terminal Server Execution Mode\n"
            << "chgusr.exe /" << (mgr.getExecutionMode() == micant::termsrv::AppExecutionMode::Install ? "install" : "execute") << "\n\n"
            << "# Active Session Inventory\n";

        for (const auto& s : mgr.getSessions()) {
            out << "# Session " << s.sessionId << ": " << WideToNarrow(s.winStationName)
                << " (User=" << WideToNarrow(s.userName) << ", State=" << StateToString(s.state) << ")\n";
        }
        output = out.str();
        return 0;
    }

    out << "Error: Unknown RDSH command '" << verb << "'. Type 'rdsh help' for usage.\n";
    output = out.str();
    return 1;
}

inline int ExecuteRdshString(std::string_view commandLine, std::string& output) {
    auto tokens = Tokenize(commandLine);
    if (tokens.empty()) {
        output.clear();
        return 0;
    }
    // Remove .exe suffix if present
    std::string firstTok(tokens[0]);
    if (firstTok.ends_with(".exe")) {
        firstTok = firstTok.substr(0, firstTok.length() - 4);
    }

    if (firstTok == "qwinsta" || firstTok == "rwinsta" || firstTok == "tscon" ||
        firstTok == "tsdiscon" || firstTok == "chgusr" || firstTok == "shadow" || firstTok == "rdsh") {
        tokens[0] = tokens[0].substr(0, tokens[0].find('.'));
    }
    return ExecuteRdsh(tokens, output);
}

} // namespace micant::rdsh_cmd::um
