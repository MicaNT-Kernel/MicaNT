/**
 * @file ipconfig_app.cpp
 * @brief Sovereign Dynamic Ring-3 TCP/IP & DHCP Configuration Utility (C:\Windows\System32\ipconfig.exe).
 *
 * ZERO direct syscall instructions! Dynamically imports KERNEL32.dll + WS2_32.dll
 * (chaining through NTDLL.dll -> Ring 0 BareMetalE1000Engine PCIe DMA + RFC 2131 DHCP).
 *
 * Usage:
 *   ipconfig.exe         - Show live PCIe e1000 adapter, ARP, DHCP lease, DNS, & DMA telemetry
 *   ipconfig.exe /renew  - Perform live 4-way RFC 2131 DHCP DISCOVER/OFFER/REQUEST/ACK renewal!
 */

#include <cstdint>
#include <cstddef>

extern "C" {

void* memset(void* dst, int c, size_t n) noexcept {
    auto* d = static_cast<unsigned char*>(dst);
    while (n--) *d++ = static_cast<unsigned char>(c);
    return dst;
}

__declspec(dllimport) void*       GetStdHandle(uint32_t nStdHandle) noexcept;
__declspec(dllimport) int32_t     WriteFile(void* hFile, const void* buf, uint32_t len, uint32_t* written, void* ov) noexcept;
__declspec(dllimport) const char* GetCommandLineA() noexcept;
__declspec(dllimport) void        ExitProcess(uint32_t uExitCode) noexcept;
__declspec(dllimport) int32_t     QueryNetAdapterInfoMica(void* outInfoBuf, uint32_t bufLen) noexcept;
__declspec(dllimport) int32_t     DhcpRenewMica(void* outInfoBuf, uint32_t bufLen) noexcept;

} // extern "C"

namespace {

static constexpr uint32_t STD_OUTPUT_HANDLE = 0xFFFFFFF5u;

#pragma pack(push, 1)
struct NetAdapterInfo {
    uint32_t present;
    uint32_t linkUp;
    uint16_t vendorId;
    uint16_t deviceId;
    uint8_t  pciBus;
    uint8_t  pciDev;
    uint8_t  pciFunc;
    uint8_t  reserved0;
    uint8_t  mac[6];
    uint8_t  gatewayMac[6];
    uint32_t ipv4Addr;
    uint32_t subnetMask;
    uint32_t gatewayIp;
    uint32_t dnsIp;
    uint64_t txPackets;
    uint64_t rxPackets;
    uint64_t arpResolved;
    uint64_t icmpReplies;
    uint64_t tcpHandshakes;
    uint32_t dhcpBound;
    uint32_t dhcpServerIp;
    uint32_t dhcpLeaseSeconds;
    uint32_t dhcpRenewCount;
    uint64_t udpPackets;
    uint64_t dnsResolved;
};
#pragma pack(pop)

static size_t StrLen(const char* s) noexcept { size_t n = 0; while (s[n]) ++n; return n; }

static bool ContainsToken(const char* hay, const char* needle) noexcept {
    if (!hay || !needle) return false;
    size_t nl = StrLen(needle);
    for (size_t i = 0; hay[i]; ++i) {
        size_t j = 0;
        while (j < nl && hay[i + j]) {
            char a = hay[i + j], b = needle[j];
            if (a >= 'A' && a <= 'Z') a += ('a' - 'A');
            if (b >= 'A' && b <= 'Z') b += ('a' - 'A');
            if (a != b) break;
            ++j;
        }
        if (j == nl) return true;
    }
    return false;
}

static void AppendStr(char* dst, size_t cap, const char* src) noexcept {
    size_t d = StrLen(dst);
    for (size_t i = 0; src[i] && d + 1 < cap; ++i) dst[d++] = src[i];
    dst[d] = '\0';
}

static void AppendU64(char* dst, size_t cap, uint64_t v) noexcept {
    char tmp[24]; size_t n = 0;
    if (v == 0) tmp[n++] = '0';
    else { while (v > 0 && n < 22) { tmp[n++] = static_cast<char>('0' + (v % 10)); v /= 10; } }
    char rev[24];
    for (size_t i = 0; i < n; ++i) rev[i] = tmp[n - 1 - i];
    rev[n] = '\0';
    AppendStr(dst, cap, rev);
}

static void AppendHexByte(char* dst, size_t cap, uint8_t b) noexcept {
    static const char hx[] = "0123456789ABCDEF";
    char s[3] = {hx[(b >> 4) & 0xF], hx[b & 0xF], '\0'};
    AppendStr(dst, cap, s);
}

static void AppendHex16(char* dst, size_t cap, uint16_t v) noexcept {
    AppendHexByte(dst, cap, static_cast<uint8_t>(v >> 8));
    AppendHexByte(dst, cap, static_cast<uint8_t>(v & 0xFF));
}

static void AppendMac(char* dst, size_t cap, const uint8_t m[6]) noexcept {
    for (int i = 0; i < 6; ++i) {
        if (i > 0) AppendStr(dst, cap, "-");
        AppendHexByte(dst, cap, m[i]);
    }
}

static void AppendIpv4(char* dst, size_t cap, uint32_t ip) noexcept {
    AppendU64(dst, cap, ip & 0xFFu); AppendStr(dst, cap, ".");
    AppendU64(dst, cap, (ip >> 8) & 0xFFu); AppendStr(dst, cap, ".");
    AppendU64(dst, cap, (ip >> 16) & 0xFFu); AppendStr(dst, cap, ".");
    AppendU64(dst, cap, (ip >> 24) & 0xFFu);
}

static void PrintLine(void* hOut, const char* s) noexcept {
    char buf[196];
    size_t n = 0;
    while (s[n] && n + 3 < sizeof(buf)) { buf[n] = s[n]; ++n; }
    buf[n++] = '\r'; buf[n++] = '\n';
    uint32_t wr = 0;
    WriteFile(hOut, buf, static_cast<uint32_t>(n), &wr, nullptr);
}

} // namespace

extern "C" void WinMainEntry() noexcept {
    void* hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    const char* cmdLine = GetCommandLineA();
    bool doRenew = ContainsToken(cmdLine, "/renew") || ContainsToken(cmdLine, "-renew");

    NetAdapterInfo info{};
    if (doRenew) {
        PrintLine(hOut, "[DHCP] Renewing IPv4 lease via RFC 2131 (DISCOVER -> OFFER -> REQUEST -> ACK)...");
        if (!DhcpRenewMica(&info, sizeof(info))) {
            PrintLine(hOut, "[DHCP] ERROR: DHCP server did not respond to 4-way handshake.");
            ExitProcess(1);
        }
        PrintLine(hOut, "[DHCP] 4-Way Lease Renewed Successfully (ACK Received)!");
    } else if (!QueryNetAdapterInfoMica(&info, sizeof(info)) || !info.present) {
        PrintLine(hOut, "MicaNT IP Configuration: No active PCIe Ethernet adapter detected.");
        ExitProcess(1);
    }

    PrintLine(hOut, "MicaNT IP Configuration (Live Hardware PCIe DMA Adapter)");
    char line[160]{};
    AppendStr(line, sizeof(line), "  Adapter PCIe . . . : ");
    AppendU64(line, sizeof(line), info.pciBus); AppendStr(line, sizeof(line), ":");
    AppendU64(line, sizeof(line), info.pciDev); AppendStr(line, sizeof(line), ".");
    AppendU64(line, sizeof(line), info.pciFunc); AppendStr(line, sizeof(line), " [");
    AppendHex16(line, sizeof(line), info.vendorId); AppendStr(line, sizeof(line), ":");
    AppendHex16(line, sizeof(line), info.deviceId); AppendStr(line, sizeof(line), "] Intel PRO/1000 (Link=");
    AppendStr(line, sizeof(line), info.linkUp ? "UP 1000Mbps)" : "DOWN)");
    PrintLine(hOut, line);

    line[0] = '\0';
    AppendStr(line, sizeof(line), "  Physical Address . : ");
    AppendMac(line, sizeof(line), info.mac);
    AppendStr(line, sizeof(line), "  | Gateway MAC (ARP): ");
    AppendMac(line, sizeof(line), info.gatewayMac);
    PrintLine(hOut, line);

    line[0] = '\0';
    AppendStr(line, sizeof(line), "  IPv4 Address . . . : ");
    AppendIpv4(line, sizeof(line), info.ipv4Addr);
    AppendStr(line, sizeof(line), "  Mask: ");
    AppendIpv4(line, sizeof(line), info.subnetMask);
    AppendStr(line, sizeof(line), "  GW: ");
    AppendIpv4(line, sizeof(line), info.gatewayIp);
    PrintLine(hOut, line);

    line[0] = '\0';
    AppendStr(line, sizeof(line), "  DHCP & DNS . . . . : ");
    if (info.dhcpBound) {
        AppendStr(line, sizeof(line), "DHCP=BOUND (Srv ");
        AppendIpv4(line, sizeof(line), info.dhcpServerIp);
        AppendStr(line, sizeof(line), ", Lease=");
        AppendU64(line, sizeof(line), info.dhcpLeaseSeconds);
        AppendStr(line, sizeof(line), "s, #");
        AppendU64(line, sizeof(line), info.dhcpRenewCount);
        AppendStr(line, sizeof(line), ") | DNS=");
    } else {
        AppendStr(line, sizeof(line), "DHCP=Static | DNS=");
    }
    AppendIpv4(line, sizeof(line), info.dnsIp);
    PrintLine(hOut, line);

    line[0] = '\0';
    AppendStr(line, sizeof(line), "  DMA Telemetry. . . : TX=");
    AppendU64(line, sizeof(line), info.txPackets);
    AppendStr(line, sizeof(line), " RX=");
    AppendU64(line, sizeof(line), info.rxPackets);
    AppendStr(line, sizeof(line), " ARP=");
    AppendU64(line, sizeof(line), info.arpResolved);
    AppendStr(line, sizeof(line), " UDP=");
    AppendU64(line, sizeof(line), info.udpPackets);
    AppendStr(line, sizeof(line), " DNS=");
    AppendU64(line, sizeof(line), info.dnsResolved);
    AppendStr(line, sizeof(line), " TCP=");
    AppendU64(line, sizeof(line), info.tcpHandshakes);
    PrintLine(hOut, line);

    ExitProcess(0);
}
