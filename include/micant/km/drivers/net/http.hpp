#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <span>

#include "micant/shared/ntstatus.hpp"
#include "micant/um/servers/scm.hpp"
#include "micant/um/subsystems/winbase/version.hpp"

namespace micant::km::http {

// ============================================================================
// 1. IOCTL Control Codes (\\.\Http / \Device\Http / \Device\Http\ReqQueue)
// ============================================================================

inline constexpr uint32_t IOCTL_HTTP_INITIALIZE            = 0x00120000;
inline constexpr uint32_t IOCTL_HTTP_CREATE_SERVER_SESSION = 0x00120004;
inline constexpr uint32_t IOCTL_HTTP_CLOSE_SERVER_SESSION  = 0x00120008;
inline constexpr uint32_t IOCTL_HTTP_CREATE_URL_GROUP      = 0x0012000C;
inline constexpr uint32_t IOCTL_HTTP_CLOSE_URL_GROUP       = 0x00120010;
inline constexpr uint32_t IOCTL_HTTP_ADD_URL               = 0x00120014;
inline constexpr uint32_t IOCTL_HTTP_REMOVE_URL            = 0x00120018;
inline constexpr uint32_t IOCTL_HTTP_CREATE_REQ_QUEUE      = 0x0012001C;
inline constexpr uint32_t IOCTL_HTTP_CLOSE_REQ_QUEUE       = 0x00120020;
inline constexpr uint32_t IOCTL_HTTP_BIND_URL_TO_QUEUE     = 0x00120024;
inline constexpr uint32_t IOCTL_HTTP_RECEIVE_REQUEST       = 0x00120028;
inline constexpr uint32_t IOCTL_HTTP_SEND_RESPONSE         = 0x0012002C;
inline constexpr uint32_t IOCTL_HTTP_FLUSH_RESPONSE_CACHE  = 0x00120030;
inline constexpr uint32_t IOCTL_HTTP_QUERY_SERVICE_STATE   = 0x00120034;

// ============================================================================
// 2. Win32 HTTP Server API Structures & Types
// ============================================================================

enum HTTP_VERB {
    HttpVerbUnparsed,
    HttpVerbUnknown,
    HttpVerbInvalid,
    HttpVerbGET,
    HttpVerbPUT,
    HttpVerbHEAD,
    HttpVerbPOST,
    HttpVerbDELETE,
    HttpVerbTRACE,
    HttpVerbOPTIONS,
    HttpVerbCONNECT,
    HttpVerbTRACK,
    HttpVerbMOVE,
    HttpVerbCOPY,
    HttpVerbPROPFIND,
    HttpVerbPROPPATCH,
    HttpVerbMKCOL,
    HttpVerbLOCK,
    HttpVerbUNLOCK,
    HttpVerbSEARCH,
    HttpVerbMaximum
};

enum HTTP_HEADER_ID {
    HttpHeaderCacheControl          = 0,
    HttpHeaderConnection            = 1,
    HttpHeaderDate                  = 2,
    HttpHeaderKeepAlive             = 3,
    HttpHeaderPragma                = 4,
    HttpHeaderTrailer               = 5,
    HttpHeaderTransferEncoding      = 6,
    HttpHeaderUpgrade               = 7,
    HttpHeaderVia                   = 8,
    HttpHeaderWarning               = 9,
    HttpHeaderAllow                 = 10,
    HttpHeaderContentLength         = 11,
    HttpHeaderContentType           = 12,
    HttpHeaderContentEncoding       = 13,
    HttpHeaderContentLanguage       = 14,
    HttpHeaderContentLocation       = 15,
    HttpHeaderContentMd5            = 16,
    HttpHeaderContentRange          = 17,
    HttpHeaderExpires               = 18,
    HttpHeaderLastModified          = 19,
    HttpHeaderAccept                = 20,
    HttpHeaderAcceptCharset         = 21,
    HttpHeaderAcceptEncoding        = 22,
    HttpHeaderAcceptLanguage        = 23,
    HttpHeaderAuthorization         = 24,
    HttpHeaderCookie                = 25,
    HttpHeaderExpect                = 26,
    HttpHeaderFrom                  = 27,
    HttpHeaderHost                  = 28,
    HttpHeaderIfMatch               = 29,
    HttpHeaderIfModifiedSince       = 30,
    HttpHeaderIfNoneMatch           = 31,
    HttpHeaderIfRange               = 32,
    HttpHeaderIfUnmodifiedSince     = 33,
    HttpHeaderMaxForwards           = 34,
    HttpHeaderProxyAuthorization    = 35,
    HttpHeaderReferer               = 36,
    HttpHeaderRange                 = 37,
    HttpHeaderTe                    = 38,
    HttpHeaderTranslate             = 39,
    HttpHeaderUserAgent             = 40,
    HttpHeaderRequestMaximum        = 41
};

enum HTTP_CACHE_POLICY_TYPE {
    HttpCachePolicyNocache,
    HttpCachePolicyUserInvalidates,
    HttpCachePolicyTimeToLive
};

struct HTTP_CACHE_POLICY {
    HTTP_CACHE_POLICY_TYPE Policy{HttpCachePolicyNocache};
    uint32_t SecondsToLive{0};
};

struct HTTP_VERSION {
    uint16_t MajorVersion{1};
    uint16_t MinorVersion{1};
};

struct HTTP_COOKED_URL {
    uint16_t FullUrlLength{0};
    uint16_t HostLength{0};
    uint16_t AbsPathLength{0};
    uint16_t QueryStringLength{0};
    const wchar_t* pFullUrl{nullptr};
    const wchar_t* pHost{nullptr};
    const wchar_t* pAbsPath{nullptr};
    const wchar_t* pQueryString{nullptr};
};

struct HTTP_KNOWN_HEADER {
    uint16_t RawValueLength{0};
    const char* pRawValue{nullptr};
};

struct HTTP_UNKNOWN_HEADER {
    uint16_t NameLength{0};
    uint16_t RawValueLength{0};
    const char* pName{nullptr};
    const char* pRawValue{nullptr};
};

struct HTTP_REQUEST_HEADERS {
    uint16_t UnknownHeaderCount{0};
    HTTP_UNKNOWN_HEADER* pUnknownHeaders{nullptr};
    HTTP_KNOWN_HEADER KnownHeaders[HttpHeaderRequestMaximum]{};
};

struct HTTP_DATA_CHUNK {
    enum DataChunkType {
        HttpDataChunkFromMemory,
        HttpDataChunkFromFileHandle
    } DataChunkType{HttpDataChunkFromMemory};

    struct {
        const void* pBuffer{nullptr};
        uint32_t BufferLength{0};
    } FromMemory;
};

struct HTTP_REQUEST_V2 {
    uint32_t Flags{0};
    uint64_t ConnectionId{0};
    uint64_t RequestId{0};
    uint64_t UrlContext{0};
    HTTP_VERSION Version{1, 1};
    HTTP_VERB Verb{HttpVerbGET};
    std::string verbString{"GET"};
    std::string rawUrl{"/"};
    std::wstring cookedFullUrl{L"http://+:80/"};
    std::wstring cookedHost{L"+"};
    std::wstring cookedPath{L"/"};
    std::map<std::string, std::string> headers;
    std::vector<uint8_t> body;
};

struct HTTP_RESPONSE_V2 {
    uint32_t Flags{0};
    HTTP_VERSION Version{1, 1};
    uint16_t StatusCode{200};
    std::string reason{"OK"};
    std::map<std::string, std::string> headers;
    std::vector<uint8_t> entityBody;
};

// ============================================================================
// 3. Kernel Data Structures: URL ACLs, Prefix Tree, Request Queues, Cache
// ============================================================================

struct KernelHttpUrlAcl {
    std::wstring urlPrefix;
    std::wstring user{L"Administrators"};
    bool listen{true};
    bool delegate{false};
    std::wstring sddl{L"D:(A;;GX;;;WD)"};
};

struct UrlPrefixEntry {
    std::wstring prefix;
    uint64_t urlGroupId{0};
    uint64_t requestQueueId{0};
    std::wstring registeredUser;
};

struct KernelHttpCachedResponse {
    std::string canonicalUrl;
    uint16_t statusCode{200};
    std::string statusReason{"OK"};
    std::string contentType{"text/html"};
    std::vector<uint8_t> body;
    uint64_t expirationEpochMs{0};
    uint64_t hitCount{0};
};

class KernelHttpRequestQueue {
public:
    uint64_t queueId{0};
    std::wstring queueName;
    uint32_t processId{0};
    uint32_t maxRequests{1000};
    std::deque<HTTP_REQUEST_V2> pendingRequests;
    mutable std::mutex mutex;

    KernelHttpRequestQueue(uint64_t id, std::wstring name, uint32_t pid)
        : queueId(id), queueName(std::move(name)), processId(pid) {}

    bool enqueue(HTTP_REQUEST_V2 req) {
        std::lock_guard<std::mutex> lock(mutex);
        if (pendingRequests.size() >= maxRequests) return false;
        pendingRequests.push_back(std::move(req));
        return true;
    }

    bool dequeue(HTTP_REQUEST_V2& outReq) {
        std::lock_guard<std::mutex> lock(mutex);
        if (pendingRequests.empty()) return false;
        outReq = std::move(pendingRequests.front());
        pendingRequests.pop_front();
        return true;
    }

    [[nodiscard]] size_t getCount() const noexcept {
        std::lock_guard<std::mutex> lock(mutex);
        return pendingRequests.size();
    }
};

// ============================================================================
// 4. Sovereign HTTP.sys Kernel Protocol Driver Engine (\Device\Http)
// ============================================================================

class HttpKernelProtocolDriver {
public:
    static HttpKernelProtocolDriver& Instance() noexcept {
        static HttpKernelProtocolDriver s_instance;
        return s_instance;
    }

    void initialize() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        if (initialized_) return;

        // Pre-seed canonical URL ACLs
        KernelHttpUrlAcl defaultHttp;
        defaultHttp.urlPrefix = L"http://+:80/";
        defaultHttp.user = L"NT SERVICE\\W3SVC";
        defaultHttp.sddl = L"D:(A;;GX;;;S-1-5-80-569256582-2953403351-2909559716-1301513147-412116970)";
        urlAcls_[defaultHttp.urlPrefix] = defaultHttp;

        KernelHttpUrlAcl defaultHttps;
        defaultHttps.urlPrefix = L"https://+:443/";
        defaultHttps.user = L"NT SERVICE\\W3SVC";
        defaultHttps.sddl = L"D:(A;;GX;;;S-1-5-80-569256582-2953403351-2909559716-1301513147-412116970)";
        urlAcls_[defaultHttps.urlPrefix] = defaultHttps;

        KernelHttpUrlAcl defaultWinRm;
        defaultWinRm.urlPrefix = L"http://+:5985/wsman/";
        defaultWinRm.user = L"NT SERVICE\\WinRM";
        defaultWinRm.sddl = L"D:(A;;GX;;;S-1-5-80-569256582-2953403351-2909559716-1301513147-412116971)";
        urlAcls_[defaultWinRm.urlPrefix] = defaultWinRm;

        initialized_ = true;
    }

    // URL ACL Management (netsh http add/delete/show urlacl)
    bool addUrlAcl(const std::wstring& prefix, const std::wstring& user, bool listen = true, bool delegate = false) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        KernelHttpUrlAcl acl;
        acl.urlPrefix = prefix;
        acl.user = user;
        acl.listen = listen;
        acl.delegate = delegate;
        acl.sddl = L"D:(A;;GX;;;WD)";
        urlAcls_[prefix] = acl;
        return true;
    }

    bool deleteUrlAcl(const std::wstring& prefix) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = urlAcls_.find(prefix);
        if (it == urlAcls_.end()) return false;
        urlAcls_.erase(it);
        return true;
    }

    [[nodiscard]] std::vector<KernelHttpUrlAcl> getUrlAcls() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<KernelHttpUrlAcl> res;
        for (const auto& [_, acl] : urlAcls_) res.push_back(acl);
        return res;
    }

    // Server Sessions & URL Groups
    uint64_t createServerSession() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        static std::atomic<uint64_t> s_nextSessionId{100};
        uint64_t sid = s_nextSessionId.fetch_add(1);
        serverSessions_.push_back(sid);
        return sid;
    }

    bool closeServerSession(uint64_t sessionId) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::find(serverSessions_.begin(), serverSessions_.end(), sessionId);
        if (it == serverSessions_.end()) return false;
        serverSessions_.erase(it);
        return true;
    }

    uint64_t createUrlGroup(uint64_t serverSessionId) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        static std::atomic<uint64_t> s_nextGroupId{200};
        uint64_t gid = s_nextGroupId.fetch_add(1);
        urlGroups_[gid] = {};
        return gid;
    }

    bool closeUrlGroup(uint64_t urlGroupId) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = urlGroups_.find(urlGroupId);
        if (it == urlGroups_.end()) return false;
        
        // Remove all prefixes belonging to this group
        for (auto pit = urlPrefixes_.begin(); pit != urlPrefixes_.end(); ) {
            if (pit->urlGroupId == urlGroupId) {
                pit = urlPrefixes_.erase(pit);
            } else {
                ++pit;
            }
        }
        urlGroups_.erase(it);
        urlGroupBoundQueue_.erase(urlGroupId);
        return true;
    }

    bool addUrlToUrlGroup(uint64_t urlGroupId, const std::wstring& prefix) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        if (urlGroups_.find(urlGroupId) == urlGroups_.end()) return false;

        // Check ACL authorization
        bool aclPermitted = false;
        for (const auto& [aclPrefix, acl] : urlAcls_) {
            if (prefix.starts_with(aclPrefix) || aclPrefix.starts_with(prefix) || aclPrefix == L"http://+:80/" || aclPrefix == L"https://+:443/") {
                aclPermitted = true;
                break;
            }
        }
        if (!aclPermitted && !urlAcls_.empty()) {
            // Auto-permit if prefix matches generic HTTP/HTTPS scheme
            aclPermitted = prefix.starts_with(L"http://") || prefix.starts_with(L"https://");
        }

        uint64_t boundQueue = 0;
        if (auto bit = urlGroupBoundQueue_.find(urlGroupId); bit != urlGroupBoundQueue_.end()) {
            boundQueue = bit->second;
        }

        UrlPrefixEntry entry;
        entry.prefix = prefix;
        entry.urlGroupId = urlGroupId;
        entry.requestQueueId = boundQueue;
        entry.registeredUser = L"Administrators";

        urlPrefixes_.push_back(entry);
        urlGroups_[urlGroupId].push_back(prefix);
        return true;
    }

    bool removeUrlFromUrlGroup(uint64_t urlGroupId, const std::wstring& prefix) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        auto git = urlGroups_.find(urlGroupId);
        if (git == urlGroups_.end()) return false;

        auto pit = std::find(git->second.begin(), git->second.end(), prefix);
        if (pit != git->second.end()) git->second.erase(pit);

        for (auto it = urlPrefixes_.begin(); it != urlPrefixes_.end(); ++it) {
            if (it->urlGroupId == urlGroupId && it->prefix == prefix) {
                urlPrefixes_.erase(it);
                return true;
            }
        }
        return false;
    }

    // Request Queues (HttpCreateRequestQueue)
    uint64_t createRequestQueue(const std::wstring& name, uint32_t processId) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        static std::atomic<uint64_t> s_nextQueueId{1000};
        uint64_t qid = s_nextQueueId.fetch_add(1);
        auto q = std::make_shared<KernelHttpRequestQueue>(qid, name, processId);
        requestQueues_[qid] = q;
        return qid;
    }

    bool closeRequestQueue(uint64_t queueId) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = requestQueues_.find(queueId);
        if (it == requestQueues_.end()) return false;
        
        // Unbind any url prefixes pointing to this queue
        for (auto& p : urlPrefixes_) {
            if (p.requestQueueId == queueId) {
                p.requestQueueId = 0;
            }
        }
        for (auto bit = urlGroupBoundQueue_.begin(); bit != urlGroupBoundQueue_.end(); ) {
            if (bit->second == queueId) {
                bit = urlGroupBoundQueue_.erase(bit);
            } else {
                ++bit;
            }
        }
        requestQueues_.erase(it);
        return true;
    }

    bool bindUrlGroupToQueue(uint64_t urlGroupId, uint64_t queueId) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        if (urlGroups_.find(urlGroupId) == urlGroups_.end()) return false;
        if (requestQueues_.find(queueId) == requestQueues_.end()) return false;

        urlGroupBoundQueue_[urlGroupId] = queueId;

        for (auto& p : urlPrefixes_) {
            if (p.urlGroupId == urlGroupId) {
                p.requestQueueId = queueId;
            }
        }
        return true;
    }

    // Inbound Request Dispatching & Longest-Prefix Matching
    bool dispatchInboundRequest(const HTTP_REQUEST_V2& req, std::string& outDirectResponse) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        totalRequestsReceived_++;

        // 1. Check Kernel Zero-Copy Response Cache
        std::string canonicalUrl = req.rawUrl;
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        auto cit = responseCache_.find(canonicalUrl);
        if (cit != responseCache_.end()) {
            if (cit->second.expirationEpochMs == 0 || cit->second.expirationEpochMs > static_cast<uint64_t>(now)) {
                cit->second.hitCount++;
                cacheHits_++;

                std::ostringstream ss;
                ss << "HTTP/1.1 " << cit->second.statusCode << " " << cit->second.statusReason << "\r\n"
                   << "Content-Type: " << cit->second.contentType << "\r\n"
                   << "Content-Length: " << cit->second.body.size() << "\r\n"
                   << "Server: Microsoft-HTTPAPI/2.0 (MicaNT Kernel http.sys)\r\n"
                   << "X-Cache: HIT-KERNEL\r\n\r\n";
                ss.write(reinterpret_cast<const char*>(cit->second.body.data()), cit->second.body.size());
                outDirectResponse = ss.str();
                totalResponsesSent_++;
                totalBytesSent_ += outDirectResponse.size();
                return true;
            } else {
                responseCache_.erase(cit);
            }
        }
        cacheMisses_++;

        // 2. Longest Prefix Match
        std::wstring reqUrl = req.cookedFullUrl;
        UrlPrefixEntry* bestMatch = nullptr;
        size_t bestLen = 0;

        for (auto& prefixEntry : urlPrefixes_) {
            if (reqUrl.starts_with(prefixEntry.prefix)) {
                if (prefixEntry.prefix.size() > bestLen) {
                    bestLen = prefixEntry.prefix.size();
                    bestMatch = &prefixEntry;
                }
            }
        }

        if (!bestMatch || bestMatch->requestQueueId == 0) {
            // 404 Not Found or 503 Service Unavailable
            outDirectResponse = "HTTP/1.1 503 Service Unavailable\r\n"
                                "Content-Type: text/html\r\n"
                                "Content-Length: 48\r\n"
                                "Server: Microsoft-HTTPAPI/2.0\r\n\r\n"
                                "<h1>503 Service Unavailable (No ReqQueue)</h1>";
            totalResponsesSent_++;
            totalBytesSent_ += outDirectResponse.size();
            return false;
        }

        auto qit = requestQueues_.find(bestMatch->requestQueueId);
        if (qit == requestQueues_.end()) return false;

        bool enq = qit->second->enqueue(req);
        if (enq) totalRequestsDispatched_++;
        return enq;
    }

    // Pull from Request Queue
    bool receiveHttpRequest(uint64_t queueId, HTTP_REQUEST_V2& outReq) noexcept {
        std::shared_ptr<KernelHttpRequestQueue> q;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = requestQueues_.find(queueId);
            if (it == requestQueues_.end()) return false;
            q = it->second;
        }
        return q->dequeue(outReq);
    }

    // Send HTTP Response & Cache Storage
    bool sendHttpResponse(uint64_t queueId, uint64_t requestId, const HTTP_RESPONSE_V2& resp, const HTTP_CACHE_POLICY& cachePolicy, const std::string& requestUrl) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        totalResponsesSent_++;
        totalBytesSent_ += resp.entityBody.size();

        // If caching is requested, store in Kernel Zero-Copy Response Cache
        if (cachePolicy.Policy == HttpCachePolicyTimeToLive && cachePolicy.SecondsToLive > 0 && !requestUrl.empty()) {
            KernelHttpCachedResponse cached;
            cached.canonicalUrl = requestUrl;
            cached.statusCode = resp.StatusCode;
            cached.statusReason = resp.reason;
            cached.contentType = resp.headers.contains("Content-Type") ? resp.headers.at("Content-Type") : "text/html";
            cached.body = resp.entityBody;
            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            cached.expirationEpochMs = static_cast<uint64_t>(now) + (cachePolicy.SecondsToLive * 1000ULL);
            cached.hitCount = 0;
            responseCache_[requestUrl] = cached;
        }

        return true;
    }

    // Response Cache Flush
    bool flushResponseCache(const std::string& urlPrefix = "") noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        if (urlPrefix.empty()) {
            responseCache_.clear();
            return true;
        }
        for (auto it = responseCache_.begin(); it != responseCache_.end(); ) {
            if (it->first.starts_with(urlPrefix)) {
                it = responseCache_.erase(it);
            } else {
                ++it;
            }
        }
        return true;
    }

    // Diagnostics & State
    [[nodiscard]] size_t getCacheSize() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return responseCache_.size();
    }

    [[nodiscard]] uint64_t getCacheHits() const noexcept { return cacheHits_.load(); }
    [[nodiscard]] uint64_t getCacheMisses() const noexcept { return cacheMisses_.load(); }
    [[nodiscard]] uint64_t getTotalRequests() const noexcept { return totalRequestsReceived_.load(); }
    [[nodiscard]] uint64_t getTotalResponses() const noexcept { return totalResponsesSent_.load(); }
    [[nodiscard]] uint64_t getTotalBytesSent() const noexcept { return totalBytesSent_.load(); }

    [[nodiscard]] size_t getActiveSessionsCount() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return serverSessions_.size();
    }

    [[nodiscard]] size_t getActiveUrlGroupsCount() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return urlGroups_.size();
    }

    [[nodiscard]] size_t getActiveQueuesCount() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return requestQueues_.size();
    }

    [[nodiscard]] size_t getRegisteredPrefixCount() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return urlPrefixes_.size();
    }

    [[nodiscard]] std::vector<UrlPrefixEntry> getRegisteredPrefixes() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return urlPrefixes_;
    }

    [[nodiscard]] std::map<uint64_t, std::shared_ptr<KernelHttpRequestQueue>> getRequestQueues() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return requestQueues_;
    }

    [[nodiscard]] std::map<std::string, KernelHttpCachedResponse> getCachedResponses() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return responseCache_;
    }

private:
    HttpKernelProtocolDriver() = default;

    mutable std::mutex mutex_;
    bool initialized_{false};

    std::map<std::wstring, KernelHttpUrlAcl> urlAcls_;
    std::vector<uint64_t> serverSessions_;
    std::map<uint64_t, std::vector<std::wstring>> urlGroups_;
    std::map<uint64_t, uint64_t> urlGroupBoundQueue_;
    std::vector<UrlPrefixEntry> urlPrefixes_;
    std::map<uint64_t, std::shared_ptr<KernelHttpRequestQueue>> requestQueues_;
    std::map<std::string, KernelHttpCachedResponse> responseCache_;

    std::atomic<uint64_t> totalRequestsReceived_{0};
    std::atomic<uint64_t> totalRequestsDispatched_{0};
    std::atomic<uint64_t> totalResponsesSent_{0};
    std::atomic<uint64_t> cacheHits_{0};
    std::atomic<uint64_t> cacheMisses_{0};
    std::atomic<uint64_t> totalBytesSent_{0};
    std::atomic<uint64_t> totalBytesReceived_{0};
};

// ============================================================================
// 5. Driver Registration & SCM Integration
// ============================================================================

inline void RegisterHttpKernelDriver() {
    static bool registered = false;
    if (registered) return;
    registered = true;

    auto& drv = HttpKernelProtocolDriver::Instance();
    drv.initialize();

    // SCM Service Record: HTTP ("HTTP Protocol Stack")
    auto& scm = scm::ServiceControlManager::get();
    auto httpRecord = std::make_shared<scm::ServiceRecord>();
    httpRecord->serviceName = L"HTTP";
    httpRecord->displayName = L"HTTP Protocol Stack";
    httpRecord->serviceType = scm::SERVICE_KERNEL_DRIVER;
    httpRecord->startType = scm::SERVICE_BOOT_START;
    httpRecord->errorControl = scm::SERVICE_ERROR_NORMAL;
    httpRecord->svchostGroup = "Network";
    httpRecord->binaryPath = L"\\SystemRoot\\system32\\drivers\\http.sys";
    httpRecord->status.dwServiceType = httpRecord->serviceType;
    httpRecord->status.dwCurrentState = scm::SERVICE_RUNNING;
    httpRecord->status.dwControlsAccepted = scm::SERVICE_ACCEPT_STOP;
    scm.registerServiceRecord(httpRecord);

    // VersionDatabase Registration
    auto& vdb = version::VersionDatabase::Instance();
    vdb.RegisterModule("http.sys", "10.0.26100.5900", "HTTP Protocol Stack");
    vdb.RegisterModule("httpapi.dll", "10.0.26100.5900", "HTTP Server API");
}

} // namespace micant::km::http
