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
#include <deque>
#include <span>

#include "ntstatus.hpp"
#include "scm.hpp"
#include "version.hpp"
#include "ldr.hpp"

namespace micant::rdp {

// ============================================================================
// 1. MS-RDPBCGR Protocol Constants & Wire Enums
// ============================================================================

inline constexpr uint16_t RDP_DEFAULT_PORT = 3389;

// TPKT Layer (RFC 1006 / ITU-T T.123)
inline constexpr uint8_t TPKT_VERSION = 3;

// X.224 Connection-Oriented Transport Protocol (ISO 8073)
inline constexpr uint8_t X224_TPDU_CR   = 0xE0; // Connection Request
inline constexpr uint8_t X224_TPDU_CC   = 0xD0; // Connection Confirm
inline constexpr uint8_t X224_TPDU_DR   = 0x80; // Disconnect Request
inline constexpr uint8_t X224_TPDU_DATA = 0xF0; // Data TPDU

// RDP Negotiation Message Types (MS-RDPBCGR 2.2.1.1)
inline constexpr uint8_t RDP_NEG_REQ     = 0x01;
inline constexpr uint8_t RDP_NEG_RSP     = 0x02;
inline constexpr uint8_t RDP_NEG_FAILURE = 0x03;

// RDP Security Protocols (requestedProtocols / selectedProtocol)
inline constexpr uint32_t PROTOCOL_RDP       = 0x00000000; // Standard RDP Security (RC4)
inline constexpr uint32_t PROTOCOL_SSL       = 0x00000001; // TLS 1.2 / TLS 1.3
inline constexpr uint32_t PROTOCOL_HYBRID    = 0x00000002; // CredSSP / Network Level Authentication (NLA)
inline constexpr uint32_t PROTOCOL_RDSTLS    = 0x00000004; // Remote Desktop Server TLS
inline constexpr uint32_t PROTOCOL_HYBRID_EX = 0x00000008; // Early User Authorization Result PDU

// RDP Negotiation Flags & Failure Codes
inline constexpr uint8_t EXTENDED_CLIENT_DATA_SUPPORTED = 0x01;
inline constexpr uint8_t RESTRICTED_ADMIN_MODE_SUPPORTED= 0x02;

inline constexpr uint32_t SSL_NOT_ALLOWED_BY_SERVER               = 0x00000001;
inline constexpr uint32_t SSL_CERT_NOT_ON_SERVER                  = 0x00000002;
inline constexpr uint32_t INCONSISTENT_FLAGS                      = 0x00000003;
inline constexpr uint32_t HYBRID_REQUIRED_BY_SERVER               = 0x00000004;
inline constexpr uint32_t SSL_WITH_USER_AUTH_REQUIRED_BY_SERVER  = 0x00000005;

// MCS (Multipoint Communication Service - ITU-T T.125)
inline constexpr uint8_t MCS_CONNECT_INITIAL   = 0x65;
inline constexpr uint8_t MCS_CONNECT_RESPONSE  = 0x66;
inline constexpr uint8_t MCS_ERECT_DOMAIN_REQ  = 0x04;
inline constexpr uint8_t MCS_ATTACH_USER_REQ   = 0x28;
inline constexpr uint8_t MCS_ATTACH_USER_CFM   = 0x2C;
inline constexpr uint8_t MCS_CHANNEL_JOIN_REQ  = 0x38;
inline constexpr uint8_t MCS_CHANNEL_JOIN_CFM  = 0x3C;

// RDP Fast-Path Output Update Types (MS-RDPBCGR 2.2.9.1.2)
inline constexpr uint8_t FASTPATH_UPDATETYPE_ORDERS       = 0x0;
inline constexpr uint8_t FASTPATH_UPDATETYPE_BITMAP       = 0x1;
inline constexpr uint8_t FASTPATH_UPDATETYPE_PALETTE      = 0x2;
inline constexpr uint8_t FASTPATH_UPDATETYPE_SYNCHRONIZE  = 0x3;
inline constexpr uint8_t FASTPATH_UPDATETYPE_SURFCMDS     = 0x4;
inline constexpr uint8_t FASTPATH_UPDATETYPE_PTR_NULL     = 0x5;
inline constexpr uint8_t FASTPATH_UPDATETYPE_PTR_DEFAULT  = 0x6;
inline constexpr uint8_t FASTPATH_UPDATETYPE_PTR_POSITION = 0x8;
inline constexpr uint8_t FASTPATH_UPDATETYPE_COLOR        = 0x9;
inline constexpr uint8_t FASTPATH_UPDATETYPE_CACHED       = 0xA;
inline constexpr uint8_t FASTPATH_UPDATETYPE_POINTER      = 0xB;

// RDP Fast-Path Input Event Codes (MS-RDPBCGR 2.2.8.1.2)
inline constexpr uint8_t FASTPATH_INPUT_EVENT_SCANCODE = 0x0;
inline constexpr uint8_t FASTPATH_INPUT_EVENT_MOUSE    = 0x1;
inline constexpr uint8_t FASTPATH_INPUT_EVENT_MOUSEX   = 0x2;
inline constexpr uint8_t FASTPATH_INPUT_EVENT_SYNC     = 0x3;
inline constexpr uint8_t FASTPATH_INPUT_EVENT_UNICODE  = 0x4;
inline constexpr uint8_t FASTPATH_INPUT_EVENT_QOE      = 0x6;

// Mouse Button Flags
inline constexpr uint16_t PTRFLAGS_DOWN      = 0x8000;
inline constexpr uint16_t PTRFLAGS_BUTTON1   = 0x1000; // Left Button
inline constexpr uint16_t PTRFLAGS_BUTTON2   = 0x2000; // Right Button
inline constexpr uint16_t PTRFLAGS_BUTTON3   = 0x4000; // Middle Button
inline constexpr uint16_t PTRFLAGS_WHEEL     = 0x0200; // Wheel Rotation
inline constexpr uint16_t PTRFLAGS_WHEEL_NEG = 0x0100;

// Standard Virtual Channels
inline constexpr const char* CHANNEL_CLIPRDR = "cliprdr"; // Clipboard Redirection
inline constexpr const char* CHANNEL_RDPSND  = "rdpsnd";  // Audio Output Redirection
inline constexpr const char* CHANNEL_RDPDR   = "rdpdr";   // Device Redirection (Disk, Print, Port)
inline constexpr const char* CHANNEL_RDPGFX  = "rdpgfx";  // Modern RemoteFX Graphics Pipeline
inline constexpr const char* CHANNEL_RAIL    = "rail";    // RemoteApp Integration
inline constexpr const char* CHANNEL_AUDIN   = "audin";   // Audio Input (Microphone)

// Clipboard Formats (MS-RDPECLIP)
inline constexpr uint32_t CF_RAW_TEXT           = 1;
inline constexpr uint32_t CF_RAW_BITMAP         = 2;
inline constexpr uint32_t CF_RAW_DIB            = 8;
inline constexpr uint32_t CF_RAW_UNICODETEXT    = 13;
inline constexpr uint32_t CF_RAW_HDROP          = 15;

// ============================================================================
// 2. Protocol Framing Structures & Headers
// ============================================================================

#pragma pack(push, 1)

struct TpktHeader {
    uint8_t  version{TPKT_VERSION};
    uint8_t  reserved{0};
    uint16_t length{0}; // Big-endian total length
};

struct X224CrHeader {
    uint8_t lengthIndicator{6};
    uint8_t tpduCode{X224_TPDU_CR};
    uint16_t dstRef{0};
    uint16_t srcRef{0x1234};
    uint8_t classOption{0};
};

struct X224CcHeader {
    uint8_t lengthIndicator{6};
    uint8_t tpduCode{X224_TPDU_CC};
    uint16_t dstRef{0x1234};
    uint16_t srcRef{0x5678};
    uint8_t classOption{0};
};

struct RdpNegReq {
    uint8_t  type{RDP_NEG_REQ};
    uint8_t  flags{0};
    uint16_t length{8};
    uint32_t requestedProtocols{PROTOCOL_HYBRID | PROTOCOL_SSL};
};

struct RdpNegRsp {
    uint8_t  type{RDP_NEG_RSP};
    uint8_t  flags{EXTENDED_CLIENT_DATA_SUPPORTED};
    uint16_t length{8};
    uint32_t selectedProtocol{PROTOCOL_HYBRID};
};

struct RdpNegFailure {
    uint8_t  type{RDP_NEG_FAILURE};
    uint8_t  flags{0};
    uint16_t length{8};
    uint32_t failureCode{0};
};

#pragma pack(pop)

// ============================================================================
// 3. High-Level RDP Data Models & Enums
// ============================================================================

enum class RdpSessionState : uint32_t {
    Disconnected,
    Handshaking,
    SecurityNegotiating,
    McsConnecting,
    Licensing,
    CapabilityExchanging,
    ActiveStreaming,
    Shadowing,
    LoggingOff
};

struct RdpDirtyRect {
    uint32_t x{0};
    uint32_t y{0};
    uint32_t width{0};
    uint32_t height{0};
};

struct RdpBitmapTile {
    RdpDirtyRect rect;
    uint32_t bitsPerPixel{32};
    std::vector<uint8_t> compressedBytes;
    bool isCompressed{true};
};

struct RdpInputEvent {
    uint8_t eventType{FASTPATH_INPUT_EVENT_SCANCODE};
    uint16_t scanCode{0};
    uint16_t flags{0};
    uint16_t mouseX{0};
    uint16_t mouseY{0};
    uint16_t mouseFlags{0};
    int16_t wheelDelta{0};
    wchar_t unicodeChar{0};
};

struct RdpChannelPacket {
    std::string channelName;
    uint16_t channelId{0};
    uint32_t flags{0};
    std::vector<uint8_t> payload;
};

struct RdpClientCapabilities {
    uint32_t desktopWidth{1920};
    uint32_t desktopHeight{1080};
    uint32_t colorDepth{32};
    bool supportRfx{true};
    bool supportH264{true};
    bool supportMultimon{false};
    bool supportAudioOut{true};
    bool supportAudioIn{false};
    bool supportClipboard{true};
    bool supportDriveRedirection{true};
    std::string clientBuild{"10.0.26100"};
    std::string clientName{"MSTSC-CLIENT"};
};

// ============================================================================
// 4. Enterprise RDP Session Object
// ============================================================================

class EnterpriseRdpSession {
private:
    uint32_t m_sessionId{0};
    std::wstring m_winStationName;
    std::string m_clientIp{"127.0.0.1"};
    std::string m_clientHostName{"MSTSC-LOCAL"};
    std::wstring m_userName{L"Administrator"};
    std::wstring m_domainName{L"MICANT"};
    
    RdpSessionState m_state{RdpSessionState::Disconnected};
    uint32_t m_selectedSecurityProtocol{PROTOCOL_HYBRID};
    RdpClientCapabilities m_capabilities{};
    
    uint64_t m_connectTimestampUs{0};
    uint64_t m_lastActivityTimestampUs{0};
    uint64_t m_totalBytesReceived{0};
    uint64_t m_totalBytesSent{0};
    uint64_t m_totalFramesEncoded{0};
    uint64_t m_totalInputEventsProcessed{0};

    // Virtual Channel Maps (Name -> ID and ID -> Name)
    std::map<std::string, uint16_t> m_channelNameToId;
    std::map<uint16_t, std::string> m_channelIdToName;
    std::deque<RdpChannelPacket> m_inboundChannelPackets;

    // Clipboard Mirror Cache
    std::map<uint32_t, std::vector<uint8_t>> m_clipboardCache;

public:
    EnterpriseRdpSession(uint32_t sessionId, std::wstring winStationName)
        : m_sessionId(sessionId), m_winStationName(std::move(winStationName)) {
        auto now = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        m_connectTimestampUs = now;
        m_lastActivityTimestampUs = now;
    }

    [[nodiscard]] uint32_t getSessionId() const noexcept { return m_sessionId; }
    [[nodiscard]] const std::wstring& getWinStationName() const noexcept { return m_winStationName; }
    [[nodiscard]] const std::string& getClientIp() const noexcept { return m_clientIp; }
    [[nodiscard]] const std::string& getClientHostName() const noexcept { return m_clientHostName; }
    [[nodiscard]] const std::wstring& getUserName() const noexcept { return m_userName; }
    [[nodiscard]] const std::wstring& getDomainName() const noexcept { return m_domainName; }
    [[nodiscard]] RdpSessionState getState() const noexcept { return m_state; }
    [[nodiscard]] uint32_t getSecurityProtocol() const noexcept { return m_selectedSecurityProtocol; }
    [[nodiscard]] const RdpClientCapabilities& getCapabilities() const noexcept { return m_capabilities; }

    [[nodiscard]] uint64_t getBytesReceived() const noexcept { return m_totalBytesReceived; }
    [[nodiscard]] uint64_t getBytesSent() const noexcept { return m_totalBytesSent; }
    [[nodiscard]] uint64_t getFramesEncoded() const noexcept { return m_totalFramesEncoded; }
    [[nodiscard]] uint64_t getInputEventsProcessed() const noexcept { return m_totalInputEventsProcessed; }

    void setClientInfo(std::string ip, std::string host, std::wstring user, std::wstring domain) {
        m_clientIp = std::move(ip);
        m_clientHostName = std::move(host);
        m_userName = std::move(user);
        m_domainName = std::move(domain);
    }

    void setState(RdpSessionState state) noexcept {
        m_state = state;
        touch();
    }

    void setSecurityProtocol(uint32_t protocol) noexcept {
        m_selectedSecurityProtocol = protocol;
    }

    void setCapabilities(const RdpClientCapabilities& caps) {
        m_capabilities = caps;
        touch();
    }

    void registerVirtualChannel(const std::string& channelName, uint16_t channelId) {
        m_channelNameToId[channelName] = channelId;
        m_channelIdToName[channelId] = channelName;
    }

    [[nodiscard]] bool hasVirtualChannel(const std::string& channelName) const {
        return m_channelNameToId.find(channelName) != m_channelNameToId.end();
    }

    [[nodiscard]] uint16_t getChannelId(const std::string& channelName) const {
        auto it = m_channelNameToId.find(channelName);
        return (it != m_channelNameToId.end()) ? it->second : 0;
    }

    void queueChannelPacket(RdpChannelPacket packet) {
        m_totalBytesReceived += packet.payload.size();
        m_inboundChannelPackets.push_back(std::move(packet));
        touch();
    }

    bool popChannelPacket(RdpChannelPacket& outPacket) {
        if (m_inboundChannelPackets.empty()) return false;
        outPacket = std::move(m_inboundChannelPackets.front());
        m_inboundChannelPackets.pop_front();
        return true;
    }

    void setClipboardData(uint32_t format, std::vector<uint8_t> data) {
        m_clipboardCache[format] = std::move(data);
        touch();
    }

    bool getClipboardData(uint32_t format, std::vector<uint8_t>& outData) const {
        auto it = m_clipboardCache.find(format);
        if (it != m_clipboardCache.end()) {
            outData = it->second;
            return true;
        }
        return false;
    }

    void recordFrameEncoded(uint64_t bytes) noexcept {
        m_totalFramesEncoded++;
        m_totalBytesSent += bytes;
        touch();
    }

    void recordInputProcessed() noexcept {
        m_totalInputEventsProcessed++;
        touch();
    }

    void touch() noexcept {
        m_lastActivityTimestampUs = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
};

// ============================================================================
// 5. Enterprise RDP Server Subsystem
// ============================================================================

class EnterpriseRdpServer {
private:
    std::mutex m_mutex;
    bool m_initialized{false};
    bool m_running{false};
    uint16_t m_port{RDP_DEFAULT_PORT};
    bool m_nlaEnforced{true};
    bool m_tlsEnforced{true};

    uint32_t m_nextSessionId{2}; // 0 = Services, 1 = Console, 2+ = Remote Desktop
    std::unordered_map<uint32_t, std::shared_ptr<EnterpriseRdpSession>> m_sessions;

    // Cumulative Server Statistics
    std::atomic<uint64_t> m_totalConnectionsAccepted{0};
    std::atomic<uint64_t> m_totalHandshakesCompleted{0};
    std::atomic<uint64_t> m_totalFramesSent{0};
    std::atomic<uint64_t> m_totalInputEventsHandled{0};
    std::atomic<uint64_t> m_totalVirtualChannelBytes{0};

    EnterpriseRdpServer() = default;

public:
    static EnterpriseRdpServer& get() {
        static EnterpriseRdpServer s_instance;
        return s_instance;
    }

    EnterpriseRdpServer(const EnterpriseRdpServer&) = delete;
    EnterpriseRdpServer& operator=(const EnterpriseRdpServer&) = delete;

    void initialize(uint16_t port = RDP_DEFAULT_PORT) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_initialized) return;

        m_port = port;
        m_initialized = true;
        m_running = true;

        // Pre-create Console Session 1 (interactive desktop on GOP linear framebuffer)
        auto consoleSession = std::make_shared<EnterpriseRdpSession>(1, L"Console");
        consoleSession->setClientInfo("127.0.0.1", "MicaNT-Console", L"Administrator", L"MICANT");
        consoleSession->setState(RdpSessionState::ActiveStreaming);
        RdpClientCapabilities consoleCaps;
        consoleCaps.desktopWidth = 1280;
        consoleCaps.desktopHeight = 800;
        consoleCaps.colorDepth = 32;
        consoleSession->setCapabilities(consoleCaps);
        m_sessions[1] = consoleSession;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_running = false;
        m_sessions.clear();
        m_initialized = false;
    }

    [[nodiscard]] bool isRunning() const noexcept { return m_running; }
    [[nodiscard]] uint16_t getPort() const noexcept { return m_port; }
    [[nodiscard]] bool isNlaEnforced() const noexcept { return m_nlaEnforced; }
    void setNlaEnforced(bool enforced) noexcept { m_nlaEnforced = enforced; }
    [[nodiscard]] bool isTlsEnforced() const noexcept { return m_tlsEnforced; }
    void setTlsEnforced(bool enforced) noexcept { m_tlsEnforced = enforced; }

    // Session Management
    std::shared_ptr<EnterpriseRdpSession> createSession(
        const std::string& clientIp,
        const std::string& clientHost,
        const std::wstring& user,
        const std::wstring& domain
    ) {
        std::lock_guard<std::mutex> lock(m_mutex);
        uint32_t sid = m_nextSessionId++;
        std::wstringstream wsName;
        wsName << L"RDP-Tcp#" << sid;

        auto session = std::make_shared<EnterpriseRdpSession>(sid, wsName.str());
        session->setClientInfo(clientIp, clientHost, user, domain);
        session->setState(RdpSessionState::Handshaking);

        // Register default standard virtual channels
        session->registerVirtualChannel(CHANNEL_CLIPRDR, 1001);
        session->registerVirtualChannel(CHANNEL_RDPSND,  1002);
        session->registerVirtualChannel(CHANNEL_RDPDR,   1003);
        session->registerVirtualChannel(CHANNEL_RDPGFX,  1004);
        session->registerVirtualChannel(CHANNEL_RAIL,    1005);

        m_sessions[sid] = session;
        m_totalConnectionsAccepted++;
        return session;
    }

    std::shared_ptr<EnterpriseRdpSession> getSession(uint32_t sessionId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessions.find(sessionId);
        return (it != m_sessions.end()) ? it->second : nullptr;
    }

    std::vector<std::shared_ptr<EnterpriseRdpSession>> getAllSessions() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::shared_ptr<EnterpriseRdpSession>> list;
        list.reserve(m_sessions.size());
        for (const auto& [_, s] : m_sessions) {
            list.push_back(s);
        }
        return list;
    }

    bool disconnectSession(uint32_t sessionId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessions.find(sessionId);
        if (it != m_sessions.end() && sessionId != 1) { // Do not disconnect console session
            it->second->setState(RdpSessionState::Disconnected);
            return true;
        }
        return false;
    }

    bool logoffSession(uint32_t sessionId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessions.find(sessionId);
        if (it != m_sessions.end() && sessionId != 1) {
            it->second->setState(RdpSessionState::LoggingOff);
            m_sessions.erase(it);
            return true;
        }
        return false;
    }

    // ========================================================================
    // Protocol Wire Handshake & Processing (MS-RDPBCGR)
    // ========================================================================

    /**
     * @brief Negotiate RDP Security Protocol via X.224 Connection Request
     */
    bool processConnectionRequest(
        const std::vector<uint8_t>& packet,
        std::vector<uint8_t>& outResponse,
        uint32_t& outSelectedProtocol
    ) {
        if (packet.size() < sizeof(TpktHeader) + sizeof(X224CrHeader)) {
            return false;
        }

        const auto* tpkt = reinterpret_cast<const TpktHeader*>(packet.data());
        if (tpkt->version != TPKT_VERSION) return false;

        const auto* cr = reinterpret_cast<const X224CrHeader*>(packet.data() + sizeof(TpktHeader));
        if (cr->tpduCode != X224_TPDU_CR) return false;

        // Parse optional RDP_NEG_REQ if present
        uint32_t requestedProtocols = PROTOCOL_RDP;
        size_t negOffset = sizeof(TpktHeader) + sizeof(X224CrHeader);
        if (packet.size() >= negOffset + sizeof(RdpNegReq)) {
            const auto* negReq = reinterpret_cast<const RdpNegReq*>(packet.data() + negOffset);
            if (negReq->type == RDP_NEG_REQ) {
                requestedProtocols = negReq->requestedProtocols;
            }
        }

        // Determine best security protocol
        if (m_nlaEnforced) {
            if (!(requestedProtocols & PROTOCOL_HYBRID)) {
                // Return RDP_NEG_FAILURE: HYBRID_REQUIRED_BY_SERVER
                outResponse.resize(sizeof(TpktHeader) + sizeof(X224CcHeader) + sizeof(RdpNegFailure));
                auto* outTpkt = reinterpret_cast<TpktHeader*>(outResponse.data());
                outTpkt->version = TPKT_VERSION;
                outTpkt->reserved = 0;
                outTpkt->length = static_cast<uint16_t>(outResponse.size());

                auto* outCc = reinterpret_cast<X224CcHeader*>(outResponse.data() + sizeof(TpktHeader));
                outCc->lengthIndicator = 6;
                outCc->tpduCode = X224_TPDU_CC;
                outCc->dstRef = cr->srcRef;
                outCc->srcRef = 0x5678;

                auto* fail = reinterpret_cast<RdpNegFailure*>(outResponse.data() + sizeof(TpktHeader) + sizeof(X224CcHeader));
                fail->type = RDP_NEG_FAILURE;
                fail->failureCode = HYBRID_REQUIRED_BY_SERVER;
                return false;
            }
            outSelectedProtocol = PROTOCOL_HYBRID;
        } else if (requestedProtocols & PROTOCOL_SSL) {
            outSelectedProtocol = PROTOCOL_SSL;
        } else {
            outSelectedProtocol = PROTOCOL_RDP;
        }

        // Build valid X.224 Connection Confirm + RDP_NEG_RSP
        outResponse.resize(sizeof(TpktHeader) + sizeof(X224CcHeader) + sizeof(RdpNegRsp));
        auto* outTpkt = reinterpret_cast<TpktHeader*>(outResponse.data());
        outTpkt->version = TPKT_VERSION;
        outTpkt->reserved = 0;
        outTpkt->length = static_cast<uint16_t>(outResponse.size());

        auto* outCc = reinterpret_cast<X224CcHeader*>(outResponse.data() + sizeof(TpktHeader));
        outCc->lengthIndicator = 6;
        outCc->tpduCode = X224_TPDU_CC;
        outCc->dstRef = cr->srcRef;
        outCc->srcRef = 0x5678;

        auto* rsp = reinterpret_cast<RdpNegRsp*>(outResponse.data() + sizeof(TpktHeader) + sizeof(X224CcHeader));
        rsp->type = RDP_NEG_RSP;
        rsp->flags = EXTENDED_CLIENT_DATA_SUPPORTED;
        rsp->length = 8;
        rsp->selectedProtocol = outSelectedProtocol;

        m_totalHandshakesCompleted++;
        return true;
    }

    /**
     * @brief Encode Fast-Path Bitmap Update for dirty screen rectangular region
     */
    static std::vector<uint8_t> encodeFastPathBitmapUpdate(const RdpDirtyRect& rect, const std::vector<uint32_t>& argbPixels) {
        std::vector<uint8_t> pdu;
        // Fast-Path Header (1-2 bytes: updateHeader, length)
        uint8_t fastpathHeader = (FASTPATH_UPDATETYPE_BITMAP & 0x0F);
        pdu.push_back(fastpathHeader);

        // Rect coordinates (x, y, w, h)
        pdu.resize(pdu.size() + 8);
        std::memcpy(pdu.data() + 1, &rect.x, 2);
        std::memcpy(pdu.data() + 3, &rect.y, 2);
        std::memcpy(pdu.data() + 5, &rect.width, 2);
        std::memcpy(pdu.data() + 7, &rect.height, 2);

        // Bitmap data (raw 32bpp BGRA or compressed)
        size_t pixelBytes = rect.width * rect.height * sizeof(uint32_t);
        size_t currentSize = pdu.size();
        pdu.resize(currentSize + pixelBytes);
        if (!argbPixels.empty()) {
            std::memcpy(pdu.data() + currentSize, argbPixels.data(), std::min(pixelBytes, argbPixels.size() * sizeof(uint32_t)));
        }

        return pdu;
    }

    /**
     * @brief Parse Fast-Path Input Event PDU into structured event
     */
    static bool parseFastPathInput(const std::vector<uint8_t>& packet, RdpInputEvent& outEvent) {
        if (packet.empty()) return false;

        uint8_t header = packet[0];
        uint8_t eventType = (header >> 5) & 0x07;
        outEvent.eventType = eventType;

        if (eventType == FASTPATH_INPUT_EVENT_SCANCODE && packet.size() >= 3) {
            outEvent.flags = packet[1];
            outEvent.scanCode = packet[2];
            return true;
        }

        if (eventType == FASTPATH_INPUT_EVENT_MOUSE && packet.size() >= 7) {
            std::memcpy(&outEvent.mouseFlags, packet.data() + 1, 2);
            std::memcpy(&outEvent.mouseX, packet.data() + 3, 2);
            std::memcpy(&outEvent.mouseY, packet.data() + 5, 2);
            return true;
        }

        if (eventType == FASTPATH_INPUT_EVENT_UNICODE && packet.size() >= 3) {
            std::memcpy(&outEvent.unicodeChar, packet.data() + 1, 2);
            return true;
        }

        return false;
    }

    // Cumulative Telemetry
    [[nodiscard]] uint64_t getTotalConnections() const noexcept { return m_totalConnectionsAccepted.load(); }
    [[nodiscard]] uint64_t getTotalHandshakes() const noexcept { return m_totalHandshakesCompleted.load(); }
    [[nodiscard]] uint64_t getTotalFrames() const noexcept { return m_totalFramesSent.load(); }
    [[nodiscard]] uint64_t getTotalInputs() const noexcept { return m_totalInputEventsHandled.load(); }
};

// ============================================================================
// 6. Win32 Dynamic Loader & SCM Registration
// ============================================================================

inline void RegisterRdpSubsystem() {
    static bool registered = false;
    if (registered) return;
    registered = true;

    auto& server = EnterpriseRdpServer::get();
    server.initialize(RDP_DEFAULT_PORT);

    // Register SCM Services: TermService, SessionEnv, UmRdpService
    auto& scm = scm::ServiceControlManager::get();

    // 1. TermService ("Remote Desktop Services")
    auto termRecord = std::make_shared<scm::ServiceRecord>();
    termRecord->serviceName = L"TermService";
    termRecord->displayName = L"Remote Desktop Services";
    termRecord->serviceType = scm::SERVICE_WIN32_SHARE_PROCESS;
    termRecord->startType = scm::SERVICE_AUTO_START;
    termRecord->errorControl = scm::SERVICE_ERROR_NORMAL;
    termRecord->svchostGroup = "termsvcs";
    termRecord->binaryPath = L"C:\\Windows\\System32\\svchost.exe -k termsvcs";
    termRecord->status.dwServiceType = termRecord->serviceType;
    termRecord->status.dwCurrentState = scm::SERVICE_RUNNING;
    termRecord->status.dwProcessId = scm::SvcHostManager::get().getOrCreateGroupProcess("termsvcs");
    scm::SvcHostManager::get().assignService("termsvcs", termRecord->serviceName);
    scm.registerServiceRecord(termRecord);

    // 2. SessionEnv ("Remote Desktop Configuration")
    auto envRecord = std::make_shared<scm::ServiceRecord>();
    envRecord->serviceName = L"SessionEnv";
    envRecord->displayName = L"Remote Desktop Configuration";
    envRecord->serviceType = scm::SERVICE_WIN32_SHARE_PROCESS;
    envRecord->startType = scm::SERVICE_AUTO_START;
    envRecord->errorControl = scm::SERVICE_ERROR_NORMAL;
    envRecord->svchostGroup = "netsvcs";
    envRecord->binaryPath = L"C:\\Windows\\System32\\svchost.exe -k netsvcs";
    envRecord->status.dwServiceType = envRecord->serviceType;
    envRecord->status.dwCurrentState = scm::SERVICE_RUNNING;
    envRecord->status.dwProcessId = scm::SvcHostManager::get().getOrCreateGroupProcess("netsvcs");
    scm::SvcHostManager::get().assignService("netsvcs", envRecord->serviceName);
    scm.registerServiceRecord(envRecord);

    // 3. UmRdpService ("Remote Desktop Device Redirector Configuration Service")
    auto umrdpRecord = std::make_shared<scm::ServiceRecord>();
    umrdpRecord->serviceName = L"UmRdpService";
    umrdpRecord->displayName = L"Remote Desktop Device Redirector Configuration Service";
    umrdpRecord->serviceType = scm::SERVICE_WIN32_SHARE_PROCESS;
    umrdpRecord->startType = scm::SERVICE_DEMAND_START;
    umrdpRecord->errorControl = scm::SERVICE_ERROR_NORMAL;
    umrdpRecord->svchostGroup = "LocalSystemNetworkRestricted";
    umrdpRecord->binaryPath = L"C:\\Windows\\System32\\svchost.exe -k LocalSystemNetworkRestricted";
    umrdpRecord->status.dwServiceType = umrdpRecord->serviceType;
    umrdpRecord->status.dwCurrentState = scm::SERVICE_RUNNING;
    umrdpRecord->status.dwProcessId = scm::SvcHostManager::get().getOrCreateGroupProcess("LocalSystemNetworkRestricted");
    scm::SvcHostManager::get().assignService("LocalSystemNetworkRestricted", umrdpRecord->serviceName);
    scm.registerServiceRecord(umrdpRecord);

    // Register Modules in VersionDatabase
    auto& vdb = version::VersionDatabase::Instance();
    vdb.RegisterModule("mstsc.exe", "10.0.26100.1", "Remote Desktop Connection Client");
    vdb.RegisterModule("rdpclip.exe", "10.0.26100.1", "RDP Clip Monitor");
    vdb.RegisterModule("rdpcorets.dll", "10.0.26100.1", "Remote Desktop Core Transport Services");
    vdb.RegisterModule("termsrv.dll", "10.0.26100.1", "Terminal Server Service");
    vdb.RegisterModule("wtsapi32.dll", "10.0.26100.1", "Windows Terminal Server API");
    vdb.RegisterModule("rdpsnd.dll", "10.0.26100.1", "RDP Audio Virtual Channel");
    vdb.RegisterModule("rdpdr.dll", "10.0.26100.1", "RDP Device Redirection Virtual Channel");
    vdb.RegisterModule("mstscax.dll", "10.0.26100.1", "Remote Desktop ActiveX Client Control");
}

} // namespace micant::rdp

// ============================================================================
// 7. Win32 C ABI Export Surface
// ============================================================================

extern "C" {

inline int32_t MicaRdpServerInitialize(uint16_t port) {
    micant::rdp::EnterpriseRdpServer::get().initialize(port);
    return 1;
}

inline int32_t MicaRdpServerStart() {
    return micant::rdp::EnterpriseRdpServer::get().isRunning() ? 1 : 0;
}

inline int32_t MicaRdpServerStop() {
    micant::rdp::EnterpriseRdpServer::get().shutdown();
    return 1;
}

inline uint32_t MicaRdpCreateSession(
    const char* clientIp,
    const char* clientHost,
    const wchar_t* user,
    const wchar_t* domain
) {
    if (!clientIp || !clientHost || !user || !domain) return 0;
    auto s = micant::rdp::EnterpriseRdpServer::get().createSession(clientIp, clientHost, user, domain);
    return s ? s->getSessionId() : 0;
}

inline int32_t MicaRdpDisconnectSession(uint32_t sessionId) {
    return micant::rdp::EnterpriseRdpServer::get().disconnectSession(sessionId) ? 1 : 0;
}

inline int32_t MicaRdpLogoffSession(uint32_t sessionId) {
    return micant::rdp::EnterpriseRdpServer::get().logoffSession(sessionId) ? 1 : 0;
}

inline int32_t MicaRdpGetServerStats(
    uint64_t* pTotalConnections,
    uint64_t* pTotalHandshakes,
    uint64_t* pActiveSessions
) {
    auto& server = micant::rdp::EnterpriseRdpServer::get();
    if (pTotalConnections) *pTotalConnections = server.getTotalConnections();
    if (pTotalHandshakes) *pTotalHandshakes = server.getTotalHandshakes();
    if (pActiveSessions) *pActiveSessions = server.getAllSessions().size();
    return 1;
}

inline int32_t MicaRdpServerShutdown() {
    micant::rdp::EnterpriseRdpServer::get().shutdown();
    return 1;
}

} // extern "C"
