#pragma once

/**
 * @file ws2_32.hpp
 * @brief MicaNT Clean-Room Windows Sockets 2 (ws2_32.dll) Bridge.
 *
 * Implements socket networking stubs and Winsock initialization.
 */

#include <cstdint>
#include <cstring>
#include "ntdef.hpp"
#include "kernel32.hpp"
#include "ldr.hpp"

namespace micant::ws2_32 {

struct WSADATA {
    uint16_t wVersion{0x0202};
    uint16_t wHighVersion{0x0202};
    char szDescription[257]{"MicaNT Clean-Room Sockets"};
    char szSystemStatus[129]{"Running"};
    uint16_t iMaxSockets{32767};
    uint16_t iMaxUdpDg{65467};
    char* lpVendorInfo{nullptr};
};

inline thread_local int g_WsaLastError = 0;

inline int WSAStartup(uint16_t /*wVersionRequired*/, WSADATA* lpWSAData) noexcept {
    if (lpWSAData) {
        *lpWSAData = WSADATA{};
    }
    return 0; // Success
}

inline int WSACleanup() noexcept {
    return 0; // Success
}

inline int WSAGetLastError() noexcept {
    return g_WsaLastError;
}

inline void WSASetLastError(int iError) noexcept {
    g_WsaLastError = iError;
}

inline uintptr_t socket(int /*af*/, int /*type*/, int /*protocol*/) noexcept {
    return static_cast<uintptr_t>(~0ULL); // INVALID_SOCKET
}

inline int closesocket(uintptr_t /*s*/) noexcept {
    return 0;
}

inline int gethostname(char* name, int namelen) noexcept {
    if (!name || namelen <= 0) return -1;
    const char host[] = "MicaNT-PC";
    std::strncpy(name, host, namelen);
    name[namelen - 1] = '\0';
    return 0;
}

inline uint32_t inet_addr(const char* cp) noexcept {
    if (!cp) return 0xFFFFFFFF; // INADDR_NONE
    if (std::strcmp(cp, "127.0.0.1") == 0) return 0x0100007F; // 127.0.0.1 in network byte order
    return 0xFFFFFFFF;
}

inline char* inet_ntoa(uint32_t in) noexcept {
    static thread_local char buf[32];
    uint8_t* b = reinterpret_cast<uint8_t*>(&in);
    std::snprintf(buf, sizeof(buf), "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
    return buf;
}

inline void InitializeWs2_32SubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("ws2_32.dll", "WSAStartup", reinterpret_cast<void*>(WSAStartup));
    ldr.registerExport("ws2_32.dll", "WSACleanup", reinterpret_cast<void*>(WSACleanup));
    ldr.registerExport("ws2_32.dll", "WSAGetLastError", reinterpret_cast<void*>(WSAGetLastError));
    ldr.registerExport("ws2_32.dll", "WSASetLastError", reinterpret_cast<void*>(WSASetLastError));
    ldr.registerExport("ws2_32.dll", "socket", reinterpret_cast<void*>(socket));
    ldr.registerExport("ws2_32.dll", "closesocket", reinterpret_cast<void*>(closesocket));
    ldr.registerExport("ws2_32.dll", "gethostname", reinterpret_cast<void*>(gethostname));
    ldr.registerExport("ws2_32.dll", "inet_addr", reinterpret_cast<void*>(inet_addr));
    ldr.registerExport("ws2_32.dll", "inet_ntoa", reinterpret_cast<void*>(inet_ntoa));
}

} // namespace micant::ws2_32
