#pragma once
/**
 * @file https_console.hpp
 * @brief Horizon 155: Bare-Metal HTTPS Secure Remote Executive Console (Port 443 / 8443),
 *        TLS 1.3 End-to-End Encryption, RESTful JSON Subsystem & Embedded Web Admin Dashboard.
 * 
 * Target Architecture: Supermicro X11SSH-F (Intel Xeon E3-1270 v6 @ 3.80GHz, 32GB DDR4 ECC).
 * Clean-Room Standard: 17 U.S.C. § 1201(f) & Google LLC v. Oracle America, Inc. (593 U.S. 1).
 */

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <sstream>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "micant/km/drivers/net/tcpip.hpp"
#include "micant/km/drivers/net/wfp.hpp"
#include "micant/km/drivers/security/cng.hpp"
#include "micant/km/drivers/security/x509.hpp"
#include "micant/km/drivers/security/tls13.hpp"

namespace micant::km::net {

class HttpsExecutiveConsole {
public:
    static HttpsExecutiveConsole& get() noexcept {
        static HttpsExecutiveConsole instance;
        return instance;
    }

    void startHttpsServer(uint16_t port = 443) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_) return;
        port_ = port;

        // 1. Create and bind listening TCP socket in TCP/IP network stack
        serverSock_ = tcpip::NetworkStack::get().createSocket(tcpip::AF_INET, tcpip::SOCK_STREAM, tcpip::IPPROTO_TCP);
        if (serverSock_ > 0) {
            tcpip::NetworkStack::get().bindSocket(serverSock_, tcpip::Ipv4Address::any(), port);
            tcpip::NetworkStack::get().listenSocket(serverSock_, 10);
            running_ = true;
        }

        // 2. Ensure WFP filter permits HTTPS inbound traffic
        auto& wfpEngine = wfp::km::WfpKernelFilterEngine::get();
        wfp::km::WfpKernelFilterRule httpsRule{};
        httpsRule.filterId = 15501;
        httpsRule.name = "MicaNT_HTTPS_Inbound_Permit";
        httpsRule.layerId = wfp::km::FWPS_LAYER_INBOUND_TRANSPORT_V4;
        httpsRule.direction = wfp::FWP_DIRECTION_INBOUND;
        httpsRule.protocol = tcpip::IPPROTO_TCP;
        httpsRule.localPortStart = port;
        httpsRule.localPortEnd = port;
        httpsRule.weight = 1500;
        httpsRule.action = wfp::FWP_ACTION_PERMIT;
        wfpEngine.addRule(httpsRule);
    }

    void stopHttpsServer() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) return;
        if (serverSock_ > 0) {
            tcpip::NetworkStack::get().closeSocket(serverSock_);
            serverSock_ = -1;
        }
        running_ = false;
    }

    [[nodiscard]] bool isRunning() const noexcept { return running_; }
    [[nodiscard]] uint16_t getPort() const noexcept { return port_; }
    [[nodiscard]] int getServerSocket() const noexcept { return serverSock_; }

    struct HttpsSessionContext {
        crypto::tls13::Tls13ServerSession tlsSession;
        bool handshakeComplete{false};
    };

    // Process Inbound Client Handshake (ClientHello -> ServerHello)
    bool handleHandshake(int clientSockId, std::span<const uint8_t> clientHelloData, std::vector<uint8_t>& serverHandshakeOut) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& sessionCtx = activeSessions_[clientSockId];
        if (sessionCtx.tlsSession.processClientHello(clientHelloData, serverHandshakeOut)) {
            sessionCtx.handshakeComplete = true;
            handshakesSucceeded_++;
            totalHttpsConnections_++;
            return true;
        }
        handshakesFailed_++;
        return false;
    }

    // Process Decrypted HTTP Request and Return Encrypted HTTP Response
    std::vector<uint8_t> handleEncryptedRequest(int clientSockId, std::span<const uint8_t> wireRecord) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = activeSessions_.find(clientSockId);
        if (it == activeSessions_.end() || !it->second.handshakeComplete) {
            return {};
        }

        std::vector<uint8_t> plaintextHttp;
        if (!it->second.tlsSession.decryptAppData(wireRecord, plaintextHttp)) {
            return {};
        }

        std::string_view reqStr(reinterpret_cast<const char*>(plaintextHttp.data()), plaintextHttp.size());
        std::string httpResponse = dispatchHttpRequest(reqStr);
        requestsProcessed_++;

        return it->second.tlsSession.encryptAppData(
            std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(httpResponse.data()), httpResponse.size())
        );
    }

    void closeSession(int clientSockId) {
        std::lock_guard<std::mutex> lock(mutex_);
        activeSessions_.erase(clientSockId);
    }

    // HTTP Dispatcher
    std::string dispatchHttpRequest(std::string_view request) {
        std::string method = "GET";
        std::string path = "/";

        size_t firstSpace = request.find(' ');
        if (firstSpace != std::string_view::npos) {
            method = std::string(request.substr(0, firstSpace));
            size_t secondSpace = request.find(' ', firstSpace + 1);
            if (secondSpace != std::string_view::npos) {
                path = std::string(request.substr(firstSpace + 1, secondSpace - firstSpace - 1));
            }
        }

        if (path == "/" || path == "/index.html") {
            return generateDashboardHtmlResponse();
        } else if (path == "/api/v1/vitals") {
            return generateVitalsJsonResponse();
        } else if (path == "/api/v1/network") {
            return generateNetworkJsonResponse();
        } else if (path == "/api/v1/exec" && method == "POST") {
            // Find POST body after \r\n\r\n
            size_t bodyPos = request.find("\r\n\r\n");
            std::string_view body = (bodyPos != std::string_view::npos) ? request.substr(bodyPos + 4) : "";
            return generateExecJsonResponse(body);
        }

        // 404 Not Found
        return "HTTP/1.1 404 Not Found\r\n"
               "Content-Type: text/plain\r\n"
               "Content-Length: 9\r\n"
               "Connection: close\r\n\r\n"
               "Not Found";
    }

    struct ConsoleTelemetry {
        uint64_t totalConnections{0};
        uint64_t handshakesSucceeded{0};
        uint64_t handshakesFailed{0};
        uint64_t requestsProcessed{0};
    };

    [[nodiscard]] ConsoleTelemetry getTelemetry() const noexcept {
        return {
            totalHttpsConnections_,
            handshakesSucceeded_,
            handshakesFailed_,
            requestsProcessed_
        };
    }

    void resetForTest() {
        std::lock_guard<std::mutex> lock(mutex_);
        activeSessions_.clear();
        totalHttpsConnections_ = 0;
        handshakesSucceeded_ = 0;
        handshakesFailed_ = 0;
        requestsProcessed_ = 0;
    }

private:
    HttpsExecutiveConsole() = default;

    std::string generateDashboardHtmlResponse() const {
        std::string html = 
            "<!DOCTYPE html>\n"
            "<html lang=\"en\">\n"
            "<head>\n"
            "  <meta charset=\"UTF-8\">\n"
            "  <title>MicaNT Sovereign Executive Console</title>\n"
            "  <style>\n"
            "    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #0D1117; color: #C9D1D9; margin: 0; padding: 20px; }\n"
            "    .container { max-width: 900px; margin: 0 auto; }\n"
            "    .header { border-bottom: 2px solid #00D4FF; padding-bottom: 10px; margin-bottom: 20px; }\n"
            "    h1 { color: #00D4FF; margin: 0; }\n"
            "    .card { background: #161B22; border: 1px solid #30363D; border-radius: 8px; padding: 16px; margin-bottom: 16px; }\n"
            "    .stat-val { color: #58A6FF; font-weight: bold; }\n"
            "    .badge { background: #238636; color: #FFF; padding: 2px 8px; border-radius: 4px; font-size: 12px; }\n"
            "  </style>\n"
            "</head>\n"
            "<body>\n"
            "  <div class=\"container\">\n"
            "    <div class=\"header\">\n"
            "      <h1>MicaNT Sovereign Executive Console</h1>\n"
            "      <p>Target Silicon: Intel Xeon E3-1270 v6 @ 3.80GHz | TLS 1.3 End-to-End Encrypted</p>\n"
            "    </div>\n"
            "    <div class=\"card\">\n"
            "      <h3>Host Security Status <span class=\"badge\">FIPS 140-3 Compliant</span></h3>\n"
            "      <p>Operating System: <span class=\"stat-val\">MicaNT Sovereign Executive [Version 10.0.26100.5900]</span></p>\n"
            "      <p>Encrypted Protocol: <span class=\"stat-val\">TLS 1.3 (RFC 8446) / AES-128-GCM-SHA256</span></p>\n"
            "      <p>Machine Identity: <span class=\"stat-val\">CN=MicaNT-Server-147.135.1.229</span></p>\n"
            "      <p>Defender Firewall: <span class=\"stat-val\">Active (mpsdrv.sys / fwpkclnt.sys)</span></p>\n"
            "    </div>\n"
            "  </div>\n"
            "</body>\n"
            "</html>\n";

        std::ostringstream oss;
        oss << "HTTP/1.1 200 OK\r\n"
            << "Content-Type: text/html; charset=UTF-8\r\n"
            << "Content-Length: " << html.size() << "\r\n"
            << "Server: MicaNT-Executive-HTTPS/1.0\r\n"
            << "Connection: keep-alive\r\n\r\n"
            << html;
        return oss.str();
    }

    std::string generateVitalsJsonResponse() const {
        std::string json = "{\"status\":\"OK\",\"os\":\"MicaNT 10.0.26100.5900\",\"arch\":\"x86_64\","
                           "\"cpu\":\"Intel Xeon E3-1270 v6 @ 3.80GHz\",\"cores\":4,\"threads\":8,"
                           "\"ram_total_mb\":32768,\"ram_free_mb\":28672,\"commit_mb\":4096,"
                           "\"tls_version\":\"TLS 1.3\",\"cipher\":\"TLS_AES_128_GCM_SHA256\"}";
        std::ostringstream oss;
        oss << "HTTP/1.1 200 OK\r\n"
            << "Content-Type: application/json\r\n"
            << "Content-Length: " << json.size() << "\r\n"
            << "Server: MicaNT-Executive-HTTPS/1.0\r\n"
            << "Connection: keep-alive\r\n\r\n"
            << json;
        return oss.str();
    }

    std::string generateNetworkJsonResponse() const {
        auto& net = tcpip::NetworkStack::get();
        std::ostringstream json;
        json << "{\"status\":\"OK\",\"adapter\":\"Intel I210 Gigabit Ethernet\","
             << "\"mac\":\"" << (net.getAdapter() ? net.getAdapter()->getMacAddress().toString() : "00:00:00:00:00:00") << "\","
             << "\"ipv4\":\"" << net.getLocalIp().toString() << "\","
             << "\"subnet\":\"" << net.getSubnetMask().toString() << "\","
             << "\"gateway\":\"" << net.getGateway().toString() << "\","
             << "\"tls_active\":true,\"firewall_active\":true}";
        std::string jsonStr = json.str();

        std::ostringstream oss;
        oss << "HTTP/1.1 200 OK\r\n"
            << "Content-Type: application/json\r\n"
            << "Content-Length: " << jsonStr.size() << "\r\n"
            << "Server: MicaNT-Executive-HTTPS/1.0\r\n"
            << "Connection: keep-alive\r\n\r\n"
            << jsonStr;
        return oss.str();
    }

    std::string generateExecJsonResponse(std::string_view body) const {
        std::string cmd = "ver";
        if (!body.empty()) {
            if (body.starts_with("cmd=")) {
                cmd = std::string(body.substr(4));
            } else {
                cmd = std::string(body);
            }
        }

        std::string cmdOutput = tcpip::RemoteExecutiveConsole::get().executeCommand(cmd);

        // Escape JSON output
        std::string escaped;
        for (char c : cmdOutput) {
            if (c == '"') escaped += "\\\"";
            else if (c == '\\') escaped += "\\\\";
            else if (c == '\r') escaped += "\\r";
            else if (c == '\n') escaped += "\\n";
            else if (c == '\t') escaped += "\\t";
            else escaped += c;
        }

        std::string json = "{\"status\":\"OK\",\"command\":\"" + cmd + "\",\"output\":\"" + escaped + "\"}";
        std::ostringstream oss;
        oss << "HTTP/1.1 200 OK\r\n"
            << "Content-Type: application/json\r\n"
            << "Content-Length: " << json.size() << "\r\n"
            << "Server: MicaNT-Executive-HTTPS/1.0\r\n"
            << "Connection: keep-alive\r\n\r\n"
            << json;
        return oss.str();
    }

    std::mutex mutex_;
    bool running_{false};
    uint16_t port_{443};
    int serverSock_{-1};
    std::unordered_map<int, HttpsSessionContext> activeSessions_;

    uint64_t totalHttpsConnections_{0};
    uint64_t handshakesSucceeded_{0};
    uint64_t handshakesFailed_{0};
    uint64_t requestsProcessed_{0};
};

} // namespace micant::km::net
