/**
 * @file nslookup_app.cpp
 * @brief Sovereign Dynamic Ring-3 RFC 1035 DNS Resolver & WinSock2 UDP Client (C:\Windows\System32\nslookup.exe).
 *
 * ZERO direct syscall instructions! Dynamically imports ONLY standard Win32 & WinSock2
 * APIs from KERNEL32.dll + WS2_32.dll -> NTDLL.dll -> Ring 0 Intel e1000 PCIe DMA!
 *
 * Usage:
 *   nslookup.exe <hostname> [dns_server_ip] [dns_port]
 *   nslookup.exe --udp <ip> <port> <message>   (Tests WinSock2 SOCK_DGRAM sendto/recvfrom!)
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

__declspec(dllimport) int32_t  WSAStartup(uint16_t wVersionRequested, void* lpWSAData) noexcept;
__declspec(dllimport) int32_t  WSACleanup() noexcept;
__declspec(dllimport) uint64_t socket(int32_t af, int32_t type, int32_t protocol) noexcept;
__declspec(dllimport) int32_t  sendto(uint64_t s, const char* buf, int32_t len, int32_t flags, const void* to, int32_t tolen) noexcept;
__declspec(dllimport) int32_t  recvfrom(uint64_t s, char* buf, int32_t len, int32_t flags, void* from, int32_t* fromlen) noexcept;
__declspec(dllimport) int32_t  closesocket(uint64_t s) noexcept;
__declspec(dllimport) uint32_t inet_addr(const char* cp) noexcept;
__declspec(dllimport) uint16_t htons(uint16_t v) noexcept;
__declspec(dllimport) uint16_t ntohs(uint16_t v) noexcept;
__declspec(dllimport) int32_t  DnsResolveMica(const char* hostname, uint32_t dnsIpNet, uint16_t dnsPortHost, void* outRes) noexcept;

} // extern "C"

namespace {

static constexpr uint32_t STD_OUTPUT_HANDLE = 0xFFFFFFF5u;
static constexpr uint64_t INVALID_SOCKET_VAL = ~0ULL;

#pragma pack(push, 1)
struct SockAddrIn {
    uint16_t sin_family;
    uint16_t sin_port;
    uint32_t sin_addr;
    uint8_t  sin_zero[8];
};

struct NetDnsResult {
    uint32_t resolvedIpNet;
    uint32_t ttlSeconds;
    uint32_t rttUs;
    uint32_t answerCount;
    uint32_t serverIpNet;
    uint16_t serverPort;
    uint16_t reserved;
};
#pragma pack(pop)

static size_t StrLen(const char* s) noexcept { size_t n = 0; while (s[n]) ++n; return n; }

static bool StrEq(const char* a, const char* b) noexcept {
    size_t i = 0;
    while (a[i] && b[i]) { if (a[i] != b[i]) return false; ++i; }
    return a[i] == b[i];
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

static int Tokenize(const char* cmd, char argvBuf[8][64]) noexcept {
    int argc = 0; size_t i = 0;
    while (cmd[i] && argc < 8) {
        while (cmd[i] == ' ' || cmd[i] == '\t') ++i;
        if (!cmd[i]) break;
        size_t o = 0;
        while (cmd[i] && cmd[i] != ' ' && cmd[i] != '\t' && o + 1 < 64) argvBuf[argc][o++] = cmd[i++];
        argvBuf[argc][o] = '\0';
        argc++;
    }
    return argc;
}

static uint16_t ParsePort(const char* s, uint16_t fallback) noexcept {
    uint32_t p = 0;
    for (size_t k = 0; s[k] >= '0' && s[k] <= '9'; ++k) p = p * 10u + static_cast<uint32_t>(s[k] - '0');
    return (p > 0 && p <= 65535u) ? static_cast<uint16_t>(p) : fallback;
}

} // namespace

extern "C" void WinMainEntry() noexcept {
    void* hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    char argv[8][64]{};
    int argc = Tokenize(GetCommandLineA(), argv);

    uint8_t wsaBuf[512]{};
    WSAStartup(0x0202u, wsaBuf);

    // Optional live WinSock2 SOCK_DGRAM sendto/recvfrom echo test mode!
    if (argc >= 5 && StrEq(argv[1], "--udp")) {
        uint32_t dstIp = inet_addr(argv[2]);
        uint16_t dstPort = ParsePort(argv[3], 19999);
        const char* msg = argv[4];
        uint64_t sock = socket(2 /* AF_INET */, 2 /* SOCK_DGRAM */, 17 /* IPPROTO_UDP */);
        if (sock == INVALID_SOCKET_VAL) {
            PrintLine(hOut, "[ws2_32 UDP] ERROR: socket(SOCK_DGRAM) failed.");
            WSACleanup();
            ExitProcess(1);
        }
        SockAddrIn to{};
        to.sin_family = 2;
        to.sin_port = htons(dstPort);
        to.sin_addr = dstIp;
        int32_t sent = sendto(sock, msg, static_cast<int32_t>(StrLen(msg)), 0, &to, sizeof(to));
        char rxBuf[256]{};
        SockAddrIn from{};
        int32_t fromLen = sizeof(from);
        int32_t got = (sent > 0) ? recvfrom(sock, rxBuf, sizeof(rxBuf) - 1, 0, &from, &fromLen) : -1;
        closesocket(sock);
        WSACleanup();

        if (got > 0) {
            rxBuf[got] = '\0';
            char line[180]{};
            AppendStr(line, sizeof(line), "[ws2_32 SOCK_DGRAM] sendto/recvfrom ");
            AppendIpv4(line, sizeof(line), from.sin_addr);
            AppendStr(line, sizeof(line), ":");
            AppendU64(line, sizeof(line), ntohs(from.sin_port));
            AppendStr(line, sizeof(line), " -> '");
            AppendStr(line, sizeof(line), rxBuf);
            AppendStr(line, sizeof(line), "' (");
            AppendU64(line, sizeof(line), static_cast<uint64_t>(got));
            AppendStr(line, sizeof(line), " B) PASS!");
            PrintLine(hOut, line);
            ExitProcess(0);
        }
        PrintLine(hOut, "[ws2_32 SOCK_DGRAM] ERROR: UDP echo timed out.");
        ExitProcess(1);
    }

    const char* host = (argc >= 2) ? argv[1] : "api.micant.local";
    uint32_t dnsIp = (argc >= 3) ? inet_addr(argv[2]) : 0u;
    if (dnsIp == 0xFFFFFFFFu) dnsIp = 0u;
    uint16_t dnsPort = (argc >= 4) ? ParsePort(argv[3], 53u) : 53u;

    NetDnsResult res{};
    if (!DnsResolveMica(host, dnsIp, dnsPort, &res) || res.resolvedIpNet == 0) {
        char err[160]{};
        AppendStr(err, sizeof(err), "[nslookup] DNS query timed out or NXDOMAIN for '");
        AppendStr(err, sizeof(err), host);
        AppendStr(err, sizeof(err), "'");
        PrintLine(hOut, err);
        WSACleanup();
        ExitProcess(1);
    }

    char line[160]{};
    AppendStr(line, sizeof(line), "Server:  ");
    AppendIpv4(line, sizeof(line), res.serverIpNet);
    AppendStr(line, sizeof(line), ":");
    AppendU64(line, sizeof(line), res.serverPort);
    AppendStr(line, sizeof(line), "  (RFC 1035 Wire UDP over Intel e1000 DMA)");
    PrintLine(hOut, line);

    line[0] = '\0';
    AppendStr(line, sizeof(line), "Name:    ");
    AppendStr(line, sizeof(line), host);
    PrintLine(hOut, line);

    line[0] = '\0';
    AppendStr(line, sizeof(line), "Address: ");
    AppendIpv4(line, sizeof(line), res.resolvedIpNet);
    AppendStr(line, sizeof(line), "  (TTL=");
    AppendU64(line, sizeof(line), res.ttlSeconds);
    AppendStr(line, sizeof(line), "s | Answers=");
    AppendU64(line, sizeof(line), res.answerCount);
    AppendStr(line, sizeof(line), " | RTT=");
    AppendU64(line, sizeof(line), res.rttUs);
    AppendStr(line, sizeof(line), "us)");
    PrintLine(hOut, line);

    WSACleanup();
    ExitProcess(0);
}
