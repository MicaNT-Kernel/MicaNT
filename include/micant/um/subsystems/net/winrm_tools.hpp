#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <map>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include "micant/um/servers/winrm_server.hpp"
#include "micant/um/servers/wmi_provider.hpp"

namespace micant::winrm_cli {

// ============================================================================
// Canonical winrm.cmd CLI Utility
// ============================================================================

class WinrmCommandExecutor {
public:
    static bool execute(const std::vector<std::string>& tokens, std::string& stdoutStr, std::string& stderrStr) {
        if (tokens.empty()) {
            stdoutStr =
                "Windows Remote Management (WinRM 2.0 / MS-WSMV / MS-PSRP)\r\n"
                "Usage: winrm <command> [<arguments>]\r\n\r\n"
                "Commands:\r\n"
                "  quickconfig (qc)    Configure WinRM service and default listeners\r\n"
                "  get                 Retrieve management resource or configuration\r\n"
                "  set                 Modify management configuration\r\n"
                "  create              Create a new instance of a resource\r\n"
                "  delete              Delete an instance of a resource\r\n"
                "  enumerate (enum)    List instances of a resource or listeners\r\n"
                "  help                Display help information\r\n";
            return true;
        }

        std::string cmd = tokens[0];
        std::string cmdLower = cmd;
        for (auto& c : cmdLower) if (c >= 'A' && c <= 'Z') c += 32;

        auto& winrm = micant::winrm::EnterpriseWinRmServer::instance();
        winrm.initialize();

        if (cmdLower == "quickconfig" || cmdLower == "qc") {
            // Verify SCM service
            auto& scm = micant::scm::ServiceControlManager::get();
            auto svc = scm.getServiceRecord(L"WinRM");
            if (svc) {
                svc->status.dwCurrentState = micant::scm::SERVICE_RUNNING;
                svc->startType = micant::scm::SERVICE_AUTO_START;
            }

            // Ensure default listeners
            winrm.ensureDefaultListeners();

            std::stringstream ss;
            ss << "WinRM service is already running on this machine.\r\n"
               << "WinRM is already set up for remote management on this machine.\r\n"
               << "[+] Verified WinRM HTTP listener on Address=* Port=5985\r\n"
               << "[+] Verified WinRM HTTPS listener on Address=* Port=5986\r\n"
               << "[+] WFP Inbound Rule #16501 enabled for ports 5985 and 5986\r\n";
            stdoutStr = ss.str();
            return true;
        } else if (cmdLower == "get") {
            if (tokens.size() < 2) {
                stderrStr = "Error: ResourceURI parameter is required for 'get' command.\r\n";
                return false;
            }
            std::string uri = tokens[1];
            std::string uriLower = uri;
            for (auto& c : uriLower) if (c >= 'A' && c <= 'Z') c += 32;

            if (uriLower.find("config/service") != std::string::npos) {
                auto cfg = winrm.getServiceConfig();
                std::stringstream ss;
                ss << "Service\r\n"
                   << "    RootMinEnvelopeSizeKb = " << cfg.rootMinEnvelopeSizeKb << "\r\n"
                   << "    MaxEnvelopeSizeKb = " << cfg.maxEnvelopeSizeKb << "\r\n"
                   << "    MaxTimeoutms = " << cfg.maxTimeoutMs << "\r\n"
                   << "    MaxBatchItems = " << cfg.maxBatchItems << "\r\n"
                   << "    MaxProviderRequests = " << cfg.maxProviderRequests << "\r\n"
                   << "    AllowUnencrypted = " << (cfg.allowUnencrypted ? "true" : "false") << "\r\n"
                   << "    Auth\r\n"
                   << "        Basic = " << (cfg.authBasic ? "true" : "false") << "\r\n"
                   << "        Kerberos = " << (cfg.authKerberos ? "true" : "false") << "\r\n"
                   << "        Negotiate = " << (cfg.authNegotiate ? "true" : "false") << "\r\n"
                   << "        Certificate = " << (cfg.authCertificate ? "true" : "false") << "\r\n"
                   << "        CredSSP = " << (cfg.authCredSsp ? "true" : "false") << "\r\n";
                stdoutStr = ss.str();
                return true;
            } else if (uriLower.find("config") != std::string::npos) {
                auto cfg = winrm.getServiceConfig();
                auto quota = winrm.getQuotaConfig();
                std::stringstream ss;
                ss << "Config\r\n"
                   << "    MaxEnvelopeSizekb = " << cfg.maxEnvelopeSizeKb << "\r\n"
                   << "    MaxTimeoutms = " << cfg.maxTimeoutMs << "\r\n"
                   << "    MaxBatchItems = " << cfg.maxBatchItems << "\r\n"
                   << "    MaxProviderRequests = " << cfg.maxProviderRequests << "\r\n"
                   << "    Service\r\n"
                   << "        RootMinEnvelopeSizeKb = " << cfg.rootMinEnvelopeSizeKb << "\r\n"
                   << "        AllowUnencrypted = " << (cfg.allowUnencrypted ? "true" : "false") << "\r\n"
                   << "        Auth\r\n"
                   << "            Basic = " << (cfg.authBasic ? "true" : "false") << "\r\n"
                   << "            Kerberos = " << (cfg.authKerberos ? "true" : "false") << "\r\n"
                   << "            Negotiate = " << (cfg.authNegotiate ? "true" : "false") << "\r\n"
                   << "            Certificate = " << (cfg.authCertificate ? "true" : "false") << "\r\n"
                   << "            CredSSP = " << (cfg.authCredSsp ? "true" : "false") << "\r\n"
                   << "    Winrs\r\n"
                   << "        AllowRemoteShellAccess = true\r\n"
                   << "        IdleTimeout = " << quota.idleTimeoutMs << "\r\n"
                   << "        MaxConcurrentUsers = " << quota.maxConcurrentUsers << "\r\n"
                   << "        MaxProcessesPerShell = " << quota.maxProcessesPerShell << "\r\n"
                   << "        MaxMemoryPerShellMB = " << quota.maxMemoryPerShellMb << "\r\n"
                   << "        MaxShellsPerUser = " << quota.maxShellsPerUser << "\r\n";
                stdoutStr = ss.str();
                return true;
            } else if (uriLower.find("wmicimv2/") != std::string::npos) {
                // e.g. wmicimv2/Win32_Service?Name=WinRM
                std::string sub = uri.substr(uriLower.find("wmicimv2/") + 9);
                size_t qPos = sub.find('?');
                std::string cls = (qPos != std::string::npos) ? sub.substr(0, qPos) : sub;
                std::string cond = (qPos != std::string::npos) ? sub.substr(qPos + 1) : "";

                std::string wql = "SELECT * FROM " + cls;
                if (!cond.empty()) wql += " WHERE " + cond;
                auto res = micant::wmi::WmiRepository::get().query(wql);
                if (res.empty()) {
                    stdoutStr = "No Instance(s) Available.\r\n";
                } else {
                    std::stringstream ss;
                    for (const auto& [k, v] : res[0].properties) {
                        ss << "    " << k << " = " << v.value << "\r\n";
                    }
                    stdoutStr = ss.str();
                }
                return true;
            } else {
                stderrStr = "Error: Unrecognized resource URI: " + uri + "\r\n";
                return false;
            }
        } else if (cmdLower == "enumerate" || cmdLower == "enum") {
            if (tokens.size() < 2) {
                stderrStr = "Error: ResourceURI parameter is required for 'enumerate' command.\r\n";
                return false;
            }
            std::string uri = tokens[1];
            std::string uriLower = uri;
            for (auto& c : uriLower) if (c >= 'A' && c <= 'Z') c += 32;

            if (uriLower.find("config/listener") != std::string::npos) {
                auto listeners = winrm.getListeners();
                std::stringstream ss;
                for (const auto& l : listeners) {
                    ss << "Listener\r\n"
                       << "    Address = " << l.address << "\r\n"
                       << "    Transport = " << (l.transport == micant::winrm::ListenerTransport::Http ? "HTTP" : "HTTPS") << "\r\n"
                       << "    Port = " << l.port << "\r\n"
                       << "    Hostname = " << l.hostname << "\r\n"
                       << "    Enabled = " << (l.enabled ? "true" : "false") << "\r\n"
                       << "    URLPrefix = " << l.urlPrefix << "\r\n"
                       << "    CertificateThumbprint = " << l.certificateThumbprint << "\r\n\r\n";
                }
                stdoutStr = ss.str();
                return true;
            } else {
                stderrStr = "Error: Unrecognized enumeration target: " + uri + "\r\n";
                return false;
            }
        } else if (cmdLower == "help" || cmdLower == "/?" || cmdLower == "-?") {
            stdoutStr =
                "Windows Remote Management (WinRM 2.0 / MS-WSMV / MS-PSRP)\r\n\r\n"
                "winrm quickconfig           - Automated service and listener bootstrap\r\n"
                "winrm get winrm/config      - Display service and quota configuration\r\n"
                "winrm enumerate winrm/config/listener - List configured HTTP and HTTPS listeners\r\n"
                "winrm get wmicimv2/<Class>?<Filter>   - Query WMI/CIM provider instances\r\n";
            return true;
        } else {
            stderrStr = "Error: Unknown winrm command: " + cmd + "\r\n";
            return false;
        }
    }
};

inline bool ExecuteWinrm(const std::vector<std::string>& tokens, std::string& stdoutStr, std::string& stderrStr) {
    return WinrmCommandExecutor::execute(tokens, stdoutStr, stderrStr);
}

inline bool ExecuteWinrmString(const std::string& cmdLine, std::string& stdoutStr, std::string& stderrStr) {
    std::vector<std::string> tokens;
    std::string cur;
    bool inQuotes = false;
    for (char c : cmdLine) {
        if (c == '\"') {
            inQuotes = !inQuotes;
        } else if ((c == ' ' || c == '\t') && !inQuotes) {
            if (!cur.empty()) {
                tokens.push_back(cur);
                cur.clear();
            }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) tokens.push_back(cur);

    // Strip leading "winrm" or "winrm.cmd"
    if (!tokens.empty()) {
        std::string first = tokens[0];
        std::string firstLower = first;
        for (auto& ch : firstLower) if (ch >= 'A' && ch <= 'Z') ch += 32;
        if (firstLower == "winrm" || firstLower == "winrm.cmd") {
            tokens.erase(tokens.begin());
        }
    }

    return ExecuteWinrm(tokens, stdoutStr, stderrStr);
}

} // namespace micant::winrm_cli

namespace micant::winrs {

// ============================================================================
// Canonical winrs.exe CLI Utility
// ============================================================================

class WinrsCommandExecutor {
public:
    static bool execute(const std::vector<std::string>& tokens, std::string& stdoutStr, std::string& stderrStr, int32_t& exitCode) {
        exitCode = 0;
        if (tokens.empty()) {
            stdoutStr =
                "Windows Remote Shell (winrs.exe)\r\n"
                "Usage: winrs -r:<endpoint> [-u:<user>] [-p:<password>] [-d:<dir>] <command>\r\n\r\n"
                "Options:\r\n"
                "  -r:<endpoint>    Target remote server (hostname, IP, or URL)\r\n"
                "  -u:<user>        User credentials (e.g. domain\\user)\r\n"
                "  -p:<password>    User password\r\n"
                "  -d:<dir>         Remote working directory\r\n"
                "  <command>        Command line to execute remotely\r\n";
            return true;
        }

        std::string remoteTarget;
        std::string user = "MICANT\\Administrator";
        std::string password;
        std::string workingDir = "C:\\Windows\\System32";
        std::vector<std::string> commandTokens;

        bool parsingOptions = true;
        for (size_t i = 0; i < tokens.size(); ++i) {
            const std::string& t = tokens[i];
            if (parsingOptions && (t.rfind("-r:", 0) == 0 || t.rfind("/r:", 0) == 0)) {
                remoteTarget = t.substr(3);
            } else if (parsingOptions && (t.rfind("-u:", 0) == 0 || t.rfind("/u:", 0) == 0)) {
                user = t.substr(3);
            } else if (parsingOptions && (t.rfind("-p:", 0) == 0 || t.rfind("/p:", 0) == 0)) {
                password = t.substr(3);
            } else if (parsingOptions && (t.rfind("-d:", 0) == 0 || t.rfind("/d:", 0) == 0)) {
                workingDir = t.substr(3);
            } else if (parsingOptions && (t == "/?" || t == "-?" || t == "help")) {
                stdoutStr = "Windows Remote Shell (winrs.exe)\r\nUsage: winrs -r:<endpoint> <command>\r\n";
                return true;
            } else {
                parsingOptions = false;
                commandTokens.push_back(t);
            }
        }

        if (commandTokens.empty()) {
            stderrStr = "Error: No command specified for remote execution.\r\n";
            exitCode = 1;
            return false;
        }

        // Reconstruct command line
        std::string cmdLine;
        for (size_t i = 0; i < commandTokens.size(); ++i) {
            if (i > 0) cmdLine += " ";
            cmdLine += commandTokens[i];
        }

        // Connect to EnterpriseWinRmServer
        auto& winrm = micant::winrm::EnterpriseWinRmServer::instance();
        winrm.initialize();

        std::string clientIp = "127.0.0.1";
        std::string shellId;
        if (!winrm.openShell(micant::winrm::ShellType::Cmd, user, clientIp, workingDir, {}, shellId)) {
            stderrStr = "winrs error: Failed to open remote shell on target '" + remoteTarget + "'.\r\n";
            exitCode = -1;
            return false;
        }

        std::string cmdId;
        if (!winrm.executeCommand(shellId, cmdLine, {}, cmdId)) {
            winrm.closeShell(shellId);
            stderrStr = "winrs error: Failed to execute command on target '" + remoteTarget + "'.\r\n";
            exitCode = -1;
            return false;
        }

        bool finished = false;
        if (!winrm.receiveCommandOutput(shellId, cmdId, stdoutStr, stderrStr, exitCode, finished)) {
            winrm.closeShell(shellId);
            stderrStr = "winrs error: Failed to retrieve command output.\r\n";
            exitCode = -1;
            return false;
        }

        winrm.closeShell(shellId);
        return (exitCode == 0);
    }
};

inline bool ExecuteWinrs(const std::vector<std::string>& tokens, std::string& stdoutStr, std::string& stderrStr, int32_t& exitCode) {
    return WinrsCommandExecutor::execute(tokens, stdoutStr, stderrStr, exitCode);
}

inline bool ExecuteWinrsString(const std::string& cmdLine, std::string& stdoutStr, std::string& stderrStr, int32_t& exitCode) {
    std::vector<std::string> tokens;
    std::string cur;
    bool inQuotes = false;
    for (char c : cmdLine) {
        if (c == '\"') {
            inQuotes = !inQuotes;
        } else if ((c == ' ' || c == '\t') && !inQuotes) {
            if (!cur.empty()) {
                tokens.push_back(cur);
                cur.clear();
            }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) tokens.push_back(cur);

    // Strip leading "winrs" or "winrs.exe"
    if (!tokens.empty()) {
        std::string first = tokens[0];
        std::string firstLower = first;
        for (auto& ch : firstLower) if (ch >= 'A' && ch <= 'Z') ch += 32;
        if (firstLower == "winrs" || firstLower == "winrs.exe") {
            tokens.erase(tokens.begin());
        }
    }

    return ExecuteWinrs(tokens, stdoutStr, stderrStr, exitCode);
}

} // namespace micant::winrs
