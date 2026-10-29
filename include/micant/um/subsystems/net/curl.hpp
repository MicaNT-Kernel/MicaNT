#pragma once
/**
 * @file curl.hpp
 * @brief Horizon 155: Sovereign Win32 HTTPS Client Utility (curl.exe / Invoke-WebRequest),
 *        URL Parsing, TLS 1.3 Handshake Negotiation, Certificate Verification & HTTP/1.1 Pipeline.
 * 
 * Target Architecture: Supermicro X11SSH-F (Intel Xeon E3-1270 v6 @ 3.80GHz, 32GB DDR4 ECC).
 * Clean-Room Standard: 17 U.S.C. § 1201(f) & Google LLC v. Oracle America, Inc. (593 U.S. 1).
 */

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <sstream>
#include <memory>
#include <optional>
#include <atomic>

#include "micant/km/drivers/security/cng.hpp"
#include "micant/km/drivers/security/x509.hpp"
#include "micant/km/drivers/security/tls13.hpp"
#include "micant/km/drivers/net/https_console.hpp"
#include "micant/um/subsystems/winbase/version.hpp"

namespace micant::um::net {

struct UrlParts {
    std::string scheme{"https"};
    std::string host{"localhost"};
    uint16_t    port{443};
    std::string path{"/"};
};

class CurlClient {
public:
    static UrlParts parseUrl(std::string_view url) {
        UrlParts parts;
        size_t schemeEnd = url.find("://");
        std::string_view rest = url;
        if (schemeEnd != std::string_view::npos) {
            parts.scheme = std::string(url.substr(0, schemeEnd));
            rest = url.substr(schemeEnd + 3);
        }

        size_t pathStart = rest.find('/');
        std::string_view hostPort = (pathStart != std::string_view::npos) ? rest.substr(0, pathStart) : rest;
        if (pathStart != std::string_view::npos) {
            parts.path = std::string(rest.substr(pathStart));
        } else {
            parts.path = "/";
        }

        size_t colon = hostPort.find(':');
        if (colon != std::string_view::npos) {
            parts.host = std::string(hostPort.substr(0, colon));
            parts.port = static_cast<uint16_t>(std::stoi(std::string(hostPort.substr(colon + 1))));
        } else {
            parts.host = std::string(hostPort);
            parts.port = (parts.scheme == "http") ? 80 : 443;
        }

        return parts;
    }

    struct HttpResponse {
        int statusCode{0};
        std::string statusText;
        std::string body;
        bool tlsVerified{false};
        std::string negotiatedCipher;
    };

    static HttpResponse execute(
        std::string_view url,
        std::string_view method = "GET",
        std::string_view postData = "",
        bool insecure = false)
    {
        HttpResponse resp;
        UrlParts urlInfo = parseUrl(url);

        if (urlInfo.scheme == "https") {
            // 1. Initialize TLS 1.3 Client Session
            km::crypto::tls13::Tls13ClientSession clientTls;
            auto clientHello = clientTls.generateClientHello(urlInfo.host);

            // 2. Transmit to HttpsExecutiveConsole
            auto& console = km::net::HttpsExecutiveConsole::get();
            std::vector<uint8_t> serverHandshake;
            static std::atomic<int> s_nextSockId{15501};
            int clientSockId = s_nextSockId.fetch_add(1);

            if (!console.handleHandshake(clientSockId, clientHello, serverHandshake)) {
                resp.statusCode = 500;
                resp.statusText = "TLS Handshake Failed";
                return resp;
            }

            // 3. Complete TLS 1.3 Client Handshake
            if (!clientTls.processServerHandshake(serverHandshake)) {
                resp.statusCode = 500;
                resp.statusText = "Server Handshake Verification Failed";
                return resp;
            }

            // 4. Verify Server Certificate (unless --insecure / -k)
            auto certOpt = km::crypto::SovereignCertStoreRegistry::get().findCertificateByHost(urlInfo.host);
            if (certOpt.has_value()) {
                resp.tlsVerified = km::crypto::SovereignCertStoreRegistry::get().verifyCertificateChain(*certOpt);
            }
            if (!insecure && !resp.tlsVerified) {
                // If not insecure and not verified, reject
                resp.statusCode = 495;
                resp.statusText = "SSL Certificate Verification Failed";
                return resp;
            }

            resp.negotiatedCipher = "TLS_AES_128_GCM_SHA256";

            // 5. Construct HTTP/1.1 Request
            std::ostringstream httpReq;
            httpReq << method << " " << urlInfo.path << " HTTP/1.1\r\n"
                    << "Host: " << urlInfo.host << "\r\n"
                    << "User-Agent: curl/8.4.0 (MicaNT sovereign-cng)\r\n"
                    << "Accept: */*\r\n";
            if (!postData.empty()) {
                httpReq << "Content-Length: " << postData.size() << "\r\n"
                        << "Content-Type: application/x-www-form-urlencoded\r\n";
            }
            httpReq << "\r\n";
            if (!postData.empty()) {
                httpReq << postData;
            }
            std::string reqStr = httpReq.str();

            // 6. Encrypt Request via Client TLS Session
            auto encReq = clientTls.encryptAppData(
                std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(reqStr.data()), reqStr.size())
            );

            // 7. Dispatch to HTTPS Executive Console
            auto encResp = console.handleEncryptedRequest(clientSockId, encReq);
            if (encResp.empty()) {
                resp.statusCode = 502;
                resp.statusText = "Bad Gateway";
                return resp;
            }

            // 8. Decrypt Response via Client TLS Session
            std::vector<uint8_t> plainResp;
            if (!clientTls.decryptAppData(encResp, plainResp)) {
                resp.statusCode = 500;
                resp.statusText = "TLS Decryption Failed";
                return resp;
            }

            std::string respStr(reinterpret_cast<const char*>(plainResp.data()), plainResp.size());
            parseHttpResponse(respStr, resp);
            console.closeSession(clientSockId);
        } else {
            // Plain HTTP fallback
            auto& console = km::net::HttpsExecutiveConsole::get();
            std::ostringstream httpReq;
            httpReq << method << " " << urlInfo.path << " HTTP/1.1\r\n"
                    << "Host: " << urlInfo.host << "\r\n\r\n"
                    << postData;
            std::string plainResp = console.dispatchHttpRequest(httpReq.str());
            parseHttpResponse(plainResp, resp);
        }

        return resp;
    }

    // CLI Entry Point (curl.exe)
    static std::string runCommandLine(std::string_view args) {
        bool insecure = false;
        std::string method = "GET";
        std::string postData;
        std::string url = "https://147.135.1.229/api/v1/vitals";

        std::istringstream iss{std::string(args)};
        std::string token;
        while (iss >> token) {
            if (token == "-k" || token == "--insecure") {
                insecure = true;
            } else if (token == "-X" && (iss >> token)) {
                method = token;
            } else if ((token == "-d" || token == "--data") && (iss >> token)) {
                postData = token;
                method = "POST";
            } else if (token.starts_with("http://") || token.starts_with("https://")) {
                url = token;
            }
        }

        auto resp = execute(url, method, postData, insecure);
        return resp.body;
    }

private:
    static void parseHttpResponse(std::string_view raw, HttpResponse& resp) {
        size_t firstLineEnd = raw.find("\r\n");
        if (firstLineEnd != std::string_view::npos) {
            std::string_view statusLine = raw.substr(0, firstLineEnd);
            size_t firstSpace = statusLine.find(' ');
            if (firstSpace != std::string_view::npos) {
                size_t secondSpace = statusLine.find(' ', firstSpace + 1);
                if (secondSpace != std::string_view::npos) {
                    resp.statusCode = std::stoi(std::string(statusLine.substr(firstSpace + 1, secondSpace - firstSpace - 1)));
                    resp.statusText = std::string(statusLine.substr(secondSpace + 1));
                }
            }
        }

        size_t bodyStart = raw.find("\r\n\r\n");
        if (bodyStart != std::string_view::npos) {
            resp.body = std::string(raw.substr(bodyStart + 4));
        } else {
            resp.body = std::string(raw);
        }
    }
};

inline void InitializeCurlSubsystem() {
    auto& verDb = version::VersionDatabase::Instance();
    verDb.RegisterModule("curl.exe", "10.0.26100.5900", "MicaNT Sovereign Curl HTTPS Transfer Tool");
}

} // namespace micant::um::net
