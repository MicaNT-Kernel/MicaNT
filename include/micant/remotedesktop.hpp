#ifndef MICANT_REMOTEDESKTOP_HPP
#define MICANT_REMOTEDESKTOP_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <functional>
#include <atomic>
#include <cstring>
#include <deque>

#include "ntstatus.hpp"
#include "version.hpp"
#include "scm.hpp"

namespace micant::rds {

// Win32 WTS Session State Enumeration (wtsapi32.h)
enum WTS_CONNECTSTATE_CLASS {
    WTSActive,              // User logged on to WinStation
    WTSConnected,           // WinStation connected to client
    WTSConnectQuery,        // In the process of connecting to query
    WTSShadow,              // Shadowing another WinStation
    WTSDisconnected,        // WinStation logged on without client
    WTSIdle,                // Waiting for connection
    WTSListen,              // WinStation listening for connection
    WTSReset,               // WinStation being reset
    WTSDown,                // WinStation down due to error
    WTSInit                 // WinStation initializing
};

// Win32 WTS Protocol Types
enum WTS_PROTOCOL_TYPE {
    WTS_PROTOCOL_TYPE_CONSOLE = 0,
    WTS_PROTOCOL_TYPE_ICA     = 1,
    WTS_PROTOCOL_TYPE_RDP     = 2
};

// Win32 WTS Information Classes (WTSQuerySessionInformation)
enum WTS_INFO_CLASS {
    WTSInitialProgram       = 0,
    WTSApplicationName      = 1,
    WTSWorkingDirectory     = 2,
    WTSOEMId                = 3,
    WTSSessionId            = 4,
    WTSUserName             = 5,
    WTSWinStationName       = 6,
    WTSDomainName           = 7,
    WTSConnectState         = 8,
    WTSClientBuildNumber    = 9,
    WTSClientName           = 10,
    WTSClientDirectory      = 11,
    WTSClientProductId      = 12,
    WTSClientHardwareId     = 13,
    WTSClientAddress        = 14,
    WTSClientDisplay        = 15,
    WTSClientProtocolType   = 16,
    WTSIdleTime             = 17,
    WTSLogonTime            = 18,
    WTSIncomingBytes        = 19,
    WTSOutgoingBytes        = 20,
    WTSIncomingFrames       = 21,
    WTSOutgoingFrames       = 22,
    WTSClientInfo           = 23,
    WTSSessionInfo          = 24
};

// Win32 WTS Shadowing Modes
enum WTS_SHADOW_MODE {
    WTS_SHADOW_DISABLE               = 0,
    WTS_SHADOW_ENABLE_INPUT_NOTIFY   = 1,
    WTS_SHADOW_ENABLE_INPUT_NO_NOTIFY= 2,
    WTS_SHADOW_ENABLE_NO_INPUT_NOTIFY= 3,
    WTS_SHADOW_ENABLE_NO_INPUT_NO_NOTIFY = 4
};

// Win32 Session Info Structure
struct WTS_SESSION_INFOW {
    uint32_t SessionId;
    wchar_t* pWinStationName;
    WTS_CONNECTSTATE_CLASS State;
};

// Client Display Metrics
struct ClientDisplayMetrics {
    uint32_t width{1920};
    uint32_t height{1080};
    uint32_t colorDepth{32}; // 16, 24, 32 bpp
    uint32_t refreshRateHz{60};
    uint32_t monitorCount{1};
};

// RDP Connection Handshake State Machine
enum class RdpHandshakeState : uint32_t {
    Disconnected       = 0,
    X224_Negotiation   = 1, // X.224 Connection Request (TLS/CredSSP/NLA)
    McsDomainConnect   = 2, // MCS Connect Initial / Attach User
    SecurityExchange   = 3, // Client/Server Random & AES Key Derivation
    ClientInfoPdu      = 4, // Auto-logon, Credentials & Domain
    CapabilityExchange = 5, // Order, Bitmap, Input, Multimonitor capabilities
    ChannelJoin        = 6, // Virtual Channels (cliprdr, rdpsnd, rdpdr)
    FinalizedSession   = 7  // Desktop Active & Streaming
};

// Virtual Channel Definition
struct VirtualChannel {
    std::string channelName; // e.g. "cliprdr", "rdpsnd", "rdpdr", "rdpgfx"
    uint16_t channelId{0};
    uint32_t channelFlags{0};
    std::deque<std::vector<uint8_t>> inboundQueue;
    std::deque<std::vector<uint8_t>> outboundQueue;
    uint64_t totalBytesReceived{0};
    uint64_t totalBytesSent{0};
};

// Remote Desktop WinStation Session Descriptor
struct RdpSession {
    uint32_t sessionId{0};
    std::wstring winStationName; // e.g. L"RDP-Tcp#1", L"Console"
    std::wstring userName;       // e.g. L"Administrator", L"DevUser"
    std::wstring domainName;     // e.g. L"MICANT"
    std::wstring clientName;     // e.g. L"TITAN-LAPTOP"
    std::string clientIp;        // e.g. "192.168.1.150"
    uint32_t clientBuild{26100};
    WTS_CONNECTSTATE_CLASS state{WTSIdle};
    WTS_PROTOCOL_TYPE protocolType{WTS_PROTOCOL_TYPE_RDP};
    RdpHandshakeState handshakeState{RdpHandshakeState::Disconnected};

    bool nlaAuthenticated{false};
    bool isConsoleSession{false};
    uint32_t shadowSessionId{0}; // 0 = not shadowed, otherwise target session ID
    WTS_SHADOW_MODE shadowMode{WTS_SHADOW_DISABLE};

    ClientDisplayMetrics display{};
    uint64_t connectTimeUs{0};
    uint64_t lastInputTimeUs{0};
    uint64_t incomingBytes{0};
    uint64_t outgoingBytes{0};

    // Virtual Channels bound to this session
    std::unordered_map<std::string, VirtualChannel> virtualChannels;
};

// ============================================================================
// RemoteDesktopSubsystem: Singleton Engine
// ============================================================================
class RemoteDesktopSubsystem {
private:
    std::mutex m_mutex;
    bool m_initialized{false};
    uint32_t m_nextSessionId{2}; // 0 = Services, 1 = Console, 2+ = RDP sessions
    uint32_t m_maxConcurrentSessions{64};
    bool m_nlaEnforced{true};

    // Session Table: SessionId -> RdpSession
    std::map<uint32_t, RdpSession> m_sessions;

    // Metrics
    std::atomic<uint64_t> m_totalConnectionsHandled{0};
    std::atomic<uint64_t> m_virtualChannelPacketsRouted{0};
    std::atomic<uint64_t> m_shadowSessionsStarted{0};
    std::atomic<uint64_t> m_nlaHandshakesPassed{0};

    RemoteDesktopSubsystem() {
        initializeDefaults();
    }

public:
    static RemoteDesktopSubsystem& instance() {
        static RemoteDesktopSubsystem s_inst;
        return s_inst;
    }

    void initializeDefaults() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_initialized) return;

        // Session 0: Non-interactive Services Session
        RdpSession sess0{};
        sess0.sessionId = 0;
        sess0.winStationName = L"Services";
        sess0.userName = L"SYSTEM";
        sess0.domainName = L"NT AUTHORITY";
        sess0.clientName = L"";
        sess0.state = WTSConnected;
        sess0.protocolType = WTS_PROTOCOL_TYPE_CONSOLE;
        sess0.handshakeState = RdpHandshakeState::FinalizedSession;
        sess0.nlaAuthenticated = true;
        sess0.isConsoleSession = false;
        m_sessions[0] = sess0;

        // Session 1: Interactive Physical Console Session
        RdpSession sess1{};
        sess1.sessionId = 1;
        sess1.winStationName = L"Console";
        sess1.userName = L"Administrator";
        sess1.domainName = L"MICANT";
        sess1.clientName = L"LOCAL";
        sess1.state = WTSActive;
        sess1.protocolType = WTS_PROTOCOL_TYPE_CONSOLE;
        sess1.handshakeState = RdpHandshakeState::FinalizedSession;
        sess1.nlaAuthenticated = true;
        sess1.isConsoleSession = true;
        sess1.display.width = 1920;
        sess1.display.height = 1080;
        sess1.display.colorDepth = 32;
        m_sessions[1] = sess1;

        // Register SCM services
        registerScmServices();

        // Register VersionDatabase
        registerVersionDatabase();

        m_initialized = true;
    }

    void registerScmServices() {
        auto& scm = micant::scm::ServiceControlManager::get();

        // 1. TermService (Remote Desktop Services)
        auto recTerm = std::make_shared<micant::scm::ServiceRecord>();
        recTerm->serviceName = L"TermService";
        recTerm->displayName = L"Remote Desktop Services";
        recTerm->serviceType = micant::scm::SERVICE_WIN32_SHARE_PROCESS;
        recTerm->startType = micant::scm::SERVICE_AUTO_START;
        recTerm->errorControl = micant::scm::SERVICE_ERROR_NORMAL;
        recTerm->binaryPath = L"C:\\Windows\\System32\\termsrv.dll";
        recTerm->status.dwServiceType = micant::scm::SERVICE_WIN32_SHARE_PROCESS;
        recTerm->status.dwCurrentState = micant::scm::SERVICE_RUNNING;
        recTerm->status.dwControlsAccepted = micant::scm::SERVICE_ACCEPT_STOP | micant::scm::SERVICE_ACCEPT_SHUTDOWN;
        scm.registerServiceRecord(recTerm);

        // 2. SessionEnv (Remote Desktop Configuration Service)
        auto recEnv = std::make_shared<micant::scm::ServiceRecord>();
        recEnv->serviceName = L"SessionEnv";
        recEnv->displayName = L"Remote Desktop Configuration";
        recEnv->serviceType = micant::scm::SERVICE_WIN32_SHARE_PROCESS;
        recEnv->startType = micant::scm::SERVICE_DEMAND_START;
        recEnv->errorControl = micant::scm::SERVICE_ERROR_NORMAL;
        recEnv->binaryPath = L"C:\\Windows\\System32\\sessenv.dll";
        recEnv->status.dwServiceType = micant::scm::SERVICE_WIN32_SHARE_PROCESS;
        recEnv->status.dwCurrentState = micant::scm::SERVICE_RUNNING;
        scm.registerServiceRecord(recEnv);

        // 3. UmRdpService (Remote Desktop Device Redirector Bus)
        auto recBus = std::make_shared<micant::scm::ServiceRecord>();
        recBus->serviceName = L"UmRdpService";
        recBus->displayName = L"Remote Desktop Device Redirector Bus";
        recBus->serviceType = micant::scm::SERVICE_KERNEL_DRIVER;
        recBus->startType = micant::scm::SERVICE_DEMAND_START;
        recBus->errorControl = micant::scm::SERVICE_ERROR_NORMAL;
        recBus->binaryPath = L"C:\\Windows\\System32\\drivers\\rdpdr.sys";
        recBus->status.dwServiceType = micant::scm::SERVICE_KERNEL_DRIVER;
        recBus->status.dwCurrentState = micant::scm::SERVICE_RUNNING;
        scm.registerServiceRecord(recBus);
    }

    void registerVersionDatabase() {
        auto& db = micant::version::VersionDatabase::Instance();
        db.RegisterModule("termsrv.dll", "10.0.26100.1", "Remote Desktop Services Engine");
        db.RegisterModule("wtsapi32.dll", "10.0.26100.1", "Windows Remote Desktop Session API");
        db.RegisterModule("rdpdr.sys", "10.0.26100.1", "Remote Desktop Device Redirector Driver");
        db.RegisterModule("mstsc.exe", "10.0.26100.1", "Remote Desktop Connection Client");
        db.RegisterModule("rdpcorets.dll", "10.0.26100.1", "RDP Core Transport Stack Subsystem");
    }

    // Connection & Handshake Engine
    bool initiateRdpConnection(const std::wstring& clientName, const std::string& clientIp,
                               const std::wstring& userName, const std::wstring& domainName,
                               const ClientDisplayMetrics& display, bool requestNla, uint32_t* outSessionId) {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_nlaEnforced && !requestNla) {
            return false; // Denied by NLA policy
        }

        if (m_sessions.size() >= m_maxConcurrentSessions) {
            return false; // Max sessions reached
        }

        uint32_t sid = m_nextSessionId++;
        RdpSession sess{};
        sess.sessionId = sid;
        sess.winStationName = L"RDP-Tcp#" + std::to_wstring(sid);
        sess.clientName = clientName;
        sess.clientIp = clientIp;
        sess.userName = userName;
        sess.domainName = domainName;
        sess.display = display;
        sess.protocolType = WTS_PROTOCOL_TYPE_RDP;
        sess.nlaAuthenticated = requestNla;

        // Perform connection sequence steps
        sess.handshakeState = RdpHandshakeState::X224_Negotiation;
        sess.handshakeState = RdpHandshakeState::McsDomainConnect;
        sess.handshakeState = RdpHandshakeState::SecurityExchange;
        sess.handshakeState = RdpHandshakeState::ClientInfoPdu;
        sess.handshakeState = RdpHandshakeState::CapabilityExchange;
        sess.handshakeState = RdpHandshakeState::ChannelJoin;

        // Open default standard static virtual channels
        bindVirtualChannel(sess, "cliprdr", 1004);
        bindVirtualChannel(sess, "rdpsnd", 1005);
        bindVirtualChannel(sess, "rdpdr", 1006);
        bindVirtualChannel(sess, "rdpgfx", 1007);

        sess.handshakeState = RdpHandshakeState::FinalizedSession;
        sess.state = WTSActive;
        sess.connectTimeUs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        sess.lastInputTimeUs = sess.connectTimeUs;

        m_sessions[sid] = sess;
        m_totalConnectionsHandled.fetch_add(1, std::memory_order_relaxed);
        if (requestNla) m_nlaHandshakesPassed.fetch_add(1, std::memory_order_relaxed);

        if (outSessionId) *outSessionId = sid;
        return true;
    }

    void bindVirtualChannel(RdpSession& sess, const std::string& chName, uint16_t chId) {
        VirtualChannel vc;
        vc.channelName = chName;
        vc.channelId = chId;
        vc.channelFlags = 0;
        sess.virtualChannels[chName] = vc;
    }

    // Session Management
    bool disconnectSession(uint32_t sessionId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessions.find(sessionId);
        if (it == m_sessions.end()) return false;
        if (it->second.isConsoleSession) return false;

        it->second.state = WTSDisconnected;
        return true;
    }

    bool logoffSession(uint32_t sessionId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessions.find(sessionId);
        if (it == m_sessions.end()) return false;
        if (it->second.isConsoleSession) return false;

        m_sessions.erase(it);
        return true;
    }

    bool getSession(uint32_t sessionId, RdpSession* outSession) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessions.find(sessionId);
        if (it == m_sessions.end()) return false;
        if (outSession) *outSession = it->second;
        return true;
    }

    std::vector<RdpSession> getAllSessions() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<RdpSession> list;
        list.reserve(m_sessions.size());
        for (const auto& [id, s] : m_sessions) {
            list.push_back(s);
        }
        return list;
    }

    // Remote Shadowing Engine
    bool startShadowSession(uint32_t clientSessionId, uint32_t targetSessionId, WTS_SHADOW_MODE mode) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto itClient = m_sessions.find(clientSessionId);
        auto itTarget = m_sessions.find(targetSessionId);
        if (itClient == m_sessions.end() || itTarget == m_sessions.end()) return false;
        if (clientSessionId == targetSessionId) return false;

        itClient->second.state = WTSShadow;
        itClient->second.shadowSessionId = targetSessionId;
        itClient->second.shadowMode = mode;
        m_shadowSessionsStarted.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    bool stopShadowSession(uint32_t clientSessionId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessions.find(clientSessionId);
        if (it == m_sessions.end()) return false;
        if (it->second.state != WTSShadow) return false;

        it->second.state = WTSActive;
        it->second.shadowSessionId = 0;
        it->second.shadowMode = WTS_SHADOW_DISABLE;
        return true;
    }

    // Virtual Channel I/O
    bool writeVirtualChannel(uint32_t sessionId, const std::string& chName, const uint8_t* data, size_t length) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessions.find(sessionId);
        if (it == m_sessions.end()) return false;

        auto chIt = it->second.virtualChannels.find(chName);
        if (chIt == it->second.virtualChannels.end()) return false;

        std::vector<uint8_t> packet(data, data + length);
        chIt->second.outboundQueue.push_back(packet);
        chIt->second.totalBytesSent += length;
        it->second.outgoingBytes += length;
        m_virtualChannelPacketsRouted.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    bool readVirtualChannel(uint32_t sessionId, const std::string& chName, std::vector<uint8_t>& outPacket) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessions.find(sessionId);
        if (it == m_sessions.end()) return false;

        auto chIt = it->second.virtualChannels.find(chName);
        if (chIt == it->second.virtualChannels.end()) return false;

        if (chIt->second.outboundQueue.empty()) return false;

        outPacket = chIt->second.outboundQueue.front();
        chIt->second.outboundQueue.pop_front();
        chIt->second.totalBytesReceived += outPacket.size();
        it->second.incomingBytes += outPacket.size();
        return true;
    }

    // Metrics & Settings
    void setNlaEnforced(bool enforced) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_nlaEnforced = enforced;
    }

    bool isNlaEnforced() const {
        return m_nlaEnforced;
    }

    uint64_t getTotalConnectionsHandled() const { return m_totalConnectionsHandled.load(std::memory_order_relaxed); }
    uint64_t getVirtualChannelPacketsRouted() const { return m_virtualChannelPacketsRouted.load(std::memory_order_relaxed); }
    uint64_t getShadowSessionsStarted() const { return m_shadowSessionsStarted.load(std::memory_order_relaxed); }
    uint64_t getNlaHandshakesPassed() const { return m_nlaHandshakesPassed.load(std::memory_order_relaxed); }
};

// ============================================================================
// Clean-Room Win32 C ABI Parity Exports (`wtsapi32.dll` & `termsrv.dll`)
// ============================================================================
extern "C" {

struct MicaChannelHandleContext {
    uint32_t sessionId;
    std::string channelName;
};

inline void* WINAPI MicaWTSOpenServerW(const wchar_t* pServerName) {
    (void)pServerName;
    return reinterpret_cast<void*>(0x1337BEEF); // Synthetic Server Handle
}

inline void WINAPI MicaWTSCloseServer(void* hServer) {
    (void)hServer;
}

inline int32_t WINAPI MicaWTSEnumerateSessionsW(void* hServer, uint32_t Reserved, uint32_t Version,
                                              WTS_SESSION_INFOW** ppSessionInfo, uint32_t* pCount) {
    (void)hServer;
    (void)Reserved;
    (void)Version;
    if (!ppSessionInfo || !pCount) return 0; // FALSE

    auto& rds = RemoteDesktopSubsystem::instance();
    auto list = rds.getAllSessions();

    *pCount = static_cast<uint32_t>(list.size());
    if (list.empty()) {
        *ppSessionInfo = nullptr;
        return 1;
    }

    size_t structBytes = sizeof(WTS_SESSION_INFOW) * list.size();
    size_t stringBytes = 0;
    for (const auto& s : list) {
        stringBytes += (s.winStationName.size() + 1) * sizeof(wchar_t);
    }

    uint8_t* pBuffer = new uint8_t[structBytes + stringBytes];
    auto* arr = reinterpret_cast<WTS_SESSION_INFOW*>(pBuffer);
    auto* pStr = reinterpret_cast<wchar_t*>(pBuffer + structBytes);

    for (size_t i = 0; i < list.size(); ++i) {
        arr[i].SessionId = list[i].sessionId;
        arr[i].State = list[i].state;
        size_t cch = list[i].winStationName.size() + 1;
        arr[i].pWinStationName = pStr;
        std::memcpy(pStr, list[i].winStationName.c_str(), cch * sizeof(wchar_t));
        pStr += cch;
    }

    *ppSessionInfo = arr;
    return 1; // TRUE
}

inline void WINAPI MicaWTSFreeMemory(void* pMemory) {
    if (!pMemory) return;
    delete[] reinterpret_cast<uint8_t*>(pMemory);
}

inline void* WINAPI MicaWTSVirtualChannelOpen(void* hServer, uint32_t SessionId, const char* pVirtualName) {
    (void)hServer;
    if (!pVirtualName) return nullptr;

    auto& rds = RemoteDesktopSubsystem::instance();
    RdpSession sess{};
    if (!rds.getSession(SessionId, &sess)) return nullptr;

    if (sess.virtualChannels.find(pVirtualName) == sess.virtualChannels.end()) return nullptr;

    auto* ctx = new MicaChannelHandleContext{SessionId, std::string(pVirtualName)};
    return reinterpret_cast<void*>(ctx);
}

inline int32_t WINAPI MicaWTSVirtualChannelWrite(void* hChannelHandle, const char* Buffer, uint32_t Length, uint32_t* pBytesWritten) {
    if (!hChannelHandle || !Buffer) return 0;

    auto* ctx = reinterpret_cast<MicaChannelHandleContext*>(hChannelHandle);
    auto& rds = RemoteDesktopSubsystem::instance();
    bool ok = rds.writeVirtualChannel(ctx->sessionId, ctx->channelName, reinterpret_cast<const uint8_t*>(Buffer), Length);
    if (ok && pBytesWritten) *pBytesWritten = Length;
    return ok ? 1 : 0;
}

inline int32_t WINAPI MicaWTSVirtualChannelRead(void* hChannelHandle, uint32_t TimeOut, char* Buffer, uint32_t BufferSize, uint32_t* pBytesRead) {
    (void)TimeOut;
    if (!hChannelHandle || !Buffer) return 0;

    auto* ctx = reinterpret_cast<MicaChannelHandleContext*>(hChannelHandle);
    auto& rds = RemoteDesktopSubsystem::instance();
    std::vector<uint8_t> pkt;
    if (!rds.readVirtualChannel(ctx->sessionId, ctx->channelName, pkt)) return 0;

    size_t copySz = std::min(static_cast<size_t>(BufferSize), pkt.size());
    std::memcpy(Buffer, pkt.data(), copySz);
    if (pBytesRead) *pBytesRead = static_cast<uint32_t>(copySz);
    return 1;
}

inline int32_t WINAPI MicaWTSVirtualChannelClose(void* hChannelHandle) {
    if (!hChannelHandle) return 0;
    delete reinterpret_cast<MicaChannelHandleContext*>(hChannelHandle);
    return 1; // TRUE
}

inline int32_t WINAPI MicaWTSDisconnectSession(void* hServer, uint32_t SessionId, int32_t bWait) {
    (void)hServer;
    (void)bWait;
    auto& rds = RemoteDesktopSubsystem::instance();
    return rds.disconnectSession(SessionId) ? 1 : 0;
}

inline int32_t WINAPI MicaWTSLogoffSession(void* hServer, uint32_t SessionId, int32_t bWait) {
    (void)hServer;
    (void)bWait;
    auto& rds = RemoteDesktopSubsystem::instance();
    return rds.logoffSession(SessionId) ? 1 : 0;
}

} // extern "C"

inline void RegisterRemoteDesktopSubsystem() {
    RemoteDesktopSubsystem::instance();
}

} // namespace micant::rds

#endif // MICANT_REMOTEDESKTOP_HPP
