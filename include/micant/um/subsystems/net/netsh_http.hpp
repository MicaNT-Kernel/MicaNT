// ============================================================================
// MicaNT: Clean-Room Windows-Compatible Operating System Executive
// Subsystem: Canonical HTTP Configuration Command-Line Tool (netsh http)
// Implementation: include/micant/um/subsystems/net/netsh_http.hpp
//
// Strict Clean-Room Implementation in modern ISO C++23. Zero External Dependencies.
// Conforms to Microsoft Win32 Command-Line Management Specifications under 17 U.S.C. Section 1201(f).
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <span>

#include "micant/km/drivers/net/http.hpp"

namespace micant::netsh::um {

class NetshHttpCommandExecutor {
public:
    static NetshHttpCommandExecutor& Instance() noexcept {
        static NetshHttpCommandExecutor s_instance;
        return s_instance;
    }

    int execute(std::span<const std::string_view> args, std::string& outStr) noexcept {
        if (args.empty()) {
            return showHelp(outStr);
        }

        size_t argIdx = 0;
        if (args[argIdx] == "http") {
            argIdx++;
            if (argIdx >= args.size()) {
                return showHelp(outStr);
            }
        }

        std::string_view verb = args[argIdx++];
        std::span<const std::string_view> subArgs = args.subspan(argIdx);

        if (verb == "show") {
            return executeShow(subArgs, outStr);
        } else if (verb == "add") {
            return executeAdd(subArgs, outStr);
        } else if (verb == "delete") {
            return executeDelete(subArgs, outStr);
        } else if (verb == "flush") {
            return executeFlush(subArgs, outStr);
        } else if (verb == "help" || verb == "/?" || verb == "?") {
            return showHelp(outStr);
        } else {
            outStr = "The following command was not found: " + std::string(verb) + "\r\n"
                     "Type 'netsh http help' to display a list of commands.\r\n";
            return 1;
        }
    }

private:
    NetshHttpCommandExecutor() = default;

    int showHelp(std::string& out) noexcept {
        std::ostringstream ss;
        ss << "\r\nThe following commands are available in this context:\r\n\r\n"
           << "Commands in this context:\r\n"
           << "add urlacl        - Adds a URL reservation entry.\r\n"
           << "delete urlacl     - Deletes a URL reservation entry.\r\n"
           << "flush responsecache - Flushes cached HTTP response fragments.\r\n"
           << "show cachestate   - Lists cached HTTP URL resources.\r\n"
           << "show servicestate - Views current HTTP service snapshot.\r\n"
           << "show urlacl       - Displays URL reservation entries.\r\n\r\n"
           << "To view help for a command, type the command, followed by a space, and then type ?.\r\n\r\n";
        out = ss.str();
        return 0;
    }

    int executeShow(std::span<const std::string_view> args, std::string& out) noexcept {
        if (args.empty()) {
            out = "Command syntax: netsh http show [urlacl | servicestate | cachestate]\r\n";
            return 1;
        }

        std::string_view target = args[0];
        if (target == "urlacl") {
            return showUrlAcl(args.subspan(1), out);
        } else if (target == "servicestate") {
            return showServiceState(args.subspan(1), out);
        } else if (target == "cachestate") {
            return showCacheState(args.subspan(1), out);
        } else {
            out = "Unknown show target: " + std::string(target) + "\r\n";
            return 1;
        }
    }

    int showUrlAcl(std::span<const std::string_view> args, std::string& out) noexcept {
        auto& drv = km::http::HttpKernelProtocolDriver::Instance();
        drv.initialize();

        std::wstring filterUrl;
        for (auto arg : args) {
            if (arg.starts_with("url=")) {
                std::string u(arg.substr(4));
                filterUrl.assign(u.begin(), u.end());
            }
        }

        auto acls = drv.getUrlAcls();
        std::ostringstream ss;
        ss << "\r\nURL Reservations:\r\n"
           << "-----------------\r\n\r\n";

        size_t count = 0;
        for (const auto& acl : acls) {
            if (!filterUrl.empty() && acl.urlPrefix != filterUrl) {
                continue;
            }
            std::string prefix(acl.urlPrefix.begin(), acl.urlPrefix.end());
            std::string user(acl.user.begin(), acl.user.end());
            std::string sddl(acl.sddl.begin(), acl.sddl.end());

            ss << "    Reserved URL            : " << prefix << "\r\n"
               << "        User: " << user << "\r\n"
               << "            Listen: " << (acl.listen ? "Yes" : "No") << "\r\n"
               << "            Delegate: " << (acl.delegate ? "Yes" : "No") << "\r\n"
               << "            SDDL: " << (sddl.empty() ? "D:(A;;GX;;;WD)" : sddl) << "\r\n\r\n";
            count++;
        }

        if (count == 0) {
            ss << "    No matching URL reservations found.\r\n\r\n";
        }

        out = ss.str();
        return 0;
    }

    int showServiceState(std::span<const std::string_view> /*args*/, std::string& out) noexcept {
        auto& drv = km::http::HttpKernelProtocolDriver::Instance();
        drv.initialize();

        std::ostringstream ss;
        ss << "\r\nSnapshot of HTTP.SYS service state:\r\n"
           << "-----------------------------------\r\n\r\n"
           << "Server session count: " << drv.getActiveSessionsCount() << "\r\n"
           << "URL groups count    : " << drv.getActiveUrlGroupsCount() << "\r\n"
           << "Request queues count: " << drv.getActiveQueuesCount() << "\r\n"
           << "Total HTTP requests : " << drv.getTotalRequests() << "\r\n"
           << "Total HTTP responses: " << drv.getTotalResponses() << "\r\n"
           << "Kernel cache hits   : " << drv.getCacheHits() << "\r\n"
           << "Kernel cache misses : " << drv.getCacheMisses() << "\r\n\r\n";

        // Registered URL Prefixes & Bound Queues
        auto prefixes = drv.getRegisteredPrefixes();
        ss << "Registered URL Prefixes (" << prefixes.size() << "):\r\n";
        for (const auto& p : prefixes) {
            std::string pfx(p.prefix.begin(), p.prefix.end());
            std::string user(p.registeredUser.begin(), p.registeredUser.end());
            ss << "    Prefix: " << pfx << " (GroupID: " << p.urlGroupId << ", QueueID: " << p.requestQueueId << ")\r\n";
        }
        ss << "\r\n";

        // Request Queues
        auto queues = drv.getRequestQueues();
        ss << "Request Queues (" << queues.size() << "):\r\n";
        for (const auto& [qid, q] : queues) {
            std::string qname(q->queueName.begin(), q->queueName.end());
            ss << "    Queue ID: " << qid << " | Name: " << qname
               << " | Controller PID: " << q->processId
               << " | Pending Requests: " << q->getCount() << "\r\n";
        }
        ss << "\r\n";

        out = ss.str();
        return 0;
    }

    int showCacheState(std::span<const std::string_view> args, std::string& out) noexcept {
        auto& drv = km::http::HttpKernelProtocolDriver::Instance();
        drv.initialize();

        std::string filterUrl;
        for (auto arg : args) {
            if (arg.starts_with("url=")) {
                filterUrl = std::string(arg.substr(4));
            }
        }

        auto cachedEntries = drv.getCachedResponses();
        std::ostringstream ss;
        ss << "\r\nSnapshot of HTTP response cache:\r\n"
           << "--------------------------------\r\n"
           << "Total cached entries: " << cachedEntries.size() << "\r\n"
           << "Kernel cache hits   : " << drv.getCacheHits() << "\r\n"
           << "Kernel cache misses : " << drv.getCacheMisses() << "\r\n\r\n";

        size_t shown = 0;
        for (const auto& [url, resp] : cachedEntries) {
            if (!filterUrl.empty() && url.find(filterUrl) == std::string::npos) {
                continue;
            }
            ss << "    URL: " << url << "\r\n"
               << "        Status Code : " << resp.statusCode << " " << resp.statusReason << "\r\n"
               << "        Content-Type: " << resp.contentType << "\r\n"
               << "        Content-Len : " << resp.body.size() << " bytes\r\n"
               << "        Hit Count   : " << resp.hitCount << "\r\n\r\n";
            shown++;
        }

        if (shown == 0) {
            ss << "    No cached responses found.\r\n\r\n";
        }

        out = ss.str();
        return 0;
    }

    int executeAdd(std::span<const std::string_view> args, std::string& out) noexcept {
        if (args.empty() || args[0] != "urlacl") {
            out = "Command syntax: netsh http add urlacl url=<url> user=<user> [listen=yes|no] [delegate=yes|no]\r\n";
            return 1;
        }

        std::string url;
        std::string user = "NT SERVICE\\W3SVC";
        bool listen = true;
        bool delegate = false;

        for (size_t i = 1; i < args.size(); ++i) {
            auto arg = args[i];
            if (arg.starts_with("url=")) {
                url = std::string(arg.substr(4));
            } else if (arg.starts_with("user=")) {
                user = std::string(arg.substr(5));
            } else if (arg.starts_with("listen=")) {
                listen = (arg.substr(7) == "yes" || arg.substr(7) == "YES");
            } else if (arg.starts_with("delegate=")) {
                delegate = (arg.substr(9) == "yes" || arg.substr(9) == "YES");
            }
        }

        if (url.empty()) {
            out = "Parameter 'url' is required.\r\n";
            return 1;
        }

        std::wstring wUrl(url.begin(), url.end());
        std::wstring wUser(user.begin(), user.end());

        auto& drv = km::http::HttpKernelProtocolDriver::Instance();
        drv.initialize();
        bool ok = drv.addUrlAcl(wUrl, wUser, listen, delegate);

        if (ok) {
            out = "\r\nURL reservation successfully added\r\n\r\n";
            return 0;
        } else {
            out = "\r\nFailed to add URL reservation.\r\n\r\n";
            return 1;
        }
    }

    int executeDelete(std::span<const std::string_view> args, std::string& out) noexcept {
        if (args.empty() || args[0] != "urlacl") {
            out = "Command syntax: netsh http delete urlacl url=<url>\r\n";
            return 1;
        }

        std::string url;
        for (size_t i = 1; i < args.size(); ++i) {
            auto arg = args[i];
            if (arg.starts_with("url=")) {
                url = std::string(arg.substr(4));
            }
        }

        if (url.empty()) {
            out = "Parameter 'url' is required.\r\n";
            return 1;
        }

        std::wstring wUrl(url.begin(), url.end());
        auto& drv = km::http::HttpKernelProtocolDriver::Instance();
        drv.initialize();
        bool ok = drv.deleteUrlAcl(wUrl);

        if (ok) {
            out = "\r\nURL reservation successfully deleted\r\n\r\n";
            return 0;
        } else {
            out = "\r\nURL reservation delete failed, Error: 2 (The system cannot find the file specified.)\r\n\r\n";
            return 2;
        }
    }

    int executeFlush(std::span<const std::string_view> args, std::string& out) noexcept {
        if (args.empty() || args[0] != "responsecache") {
            out = "Command syntax: netsh http flush responsecache [url=<url>]\r\n";
            return 1;
        }

        std::string urlPrefix;
        for (size_t i = 1; i < args.size(); ++i) {
            auto arg = args[i];
            if (arg.starts_with("url=")) {
                urlPrefix = std::string(arg.substr(4));
            }
        }

        auto& drv = km::http::HttpKernelProtocolDriver::Instance();
        drv.initialize();
        drv.flushResponseCache(urlPrefix);

        out = "\r\nHTTP response cache successfully flushed.\r\n\r\n";
        return 0;
    }
};

} // namespace micant::netsh::um
