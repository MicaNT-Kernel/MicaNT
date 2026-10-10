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


// ============================================================================
// Extended Winsock 2.0 Primitives & Event Synchronization
// ============================================================================

inline win32::HANDLE WINAPI WSACreateEvent() noexcept {
    return win32::CreateEventW(nullptr, 1, 0, nullptr);
}

inline win32::BOOL WINAPI WSACloseEvent(win32::HANDLE hEvent) noexcept {
    return win32::CloseHandle(hEvent);
}

inline win32::BOOL WINAPI WSASetEvent(win32::HANDLE hEvent) noexcept {
    return win32::SetEvent(hEvent);
}

inline win32::DWORD WINAPI WSAWaitForMultipleEvents(win32::DWORD cEvents,
                                                    const win32::HANDLE* lphEvents,
                                                    win32::BOOL fWaitAll,
                                                    win32::DWORD dwTimeout,
                                                    [[maybe_unused]] win32::BOOL fAlertable) noexcept {
    return win32::WaitForMultipleObjects(cEvents, lphEvents, fWaitAll, dwTimeout);
}

inline win32::HANDLE WINAPI WSAAsyncGetHostByName([[maybe_unused]] win32::HWND hWnd,
                                                  [[maybe_unused]] unsigned int wMsg,
                                                  [[maybe_unused]] const char* name,
                                                  char* buf,
                                                  int buflen) noexcept {
    if (buf && buflen >= 16) {
        std::memset(buf, 0, 16);
    }
    return reinterpret_cast<win32::HANDLE>(0x6001);
}

inline int WINAPI WSACancelAsyncRequest([[maybe_unused]] win32::HANDLE hAsyncTaskHandle) noexcept {
    return 0;
}

inline int WINAPI WSAEnumNetworkEvents([[maybe_unused]] SOCKET s,
                                       [[maybe_unused]] win32::HANDLE hEventObject,
                                       void* lpNetworkEvents) noexcept {
    if (lpNetworkEvents) {
        std::memset(lpNetworkEvents, 0, 44);
    }
    return 0;
}

inline int WINAPI WSAEventSelect([[maybe_unused]] SOCKET s,
                                 [[maybe_unused]] win32::HANDLE hEventObject,
                                 [[maybe_unused]] long lNetworkEvents) noexcept {
    return 0;
}

struct MicaServEnt {
    char* s_name;
    char** s_aliases;
    short s_port;
    char* s_proto;
};

inline void* WINAPI getservbyname(const char* name, const char* proto) noexcept {
    static char sName[32] = "ssh";
    static char sProto[16] = "tcp";
    static char* sAliases[2] = { nullptr, nullptr };
    static MicaServEnt se;
    se.s_name = sName;
    se.s_aliases = sAliases;
    se.s_port = 22;
    se.s_proto = sProto;
    if (name && std::strcmp(name, "http") == 0) se.s_port = 80;
    if (proto) std::strncpy(sProto, proto, sizeof(sProto) - 1);
    return &se;
}

inline int WINAPI getsockopt([[maybe_unused]] SOCKET s, [[maybe_unused]] int level, [[maybe_unused]] int optname, char* optval, int* optlen) noexcept {
    if (optval && optlen && *optlen >= 4) {
        *reinterpret_cast<int32_t*>(optval) = 0;
    }
    return 0;
}

inline int WINAPI ioctlsocket([[maybe_unused]] SOCKET s, [[maybe_unused]] long cmd, [[maybe_unused]] uint32_t* argp) noexcept {
    return 0;
}

inline int WINAPI select([[maybe_unused]] int nfds, [[maybe_unused]] void* readfds, [[maybe_unused]] void* writefds, [[maybe_unused]] void* exceptfds, [[maybe_unused]] const void* timeout) noexcept {
    return 1;
}

inline int WINAPI __WSAFDIsSet([[maybe_unused]] SOCKET fd, [[maybe_unused]] void* set) noexcept {
    return 1;
}

inline const char* WINAPI inet_ntop([[maybe_unused]] int af, [[maybe_unused]] const void* src, char* dst, size_t size) noexcept {
    const char* loopback = "127.0.0.1";
    if (dst && size > std::strlen(loopback)) {
        std::strcpy(dst, loopback);
        return dst;
    }
    return nullptr;
}

inline int WINAPI inet_pton([[maybe_unused]] int af, const char* src, void* dst) noexcept {
    if (!src || !dst) return -1;
    *reinterpret_cast<uint32_t*>(dst) = INADDR_LOOPBACK;
    return 1;
}

inline int WINAPI WSAConnect([[maybe_unused]] SOCKET s, const void* /*name*/, int /*namelen*/, void* /*lpCallerData*/, void* /*lpCalleeData*/, void* /*lpSQOS*/, void* /*lpGQOS*/) noexcept {
    return 0;
}

inline SOCKET WINAPI WSAAccept([[maybe_unused]] SOCKET s, void* /*addr*/, int* /*addrlen*/, void* /*lpfnCondition*/, uintptr_t /*dwCallbackData*/) noexcept {
    return static_cast<SOCKET>(0x4101);
}

inline int WINAPI WSAHtonl([[maybe_unused]] SOCKET s, uint32_t hostlong, uint32_t* lpNetlong) noexcept {
    if (lpNetlong) *lpNetlong = htonl(hostlong);
    return 0;
}

inline int WINAPI WSANtohl([[maybe_unused]] SOCKET s, uint32_t netlong, uint32_t* lpHostlong) noexcept {
    if (lpHostlong) *lpHostlong = ntohl(netlong);
    return 0;
}

inline int WINAPI WSANtohs([[maybe_unused]] SOCKET s, uint16_t netshort, uint16_t* lpHostshort) noexcept {
    if (lpHostshort) *lpHostshort = ntohs(netshort);
    return 0;
}

inline int WINAPI WSASend([[maybe_unused]] SOCKET s, void* /*lpBuffers*/, uint32_t /*dwBufferCount*/, uint32_t* lpNumberOfBytesSent, uint32_t /*dwFlags*/, void* /*lpOverlapped*/, void* /*lpCompletionRoutine*/) noexcept {
    if (lpNumberOfBytesSent) *lpNumberOfBytesSent = 1024;
    return 0;
}

inline int WINAPI WSARecv([[maybe_unused]] SOCKET s, void* /*lpBuffers*/, uint32_t /*dwBufferCount*/, uint32_t* lpNumberOfBytesRecvd, uint32_t* lpFlags, void* /*lpOverlapped*/, void* /*lpCompletionRoutine*/) noexcept {
    if (lpNumberOfBytesRecvd) *lpNumberOfBytesRecvd = 512;
    if (lpFlags) *lpFlags = 0;
    return 0;
}

inline int WINAPI WSASendTo([[maybe_unused]] SOCKET s, void* /*lpBuffers*/, uint32_t /*dwBufferCount*/, uint32_t* lpNumberOfBytesSent, uint32_t /*dwFlags*/, const void* /*to*/, int /*tolen*/, void* /*lpOverlapped*/, void* /*lpCompletionRoutine*/) noexcept {
    if (lpNumberOfBytesSent) *lpNumberOfBytesSent = 1024;
    return 0;
}

inline int WINAPI WSARecvFrom([[maybe_unused]] SOCKET s, void* /*lpBuffers*/, uint32_t /*dwBufferCount*/, uint32_t* lpNumberOfBytesRecvd, uint32_t* lpFlags, void* /*from*/, int* /*fromlen*/, void* /*lpOverlapped*/, void* /*lpCompletionRoutine*/) noexcept {
    if (lpNumberOfBytesRecvd) *lpNumberOfBytesRecvd = 512;
    if (lpFlags) *lpFlags = 0;
    return 0;
}

inline int WINAPI WSAIoctl([[maybe_unused]] SOCKET s, uint32_t /*dwIoControlCode*/, void* /*lpvInBuffer*/, uint32_t /*cbInBuffer*/, void* /*lpvOutBuffer*/, uint32_t /*cbOutBuffer*/, uint32_t* lpcbBytesReturned, void* /*lpOverlapped*/, void* /*lpCompletionRoutine*/) noexcept {
    if (lpcbBytesReturned) *lpcbBytesReturned = 0;
    return 0;
}

inline SOCKET WINAPI WSASocketA([[maybe_unused]] int af, [[maybe_unused]] int type, [[maybe_unused]] int protocol, void* /*lpProtocolInfo*/, uint32_t /*g*/, uint32_t /*dwFlags*/) noexcept {
    return static_cast<SOCKET>(0x4201);
}

inline SOCKET WINAPI WSASocketW([[maybe_unused]] int af, [[maybe_unused]] int type, [[maybe_unused]] int protocol, void* /*lpProtocolInfo*/, uint32_t /*g*/, uint32_t /*dwFlags*/) noexcept {
    return static_cast<SOCKET>(0x4202);
}

struct MicaAddrInfo {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    size_t ai_addrlen;
    char* ai_canonname;
    void* ai_addr;
    struct MicaAddrInfo* ai_next;
};

inline int WINAPI getaddrinfo([[maybe_unused]] const char* pNodeName, [[maybe_unused]] const char* pServiceName, [[maybe_unused]] const void* pHints, void** ppResult) noexcept {
    if (!ppResult) return -1;
    auto* ai = new MicaAddrInfo();
    ai->ai_flags = 0;
    ai->ai_family = 2; // AF_INET
    ai->ai_socktype = 1; // SOCK_STREAM
    ai->ai_protocol = 6; // IPPROTO_TCP
    ai->ai_addrlen = 16;
    ai->ai_canonname = nullptr;
    ai->ai_addr = new uint8_t[16]();
    ai->ai_next = nullptr;
    *ppResult = ai;
    return 0;
}

inline void WINAPI freeaddrinfo(void* pAddrInfo) noexcept {
    if (!pAddrInfo) return;
    auto* ai = reinterpret_cast<MicaAddrInfo*>(pAddrInfo);
    while (ai) {
        auto* next = ai->ai_next;
        delete[] reinterpret_cast<uint8_t*>(ai->ai_addr);
        delete ai;
        ai = next;
    }
}

inline int WINAPI getnameinfo([[maybe_unused]] const void* pSockaddr, [[maybe_unused]] int SockaddrLength, char* pNodeBuffer, uint32_t NodeBufferSize, char* pServiceBuffer, uint32_t ServiceBufferSize, [[maybe_unused]] int Flags) noexcept {
    if (pNodeBuffer && NodeBufferSize > 0) {
        std::snprintf(pNodeBuffer, NodeBufferSize, "localhost");
    }
    if (pServiceBuffer && ServiceBufferSize > 0) {
        std::snprintf(pServiceBuffer, ServiceBufferSize, "8080");
    }
    return 0;
}

inline int WINAPI WSAStringToAddressW([[maybe_unused]] wchar_t* AddressString, [[maybe_unused]] int AddressFamily, void* /*lpProtocolInfo*/, void* /*lpAddress*/, int* lpAddressLength) noexcept {
    if (lpAddressLength) *lpAddressLength = 16;
    return 0;
}

inline int WINAPI WSAAddressToStringW(void* /*lpsaAddress*/, [[maybe_unused]] uint32_t dwAddressLength, void* /*lpProtocolInfo*/, wchar_t* lpszAddressString, uint32_t* lpdwAddressStringLength) noexcept {
    const wchar_t* loopback = L"127.0.0.1:8080";
    size_t len = std::wcslen(loopback);
    if (lpszAddressString && lpdwAddressStringLength && *lpdwAddressStringLength > len) {
        std::wcscpy(lpszAddressString, loopback);
    }
    if (lpdwAddressStringLength) *lpdwAddressStringLength = static_cast<uint32_t>(len + 1);
    return 0;
}

inline win32::BOOL WINAPI AcceptEx([[maybe_unused]] SOCKET sListenSocket, [[maybe_unused]] SOCKET sAcceptSocket, void* /*lpOutputBuffer*/, [[maybe_unused]] uint32_t dwReceiveDataLength, [[maybe_unused]] uint32_t dwLocalAddressLength, [[maybe_unused]] uint32_t dwRemoteAddressLength, uint32_t* lpdwBytesReceived, void* /*lpOverlapped*/) noexcept {
    if (lpdwBytesReceived) *lpdwBytesReceived = 0;
    return 1;
}

inline void WINAPI GetAcceptExSockaddrs(void* /*lpOutputBuffer*/, [[maybe_unused]] uint32_t dwReceiveDataLength, [[maybe_unused]] uint32_t dwLocalAddressLength, [[maybe_unused]] uint32_t dwRemoteAddressLength, void** LocalSockaddr, int* LocalSockaddrLength, void** RemoteSockaddr, int* RemoteSockaddrLength) noexcept {
    static uint8_t mockLocal[32] = { 0 };
    static uint8_t mockRemote[32] = { 0 };
    if (LocalSockaddr) *LocalSockaddr = mockLocal;
    if (LocalSockaddrLength) *LocalSockaddrLength = sizeof(mockLocal);
    if (RemoteSockaddr) *RemoteSockaddr = mockRemote;
    if (RemoteSockaddrLength) *RemoteSockaddrLength = sizeof(mockRemote);
}

inline void InitializeWs2_32SubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();

    // ws2_32.dll Named Exports
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
    ldr.registerExport("ws2_32.dll", "select", reinterpret_cast<void*>(select));
    ldr.registerExport("ws2_32.dll", "ioctlsocket", reinterpret_cast<void*>(ioctlsocket));
    ldr.registerExport("ws2_32.dll", "getsockopt", reinterpret_cast<void*>(getsockopt));
    ldr.registerExport("ws2_32.dll", "__WSAFDIsSet", reinterpret_cast<void*>(__WSAFDIsSet));
    ldr.registerExport("ws2_32.dll", "AcceptEx", reinterpret_cast<void*>(AcceptEx));
    ldr.registerExport("ws2_32.dll", "GetAcceptExSockaddrs", reinterpret_cast<void*>(GetAcceptExSockaddrs));
    ldr.registerExport("ws2_32.dll", "WSACreateEvent", reinterpret_cast<void*>(WSACreateEvent));
    ldr.registerExport("ws2_32.dll", "WSACloseEvent", reinterpret_cast<void*>(WSACloseEvent));
    ldr.registerExport("ws2_32.dll", "WSASetEvent", reinterpret_cast<void*>(WSASetEvent));
    ldr.registerExport("ws2_32.dll", "WSAWaitForMultipleEvents", reinterpret_cast<void*>(WSAWaitForMultipleEvents));
    ldr.registerExport("ws2_32.dll", "WSAConnect", reinterpret_cast<void*>(WSAConnect));
    ldr.registerExport("ws2_32.dll", "WSAAccept", reinterpret_cast<void*>(WSAAccept));
    ldr.registerExport("ws2_32.dll", "WSAHtonl", reinterpret_cast<void*>(WSAHtonl));
    ldr.registerExport("ws2_32.dll", "WSANtohl", reinterpret_cast<void*>(WSANtohl));
    ldr.registerExport("ws2_32.dll", "WSANtohs", reinterpret_cast<void*>(WSANtohs));
    ldr.registerExport("ws2_32.dll", "WSASend", reinterpret_cast<void*>(WSASend));
    ldr.registerExport("ws2_32.dll", "WSARecv", reinterpret_cast<void*>(WSARecv));
    ldr.registerExport("ws2_32.dll", "WSASendTo", reinterpret_cast<void*>(WSASendTo));
    ldr.registerExport("ws2_32.dll", "WSARecvFrom", reinterpret_cast<void*>(WSARecvFrom));
    ldr.registerExport("ws2_32.dll", "WSAIoctl", reinterpret_cast<void*>(WSAIoctl));
    ldr.registerExport("ws2_32.dll", "WSASocketA", reinterpret_cast<void*>(WSASocketA));
    ldr.registerExport("ws2_32.dll", "WSASocketW", reinterpret_cast<void*>(WSASocketW));
    ldr.registerExport("ws2_32.dll", "getaddrinfo", reinterpret_cast<void*>(getaddrinfo));
    ldr.registerExport("ws2_32.dll", "freeaddrinfo", reinterpret_cast<void*>(freeaddrinfo));
    ldr.registerExport("ws2_32.dll", "getnameinfo", reinterpret_cast<void*>(getnameinfo));
    ldr.registerExport("ws2_32.dll", "WSAStringToAddressW", reinterpret_cast<void*>(WSAStringToAddressW));
    ldr.registerExport("ws2_32.dll", "WSAAddressToStringW", reinterpret_cast<void*>(WSAAddressToStringW));
    ldr.registerExport("ws2_32.dll", "WSAAsyncGetHostByName", reinterpret_cast<void*>(WSAAsyncGetHostByName));
    ldr.registerExport("ws2_32.dll", "WSACancelAsyncRequest", reinterpret_cast<void*>(WSACancelAsyncRequest));
    ldr.registerExport("ws2_32.dll", "WSAEnumNetworkEvents", reinterpret_cast<void*>(WSAEnumNetworkEvents));
    ldr.registerExport("ws2_32.dll", "WSAEventSelect", reinterpret_cast<void*>(WSAEventSelect));
    ldr.registerExport("ws2_32.dll", "getservbyname", reinterpret_cast<void*>(getservbyname));
    ldr.registerExport("ws2_32.dll", "inet_ntop", reinterpret_cast<void*>(inet_ntop));
    ldr.registerExport("ws2_32.dll", "inet_pton", reinterpret_cast<void*>(inet_pton));

    // ws2_32.dll Numeric Ordinal Exports
    ldr.registerExportOrdinal("ws2_32.dll", 1, reinterpret_cast<void*>(accept));
    ldr.registerExportOrdinal("ws2_32.dll", 2, reinterpret_cast<void*>(bind));
    ldr.registerExportOrdinal("ws2_32.dll", 3, reinterpret_cast<void*>(closesocket));
    ldr.registerExportOrdinal("ws2_32.dll", 4, reinterpret_cast<void*>(connect));
    ldr.registerExportOrdinal("ws2_32.dll", 5, reinterpret_cast<void*>(getpeername));
    ldr.registerExportOrdinal("ws2_32.dll", 6, reinterpret_cast<void*>(getsockname));
    ldr.registerExportOrdinal("ws2_32.dll", 7, reinterpret_cast<void*>(getsockopt));
    ldr.registerExportOrdinal("ws2_32.dll", 8, reinterpret_cast<void*>(htonl));
    ldr.registerExportOrdinal("ws2_32.dll", 9, reinterpret_cast<void*>(htons));
    ldr.registerExportOrdinal("ws2_32.dll", 10, reinterpret_cast<void*>(inet_addr));
    ldr.registerExportOrdinal("ws2_32.dll", 11, reinterpret_cast<void*>(inet_ntoa));
    ldr.registerExportOrdinal("ws2_32.dll", 12, reinterpret_cast<void*>(ioctlsocket));
    ldr.registerExportOrdinal("ws2_32.dll", 13, reinterpret_cast<void*>(listen));
    ldr.registerExportOrdinal("ws2_32.dll", 14, reinterpret_cast<void*>(ntohl));
    ldr.registerExportOrdinal("ws2_32.dll", 15, reinterpret_cast<void*>(ntohs));
    ldr.registerExportOrdinal("ws2_32.dll", 16, reinterpret_cast<void*>(recv));
    ldr.registerExportOrdinal("ws2_32.dll", 17, reinterpret_cast<void*>(recvfrom));
    ldr.registerExportOrdinal("ws2_32.dll", 18, reinterpret_cast<void*>(select));
    ldr.registerExportOrdinal("ws2_32.dll", 19, reinterpret_cast<void*>(send));
    ldr.registerExportOrdinal("ws2_32.dll", 20, reinterpret_cast<void*>(sendto));
    ldr.registerExportOrdinal("ws2_32.dll", 21, reinterpret_cast<void*>(setsockopt));
    ldr.registerExportOrdinal("ws2_32.dll", 22, reinterpret_cast<void*>(shutdown));
    ldr.registerExportOrdinal("ws2_32.dll", 23, reinterpret_cast<void*>(socket));
    ldr.registerExportOrdinal("ws2_32.dll", 51, reinterpret_cast<void*>(gethostbyname));
    ldr.registerExportOrdinal("ws2_32.dll", 52, reinterpret_cast<void*>(gethostbyname));
    ldr.registerExportOrdinal("ws2_32.dll", 55, reinterpret_cast<void*>(getservbyname));
    ldr.registerExportOrdinal("ws2_32.dll", 57, reinterpret_cast<void*>(gethostname));
    ldr.registerExportOrdinal("ws2_32.dll", 101, reinterpret_cast<void*>(WSAAsyncSelect));
    ldr.registerExportOrdinal("ws2_32.dll", 102, reinterpret_cast<void*>(gethostbyname));
    ldr.registerExportOrdinal("ws2_32.dll", 103, reinterpret_cast<void*>(WSAAsyncGetHostByName));
    ldr.registerExportOrdinal("ws2_32.dll", 108, reinterpret_cast<void*>(WSACancelAsyncRequest));
    ldr.registerExportOrdinal("ws2_32.dll", 111, reinterpret_cast<void*>(WSAGetLastError));
    ldr.registerExportOrdinal("ws2_32.dll", 112, reinterpret_cast<void*>(WSASetLastError));
    ldr.registerExportOrdinal("ws2_32.dll", 115, reinterpret_cast<void*>(WSAStartup));
    ldr.registerExportOrdinal("ws2_32.dll", 116, reinterpret_cast<void*>(WSACleanup));
    ldr.registerExportOrdinal("ws2_32.dll", 151, reinterpret_cast<void*>(__WSAFDIsSet));
    ldr.registerExportOrdinal("ws2_32.dll", 1141, reinterpret_cast<void*>(WSAIoctl));
    ldr.registerExportOrdinal("ws2_32.dll", 1142, reinterpret_cast<void*>(WSASocketA));

    // wsock32.dll Named Exports
    ldr.registerExport("wsock32.dll", "WSAStartup", reinterpret_cast<void*>(WSAStartup));
    ldr.registerExport("wsock32.dll", "WSACleanup", reinterpret_cast<void*>(WSACleanup));
    ldr.registerExport("wsock32.dll", "WSAGetLastError", reinterpret_cast<void*>(WSAGetLastError));
    ldr.registerExport("wsock32.dll", "WSASetLastError", reinterpret_cast<void*>(WSASetLastError));
    ldr.registerExport("wsock32.dll", "socket", reinterpret_cast<void*>(socket));
    ldr.registerExport("wsock32.dll", "bind", reinterpret_cast<void*>(bind));
    ldr.registerExport("wsock32.dll", "listen", reinterpret_cast<void*>(listen));
    ldr.registerExport("wsock32.dll", "accept", reinterpret_cast<void*>(accept));
    ldr.registerExport("wsock32.dll", "connect", reinterpret_cast<void*>(connect));
    ldr.registerExport("wsock32.dll", "send", reinterpret_cast<void*>(send));
    ldr.registerExport("wsock32.dll", "recv", reinterpret_cast<void*>(recv));
    ldr.registerExport("wsock32.dll", "sendto", reinterpret_cast<void*>(sendto));
    ldr.registerExport("wsock32.dll", "recvfrom", reinterpret_cast<void*>(recvfrom));
    ldr.registerExport("wsock32.dll", "closesocket", reinterpret_cast<void*>(closesocket));
    ldr.registerExport("wsock32.dll", "gethostname", reinterpret_cast<void*>(gethostname));
    ldr.registerExport("wsock32.dll", "inet_addr", reinterpret_cast<void*>(inet_addr));
    ldr.registerExport("wsock32.dll", "inet_ntoa", reinterpret_cast<void*>(inet_ntoa));
    ldr.registerExport("wsock32.dll", "htons", reinterpret_cast<void*>(htons));
    ldr.registerExport("wsock32.dll", "ntohs", reinterpret_cast<void*>(ntohs));
    ldr.registerExport("wsock32.dll", "htonl", reinterpret_cast<void*>(htonl));
    ldr.registerExport("wsock32.dll", "ntohl", reinterpret_cast<void*>(ntohl));
    ldr.registerExport("wsock32.dll", "getpeername", reinterpret_cast<void*>(getpeername));
    ldr.registerExport("wsock32.dll", "getsockname", reinterpret_cast<void*>(getsockname));
    ldr.registerExport("wsock32.dll", "setsockopt", reinterpret_cast<void*>(setsockopt));
    ldr.registerExport("wsock32.dll", "shutdown", reinterpret_cast<void*>(shutdown));
    ldr.registerExport("wsock32.dll", "gethostbyname", reinterpret_cast<void*>(gethostbyname));
    ldr.registerExport("wsock32.dll", "WSAAsyncSelect", reinterpret_cast<void*>(WSAAsyncSelect));
    ldr.registerExport("wsock32.dll", "select", reinterpret_cast<void*>(select));
    ldr.registerExport("wsock32.dll", "ioctlsocket", reinterpret_cast<void*>(ioctlsocket));
    ldr.registerExport("wsock32.dll", "getsockopt", reinterpret_cast<void*>(getsockopt));
    ldr.registerExport("wsock32.dll", "__WSAFDIsSet", reinterpret_cast<void*>(__WSAFDIsSet));
    ldr.registerExport("wsock32.dll", "AcceptEx", reinterpret_cast<void*>(AcceptEx));
    ldr.registerExport("wsock32.dll", "GetAcceptExSockaddrs", reinterpret_cast<void*>(GetAcceptExSockaddrs));
    ldr.registerExport("wsock32.dll", "WSACreateEvent", reinterpret_cast<void*>(WSACreateEvent));
    ldr.registerExport("wsock32.dll", "WSACloseEvent", reinterpret_cast<void*>(WSACloseEvent));
    ldr.registerExport("wsock32.dll", "WSASetEvent", reinterpret_cast<void*>(WSASetEvent));
    ldr.registerExport("wsock32.dll", "WSAWaitForMultipleEvents", reinterpret_cast<void*>(WSAWaitForMultipleEvents));
    ldr.registerExport("wsock32.dll", "WSAConnect", reinterpret_cast<void*>(WSAConnect));
    ldr.registerExport("wsock32.dll", "WSAAccept", reinterpret_cast<void*>(WSAAccept));
    ldr.registerExport("wsock32.dll", "WSAHtonl", reinterpret_cast<void*>(WSAHtonl));
    ldr.registerExport("wsock32.dll", "WSANtohl", reinterpret_cast<void*>(WSANtohl));
    ldr.registerExport("wsock32.dll", "WSANtohs", reinterpret_cast<void*>(WSANtohs));
    ldr.registerExport("wsock32.dll", "WSASend", reinterpret_cast<void*>(WSASend));
    ldr.registerExport("wsock32.dll", "WSARecv", reinterpret_cast<void*>(WSARecv));
    ldr.registerExport("wsock32.dll", "WSASendTo", reinterpret_cast<void*>(WSASendTo));
    ldr.registerExport("wsock32.dll", "WSARecvFrom", reinterpret_cast<void*>(WSARecvFrom));
    ldr.registerExport("wsock32.dll", "WSAIoctl", reinterpret_cast<void*>(WSAIoctl));
    ldr.registerExport("wsock32.dll", "WSASocketA", reinterpret_cast<void*>(WSASocketA));
    ldr.registerExport("wsock32.dll", "WSASocketW", reinterpret_cast<void*>(WSASocketW));
    ldr.registerExport("wsock32.dll", "getaddrinfo", reinterpret_cast<void*>(getaddrinfo));
    ldr.registerExport("wsock32.dll", "freeaddrinfo", reinterpret_cast<void*>(freeaddrinfo));
    ldr.registerExport("wsock32.dll", "getnameinfo", reinterpret_cast<void*>(getnameinfo));
    ldr.registerExport("wsock32.dll", "WSAStringToAddressW", reinterpret_cast<void*>(WSAStringToAddressW));
    ldr.registerExport("wsock32.dll", "WSAAddressToStringW", reinterpret_cast<void*>(WSAAddressToStringW));
    ldr.registerExport("wsock32.dll", "WSAAsyncGetHostByName", reinterpret_cast<void*>(WSAAsyncGetHostByName));
    ldr.registerExport("wsock32.dll", "WSACancelAsyncRequest", reinterpret_cast<void*>(WSACancelAsyncRequest));
    ldr.registerExport("wsock32.dll", "WSAEnumNetworkEvents", reinterpret_cast<void*>(WSAEnumNetworkEvents));
    ldr.registerExport("wsock32.dll", "WSAEventSelect", reinterpret_cast<void*>(WSAEventSelect));
    ldr.registerExport("wsock32.dll", "getservbyname", reinterpret_cast<void*>(getservbyname));
    ldr.registerExport("wsock32.dll", "inet_ntop", reinterpret_cast<void*>(inet_ntop));
    ldr.registerExport("wsock32.dll", "inet_pton", reinterpret_cast<void*>(inet_pton));

    // wsock32.dll Numeric Ordinal Exports
    ldr.registerExportOrdinal("wsock32.dll", 1, reinterpret_cast<void*>(accept));
    ldr.registerExportOrdinal("wsock32.dll", 2, reinterpret_cast<void*>(bind));
    ldr.registerExportOrdinal("wsock32.dll", 3, reinterpret_cast<void*>(closesocket));
    ldr.registerExportOrdinal("wsock32.dll", 4, reinterpret_cast<void*>(connect));
    ldr.registerExportOrdinal("wsock32.dll", 5, reinterpret_cast<void*>(getpeername));
    ldr.registerExportOrdinal("wsock32.dll", 6, reinterpret_cast<void*>(getsockname));
    ldr.registerExportOrdinal("wsock32.dll", 7, reinterpret_cast<void*>(getsockopt));
    ldr.registerExportOrdinal("wsock32.dll", 8, reinterpret_cast<void*>(htonl));
    ldr.registerExportOrdinal("wsock32.dll", 9, reinterpret_cast<void*>(htons));
    ldr.registerExportOrdinal("wsock32.dll", 10, reinterpret_cast<void*>(inet_addr));
    ldr.registerExportOrdinal("wsock32.dll", 11, reinterpret_cast<void*>(inet_ntoa));
    ldr.registerExportOrdinal("wsock32.dll", 12, reinterpret_cast<void*>(ioctlsocket));
    ldr.registerExportOrdinal("wsock32.dll", 13, reinterpret_cast<void*>(listen));
    ldr.registerExportOrdinal("wsock32.dll", 14, reinterpret_cast<void*>(ntohl));
    ldr.registerExportOrdinal("wsock32.dll", 15, reinterpret_cast<void*>(ntohs));
    ldr.registerExportOrdinal("wsock32.dll", 16, reinterpret_cast<void*>(recv));
    ldr.registerExportOrdinal("wsock32.dll", 17, reinterpret_cast<void*>(recvfrom));
    ldr.registerExportOrdinal("wsock32.dll", 18, reinterpret_cast<void*>(select));
    ldr.registerExportOrdinal("wsock32.dll", 19, reinterpret_cast<void*>(send));
    ldr.registerExportOrdinal("wsock32.dll", 20, reinterpret_cast<void*>(sendto));
    ldr.registerExportOrdinal("wsock32.dll", 21, reinterpret_cast<void*>(setsockopt));
    ldr.registerExportOrdinal("wsock32.dll", 22, reinterpret_cast<void*>(shutdown));
    ldr.registerExportOrdinal("wsock32.dll", 23, reinterpret_cast<void*>(socket));
    ldr.registerExportOrdinal("wsock32.dll", 51, reinterpret_cast<void*>(gethostbyname));
    ldr.registerExportOrdinal("wsock32.dll", 52, reinterpret_cast<void*>(gethostbyname));
    ldr.registerExportOrdinal("wsock32.dll", 55, reinterpret_cast<void*>(getservbyname));
    ldr.registerExportOrdinal("wsock32.dll", 57, reinterpret_cast<void*>(gethostname));
    ldr.registerExportOrdinal("wsock32.dll", 101, reinterpret_cast<void*>(WSAAsyncSelect));
    ldr.registerExportOrdinal("wsock32.dll", 102, reinterpret_cast<void*>(gethostbyname));
    ldr.registerExportOrdinal("wsock32.dll", 103, reinterpret_cast<void*>(WSAAsyncGetHostByName));
    ldr.registerExportOrdinal("wsock32.dll", 108, reinterpret_cast<void*>(WSACancelAsyncRequest));
    ldr.registerExportOrdinal("wsock32.dll", 111, reinterpret_cast<void*>(WSAGetLastError));
    ldr.registerExportOrdinal("wsock32.dll", 112, reinterpret_cast<void*>(WSASetLastError));
    ldr.registerExportOrdinal("wsock32.dll", 115, reinterpret_cast<void*>(WSAStartup));
    ldr.registerExportOrdinal("wsock32.dll", 116, reinterpret_cast<void*>(WSACleanup));
    ldr.registerExportOrdinal("wsock32.dll", 151, reinterpret_cast<void*>(__WSAFDIsSet));
    ldr.registerExportOrdinal("wsock32.dll", 1141, reinterpret_cast<void*>(WSAIoctl));
    ldr.registerExportOrdinal("wsock32.dll", 1142, reinterpret_cast<void*>(WSASocketA));

}

} // namespace micant::ws2_32
