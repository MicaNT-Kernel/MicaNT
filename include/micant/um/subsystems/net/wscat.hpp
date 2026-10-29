// ============================================================================
// MicaNT: Canonical Win32 WebSocket CLI Diagnostic Client Tool (wscat.exe)
// (wscat.hpp)
//
// Modern Clean-Room Implementation in pure ISO C++23.
// Provides command-line WebSocket connectivity to ws:// and wss:// endpoints,
// RFC 6455 framing with client masking, interactive command execution,
// ping/pong keep-alives, and VersionDatabase registration.
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <optional>
#include <sstream>
#include <atomic>

#include "micant/km/drivers/security/cng.hpp"
#include "micant/km/drivers/security/tls13.hpp"
#include "micant/km/drivers/net/https_console.hpp"
#include "micant/um/subsystems/net/websocket.hpp"
#include "micant/um/subsystems/winbase/version.hpp"

namespace micant::um::net::ws {

struct WsUrlParts {
    std::string scheme{"wss"};
    std::string host{"localhost"};
    uint16_t port{443};
    std::string path{"/api/v1/terminal"};
};

class WsCatClient {
public:
    static WsUrlParts parseUrl(std::string_view url) {
        WsUrlParts parts;
        if (url.starts_with("wss://")) {
            parts.scheme = "wss";
            url.remove_prefix(6);
            parts.port = 443;
        } else if (url.starts_with("ws://")) {
            parts.scheme = "ws";
            url.remove_prefix(5);
            parts.port = 80;
        }

        size_t slashPos = url.find('/');
        std::string_view hostPort = (slashPos != std::string_view::npos) ? url.substr(0, slashPos) : url;
        if (slashPos != std::string_view::npos) {
            parts.path = std::string(url.substr(slashPos));
        } else {
            parts.path = "/";
        }

        size_t colonPos = hostPort.find(':');
        if (colonPos != std::string_view::npos) {
            parts.host = std::string(hostPort.substr(0, colonPos));
            parts.port = static_cast<uint16_t>(std::atoi(std::string(hostPort.substr(colonPos + 1)).c_str()));
        } else {
            parts.host = std::string(hostPort);
        }

        return parts;
    }

    struct SessionResult {
        bool connected{false};
        uint32_t httpStatusCode{0};
        std::string serverAcceptKey;
        std::vector<std::string> receivedMessages;
        uint64_t framesSent{0};
        uint64_t framesReceived{0};
        bool pongReceived{false};
        bool closedCleanly{false};
    };

    // Connects, performs TLS 1.3 (if wss), upgrades to WebSocket, sends message, receives replies
    SessionResult runInteractiveSession(
        std::string_view url,
        const std::vector<std::string>& messagesToSend,
        bool sendPingProbe = false)
    {
        SessionResult result;
        auto urlParts = parseUrl(url);

        static std::atomic<int> s_wscatSockId{15650};
        int clientSockId = s_wscatSockId.fetch_add(1);

        auto& console = km::net::HttpsExecutiveConsole::get();
        km::crypto::tls13::Tls13ClientSession tlsClient;
        WebSocketSession wsSession(true); // Client mode with masking enabled

        // 1. Perform TLS 1.3 Handshake (for wss://)
        if (urlParts.scheme == "wss") {
            auto clientHello = tlsClient.generateClientHello(urlParts.host);

            std::vector<uint8_t> serverHandshake;
            if (!console.handleHandshake(clientSockId, clientHello, serverHandshake)) {
                return result;
            }
            if (!tlsClient.processServerHandshake(serverHandshake)) {
                console.closeSession(clientSockId);
                return result;
            }
        }

        // 2. Perform RFC 6455 HTTP/1.1 Upgrade Handshake
        std::string clientKey = WebSocketHandshake::generateClientKey();
        std::ostringstream upgradeReq;
        upgradeReq << "GET " << urlParts.path << " HTTP/1.1\r\n"
                   << "Host: " << urlParts.host << ":" << urlParts.port << "\r\n"
                   << "Upgrade: websocket\r\n"
                   << "Connection: Upgrade\r\n"
                   << "Sec-WebSocket-Key: " << clientKey << "\r\n"
                   << "Sec-WebSocket-Version: 13\r\n\r\n";

        std::string reqStr = upgradeReq.str();
        std::string respStr;

        if (urlParts.scheme == "wss") {
            auto encryptedReq = tlsClient.encryptAppData(
                std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(reqStr.data()), reqStr.size())
            );
            auto encryptedResp = console.handleEncryptedRequest(clientSockId, encryptedReq);

            std::vector<uint8_t> plaintextResp;
            if (!tlsClient.decryptAppData(encryptedResp, plaintextResp)) {
                console.closeSession(clientSockId);
                return result;
            }
            respStr.assign(reinterpret_cast<const char*>(plaintextResp.data()), plaintextResp.size());
        } else {
            respStr = console.dispatchHttpRequest(reqStr);
        }

        // 3. Verify HTTP 101 Switching Protocols & Sec-WebSocket-Accept
        if (respStr.find("101 Switching Protocols") != std::string::npos) {
            result.httpStatusCode = 101;
            size_t acceptPos = respStr.find("Sec-WebSocket-Accept: ");
            if (acceptPos != std::string::npos) {
                size_t acceptStart = acceptPos + 22;
                size_t acceptEnd = respStr.find("\r\n", acceptStart);
                if (acceptEnd != std::string::npos) {
                    result.serverAcceptKey = respStr.substr(acceptStart, acceptEnd - acceptStart);
                    if (WebSocketHandshake::verifyAcceptKey(clientKey, result.serverAcceptKey)) {
                        result.connected = true;
                        wsSession.markOpen();
                    }
                }
            }
        }

        if (!result.connected) {
            console.closeSession(clientSockId);
            return result;
        }

        // 4. Send Ping frame if requested
        if (sendPingProbe) {
            std::string pingData = "wscat-ping-test";
            auto pingWire = wsSession.createPingFrame(
                std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(pingData.data()), pingData.size())
            );
            result.framesSent++;

            if (urlParts.scheme == "wss") {
                auto encPing = tlsClient.encryptAppData(pingWire);
                auto encPong = console.handleEncryptedRequest(clientSockId, encPing);
                std::vector<uint8_t> ptPong;
                if (tlsClient.decryptAppData(encPong, ptPong)) {
                    std::vector<WebSocketFrame> replyFrames;
                    wsSession.processIncomingBytes(ptPong, replyFrames);
                    for (const auto& f : replyFrames) {
                        result.framesReceived++;
                        if (f.opcode == OPCODE_PONG) {
                            result.pongReceived = true;
                        }
                    }
                }
            }
        }

        // 5. Send User Messages and Collect Server Replies
        for (const auto& msg : messagesToSend) {
            auto frameWire = wsSession.createTextFrame(msg);
            result.framesSent++;

            if (urlParts.scheme == "wss") {
                auto encFrame = tlsClient.encryptAppData(frameWire);
                auto encReply = console.handleEncryptedRequest(clientSockId, encFrame);

                std::vector<uint8_t> ptReply;
                if (tlsClient.decryptAppData(encReply, ptReply)) {
                    std::vector<WebSocketFrame> replyFrames;
                    wsSession.processIncomingBytes(ptReply, replyFrames);
                    for (const auto& f : replyFrames) {
                        result.framesReceived++;
                        if (f.opcode == OPCODE_TEXT) {
                            result.receivedMessages.push_back(f.asText());
                        } else if (f.opcode == OPCODE_PONG) {
                            result.pongReceived = true;
                        }
                    }
                }
            }
        }

        // 6. Graceful Close frame
        auto closeWire = wsSession.createCloseFrame(CLOSE_NORMAL_CLOSURE, "Session finished");
        result.framesSent++;
        if (urlParts.scheme == "wss") {
            auto encClose = tlsClient.encryptAppData(closeWire);
            auto encCloseReply = console.handleEncryptedRequest(clientSockId, encClose);
            std::vector<uint8_t> ptClose;
            if (tlsClient.decryptAppData(encCloseReply, ptClose)) {
                std::vector<WebSocketFrame> closeFrames;
                wsSession.processIncomingBytes(ptClose, closeFrames);
                for (const auto& f : closeFrames) {
                    result.framesReceived++;
                    if (f.opcode == OPCODE_CLOSE) {
                        result.closedCleanly = true;
                    }
                }
            }
        }

        console.closeSession(clientSockId);
        return result;
    }

    // CLI Entry Point
    static int main(int argc, const char* argv[]) {
        if (argc < 2) {
            return 1;
        }

        std::string url;
        std::vector<std::string> messages;
        bool sendPing = false;

        for (int i = 1; i < argc; ++i) {
            std::string_view arg = argv[i];
            if ((arg == "-c" || arg == "--connect") && i + 1 < argc) {
                url = argv[++i];
            } else if ((arg == "-m" || arg == "--message") && i + 1 < argc) {
                messages.push_back(argv[++i]);
            } else if (arg == "-p" || arg == "--ping") {
                sendPing = true;
            }
        }

        if (url.empty()) return 1;

        WsCatClient client;
        auto res = client.runInteractiveSession(url, messages, sendPing);
        return res.connected ? 0 : 1;
    }
};

inline void InitializeWsCatSubsystem() {
    auto& verDb = version::VersionDatabase::Instance();
    verDb.RegisterModule("wscat.exe", "10.0.26100.5900", "MicaNT Real-Time WebSocket Diagnostics Utility");
}

} // namespace micant::um::net::ws
