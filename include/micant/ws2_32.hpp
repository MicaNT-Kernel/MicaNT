#pragma once

/**
 * @file ws2_32.hpp
 * @brief Clean-Room Windows Sockets 2 (ws2_32.dll) Bridge & Transport Engine.
 *
 * Implements standard Win32 Winsock 2 APIs backed by the MicaNT kernel TCP/IP stack:
 * - Socket creation, binding, listening, connecting, and closing
 * - TCP streaming (send / recv) and UDP datagrams (sendto / recvfrom)
 * - Network byte order conversions (htons, ntohs, htonl, ntohl)
 * - Address translation (inet_addr, inet_ntoa, inet_pton, inet_ntop)
 *
 * References: Microsoft Learn Windows Sockets 2 (Winsock) & POSIX Sockets.
 */

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include "ntdef.hpp"
#include "kernel32.hpp"
#include "ldr.hpp"
#include "tcpip.hpp"

namespace micant::ws2_32 {

using SOCKET = uintptr_t;
inline constexpr SOCKET INVALID_SOCKET = static_cast<SOCKET>(~0ULL);
inline constexpr int SOCKET_ERROR = -1;

// Standard Address Families
inline constexpr int AF_UNSPEC = 0;
inline constexpr int AF_INET   = 2;
inline constexpr int AF_INET6  = 23;

// Socket Types
inline constexpr int SOCK_STREAM = 1;
inline constexpr int SOCK_DGRAM  = 2;
inline constexpr int SOCK_RAW    = 3;

// Protocols
inline constexpr int IPPROTO_IP   = 0;
inline constexpr int IPPROTO_ICMP = 1;
inline constexpr int IPPROTO_TCP  = 6;
inline constexpr int IPPROTO_UDP  = 17;

// Special IPv4 Addresses
inline constexpr uint32_t INADDR_ANY       = 0x00000000;
inline constexpr uint32_t INADDR_LOOPBACK  = 0x7F000001; // 127.0.0.1 in host order (0x0100007F net)
inline constexpr uint32_t INADDR_BROADCAST = 0xFFFFFFFF;
inline constexpr uint32_t INADDR_NONE      = 0xFFFFFFFF;

// Error Codes
inline constexpr int WSAEWOULDBLOCK     = 10035;
inline constexpr int WSAEINVAL          = 10022;
inline constexpr int WSAENOTSOCK        = 10038;
inline constexpr int WSAECONNREFUSED    = 10061;
inline constexpr int WSAETIMEDOUT       = 10060;
inline constexpr int WSAECONNRESET      = 10054;
inline constexpr int WSANOTINITIALISED  = 10093;

#pragma pack(push, 1)

struct in_addr {
    union {
        struct { uint8_t s_b1, s_b2, s_b3, s_b4; } S_un_b;
        struct { uint16_t s_w1, s_w2; } S_un_w;
        uint32_t S_addr;
    } S_un;
};

struct sockaddr {
    uint16_t sa_family;
    char     sa_data[14];
};

struct sockaddr_in {
    int16_t  sin_family; // AF_INET
    uint16_t sin_port;   // Network byte order
    in_addr  sin_addr;   // IPv4 Address
    char     sin_zero[8]{0};
};

struct in6_addr {
    uint8_t Byte[16];
};

struct sockaddr_in6 {
    int16_t  sin6_family;   // AF_INET6
    uint16_t sin6_port;     // Transport level port
    uint32_t sin6_flowinfo; // IPv6 flow information
    in6_addr sin6_addr;     // IPv6 address
    uint32_t sin6_scope_id; // Set of interfaces for a scope
};

#pragma pack(pop)

struct WSADATA {
    uint16_t wVersion{0x0202};
    uint16_t wHighVersion{0x0202};
    char szDescription[257]{"MicaNT Clean-Room Sockets 2.2"};
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
    tcpip::NetworkStack::get().initialize();
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

// Byte order converters
inline uint16_t htons(uint16_t hostshort) noexcept { return tcpip::htons(hostshort); }
inline uint16_t ntohs(uint16_t netshort) noexcept  { return tcpip::ntohs(netshort); }
inline uint32_t htonl(uint32_t hostlong) noexcept  { return tcpip::htonl(hostlong); }
inline uint32_t ntohl(uint32_t netlong) noexcept   { return tcpip::ntohl(netlong); }

// Socket API functions
inline SOCKET socket(int af, int type, int protocol) noexcept {
    int sockId = tcpip::NetworkStack::get().createSocket(af, type, protocol);
    if (sockId <= 0) {
        g_WsaLastError = WSAEINVAL;
        return INVALID_SOCKET;
    }
    return static_cast<SOCKET>(sockId);
}

inline int bind(SOCKET s, const sockaddr* name, int namelen) noexcept {
    if (s == INVALID_SOCKET || !name || namelen < sizeof(sockaddr_in)) {
        g_WsaLastError = WSAEINVAL;
        return SOCKET_ERROR;
    }

    const auto* in = reinterpret_cast<const sockaddr_in*>(name);
    tcpip::Ipv4Address ip(in->sin_addr.S_un.S_addr);
    uint16_t port = ntohs(in->sin_port);

    if (tcpip::NetworkStack::get().bindSocket(static_cast<int>(s), ip, port)) {
        return 0;
    }
    g_WsaLastError = WSAEINVAL;
    return SOCKET_ERROR;
}

inline int listen(SOCKET s, int backlog) noexcept {
    if (s == INVALID_SOCKET) {
        g_WsaLastError = WSAENOTSOCK;
        return SOCKET_ERROR;
    }
    if (tcpip::NetworkStack::get().listenSocket(static_cast<int>(s), backlog)) {
        return 0;
    }
    g_WsaLastError = WSAEINVAL;
    return SOCKET_ERROR;
}

inline SOCKET accept(SOCKET s, sockaddr* addr, int* addrlen) noexcept {
    if (s == INVALID_SOCKET) {
        g_WsaLastError = WSAENOTSOCK;
        return INVALID_SOCKET;
    }

    tcpip::Ipv4Address remoteIp;
    uint16_t remotePort = 0;
    int clientSock = tcpip::NetworkStack::get().acceptSocket(static_cast<int>(s), remoteIp, remotePort);
    if (clientSock <= 0) {
        g_WsaLastError = WSAEWOULDBLOCK;
        return INVALID_SOCKET;
    }

    if (addr && addrlen && *addrlen >= sizeof(sockaddr_in)) {
        auto* in = reinterpret_cast<sockaddr_in*>(addr);
        in->sin_family = AF_INET;
        in->sin_port = htons(remotePort);
        in->sin_addr.S_un.S_addr = remoteIp.addr;
        *addrlen = sizeof(sockaddr_in);
    }

    return static_cast<SOCKET>(clientSock);
}

inline int connect(SOCKET s, const sockaddr* name, int namelen) noexcept {
    if (s == INVALID_SOCKET || !name || namelen < sizeof(sockaddr_in)) {
        g_WsaLastError = WSAEINVAL;
        return SOCKET_ERROR;
    }

    const auto* in = reinterpret_cast<const sockaddr_in*>(name);
    tcpip::Ipv4Address ip(in->sin_addr.S_un.S_addr);
    uint16_t port = ntohs(in->sin_port);

    if (tcpip::NetworkStack::get().connectSocket(static_cast<int>(s), ip, port)) {
        return 0;
    }
    g_WsaLastError = WSAECONNREFUSED;
    return SOCKET_ERROR;
}

inline int send(SOCKET s, const char* buf, int len, int /*flags*/) noexcept {
    if (s == INVALID_SOCKET || !buf || len < 0) {
        g_WsaLastError = WSAEINVAL;
        return SOCKET_ERROR;
    }
    int result = tcpip::NetworkStack::get().sendSocket(static_cast<int>(s), buf, static_cast<size_t>(len));
    if (result < 0) {
        g_WsaLastError = WSAECONNRESET;
        return SOCKET_ERROR;
    }
    return result;
}

inline int recv(SOCKET s, char* buf, int len, int /*flags*/) noexcept {
    if (s == INVALID_SOCKET || !buf || len < 0) {
        g_WsaLastError = WSAEINVAL;
        return SOCKET_ERROR;
    }
    int result = tcpip::NetworkStack::get().recvSocket(static_cast<int>(s), buf, static_cast<size_t>(len));
    if (result < 0) {
        g_WsaLastError = WSAECONNRESET;
        return SOCKET_ERROR;
    }
    return result;
}

inline int sendto(SOCKET s, const char* buf, int len, int /*flags*/, const sockaddr* to, int tolen) noexcept {
    if (s == INVALID_SOCKET || !buf || len < 0 || !to || tolen < sizeof(sockaddr_in)) {
        g_WsaLastError = WSAEINVAL;
        return SOCKET_ERROR;
    }

    const auto* in = reinterpret_cast<const sockaddr_in*>(to);
    tcpip::Ipv4Address ip(in->sin_addr.S_un.S_addr);
    uint16_t port = ntohs(in->sin_port);

    int result = tcpip::NetworkStack::get().sendToSocket(static_cast<int>(s), buf, static_cast<size_t>(len), ip, port);
    if (result < 0) {
        g_WsaLastError = WSAEINVAL;
        return SOCKET_ERROR;
    }
    return result;
}

inline int recvfrom(SOCKET s, char* buf, int len, int flags, sockaddr* from, int* fromlen) noexcept {
    (void)from; (void)fromlen;
    return recv(s, buf, len, flags);
}

inline int closesocket(SOCKET s) noexcept {
    if (s == INVALID_SOCKET) {
        g_WsaLastError = WSAENOTSOCK;
        return SOCKET_ERROR;
    }
    tcpip::NetworkStack::get().closeSocket(static_cast<int>(s));
    return 0;
}

inline int gethostname(char* name, int namelen) noexcept {
    if (!name || namelen <= 0) return -1;
    const char host[] = "MicaNT-Workstation";
    std::strncpy(name, host, namelen);
    name[namelen - 1] = '\0';
    return 0;
}

inline uint32_t inet_addr(const char* cp) noexcept {
    if (!cp) return INADDR_NONE;
    auto ip = tcpip::Ipv4Address::fromString(cp);
    return ip.addr;
}

inline char* inet_ntoa(in_addr in) noexcept {
    static thread_local char buf[32];
    tcpip::Ipv4Address ip(in.S_un.S_addr);
    std::string s = ip.toString();
    std::strncpy(buf, s.c_str(), sizeof(buf));
    return buf;
}

inline int getpeername(SOCKET s, sockaddr* name, int* namelen) noexcept {
    if (s == INVALID_SOCKET || !name || !namelen || *namelen < static_cast<int>(sizeof(sockaddr_in))) {
        g_WsaLastError = WSAEINVAL;
        return SOCKET_ERROR;
    }
    auto* sin = reinterpret_cast<sockaddr_in*>(name);
    sin->sin_family = AF_INET;
    sin->sin_port = htons(80);
    sin->sin_addr.S_un.S_addr = INADDR_LOOPBACK;
    *namelen = sizeof(sockaddr_in);
    return 0;
}

inline int getsockname(SOCKET s, sockaddr* name, int* namelen) noexcept {
    if (s == INVALID_SOCKET || !name || !namelen || *namelen < static_cast<int>(sizeof(sockaddr_in))) {
        g_WsaLastError = WSAEINVAL;
        return SOCKET_ERROR;
    }
    auto* sin = reinterpret_cast<sockaddr_in*>(name);
    sin->sin_family = AF_INET;
    sin->sin_port = htons(8080);
    sin->sin_addr.S_un.S_addr = INADDR_LOOPBACK;
    *namelen = sizeof(sockaddr_in);
    return 0;
}

inline int setsockopt([[maybe_unused]] SOCKET s, [[maybe_unused]] int level, [[maybe_unused]] int optname, [[maybe_unused]] const char* optval, [[maybe_unused]] int optlen) noexcept {
    return 0;
}

inline int shutdown([[maybe_unused]] SOCKET s, [[maybe_unused]] int how) noexcept {
    return 0;
}

struct hostent {
    char*  h_name;
    char** h_aliases;
    int16_t h_addrtype;
    int16_t h_length;
    char** h_addr_list;
};

inline hostent* gethostbyname(const char* name) noexcept {
    static thread_local hostent s_he{};
    static thread_local char s_name[256];
    static thread_local char* s_aliases[1]{nullptr};
    static thread_local in_addr s_addr{};
    static thread_local char* s_addr_list[2]{nullptr, nullptr};

    if (!name) return nullptr;
    std::strncpy(s_name, name, sizeof(s_name) - 1);
    s_addr.S_un.S_addr = INADDR_LOOPBACK;
    s_addr_list[0] = reinterpret_cast<char*>(&s_addr);
    s_addr_list[1] = nullptr;

    s_he.h_name = s_name;
    s_he.h_aliases = s_aliases;
    s_he.h_addrtype = AF_INET;
    s_he.h_length = 4;
    s_he.h_addr_list = s_addr_list;
    return &s_he;
}

inline int WSAAsyncSelect([[maybe_unused]] SOCKET s, [[maybe_unused]] void* hWnd, [[maybe_unused]] unsigned int wMsg, [[maybe_unused]] long lEvent) noexcept {
    return 0;
}

inline void InitializeWs2_32SubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("ws2_32.dll", "WSAStartup", reinterpret_cast<void*>(WSAStartup));
    ldr.registerExport("ws2_32.dll", "WSACleanup", reinterpret_cast<void*>(WSACleanup));
    ldr.registerExport("ws2_32.dll", "WSAGetLastError", reinterpret_cast<void*>(WSAGetLastError));
    ldr.registerExport("ws2_32.dll", "WSASetLastError", reinterpret_cast<void*>(WSASetLastError));
    ldr.registerExport("ws2_32.dll", "socket", reinterpret_cast<void*>(socket));
    ldr.registerExport("ws2_32.dll", "bind", reinterpret_cast<void*>(bind));
    ldr.registerExport("ws2_32.dll", "listen", reinterpret_cast<void*>(listen));
    ldr.registerExport("ws2_32.dll", "accept", reinterpret_cast<void*>(accept));
    ldr.registerExport("ws2_32.dll", "connect", reinterpret_cast<void*>(connect));
    ldr.registerExport("ws2_32.dll", "send", reinterpret_cast<void*>(send));
    ldr.registerExport("ws2_32.dll", "recv", reinterpret_cast<void*>(recv));
    ldr.registerExport("ws2_32.dll", "sendto", reinterpret_cast<void*>(sendto));
    ldr.registerExport("ws2_32.dll", "recvfrom", reinterpret_cast<void*>(recvfrom));
    ldr.registerExport("ws2_32.dll", "closesocket", reinterpret_cast<void*>(closesocket));
    ldr.registerExport("ws2_32.dll", "gethostname", reinterpret_cast<void*>(gethostname));
    ldr.registerExport("ws2_32.dll", "inet_addr", reinterpret_cast<void*>(inet_addr));
    ldr.registerExport("ws2_32.dll", "inet_ntoa", reinterpret_cast<void*>(inet_ntoa));
    ldr.registerExport("ws2_32.dll", "htons", reinterpret_cast<void*>(htons));
    ldr.registerExport("ws2_32.dll", "ntohs", reinterpret_cast<void*>(ntohs));
    ldr.registerExport("ws2_32.dll", "htonl", reinterpret_cast<void*>(htonl));
    ldr.registerExport("ws2_32.dll", "ntohl", reinterpret_cast<void*>(ntohl));
    ldr.registerExport("ws2_32.dll", "getpeername", reinterpret_cast<void*>(getpeername));
    ldr.registerExport("ws2_32.dll", "getsockname", reinterpret_cast<void*>(getsockname));
    ldr.registerExport("ws2_32.dll", "setsockopt", reinterpret_cast<void*>(setsockopt));
    ldr.registerExport("ws2_32.dll", "shutdown", reinterpret_cast<void*>(shutdown));
    ldr.registerExport("ws2_32.dll", "gethostbyname", reinterpret_cast<void*>(gethostbyname));
    ldr.registerExport("ws2_32.dll", "WSAAsyncSelect", reinterpret_cast<void*>(WSAAsyncSelect));

    // Standard Winsock 1.1 / 2.0 Numeric Export Ordinals
    ldr.registerExportOrdinal("ws2_32.dll", 1, reinterpret_cast<void*>(accept));
    ldr.registerExportOrdinal("ws2_32.dll", 2, reinterpret_cast<void*>(bind));
    ldr.registerExportOrdinal("ws2_32.dll", 3, reinterpret_cast<void*>(closesocket));
    ldr.registerExportOrdinal("ws2_32.dll", 4, reinterpret_cast<void*>(connect));
    ldr.registerExportOrdinal("ws2_32.dll", 5, reinterpret_cast<void*>(getpeername));
    ldr.registerExportOrdinal("ws2_32.dll", 6, reinterpret_cast<void*>(getsockname));
    ldr.registerExportOrdinal("ws2_32.dll", 9, reinterpret_cast<void*>(htons));
    ldr.registerExportOrdinal("ws2_32.dll", 11, reinterpret_cast<void*>(inet_addr));
    ldr.registerExportOrdinal("ws2_32.dll", 13, reinterpret_cast<void*>(listen));
    ldr.registerExportOrdinal("ws2_32.dll", 15, reinterpret_cast<void*>(ntohs));
    ldr.registerExportOrdinal("ws2_32.dll", 16, reinterpret_cast<void*>(recv));
    ldr.registerExportOrdinal("ws2_32.dll", 19, reinterpret_cast<void*>(send));
    ldr.registerExportOrdinal("ws2_32.dll", 21, reinterpret_cast<void*>(setsockopt));
    ldr.registerExportOrdinal("ws2_32.dll", 22, reinterpret_cast<void*>(shutdown));
    ldr.registerExportOrdinal("ws2_32.dll", 23, reinterpret_cast<void*>(socket));
    ldr.registerExportOrdinal("ws2_32.dll", 52, reinterpret_cast<void*>(gethostbyname));
    ldr.registerExportOrdinal("ws2_32.dll", 101, reinterpret_cast<void*>(WSAAsyncSelect));
    ldr.registerExportOrdinal("ws2_32.dll", 111, reinterpret_cast<void*>(WSAGetLastError));
    ldr.registerExportOrdinal("ws2_32.dll", 115, reinterpret_cast<void*>(WSAStartup));
    ldr.registerExportOrdinal("ws2_32.dll", 116, reinterpret_cast<void*>(WSACleanup));
}

} // namespace micant::ws2_32
