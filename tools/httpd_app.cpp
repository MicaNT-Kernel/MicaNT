/**
 * @file httpd_app.cpp
 * @brief Sovereign Dynamic Ring-3 Bare-Metal HTTP/1.1 Web Server (C:\Windows\System32\httpd.exe).
 *
 * ZERO direct `syscall` instructions! Dynamically imports ONLY standard Win32 & WinSock2 APIs
 * from KERNEL32.dll + WS2_32.dll -> NTDLL.dll -> Ring-0 Intel e1000 PCIe Bus-Master DMA & NTFS 3.1:
 *   - WSAStartup, socket, bind, listen, accept, recv, send, closesocket, WSACleanup, htons
 *   - CreateFileW, ReadFile, CloseHandle, QuerySystemProcessesMica, QueryNetAdapterInfoMica
 *
 * Live HTTP/1.1 Endpoints served directly from MicaNT bare-metal hardware:
 *   GET /                 -> Bare-Metal HTML Server Dashboard (live IP, MAC, PID, SMP cores)
 *   GET /api/status       -> Live JSON Hardware, SMP, Process & Intel e1000 DMA Telemetry
 *   GET /ntfs/<path>      -> Real on-disk NTFS 3.1 file read via kernel32!CreateFileW + ReadFile
 *   GET /shutdown         -> Graceful RFC 793 shutdown returning JSON summary & ExitProcess(0)
 *
 * Usage:
 *   httpd.exe [port]      (default: 80)
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
__declspec(dllimport) void*       CreateFileW(const wchar_t* name, uint32_t access, uint32_t share, void* sa, uint32_t disp, uint32_t flags, void* tmpl) noexcept;
__declspec(dllimport) int32_t     ReadFile(void* hFile, void* buf, uint32_t toRead, uint32_t* got, void* ov) noexcept;
__declspec(dllimport) int32_t     CloseHandle(void* h) noexcept;
__declspec(dllimport) const char* GetCommandLineA() noexcept;
__declspec(dllimport) uint32_t    GetCurrentProcessId() noexcept;
__declspec(dllimport) int32_t     QuerySystemProcessesMica(void* outBuf, uint32_t bufLen) noexcept;
__declspec(dllimport) void        ExitProcess(uint32_t uExitCode) noexcept;

__declspec(dllimport) int32_t  WSAStartup(uint16_t wVersionRequested, void* lpWSAData) noexcept;
__declspec(dllimport) int32_t  WSACleanup() noexcept;
__declspec(dllimport) uint64_t socket(int32_t af, int32_t type, int32_t protocol) noexcept;
__declspec(dllimport) int32_t  bind(uint64_t s, const void* addr, int32_t namelen) noexcept;
__declspec(dllimport) int32_t  listen(uint64_t s, int32_t backlog) noexcept;
__declspec(dllimport) uint64_t accept(uint64_t s, void* addr, int32_t* addrlen) noexcept;
__declspec(dllimport) int32_t  send(uint64_t s, const char* buf, int32_t len, int32_t flags) noexcept;
__declspec(dllimport) int32_t  recv(uint64_t s, char* buf, int32_t len, int32_t flags) noexcept;
__declspec(dllimport) int32_t  closesocket(uint64_t s) noexcept;
__declspec(dllimport) uint16_t htons(uint16_t v) noexcept;
__declspec(dllimport) uint16_t ntohs(uint16_t v) noexcept;
__declspec(dllimport) int32_t  QueryNetAdapterInfoMica(void* outBuf, uint32_t bufLen) noexcept;

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

struct NtProcRow {
    uint32_t pid;
    uint32_t parentPid;
    uint32_t state;
    uint32_t dllCount;
    uint64_t cr3Phys;
    uint64_t cpuTicks;
    uint64_t syscallCount;
    char     imageName[32];
};

struct NtSysProcSnap {
    uint32_t onlineCores;
    uint32_t procCount;
    uint64_t totalPreemptSwitches;
    uint64_t totalClockTicks;
    uint64_t mousePackets;
    uint64_t dwmComposites;
    uint32_t activeWindows;
    uint32_t reserved;
    NtProcRow rows[8];
};

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

static size_t StrLen(const char* s) noexcept {
    size_t n = 0;
    while (s[n]) ++n;
    return n;
}

static bool StrEq(const char* a, const char* b) noexcept {
    size_t i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return false;
        ++i;
    }
    return a[i] == '\0' && b[i] == '\0';
}

static bool StartsWith(const char* s, const char* pfx) noexcept {
    for (size_t i = 0; pfx[i]; ++i) {
        if (s[i] != pfx[i]) return false;
    }
    return true;
}

static void AppendStr(char* dst, size_t cap, const char* src) noexcept {
    size_t d = StrLen(dst);
    for (size_t i = 0; src[i] && d + 1 < cap; ++i) dst[d++] = src[i];
    dst[d] = '\0';
}

static void AppendBytes(char* dst, size_t& len, size_t cap, const char* src, size_t srcLen) noexcept {
    for (size_t i = 0; i < srcLen && len < cap; ++i) dst[len++] = src[i];
}

static void AppendU64(char* dst, size_t cap, uint64_t v) noexcept {
    char tmp[24];
    size_t n = 0;
    if (v == 0) tmp[n++] = '0';
    else {
        while (v > 0 && n < 22) {
            tmp[n++] = static_cast<char>('0' + (v % 10));
            v /= 10;
        }
    }
    char rev[24];
    for (size_t i = 0; i < n; ++i) rev[i] = tmp[n - 1 - i];
    rev[n] = '\0';
    AppendStr(dst, cap, rev);
}

static void AppendHexByte(char* dst, size_t cap, uint8_t b) noexcept {
    const char hex[] = "0123456789ABCDEF";
    char s[3] = { hex[(b >> 4) & 0x0Fu], hex[b & 0x0Fu], '\0' };
    AppendStr(dst, cap, s);
}

static void AppendHex16(char* dst, size_t cap, uint16_t v) noexcept {
    AppendHexByte(dst, cap, static_cast<uint8_t>((v >> 8) & 0xFFu));
    AppendHexByte(dst, cap, static_cast<uint8_t>(v & 0xFFu));
}

static void AppendIpv4(char* dst, size_t cap, uint32_t ip) noexcept {
    AppendU64(dst, cap, ip & 0xFFu); AppendStr(dst, cap, ".");
    AppendU64(dst, cap, (ip >> 8) & 0xFFu); AppendStr(dst, cap, ".");
    AppendU64(dst, cap, (ip >> 16) & 0xFFu); AppendStr(dst, cap, ".");
    AppendU64(dst, cap, (ip >> 24) & 0xFFu);
}

static void AppendMac(char* dst, size_t cap, const uint8_t mac[6]) noexcept {
    for (int i = 0; i < 6; ++i) {
        if (i > 0) AppendStr(dst, cap, ":");
        AppendHexByte(dst, cap, mac[i]);
    }
}

static void PrintLine(void* hOut, const char* s) noexcept {
    char buf[196];
    size_t n = 0;
    while (s[n] && n + 3 < sizeof(buf)) { buf[n] = s[n]; ++n; }
    buf[n++] = '\r';
    buf[n++] = '\n';
    uint32_t wr = 0;
    WriteFile(hOut, buf, static_cast<uint32_t>(n), &wr, nullptr);
}

static int Tokenize(const char* cmd, char argvBuf[6][64]) noexcept {
    int argc = 0;
    size_t i = 0;
    while (cmd[i] && argc < 6) {
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
    for (size_t k = 0; s[k] >= '0' && s[k] <= '9'; ++k) {
        p = p * 10u + static_cast<uint32_t>(s[k] - '0');
    }
    return (p > 0 && p <= 65535u) ? static_cast<uint16_t>(p) : fallback;
}

static bool ParseHttpRequestLine(const char* req, int32_t reqLen, char* methodOut, size_t mCap, char* pathOut, size_t pCap) noexcept {
    int32_t i = 0;
    size_t mo = 0;
    while (i < reqLen && req[i] != ' ' && req[i] != '\r' && req[i] != '\n') {
        if (mo + 1 < mCap) methodOut[mo++] = req[i];
        ++i;
    }
    methodOut[mo] = '\0';
    if (i >= reqLen || req[i] != ' ') return false;
    while (i < reqLen && req[i] == ' ') ++i;
    size_t po = 0;
    while (i < reqLen && req[i] != ' ' && req[i] != '?' && req[i] != '\r' && req[i] != '\n') {
        if (po + 1 < pCap) pathOut[po++] = req[i];
        ++i;
    }
    pathOut[po] = '\0';
    return (mo > 0 && po > 0);
}

static int32_t SendHttpResponse(
    uint64_t clientSock,
    const char* statusLine,
    const char* contentType,
    const char* body,
    size_t bodyLen
) noexcept {
    static char txBuf[8192];
    txBuf[0] = '\0';
    AppendStr(txBuf, sizeof(txBuf), "HTTP/1.1 ");
    AppendStr(txBuf, sizeof(txBuf), statusLine);
    AppendStr(txBuf, sizeof(txBuf), "\r\nServer: MicaNT-HTTPD/10.0 (Bare-Metal x86_64 e1000)\r\nContent-Type: ");
    AppendStr(txBuf, sizeof(txBuf), contentType);
    AppendStr(txBuf, sizeof(txBuf), "\r\nContent-Length: ");
    AppendU64(txBuf, sizeof(txBuf), bodyLen);
    AppendStr(txBuf, sizeof(txBuf), "\r\nConnection: close\r\n\r\n");

    size_t totalLen = StrLen(txBuf);
    AppendBytes(txBuf, totalLen, sizeof(txBuf), body, bodyLen);
    return send(clientSock, txBuf, static_cast<int32_t>(totalLen), 0);
}

static bool ReadNtfsFile(const char* urlSubPath, char* outBuf, uint32_t maxBytes, uint32_t& outRead) noexcept {
    outRead = 0;
    if (!urlSubPath || !urlSubPath[0]) return false;

    // Reject path traversal
    for (size_t i = 0; urlSubPath[i]; ++i) {
        if (urlSubPath[i] == '.' && urlSubPath[i + 1] == '.') return false;
    }

    wchar_t wPath[160];
    size_t o = 0;
    if (urlSubPath[0] != '/' && urlSubPath[0] != '\\') {
        wPath[o++] = L'\\';
    }
    for (size_t i = 0; urlSubPath[i] && o + 1 < 160; ++i) {
        char c = urlSubPath[i];
        wPath[o++] = (c == '/') ? L'\\' : static_cast<wchar_t>(static_cast<unsigned char>(c));
    }
    wPath[o] = L'\0';

    void* hFile = CreateFileW(wPath, 0x80000000u /* GENERIC_READ */, 1u, nullptr, 3u /* OPEN_EXISTING */, 0x80u, nullptr);
    if (hFile == reinterpret_cast<void*>(-1) || hFile == nullptr) {
        return false;
    }
    uint32_t got = 0;
    int32_t ok = ReadFile(hFile, outBuf, maxBytes, &got, nullptr);
    CloseHandle(hFile);
    if (!ok) return false;
    outRead = got;
    return true;
}

static NtSysProcSnap  g_snap;
static NetAdapterInfo g_nic;
static char           g_reqBuf[2048];
static char           g_bodyBuf[6144];

} // namespace

extern "C" void WinMainEntry() noexcept {
    void* hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    char argv[6][64]{};
    int argc = Tokenize(GetCommandLineA(), argv);
    uint16_t port = (argc >= 2) ? ParsePort(argv[1], 80u) : 80u;

    uint8_t wsaBuf[512]{};
    if (WSAStartup(0x0202u, wsaBuf) != 0) {
        PrintLine(hOut, "[httpd] ERROR: WSAStartup failed.");
        ExitProcess(1);
    }

    QueryNetAdapterInfoMica(&g_nic, sizeof(g_nic));
    QuerySystemProcessesMica(&g_snap, sizeof(g_snap));

    uint64_t listenSock = socket(2 /* AF_INET */, 1 /* SOCK_STREAM */, 6 /* IPPROTO_TCP */);
    if (listenSock == INVALID_SOCKET_VAL) {
        PrintLine(hOut, "[httpd] ERROR: ws2_32!socket failed.");
        WSACleanup();
        ExitProcess(1);
    }

    SockAddrIn bindAddr{};
    bindAddr.sin_family = 2; // AF_INET
    bindAddr.sin_port = htons(port);
    bindAddr.sin_addr = 0;   // INADDR_ANY (0.0.0.0)

    if (bind(listenSock, &bindAddr, sizeof(bindAddr)) != 0) {
        PrintLine(hOut, "[httpd] ERROR: ws2_32!bind failed.");
        closesocket(listenSock);
        WSACleanup();
        ExitProcess(1);
    }

    if (listen(listenSock, 8) != 0) {
        PrintLine(hOut, "[httpd] ERROR: ws2_32!listen failed.");
        closesocket(listenSock);
        WSACleanup();
        ExitProcess(1);
    }

    PrintLine(hOut, "==========================================================================");
    PrintLine(hOut, "  MicaNT Bare-Metal HTTP/1.1 Server (Ring-3 httpd.exe | 0 Direct Syscalls)");
    PrintLine(hOut, "==========================================================================");

    char bLine[140]{};
    AppendStr(bLine, sizeof(bLine), "[httpd] LISTENING on ");
    AppendIpv4(bLine, sizeof(bLine), g_nic.ipv4Addr);
    AppendStr(bLine, sizeof(bLine), ":");
    AppendU64(bLine, sizeof(bLine), port);
    AppendStr(bLine, sizeof(bLine), " | MAC=");
    AppendMac(bLine, sizeof(bLine), g_nic.mac);
    AppendStr(bLine, sizeof(bLine), " | PID=");
    AppendU64(bLine, sizeof(bLine), GetCurrentProcessId());
    PrintLine(hOut, bLine);
    PrintLine(hOut, "[httpd] Routes: GET / | GET /api/status | GET /ntfs/<path> | GET /shutdown");

    uint32_t servedCount = 0;
    bool running = true;

    while (running) {
        SockAddrIn peer{};
        int32_t peerLen = sizeof(peer);
        uint64_t clientSock = accept(listenSock, &peer, &peerLen);
        if (clientSock == INVALID_SOCKET_VAL) break;

        memset(g_reqBuf, 0, sizeof(g_reqBuf));
        int32_t rlen = recv(clientSock, g_reqBuf, static_cast<int32_t>(sizeof(g_reqBuf) - 1), 0);
        if (rlen <= 0) {
            closesocket(clientSock);
            continue;
        }
        g_reqBuf[rlen] = '\0';

        char method[16]{};
        char path[128]{};
        if (!ParseHttpRequestLine(g_reqBuf, rlen, method, sizeof(method), path, sizeof(path))) {
            const char bad[] = "400 Bad Request\r\n";
            SendHttpResponse(clientSock, "400 Bad Request", "text/plain", bad, sizeof(bad) - 1);
            closesocket(clientSock);
            continue;
        }

        servedCount++;
        QueryNetAdapterInfoMica(&g_nic, sizeof(g_nic));
        QuerySystemProcessesMica(&g_snap, sizeof(g_snap));

        const char* statusStr = "200 OK";
        const char* ctype = "text/plain";
        size_t bodyLen = 0;

        if (StrEq(path, "/") || StrEq(path, "/index.html")) {
            ctype = "text/html; charset=utf-8";
            g_bodyBuf[0] = '\0';
            AppendStr(g_bodyBuf, sizeof(g_bodyBuf),
                "<!doctype html><html><head><title>MicaNT Bare-Metal HTTP/1.1 Server</title></head>"
                "<body><h1>MicaNT Sovereign Bare-Metal OS (x86_64 UEFI)</h1>"
                "<p>Served live from Ring-3 <b>C:\\Windows\\System32\\httpd.exe</b> over Intel e1000 PCIe DMA!</p>"
                "<ul><li>IPv4: ");
            AppendIpv4(g_bodyBuf, sizeof(g_bodyBuf), g_nic.ipv4Addr);
            AppendStr(g_bodyBuf, sizeof(g_bodyBuf), "</li><li>MAC: ");
            AppendMac(g_bodyBuf, sizeof(g_bodyBuf), g_nic.mac);
            AppendStr(g_bodyBuf, sizeof(g_bodyBuf), "</li><li>SMP Cores: ");
            AppendU64(g_bodyBuf, sizeof(g_bodyBuf), g_snap.onlineCores ? g_snap.onlineCores : 4);
            AppendStr(g_bodyBuf, sizeof(g_bodyBuf), "</li><li>PID: ");
            AppendU64(g_bodyBuf, sizeof(g_bodyBuf), GetCurrentProcessId());
            AppendStr(g_bodyBuf, sizeof(g_bodyBuf), "</li><li>Requests Served: ");
            AppendU64(g_bodyBuf, sizeof(g_bodyBuf), servedCount);
            AppendStr(g_bodyBuf, sizeof(g_bodyBuf),
                "</li></ul><p>Endpoints: <a href=\"/api/status\">/api/status</a> | "
                "<a href=\"/ntfs/README.txt\">/ntfs/README.txt</a> | "
                "<a href=\"/ntfs/MicaNT/system.ini\">/ntfs/MicaNT/system.ini</a></p></body></html>\r\n");
            bodyLen = StrLen(g_bodyBuf);
        } else if (StrEq(path, "/api/status")) {
            ctype = "application/json";
            g_bodyBuf[0] = '\0';
            AppendStr(g_bodyBuf, sizeof(g_bodyBuf), "{\"server\":\"MicaNT-HTTPD/10.0\",\"milestone\":16,\"arch\":\"x86_64-uefi\",\"cores\":");
            AppendU64(g_bodyBuf, sizeof(g_bodyBuf), g_snap.onlineCores ? g_snap.onlineCores : 4);
            AppendStr(g_bodyBuf, sizeof(g_bodyBuf), ",\"procs\":");
            AppendU64(g_bodyBuf, sizeof(g_bodyBuf), g_snap.procCount);
            AppendStr(g_bodyBuf, sizeof(g_bodyBuf), ",\"pid\":");
            AppendU64(g_bodyBuf, sizeof(g_bodyBuf), GetCurrentProcessId());
            AppendStr(g_bodyBuf, sizeof(g_bodyBuf), ",\"ip\":\"");
            AppendIpv4(g_bodyBuf, sizeof(g_bodyBuf), g_nic.ipv4Addr);
            AppendStr(g_bodyBuf, sizeof(g_bodyBuf), "\",\"mac\":\"");
            AppendMac(g_bodyBuf, sizeof(g_bodyBuf), g_nic.mac);
            AppendStr(g_bodyBuf, sizeof(g_bodyBuf), "\",\"nic\":\"");
            AppendHex16(g_bodyBuf, sizeof(g_bodyBuf), g_nic.vendorId);
            AppendStr(g_bodyBuf, sizeof(g_bodyBuf), ":");
            AppendHex16(g_bodyBuf, sizeof(g_bodyBuf), g_nic.deviceId);
            AppendStr(g_bodyBuf, sizeof(g_bodyBuf), "\",\"dhcp_bound\":");
            AppendStr(g_bodyBuf, sizeof(g_bodyBuf), g_nic.dhcpBound ? "true" : "false");
            AppendStr(g_bodyBuf, sizeof(g_bodyBuf), ",\"dhcp_lease_s\":");
            AppendU64(g_bodyBuf, sizeof(g_bodyBuf), g_nic.dhcpLeaseSeconds);
            AppendStr(g_bodyBuf, sizeof(g_bodyBuf), ",\"tx_pkts\":");
            AppendU64(g_bodyBuf, sizeof(g_bodyBuf), g_nic.txPackets);
            AppendStr(g_bodyBuf, sizeof(g_bodyBuf), ",\"rx_pkts\":");
            AppendU64(g_bodyBuf, sizeof(g_bodyBuf), g_nic.rxPackets);
            AppendStr(g_bodyBuf, sizeof(g_bodyBuf), ",\"tcp_handshakes\":");
            AppendU64(g_bodyBuf, sizeof(g_bodyBuf), g_nic.tcpHandshakes);
            AppendStr(g_bodyBuf, sizeof(g_bodyBuf), ",\"requests_served\":");
            AppendU64(g_bodyBuf, sizeof(g_bodyBuf), servedCount);
            AppendStr(g_bodyBuf, sizeof(g_bodyBuf), "}\r\n");
            bodyLen = StrLen(g_bodyBuf);
        } else if (StartsWith(path, "/ntfs/") && path[6] != '\0') {
            uint32_t fileBytes = 0;
            if (ReadNtfsFile(path + 5, g_bodyBuf, static_cast<uint32_t>(sizeof(g_bodyBuf) - 1), fileBytes)) {
                ctype = "text/plain; charset=utf-8";
                bodyLen = fileBytes;
            } else {
                statusStr = "404 Not Found";
                ctype = "text/plain";
                g_bodyBuf[0] = '\0';
                AppendStr(g_bodyBuf, sizeof(g_bodyBuf), "404 NTFS File Not Found: ");
                AppendStr(g_bodyBuf, sizeof(g_bodyBuf), path + 5);
                AppendStr(g_bodyBuf, sizeof(g_bodyBuf), "\r\n");
                bodyLen = StrLen(g_bodyBuf);
            }
        } else if (StrEq(path, "/shutdown")) {
            ctype = "application/json";
            g_bodyBuf[0] = '\0';
            AppendStr(g_bodyBuf, sizeof(g_bodyBuf), "{\"shutdown\":true,\"server\":\"MicaNT-HTTPD/10.0\",\"served\":");
            AppendU64(g_bodyBuf, sizeof(g_bodyBuf), servedCount);
            AppendStr(g_bodyBuf, sizeof(g_bodyBuf), "}\r\n");
            bodyLen = StrLen(g_bodyBuf);
            running = false;
        } else {
            statusStr = "404 Not Found";
            ctype = "text/plain";
            g_bodyBuf[0] = '\0';
            AppendStr(g_bodyBuf, sizeof(g_bodyBuf), "404 Not Found: ");
            AppendStr(g_bodyBuf, sizeof(g_bodyBuf), path);
            AppendStr(g_bodyBuf, sizeof(g_bodyBuf), "\r\n");
            bodyLen = StrLen(g_bodyBuf);
        }

        SendHttpResponse(clientSock, statusStr, ctype, g_bodyBuf, bodyLen);
        closesocket(clientSock);

        char logLine[140]{};
        AppendStr(logLine, sizeof(logLine), "[HTTP #");
        AppendU64(logLine, sizeof(logLine), servedCount);
        AppendStr(logLine, sizeof(logLine), "] ");
        AppendIpv4(logLine, sizeof(logLine), peer.sin_addr);
        AppendStr(logLine, sizeof(logLine), ":");
        AppendU64(logLine, sizeof(logLine), ntohs(peer.sin_port));
        AppendStr(logLine, sizeof(logLine), " ");
        AppendStr(logLine, sizeof(logLine), method);
        AppendStr(logLine, sizeof(logLine), " ");
        AppendStr(logLine, sizeof(logLine), path);
        AppendStr(logLine, sizeof(logLine), " -> ");
        AppendStr(logLine, sizeof(logLine), statusStr);
        AppendStr(logLine, sizeof(logLine), " (");
        AppendU64(logLine, sizeof(logLine), bodyLen);
        AppendStr(logLine, sizeof(logLine), " B)");
        PrintLine(hOut, logLine);
    }

    closesocket(listenSock);
    WSACleanup();

    char bye[128]{};
    AppendStr(bye, sizeof(bye), "[httpd] Graceful RFC 793 shutdown complete! Total HTTP/1.1 requests served: ");
    AppendU64(bye, sizeof(bye), servedCount);
    PrintLine(hOut, bye);
    ExitProcess(0);
}
