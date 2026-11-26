/**
 * @file ws2_32.cpp
 * @brief Sovereign WinSock2 Network DLL (C:\Windows\System32\ws2_32.dll) for MicaNT.
 *
 * Freestanding ISO C++23. Clean-Room design. ZERO direct `syscall` instructions!
 * Dynamically imports native NtNet* & NtIcmpSendEcho gateways strictly from ntdll.dll
 * via real PE32+ IAT chaining:
 *   App (ping/ipconfig/curl) -> ws2_32.dll (IAT) -> ntdll.dll (IAT) -> Ring 0 e1000 DMA!
 */

#include <cstdint>
#include <cstddef>

extern "C" {

__declspec(dllimport) uint64_t NtIcmpSendEcho(uint32_t destIp, const void* buffer, uint32_t size, uint32_t timeoutMs) noexcept;
__declspec(dllimport) uint64_t NtNetQueryAdapterInfo(void* outInfoBuf, uint32_t bufLen) noexcept;
__declspec(dllimport) uint64_t NtNetSocketCreate(void** outSocketHandle) noexcept;
__declspec(dllimport) uint64_t NtNetSocketConnect(void* hSocket, uint32_t ipv4NetOrder, uint16_t portHostOrder) noexcept;
__declspec(dllimport) int32_t  NtNetSocketSend(void* hSocket, const void* buf, uint32_t len) noexcept;
__declspec(dllimport) int32_t  NtNetSocketRecv(void* hSocket, void* buf, uint32_t maxLen) noexcept;
__declspec(dllimport) uint64_t NtNetSocketClose(void* hSocket) noexcept;

static constexpr uint64_t INVALID_SOCKET_VAL = ~0ULL;

#pragma pack(push, 1)
struct WSAData {
    uint16_t wVersion;
    uint16_t wHighVersion;
    uint16_t iMaxSockets;
    uint16_t iMaxUdpDg;
    char*    lpVendorInfo;
    char     szDescription[257];
    char     szSystemStatus[129];
};

struct SockAddrIn {
    uint16_t sin_family;
    uint16_t sin_port;   // Network byte order
    uint32_t sin_addr;   // Network byte order
    uint8_t  sin_zero[8];
};
#pragma pack(pop)

bool DllMain(void*, unsigned long, void*) noexcept {
    return true;
}

__declspec(dllexport) uint16_t htons(uint16_t v) noexcept {
    return static_cast<uint16_t>((v << 8) | (v >> 8));
}

__declspec(dllexport) uint16_t ntohs(uint16_t v) noexcept {
    return htons(v);
}

__declspec(dllexport) uint32_t htonl(uint32_t v) noexcept {
    return ((v & 0x000000FFu) << 24) |
           ((v & 0x0000FF00u) << 8)  |
           ((v & 0x00FF0000u) >> 8)  |
           ((v & 0xFF000000u) >> 24);
}

__declspec(dllexport) uint32_t ntohl(uint32_t v) noexcept {
    return htonl(v);
}

__declspec(dllexport) uint32_t inet_addr(const char* cp) noexcept {
    if (!cp) return 0xFFFFFFFFu;
    uint32_t octets[4]{0, 0, 0, 0};
    int idx = 0;
    bool hasDigit = false;
    for (size_t i = 0; cp[i]; ++i) {
        char c = cp[i];
        if (c >= '0' && c <= '9') {
            octets[idx] = octets[idx] * 10u + static_cast<uint32_t>(c - '0');
            if (octets[idx] > 255u) return 0xFFFFFFFFu;
            hasDigit = true;
        } else if (c == '.') {
            if (!hasDigit || idx >= 3) return 0xFFFFFFFFu;
            idx++;
            hasDigit = false;
        } else {
            return 0xFFFFFFFFu;
        }
    }
    if (idx != 3 || !hasDigit) return 0xFFFFFFFFu;
    return (octets[0] & 0xFFu) |
           ((octets[1] & 0xFFu) << 8) |
           ((octets[2] & 0xFFu) << 16) |
           ((octets[3] & 0xFFu) << 24);
}

__declspec(dllexport) int32_t WSAStartup(uint16_t wVersionRequested, WSAData* lpWSAData) noexcept {
    if (lpWSAData) {
        lpWSAData->wVersion = wVersionRequested ? wVersionRequested : 0x0202u;
        lpWSAData->wHighVersion = 0x0202u;
        lpWSAData->iMaxSockets = 64;
        lpWSAData->iMaxUdpDg = 1472;
        lpWSAData->lpVendorInfo = nullptr;
        const char desc[] = "MicaNT WinSock 2.2 (Intel e1000/I210 DMA)";
        const char stat[] = "Running";
        for (size_t i = 0; i < sizeof(desc); ++i) lpWSAData->szDescription[i] = desc[i];
        for (size_t i = 0; i < sizeof(stat); ++i) lpWSAData->szSystemStatus[i] = stat[i];
    }
    return 0;
}

__declspec(dllexport) int32_t WSACleanup() noexcept {
    return 0;
}

__declspec(dllexport) uint64_t socket(int32_t af, int32_t type, int32_t /*protocol*/) noexcept {
    if (af != 2 /* AF_INET */ || type != 1 /* SOCK_STREAM */) return INVALID_SOCKET_VAL;
    void* h = nullptr;
    if (NtNetSocketCreate(&h) != 0 || h == nullptr) return INVALID_SOCKET_VAL;
    return reinterpret_cast<uint64_t>(h);
}

__declspec(dllexport) int32_t connect(uint64_t s, const void* name, int32_t namelen) noexcept {
    if (s == INVALID_SOCKET_VAL || !name || namelen < static_cast<int32_t>(sizeof(SockAddrIn))) return -1;
    const auto* sa = static_cast<const SockAddrIn*>(name);
    if (sa->sin_family != 2 /* AF_INET */) return -1;
    uint16_t hostPort = ntohs(sa->sin_port);
    uint64_t st = NtNetSocketConnect(reinterpret_cast<void*>(s), sa->sin_addr, hostPort);
    return (st == 0) ? 0 : -1;
}

__declspec(dllexport) int32_t send(uint64_t s, const char* buf, int32_t len, int32_t /*flags*/) noexcept {
    if (s == INVALID_SOCKET_VAL || !buf || len < 0) return -1;
    return NtNetSocketSend(reinterpret_cast<void*>(s), buf, static_cast<uint32_t>(len));
}

__declspec(dllexport) int32_t recv(uint64_t s, char* buf, int32_t len, int32_t /*flags*/) noexcept {
    if (s == INVALID_SOCKET_VAL || !buf || len <= 0) return -1;
    return NtNetSocketRecv(reinterpret_cast<void*>(s), buf, static_cast<uint32_t>(len));
}

__declspec(dllexport) int32_t closesocket(uint64_t s) noexcept {
    if (s == INVALID_SOCKET_VAL) return -1;
    return (NtNetSocketClose(reinterpret_cast<void*>(s)) == 0) ? 0 : -1;
}

// Sovereign IPHLP / ICMP wrappers over ntdll (keeps ping.exe & ipconfig.exe at 0 direct syscalls!)
__declspec(dllexport) int32_t IcmpSendEchoMica(uint32_t destIp, void* replyInfoBuf, uint32_t replyInfoSize) noexcept {
    uint64_t st = NtIcmpSendEcho(destIp, replyInfoBuf, replyInfoSize, 1000);
    return (st == 0) ? 1 : 0;
}

__declspec(dllexport) int32_t QueryNetAdapterInfoMica(void* outInfoBuf, uint32_t bufLen) noexcept {
    uint64_t st = NtNetQueryAdapterInfo(outInfoBuf, bufLen);
    return (st == 0) ? 1 : 0;
}

} // extern "C"
