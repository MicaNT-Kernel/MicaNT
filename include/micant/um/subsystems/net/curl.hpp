#pragma once
/**
 * @file curl.hpp
 * @brief Horizon 155: Sovereign Win32 HTTPS Client Utility (curl.exe / Invoke-WebRequest),
 *        URL parsing and HTTP via the canonical WinHTTP/AFD socket path.
 *        HTTPS remains unavailable until the clean-room TLS verifier is complete.
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
#include <algorithm>
#include <limits>

#include "micant/um/subsystems/net/winhttp.hpp"
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
        if (urlInfo.scheme != "http" && urlInfo.scheme != "https") {
            resp.statusText = "Unsupported URL scheme";
            return resp;
        }
        (void)insecure; // Certificate bypass is never honored.
        const std::wstring host(urlInfo.host.begin(), urlInfo.host.end());
        const std::wstring path(urlInfo.path.begin(), urlInfo.path.end());
        const std::wstring verb(method.begin(), method.end());
        auto session = winhttp::WinHttpOpen(L"curl/8.4.0 (MicaNT)",
            winhttp::WINHTTP_ACCESS_TYPE_NO_PROXY, nullptr, nullptr, 0);
        if (!session) { resp.statusText = "WinHTTP session creation failed"; return resp; }
        auto connection = winhttp::WinHttpConnect(session, host.c_str(), urlInfo.port, 0);
        if (!connection) {
            winhttp::WinHttpCloseHandle(session);
            resp.statusText = "WinHTTP connection creation failed";
            return resp;
        }
        const winhttp::DWORD flags = urlInfo.scheme == "https" ? winhttp::WINHTTP_FLAG_SECURE : 0;
        auto request = winhttp::WinHttpOpenRequest(connection, verb.c_str(), path.c_str(),
            L"HTTP/1.1", nullptr, nullptr, flags);
        if (!request) {
            winhttp::WinHttpCloseHandle(connection);
            winhttp::WinHttpCloseHandle(session);
            resp.statusText = "WinHTTP request creation failed";
            return resp;
        }
        const winhttp::BOOL sent = winhttp::WinHttpSendRequest(request, nullptr, 0,
            postData.empty() ? nullptr : const_cast<char*>(postData.data()),
            static_cast<winhttp::DWORD>(postData.size()), static_cast<winhttp::DWORD>(postData.size()), 0);
        if (sent && winhttp::WinHttpReceiveResponse(request, nullptr)) {
            winhttp::DWORD statusLength = sizeof(winhttp::DWORD);
            winhttp::DWORD statusCode = 0;
            if (winhttp::WinHttpQueryHeaders(request,
                    winhttp::WINHTTP_QUERY_STATUS_CODE | winhttp::WINHTTP_QUERY_FLAG_NUMBER,
                    nullptr, &statusCode, &statusLength, nullptr)) {
                resp.statusCode = static_cast<int>(statusCode);
                winhttp::DWORD available = 0;
                while (winhttp::WinHttpQueryDataAvailable(request, &available) && available) {
                    if (available > 8 * 1024 * 1024 || resp.body.size() > 8 * 1024 * 1024 - available)
                        break;
                    const size_t oldSize = resp.body.size();
                    resp.body.resize(oldSize + available);
                    winhttp::DWORD read = 0;
                    if (!winhttp::WinHttpReadData(request, resp.body.data() + oldSize, available, &read)) {
                        resp.body.resize(oldSize);
                        break;
                    }
                    resp.body.resize(oldSize + read);
                    if (read == 0) break;
                }
            }
        } else {
            resp.statusText = urlInfo.scheme == "https"
                ? "TLS authentication provider unavailable"
                : "HTTP transport failed";
        }
        winhttp::WinHttpCloseHandle(request);
        winhttp::WinHttpCloseHandle(connection);
        winhttp::WinHttpCloseHandle(session);
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
