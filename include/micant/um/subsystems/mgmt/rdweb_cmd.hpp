#pragma once

/**
 * @file rdweb_cmd.hpp
 * @brief Canonical Win32 rdwebadmin.exe / rdweb.exe CLI Management Utilities for RD Web Access.
 *
 * Clean-room implementation of Remote Desktop Web Access (RD Web Access)
 * CLI management tools under 17 U.S.C. § 1201(f) & Google LLC v. Oracle America, Inc.
 * (141 S. Ct. 1183, 2021).
 *
 * Provides complete CLI management for:
 *   - rdwebadmin status / rdweb status: Introspect RD Web Access health and feed metrics
 *   - rdwebadmin feed: Generate and display MS-TSWAP XML workspace provisioning feed
 *   - rdwebadmin list: Tabular inventory of published RemoteApps and desktop pools
 *   - rdwebadmin publish <id> <name> <cmdPath> <coll>: Publish new RemoteApp
 *   - rdwebadmin unpublish <id>: Remove published RemoteApp
 *   - rdwebadmin generate-rdp <id> [host]: Synthesize cryptographically signed .rdp file
 *   - rdwebadmin dump: Export declarative PowerShell RDS configuration script
 *   - rdwebadmin help: Command usage and syntax
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

#include "micant/um/servers/rdwebaccess.hpp"

namespace micant::rdweb_cmd::um {

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

inline std::string ResourceTypeToString(rdweb::ResourceType type) {
    return (type == rdweb::ResourceTypeRemoteApp) ? "RemoteApp" : "Desktop";
}

class RdWebCommandInterpreter {
public:
    static RdWebCommandInterpreter& get() {
        static RdWebCommandInterpreter instance;
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
        auto& mgr = rdweb::RdWebAccessManager::get();

        if (subCmd == "status") {
            ss << "Remote Desktop Web Access Subsystem Status:\n"
               << "===============================================================================\n"
               << "Service Name:              RDWebAccess (Remote Desktop Web Access)\n"
               << "Status:                    RUNNING\n"
               << "Web Feed Endpoint:         https://localhost/RDWeb/Feed/webfeed.aspx\n"
               << "HTML5 Client Endpoint:     https://localhost/RDWeb/webclient/index.html\n"
               << "Published Resources:       " << mgr.getPublishedResourceCount() << " items\n"
               << "Feed Requests Processed:   " << mgr.getTotalFeedRequests() << "\n"
               << "HTML5 Client Launches:     " << mgr.getTotalWebClientLaunches() << "\n"
               << "Signed .rdp Files Issued:  " << mgr.getTotalSignedRdpFilesIssued() << "\n"
               << "Feed Protocol:             MS-TSWAP v2.1 (Schema 2010/05/tsf)\n"
               << "===============================================================================\n";
        }
        else if (subCmd == "feed" || subCmd == "workspace") {
            ss << mgr.generateMsTswapWorkspaceFeed("Administrator");
        }
        else if (subCmd == "list" || subCmd == "apps") {
            auto resources = mgr.getAllResources();
            ss << "Published Remote Desktop Workspace Resources\n"
               << "------------------------------------------------------------------------------------------------------------------------\n"
               << std::left << std::setw(16) << "RESOURCE ID"
               << std::setw(24) << "DISPLAY NAME"
               << std::setw(14) << "TYPE"
               << std::setw(24) << "COLLECTION"
               << std::setw(10) << "LAUNCHES"
               << "COMMAND PATH\n"
               << "------------------------------------------------------------------------------------------------------------------------\n";
            for (const auto& r : resources) {
                ss << std::left << std::setw(16) << r.resourceId
                   << std::setw(24) << r.displayName
                   << std::setw(14) << ResourceTypeToString(r.type)
                   << std::setw(24) << r.collectionName
                   << std::setw(10) << r.launchCount
                   << r.commandPath << "\n";
            }
            ss << "------------------------------------------------------------------------------------------------------------------------\n"
               << "Total Published Resources: " << resources.size() << "\n";
        }
        else if (subCmd == "publish") {
            if (args.size() < 5) {
                ss << "[-] Error: Missing arguments.\n"
                   << "    Usage: rdwebadmin publish <id> <name> <cmdPath> <collection> [/alias:<alias>]\n";
            } else {
                rdweb::PublishedResource res{};
                res.resourceId = std::string(args[1]);
                res.displayName = std::string(args[2]);
                res.commandPath = std::string(args[3]);
                res.collectionName = std::string(args[4]);
                res.alias = res.resourceId;
                res.type = rdweb::ResourceTypeRemoteApp;
                res.allowedUserGroups = {"Domain Users"};
                res.isVisibleInFeed = true;

                for (size_t i = 5; i < args.size(); ++i) {
                    if (args[i].starts_with("/alias:")) {
                        res.alias = std::string(args[i].substr(7));
                    }
                }

                if (mgr.publishResource(res)) {
                    ss << "[+] Successfully published resource: " << res.displayName << " (" << res.resourceId << ")\n";
                } else {
                    ss << "[-] Failed to publish resource.\n";
                }
            }
        }
        else if (subCmd == "unpublish") {
            if (args.size() < 2) {
                ss << "[-] Error: Missing resource ID.\n"
                   << "    Usage: rdwebadmin unpublish <id>\n";
            } else {
                std::string id = std::string(args[1]);
                if (mgr.unpublishResource(id)) {
                    ss << "[+] Successfully unpublished resource: " << id << "\n";
                } else {
                    ss << "[-] Failed to unpublish resource: resource ID not found.\n";
                }
            }
        }
        else if (subCmd == "generate-rdp" || subCmd == "rdp") {
            if (args.size() < 2) {
                ss << "[-] Error: Missing resource ID.\n"
                   << "    Usage: rdwebadmin generate-rdp <id> [host]\n";
            } else {
                std::string id = std::string(args[1]);
                std::string host = (args.size() > 2) ? std::string(args[2]) : "rdsh01.corp.micant.internal";
                ss << mgr.generateSignedRdpFile(id, host);
            }
        }
        else if (subCmd == "dump") {
            ss << "# -----------------------------------------------------------------------------\n"
               << "# Remote Desktop Web Access Export Configuration\n"
               << "# Generated by MicaNT rdwebadmin.exe (MS-TSWAP Parity Engine)\n"
               << "# Copyright (C) 2026 MicaNT Authors.\n"
               << "# -----------------------------------------------------------------------------\n\n"
               << "Import-Module RemoteDesktop\n\n";

            auto resList = mgr.getAllResources();
            for (const auto& r : resList) {
                if (r.type == rdweb::ResourceTypeRemoteApp) {
                    ss << "New-RDRemoteApp -CollectionName \"" << r.collectionName << "\" "
                       << "-DisplayName \"" << r.displayName << "\" "
                       << "-FilePath \"" << r.commandPath << "\" "
                       << "-Alias \"" << r.alias << "\"\n";
                }
            }
            ss << "\n# End of RD Web Access Configuration Export\n";
        }
        else {
            ss << "[-] Unknown rdwebadmin command: " << subCmd << "\n"
               << "    Type 'rdwebadmin help' for a list of available commands.\n";
        }

        outResult = ss.str();
    }

private:
    RdWebCommandInterpreter() = default;

    void printHelp(std::ostream& os) const {
        os << "MicaNT Remote Desktop Web Access Administration Tool (rdwebadmin / rdweb)\n"
           << "Copyright (C) 2026 MicaNT Authors.\n"
           << "Provides sovereign administration of MS-TSWAP workspace feeds and RemoteApps.\n\n"
           << "Usage:\n"
           << "  rdwebadmin status                         Display Web Access service vitals and metrics\n"
           << "  rdwebadmin feed                           Generate MS-TSWAP XML workspace feed\n"
           << "  rdwebadmin list                           List all published RemoteApps and desktop pools\n"
           << "  rdwebadmin publish <id> <name> <cmd> <col> Publish new RemoteApp to collection\n"
           << "  rdwebadmin unpublish <id>                 Remove published resource\n"
           << "  rdwebadmin generate-rdp <id> [host]       Generate cryptographically signed .rdp file\n"
           << "  rdwebadmin dump                           Export declarative PowerShell RDS script\n"
           << "  rdwebadmin help                           Display this help message\n";
    }
};

inline void ExecuteRdwebString(std::string_view commandLine, std::string& outResult) {
    auto tokens = Tokenize(commandLine);
    if (tokens.empty()) {
        outResult = "[-] No command specified.\n";
        return;
    }

    std::span<const std::string_view> args(tokens.data() + 1, tokens.size() - 1);
    RdWebCommandInterpreter::get().execute(args, outResult);
}

} // namespace micant::rdweb_cmd::um
