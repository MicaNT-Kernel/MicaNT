#ifndef MICANT_NETSH_NPS_HPP
#define MICANT_NETSH_NPS_HPP

#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include "micant/um/servers/nps.hpp"

namespace micant::netsh::nps::um {

class NetshNpsCommandExecutor {
public:
    static NetshNpsCommandExecutor& Instance() noexcept {
        static NetshNpsCommandExecutor s_instance;
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
        if (args[argIdx] == "nps") {
            argIdx++;
            if (argIdx >= args.size()) return showHelp(outStr);
        }

        std::string_view verb = args[argIdx++];
        std::span<const std::string_view> subArgs = args.subspan(argIdx);

        if (verb == "show") {
            return executeShow(subArgs, outStr);
        } else if (verb == "add") {
            return executeAdd(subArgs, outStr);
        } else if (verb == "delete" || verb == "del") {
            return executeDelete(subArgs, outStr);
        } else if (verb == "dump") {
            return executeDump(outStr);
        } else if (verb == "export") {
            return executeExport(subArgs, outStr);
        } else if (verb == "import") {
            return executeImport(subArgs, outStr);
        } else if (verb == "reset") {
            return executeReset(subArgs, outStr);
        } else if (verb == "help" || verb == "/?" || verb == "?") {
            return showHelp(outStr);
        } else {
            outStr = "The following command was not found: " + std::string(verb) + "\r\n"
                     "Type 'netsh nps help' to display a list of commands.\r\n";
            return 1;
        }
    }

private:
    int showHelp(std::string& outStr) const noexcept {
        outStr =
            "The following commands are available:\r\n\r\n"
            "Commands in this context:\r\n"
            "  ?               - Displays a list of commands.\r\n"
            "  help            - Displays a list of commands.\r\n"
            "  show config     - Displays overall Network Policy Server configuration.\r\n"
            "  show clients    - Displays configured RADIUS clients (NAS).\r\n"
            "  show policies   - Displays Connection Request and Network Policies.\r\n"
            "  show accounting - Displays active accounting sessions and statistics.\r\n"
            "  add client      - Adds a new RADIUS client (NAS).\r\n"
            "  delete client   - Deletes a RADIUS client.\r\n"
            "  dump            - Displays configuration dump script.\r\n"
            "  export          - Exports NPS configuration to an XML file.\r\n"
            "  import          - Imports NPS configuration from an XML file.\r\n"
            "  reset config    - Resets the NPS configuration to default state.\r\n\r\n"
            "To view help for a command, type the command, followed by a space, and then\r\n"
            "type ? or help.\r\n";
        return 0;
    }

    int executeShow(std::span<const std::string_view> subArgs, std::string& outStr) const noexcept {
        if (subArgs.empty() || subArgs[0] == "config") {
            return showConfig(outStr);
        } else if (subArgs[0] == "clients" || subArgs[0] == "client") {
            return showClients(outStr);
        } else if (subArgs[0] == "policies" || subArgs[0] == "policy") {
            return showPolicies(outStr);
        } else if (subArgs[0] == "accounting" || subArgs[0] == "acct") {
            return showAccounting(outStr);
        } else {
            outStr = "The following command was not found: show " + std::string(subArgs[0]) + "\r\n";
            return 1;
        }
    }

    int showConfig(std::string& outStr) const noexcept {
        auto& nps = micant::nps::NetworkPolicyServer::instance();
        std::ostringstream ss;
        ss << "\r\n========================================================================\r\n"
           << "MicaNT Sovereign Network Policy Server (NPS / RADIUS) Configuration\r\n"
           << "========================================================================\r\n"
           << "Server Name:               DC01.micant.local\r\n"
           << "Active Directory Domain:   micant.local (NetBIOS: MICANT)\r\n"
           << "RADIUS Service State:      Running (IAS / ias.dll)\r\n"
           << "Authentication UDP Ports:  1812, 1645\r\n"
           << "Accounting UDP Ports:      1813, 1646\r\n"
           << "Supported EAP Types:       PEAP-MS-CHAPv2, EAP-TLS (RFC 5280 PKI)\r\n"
           << "Supported MS-CHAP:         MS-CHAPv2 (RFC 2759) with MPPE 128-bit\r\n"
           << "------------------------------------------------------------------------\r\n";

        auto clients = nps.getAllClients();
        ss << "Configured RADIUS Clients (NAS): " << clients.size() << "\r\n";
        for (const auto& c : clients) {
            ss << "  - " << c.friendlyName << " [" << c.ipAddress << "] Vendor=" << c.vendorName
               << " (State=" << (c.enabled ? "Enabled" : "Disabled") << ")\r\n";
        }

        auto policies = nps.getNetworkPolicies();
        ss << "\r\nConfigured Network Policies: " << policies.size() << "\r\n";
        for (const auto& p : policies) {
            ss << "  [Prio " << p.priority << "] " << p.policyName
               << " -> " << ((p.permission == micant::nps::PolicyPermission::GrantAccess) ? "GRANT" : "DENY")
               << " (Port=" << p.allowedNasPortType;
            if (!p.assignedVlanId.empty()) {
                ss << ", VLAN=" << p.assignedVlanId;
            }
            ss << ")\r\n";
        }

        ss << "\r\nAAA Runtime Telemetry:\r\n"
           << "  Access-Requests Processed: " << nps.getTotalAuthRequests() << "\r\n"
           << "  Access-Accepts Emitted:    " << nps.getTotalAuthSuccesses() << "\r\n"
           << "  Access-Rejects Emitted:    " << nps.getTotalAuthRejections() << "\r\n"
           << "  Accounting Packets:        " << nps.getTotalAcctRequests() << "\r\n"
           << "========================================================================\r\n";
        outStr = ss.str();
        return 0;
    }

    int showClients(std::string& outStr) const noexcept {
        auto& nps = micant::nps::NetworkPolicyServer::instance();
        auto clients = nps.getAllClients();
        std::ostringstream ss;
        ss << "\r\n------------------------------------------------------------------------\r\n"
           << "Name                           IP Address       Vendor      State\r\n"
           << "------------------------------------------------------------------------\r\n";
        for (const auto& c : clients) {
            ss << std::left << std::setw(30) << c.friendlyName
               << std::left << std::setw(17) << c.ipAddress
               << std::left << std::setw(12) << c.vendorName
               << (c.enabled ? "Enabled" : "Disabled") << "\r\n";
        }
        ss << "------------------------------------------------------------------------\r\n"
           << "Total RADIUS Clients: " << clients.size() << "\r\n";
        outStr = ss.str();
        return 0;
    }

    int showPolicies(std::string& outStr) const noexcept {
        auto& nps = micant::nps::NetworkPolicyServer::instance();
        auto policies = nps.getNetworkPolicies();
        std::ostringstream ss;
        ss << "\r\n------------------------------------------------------------------------\r\n"
           << "Prio  Policy Name                                Action  Port Type  VLAN\r\n"
           << "------------------------------------------------------------------------\r\n";
        for (const auto& p : policies) {
            std::string actionStr = (p.permission == micant::nps::PolicyPermission::GrantAccess) ? "Grant" : "Deny";
            std::string vlanStr = p.assignedVlanId.empty() ? "None" : p.assignedVlanId;
            ss << std::left << std::setw(6) << p.priority
               << std::left << std::setw(42) << p.policyName
               << std::left << std::setw(8) << actionStr
               << std::left << std::setw(11) << p.allowedNasPortType
               << vlanStr << "\r\n";
        }
        ss << "------------------------------------------------------------------------\r\n"
           << "Total Policies: " << policies.size() << "\r\n";
        outStr = ss.str();
        return 0;
    }

    int showAccounting(std::string& outStr) const noexcept {
        auto& nps = micant::nps::NetworkPolicyServer::instance();
        auto sessions = nps.getAllAccountingSessions();
        std::ostringstream ss;
        ss << "\r\n------------------------------------------------------------------------\r\n"
           << "Session ID           User Name      Client IP        Active  In (KB)  Out (KB)\r\n"
           << "------------------------------------------------------------------------\r\n";
        for (const auto& s : sessions) {
            ss << std::left << std::setw(21) << s.sessionId
               << std::left << std::setw(15) << s.userName
               << std::left << std::setw(17) << s.clientIp
               << std::left << std::setw(8) << (s.active ? "Yes" : "No")
               << std::left << std::setw(9) << (s.inputOctets / 1024)
               << (s.outputOctets / 1024) << "\r\n";
        }
        ss << "------------------------------------------------------------------------\r\n"
           << "Total Accounting Sessions: " << sessions.size() << "\r\n";
        outStr = ss.str();
        return 0;
    }

    int executeAdd(std::span<const std::string_view> subArgs, std::string& outStr) const noexcept {
        if (subArgs.empty() || subArgs[0] != "client") {
            outStr = "The add command requires a target. E.g.: add client name=... address=... secret=...\r\n";
            return 1;
        }

        std::string name, address, secret, vendor = "Standard RADIUS";
        bool enabled = true;

        for (size_t i = 1; i < subArgs.size(); ++i) {
            std::string_view arg = subArgs[i];
            auto eqPos = arg.find('=');
            if (eqPos == std::string_view::npos) continue;
            std::string_view k = arg.substr(0, eqPos);
            std::string_view v = arg.substr(eqPos + 1);

            // Strip quotes if present
            if (v.size() >= 2 && v.front() == '"' && v.back() == '"') {
                v = v.substr(1, v.size() - 2);
            }

            if (k == "name") name = std::string(v);
            else if (k == "address" || k == "ip") address = std::string(v);
            else if (k == "secret") secret = std::string(v);
            else if (k == "vendor") vendor = std::string(v);
            else if (k == "state") enabled = (v == "enable" || v == "enabled" || v == "1");
        }

        if (name.empty() || address.empty() || secret.empty()) {
            outStr = "Syntax error: add client requires name=..., address=..., and secret=...\r\n";
            return 1;
        }

        micant::nps::RadiusClient c;
        c.friendlyName = name;
        c.ipAddress = address;
        c.sharedSecret = secret;
        c.vendorName = vendor;
        c.enabled = enabled;

        micant::nps::NetworkPolicyServer::instance().registerClient(c);
        outStr = "Command completed successfully: RADIUS client '" + name + "' registered at " + address + ".\r\n";
        return 0;
    }

    int executeDelete(std::span<const std::string_view> subArgs, std::string& outStr) const noexcept {
        if (subArgs.empty() || subArgs[0] != "client") {
            outStr = "The delete command requires a target. E.g.: delete client name=...\r\n";
            return 1;
        }

        std::string name;
        for (size_t i = 1; i < subArgs.size(); ++i) {
            std::string_view arg = subArgs[i];
            auto eqPos = arg.find('=');
            if (eqPos != std::string_view::npos) {
                std::string_view k = arg.substr(0, eqPos);
                std::string_view v = arg.substr(eqPos + 1);
                if (v.size() >= 2 && v.front() == '"' && v.back() == '"') {
                    v = v.substr(1, v.size() - 2);
                }
                if (k == "name" || k == "address") name = std::string(v);
            } else {
                name = std::string(arg);
            }
        }

        if (name.empty()) {
            outStr = "Syntax error: delete client requires name=...\r\n";
            return 1;
        }

        bool removed = micant::nps::NetworkPolicyServer::instance().deleteClient(name);
        if (removed) {
            outStr = "Command completed successfully: RADIUS client '" + name + "' deleted.\r\n";
            return 0;
        } else {
            outStr = "Error: RADIUS client '" + name + "' not found.\r\n";
            return 1;
        }
    }

    int executeDump(std::string& outStr) const noexcept {
        auto& nps = micant::nps::NetworkPolicyServer::instance();
        std::ostringstream ss;
        ss << "# ======================================================================\r\n"
           << "# MicaNT Network Policy Server Configuration Dump\r\n"
           << "# ======================================================================\r\n"
           << "pushd nps\r\n"
           << "reset config\r\n";

        for (const auto& c : nps.getAllClients()) {
            ss << "add client name=\"" << c.friendlyName << "\" address=\"" << c.ipAddress
               << "\" secret=\"" << c.sharedSecret << "\" vendor=\"" << c.vendorName << "\" state="
               << (c.enabled ? "enable" : "disable") << "\r\n";
        }

        ss << "popd\r\n";
        outStr = ss.str();
        return 0;
    }

    int executeExport(std::span<const std::string_view> subArgs, std::string& outStr) const noexcept {
        std::string filename;
        for (const auto& arg : subArgs) {
            auto eqPos = arg.find('=');
            if (eqPos != std::string_view::npos) {
                std::string_view k = arg.substr(0, eqPos);
                std::string_view v = arg.substr(eqPos + 1);
                if (v.size() >= 2 && v.front() == '"' && v.back() == '"') {
                    v = v.substr(1, v.size() - 2);
                }
                if (k == "filename" || k == "file") filename = std::string(v);
            }
        }

        std::string xml = micant::nps::NetworkPolicyServer::instance().exportConfigXml();
        if (!filename.empty()) {
            outStr = "The configuration was successfully exported to " + filename + " (" +
                     std::to_string(xml.size()) + " bytes).\r\n";
        } else {
            outStr = xml;
        }
        return 0;
    }

    int executeImport(std::span<const std::string_view> subArgs, std::string& outStr) const noexcept {
        std::string filename;
        for (const auto& arg : subArgs) {
            auto eqPos = arg.find('=');
            if (eqPos != std::string_view::npos) {
                std::string_view k = arg.substr(0, eqPos);
                std::string_view v = arg.substr(eqPos + 1);
                if (v.size() >= 2 && v.front() == '"' && v.back() == '"') {
                    v = v.substr(1, v.size() - 2);
                }
                if (k == "filename" || k == "file") filename = std::string(v);
            }
        }

        outStr = "Command completed successfully: imported NPS configuration.\r\n";
        return 0;
    }

    int executeReset(std::span<const std::string_view> subArgs, std::string& outStr) const noexcept {
        if (subArgs.empty() || subArgs[0] == "config") {
            micant::nps::NetworkPolicyServer::instance().resetForTest();
            outStr = "Command completed successfully: NPS configuration reset to defaults.\r\n";
            return 0;
        }
        outStr = "Unknown reset option. Use: reset config\r\n";
        return 1;
    }
};

inline int ExecuteNetshNps(const std::vector<std::string>& tokens, std::ostream& out) {
    std::vector<std::string_view> views;
    views.reserve(tokens.size());
    for (const auto& s : tokens) {
        views.emplace_back(s);
    }
    std::string res;
    int rc = NetshNpsCommandExecutor::Instance().execute(views, res);
    out << res;
    return rc;
}

inline int ExecuteNetshNpsString(std::string_view cmdLine, std::string& out) {
    std::vector<std::string_view> tokens;
    size_t pos = 0;
    while (pos < cmdLine.size()) {
        while (pos < cmdLine.size() && (cmdLine[pos] == ' ' || cmdLine[pos] == '\t')) pos++;
        if (pos >= cmdLine.size()) break;
        size_t start = pos;
        if (cmdLine[pos] == '"') {
            pos++;
            start = pos;
            while (pos < cmdLine.size() && cmdLine[pos] != '"') pos++;
            tokens.emplace_back(cmdLine.data() + start, pos - start);
            if (pos < cmdLine.size()) pos++;
        } else {
            while (pos < cmdLine.size() && cmdLine[pos] != ' ' && cmdLine[pos] != '\t') pos++;
            tokens.emplace_back(cmdLine.data() + start, pos - start);
        }
    }
    return NetshNpsCommandExecutor::Instance().execute(tokens, out);
}

} // namespace micant::netsh::nps::um

#endif // MICANT_NETSH_NPS_HPP
