#pragma once

/**
 * @file branchcache_cmd.hpp
 * @brief Canonical Win32 branchcache.exe / netsh branchcache Command-Line Management Utility.
 *
 * Clean-room implementation of the Microsoft Windows Server BranchCache & WAN Acceleration
 * administrative CLI utility under 17 U.S.C. § 1201(f) & Google LLC v. Oracle America, Inc.
 * (141 S. Ct. 1183, 2021).
 *
 * Provides complete CLI management for:
 *   - branchcache status: query operational mode, cache metrics, and WAN bandwidth savings
 *   - branchcache mode <distributed|hosted|disabled> [/server:<fqdn>]: configure cache mode
 *   - branchcache peers: inspect WS-Discovery subnet peer nodes and latency
 *   - branchcache flush: flush in-memory and persisted local cache blocks
 *   - branchcache directaccess (da): query DirectAccess IP-HTTPS transition tunnel state
 *   - branchcache quic (smbquic): query SMB over QUIC active sessions and 0-RTT migrations
 *   - branchcache dump: generate declarative configuration script
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

#include "micant/um/servers/branchcache.hpp"

namespace micant::branchcache_cmd::um {

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
// branchcache.exe CLI Command Handler
// ----------------------------------------------------------------------------
inline int ExecuteBranchCache(int argc, const char* argv[], std::ostream& out) {
    auto& bc = micant::wan::BranchCacheSubsystem::get();
    auto& da = micant::wan::DirectAccessSubsystem::get();
    auto& quic = micant::wan::SmbQuicSubsystem::get();

    bc.initialize();
    da.initialize();
    quic.initialize();

    if (argc < 2) {
        out << "Microsoft Windows [Version 10.0.26100.1]\n"
            << "BranchCache & Edge WAN Acceleration Utility (branchcache.exe)\n\n"
            << "Usage:\n"
            << "  branchcache status                           - Display BranchCache operational status\n"
            << "  branchcache mode <distributed|hosted|disabled> [/server:<fqdn>]\n"
            << "                                               - Configure BranchCache operational mode\n"
            << "  branchcache peers                            - List discovered subnet peer cache nodes\n"
            << "  branchcache flush                            - Flush local block cache contents\n"
            << "  branchcache directaccess (da)                - Display DirectAccess IP-HTTPS tunnel status\n"
            << "  branchcache quic (smbquic)                   - Display SMB over QUIC sessions & migrations\n"
            << "  branchcache dump                             - Export configuration script\n"
            << "  branchcache help                             - Display this help message\n";
        return 0;
    }

    std::string_view verb = argv[1];

    if (verb == "help" || verb == "/?" || verb == "-?") {
        const char* dummy[] = {"branchcache"};
        return ExecuteBranchCache(1, dummy, out);
    }

    // --- branchcache status ---
    if (verb == "status" || verb == "show") {
        out << "BranchCache Subsystem Status:\n"
            << "  Service Status:        Running (PeerDistSvc in svchost.exe -k PeerDist)\n"
            << "  Operational Mode:      " << micant::wan::BranchCacheModeToString(bc.getMode()) << "\n"
            << "  Hosted Cache Server:   " << bc.getHostedCacheServer() << "\n"
            << "  Cached Local Blocks:   " << bc.getLocalBlockCount() << "\n"
            << "  Discovered Peers:      " << bc.getDiscoveredPeerCount() << "\n"
            << "  Total Bytes Requested: " << FormatBytes(bc.getBytesRequested()) << "\n"
            << "  Bytes from Cache:      " << FormatBytes(bc.getBytesFromLocalCache()) << "\n"
            << "  Bytes from Peers:      " << FormatBytes(bc.getBytesFromPeers()) << "\n"
            << "  Bytes from Origin:     " << FormatBytes(bc.getBytesFromOrigin()) << "\n"
            << "  WAN Bandwidth Savings: " << std::fixed << std::setprecision(1) << bc.getWanSavingsRatio() << "%\n";
        return 0;
    }

    // --- branchcache mode <distributed|hosted|disabled> [/server:<fqdn>] ---
    if (verb == "mode" || verb == "set") {
        if (argc < 3) {
            out << "Error: Missing mode parameter. Syntax: branchcache mode <distributed|hosted|disabled> [/server:<fqdn>]\n";
            return 1;
        }
        std::string_view mArg = argv[2];
        if (mArg == "distributed" || mArg == "p2p") {
            bc.setMode(micant::wan::BranchCacheMode::Distributed);
            out << "BranchCache mode configured to Distributed Cache (Peer-to-Peer on local subnet).\n";
        } else if (mArg == "hosted") {
            std::string server = "hostedcache.corp.contoso.com";
            for (int i = 3; i < argc; ++i) {
                std::string_view a = argv[i];
                if (a.starts_with("/server:")) server = std::string(a.substr(8));
            }
            bc.setHostedCacheServer(server);
            out << "BranchCache mode configured to Hosted Cache (Server: " << server << ").\n";
        } else if (mArg == "disabled" || mArg == "disable") {
            bc.setMode(micant::wan::BranchCacheMode::Disabled);
            out << "BranchCache operational mode disabled.\n";
        } else {
            out << "Unknown mode: " << mArg << ". Valid options: distributed, hosted, disabled.\n";
            return 1;
        }
        return 0;
    }

    // --- branchcache peers ---
    if (verb == "peers") {
        auto peers = bc.getAllPeers();
        out << "PeerId               IPAddress        Port  RTT(ms)  BlocksServed  Status\n"
            << "-------------------  ---------------  ----  -------  ------------  ------\n";

        for (const auto& p : peers) {
            out << std::left
                << std::setw(21) << p.peerId
                << std::setw(17) << p.ipAddress
                << std::setw(6)  << p.port
                << std::setw(9)  << p.roundTripTimeMs
                << std::setw(14) << p.blocksServed
                << (p.isActive ? "Active" : "Offline") << "\n";
        }
        return 0;
    }

    // --- branchcache flush ---
    if (verb == "flush") {
        bc.flushCache();
        out << "BranchCache local block cache flushed successfully (0 blocks remaining).\n";
        return 0;
    }

    // --- branchcache directaccess (da) ---
    if (verb == "directaccess" || verb == "da") {
        const auto& cfg = da.getConfig();
        out << "DirectAccess & IP-HTTPS Transition Tunnel Status:\n"
            << "  Driver Status:         Running (iphttps.sys)\n"
            << "  Tunnel State:          " << micant::wan::DirectAccessTunnelStateToString(da.getState()) << "\n"
            << "  Gateway FQDN:          " << cfg.gatewayFqdn << ":" << cfg.gatewayPort << "\n"
            << "  Virtual IPv6 Address:  " << cfg.clientIpv6Address << "\n"
            << "  IPv6 Route Prefix:     " << cfg.virtualIpv6Prefix << "\n"
            << "  Gateway Round-Trip:    " << da.getGatewayLatencyMs() << " ms\n"
            << "  Packets Encapsulated:  " << da.getPacketsEncapsulated() << "\n"
            << "  Packets Decapsulated:  " << da.getPacketsDecapsulated() << "\n"
            << "  Total Bytes Traversed: " << FormatBytes(da.getBytesTransferred()) << "\n";
        return 0;
    }

    // --- branchcache quic (smbquic) ---
    if (verb == "quic" || verb == "smbquic") {
        auto sessions = quic.getAllSessions();
        out << "SMB over QUIC (RFC 9000) Transport Status:\n"
            << "  Driver Status:         Running (smbquic.sys)\n"
            << "  Active Sessions:       " << sessions.size() << "\n\n"
            << "SessionId  ServerHost              Port  State                   0-RTT  Migrated  RTT(ms)\n"
            << "---------  ----------------------  ----  -----                   -----  --------  -------\n";

        for (const auto& s : sessions) {
            out << std::left
                << std::setw(11) << s->getId()
                << std::setw(24) << s->getServerHost()
                << std::setw(6)  << s->getServerPort()
                << std::setw(24) << micant::wan::SmbQuicConnectionStateToString(s->getState())
                << std::setw(7)  << (s->is0RttResumed() ? "Yes" : "No")
                << std::setw(10) << (s->isMigrated() ? "Yes" : "No")
                << s->getRttMs() << "\n";
        }
        return 0;
    }

    // --- branchcache dump ---
    if (verb == "dump") {
        out << "# ------------------------------------------------------------\n"
            << "# MicaNT BranchCache & WAN Acceleration Netsh Configuration Script\n"
            << "# ------------------------------------------------------------\n"
            << "netsh branchcache set service mode=" << ((bc.getMode() == micant::wan::BranchCacheMode::Hosted) ? "hostedclient" : "distributed") << "\n"
            << "netsh branchcache set service hostedcache=" << bc.getHostedCacheServer() << "\n"
            << "netsh interface iphttps set int " << da.getConfig().gatewayFqdn << "\n";
        return 0;
    }

    out << "Unknown command: branchcache " << verb << ". Type 'branchcache help' for usage.\n";
    return 1;
}

inline void ExecuteBranchCacheString(std::string_view commandLine, std::string& out) {
    auto tokens = Tokenize(commandLine);
    if (tokens.empty()) {
        out = "branchcache: missing command arguments.\n";
        return;
    }

    std::vector<std::string> args;
    args.reserve(tokens.size());
    for (auto t : tokens) args.emplace_back(t);

    std::vector<const char*> argv;
    argv.reserve(args.size());
    for (const auto& a : args) argv.push_back(a.c_str());

    std::ostringstream ss;
    ExecuteBranchCache(static_cast<int>(argv.size()), argv.data(), ss);
    out = ss.str();
}

} // namespace micant::branchcache_cmd::um
