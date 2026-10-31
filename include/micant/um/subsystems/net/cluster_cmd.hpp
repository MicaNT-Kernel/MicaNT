#pragma once

/**
 * @file cluster_cmd.hpp
 * @brief Canonical Win32 cluster.exe Command-Line Management Utility.
 *
 * Clean-room implementation of the Microsoft Windows Server Failover Clustering
 * administrative utility (cluster.exe) under 17 U.S.C. § 1201(f) & Google LLC v. Oracle America, Inc.
 *
 * Provides complete CLI management for:
 *   - cluster node: node membership, state inspection, pause/drain, and resume
 *   - cluster group: resource groups / roles, owner nodes, live migration / failover
 *   - cluster res: resource dependency hierarchies, online / offline controls
 *   - cluster quorum: witness configuration (Node Majority, Disk Witness, FSW, Cloud Witness) & dynamic quorum
 *   - cluster csv: Cluster Shared Volume redirect state (Direct I/O vs. Network Redirected) & I/O statistics
 *   - cluster log /gen: diagnostic report generation (Cluster.log)
 *   - cluster status: cluster vitals, Paxos consensus epoch, and heartbeat telemetry
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

#include "micant/um/servers/clustering.hpp"
#include "micant/km/drivers/fs/csvfs.hpp"

namespace micant::cluster_cmd::um {

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

inline void ExecuteClusterString(std::string_view cmdLine, std::string& out) {
    auto tokens = Tokenize(cmdLine);
    if (!tokens.empty() && (tokens[0] == "cluster" || tokens[0] == "cluster.exe")) {
        tokens.erase(tokens.begin());
    }

    if (tokens.empty() || tokens[0] == "/help" || tokens[0] == "-help" || tokens[0] == "/?" || tokens[0] == "help") {
        std::ostringstream ss;
        ss << "MicaNT Cluster Administrative Utility (cluster.exe 10.0.26100.1)\n"
           << "Usage: cluster [options] <object> <command> [parameters]\n\n"
           << "Objects:\n"
           << "  node                             Manage cluster nodes (status, pause, resume)\n"
           << "  group                            Manage resource groups / roles (status, moveto)\n"
           << "  res                              Manage cluster resources (status, online, offline)\n"
           << "  quorum                           Inspect and configure cluster quorum and witness\n"
           << "  csv                              Inspect and manage Cluster Shared Volumes (CSVFS)\n"
           << "  log /gen                         Generate cluster diagnostic log (Cluster.log)\n"
           << "  status                           Display cluster health vitals and Paxos consensus state\n";
        out = ss.str();
        return;
    }

    auto& engine = micant::cluster::FailoverClusterSubsystem::get();
    engine.initialize();

    std::string obj = std::string(tokens[0]);
    std::transform(obj.begin(), obj.end(), obj.begin(), ::tolower);

    // ------------------------------------------------------------------------
    // Subcommand: cluster node
    // ------------------------------------------------------------------------
    if (obj == "node") {
        if (tokens.size() == 1 || (tokens.size() == 2 && (tokens[1] == "/status" || tokens[1] == "status"))) {
            auto nodes = engine.getAllNodes();
            // Sort by Node ID for clean presentation
            std::sort(nodes.begin(), nodes.end(), [](const auto& a, const auto& b) { return a.nodeId < b.nodeId; });

            std::ostringstream ss;
            ss << "Cluster Nodes:\n"
               << "  Node ID  Node Name        IP Address       State    Weight  Vote  Role\n"
               << "  --------------------------------------------------------------------------------\n";
            for (const auto& n : nodes) {
                ss << "  " << std::left << std::setw(9) << n.nodeId
                   << std::setw(17) << n.nodeName
                   << std::setw(17) << n.ipAddress
                   << std::setw(9) << micant::cluster::ClusterNodeStateToString(n.state)
                   << std::setw(8) << n.voteWeight
                   << std::setw(6) << n.currentVote
                   << (n.isCoordinator ? "Coordinator" : "Member") << "\n";
            }
            out = ss.str();
            return;
        }

        std::string targetNode(tokens[1]);
        auto* pNode = engine.findNodeByNameOrId(targetNode);
        if (!pNode) {
            out = "[-] Node '" + targetNode + "' not found in cluster configuration.\n";
            return;
        }

        for (size_t i = 2; i < tokens.size(); ++i) {
            std::string action(tokens[i]);
            std::transform(action.begin(), action.end(), action.begin(), ::tolower);

            if (action == "/pause" || action == "pause") {
                bool ok = engine.pauseNode(pNode->nodeId);
                if (ok) {
                    out = "[+] Node " + pNode->nodeName + " (ID: " + std::to_string(pNode->nodeId) +
                          ") successfully PAUSED. Hosted resource groups drained to surviving nodes.\n";
                } else {
                    out = "[-] Failed to pause node " + pNode->nodeName + ".\n";
                }
                return;
            } else if (action == "/resume" || action == "resume") {
                bool ok = engine.resumeNode(pNode->nodeId);
                if (ok) {
                    out = "[+] Node " + pNode->nodeName + " (ID: " + std::to_string(pNode->nodeId) +
                          ") successfully RESUMED to active cluster membership.\n";
                } else {
                    out = "[-] Failed to resume node " + pNode->nodeName + ".\n";
                }
                return;
            }
        }

        out = "[-] Invalid command for cluster node. Use /pause or /resume.\n";
        return;
    }

    // ------------------------------------------------------------------------
    // Subcommand: cluster group
    // ------------------------------------------------------------------------
    if (obj == "group") {
        if (tokens.size() == 1 || (tokens.size() == 2 && (tokens[1] == "/status" || tokens[1] == "status"))) {
            auto groups = engine.getAllGroups();
            std::ostringstream ss;
            ss << "Cluster Resource Groups:\n"
               << "  Group ID         State     Owner Node        Preferred Node    Group Name\n"
               << "  --------------------------------------------------------------------------------\n";
            for (const auto& g : groups) {
                ss << "  " << std::left << std::setw(17) << g.groupId
                   << std::setw(10) << micant::cluster::ClusterGroupStateToString(g.state)
                   << std::setw(18) << g.ownerNodeId
                   << std::setw(18) << g.preferredNodeId
                   << g.groupName << "\n";
            }
            out = ss.str();
            return;
        }

        std::string targetGroup(tokens[1]);
        auto* pGrp = engine.findGroupByNameOrId(targetGroup);
        if (!pGrp) {
            out = "[-] Resource group '" + targetGroup + "' not found in cluster configuration.\n";
            return;
        }

        for (size_t i = 2; i < tokens.size(); ++i) {
            std::string action(tokens[i]);
            std::string actionLower = action;
            std::transform(actionLower.begin(), actionLower.end(), actionLower.begin(), ::tolower);

            if (actionLower.starts_with("/moveto:") || actionLower.starts_with("-moveto:")) {
                std::string destStr = action.substr(8);
                auto* pDestNode = engine.findNodeByNameOrId(destStr);
                if (!pDestNode) {
                    out = "[-] Target node '" + destStr + "' for group migration does not exist.\n";
                    return;
                }

                bool ok = engine.failoverGroup(pGrp->groupId, pDestNode->nodeId);
                if (ok) {
                    out = "[+] Resource group '" + pGrp->groupId + "' successfully migrated to Node " +
                          std::to_string(pDestNode->nodeId) + " (" + pDestNode->nodeName + "). State: Online.\n";
                } else {
                    out = "[-] Failed to migrate resource group '" + pGrp->groupId + "' to Node " + destStr + ".\n";
                }
                return;
            }
        }

        out = "[-] Invalid command for cluster group. Use /moveto:<node>.\n";
        return;
    }

    // ------------------------------------------------------------------------
    // Subcommand: cluster res
    // ------------------------------------------------------------------------
    if (obj == "res") {
        if (tokens.size() == 1 || (tokens.size() == 2 && (tokens[1] == "/status" || tokens[1] == "status"))) {
            auto resources = engine.getAllResources();
            std::ostringstream ss;
            ss << "Cluster Resources:\n"
               << "  Resource ID      State     Type              Owner Group       Owner Node  Dependencies\n"
               << "  --------------------------------------------------------------------------------------------\n";
            for (const auto& r : resources) {
                std::string depStr;
                if (r.dependencies.empty()) {
                    depStr = "None";
                } else {
                    for (size_t d = 0; d < r.dependencies.size(); ++d) {
                        if (d > 0) depStr += ", ";
                        depStr += r.dependencies[d];
                    }
                }
                ss << "  " << std::left << std::setw(17) << r.resourceId
                   << std::setw(10) << micant::cluster::ClusterResourceStateToString(r.state)
                   << std::setw(18) << r.resourceType
                   << std::setw(18) << r.ownerGroup
                   << std::setw(12) << r.ownerNodeId
                   << depStr << "\n";
            }
            out = ss.str();
            return;
        }

        std::string targetRes(tokens[1]);
        auto* pRes = engine.findResourceByNameOrId(targetRes);
        if (!pRes) {
            out = "[-] Resource '" + targetRes + "' not found in cluster configuration.\n";
            return;
        }

        for (size_t i = 2; i < tokens.size(); ++i) {
            std::string action(tokens[i]);
            std::transform(action.begin(), action.end(), action.begin(), ::tolower);

            if (action == "/online" || action == "online") {
                bool ok = engine.setResourceOnline(pRes->resourceId);
                if (ok) {
                    out = "[+] Resource '" + pRes->resourceId + "' is now ONLINE.\n";
                } else {
                    out = "[-] Failed to bring resource '" + pRes->resourceId + "' online: dependency unsatisfied or resource unavailable.\n";
                }
                return;
            } else if (action == "/offline" || action == "offline") {
                bool ok = engine.setResourceOffline(pRes->resourceId);
                if (ok) {
                    out = "[+] Resource '" + pRes->resourceId + "' is now OFFLINE.\n";
                } else {
                    out = "[-] Failed to take resource '" + pRes->resourceId + "' offline.\n";
                }
                return;
            }
        }

        out = "[-] Invalid command for cluster res. Use /online or /offline.\n";
        return;
    }

    // ------------------------------------------------------------------------
    // Subcommand: cluster quorum
    // ------------------------------------------------------------------------
    if (obj == "quorum") {
        if (tokens.size() == 1 || (tokens.size() == 2 && (tokens[1] == "/status" || tokens[1] == "status"))) {
            std::ostringstream ss;
            ss << "Cluster Quorum Configuration:\n"
               << "  Quorum Model:       " << micant::cluster::QuorumWitnessTypeToString(engine.getWitnessType()) << "\n"
               << "  Witness Path:       " << (engine.getWitnessPath().empty() ? "(none)" : engine.getWitnessPath()) << "\n"
               << "  Dynamic Quorum:     " << (engine.isDynamicQuorumEnabled() ? "ENABLED (Last-Man Standing Resilience)" : "DISABLED (Static Majority)") << "\n"
               << "  Active Votes:       " << engine.getQuorumVotesActive() << "\n"
               << "  Total Votes:        " << engine.getQuorumVotesTotal() << "\n"
               << "  Quorum Majority:    " << (engine.getQuorumVotesTotal() / 2 + 1) << " votes required\n"
               << "  Quorum Status:      " << (engine.hasQuorum() ? "ACHIEVED (Cluster operational)" : "LOST (Split-brain fenced)") << "\n";
            out = ss.str();
            return;
        }

        micant::cluster::QuorumWitnessType newType = engine.getWitnessType();
        std::string newPath = engine.getWitnessPath();
        bool dynamicQuorum = engine.isDynamicQuorumEnabled();
        bool updated = false;

        for (size_t i = 1; i < tokens.size(); ++i) {
            std::string arg(tokens[i]);
            std::string argLower = arg;
            std::transform(argLower.begin(), argLower.end(), argLower.begin(), ::tolower);

            if (argLower.starts_with("/set:") || argLower.starts_with("-set:")) {
                std::string model = argLower.substr(5);
                if (model == "nodemajority" || model == "none") {
                    newType = micant::cluster::QuorumWitnessType::None;
                    newPath = "";
                    updated = true;
                } else if (model == "diskwitness" || model == "disk") {
                    newType = micant::cluster::QuorumWitnessType::DiskWitness;
                    newPath = "ClusterDisk1 (Witness LUN 0)";
                    updated = true;
                } else if (model == "filesharewitness" || model == "fileshare" || model == "fsw") {
                    newType = micant::cluster::QuorumWitnessType::FileShareWitness;
                    if (newPath.empty() || newPath == "ClusterDisk1 (Witness LUN 0)") {
                        newPath = "\\\\fs01\\clusterwitness";
                    }
                    updated = true;
                } else if (model == "cloudwitness" || model == "cloud") {
                    newType = micant::cluster::QuorumWitnessType::CloudWitness;
                    newPath = "https://titanclusstorage.blob.core.windows.net/witness";
                    updated = true;
                }
            } else if (argLower.starts_with("/path:") || argLower.starts_with("-path:")) {
                newPath = arg.substr(6);
                updated = true;
            } else if (argLower == "/dynamic:on" || argLower == "-dynamic:on") {
                dynamicQuorum = true;
                updated = true;
            } else if (argLower == "/dynamic:off" || argLower == "-dynamic:off") {
                dynamicQuorum = false;
                updated = true;
            }
        }

        if (updated) {
            engine.setQuorumWitness(newType, newPath, true);
            engine.setDynamicQuorum(dynamicQuorum);
            out = "[+] Cluster quorum configuration successfully updated.\n";
            return;
        }

        out = "[-] Invalid command for cluster quorum. Use /set:<nodemajority|diskwitness|filesharewitness|cloudwitness> [/path:<path>] [/dynamic:on|off].\n";
        return;
    }

    // ------------------------------------------------------------------------
    // Subcommand: cluster csv
    // ------------------------------------------------------------------------
    if (obj == "csv") {
        auto& csvSys = micant::csvfs::CsvfsSubsystem::get();
        csvSys.initialize();

        if (tokens.size() == 1 || (tokens.size() == 2 && (tokens[1] == "/status" || tokens[1] == "status"))) {
            auto vols = csvSys.getAllVolumes();
            std::ostringstream ss;
            ss << "Cluster Shared Volumes (CSVFS v2.0):\n"
               << "  Volume Path               State   Redirect State                  Coordinator  Direct I/O   Redirected\n"
               << "  ------------------------------------------------------------------------------------------------------\n";
            for (const auto& v : vols) {
                uint64_t directIo = v->getTotalDirectReads() + v->getTotalDirectWrites();
                ss << "  " << std::left << std::setw(26) << v->getVolumePath()
                   << std::setw(8) << micant::csvfs::CsvVolumeStateToString(v->getVolumeState())
                   << std::setw(32) << micant::csvfs::CsvRedirectStateToString(v->getRedirectState())
                   << std::setw(13) << v->getCoordinatorNodeId()
                   << std::setw(13) << directIo
                   << v->getTotalRedirectedOps() << "\n";
            }
            out = ss.str();
            return;
        }

        std::string volPath(tokens[1]);
        auto vol = csvSys.getVolume(volPath);
        if (!vol) {
            out = "[-] Cluster Shared Volume '" + volPath + "' not found.\n";
            return;
        }

        for (size_t i = 2; i < tokens.size(); ++i) {
            std::string action(tokens[i]);
            std::string actionLower = action;
            std::transform(actionLower.begin(), actionLower.end(), actionLower.begin(), ::tolower);

            if (actionLower.starts_with("/redirect:") || actionLower.starts_with("-redirect:")) {
                std::string mode = actionLower.substr(10);
                micant::csvfs::CsvRedirectState rState = micant::csvfs::CsvRedirectState::DirectIo;
                if (mode == "direct" || mode == "directio") {
                    rState = micant::csvfs::CsvRedirectState::DirectIo;
                } else if (mode == "fileredirect" || mode == "file") {
                    rState = micant::csvfs::CsvRedirectState::FileRedirected;
                } else if (mode == "blockredirect" || mode == "block") {
                    rState = micant::csvfs::CsvRedirectState::BlockRedirected;
                } else if (mode == "userrequested" || mode == "maintenance") {
                    rState = micant::csvfs::CsvRedirectState::UserRequested;
                } else {
                    out = "[-] Unknown redirect mode '" + mode + "'. Valid: direct, fileredirect, blockredirect, userrequested.\n";
                    return;
                }

                vol->setRedirectState(rState);
                out = "[+] CSV volume '" + volPath + "' redirect state set to: " +
                      micant::csvfs::CsvRedirectStateToString(rState) + ".\n";
                return;
            }
        }

        out = "[-] Invalid command for cluster csv. Use /redirect:<mode>.\n";
        return;
    }

    // ------------------------------------------------------------------------
    // Subcommand: cluster log /gen
    // ------------------------------------------------------------------------
    if (obj == "log") {
        if (tokens.size() >= 2) {
            std::string action(tokens[1]);
            std::transform(action.begin(), action.end(), action.begin(), ::tolower);
            if (action == "/gen" || action == "gen") {
                std::string logReport = engine.generateClusterLog();
                std::ostringstream ss;
                ss << "[+] Generating cluster diagnostic report...\n"
                   << "[+] Cluster report written to C:\\Windows\\Cluster\\Reports\\Cluster.log\n\n"
                   << logReport;
                out = ss.str();
                return;
            }
        }
        out = "[-] Invalid command for cluster log. Use 'cluster log /gen'.\n";
        return;
    }

    // ------------------------------------------------------------------------
    // Subcommand: cluster status
    // ------------------------------------------------------------------------
    if (obj == "status") {
        uint32_t nodesUp = 0;
        for (const auto& n : engine.getAllNodes()) {
            if (n.state == micant::cluster::ClusterNodeState::Up) nodesUp++;
        }

        std::ostringstream ss;
        ss << "MicaNT Windows Server Failover Cluster Status (WSFC 10.0.26100.1):\n"
           << "  Cluster Name:          " << (engine.getPaxosValue("ClusterName").empty() ? "TitanSovereignCluster" : engine.getPaxosValue("ClusterName")) << "\n"
           << "  Consensus Epoch:       " << engine.getEpoch() << "\n"
           << "  Cluster Nodes:         " << nodesUp << " / " << engine.getAllNodes().size() << " Online\n"
           << "  Quorum Status:         " << (engine.hasQuorum() ? "ACHIEVED (Cluster Operational)" : "LOST (Split-Brain Fenced)") << "\n"
           << "  Quorum Model:          " << micant::cluster::QuorumWitnessTypeToString(engine.getWitnessType()) << " (" << engine.getWitnessPath() << ")\n"
           << "  Dynamic Quorum:        " << (engine.isDynamicQuorumEnabled() ? "ENABLED (Last-Man Standing Resilience)" : "DISABLED (Static Majority)") << "\n"
           << "  Active Votes:          " << engine.getQuorumVotesActive() << " / " << engine.getQuorumVotesTotal() << "\n"
           << "  Active Roles/Groups:   " << engine.getAllGroups().size() << " Configured\n"
           << "  Active Resources:      " << engine.getAllResources().size() << " Managed\n"
           << "  Heartbeats Sent:       " << engine.getHeartbeatCount() << "\n"
           << "  Failover Events:       " << engine.getGroupsMigratedCount() << "\n"
           << "  CSV Volumes:           " << micant::csvfs::CsvfsSubsystem::get().getAllVolumes().size() << " Active\n";
        out = ss.str();
        return;
    }

    out = "[-] Unknown cluster object or command: " + obj + ". Run 'cluster /help' for usage.\n";
}

} // namespace micant::cluster_cmd::um
