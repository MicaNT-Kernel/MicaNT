/**
 * @file ping_app.cpp
 * @brief Sovereign Dynamic Ring-3 Hardware ICMP Ping Utility (C:\Windows\System32\ping.exe).
 *
 * ZERO direct syscall instructions! Dynamically imports KERNEL32.dll + WS2_32.dll
 * (chaining through NTDLL.dll -> Ring 0 Intel e1000 PCIe DMA ICMP Echo Request/Reply).
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
__declspec(dllimport) uint32_t    inet_addr(const char* cp) noexcept;
__declspec(dllimport) int32_t     IcmpSendEchoMica(uint32_t destIp, void* replyInfoBuf, uint32_t replyInfoSize) noexcept;

} // extern "C"

namespace {

static constexpr uint32_t STD_OUTPUT_HANDLE = 0xFFFFFFF5u;

#pragma pack(push, 1)
struct NetIcmpReplyInfo {
    uint32_t rttUs;
    uint32_t ttl;
    uint32_t replyIp;
    uint32_t bytes;
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

} // namespace

extern "C" void WinMainEntry() noexcept {
    void* hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    char argv[8][64]{};
    int argc = Tokenize(GetCommandLineA(), argv);

    const char* targetStr = "10.0.2.2";
    uint32_t count = 4;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-' && (argv[i][1] == 'n' || argv[i][1] == 'N') && i + 1 < argc) {
            ++i;
            uint32_t v = 0;
            for (size_t k = 0; argv[i][k] >= '0' && argv[i][k] <= '9'; ++k) v = v * 10u + static_cast<uint32_t>(argv[i][k] - '0');
            if (v >= 1 && v <= 16) count = v;
        } else if (argv[i][0] != '-') {
            targetStr = argv[i];
        }
    }

    uint32_t targetIp = inet_addr(targetStr);
    if (targetIp == 0xFFFFFFFFu) {
        PrintLine(hOut, "Ping request could not parse IPv4 address.");
        ExitProcess(1);
    }

    char line[160]{};
    AppendStr(line, sizeof(line), "Pinging ");
    AppendIpv4(line, sizeof(line), targetIp);
    AppendStr(line, sizeof(line), " with 32 bytes of data via Intel e1000 DMA:");
    PrintLine(hOut, line);

    uint32_t received = 0;
    for (uint32_t i = 0; i < count; ++i) {
        NetIcmpReplyInfo rep{};
        if (IcmpSendEchoMica(targetIp, &rep, sizeof(rep))) {
            received++;
            line[0] = '\0';
            AppendStr(line, sizeof(line), "  Reply from ");
            AppendIpv4(line, sizeof(line), rep.replyIp ? rep.replyIp : targetIp);
            AppendStr(line, sizeof(line), ": bytes=");
            AppendU64(line, sizeof(line), rep.bytes ? rep.bytes : 32);
            if (rep.rttUs < 1000u) {
                AppendStr(line, sizeof(line), " time<1ms (");
                AppendU64(line, sizeof(line), rep.rttUs);
                AppendStr(line, sizeof(line), "us) TTL=");
            } else {
                AppendStr(line, sizeof(line), " time=");
                AppendU64(line, sizeof(line), rep.rttUs / 1000u);
                AppendStr(line, sizeof(line), "ms TTL=");
            }
            AppendU64(line, sizeof(line), rep.ttl);
            PrintLine(hOut, line);
        } else {
            PrintLine(hOut, "  Request timed out.");
        }
    }

    line[0] = '\0';
    AppendStr(line, sizeof(line), "Ping statistics: Sent = ");
    AppendU64(line, sizeof(line), count);
    AppendStr(line, sizeof(line), ", Received = ");
    AppendU64(line, sizeof(line), received);
    AppendStr(line, sizeof(line), ", Lost = ");
    AppendU64(line, sizeof(line), count - received);
    AppendStr(line, sizeof(line), " (");
    AppendU64(line, sizeof(line), ((count - received) * 100u) / count);
    AppendStr(line, sizeof(line), "% loss)");
    PrintLine(hOut, line);

    ExitProcess(received > 0 ? 0u : 1u);
}
