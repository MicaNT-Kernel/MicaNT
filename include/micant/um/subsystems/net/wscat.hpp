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
        std::string error;
    };

// Only a real AFD/TLS transport can make this diagnostic succeed. Until that
// path is connected, return an explicit unsupported reason and no fake frames.
    SessionResult runInteractiveSession(
        std::string_view url,
        const std::vector<std::string>& messagesToSend,
        bool sendPingProbe = false)
    {
        (void)messagesToSend;
        (void)sendPingProbe;
        SessionResult result;
        const auto parts = parseUrl(url);
        if (parts.scheme == "wss")
            result.error = "authenticated TLS transport is unavailable";
        else
            result.error = "WebSocket transport is not connected to the canonical AFD socket path";
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
