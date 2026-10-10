#pragma once

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <atomic>
#include <cstring>
#include <cstdint>
#include <random>
#include <algorithm>
#include "ntstatus.hpp"
#include "scm.hpp"
#include "version.hpp"

namespace micant::winrm {

// ============================================================================
// WS-Management & PSRP Constants & URIs
// ============================================================================

inline constexpr const char* URI_WSMAN_ACTION_TRANSFER_GET    = "http://schemas.xmlsoap.org/ws/2004/09/transfer/Get";
inline constexpr const char* URI_WSMAN_ACTION_TRANSFER_PUT    = "http://schemas.xmlsoap.org/ws/2004/09/transfer/Put";
inline constexpr const char* URI_WSMAN_ACTION_TRANSFER_CREATE = "http://schemas.xmlsoap.org/ws/2004/09/transfer/Create";
inline constexpr const char* URI_WSMAN_ACTION_TRANSFER_DELETE = "http://schemas.xmlsoap.org/ws/2004/09/transfer/Delete";
inline constexpr const char* URI_WSMAN_ACTION_ENUMERATE       = "http://schemas.xmlsoap.org/ws/2004/09/enumeration/Enumerate";
inline constexpr const char* URI_WSMAN_ACTION_PULL            = "http://schemas.xmlsoap.org/ws/2004/09/enumeration/Pull";

inline constexpr const char* URI_WSMAN_ACTION_SHELL_COMMAND   = "http://schemas.microsoft.com/wbem/wsman/1/windows/shell/Command";
inline constexpr const char* URI_WSMAN_ACTION_SHELL_RECEIVE   = "http://schemas.microsoft.com/wbem/wsman/1/windows/shell/Receive";
inline constexpr const char* URI_WSMAN_ACTION_SHELL_SIGNAL    = "http://schemas.microsoft.com/wbem/wsman/1/windows/shell/Signal";
inline constexpr const char* URI_WSMAN_ACTION_SHELL_SEND      = "http://schemas.microsoft.com/wbem/wsman/1/windows/shell/Send";

inline constexpr const char* URI_RESOURCE_CMD_SHELL           = "http://schemas.microsoft.com/wbem/wsman/1/windows/shell/cmd";
inline constexpr const char* URI_RESOURCE_POWERSHELL          = "http://schemas.microsoft.com/powershell/Microsoft.PowerShell";
inline constexpr const char* URI_RESOURCE_CONFIG_SERVICE      = "http://schemas.microsoft.com/wbem/wsman/1/config/service";
inline constexpr const char* URI_RESOURCE_CONFIG_LISTENER     = "http://schemas.microsoft.com/wbem/wsman/1/config/listener";

inline constexpr const char* SIGNAL_CODE_TERMINATE            = "http://schemas.microsoft.com/wbem/wsman/1/windows/shell/signal/terminate";
inline constexpr const char* SIGNAL_CODE_CTRL_C               = "http://schemas.microsoft.com/wbem/wsman/1/windows/shell/signal/ctrl_c";

enum class ShellType {
    Cmd,
    PowerShell
};

enum class CommandState {
    Running,
    Completed,
    Terminated,
    Failed
};

enum class ListenerTransport {
    Http,
    Https
};

// ============================================================================
// Data Structures
// ============================================================================

struct WinRmListener {
    ListenerTransport transport{ListenerTransport::Http};
    std::string address{"*"};
    uint16_t port{5985};
    std::string hostname{"titan-mgmt01.micant.internal"};
    bool enabled{true};
    std::string certificateThumbprint;
    std::string urlPrefix{"wsman"};
};

struct WinRmServiceConfig {
    uint32_t rootMinEnvelopeSizeKb{500};
    uint32_t maxEnvelopeSizeKb{500};
    uint32_t maxTimeoutMs{60000};
    uint32_t maxBatchItems{32000};
    uint32_t maxProviderRequests{25};
    bool allowUnencrypted{false};
    bool authBasic{false};
    bool authKerberos{true};
    bool authNegotiate{true};
    bool authCertificate{true};
    bool authCredSsp{false};
};

struct WinRmQuotaConfig {
    uint32_t maxConcurrentUsers{10};
    uint32_t maxShellsPerUser{30};
    uint32_t maxProcessesPerShell{25};
    uint64_t idleTimeoutMs{7200000}; // 2 hours
    uint64_t maxMemoryPerShellMb{1024};
};

struct RemoteCommand {
    std::string commandId;
    std::string commandLine;
    std::vector<std::string> arguments;
    std::string stdoutBuffer;
    std::string stderrBuffer;
    int32_t exitCode{0};
    CommandState state{CommandState::Running};
    uint64_t startTimeUs{0};
    uint64_t finishTimeUs{0};
    bool exitCodeReported{false};
};

struct RemoteShell {
    std::string shellId;
    ShellType type{ShellType::Cmd};
    std::string ownerUser{"TITAN\\Administrator"};
    std::string ownerSid{"S-1-5-21-3819284712-2819482910-1829481923-500"};
    std::string clientIp{"10.0.0.100"};
    std::string workingDirectory{"C:\\Windows\\System32"};
    std::map<std::string, std::string> environment;
    uint64_t createdTimeUs{0};
    uint64_t lastActivityTimeUs{0};
    bool isClosed{false};
    std::map<std::string, RemoteCommand> commands;
    
    // PSRP Runspace pool status
    bool isPsrpPoolOpen{false};
    uint32_t psrpMaxRunspaces{5};
    uint32_t psrpMinRunspaces{1};
};

struct WsManMessage {
    std::string action;
    std::string resourceUri;
    std::string messageId;
    std::string replyTo{"http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous"};
    std::string to;
    std::map<std::string, std::string> selectors; // e.g. ShellId
    std::string bodyXml;
};

struct WinRmStatistics {
    uint32_t activeShells{0};
    uint32_t totalShellsCreated{0};
    uint32_t totalCommandsExecuted{0};
    uint64_t totalBytesReceived{0};
    uint64_t totalBytesSent{0};
    uint32_t totalSoapRequests{0};
    uint32_t totalSoapFaults{0};
};

// ============================================================================
// Enterprise WinRM Server Subsystem
// ============================================================================

class EnterpriseWinRmServer {
public:
    static EnterpriseWinRmServer& instance() {
        static EnterpriseWinRmServer s_inst;
        return s_inst;
    }

    bool initialize() {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_initialized) return true;

        registerScmService();
        registerVersionDatabase();

        // Default default listeners (HTTP 5985, HTTPS 5986)
        WinRmListener httpListener;
        httpListener.transport = ListenerTransport::Http;
        httpListener.address = "*";
        httpListener.port = 5985;
        httpListener.hostname = "titan-mgmt01.micant.internal";
        httpListener.enabled = true;
        m_listeners["HTTP:5985"] = httpListener;

        WinRmListener httpsListener;
        httpsListener.transport = ListenerTransport::Https;
        httpsListener.address = "*";
        httpsListener.port = 5986;
        httpsListener.hostname = "titan-mgmt01.micant.internal";
        httpsListener.enabled = true;
        httpsListener.certificateThumbprint = "4F5B891A6C2E337890BCDEF123456789ABCDEF01";
        m_listeners["HTTPS:5986"] = httpsListener;

        m_initialized = true;
        return true;
    }

    void shutdown() {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_shells.clear();
        m_initialized = false;
    }

    // --- Configuration & Quotas ---

    WinRmServiceConfig getServiceConfig() const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_serviceConfig;
    }

    void setServiceConfig(const WinRmServiceConfig& cfg) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_serviceConfig = cfg;
    }

    WinRmQuotaConfig getQuotaConfig() const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_quotaConfig;
    }

    void setQuotaConfig(const WinRmQuotaConfig& cfg) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_quotaConfig = cfg;
    }

    // --- Listeners Management ---

    bool addListener(const WinRmListener& listener) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        std::string key = (listener.transport == ListenerTransport::Http ? "HTTP:" : "HTTPS:") + std::to_string(listener.port);
        m_listeners[key] = listener;
        return true;
    }

    bool removeListener(ListenerTransport transport, uint16_t port) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        std::string key = (transport == ListenerTransport::Http ? "HTTP:" : "HTTPS:") + std::to_string(port);
        return m_listeners.erase(key) > 0;
    }

    std::vector<WinRmListener> getListeners() const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        std::vector<WinRmListener> result;
        result.reserve(m_listeners.size());
        for (const auto& [_, l] : m_listeners) {
            result.push_back(l);
        }
        return result;
    }

    // --- Remote Shell Session Management ---

    bool openShell(ShellType type, const std::string& ownerUser, const std::string& clientIp,
                   const std::string& workingDir, const std::map<std::string, std::string>& env,
                   std::string& outShellId) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);

        // Check concurrent users & shells quota
        uint32_t activeUserShells = 0;
        for (const auto& [_, sh] : m_shells) {
            if (!sh.isClosed && sh.ownerUser == ownerUser) {
                activeUserShells++;
            }
        }
        if (activeUserShells >= m_quotaConfig.maxShellsPerUser) {
            m_stats.totalSoapFaults++;
            return false;
        }

        std::string shellId = generateUuid();
        RemoteShell shell;
        shell.shellId = shellId;
        shell.type = type;
        shell.ownerUser = ownerUser;
        shell.clientIp = clientIp;
        shell.workingDirectory = workingDir.empty() ? "C:\\Windows\\System32" : workingDir;
        shell.environment = env;
        if (shell.environment.find("COMPUTERNAME") == shell.environment.end()) {
            shell.environment["COMPUTERNAME"] = "TITAN-MGMT01";
        }
        if (shell.environment.find("USERNAME") == shell.environment.end()) {
            shell.environment["USERNAME"] = ownerUser;
        }

        uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        shell.createdTimeUs = now;
        shell.lastActivityTimeUs = now;
        shell.isClosed = false;

        if (type == ShellType::PowerShell) {
            shell.isPsrpPoolOpen = true;
        }

        m_shells[shellId] = shell;
        outShellId = shellId;

        m_stats.activeShells++;
        m_stats.totalShellsCreated++;
        return true;
    }

    bool closeShell(const std::string& shellId) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_shells.find(shellId);
        if (it == m_shells.end() || it->second.isClosed) return false;

        it->second.isClosed = true;
        for (auto& [_, cmd] : it->second.commands) {
            if (cmd.state == CommandState::Running) {
                cmd.state = CommandState::Terminated;
                cmd.exitCode = -1;
            }
        }

        if (m_stats.activeShells > 0) m_stats.activeShells--;
        return true;
    }

    bool getShell(const std::string& shellId, RemoteShell& outShell) const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_shells.find(shellId);
        if (it == m_shells.end() || it->second.isClosed) return false;
        outShell = it->second;
        return true;
    }

    std::vector<RemoteShell> getActiveShells() const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        std::vector<RemoteShell> res;
        for (const auto& [_, sh] : m_shells) {
            if (!sh.isClosed) res.push_back(sh);
        }
        return res;
    }

    // --- Command Execution & Output Streaming ---

    bool executeCommand(const std::string& shellId, const std::string& commandLine,
                        const std::vector<std::string>& arguments, std::string& outCommandId) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_shells.find(shellId);
        if (it == m_shells.end() || it->second.isClosed) return false;

        // Check max processes per shell
        uint32_t runningCmds = 0;
        for (const auto& [_, c] : it->second.commands) {
            if (c.state == CommandState::Running) runningCmds++;
        }
        if (runningCmds >= m_quotaConfig.maxProcessesPerShell) {
            m_stats.totalSoapFaults++;
            return false;
        }

        std::string cmdId = generateUuid();
        RemoteCommand cmd;
        cmd.commandId = cmdId;
        cmd.commandLine = commandLine;
        cmd.arguments = arguments;
        cmd.state = CommandState::Running;

        uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        cmd.startTimeUs = now;
        it->second.lastActivityTimeUs = now;

        // Execute command logic (clean-room internal dispatcher)
        executeInternalCommand(it->second, cmd);

        it->second.commands[cmdId] = cmd;
        outCommandId = cmdId;

        m_stats.totalCommandsExecuted++;
        return true;
    }

    bool receiveCommandOutput(const std::string& shellId, const std::string& commandId,
                              std::string& outStdout, std::string& outStderr,
                              int32_t& outExitCode, bool& outFinished) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_shells.find(shellId);
        if (it == m_shells.end()) return false;

        auto cIt = it->second.commands.find(commandId);
        if (cIt == it->second.commands.end()) return false;

        auto& cmd = cIt->second;
        outStdout = cmd.stdoutBuffer;
        outStderr = cmd.stderrBuffer;
        outExitCode = cmd.exitCode;
        outFinished = (cmd.state != CommandState::Running);

        m_stats.totalBytesSent += (outStdout.size() + outStderr.size());
        cmd.exitCodeReported = true;
        return true;
    }

    bool signalCommand(const std::string& shellId, const std::string& commandId, const std::string& signalCode) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_shells.find(shellId);
        if (it == m_shells.end()) return false;

        auto cIt = it->second.commands.find(commandId);
        if (cIt == it->second.commands.end()) return false;

        if (signalCode == SIGNAL_CODE_TERMINATE || signalCode == SIGNAL_CODE_CTRL_C) {
            cIt->second.state = CommandState::Terminated;
            cIt->second.exitCode = -1;
            cIt->second.stderrBuffer += "\nCommand canceled by signal.\n";
            return true;
        }
        return false;
    }

    // --- SOAP WS-Management Wire Processing ---

    bool processSoapRequest(const std::string& inXml, std::string& outXml) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_stats.totalSoapRequests++;
        m_stats.totalBytesReceived += inXml.size();

        WsManMessage req;
        if (!parseSoapEnvelope(inXml, req)) {
            m_stats.totalSoapFaults++;
            outXml = buildSoapFault("wsman:InvalidEnvelope", "Invalid or malformed WS-Management SOAP envelope");
            m_stats.totalBytesSent += outXml.size();
            return false;
        }

        // Action routing
        if (req.action == URI_WSMAN_ACTION_TRANSFER_CREATE) {
            // Create Shell
            std::string shellId;
            ShellType st = (req.resourceUri == URI_RESOURCE_POWERSHELL) ? ShellType::PowerShell : ShellType::Cmd;
            if (openShell(st, "TITAN\\Administrator", "127.0.0.1", "C:\\Windows\\System32", {}, shellId)) {
                outXml = buildCreateShellResponse(req, shellId);
                m_stats.totalBytesSent += outXml.size();
                return true;
            } else {
                m_stats.totalSoapFaults++;
                outXml = buildSoapFault("wsman:QuotaLimitReached", "User quota exceeded for remote shells");
                m_stats.totalBytesSent += outXml.size();
                return false;
            }
        } else if (req.action == URI_WSMAN_ACTION_TRANSFER_DELETE) {
            // Delete Shell
            auto it = req.selectors.find("ShellId");
            if (it != req.selectors.end() && closeShell(it->second)) {
                outXml = buildDeleteShellResponse(req);
                m_stats.totalBytesSent += outXml.size();
                return true;
            } else {
                m_stats.totalSoapFaults++;
                outXml = buildSoapFault("wsa:DestinationUnreachable", "Target ShellId does not exist or already closed");
                m_stats.totalBytesSent += outXml.size();
                return false;
            }
        } else if (req.action == URI_WSMAN_ACTION_SHELL_COMMAND) {
            // Execute Command inside Shell
            auto it = req.selectors.find("ShellId");
            if (it == req.selectors.end()) {
                m_stats.totalSoapFaults++;
                outXml = buildSoapFault("wsman:InvalidSelectors", "Missing ShellId selector");
                m_stats.totalBytesSent += outXml.size();
                return false;
            }
            std::string cmdLine = extractXmlTag(req.bodyXml, "rsp:CommandLine");
            if (cmdLine.empty()) cmdLine = extractXmlTag(req.bodyXml, "CommandLine");
            if (cmdLine.empty()) cmdLine = "whoami";

            std::string cmdId;
            if (executeCommand(it->second, cmdLine, {}, cmdId)) {
                outXml = buildExecuteCommandResponse(req, cmdId);
                m_stats.totalBytesSent += outXml.size();
                return true;
            } else {
                m_stats.totalSoapFaults++;
                outXml = buildSoapFault("wsman:ExecutionError", "Failed to execute command in shell");
                m_stats.totalBytesSent += outXml.size();
                return false;
            }
        } else if (req.action == URI_WSMAN_ACTION_SHELL_RECEIVE) {
            // Stream Command Output
            auto sIt = req.selectors.find("ShellId");
            std::string cmdId = extractXmlTag(req.bodyXml, "rsp:CommandId");
            if (cmdId.empty()) cmdId = extractXmlTag(req.bodyXml, "CommandId");

            if (sIt != req.selectors.end() && !cmdId.empty()) {
                std::string stdOutStr, stdErrStr;
                int32_t exitCode = 0;
                bool finished = false;
                if (receiveCommandOutput(sIt->second, cmdId, stdOutStr, stdErrStr, exitCode, finished)) {
                    outXml = buildReceiveOutputResponse(req, stdOutStr, stdErrStr, exitCode, finished);
                    m_stats.totalBytesSent += outXml.size();
                    return true;
                }
            }
            m_stats.totalSoapFaults++;
            outXml = buildSoapFault("wsman:InvalidCommand", "Unknown shell or command ID for receive");
            m_stats.totalBytesSent += outXml.size();
            return false;
        } else if (req.action == URI_WSMAN_ACTION_SHELL_SIGNAL) {
            // Signal Command (Terminate/Ctrl-C)
            auto sIt = req.selectors.find("ShellId");
            std::string cmdId = extractXmlTag(req.bodyXml, "rsp:CommandId");
            std::string sigCode = extractXmlTag(req.bodyXml, "rsp:Code");
            if (sigCode.empty()) sigCode = SIGNAL_CODE_TERMINATE;

            if (sIt != req.selectors.end() && !cmdId.empty()) {
                if (signalCommand(sIt->second, cmdId, sigCode)) {
                    outXml = buildSignalResponse(req);
                    m_stats.totalBytesSent += outXml.size();
                    return true;
                }
            }
            m_stats.totalSoapFaults++;
            outXml = buildSoapFault("wsman:SignalFailed", "Failed to deliver signal to command");
            m_stats.totalBytesSent += outXml.size();
            return false;
        } else if (req.action == URI_WSMAN_ACTION_TRANSFER_GET) {
            // Get configuration
            outXml = buildGetConfigResponse(req);
            m_stats.totalBytesSent += outXml.size();
            return true;
        }

        m_stats.totalSoapFaults++;
        outXml = buildSoapFault("wsa:ActionNotSupported", "The requested action is not supported by WinRM server");
        m_stats.totalBytesSent += outXml.size();
        return false;
    }

    WinRmStatistics getStatistics() const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_stats;
    }

private:
    EnterpriseWinRmServer() = default;
    ~EnterpriseWinRmServer() = default;
    EnterpriseWinRmServer(const EnterpriseWinRmServer&) = delete;
    EnterpriseWinRmServer& operator=(const EnterpriseWinRmServer&) = delete;

    void registerScmService() {
        auto& scm = micant::scm::ServiceControlManager::get();
        auto recWinRm = std::make_shared<micant::scm::ServiceRecord>();
        recWinRm->serviceName = L"WinRM";
        recWinRm->displayName = L"Windows Remote Management (WS-Management)";
        recWinRm->serviceType = micant::scm::SERVICE_WIN32_SHARE_PROCESS;
        recWinRm->startType = micant::scm::SERVICE_AUTO_START;
        recWinRm->errorControl = micant::scm::SERVICE_ERROR_NORMAL;
        recWinRm->binaryPath = L"C:\\Windows\\System32\\svchost.exe -k NetworkService";
        recWinRm->status.dwCurrentState = micant::scm::SERVICE_RUNNING;
        scm.registerServiceRecord(recWinRm);
    }

    void registerVersionDatabase() {
        auto& db = micant::version::VersionDatabase::Instance();
        db.RegisterModule("winrm.cmd", "10.0.26100.1", "Windows Remote Management Command Line Tool");
        db.RegisterModule("winrs.exe", "10.0.26100.1", "Windows Remote Shell Client");
        db.RegisterModule("wsmprovhost.exe", "10.0.26100.1", "WS-Management Provider Host");
        db.RegisterModule("wsmres.dll", "10.0.26100.1", "WS-Management Resources");
        db.RegisterModule("wsmagent.dll", "10.0.26100.1", "WS-Management Agent Core");
        db.RegisterModule("wsmsvc.dll", "10.0.26100.1", "WS-Management Service DLL");
    }

    std::string generateUuid() {
        static std::mt19937_64 rng(133742);
        std::uniform_int_distribution<uint64_t> dist;
        uint64_t p1 = dist(rng);
        uint64_t p2 = dist(rng);

        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        ss << std::setw(8) << (p1 >> 32) << "-";
        ss << std::setw(4) << ((p1 >> 16) & 0xFFFF) << "-";
        ss << std::setw(4) << (0x4000 | ((p1) & 0x0FFF)) << "-";
        ss << std::setw(4) << (0x8000 | ((p2 >> 48) & 0x3FFF)) << "-";
        ss << std::setw(12) << (p2 & 0xFFFFFFFFFFFFULL);
        return ss.str();
    }

    void executeInternalCommand(const RemoteShell& shell, RemoteCommand& cmd) {
        std::string raw = cmd.commandLine;
        // Trim whitespace
        while (!raw.empty() && (raw.front() == ' ' || raw.front() == '\t')) raw.erase(0, 1);
        while (!raw.empty() && (raw.back() == ' ' || raw.back() == '\t')) raw.pop_back();

        if (raw == "whoami" || raw == "whoami.exe") {
            cmd.stdoutBuffer = shell.ownerUser + "\r\n";
            cmd.exitCode = 0;
            cmd.state = CommandState::Completed;
        } else if (raw == "hostname" || raw == "hostname.exe") {
            cmd.stdoutBuffer = "TITAN-MGMT01\r\n";
            cmd.exitCode = 0;
            cmd.state = CommandState::Completed;
        } else if (raw.rfind("echo ", 0) == 0) {
            cmd.stdoutBuffer = raw.substr(5) + "\r\n";
            cmd.exitCode = 0;
            cmd.state = CommandState::Completed;
        } else if (raw == "ipconfig" || raw == "ipconfig /all") {
            cmd.stdoutBuffer = 
                "\r\nWindows IP Configuration\r\n\r\n"
                "Ethernet adapter vEthernet (Enterprise):\r\n"
                "   Connection-specific DNS Suffix  . : micant.internal\r\n"
                "   IPv4 Address. . . . . . . . . . . : 10.0.0.10\r\n"
                "   Subnet Mask . . . . . . . . . . . : 255.255.255.0\r\n"
                "   Default Gateway . . . . . . . . . : 10.0.0.1\r\n";
            cmd.exitCode = 0;
            cmd.state = CommandState::Completed;
        } else if (raw == "ver") {
            cmd.stdoutBuffer = "\r\nMicrosoft Windows [Version 10.0.26100.1]\r\n";
            cmd.exitCode = 0;
            cmd.state = CommandState::Completed;
        } else if (raw.rfind("Get-Process", 0) == 0 || raw.rfind("ps", 0) == 0) {
            cmd.stdoutBuffer = 
                "Handles  NPM(K)    PM(K)      WS(K)     CPU(s)     Id  SI ProcessName\r\n"
                "-------  ------    -----      -----     ------     --  -- -----------\r\n"
                "    412      24    42100      52180       1.24    840   1 winrm\r\n"
                "    890      48   102400     118400       3.89    912   1 svchost\r\n"
                "   1204      62   210500     245100       8.15    404   0 System\r\n";
            cmd.exitCode = 0;
            cmd.state = CommandState::Completed;
        } else {
            // General success simulation for arbitrary commands
            cmd.stdoutBuffer = "[WinRM Execution: " + raw + " on TITAN-MGMT01]\r\nExecution completed successfully.\r\n";
            cmd.exitCode = 0;
            cmd.state = CommandState::Completed;
        }

        uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        cmd.finishTimeUs = now;
    }

    bool parseSoapEnvelope(const std::string& xml, WsManMessage& outMsg) {
        if (xml.find("<s:Envelope") == std::string::npos && xml.find("<soap:Envelope") == std::string::npos &&
            xml.find("<Envelope") == std::string::npos) {
            return false;
        }

        outMsg.action = extractXmlTag(xml, "wsa:Action");
        if (outMsg.action.empty()) outMsg.action = extractXmlTag(xml, "Action");

        outMsg.resourceUri = extractXmlTag(xml, "wsman:ResourceURI");
        if (outMsg.resourceUri.empty()) outMsg.resourceUri = extractXmlTag(xml, "ResourceURI");

        outMsg.messageId = extractXmlTag(xml, "wsa:MessageID");
        if (outMsg.messageId.empty()) outMsg.messageId = extractXmlTag(xml, "MessageID");

        // Parse SelectorSet
        size_t selPos = xml.find("<wsman:SelectorSet>");
        if (selPos == std::string::npos) selPos = xml.find("<SelectorSet>");
        if (selPos != std::string::npos) {
            size_t selEnd = xml.find("</wsman:SelectorSet>", selPos);
            if (selEnd == std::string::npos) selEnd = xml.find("</SelectorSet>", selPos);
            if (selEnd != std::string::npos) {
                std::string selBlock = xml.substr(selPos, selEnd - selPos);
                // Look for ShellId
                size_t idPos = selBlock.find("Name=\"ShellId\"");
                if (idPos != std::string::npos) {
                    size_t vStart = selBlock.find('>', idPos);
                    size_t vEnd = selBlock.find('<', vStart);
                    if (vStart != std::string::npos && vEnd != std::string::npos) {
                        outMsg.selectors["ShellId"] = selBlock.substr(vStart + 1, vEnd - vStart - 1);
                    }
                }
            }
        }

        // Body
        size_t bStart = xml.find("<s:Body>");
        if (bStart == std::string::npos) bStart = xml.find("<Body>");
        if (bStart != std::string::npos) {
            size_t bEnd = xml.find("</s:Body>", bStart);
            if (bEnd == std::string::npos) bEnd = xml.find("</Body>", bStart);
            if (bEnd != std::string::npos) {
                outMsg.bodyXml = xml.substr(bStart, bEnd - bStart);
            }
        }

        return !outMsg.action.empty();
    }

    std::string extractXmlTag(const std::string& xml, const std::string& tag) {
        std::string openTag = "<" + tag;
        size_t start = xml.find(openTag);
        if (start == std::string::npos) return "";

        size_t contentStart = xml.find('>', start);
        if (contentStart == std::string::npos) return "";
        contentStart++;

        std::string closeTag = "</" + tag + ">";
        size_t end = xml.find(closeTag, contentStart);
        if (end == std::string::npos) return "";

        return xml.substr(contentStart, end - contentStart);
    }

    std::string buildCreateShellResponse(const WsManMessage& req, const std::string& shellId) {
        std::stringstream ss;
        ss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
           << "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
           << "xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
           << "xmlns:wsman=\"http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd\" "
           << "xmlns:rsp=\"http://schemas.microsoft.com/wbem/wsman/1/windows/shell\">\r\n"
           << "  <s:Header>\r\n"
           << "    <wsa:Action>http://schemas.xmlsoap.org/ws/2004/09/transfer/CreateResponse</wsa:Action>\r\n"
           << "    <wsa:RelatesTo>" << req.messageId << "</wsa:RelatesTo>\r\n"
           << "  </s:Header>\r\n"
           << "  <s:Body>\r\n"
           << "    <rsp:Shell>\r\n"
           << "      <rsp:ShellId>" << shellId << "</rsp:ShellId>\r\n"
           << "      <rsp:ResourceURI>" << req.resourceUri << "</rsp:ResourceURI>\r\n"
           << "      <rsp:Owner>TITAN\\Administrator</rsp:Owner>\r\n"
           << "      <rsp:ClientIP>127.0.0.1</rsp:ClientIP>\r\n"
           << "    </rsp:Shell>\r\n"
           << "  </s:Body>\r\n"
           << "</s:Envelope>\r\n";
        return ss.str();
    }

    std::string buildDeleteShellResponse(const WsManMessage& req) {
        std::stringstream ss;
        ss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
           << "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
           << "xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\">\r\n"
           << "  <s:Header>\r\n"
           << "    <wsa:Action>http://schemas.xmlsoap.org/ws/2004/09/transfer/DeleteResponse</wsa:Action>\r\n"
           << "    <wsa:RelatesTo>" << req.messageId << "</wsa:RelatesTo>\r\n"
           << "  </s:Header>\r\n"
           << "  <s:Body/>\r\n"
           << "</s:Envelope>\r\n";
        return ss.str();
    }

    std::string buildExecuteCommandResponse(const WsManMessage& req, const std::string& cmdId) {
        std::stringstream ss;
        ss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
           << "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
           << "xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
           << "xmlns:rsp=\"http://schemas.microsoft.com/wbem/wsman/1/windows/shell\">\r\n"
           << "  <s:Header>\r\n"
           << "    <wsa:Action>http://schemas.microsoft.com/wbem/wsman/1/windows/shell/CommandResponse</wsa:Action>\r\n"
           << "    <wsa:RelatesTo>" << req.messageId << "</wsa:RelatesTo>\r\n"
           << "  </s:Header>\r\n"
           << "  <s:Body>\r\n"
           << "    <rsp:CommandResponse>\r\n"
           << "      <rsp:CommandId>" << cmdId << "</rsp:CommandId>\r\n"
           << "    </rsp:CommandResponse>\r\n"
           << "  </s:Body>\r\n"
           << "</s:Envelope>\r\n";
        return ss.str();
    }

    std::string buildReceiveOutputResponse(const WsManMessage& req, const std::string& stdoutStr,
                                           const std::string& stderrStr, int32_t exitCode, bool finished) {
        std::stringstream ss;
        ss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
           << "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
           << "xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
           << "xmlns:rsp=\"http://schemas.microsoft.com/wbem/wsman/1/windows/shell\">\r\n"
           << "  <s:Header>\r\n"
           << "    <wsa:Action>http://schemas.microsoft.com/wbem/wsman/1/windows/shell/ReceiveResponse</wsa:Action>\r\n"
           << "    <wsa:RelatesTo>" << req.messageId << "</wsa:RelatesTo>\r\n"
           << "  </s:Header>\r\n"
           << "  <s:Body>\r\n"
           << "    <rsp:ReceiveResponse>\r\n";

        if (!stdoutStr.empty()) {
            ss << "      <rsp:Stream Name=\"stdout\">" << stdoutStr << "</rsp:Stream>\r\n";
        }
        if (!stderrStr.empty()) {
            ss << "      <rsp:Stream Name=\"stderr\">" << stderrStr << "</rsp:Stream>\r\n";
        }
        if (finished) {
            ss << "      <rsp:CommandState State=\"Done\">\r\n"
               << "        <rsp:ExitCode>" << exitCode << "</rsp:ExitCode>\r\n"
               << "      </rsp:CommandState>\r\n";
        } else {
            ss << "      <rsp:CommandState State=\"Running\"/>\r\n";
        }

        ss << "    </rsp:ReceiveResponse>\r\n"
           << "  </s:Body>\r\n"
           << "</s:Envelope>\r\n";
        return ss.str();
    }

    std::string buildSignalResponse(const WsManMessage& req) {
        std::stringstream ss;
        ss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
           << "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
           << "xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
           << "xmlns:rsp=\"http://schemas.microsoft.com/wbem/wsman/1/windows/shell\">\r\n"
           << "  <s:Header>\r\n"
           << "    <wsa:Action>http://schemas.microsoft.com/wbem/wsman/1/windows/shell/SignalResponse</wsa:Action>\r\n"
           << "    <wsa:RelatesTo>" << req.messageId << "</wsa:RelatesTo>\r\n"
           << "  </s:Header>\r\n"
           << "  <s:Body/>\r\n"
           << "</s:Envelope>\r\n";
        return ss.str();
    }

    std::string buildGetConfigResponse(const WsManMessage& req) {
        std::stringstream ss;
        ss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
           << "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
           << "xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
           << "xmlns:cfg=\"http://schemas.microsoft.com/wbem/wsman/1/config\">\r\n"
           << "  <s:Header>\r\n"
           << "    <wsa:Action>http://schemas.xmlsoap.org/ws/2004/09/transfer/GetResponse</wsa:Action>\r\n"
           << "    <wsa:RelatesTo>" << req.messageId << "</wsa:RelatesTo>\r\n"
           << "  </s:Header>\r\n"
           << "  <s:Body>\r\n"
           << "    <cfg:Config>\r\n"
           << "      <cfg:MaxEnvelopeSizekb>" << m_serviceConfig.maxEnvelopeSizeKb << "</cfg:MaxEnvelopeSizekb>\r\n"
           << "      <cfg:MaxTimeoutms>" << m_serviceConfig.maxTimeoutMs << "</cfg:MaxTimeoutms>\r\n"
           << "      <cfg:MaxBatchItems>" << m_serviceConfig.maxBatchItems << "</cfg:MaxBatchItems>\r\n"
           << "    </cfg:Config>\r\n"
           << "  </s:Body>\r\n"
           << "</s:Envelope>\r\n";
        return ss.str();
    }

    std::string buildSoapFault(const std::string& faultSubcode, const std::string& faultReason) {
        std::stringstream ss;
        ss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
           << "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
           << "xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\">\r\n"
           << "  <s:Header>\r\n"
           << "    <wsa:Action>http://schemas.xmlsoap.org/ws/2004/08/addressing/fault</wsa:Action>\r\n"
           << "  </s:Header>\r\n"
           << "  <s:Body>\r\n"
           << "    <s:Fault>\r\n"
           << "      <s:Code>\r\n"
           << "        <s:Value>s:Sender</s:Value>\r\n"
           << "        <s:Subcode><s:Value>" << faultSubcode << "</s:Value></s:Subcode>\r\n"
           << "      </s:Code>\r\n"
           << "      <s:Reason>\r\n"
           << "        <s:Text xml:lang=\"en-US\">" << faultReason << "</s:Text>\r\n"
           << "      </s:Reason>\r\n"
           << "    </s:Fault>\r\n"
           << "  </s:Body>\r\n"
           << "</s:Envelope>\r\n";
        return ss.str();
    }

    mutable std::recursive_mutex m_mutex;
    bool m_initialized{false};
    WinRmServiceConfig m_serviceConfig;
    WinRmQuotaConfig m_quotaConfig;
    std::map<std::string, WinRmListener> m_listeners;
    std::map<std::string, RemoteShell> m_shells;
    WinRmStatistics m_stats;
};

} // namespace micant::winrm

// ============================================================================
// Win32 C ABI Exports
// ============================================================================

using micant::NTSTATUS;

extern "C" {

inline NTSTATUS MicaWinRmInitialize() {
    return micant::winrm::EnterpriseWinRmServer::instance().initialize() ? micant::STATUS_SUCCESS : micant::STATUS_UNSUCCESSFUL;
}

inline NTSTATUS MicaWinRmCreateListener(const char* transport, uint16_t port, const char* address, const char* certThumbprint) {
    if (!transport) return micant::STATUS_INVALID_PARAMETER;
    micant::winrm::WinRmListener l;
    std::string transStr(transport);
    l.transport = (transStr == "HTTPS" || transStr == "https") ? micant::winrm::ListenerTransport::Https : micant::winrm::ListenerTransport::Http;
    l.port = port;
    l.address = address ? address : "*";
    if (certThumbprint) l.certificateThumbprint = certThumbprint;
    l.enabled = true;
    return micant::winrm::EnterpriseWinRmServer::instance().addListener(l) ? micant::STATUS_SUCCESS : micant::STATUS_UNSUCCESSFUL;
}

inline NTSTATUS MicaWinRmOpenShell(const char* shellType, const char* user, char* outShellId, size_t outShellIdCap) {
    if (!outShellId || outShellIdCap < 37) return micant::STATUS_BUFFER_TOO_SMALL;
    std::string st = shellType ? shellType : "cmd";
    micant::winrm::ShellType type = (st == "powershell" || st == "ps") ? micant::winrm::ShellType::PowerShell : micant::winrm::ShellType::Cmd;
    std::string u = user ? user : "TITAN\\Administrator";
    std::string sId;
    if (!micant::winrm::EnterpriseWinRmServer::instance().openShell(type, u, "127.0.0.1", "C:\\Windows\\System32", {}, sId)) {
        return micant::STATUS_UNSUCCESSFUL;
    }
    std::strncpy(outShellId, sId.c_str(), outShellIdCap - 1);
    outShellId[outShellIdCap - 1] = '\0';
    return micant::STATUS_SUCCESS;
}

inline NTSTATUS MicaWinRmExecuteCommand(const char* shellId, const char* commandLine, char* outCommandId, size_t outCommandIdCap) {
    if (!shellId || !commandLine || !outCommandId || outCommandIdCap < 37) return micant::STATUS_INVALID_PARAMETER;
    std::string cId;
    if (!micant::winrm::EnterpriseWinRmServer::instance().executeCommand(shellId, commandLine, {}, cId)) {
        return micant::STATUS_UNSUCCESSFUL;
    }
    std::strncpy(outCommandId, cId.c_str(), outCommandIdCap - 1);
    outCommandId[outCommandIdCap - 1] = '\0';
    return micant::STATUS_SUCCESS;
}

inline NTSTATUS MicaWinRmReceiveOutput(const char* shellId, const char* commandId,
                                      char* outStdout, size_t outStdoutCap,
                                      int32_t* outExitCode, bool* outFinished) {
    if (!shellId || !commandId || !outStdout) return micant::STATUS_INVALID_PARAMETER;
    std::string sOut, sErr;
    int32_t ec = 0;
    bool fin = false;
    if (!micant::winrm::EnterpriseWinRmServer::instance().receiveCommandOutput(shellId, commandId, sOut, sErr, ec, fin)) {
        return micant::STATUS_UNSUCCESSFUL;
    }
    std::strncpy(outStdout, sOut.c_str(), outStdoutCap - 1);
    outStdout[outStdoutCap - 1] = '\0';
    if (outExitCode) *outExitCode = ec;
    if (outFinished) *outFinished = fin;
    return micant::STATUS_SUCCESS;
}

inline NTSTATUS MicaWinRmCloseShell(const char* shellId) {
    if (!shellId) return micant::STATUS_INVALID_PARAMETER;
    return micant::winrm::EnterpriseWinRmServer::instance().closeShell(shellId) ? micant::STATUS_SUCCESS : micant::STATUS_UNSUCCESSFUL;
}

inline NTSTATUS MicaWinRmGetStats(uint32_t* activeShells, uint32_t* totalCommands, uint64_t* totalBytesTransferred) {
    auto stats = micant::winrm::EnterpriseWinRmServer::instance().getStatistics();
    if (activeShells) *activeShells = stats.activeShells;
    if (totalCommands) *totalCommands = stats.totalCommandsExecuted;
    if (totalBytesTransferred) *totalBytesTransferred = (stats.totalBytesReceived + stats.totalBytesSent);
    return micant::STATUS_SUCCESS;
}

inline NTSTATUS MicaWinRmShutdown() {
    micant::winrm::EnterpriseWinRmServer::instance().shutdown();
    return micant::STATUS_SUCCESS;
}

} // extern "C"
