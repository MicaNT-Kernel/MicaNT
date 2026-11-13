#pragma once

// Mode-neutral WinHTTP client declarations for the native Winsock and DNS
// DLLs. The implementation resides in those DLLs, not in kernel headers or
// the hosted DNS/Winsock adapters.
#include <cstdint>

namespace micant::ws2_32 {
using SOCKET = uintptr_t;
inline constexpr SOCKET INVALID_SOCKET = ~SOCKET{0};
inline constexpr int SOCKET_ERROR = -1;
inline constexpr int AF_INET = 2;
inline constexpr int SOCK_STREAM = 1;
inline constexpr int IPPROTO_TCP = 6;
inline constexpr int SOL_SOCKET = 0xffff;
inline constexpr int SO_RCVTIMEO = 0x1006;
inline constexpr int SO_SNDTIMEO = 0x1005;
inline constexpr uint32_t INADDR_NONE = 0xffffffffu;
struct sockaddr { uint16_t sa_family; char sa_data[14]; };
struct in_addr { union { uint32_t S_addr; } S_un; };
struct sockaddr_in { int16_t sin_family; uint16_t sin_port; in_addr sin_addr; uint8_t sin_zero[8]; };
static_assert(sizeof(sockaddr_in) == 16);
struct WSADATA {
    uint16_t version, highVersion;
    uint16_t maxSockets, maxUdpDatagram;
    char* vendorInfo;
    char description[257];
    char status[129];
};
extern "C" __declspec(dllimport) int WSAStartup(uint16_t, WSADATA*) noexcept;
extern "C" __declspec(dllimport) int WSACleanup() noexcept;
extern "C" __declspec(dllimport) SOCKET socket(int, int, int) noexcept;
extern "C" __declspec(dllimport) int connect(SOCKET, const sockaddr*, int) noexcept;
extern "C" __declspec(dllimport) int send(SOCKET, const char*, int, int) noexcept;
extern "C" __declspec(dllimport) int recv(SOCKET, char*, int, int) noexcept;
extern "C" __declspec(dllimport) int closesocket(SOCKET) noexcept;
extern "C" __declspec(dllimport) int setsockopt(SOCKET, int, int, const char*, int) noexcept;
inline constexpr uint16_t htons(uint16_t value) noexcept {
    return static_cast<uint16_t>((value << 8) | (value >> 8));
}
inline uint32_t inet_addr(const char* text) noexcept {
    if (!text) return INADDR_NONE;
    uint32_t result = 0;
    for (unsigned part = 0; part < 4; ++part) {
        unsigned value = 0, digits = 0;
        while (*text >= '0' && *text <= '9') {
            if (++digits > 3 || (value = value * 10 + static_cast<unsigned>(*text++ - '0')) > 255)
                return INADDR_NONE;
        }
        if (!digits) return INADDR_NONE;
        result |= static_cast<uint32_t>(value) << (part * 8);
        if (part < 3) { if (*text++ != '.') return INADDR_NONE; }
        else if (*text) return INADDR_NONE;
    }
    return result;
}
}

namespace micant::dnsapi {
inline constexpr uint16_t DNS_TYPE_A = 1;
inline constexpr uint32_t DNS_STATUS_SUCCESS = 0;
struct DNS_RECORD_FLAGS { uint32_t value; };
struct DNS_A_DATA { uint32_t IpAddress; };
struct DNS_AAAA_DATA { uint8_t Ip6Address[16]; };
#pragma pack(push, 4)
struct DNS_RECORDA {
    DNS_RECORDA* pNext;
    const char* pName;
    uint16_t wType, wDataLength;
    DNS_RECORD_FLAGS Flags;
    uint32_t dwTtl, dwReserved;
    union { DNS_A_DATA A; DNS_AAAA_DATA AAAA; } Data;
};
#pragma pack(pop)
static_assert(sizeof(DNS_RECORDA) == 48);
extern "C" __declspec(dllimport) uint32_t DnsQuery_A(const char*, uint16_t, uint32_t,
                                                      void*, DNS_RECORDA**, void**) noexcept;
extern "C" __declspec(dllimport) uint32_t MicantDnsQueryWithTimeout(
    const char*, uint16_t, uint32_t, DNS_RECORDA**) noexcept;
extern "C" __declspec(dllimport) void DnsRecordListFree(void*, uint32_t) noexcept;
inline void DnsRecordListFreeA(DNS_RECORDA* records) noexcept { DnsRecordListFree(records, 0); }
}
