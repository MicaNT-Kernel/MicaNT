/**
 * @file tasklist_app.cpp
 * @brief Sovereign Dynamic Ring-3 Live Process Enumeration Utility (C:\Windows\System32\tasklist.exe).
 *
 * ZERO direct `syscall` instructions! Dynamically imports ONLY KERNEL32.dll
 * (chaining through NTDLL.dll -> Ring 0 NtQuerySystemInformation Class 5)
 * using the canonical shared ABI header `include/micant/shared/abi/proc.hpp`.
 *
 * Usage:
 *   tasklist.exe       - Live Ring-0 isolated-CR3 process table + SMP metrics
 *   tasklist.exe /V    - Verbose CR3 physical address, DLL count, CPU ticks, & syscalls
 *   tasklist.exe /?    - Usage help
 */

#include <cstdint>
#include <cstddef>
#include "micant/shared/abi/proc.hpp"

extern "C" {

void* memset(void* dst, int c, size_t n) noexcept {
    auto* d = static_cast<unsigned char*>(dst);
    while (n--) *d++ = static_cast<unsigned char>(c);
    return dst;
}

__declspec(dllimport) void*       GetStdHandle(uint32_t nStdHandle) noexcept;
__declspec(dllimport) int32_t     WriteFile(void* hFile, const void* buf, uint32_t len, uint32_t* written, void* ov) noexcept;
__declspec(dllimport) const char* GetCommandLineA() noexcept;
__declspec(dllimport) uint32_t    GetCurrentProcessId() noexcept;
__declspec(dllimport) int32_t     QuerySystemProcessesMica(void* outBuf, uint32_t bufLen) noexcept;
__declspec(dllimport) void        ExitProcess(uint32_t uExitCode) noexcept;

} // extern "C"

namespace {

static constexpr uint32_t STD_OUTPUT_HANDLE = 0xFFFFFFF5u;

static size_t StrLen(const char* s) noexcept { size_t n = 0; while (s[n]) ++n; return n; }

static void AppendStr(char* dst, size_t cap, const char* src) noexcept {
    size_t d = StrLen(dst);
    for (size_t i = 0; src[i] && d + 1 < cap; ++i) dst[d++] = src[i];
    dst[d] = '\0';
}

static void AppendPad(char* dst, size_t cap, const char* src, size_t width) noexcept {
    size_t sl = StrLen(src);
    AppendStr(dst, cap, src);
    for (size_t i = sl; i < width; ++i) AppendStr(dst, cap, " ");
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

static void AppendU64Pad(char* dst, size_t cap, uint64_t v, size_t width) noexcept {
    char num[24]{};
    AppendU64(num, sizeof(num), v);
    size_t nl = StrLen(num);
    for (size_t i = nl; i < width; ++i) AppendStr(dst, cap, " ");
    AppendStr(dst, cap, num);
}

static void AppendHex64(char* dst, size_t cap, uint64_t v) noexcept {
    const char hex[] = "0123456789ABCDEF";
    char s[19]; s[0] = '0'; s[1] = 'x';
    for (int i = 0; i < 16; ++i) s[2 + i] = hex[(v >> (60 - i * 4)) & 0x0Fu];
    s[18] = '\0';
    AppendStr(dst, cap, s);
}

static bool HasFlag(const char* cmd, const char* flag) noexcept {
    if (!cmd || !flag) return false;
    size_t fl = StrLen(flag);
    for (size_t i = 0; cmd[i]; ++i) {
        size_t j = 0;
        while (j < fl && cmd[i + j]) {
            char a = cmd[i + j], b = flag[j];
            if (a >= 'a' && a <= 'z') a -= ('a' - 'A');
            if (b >= 'a' && b <= 'z') b -= ('a' - 'A');
            if (a != b) break;
            ++j;
        }
        if (j == fl) return true;
    }
    return false;
}

static void PrintLine(void* hOut, const char* s) noexcept {
    char buf[196];
    size_t n = 0;
    while (s[n] && n + 3 < sizeof(buf)) { buf[n] = s[n]; ++n; }
    buf[n++] = '\r'; buf[n++] = '\n';
    uint32_t wr = 0;
    WriteFile(hOut, buf, static_cast<uint32_t>(n), &wr, nullptr);
}

static const char* StateName(uint32_t st) noexcept {
    switch (st) {
        case 1: return "Ready";
        case 2: return "Running";
        case 3: return "Waiting";
        case 4: return "Exited";
        default: return "System";
    }
}

static micant::abi::NtSysProcSnap g_snap;

} // namespace

extern "C" void WinMainEntry() noexcept {
    void* hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    const char* cmd = GetCommandLineA();
    if (HasFlag(cmd, "/?")) {
        PrintLine(hOut, "TASKLIST [/V] [/?] - Live Ring-0 Isolated-CR3 Process Snapshot (0 Direct Syscalls)");
        ExitProcess(0);
    }

    bool verbose = HasFlag(cmd, "/V");
    memset(&g_snap, 0, sizeof(g_snap));
    if (!QuerySystemProcessesMica(&g_snap, sizeof(g_snap))) {
        PrintLine(hOut, "[tasklist] ERROR: NtQuerySystemInformation(Class 5) failed.");
        ExitProcess(1);
    }

    char hdr[140]{};
    AppendStr(hdr, sizeof(hdr), "MicaNT Live Task List | SMP Cores=");
    AppendU64(hdr, sizeof(hdr), g_snap.onlineCores ? g_snap.onlineCores : 4);
    AppendStr(hdr, sizeof(hdr), " | Procs=");
    AppendU64(hdr, sizeof(hdr), g_snap.procCount);
    AppendStr(hdr, sizeof(hdr), " | Preempts=");
    AppendU64(hdr, sizeof(hdr), g_snap.totalPreemptSwitches);
    AppendStr(hdr, sizeof(hdr), " | SelfPID=");
    AppendU64(hdr, sizeof(hdr), GetCurrentProcessId());
    PrintLine(hOut, hdr);

    if (verbose) {
        PrintLine(hOut, "Image Name              PID   PPID State    DLLs  Syscalls  CR3 Physical");
        PrintLine(hOut, "==================== ====== ====== ======== ==== ========= ==================");
    } else {
        PrintLine(hOut, "Image Name              PID   PPID State    DLLs   Ticks  Syscalls");
        PrintLine(hOut, "==================== ====== ====== ======== ==== ======= =========");
    }

    // Kernel System process row (PID 4)
    {
        char row[140]{};
        AppendPad(row, sizeof(row), "ntoskrnl.exe", 20);
        AppendStr(row, sizeof(row), " "); AppendU64Pad(row, sizeof(row), 4, 6);
        AppendStr(row, sizeof(row), " "); AppendU64Pad(row, sizeof(row), 0, 6);
        AppendStr(row, sizeof(row), " "); AppendPad(row, sizeof(row), "Ring0", 8);
        AppendStr(row, sizeof(row), " "); AppendU64Pad(row, sizeof(row), 0, 4);
        if (verbose) {
            AppendStr(row, sizeof(row), " "); AppendU64Pad(row, sizeof(row), 0, 9);
            AppendStr(row, sizeof(row), " Ring0-Kernel-CR3");
        } else {
            AppendStr(row, sizeof(row), " "); AppendU64Pad(row, sizeof(row), g_snap.totalClockTicks, 7);
            AppendStr(row, sizeof(row), " "); AppendU64Pad(row, sizeof(row), 0, 9);
        }
        PrintLine(hOut, row);
    }

    uint32_t n = (g_snap.procCount <= 8u) ? g_snap.procCount : 8u;
    for (uint32_t i = 0; i < n; ++i) {
        const auto& r = g_snap.rows[i];
        if (r.pid == 0) continue;
        char row[140]{};
        AppendPad(row, sizeof(row), r.imageName[0] ? r.imageName : "proc.exe", 20);
        AppendStr(row, sizeof(row), " "); AppendU64Pad(row, sizeof(row), r.pid, 6);
        AppendStr(row, sizeof(row), " "); AppendU64Pad(row, sizeof(row), r.parentPid, 6);
        AppendStr(row, sizeof(row), " "); AppendPad(row, sizeof(row), StateName(r.state), 8);
        AppendStr(row, sizeof(row), " "); AppendU64Pad(row, sizeof(row), r.dllCount, 4);
        if (verbose) {
            AppendStr(row, sizeof(row), " "); AppendU64Pad(row, sizeof(row), r.syscallCount, 9);
            AppendStr(row, sizeof(row), " "); AppendHex64(row, sizeof(row), r.cr3Phys);
        } else {
            AppendStr(row, sizeof(row), " "); AppendU64Pad(row, sizeof(row), r.cpuTicks, 7);
            AppendStr(row, sizeof(row), " "); AppendU64Pad(row, sizeof(row), r.syscallCount, 9);
        }
        PrintLine(hOut, row);
    }

    ExitProcess(0);
}
