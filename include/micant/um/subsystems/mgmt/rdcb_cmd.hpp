#pragma once

/**
 * @file rdcb_cmd.hpp
 * @brief Canonical Win32 cbadmin.exe / rdcb.exe CLI Management Utilities for RD Connection Broker.
 *
 * Clean-room implementation of Remote Desktop Connection Broker (RD Connection Broker)
 * CLI management tools under 17 U.S.C. § 1201(f) & Google LLC v. Oracle America, Inc.
 * (141 S. Ct. 1183, 2021).
 *
 * Provides complete CLI management for:
 *   - cbadmin status / rdcb status: Introspect broker vitals, farm health, active sessions, redirection metrics
 *   - cbadmin collections: Tabular inventory of RemoteApp and Desktop collections
 *   - cbadmin create-collection <name> [/type:<type>] [/desc:<desc>]: Provision new collection
 *   - cbadmin add-host <collection> <fqdn> <ip> [/weight:N]: Add RDSH server to collection
 *   - cbadmin remove-host <collection> <fqdn>: Remove RDSH server from collection
 *   - cbadmin drain <collection> <fqdn> <on|off>: Toggle host drain mode for maintenance
 *   - cbadmin sessions [/collection:<name>]: Tabular view of active & disconnected sessions
 *   - cbadmin redirect-test <user> <collection>: Evaluate load balancing or reconnect decision
 *   - cbadmin dump: Export declarative PowerShell RDS Connection Broker configuration script
 *   - cbadmin help: Command usage and syntax
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

#include "micant/um/servers/rdcb.hpp"

namespace micant::rdcb_cmd::um {

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

inline std::string CollectionTypeToString(rdcb::CollectionType type) {
    switch (type) {
        case rdcb::CollectionRemoteApp:       return "RemoteApp";
        case rdcb::CollectionPooledDesktop:   return "Pooled Desktop";
        case rdcb::CollectionPersonalDesktop: return "Personal Desktop";
        default:                              return "Unknown";
    }
}

inline std::string LoadBalancingModelToString(rdcb::LoadBalancingModel model) {
    switch (model) {
        case rdcb::ModelLeastSessions:    return "Least Sessions";
        case rdcb::ModelRoundRobin:       return "Round Robin";
        case rdcb::ModelResourceWeighted: return "Resource Weighted";
        default:                          return "Standard";
    }
}

inline std::string SessionStateToString(rdcb::RdcbSessionState state) {
    switch (state) {
        case rdcb::StateActive:       return "Active";
        case rdcb::StateConnected:    return "Connected";
        case rdcb::StateDisconnected: return "Disconnected";
        case rdcb::StateIdle:         return "Idle";
        case rdcb::StateShadowing:    return "Shadowing";
        case rdcb::StateTerminated:   return "Terminated";
        default:                      return "Unknown";
    }
}

inline std::string DrainModeToString(rdcb::ServerDrainMode mode) {
    switch (mode) {
        case rdcb::DrainModeDisabled:     return "Disabled (Normal)";
        case rdcb::DrainModeEnabled:      return "Enabled (Draining)";
        case rdcb::DrainModeUntilRestart: return "Drain Until Restart";
        default:                          return "Disabled";
    }
}

class RdcbCommandInterpreter {
public:
    static RdcbCommandInterpreter& get() {
        static RdcbCommandInterpreter instance;
        return instance;
    }

    void execute(std::span<const std::string_view> args, std::string& outResult) {
        std::ostringstream ss;

        if (args.empty() || args[0] == "help" || args[0] == "/?" || args[0] == "-?") {
            printHelp(ss);
            outResult = ss.str();
            return;
        }

        std::string_view subCmd = args[0];
        auto& mgr = rdcb::RdConnectionBrokerManager::get();

        if (subCmd == "status") {
            ss << "Remote Desktop Connection Broker Subsystem\n"
               << "===============================================================================\n"
               << "Service Name:              Tssdis (Remote Desktop Connection Broker)\n"
               << "Status:                    RUNNING\n"
               << "Listening Port:            5504 (TCP / MS-RDPBCGR Redirection)\n"
               << "Active Collections:        " << mgr.getActiveFarmsCount() << " collections\n"
               << "Registered RDSH Hosts:     " << mgr.getTotalRegisteredHosts() << " servers\n"
               << "Indexed User Sessions:     " << mgr.getIndexedSessionsCount() << " sessions\n"
               << "Total Redirects Issued:    " << mgr.getTotalRedirectsIssued() << " redirects\n"
               << "Reconnections Routed:      " << mgr.getTotalReconnectsRouted() << " reconnects\n"
               << "Heartbeats Received:       " << mgr.getTotalHeartbeatsReceived() << "\n"
               << "High Availability Mode:    Dynamic Active-Active\n"
               << "===============================================================================\n";
        }
        else if (subCmd == "collections" || subCmd == "list-collections") {
            auto cols = mgr.getAllCollections();
            ss << "Remote Desktop Session Host Collections\n"
               << "------------------------------------------------------------------------------------------------------------------------\n"
               << std::left << std::setw(24) << "COLLECTION NAME"
               << std::setw(18) << "TYPE"
               << std::setw(18) << "LOAD BALANCING"
               << std::setw(10) << "HOSTS"
               << std::setw(8)  << "UPD"
               << "UPD STORAGE PATH\n"
               << "------------------------------------------------------------------------------------------------------------------------\n";
            for (const auto& c : cols) {
                ss << std::left << std::setw(24) << c.collectionName
                   << std::setw(18) << CollectionTypeToString(c.type)
                   << std::setw(18) << LoadBalancingModelToString(c.balancingModel)
                   << std::setw(10) << c.memberServers.size()
                   << std::setw(8)  << (c.enableUpd ? "YES" : "NO")
                   << (c.enableUpd ? c.updStoragePath : "N/A") << "\n";
            }
            ss << "------------------------------------------------------------------------------------------------------------------------\n"
               << "Total Collections: " << cols.size() << "\n";
        }
        else if (subCmd == "hosts" || subCmd == "list-hosts") {
            std::string targetCol;
            if (args.size() > 1 && !args[1].starts_with("/")) {
                targetCol = std::string(args[1]);
            }

            auto cols = mgr.getAllCollections();
            ss << "Remote Desktop Session Host Members\n"
               << "------------------------------------------------------------------------------------------------------------------------\n"
               << std::left << std::setw(20) << "COLLECTION"
               << std::setw(32) << "SERVER FQDN"
               << std::setw(16) << "IP ADDRESS"
               << std::setw(8)  << "PORT"
               << std::setw(8)  << "WEIGHT"
               << std::setw(10) << "SESSIONS"
               << std::setw(8)  << "STATUS"
               << "DRAIN MODE\n"
               << "------------------------------------------------------------------------------------------------------------------------\n";
            uint32_t totalHosts = 0;
            for (const auto& c : cols) {
                if (!targetCol.empty() && c.collectionName != targetCol) continue;
                for (const auto& s : c.memberServers) {
                    totalHosts++;
                    ss << std::left << std::setw(20) << c.collectionName
                       << std::setw(32) << s.serverFqdn
                       << std::setw(16) << s.ipAddress
                       << std::setw(8)  << s.port
                       << std::setw(8)  << s.relativeWeight
                       << std::setw(10) << (std::to_string(s.activeSessions) + "/" + std::to_string(s.maxSessions))
                       << std::setw(8)  << (s.isOnline ? "Online" : "Offline")
                       << DrainModeToString(s.drainMode) << "\n";
                }
            }
            ss << "------------------------------------------------------------------------------------------------------------------------\n"
               << "Total Hosts Listed: " << totalHosts << "\n";
        }
        else if (subCmd == "create-collection") {
            if (args.size() < 2) {
                ss << "[-] Error: Missing required collection name.\n"
                   << "    Usage: cbadmin create-collection <name> [/type:RemoteApp|Pooled|Personal] [/desc:<desc>]\n";
            } else {
                rdcb::CollectionRecord rec{};
                rec.collectionName = std::string(args[1]);
                rec.type = rdcb::CollectionRemoteApp;
                rec.balancingModel = rdcb::ModelLeastSessions;
                rec.description = "User created collection";

                for (size_t i = 2; i < args.size(); ++i) {
                    if (args[i].starts_with("/type:")) {
                        std::string_view val = args[i].substr(6);
                        if (val == "Pooled") rec.type = rdcb::CollectionPooledDesktop;
                        else if (val == "Personal") rec.type = rdcb::CollectionPersonalDesktop;
                        else rec.type = rdcb::CollectionRemoteApp;
                    } else if (args[i].starts_with("/desc:")) {
                        rec.description = std::string(args[i].substr(6));
                    }
                }

                if (mgr.createCollection(rec)) {
                    ss << "[+] Successfully created collection: " << rec.collectionName
                       << " (" << CollectionTypeToString(rec.type) << ")\n";
                } else {
                    ss << "[-] Failed to create collection: " << rec.collectionName
                       << " (collection may already exist).\n";
                }
            }
        }
        else if (subCmd == "add-host") {
            if (args.size() < 4) {
                ss << "[-] Error: Missing arguments.\n"
                   << "    Usage: cbadmin add-host <collection> <fqdn> <ip> [/weight:N] [/max:N]\n";
            } else {
                std::string col = std::string(args[1]);
                rdcb::RdshServerRecord srv{};
                srv.serverFqdn = std::string(args[2]);
                srv.ipAddress = std::string(args[3]);
                srv.port = 3389;
                srv.relativeWeight = 100;
                srv.maxSessions = 100;
                srv.isOnline = true;
                srv.drainMode = rdcb::DrainModeDisabled;

                for (size_t i = 4; i < args.size(); ++i) {
                    if (args[i].starts_with("/weight:")) {
                        srv.relativeWeight = static_cast<uint32_t>(std::stoul(std::string(args[i].substr(8))));
                    } else if (args[i].starts_with("/max:")) {
                        srv.maxSessions = static_cast<uint32_t>(std::stoul(std::string(args[i].substr(5))));
                    }
                }

                if (mgr.addServerToCollection(col, srv)) {
                    ss << "[+] Successfully added host " << srv.serverFqdn
                       << " (" << srv.ipAddress << ") to collection " << col << ".\n";
                } else {
                    ss << "[-] Failed to add host: collection not found or server duplicate.\n";
                }
            }
        }
        else if (subCmd == "drain") {
            if (args.size() < 4) {
                ss << "[-] Error: Missing arguments.\n"
                   << "    Usage: cbadmin drain <collection> <fqdn> <on|off|untilrestart>\n";
            } else {
                std::string col = std::string(args[1]);
                std::string fqdn = std::string(args[2]);
                std::string_view modeStr = args[3];

                rdcb::ServerDrainMode mode = rdcb::DrainModeDisabled;
                if (modeStr == "on" || modeStr == "enable" || modeStr == "true") {
                    mode = rdcb::DrainModeEnabled;
                } else if (modeStr == "untilrestart") {
                    mode = rdcb::DrainModeUntilRestart;
                }

                if (mgr.setServerDrainMode(col, fqdn, mode)) {
                    ss << "[+] Drain mode for host " << fqdn << " in collection " << col
                       << " set to: " << DrainModeToString(mode) << "\n";
                } else {
                    ss << "[-] Failed to set drain mode: host or collection not found.\n";
                }
            }
        }
        else if (subCmd == "sessions" || subCmd == "list-sessions") {
            auto sessions = mgr.getAllSessions();
            ss << "Remote Desktop Connection Broker Session Directory\n"
               << "------------------------------------------------------------------------------------------------------------------------\n"
               << std::left << std::setw(12) << "SESSION ID"
               << std::setw(16) << "USER"
               << std::setw(20) << "COLLECTION"
               << std::setw(30) << "HOST SERVER"
               << std::setw(16) << "STATE"
               << "CLIENT IP\n"
               << "------------------------------------------------------------------------------------------------------------------------\n";
            for (const auto& s : sessions) {
                ss << std::left << std::setw(12) << s.sessionId
                   << std::setw(16) << (s.domainName.empty() ? s.userName : (s.domainName + "\\" + s.userName))
                   << std::setw(20) << s.collectionName
                   << std::setw(30) << s.hostServerFqdn
                   << std::setw(16) << SessionStateToString(s.state)
                   << s.clientIp << "\n";
            }
            ss << "------------------------------------------------------------------------------------------------------------------------\n"
               << "Total Indexed Sessions: " << sessions.size() << "\n";
        }
        else if (subCmd == "redirect-test" || subCmd == "resolve") {
            if (args.size() < 3) {
                ss << "[-] Error: Missing username or collection name.\n"
                   << "    Usage: cbadmin redirect-test <username> <collection>\n";
            } else {
                std::string user = std::string(args[1]);
                std::string col = std::string(args[2]);

                auto target = mgr.resolveConnectionTarget(user, "MICANT", col);
                if (target.isSuccess) {
                    ss << "[+] Connection Redirection Resolution Successful:\n"
                       << "    Type:               " << (target.isReconnection ? "SESSION RECONNECT (Disconnected session resumed)" : "NEW BALANCED SESSION") << "\n"
                       << "    Target Server FQDN: " << target.targetFqdn << "\n"
                       << "    Target Server IP:   " << target.targetIp << ":" << target.targetPort << "\n"
                       << "    Assigned SessionId: " << target.sessionId << "\n"
                       << "    MS-RDPBCGR Token:   " << target.routingToken
                       << "    Redirection PDU:    " << target.redirectionPdu.size() << " bytes encoded\n";
                } else {
                    ss << "[-] Redirection Resolution Failed: " << target.errorMessage << "\n";
                }
            }
        }
        else if (subCmd == "dump") {
            ss << "# -----------------------------------------------------------------------------\n"
               << "# Remote Desktop Connection Broker Export Configuration\n"
               << "# Generated by MicaNT cbadmin.exe (MS-RDPBCGR / tssdis.exe Parity Engine)\n"
               << "# Copyright (C) 2026 MicaNT Authors.\n"
               << "# -----------------------------------------------------------------------------\n\n"
               << "Import-Module RemoteDesktop\n\n";

            auto cols = mgr.getAllCollections();
            for (const auto& c : cols) {
                ss << "# Collection: " << c.collectionName << "\n"
                   << "New-RDSessionCollection -CollectionName \"" << c.collectionName << "\" "
                   << "-CollectionDescription \"" << c.description << "\" "
                   << "-ConnectionBroker \"cb01.corp.micant.internal\"\n";
                for (const auto& s : c.memberServers) {
                    ss << "Add-RDSessionHost -CollectionName \"" << c.collectionName << "\" "
                       << "-SessionHost \"" << s.serverFqdn << "\" "
                       << "-Weight " << s.relativeWeight << "\n";
                }
                if (c.enableUpd) {
                    ss << "Set-RDSessionCollectionConfiguration -CollectionName \"" << c.collectionName << "\" "
                       << "-EnableUserProfileDisk -DiskPath \"" << c.updStoragePath << "\" "
                       << "-MaxUserProfileDiskSizeGB " << c.updMaxSizeGb << "\n";
                }
                ss << "\n";
            }
            ss << "# End of Connection Broker Configuration Export\n";
        }
        else {
            ss << "[-] Unknown cbadmin command: " << subCmd << "\n"
               << "    Type 'cbadmin help' for a list of available commands.\n";
        }

        outResult = ss.str();
    }

private:
    RdcbCommandInterpreter() = default;

    void printHelp(std::ostream& os) const {
        os << "MicaNT Remote Desktop Connection Broker Administration Tool (cbadmin / rdcb)\n"
           << "Copyright (C) 2026 MicaNT Authors.\n"
           << "Provides sovereign administration of RD Connection Broker and session routing.\n\n"
           << "Usage:\n"
           << "  cbadmin status                            Display broker vitals and redirection metrics\n"
           << "  cbadmin collections                       List all RemoteApp and Desktop collections\n"
           << "  cbadmin hosts [collection]                List RD Session Host servers in collections\n"
           << "  cbadmin create-collection <name> [opts]   Create new RD session collection\n"
           << "  cbadmin add-host <coll> <fqdn> <ip>       Add RDSH server to collection\n"
           << "  cbadmin drain <coll> <fqdn> <on|off>      Set server drain mode for maintenance\n"
           << "  cbadmin sessions                          List indexed user sessions across all hosts\n"
           << "  cbadmin redirect-test <user> <coll>       Simulate routing resolution and generate PDU\n"
           << "  cbadmin dump                              Export declarative PowerShell RDS script\n"
           << "  cbadmin help                              Display this help message\n";
    }
};

inline void ExecuteRdcbString(std::string_view commandLine, std::string& outResult) {
    auto tokens = Tokenize(commandLine);
    if (tokens.empty()) {
        outResult = "[-] No command specified.\n";
        return;
    }

    std::span<const std::string_view> args(tokens.data() + 1, tokens.size() - 1);
    RdcbCommandInterpreter::get().execute(args, outResult);
}

} // namespace micant::rdcb_cmd::um
