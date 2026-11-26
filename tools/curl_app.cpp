/**
 * @file curl_app.cpp
 * @brief Sovereign Dynamic Ring-3 WinSock2 TCP/HTTP Client (C:\Windows\System32\curl.exe).
 *
 * ZERO direct syscall instructions! Dynamically imports ONLY standard Win32 & WinSock2
 * APIs from KERNEL32.dll + WS2_32.dll (WSAStartup, socket, connect, send, recv, closesocket,
 * WSACleanup, inet_addr, htons) -> NTDLL.dll -> Ring 0 Intel e1000 PCIe DMA TCP/IP stack!
 *
 * Usage: curl.exe <ipv4> <port> <path>
 * Example: curl.exe 10.0.2.2 18080 /micant
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
__declspec(dllimport) int32_t  connect(uint64_t s, const void* name, int32_t namelen) noexcept;
__declspec(dllimport) int32_t  send(uint64_t s, const char* buf, int32_t len, int32_t flags) noexcept;
__declspec(dllimport) int32_t  recv(uint64_t s, char* buf, int32_t len, int32_t flags) noexcept;
__declspec(dllimport) int32_t  closesocket(uint64_t s) noexcept;
__declspec(dllimport) uint32_t inet_addr(const char* cp) noexcept;
__declspec(dllimport) uint16_t htons(uint16_t v) noexcept;

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
#pragma pack(pop)

static size_t StrLen(const char* s) noexcept { size_t n = 0; while (s[n]) ++n; return n; }

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

} // namespace

extern "C" void WinMainEntry() noexcept {
    void* hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    char argv[8][64]{};
    int argc = Tokenize(GetCommandLineA(), argv);

    const char* hostStr = (argc >= 2) ? argv[1] : "10.0.2.2";
    uint16_t port = 18080;
    if (argc >= 3) {
        uint32_t p = 0;
        for (size_t k = 0; argv[2][k] >= '0' && argv[2][k] <= '9'; ++k) p = p * 10u + static_cast<uint32_t>(argv[2][k] - '0');
        if (p > 0 && p <= 65535u) port = static_cast<uint16_t>(p);
    }
    const char* pathStr = (argc >= 4) ? argv[3] : "/micant";

    uint8_t wsaBuf[512]{};
    if (WSAStartup(0x0202u, wsaBuf) != 0) {
        PrintLine(hOut, "[curl] ERROR: WSAStartup failed.");
        ExitProcess(1);
    }

    uint64_t sock = socket(2 /* AF_INET */, 1 /* SOCK_STREAM */, 6 /* IPPROTO_TCP */);
    if (sock == INVALID_SOCKET_VAL) {
        PrintLine(hOut, "[curl] ERROR: ws2_32!socket failed.");
        WSACleanup();
        ExitProcess(1);
    }

    SockAddrIn sa{};
    sa.sin_family = 2;
    sa.sin_port = htons(port);
    sa.sin_addr = inet_addr(hostStr);

    char hdr[160]{};
    AppendStr(hdr, sizeof(hdr), "[ws2_32] TCP connect(");
    AppendStr(hdr, sizeof(hdr), hostStr);
    AppendStr(hdr, sizeof(hdr), ":");
    AppendU64(hdr, sizeof(hdr), port);
    AppendStr(hdr, sizeof(hdr), ") -> SYN/SYN-ACK/ACK...");
    PrintLine(hOut, hdr);

    if (connect(sock, &sa, sizeof(sa)) != 0) {
        PrintLine(hOut, "[ws2_32] ERROR: TCP connect timed out or refused.");
        closesocket(sock);
        WSACleanup();
        ExitProcess(1);
    }

    char req[256]{};
    AppendStr(req, sizeof(req), "GET ");
    AppendStr(req, sizeof(req), pathStr);
    AppendStr(req, sizeof(req), " HTTP/1.0\r\nHost: ");
    AppendStr(req, sizeof(req), hostStr);
    AppendStr(req, sizeof(req), "\r\nUser-Agent: MicaNT-WS2/10.0\r\nConnection: close\r\n\r\n");

    int32_t sent = send(sock, req, static_cast<int32_t>(StrLen(req)), 0);
    if (sent <= 0) {
        PrintLine(hOut, "[ws2_32] ERROR: TCP send failed.");
        closesocket(sock);
        WSACleanup();
        ExitProcess(1);
    }

    char resp[2048]{};
    int32_t totalRx = 0;
    while (totalRx + 1 < static_cast<int32_t>(sizeof(resp))) {
        int32_t n = recv(sock, resp + totalRx, static_cast<int32_t>(sizeof(resp)) - 1 - totalRx, 0);
        if (n <= 0) break;
        totalRx += n;
    }
    resp[totalRx] = '\0';

    closesocket(sock);
    WSACleanup();

    char stats[140]{};
    AppendStr(stats, sizeof(stats), "[ws2_32] TCP ESTABLISHED -> Sent ");
    AppendU64(stats, sizeof(stats), static_cast<uint64_t>(sent));
    AppendStr(stats, sizeof(stats), " B | Recv ");
    AppendU64(stats, sizeof(stats), static_cast<uint64_t>(totalRx));
    AppendStr(stats, sizeof(stats), " B (FIN closed cleanly):");
    PrintLine(hOut, stats);

    // Print HTTP status line + body cleanly (skipping intermediate headers to keep console tidy!)
    int32_t pos = 0;
    bool firstLine = true;
    bool inBody = false;
    while (pos < totalRx) {
        int32_t end = pos;
        while (end < totalRx && resp[end] != '\r' && resp[end] != '\n') ++end;
        int32_t lineLen = end - pos;
        if (lineLen == 0) {
            inBody = true;
        } else if (firstLine || inBody) {
            char out[128]{};
            size_t o = 0;
            out[o++] = ' '; out[o++] = ' ';
            for (int32_t k = 0; k < lineLen && o + 1 < sizeof(out); ++k) {
                char c = resp[pos + k];
                out[o++] = (c >= 0x20 && c <= 0x7E) ? c : ' ';
            }
            out[o] = '\0';
            PrintLine(hOut, out);
            firstLine = false;
        }
        pos = end;
        if (pos < totalRx && resp[pos] == '\r') ++pos;
        if (pos < totalRx && resp[pos] == '\n') ++pos;
    }

    ExitProcess(totalRx > 0 ? 0u : 1u);
}
