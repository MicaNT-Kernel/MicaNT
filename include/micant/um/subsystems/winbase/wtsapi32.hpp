#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstring>
#include <cstdlib>
#include "micant/shared/ntstatus.hpp"
#include "micant/um/servers/rdp.hpp"

namespace micant::wts::um {

using HANDLE = void*;
using BOOL   = int;
using DWORD  = uint32_t;
using WCHAR  = wchar_t;
using LPWSTR = wchar_t*;
using PVOID  = void*;

inline constexpr HANDLE WTS_CURRENT_SERVER_HANDLE = nullptr;
inline constexpr DWORD  WTS_CURRENT_SESSION       = static_cast<DWORD>(-1);

enum WTS_CONNECTSTATE_CLASS {
    WTSActive,
    WTSConnected,
    WTSConnectQuery,
    WTSShadow,
    WTSDisconnected,
    WTSIdle,
    WTSListen,
    WTSReset,
    WTSDown,
    WTSInit
};

enum WTS_INFO_CLASS {
    WTSInitialProgram,
    WTSApplicationName,
    WTSWorkingDirectory,
    WTSOEMId,
    WTSSessionId,
    WTSUserName,
    WTSWinStationName,
    WTSDomainName,
    WTSConnectState,
    WTSClientBuildNumber,
    WTSClientName,
    WTSClientDirectory,
    WTSClientProductId,
    WTSClientHardwareId,
    WTSClientAddress,
    WTSClientDisplay,
    WTSClientProtocolType
};

struct WTS_SESSION_INFOW {
    DWORD                  SessionId;
    LPWSTR                 pWinStationName;
    WTS_CONNECTSTATE_CLASS State;
};
using PWTS_SESSION_INFOW = WTS_SESSION_INFOW*;

struct WTS_CLIENT_ADDRESS {
    DWORD AddressFamily; // 2 = AF_INET
    uint8_t Address[20];
};

struct WTS_CLIENT_DISPLAY {
    DWORD HorizontalResolution;
    DWORD VerticalResolution;
    DWORD ColorDepth;
};

// ============================================================================
// Win32 WTSAPI32 Implementation
// ============================================================================

inline BOOL WTSEnumerateSessionsW(
    HANDLE hServer,
    DWORD Reserved,
    DWORD Version,
    PWTS_SESSION_INFOW* ppSessionInfo,
    DWORD* pCount
) {
    if (!ppSessionInfo || !pCount || Version != 1) return 0;

    auto& server = rdp::EnterpriseRdpServer::get();
    auto sessions = server.getAllSessions();

    DWORD count = static_cast<DWORD>(sessions.size());
    if (count == 0) {
        *ppSessionInfo = nullptr;
        *pCount = 0;
        return 1;
    }

    auto* arr = static_cast<WTS_SESSION_INFOW*>(std::malloc(count * sizeof(WTS_SESSION_INFOW)));
    if (!arr) return 0;

    for (DWORD i = 0; i < count; ++i) {
        arr[i].SessionId = sessions[i]->getSessionId();
        const auto& ws = sessions[i]->getWinStationName();
        size_t len = (ws.size() + 1) * sizeof(WCHAR);
        arr[i].pWinStationName = static_cast<LPWSTR>(std::malloc(len));
        if (arr[i].pWinStationName) {
            std::memcpy(arr[i].pWinStationName, ws.c_str(), len);
        }

        switch (sessions[i]->getState()) {
            case rdp::RdpSessionState::ActiveStreaming:
                arr[i].State = WTSActive;
                break;
            case rdp::RdpSessionState::Disconnected:
                arr[i].State = WTSDisconnected;
                break;
            case rdp::RdpSessionState::Shadowing:
                arr[i].State = WTSShadow;
                break;
            default:
                arr[i].State = WTSConnected;
                break;
        }
    }

    *ppSessionInfo = arr;
    *pCount = count;
    return 1;
}

inline BOOL WTSQuerySessionInformationW(
    HANDLE hServer,
    DWORD SessionId,
    WTS_INFO_CLASS WTSInfoClass,
    LPWSTR* ppBuffer,
    DWORD* pBytesReturned
) {
    if (!ppBuffer || !pBytesReturned) return 0;

    auto& server = rdp::EnterpriseRdpServer::get();
    auto session = server.getSession(SessionId);
    if (!session) return 0;

    std::wstring resultStr;
    switch (WTSInfoClass) {
        case WTSUserName:
            resultStr = session->getUserName();
            break;
        case WTSDomainName:
            resultStr = session->getDomainName();
            break;
        case WTSWinStationName:
            resultStr = session->getWinStationName();
            break;
        case WTSClientName: {
            std::string h = session->getClientHostName();
            resultStr.assign(h.begin(), h.end());
            break;
        }
        case WTSClientBuildNumber: {
            std::string b = session->getCapabilities().clientBuild;
            resultStr.assign(b.begin(), b.end());
            break;
        }
        default:
            return 0;
    }

    size_t bytes = (resultStr.size() + 1) * sizeof(WCHAR);
    auto* buf = static_cast<LPWSTR>(std::malloc(bytes));
    if (!buf) return 0;

    std::memcpy(buf, resultStr.c_str(), bytes);
    *ppBuffer = buf;
    *pBytesReturned = static_cast<DWORD>(bytes);
    return 1;
}

inline BOOL WTSDisconnectSession(
    HANDLE hServer,
    DWORD SessionId,
    BOOL bWait
) {
    auto& server = rdp::EnterpriseRdpServer::get();
    return server.disconnectSession(SessionId) ? 1 : 0;
}

inline BOOL WTSLogoffSession(
    HANDLE hServer,
    DWORD SessionId,
    BOOL bWait
) {
    auto& server = rdp::EnterpriseRdpServer::get();
    return server.logoffSession(SessionId) ? 1 : 0;
}

inline void WTSFreeMemory(PVOID pMemory) {
    if (pMemory) {
        std::free(pMemory);
    }
}

} // namespace micant::wts::um
