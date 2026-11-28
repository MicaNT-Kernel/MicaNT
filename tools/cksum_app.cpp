/**
 * @file cksum_app.cpp
 * @brief Sovereign Dynamic Ring-3 Multi-MB NTFS CRC32 Verifier (C:\Windows\System32\cksum.exe).
 *
 * ZERO direct `syscall` instructions! Dynamically imports ONLY KERNEL32.dll
 * (CreateFileW, ReadFile, CloseHandle, GetCommandLineA, WriteFile, ExitProcess).
 * Streams arbitrarily large files (>> 64 KiB, e.g. 1.27 MB ntoskrnl.exe) in 32 KiB
 * chunks over bare-metal NVMe/AHCI Bus-Master DMA and computes exact IEEE 802.3 CRC32.
 *
 * Usage:
 *   cksum.exe <ntfs_path>
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
__declspec(dllimport) void        ExitProcess(uint32_t uExitCode) noexcept;

} // extern "C"

namespace {

static constexpr uint32_t STD_OUTPUT_HANDLE = 0xFFFFFFF5u;
alignas(4096) static uint8_t g_chunkBuf[32768];

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

static void AppendHex32(char* dst, size_t cap, uint32_t v) noexcept {
    const char hex[] = "0123456789ABCDEF";
    char s[11]; s[0] = '0'; s[1] = 'x';
    for (int i = 0; i < 8; ++i) s[2 + i] = hex[(v >> (28 - i * 4)) & 0x0Fu];
    s[10] = '\0';
    AppendStr(dst, cap, s);
}

static void PrintLine(void* hOut, const char* s) noexcept {
    char buf[196];
    size_t n = 0;
    while (s[n] && n + 3 < sizeof(buf)) { buf[n] = s[n]; ++n; }
    buf[n++] = '\r'; buf[n++] = '\n';
    uint32_t wr = 0;
    WriteFile(hOut, buf, static_cast<uint32_t>(n), &wr, nullptr);
}

static int Tokenize(const char* cmd, char argvBuf[4][128]) noexcept {
    int argc = 0; size_t i = 0;
    while (cmd[i] && argc < 4) {
        while (cmd[i] == ' ' || cmd[i] == '\t') ++i;
        if (!cmd[i]) break;
        size_t o = 0;
        while (cmd[i] && cmd[i] != ' ' && cmd[i] != '\t' && o + 1 < 128) argvBuf[argc][o++] = cmd[i++];
        argvBuf[argc][o] = '\0';
        argc++;
    }
    return argc;
}

static uint32_t UpdateCrc32(uint32_t crc, const uint8_t* data, size_t len) noexcept {
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc & 1u) ? ((crc >> 1) ^ 0xEDB88320u) : (crc >> 1);
        }
    }
    return crc;
}

} // namespace

extern "C" void WinMainEntry() noexcept {
    void* hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    char argv[4][128]{};
    int argc = Tokenize(GetCommandLineA(), argv);
    const char* pathA = (argc >= 2) ? argv[1] : "\\Windows\\System32\\ntoskrnl.exe";

    wchar_t pathW[128]{};
    size_t pi = 0;
    while (pathA[pi] && pi + 1 < 128) {
        pathW[pi] = static_cast<wchar_t>(static_cast<unsigned char>(pathA[pi]));
        ++pi;
    }
    pathW[pi] = L'\0';

    void* hFile = CreateFileW(pathW, 0x80000000u, 1u, nullptr, 3u, 0x80u, nullptr);
    if (hFile == reinterpret_cast<void*>(-1) || hFile == nullptr) {
        char err[160]{};
        AppendStr(err, sizeof(err), "[cksum] ERROR: CreateFileW failed for ");
        AppendStr(err, sizeof(err), pathA);
        PrintLine(hOut, err);
        ExitProcess(1);
    }

    uint32_t crc = 0xFFFFFFFFu;
    uint64_t totalBytes = 0;
    uint32_t chunks = 0;
    bool isMzPe = false;

    while (true) {
        uint32_t got = 0;
        if (!ReadFile(hFile, g_chunkBuf, sizeof(g_chunkBuf), &got, nullptr)) {
            CloseHandle(hFile);
            PrintLine(hOut, "[cksum] ERROR: ReadFile DMA error.");
            ExitProcess(1);
        }
        if (got == 0) break;
        if (chunks == 0 && got >= 2 && g_chunkBuf[0] == 'M' && g_chunkBuf[1] == 'Z') {
            isMzPe = true;
        }
        crc = UpdateCrc32(crc, g_chunkBuf, got);
        totalBytes += got;
        chunks++;
    }
    CloseHandle(hFile);
    crc ^= 0xFFFFFFFFu;

    char line[160]{};
    AppendStr(line, sizeof(line), "[cksum] CRC32=");
    AppendHex32(line, sizeof(line), crc);
    AppendStr(line, sizeof(line), " | Bytes=");
    AppendU64(line, sizeof(line), totalBytes);
    AppendStr(line, sizeof(line), " (");
    AppendU64(line, sizeof(line), chunks);
    AppendStr(line, sizeof(line), "x32KB)");
    if (isMzPe) AppendStr(line, sizeof(line), " [PE32+ MZ OK]");
    AppendStr(line, sizeof(line), " | ");
    AppendStr(line, sizeof(line), pathA);
    PrintLine(hOut, line);
    ExitProcess(totalBytes > 0 ? 0u : 1u);
}
