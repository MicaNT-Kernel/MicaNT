#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <atomic>
#include <cstring>
#include <cstdint>

#include "micant/km/drivers/net/http.hpp"

namespace micant::httpapi::um {

using HANDLE = void*;
using PHANDLE = HANDLE*;
using ULONG = uint32_t;
using PULONG = ULONG*;
using PVOID = void*;
using PCWSTR = const wchar_t*;
using HTTP_SERVER_SESSION_ID = uint64_t;
using PHTTP_SERVER_SESSION_ID = HTTP_SERVER_SESSION_ID*;
using HTTP_URL_GROUP_ID = uint64_t;
using PHTTP_URL_GROUP_ID = HTTP_URL_GROUP_ID*;
using HTTP_URL_CONTEXT = uint64_t;
using HTTP_REQUEST_ID = uint64_t;

inline constexpr ULONG NO_ERROR = 0;
inline constexpr ULONG ERROR_INVALID_PARAMETER = 87;
inline constexpr ULONG ERROR_NOT_ENOUGH_MEMORY = 8;
inline constexpr ULONG ERROR_ALREADY_EXISTS = 183;
inline constexpr ULONG ERROR_FILE_NOT_FOUND = 2;
inline constexpr ULONG ERROR_HANDLE_EOF = 38;
inline constexpr ULONG ERROR_ACCESS_DENIED = 5;

inline constexpr ULONG HTTP_INITIALIZE_SERVER = 0x00000001;
inline constexpr ULONG HTTP_INITIALIZE_CONFIG = 0x00000002;

struct HTTPAPI_VERSION {
    unsigned short HttpApiMajorVersion;
    unsigned short HttpApiMinorVersion;
};

inline constexpr HTTPAPI_VERSION HTTPAPI_VERSION_1 = { 1, 0 };
inline constexpr HTTPAPI_VERSION HTTPAPI_VERSION_2 = { 2, 0 };

enum HTTP_SERVER_PROPERTY {
    HttpServerAuthenticationProperty,
    HttpServerLoggingProperty,
    HttpServerQosProperty,
    HttpServerTimeoutsProperty,
    HttpServerQueueLengthProperty,
    HttpServerStateProperty,
    HttpServer503VerbosityProperty,
    HttpServerBindingProperty,
    HttpServerExtendedAuthenticationProperty,
    HttpServerListenEndpointProperty,
    HttpServerChannelBindProperty,
    HttpServerProtectionLevelProperty
};

struct HTTP_BINDING_INFO {
    ULONG Flags{0x00000001};
    HANDLE RequestQueueHandle{nullptr};
};

// ============================================================================
// User-Mode Handle Table for Request Queues
// ============================================================================

class HttpHandleRegistry {
public:
    static HttpHandleRegistry& Instance() noexcept {
        static HttpHandleRegistry s_instance;
        return s_instance;
    }

    HANDLE registerQueue(uint64_t queueId) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        uintptr_t handleVal = static_cast<uintptr_t>(queueId);
        HANDLE h = reinterpret_cast<HANDLE>(handleVal);
        handles_[h] = queueId;
        return h;
    }

    bool unregisterQueue(HANDLE h) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = handles_.find(h);
        if (it == handles_.end()) return false;
        handles_.erase(it);
        return true;
    }

    uint64_t lookupQueue(HANDLE h) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = handles_.find(h);
        return (it != handles_.end()) ? it->second : 0;
    }

private:
    HttpHandleRegistry() = default;
    mutable std::mutex mutex_;
    std::map<HANDLE, uint64_t> handles_;
};

// ============================================================================
// Win32 HTTP Server API Implementation (httpapi.dll)
// ============================================================================

inline ULONG HttpInitialize(
    HTTPAPI_VERSION Version,
    ULONG Flags,
    PVOID pReserved
) noexcept {
    if (Version.HttpApiMajorVersion != 1 && Version.HttpApiMajorVersion != 2) {
        return ERROR_INVALID_PARAMETER;
    }
    km::http::HttpKernelProtocolDriver::Instance().initialize();
    return NO_ERROR;
}

inline ULONG HttpTerminate(
    ULONG Flags,
    PVOID pReserved
) noexcept {
    return NO_ERROR;
}

inline ULONG HttpCreateServerSession(
    HTTPAPI_VERSION Version,
    PHTTP_SERVER_SESSION_ID pServerSessionId,
    ULONG Reserved
) noexcept {
    if (!pServerSessionId || (Version.HttpApiMajorVersion != 2)) {
        return ERROR_INVALID_PARAMETER;
    }
    *pServerSessionId = km::http::HttpKernelProtocolDriver::Instance().createServerSession();
    return NO_ERROR;
}

inline ULONG HttpCloseServerSession(
    HTTP_SERVER_SESSION_ID ServerSessionId
) noexcept {
    return km::http::HttpKernelProtocolDriver::Instance().closeServerSession(ServerSessionId) ? NO_ERROR : ERROR_INVALID_PARAMETER;
}

inline ULONG HttpCreateUrlGroup(
    HTTP_SERVER_SESSION_ID ServerSessionId,
    PHTTP_URL_GROUP_ID pUrlGroupId,
    ULONG Reserved
) noexcept {
    if (!pUrlGroupId) return ERROR_INVALID_PARAMETER;
    *pUrlGroupId = km::http::HttpKernelProtocolDriver::Instance().createUrlGroup(ServerSessionId);
    return NO_ERROR;
}

inline ULONG HttpCloseUrlGroup(
    HTTP_URL_GROUP_ID UrlGroupId
) noexcept {
    return km::http::HttpKernelProtocolDriver::Instance().closeUrlGroup(UrlGroupId) ? NO_ERROR : ERROR_INVALID_PARAMETER;
}

inline ULONG HttpAddUrlToUrlGroup(
    HTTP_URL_GROUP_ID UrlGroupId,
    PCWSTR pFullyQualifiedUrl,
    HTTP_URL_CONTEXT UrlContext,
    ULONG Reserved
) noexcept {
    if (!pFullyQualifiedUrl) return ERROR_INVALID_PARAMETER;
    std::wstring url(pFullyQualifiedUrl);
    return km::http::HttpKernelProtocolDriver::Instance().addUrlToUrlGroup(UrlGroupId, url) ? NO_ERROR : ERROR_ACCESS_DENIED;
}

inline ULONG HttpRemoveUrlFromUrlGroup(
    HTTP_URL_GROUP_ID UrlGroupId,
    PCWSTR pFullyQualifiedUrl,
    ULONG Flags
) noexcept {
    if (!pFullyQualifiedUrl) return ERROR_INVALID_PARAMETER;
    std::wstring url(pFullyQualifiedUrl);
    return km::http::HttpKernelProtocolDriver::Instance().removeUrlFromUrlGroup(UrlGroupId, url) ? NO_ERROR : ERROR_FILE_NOT_FOUND;
}

inline ULONG HttpCreateRequestQueue(
    HTTPAPI_VERSION Version,
    PCWSTR pName,
    PVOID pSecurityAttributes,
    ULONG Flags,
    PHANDLE pReqQueueHandle
) noexcept {
    if (!pReqQueueHandle) return ERROR_INVALID_PARAMETER;
    std::wstring qname = pName ? pName : L"DefaultRequestQueue";
    uint64_t qid = km::http::HttpKernelProtocolDriver::Instance().createRequestQueue(qname, 1000);
    HANDLE h = HttpHandleRegistry::Instance().registerQueue(qid);
    *pReqQueueHandle = h;
    return NO_ERROR;
}

inline ULONG HttpCloseRequestQueue(
    HANDLE ReqQueueHandle
) noexcept {
    uint64_t qid = HttpHandleRegistry::Instance().lookupQueue(ReqQueueHandle);
    if (qid == 0) return ERROR_INVALID_PARAMETER;
    HttpHandleRegistry::Instance().unregisterQueue(ReqQueueHandle);
    return km::http::HttpKernelProtocolDriver::Instance().closeRequestQueue(qid) ? NO_ERROR : ERROR_INVALID_PARAMETER;
}

inline ULONG HttpSetUrlGroupProperty(
    HTTP_URL_GROUP_ID UrlGroupId,
    HTTP_SERVER_PROPERTY Property,
    PVOID pPropertyInformation,
    ULONG PropertyInformationLength
) noexcept {
    if (Property == HttpServerBindingProperty) {
        if (!pPropertyInformation || PropertyInformationLength < sizeof(HTTP_BINDING_INFO)) {
            return ERROR_INVALID_PARAMETER;
        }
        const auto* info = reinterpret_cast<const HTTP_BINDING_INFO*>(pPropertyInformation);
        uint64_t qid = HttpHandleRegistry::Instance().lookupQueue(info->RequestQueueHandle);
        if (qid == 0) return ERROR_INVALID_PARAMETER;
        return km::http::HttpKernelProtocolDriver::Instance().bindUrlGroupToQueue(UrlGroupId, qid) ? NO_ERROR : ERROR_INVALID_PARAMETER;
    }
    return NO_ERROR;
}

inline ULONG HttpReceiveHttpRequest(
    HANDLE ReqQueueHandle,
    HTTP_REQUEST_ID RequestId,
    ULONG Flags,
    km::http::HTTP_REQUEST_V2* pRequestBuffer,
    ULONG RequestBufferLength,
    PULONG pBytesReceived,
    PVOID pOverlapped
) noexcept {
    if (!pRequestBuffer) return ERROR_INVALID_PARAMETER;
    uint64_t qid = HttpHandleRegistry::Instance().lookupQueue(ReqQueueHandle);
    if (qid == 0) return ERROR_INVALID_PARAMETER;

    km::http::HTTP_REQUEST_V2 req;
    if (!km::http::HttpKernelProtocolDriver::Instance().receiveHttpRequest(qid, req)) {
        return ERROR_HANDLE_EOF;
    }

    *pRequestBuffer = req;
    if (pBytesReceived) *pBytesReceived = sizeof(km::http::HTTP_REQUEST_V2);
    return NO_ERROR;
}

inline ULONG HttpSendHttpResponse(
    HANDLE ReqQueueHandle,
    HTTP_REQUEST_ID RequestId,
    ULONG Flags,
    const km::http::HTTP_RESPONSE_V2* pHttpResponse,
    const km::http::HTTP_CACHE_POLICY* pCachePolicy,
    PULONG pBytesSent,
    PVOID pReserved2,
    ULONG Reserved3,
    PVOID pOverlapped,
    PVOID pLogData,
    const std::string& requestUrl = ""
) noexcept {
    if (!pHttpResponse) return ERROR_INVALID_PARAMETER;
    uint64_t qid = HttpHandleRegistry::Instance().lookupQueue(ReqQueueHandle);
    if (qid == 0) return ERROR_INVALID_PARAMETER;

    km::http::HTTP_CACHE_POLICY policy;
    if (pCachePolicy) policy = *pCachePolicy;

    bool ok = km::http::HttpKernelProtocolDriver::Instance().sendHttpResponse(qid, RequestId, *pHttpResponse, policy, requestUrl);
    if (pBytesSent) *pBytesSent = static_cast<ULONG>(pHttpResponse->entityBody.size());
    return ok ? NO_ERROR : ERROR_INVALID_PARAMETER;
}

inline ULONG HttpFlushResponseCache(
    HANDLE ReqQueueHandle,
    PCWSTR pUrlPrefix,
    ULONG Flags,
    PVOID pOverlapped
) noexcept {
    std::string prefix;
    if (pUrlPrefix) {
        std::wstring ws(pUrlPrefix);
        prefix.assign(ws.begin(), ws.end());
    }
    return km::http::HttpKernelProtocolDriver::Instance().flushResponseCache(prefix) ? NO_ERROR : ERROR_INVALID_PARAMETER;
}

} // namespace micant::httpapi::um
