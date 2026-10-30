// ============================================================================
// MicaNT: Clean-Room Windows-Compatible Operating System Executive
// Subsystem: Canonical DHCP Configuration Command-Line Tool (netsh dhcp)
// Implementation: include/micant/um/subsystems/net/netsh_dhcp.hpp
//
// Strict Clean-Room Implementation in modern ISO C++23. Zero External Dependencies.
// Conforms to Microsoft Win32 Command-Line Management Specifications under 17 U.S.C. Section 1201(f).
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <span>

#include "micant/um/servers/dhcp_server.hpp"

namespace micant::netsh::dhcp::um {

class NetshDhcpCommandExecutor {
public:
    static NetshDhcpCommandExecutor& Instance() noexcept {
        static NetshDhcpCommandExecutor s_instance;
        return s_instance;
    }

    int execute(std::span<const std::string_view> args, std::string& outStr) noexcept {
        if (args.empty()) {
            return showHelp(outStr);
        }

        size_t argIdx = 0;
        if (args[argIdx] == "netsh") {
            argIdx++;
            if (argIdx >= args.size()) return showHelp(outStr);
        }
        if (args[argIdx] == "dhcp") {
            argIdx++;
            if (argIdx >= args.size()) return showHelp(outStr);
        }
        if (args[argIdx] == "server") {
            argIdx++;
            if (argIdx >= args.size()) return showHelp(outStr);
        }

        std::string_view verb = args[argIdx++];
        std::span<const std::string_view> subArgs = args.subspan(argIdx);

        if (verb == "show") {
            return executeShow(subArgs, outStr);
        } else if (verb == "add") {
            return executeAdd(subArgs, outStr);
        } else if (verb == "delete") {
            return executeDelete(subArgs, outStr);
        } else if (verb == "scope") {
            return executeScope(subArgs, outStr);
        } else if (verb == "set") {
            return executeSet(subArgs, outStr);
        } else if (verb == "dump") {
            return executeDump(outStr);
        } else if (verb == "help" || verb == "/?" || verb == "?") {
            return showHelp(outStr);
        } else {
            outStr = "The following command was not found: " + std::string(verb) + "\r\n"
                     "Type 'netsh dhcp server help' to display a list of commands.\r\n";
            return 1;
        }
    }

private:
    NetshDhcpCommandExecutor() = default;

    int showHelp(std::string& out) noexcept {
        std::ostringstream ss;
        ss << "\r\nThe following commands are available in this context:\r\n\r\n"
           << "Commands in this context:\r\n"
           << "show scope        - Displays all active scopes on the DHCP server.\r\n"
           << "show status       - Displays server operational and AD authorization status.\r\n"
           << "add scope         - Creates a new subnet scope.\r\n"
           << "delete scope      - Deletes an existing subnet scope.\r\n"
           << "scope <subnet>    - Context for scope-specific commands (add iprange, reservedip, show clients).\r\n"
           << "set status        - Sets the Active Directory authorization status.\r\n"
           << "dump              - Dumps configuration script.\r\n\r\n"
           << "To view help for a command, type the command, followed by a space, and then type ?.\r\n\r\n";
        out = ss.str();
        return 0;
    }

    int executeShow(std::span<const std::string_view> args, std::string& out) noexcept {
        if (args.empty()) {
            out = "Command syntax: netsh dhcp server show [scope | status]\r\n";
            return 1;
        }

        std::string_view target = args[0];
        auto& srv = micant::dhcp::EnterpriseDhcpServer::instance();

        if (target == "scope" || target == "scopes") {
            auto scopes = srv.getAllScopes();
            std::ostringstream ss;
            ss << "\r\nScope Address    Subnet Mask      State     Scope Name\r\n"
               << "========================================================================\r\n";
            for (const auto& sc : scopes) {
                ss << std::left << std::setw(17) << sc.scopeId
                   << std::setw(17) << sc.subnetMask
                   << std::setw(10) << (sc.isActive ? "Active" : "Inactive")
                   << sc.scopeName << "\r\n";
            }
            ss << "\r\nTotal Scopes: " << scopes.size() << "\r\n"
               << "Command completed successfully.\r\n";
            out = ss.str();
            return 0;

        } else if (target == "status") {
            std::ostringstream ss;
            ss << "\r\nDHCP Server Service Configuration & Status:\r\n"
               << "========================================================================\r\n"
               << "Server Host Name:             " << srv.getServerHost() << "\r\n"
               << "Server IPv4 Address:          " << srv.getServerIp() << "\r\n"
               << "Active Directory Domain:      " << srv.getDomainName() << "\r\n"
               << "Active Directory Path:        " << srv.getAdDirectoryPath() << "\r\n"
               << "Authorization Status:         " << (srv.isServerAuthorized() ? "Authorized in AD" : "Unauthorized in AD") << "\r\n"
               << "Rogue Detection Status:       " << (srv.isRogueSuppressed() ? "Suppressed (Rogue Server Detected)" : "Normal (Servicing Active)") << "\r\n"
               << "Active IPv4 Scopes:           " << srv.getScopeCount() << "\r\n"
               << "Total Discovers Handled:      " << srv.getTotalDiscovers() << "\r\n"
               << "Total Offers Sent:            " << srv.getTotalOffers() << "\r\n"
               << "Total Requests Processed:     " << srv.getTotalRequests() << "\r\n"
               << "Total Acks Sent:              " << srv.getTotalAcks() << "\r\n"
               << "Total Naks Sent:              " << srv.getTotalNaks() << "\r\n"
               << "Total Releases:               " << srv.getTotalReleases() << "\r\n"
               << "Total DNS Updates:            " << srv.getTotalDnsUpdatesSucceeded() << " / " << srv.getTotalDnsUpdatesAttempted() << "\r\n"
               << "Command completed successfully.\r\n";
            out = ss.str();
            return 0;
        }

        out = "Unknown show target: " + std::string(target) + "\r\n";
        return 1;
    }

    int executeAdd(std::span<const std::string_view> args, std::string& out) noexcept {
        if (args.empty()) {
            out = "Command syntax: netsh dhcp server add scope <subnet> <mask|prefix> [name]\r\n";
            return 1;
        }

        if (args[0] == "scope") {
            if (args.size() < 3) {
                out = "Command syntax: netsh dhcp server add scope <subnet> <mask> [name]\r\n";
                return 1;
            }
            std::string subnet(args[1]);
            std::string mask(args[2]);
            std::string name = (args.size() > 3) ? std::string(args[3]) : ("Scope-" + subnet);

            std::string startIp = micant::dhcp::uintToIp(micant::dhcp::ipToUint(subnet) + 10);
            std::string endIp   = micant::dhcp::uintToIp(micant::dhcp::ipToUint(subnet) + 200);

            auto& srv = micant::dhcp::EnterpriseDhcpServer::instance();
            if (srv.createScope(subnet, mask, startIp, endIp, 691200, name)) {
                out = "Scope " + subnet + " added successfully.\r\nCommand completed successfully.\r\n";
                return 0;
            } else {
                out = "Failed to add scope " + subnet + ". It may already exist or parameters are invalid.\r\n";
                return 1;
            }
        }

        out = "Unknown add target: " + std::string(args[0]) + "\r\n";
        return 1;
    }

    int executeDelete(std::span<const std::string_view> args, std::string& out) noexcept {
        if (args.size() < 2 || args[0] != "scope") {
            out = "Command syntax: netsh dhcp server delete scope <subnet>\r\n";
            return 1;
        }

        std::string subnet(args[1]);
        auto& srv = micant::dhcp::EnterpriseDhcpServer::instance();
        if (srv.deleteScope(subnet)) {
            out = "Scope " + subnet + " deleted successfully.\r\nCommand completed successfully.\r\n";
            return 0;
        } else {
            out = "Failed to delete scope " + subnet + ". Scope not found.\r\n";
            return 1;
        }
    }

    int executeScope(std::span<const std::string_view> args, std::string& out) noexcept {
        if (args.size() < 2) {
            out = "Command syntax: netsh dhcp server scope <subnet> [add | delete | show]\r\n";
            return 1;
        }

        std::string subnet(args[0]);
        std::string_view verb = args[1];
        std::span<const std::string_view> subArgs = args.subspan(2);

        auto& srv = micant::dhcp::EnterpriseDhcpServer::instance();
        micant::dhcp::DhcpScope sc;
        if (!srv.getScope(subnet, sc)) {
            out = "Scope " + subnet + " not found on DHCP server.\r\n";
            return 1;
        }

        if (verb == "show") {
            if (subArgs.empty() || subArgs[0] == "clients" || subArgs[0] == "leases") {
                std::ostringstream ss;
                ss << "\r\nActive DHCP Client Leases for Scope [" << subnet << "]:\r\n"
                   << "===========================================================================================\r\n"
                   << "IP Address       Subnet Mask      MAC Address        Status    Client Name\r\n"
                   << "===========================================================================================\r\n";
                for (const auto& [ip, lease] : sc.leases) {
                    ss << std::left << std::setw(17) << ip
                       << std::setw(17) << sc.subnetMask
                       << std::setw(19) << lease.macAddress
                       << std::setw(10) << micant::dhcp::LeaseStateToString(lease.state)
                       << lease.hostName << "\r\n";
                }
                ss << "\r\nTotal Leases: " << sc.leases.size() << "\r\n"
                   << "Command completed successfully.\r\n";
                out = ss.str();
                return 0;

            } else if (subArgs[0] == "reservedip" || subArgs[0] == "reservations") {
                std::ostringstream ss;
                ss << "\r\nReserved IP Addresses for Scope [" << subnet << "]:\r\n"
                   << "========================================================================\r\n"
                   << "IP Address       MAC Address        Client Name\r\n"
                   << "========================================================================\r\n";
                for (const auto& [mac, res] : sc.reservations) {
                    ss << std::left << std::setw(17) << res.ipAddress
                       << std::setw(19) << res.macAddress
                       << res.clientName << "\r\n";
                }
                ss << "\r\nTotal Reservations: " << sc.reservations.size() << "\r\n"
                   << "Command completed successfully.\r\n";
                out = ss.str();
                return 0;
            }

        } else if (verb == "add") {
            if (subArgs.size() >= 3 && subArgs[0] == "reservedip") {
                std::string ip(subArgs[1]);
                std::string mac(subArgs[2]);
                std::string name = (subArgs.size() > 3) ? std::string(subArgs[3]) : ("Reserved-" + ip);
                if (srv.addReservation(subnet, ip, mac, name)) {
                    out = "Reservation " + ip + " [" + mac + "] added to scope " + subnet + ".\r\nCommand completed successfully.\r\n";
                    return 0;
                } else {
                    out = "Failed to add reservation " + ip + " to scope " + subnet + ".\r\n";
                    return 1;
                }
            } else if (subArgs.size() >= 3 && subArgs[0] == "iprange") {
                std::string start(subArgs[1]);
                std::string end(subArgs[2]);
                sc.startIp = start;
                sc.endIp = end;
                out = "IP range for scope " + subnet + " set to " + start + " - " + end + ".\r\nCommand completed successfully.\r\n";
                return 0;
            }

        } else if (verb == "delete") {
            if (subArgs.size() >= 3 && subArgs[0] == "reservedip") {
                std::string ip(subArgs[1]);
                std::string mac(subArgs[2]);
                if (srv.deleteReservation(subnet, ip, mac)) {
                    out = "Reservation " + ip + " deleted from scope " + subnet + ".\r\nCommand completed successfully.\r\n";
                    return 0;
                } else {
                    out = "Failed to delete reservation " + ip + " from scope " + subnet + ".\r\n";
                    return 1;
                }
            }
        }

        out = "Invalid scope command syntax.\r\n";
        return 1;
    }

    int executeSet(std::span<const std::string_view> args, std::string& out) noexcept {
        if (args.size() < 2 || args[0] != "status") {
            out = "Command syntax: netsh dhcp server set status <authorized | unauthorized>\r\n";
            return 1;
        }

        auto& srv = micant::dhcp::EnterpriseDhcpServer::instance();
        std::string_view statusVal = args[1];

        if (statusVal == "authorized" || statusVal == "auth") {
            srv.authorizeServerInAd(srv.getServerIp(), srv.getDomainName());
            out = "DHCP Server status set to AUTHORIZED in Active Directory (" + srv.getDomainName() + ").\r\nCommand completed successfully.\r\n";
            return 0;
        } else if (statusVal == "unauthorized" || statusVal == "unauth") {
            srv.unauthorizeServerInAd(srv.getServerIp(), srv.getDomainName());
            out = "DHCP Server status set to UNAUTHORIZED in Active Directory (" + srv.getDomainName() + ").\r\nRogue server protection active.\r\nCommand completed successfully.\r\n";
            return 0;
        }

        out = "Invalid status option. Must be 'authorized' or 'unauthorized'.\r\n";
        return 1;
    }

    int executeDump(std::string& out) noexcept {
        auto& srv = micant::dhcp::EnterpriseDhcpServer::instance();
        auto scopes = srv.getAllScopes();

        std::ostringstream ss;
        ss << "# =====================================================================\r\n"
           << "# MicaNT DHCP Server Configuration Script Dump\r\n"
           << "# Generated by netsh dhcp server dump\r\n"
           << "# =====================================================================\r\n\r\n"
           << "# Server Configuration\r\n"
           << "netsh dhcp server set status " << (srv.isServerAuthorized() ? "authorized" : "unauthorized") << "\r\n\r\n";

        for (const auto& sc : scopes) {
            ss << "# Scope " << sc.scopeId << "\r\n"
               << "netsh dhcp server add scope " << sc.scopeId << " " << sc.subnetMask << " \"" << sc.scopeName << "\"\r\n"
               << "netsh dhcp server scope " << sc.scopeId << " add iprange " << sc.startIp << " " << sc.endIp << "\r\n";
            for (const auto& [mac, res] : sc.reservations) {
                ss << "netsh dhcp server scope " << sc.scopeId << " add reservedip " << res.ipAddress << " " << res.macAddress << " \"" << res.clientName << "\"\r\n";
            }
            ss << "\r\n";
        }
        out = ss.str();
        return 0;
    }
};

inline int ExecuteNetshDhcp(const std::vector<std::string>& tokens, std::ostream& out) {
    std::vector<std::string_view> views;
    views.reserve(tokens.size());
    for (const auto& s : tokens) {
        views.emplace_back(s);
    }
    std::string res;
    int rc = NetshDhcpCommandExecutor::Instance().execute(views, res);
    out << res;
    return rc;
}

inline int ExecuteNetshDhcpString(std::string_view cmdLine, std::string& out) {
    std::vector<std::string_view> tokens;
    size_t pos = 0;
    while (pos < cmdLine.size()) {
        while (pos < cmdLine.size() && (cmdLine[pos] == ' ' || cmdLine[pos] == '\t')) pos++;
        if (pos >= cmdLine.size()) break;
        size_t start = pos;
        while (pos < cmdLine.size() && cmdLine[pos] != ' ' && cmdLine[pos] != '\t') pos++;
        tokens.emplace_back(cmdLine.data() + start, pos - start);
    }
    return NetshDhcpCommandExecutor::Instance().execute(tokens, out);
}

} // namespace micant::netsh::dhcp::um
