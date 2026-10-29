#pragma once

#include <string>
#include <vector>
#include <string_view>
#include <sstream>
#include <iomanip>
#include <memory>
#include <map>
#include <span>
#include <algorithm>

#include "micant/um/servers/rdp.hpp"
#include "micant/um/subsystems/winbase/wtsapi32.hpp"

namespace micant::mstsc::um {

struct MstscConfig {
    std::string server{"147.135.1.229"};
    uint16_t port{3389};
    bool adminConsole{false};
    bool fullScreen{false};
    uint32_t width{1920};
    uint32_t height{1080};
    uint32_t shadowSessionId{0};
    bool restrictedAdmin{false};
    std::wstring user{L"Administrator"};
    std::wstring domain{L"MICANT"};
};

class MstscClientSession {
public:
    MstscClientSession() = default;

    bool connect(const MstscConfig& config) noexcept {
        config_ = config;
        auto& server = rdp::EnterpriseRdpServer::get();
        if (!server.isRunning()) {
            server.initialize(config.port);
        }

        // 1. Create Server Session
        serverSession_ = server.createSession("127.0.0.1", "MSTSC-CLIENT", config.user, config.domain);
        if (!serverSession_) return false;

        sessionId_ = serverSession_->getSessionId();

        // Configure client capabilities
        rdp::RdpClientCapabilities caps;
        caps.desktopWidth = config.width;
        caps.desktopHeight = config.height;
        caps.colorDepth = 32;
        caps.supportRfx = true;
        caps.supportH264 = true;
        caps.supportClipboard = true;
        caps.supportAudioOut = true;
        caps.clientBuild = "10.0.26100.5900";
        caps.clientName = "MSTSC-CLIENT";
        serverSession_->setCapabilities(caps);

        // 2. Step 1: X.224 Connection Request & Security Negotiation
        std::vector<uint8_t> crPdu(sizeof(rdp::TpktHeader) + sizeof(rdp::X224CrHeader) + sizeof(rdp::RdpNegReq), 0);
        auto* tpkt = reinterpret_cast<rdp::TpktHeader*>(crPdu.data());
        tpkt->version = rdp::TPKT_VERSION;
        tpkt->length = static_cast<uint16_t>(crPdu.size());

        auto* cr = reinterpret_cast<rdp::X224CrHeader*>(crPdu.data() + sizeof(rdp::TpktHeader));
        cr->lengthIndicator = 6;
        cr->tpduCode = rdp::X224_TPDU_CR;
        cr->srcRef = 0x1234;

        auto* neg = reinterpret_cast<rdp::RdpNegReq*>(crPdu.data() + sizeof(rdp::TpktHeader) + sizeof(rdp::X224CrHeader));
        neg->type = rdp::RDP_NEG_REQ;
        neg->length = 8;
        neg->requestedProtocols = rdp::PROTOCOL_HYBRID | rdp::PROTOCOL_SSL;

        std::vector<uint8_t> ccResp;
        uint32_t selectedProtocol = 0;
        if (!server.processConnectionRequest(crPdu, ccResp, selectedProtocol)) {
            return false;
        }
        negotiatedProtocol_ = selectedProtocol;
        serverSession_->setSecurityProtocol(selectedProtocol);

        // 3. Step 2: MCS Connect Initial & Response
        std::vector<uint8_t> mcsReq; // Mocked client Connect-Initial packet
        std::vector<uint8_t> mcsResp;
        uint16_t ioChannel = 0, bcastChannel = 0;
        if (!server.processMcsConnectInitial(mcsReq, mcsResp, ioChannel, bcastChannel)) {
            return false;
        }
        ioChannel_ = ioChannel;

        // 4. Step 3: MCS Attach User
        std::vector<uint8_t> attachResp;
        uint16_t userId = 0;
        if (!server.processAttachUser(sessionId_, attachResp, userId)) {
            return false;
        }
        userId_ = userId;

        // 5. Step 4: MCS Channel Joins (User Channel + Virtual Channels)
        std::vector<uint8_t> joinResp;
        server.processChannelJoin(userId_, userId_, joinResp);
        server.processChannelJoin(userId_, ioChannel_, joinResp);
        server.processChannelJoin(userId_, 1001 /* cliprdr */, joinResp);
        server.processChannelJoin(userId_, 1002 /* rdpsnd */, joinResp);
        server.processChannelJoin(userId_, 1003 /* rdpdr */, joinResp);

        // 6. Step 5: Capability Exchange (Demand Active PDU -> Confirm Active PDU)
        std::vector<uint8_t> demandPdu;
        if (!server.buildDemandActivePdu(sessionId_, demandPdu)) {
            return false;
        }

        std::vector<uint8_t> confirmPdu = { 0x00, 0x13 }; // Mock Confirm Active PDU
        if (!server.processConfirmActivePdu(sessionId_, confirmPdu)) {
            return false;
        }

        connected_ = true;
        return true;
    }

    bool disconnect() noexcept {
        if (!connected_) return false;
        auto& server = rdp::EnterpriseRdpServer::get();
        server.disconnectSession(sessionId_);
        connected_ = false;
        return true;
    }

    // Input Injection
    bool sendKey(uint8_t scancode, bool down) noexcept {
        if (!connected_ || !serverSession_) return false;
        rdp::RdpInputEvent ev;
        ev.eventType = rdp::FASTPATH_INPUT_EVENT_SCANCODE;
        ev.scanCode = scancode;
        ev.flags = down ? 0 : 0x01; // 0x01 = release
        serverSession_->recordInputProcessed();
        return true;
    }

    bool sendMouse(uint16_t x, uint16_t y, uint16_t flags) noexcept {
        if (!connected_ || !serverSession_) return false;
        rdp::RdpInputEvent ev;
        ev.eventType = rdp::FASTPATH_INPUT_EVENT_MOUSE;
        ev.mouseX = x;
        ev.mouseY = y;
        ev.mouseFlags = flags;
        serverSession_->recordInputProcessed();
        return true;
    }

    // Virtual Channel Operations
    bool sendClipboardText(const std::wstring& text) noexcept {
        if (!connected_ || !serverSession_) return false;
        std::vector<uint8_t> data(text.size() * sizeof(wchar_t));
        std::memcpy(data.data(), text.data(), data.size());
        serverSession_->setClipboardData(rdp::CF_RAW_UNICODETEXT, data);
        auto& server = rdp::EnterpriseRdpServer::get();
        return server.routeVirtualChannelPacket(sessionId_, rdp::CHANNEL_CLIPRDR, data);
    }

    bool receiveClipboardText(std::wstring& outText) noexcept {
        if (!connected_ || !serverSession_) return false;
        std::vector<uint8_t> data;
        if (!serverSession_->getClipboardData(rdp::CF_RAW_UNICODETEXT, data)) return false;
        outText.assign(reinterpret_cast<const wchar_t*>(data.data()), data.size() / sizeof(wchar_t));
        return true;
    }

    [[nodiscard]] bool isConnected() const noexcept { return connected_; }
    [[nodiscard]] uint32_t getSessionId() const noexcept { return sessionId_; }
    [[nodiscard]] uint32_t getNegotiatedProtocol() const noexcept { return negotiatedProtocol_; }
    [[nodiscard]] uint16_t getUserId() const noexcept { return userId_; }
    [[nodiscard]] uint16_t getIoChannel() const noexcept { return ioChannel_; }

private:
    MstscConfig config_{};
    bool connected_{false};
    uint32_t sessionId_{0};
    uint32_t negotiatedProtocol_{0};
    uint16_t userId_{0};
    uint16_t ioChannel_{0};
    std::shared_ptr<rdp::EnterpriseRdpSession> serverSession_;
};

// ============================================================================
// Canonical mstsc.exe Command Line Executor
// ============================================================================

class MstscCommandExecutor {
public:
    static MstscCommandExecutor& Instance() noexcept {
        static MstscCommandExecutor s_instance;
        return s_instance;
    }

    int execute(std::span<const std::string_view> args, std::string& outStr) noexcept {
        if (args.empty()) {
            return showUsage(outStr);
        }

        MstscConfig cfg;
        for (const auto& arg : args) {
            if (arg == "/?" || arg == "-?" || arg == "/help") {
                return showUsage(outStr);
            } else if (arg.starts_with("/v:") || arg.starts_with("-v:")) {
                auto val = arg.substr(3);
                auto colon = val.find(':');
                if (colon != std::string_view::npos) {
                    cfg.server = std::string(val.substr(0, colon));
                    cfg.port = static_cast<uint16_t>(std::atoi(std::string(val.substr(colon + 1)).c_str()));
                } else {
                    cfg.server = std::string(val);
                    cfg.port = 3389;
                }
            } else if (arg == "/admin" || arg == "/console") {
                cfg.adminConsole = true;
            } else if (arg == "/f") {
                cfg.fullScreen = true;
            } else if (arg.starts_with("/w:") || arg.starts_with("-w:")) {
                cfg.width = static_cast<uint32_t>(std::atoi(std::string(arg.substr(3)).c_str()));
            } else if (arg.starts_with("/h:") || arg.starts_with("-h:")) {
                cfg.height = static_cast<uint32_t>(std::atoi(std::string(arg.substr(3)).c_str()));
            } else if (arg.starts_with("/shadow:") || arg.starts_with("-shadow:")) {
                cfg.shadowSessionId = static_cast<uint32_t>(std::atoi(std::string(arg.substr(8)).c_str()));
            } else if (arg == "/restrictedAdmin") {
                cfg.restrictedAdmin = true;
            }
        }

        MstscClientSession client;
        if (!client.connect(cfg)) {
            outStr = "Remote Desktop Connection failed to establish a secure session with " + cfg.server + ".\n";
            return 1;
        }

        std::ostringstream ss;
        ss << "Connecting to " << cfg.server << ":" << cfg.port << " ...\n";
        ss << "Security Protocol: " << (client.getNegotiatedProtocol() == rdp::PROTOCOL_HYBRID ? "CredSSP / NLA" : "TLS 1.3") << "\n";
        ss << "Remote Desktop Session #" << client.getSessionId() << " established successfully (" << cfg.width << "x" << cfg.height << " 32bpp).\n";
        outStr = ss.str();
        return 0;
    }

private:
    MstscCommandExecutor() = default;

    int showUsage(std::string& out) noexcept {
        out = "MSTSC [<connection file>] [/v:<server[:port]>] [/admin] [/f[ullscreen]]\n"
              "      [/w:<width>] [/h:<height>] [/shadow:<sessionID>] [/restrictedAdmin]\n\n"
              "  /v:<server[:port]>  Specifies the remote computer to connect to.\n"
              "  /admin              Connects you to the session for administering a server.\n"
              "  /f                  Starts Remote Desktop in full-screen mode.\n"
              "  /w:<width>          Specifies the width of the Remote Desktop window.\n"
              "  /h:<height>         Specifies the height of the Remote Desktop window.\n"
              "  /shadow:<sessionID> Specifies the sessionID to shadow.\n"
              "  /restrictedAdmin    Connects you to the remote computer in Restricted Admin mode.\n";
        return 0;
    }
};

// ============================================================================
// Canonical query.exe and reset.exe Session Management CLI
// ============================================================================

class SessionCommandExecutor {
public:
    static SessionCommandExecutor& Instance() noexcept {
        static SessionCommandExecutor s_instance;
        return s_instance;
    }

    int executeQuery(std::span<const std::string_view> args, std::string& outStr) noexcept {
        if (args.empty() || args[0] == "session") {
            return querySession(outStr);
        } else if (args[0] == "user") {
            return queryUser(outStr);
        } else {
            outStr = "Unknown query target. Valid targets: query session, query user\n";
            return 1;
        }
    }

    int executeReset(std::span<const std::string_view> args, std::string& outStr) noexcept {
        // Syntax: reset session <sessionname | sessionID>
        if (args.empty()) {
            outStr = "Reset a session.\n\nRESET SESSION {sessionname | sessionid}\n";
            return 1;
        }

        std::string_view target = (args[0] == "session" && args.size() > 1) ? args[1] : args[0];
        uint32_t sid = static_cast<uint32_t>(std::atoi(std::string(target).c_str()));
        if (sid == 0 && target != "0") {
            outStr = "Invalid session ID specified.\n";
            return 1;
        }

        if (wts::um::WTSLogoffSession(wts::um::WTS_CURRENT_SERVER_HANDLE, sid, 1)) {
            outStr = "Session " + std::string(target) + " has been reset successfully.\n";
            return 0;
        } else {
            outStr = "Could not reset session ID " + std::string(target) + ". Error 7022.\n";
            return 2;
        }
    }

private:
    SessionCommandExecutor() = default;

    int querySession(std::string& out) noexcept {
        wts::um::PWTS_SESSION_INFOW pSessionInfo = nullptr;
        wts::um::DWORD count = 0;

        if (!wts::um::WTSEnumerateSessionsW(wts::um::WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSessionInfo, &count)) {
            out = "Failed to enumerate sessions.\n";
            return 1;
        }

        std::ostringstream ss;
        ss << " SESSIONNAME       USERNAME                 ID  STATE    TYPE        DEVICE \n";
        for (wts::um::DWORD i = 0; i < count; ++i) {
            std::wstring wsName = pSessionInfo[i].pWinStationName ? pSessionInfo[i].pWinStationName : L"";
            std::string ws(wsName.begin(), wsName.end());

            wts::um::LPWSTR pUser = nullptr;
            wts::um::DWORD uBytes = 0;
            std::string user = "";
            if (wts::um::WTSQuerySessionInformationW(wts::um::WTS_CURRENT_SERVER_HANDLE, pSessionInfo[i].SessionId, wts::um::WTSUserName, &pUser, &uBytes) && pUser) {
                std::wstring uws(pUser);
                user.assign(uws.begin(), uws.end());
                wts::um::WTSFreeMemory(pUser);
            }

            std::string stateStr = (pSessionInfo[i].State == wts::um::WTSActive) ? "Active" :
                                   (pSessionInfo[i].State == wts::um::WTSDisconnected) ? "Disc" : "Conn";

            ss << " " << std::left << std::setw(17) << ws
               << std::left << std::setw(25) << user
               << std::right << std::setw(2) << pSessionInfo[i].SessionId << "  "
               << std::left << std::setw(9) << stateStr
               << "\n";

            if (pSessionInfo[i].pWinStationName) {
                wts::um::WTSFreeMemory(pSessionInfo[i].pWinStationName);
            }
        }
        wts::um::WTSFreeMemory(pSessionInfo);

        out = ss.str();
        return 0;
    }

    int queryUser(std::string& out) noexcept {
        wts::um::PWTS_SESSION_INFOW pSessionInfo = nullptr;
        wts::um::DWORD count = 0;

        if (!wts::um::WTSEnumerateSessionsW(wts::um::WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSessionInfo, &count)) {
            out = "Failed to enumerate users.\n";
            return 1;
        }

        std::ostringstream ss;
        ss << " USERNAME              SESSIONNAME        ID  STATE   IDLE TIME  LOGON TIME\n";
        for (wts::um::DWORD i = 0; i < count; ++i) {
            std::wstring wsName = pSessionInfo[i].pWinStationName ? pSessionInfo[i].pWinStationName : L"";
            std::string ws(wsName.begin(), wsName.end());

            wts::um::LPWSTR pUser = nullptr;
            wts::um::DWORD uBytes = 0;
            std::string user = "";
            if (wts::um::WTSQuerySessionInformationW(wts::um::WTS_CURRENT_SERVER_HANDLE, pSessionInfo[i].SessionId, wts::um::WTSUserName, &pUser, &uBytes) && pUser) {
                std::wstring uws(pUser);
                user.assign(uws.begin(), uws.end());
                wts::um::WTSFreeMemory(pUser);
            }

            if (user.empty()) continue; // Skip service sessions with no user

            std::string stateStr = (pSessionInfo[i].State == wts::um::WTSActive) ? "Active" : "Disc";

            ss << " " << std::left << std::setw(22) << user
               << std::left << std::setw(19) << ws
               << std::right << std::setw(2) << pSessionInfo[i].SessionId << "  "
               << std::left << std::setw(8) << stateStr
               << "      none  10/29/2026\n";

            if (pSessionInfo[i].pWinStationName) {
                wts::um::WTSFreeMemory(pSessionInfo[i].pWinStationName);
            }
        }
        wts::um::WTSFreeMemory(pSessionInfo);

        out = ss.str();
        return 0;
    }
};

} // namespace micant::mstsc::um
