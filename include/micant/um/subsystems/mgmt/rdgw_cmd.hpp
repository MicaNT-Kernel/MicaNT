#pragma once

/**
 * @file rdgw_cmd.hpp
 * @brief Canonical Win32 tsgadmin.exe / rdgw.exe CLI Management Utilities for RD Gateway.
 *
 * Clean-room implementation of Microsoft Windows Server Remote Desktop Gateway (RD Gateway)
 * CLI management tools under 17 U.S.C. § 1201(f) & Google LLC v. Oracle America, Inc.
 * (141 S. Ct. 1183, 2021).
 *
 * Provides complete CLI management for:
 *   - tsgadmin status / rdgw status: Introspect gateway health, active tunnels, policy stats
 *   - tsgadmin cap list: Tabular inventory of Connection Authorization Policies (RD CAPs)
 *   - tsgadmin cap add <name> <group>: Create new RD CAP
 *   - tsgadmin rap list: Tabular inventory of Resource Authorization Policies (RD RAPs)
 *   - tsgadmin rap add <name> <group> <resource> [port]: Create new RD RAP
 *   - tsgadmin tunnel list: Tabular inventory of active encapsulated RDP sessions
 *   - tsgadmin tunnel close <id>: Terminate an active encapsulated tunnel
 *   - tsgadmin dump: Export declarative PowerShell RDS Gateway configuration script
 *   - tsgadmin help: Command usage and syntax
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

#include "micant/um/servers/rdgateway.hpp"

namespace micant::rdgw_cmd::um {

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

inline std::string TunnelStateToString(micant::rdgw::TunnelState st) {
    switch (st) {
        case micant::rdgw::TunnelConnected:     return "Connected";
        case micant::rdgw::TunnelConnecting:    return "Connecting";
        case micant::rdgw::TunnelDisconnecting: return "Disconnecting";
        case micant::rdgw::TunnelDisconnected:  return "Disconnected";
        default:                                return "Unknown";
    }
}

inline std::string JoinStrings(const std::vector<std::string>& vec, std::string_view delim = ", ") {
    if (vec.empty()) return "*";
    std::ostringstream ss;
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) ss << delim;
        ss << vec[i];
    }
    return ss.str();
}

inline int ExecuteRdgw(std::span<std::string_view> args, std::string& output) {
    std::ostringstream out;
    auto& mgr = micant::rdgw::RdGatewayManager::get();

    if (args.empty() || args[0] == "help" || args[0] == "/?" || args[0] == "-h" || args[0] == "--help") {
        out << "Microsoft Windows Remote Desktop Gateway (RD Gateway) Subsystem CLI\n"
            << "Copyright (C) Microsoft Corporation / Sovereign Clean-Room MicaNT.\n\n"
            << "Available Utilities:\n"
            << "  tsgadmin status / rdgw status                 Introspect gateway status, active tunnels, metrics\n"
            << "  tsgadmin cap list                             Tabular inventory of Connection Authorization Policies\n"
            << "  tsgadmin cap add <name> <group>               Create a new RD CAP\n"
            << "  tsgadmin rap list                             Tabular inventory of Resource Authorization Policies\n"
            << "  tsgadmin rap add <name> <group> <res> [port]  Create a new RD RAP\n"
            << "  tsgadmin tunnel list                          Tabular inventory of active RDP HTTPS tunnels\n"
            << "  tsgadmin tunnel close <id>                    Terminate an active encapsulated tunnel\n"
            << "  tsgadmin dump                                 Export declarative PowerShell RD Gateway configuration\n"
            << "  tsgadmin help                                 Display command syntax\n";
        output = out.str();
        return 0;
    }

    std::string verb(args[0]);

    if ((verb == "tsgadmin" || verb == "rdgw") && args.size() > 1) {
        return ExecuteRdgw(args.subspan(1), output);
    }

    // ------------------------------------------------------------------------
    // status
    // ------------------------------------------------------------------------
    if (verb == "status") {
        auto info = mgr.getServerInfo();
        out << "Remote Desktop Gateway Subsystem Status:\n"
            << "  Service Name:        TSGateway (Running)\n"
            << "  Server Name:         " << info.serverName << "\n"
            << "  Server FQDN:         " << info.fqdn << "\n"
            << "  Listening Port:      TCP " << info.listeningPort << " (HTTPS / WSS / RPC-HTTP)\n"
            << "  SSL Certificate:     " << (info.sslCertificateConfigured ? "Configured" : "Missing") << "\n"
            << "  SSL Thumbprint:      " << info.certificateThumbprint << "\n"
            << "  Active Tunnels:      " << mgr.getActiveTunnelCount() << "\n"
            << "  CAP Evaluations:     " << mgr.getTotalCapEvaluations() << "\n"
            << "  RAP Evaluations:     " << mgr.getTotalRapEvaluations() << "\n"
            << "  Bytes Transferred:   " << mgr.getTotalBytesTransferred() << " bytes\n"
            << "  Max Connections:     " << info.maxConnections << "\n";
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // cap list / cap-list
    // ------------------------------------------------------------------------
    if (verb == "cap-list" || (verb == "cap" && args.size() > 1 && args[1] == "list")) {
        auto caps = mgr.getCaps();
        out << std::left
            << std::setw(8)  << "CAPID"
            << std::setw(30) << "NAME"
            << std::setw(10) << "STATUS"
            << std::setw(12) << "IDLE(MIN)"
            << "USER GROUPS\n";
        out << std::string(80, '-') << "\n";

        for (const auto& c : caps) {
            out << std::left
                << std::setw(8)  << c.capId
                << std::setw(30) << c.name
                << std::setw(10) << (c.enabled ? "Enabled" : "Disabled")
                << std::setw(12) << c.idleTimeoutMinutes
                << JoinStrings(c.allowedUserGroups) << "\n";
        }
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // cap add <name> <group>
    // ------------------------------------------------------------------------
    if (verb == "cap-add" || (verb == "cap" && args.size() > 1 && args[1] == "add")) {
        size_t offset = (verb == "cap-add") ? 1 : 2;
        if (args.size() <= offset + 1) {
            out << "Error: cap add requires <name> and <group>.\n";
            output = out.str();
            return 1;
        }

        std::string name(args[offset]);
        std::string group(args[offset + 1]);

        micant::rdgw::ConnectionAuthPolicy cap{};
        cap.name = name;
        cap.enabled = true;
        cap.allowedUserGroups = {group};
        cap.allowedAuthMethods = micant::rdgw::AuthPassword | micant::rdgw::AuthSmartCard;

        uint32_t capId = mgr.addCap(cap);
        out << "SUCCESS: Created RD Connection Authorization Policy (CAP) ID " << capId
            << " ('" << name << "') for group '" << group << "'.\n";
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // rap list / rap-list
    // ------------------------------------------------------------------------
    if (verb == "rap-list" || (verb == "rap" && args.size() > 1 && args[1] == "list")) {
        auto raps = mgr.getRaps();
        out << std::left
            << std::setw(8)  << "RAPID"
            << std::setw(28) << "NAME"
            << std::setw(10) << "STATUS"
            << std::setw(24) << "RESOURCES"
            << "USER GROUPS\n";
        out << std::string(80, '-') << "\n";

        for (const auto& r : raps) {
            out << std::left
                << std::setw(8)  << r.rapId
                << std::setw(28) << r.name
                << std::setw(10) << (r.enabled ? "Enabled" : "Disabled")
                << std::setw(24) << JoinStrings(r.allowedResources)
                << JoinStrings(r.allowedUserGroups) << "\n";
        }
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // rap add <name> <group> <resource> [port]
    // ------------------------------------------------------------------------
    if (verb == "rap-add" || (verb == "rap" && args.size() > 1 && args[1] == "add")) {
        size_t offset = (verb == "rap-add") ? 1 : 2;
        if (args.size() <= offset + 2) {
            out << "Error: rap add requires <name> <group> and <resource>.\n";
            output = out.str();
            return 1;
        }

        std::string name(args[offset]);
        std::string group(args[offset + 1]);
        std::string res(args[offset + 2]);
        uint16_t port = 3389;
        if (args.size() > offset + 3) {
            try {
                port = static_cast<uint16_t>(std::stoul(std::string(args[offset + 3])));
            } catch (...) {}
        }

        micant::rdgw::ResourceAuthPolicy rap{};
        rap.name = name;
        rap.enabled = true;
        rap.allowedUserGroups = {group};
        rap.allowedResources = {res};
        rap.allowedPorts = {port};

        uint32_t rapId = mgr.addRap(rap);
        out << "SUCCESS: Created RD Resource Authorization Policy (RAP) ID " << rapId
            << " ('" << name << "') targeting '" << res << ":" << port << "'.\n";
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // tunnel list / tunnel-list
    // ------------------------------------------------------------------------
    if (verb == "tunnel-list" || (verb == "tunnel" && args.size() > 1 && args[1] == "list") || verb == "tunnels") {
        auto tunnels = mgr.getTunnels();
        out << std::left
            << std::setw(10) << "TUNNELID"
            << std::setw(16) << "CLIENT IP"
            << std::setw(16) << "USER"
            << std::setw(22) << "DESTINATION"
            << std::setw(14) << "STATE"
            << "BYTES IN/OUT\n";
        out << std::string(90, '-') << "\n";

        for (const auto& t : tunnels) {
            std::string dest = t.destinationHost + ":" + std::to_string(t.destinationPort);
            std::string bytes = std::to_string(t.bytesIn) + " / " + std::to_string(t.bytesOut);
            out << std::left
                << std::setw(10) << t.tunnelId
                << std::setw(16) << t.clientIp
                << std::setw(16) << t.userName
                << std::setw(22) << dest
                << std::setw(14) << TunnelStateToString(t.state)
                << bytes << "\n";
        }
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // tunnel close <tunnelId>
    // ------------------------------------------------------------------------
    if (verb == "tunnel-close" || (verb == "tunnel" && args.size() > 1 && args[1] == "close")) {
        size_t offset = (verb == "tunnel-close") ? 1 : 2;
        if (args.size() <= offset) {
            out << "Error: tunnel close requires <tunnelId>.\n";
            output = out.str();
            return 1;
        }

        uint64_t tid = 0;
        try {
            tid = std::stoull(std::string(args[offset]));
        } catch (...) {
            out << "Error: Invalid tunnel ID.\n";
            output = out.str();
            return 1;
        }

        if (mgr.closeTunnel(tid)) {
            out << "SUCCESS: Closed RD Gateway Tunnel ID " << tid << ".\n";
            output = out.str();
            return 0;
        } else {
            out << "Error: Tunnel ID " << tid << " not found or already closed.\n";
            output = out.str();
            return 1;
        }
    }

    // ------------------------------------------------------------------------
    // dump
    // ------------------------------------------------------------------------
    if (verb == "dump") {
        auto info = mgr.getServerInfo();
        out << "# MicaNT Sovereign Remote Desktop Gateway (RD Gateway) Configuration Export\n"
            << "# Generated: 2026-11-01 [MS-TSGU Protocol / tsgateway.exe]\n\n"
            << "# SCM Service Configuration\n"
            << "sc.exe config TSGateway start= auto\n"
            << "net start TSGateway\n\n"
            << "# Gateway SSL Certificate Binding\n"
            << "netsh http add sslcert ipport=0.0.0.0:" << info.listeningPort
            << " certhash=" << info.certificateThumbprint << " appid={8F3A2B1C-9D0E-4F5A-6B7C-8D9E0F1A2B3C}\n\n"
            << "# RD Connection Authorization Policies (CAPs)\n";

        for (const auto& c : mgr.getCaps()) {
            out << "# CAP " << c.capId << ": " << c.name << " [Groups: " << JoinStrings(c.allowedUserGroups) << "]\n";
        }
        out << "\n# RD Resource Authorization Policies (RAPs)\n";
        for (const auto& r : mgr.getRaps()) {
            out << "# RAP " << r.rapId << ": " << r.name << " [Resources: " << JoinStrings(r.allowedResources)
                << ", Ports: " << (r.allowedPorts.empty() ? 3389 : r.allowedPorts[0]) << "]\n";
        }
        output = out.str();
        return 0;
    }

    out << "Error: Unknown RD Gateway command '" << verb << "'. Type 'tsgadmin help' for usage.\n";
    output = out.str();
    return 1;
}

inline int ExecuteRdgwString(std::string_view commandLine, std::string& output) {
    auto tokens = Tokenize(commandLine);
    if (tokens.empty()) {
        output.clear();
        return 0;
    }
    std::string firstTok(tokens[0]);
    if (firstTok.ends_with(".exe")) {
        firstTok = firstTok.substr(0, firstTok.length() - 4);
    }
    if (firstTok == "tsgadmin" || firstTok == "rdgw") {
        tokens[0] = tokens[0].substr(0, tokens[0].find('.'));
    }
    return ExecuteRdgw(tokens, output);
}

} // namespace micant::rdgw_cmd::um
