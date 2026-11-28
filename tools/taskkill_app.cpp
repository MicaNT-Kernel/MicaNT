/**
 * @file taskkill_app.cpp
 * @brief Sovereign Dynamic Ring-3 Process Termination Utility (C:\Windows\System32\taskkill.exe).
 *
 * ZERO direct `syscall` instructions! Dynamically imports ONLY KERNEL32.dll
 * (QuerySystemProcessesMica + TerminateProcess -> NTDLL.dll -> Ring 0).
 *
 * Usage:
 *   taskkill.exe /PID <pid> [/F]
 *   taskkill.exe /IM <image.exe> [/F]
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
__declspec(dllimport) int32_t     TerminateProcess(void* hProcess, uint32_t uExitCode) noexcept;
__declspec(dllimport) void        ExitProcess(uint32_t uExitCode) noexcept;

} // extern "C"

namespace {

static constexpr uint32_t STD_OUTPUT_HANDLE = 0xFFFFFFF5u;

static size_t StrLen(const char* s) noexcept { size_t n = 0; while (s[n]) ++n; return n; }

static bool StrCaseEq(const char* a, const char* b) noexcept {
    size_t i = 0;
    while (a[i] && b[i]) {
        char ca = a[i], cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca += ('a' - 'A');
        if (cb >= 'A' && cb <= 'Z') cb += ('a' - 'A');
        if (ca != cb) return false;
        ++i;
    }
    return a[i] == '\0' && b[i] == '\0';
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

static bool ParseU32(const char* s, uint32_t& out) noexcept {
    if (!s || !s[0]) return false;
    uint32_t v = 0;
    for (size_t i = 0; s[i]; ++i) {
        if (s[i] < '0' || s[i] > '9') return false;
        v = v * 10u + static_cast<uint32_t>(s[i] - '0');
    }
    out = v;
    return true;
}

static micant::abi::NtSysProcSnap g_snap;

} // namespace

extern "C" void WinMainEntry() noexcept {
    void* hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    char argv[8][64]{};
    int argc = Tokenize(GetCommandLineA(), argv);

    if (argc < 2 || StrCaseEq(argv[1], "/?")) {
        PrintLine(hOut, "TASKKILL /PID <pid> | /IM <imagename> [/F] - Live Ring-0 TerminateProcess");
        ExitProcess(0);
    }

    uint32_t targetPid = 0;
    bool hasPid = false;
    const char* targetIm = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (StrCaseEq(argv[i], "/PID") && i + 1 < argc) {
            hasPid = ParseU32(argv[++i], targetPid);
        } else if (StrCaseEq(argv[i], "/IM") && i + 1 < argc) {
            targetIm = argv[++i];
        }
    }

    if (!hasPid && !targetIm) {
        PrintLine(hOut, "[taskkill] ERROR: Specify /PID <pid> or /IM <imagename>.");
        ExitProcess(1);
    }

    if (hasPid && (targetPid == 0 || targetPid == 4)) {
        char err[128]{};
        AppendStr(err, sizeof(err), "[taskkill] ACCESS DENIED: Critical Ring-0 System PID ");
        AppendU64(err, sizeof(err), targetPid);
        AppendStr(err, sizeof(err), " is protected.");
        PrintLine(hOut, err);
        ExitProcess(1);
    }

    memset(&g_snap, 0, sizeof(g_snap));
    QuerySystemProcessesMica(&g_snap, sizeof(g_snap));

    if (!hasPid && targetIm) {
        if (StrCaseEq(targetIm, "ntoskrnl.exe") || StrCaseEq(targetIm, "system")) {
            PrintLine(hOut, "[taskkill] ACCESS DENIED: Critical Ring-0 System image is protected.");
            ExitProcess(1);
        }
        uint32_t n = (g_snap.procCount <= 8u) ? g_snap.procCount : 8u;
        for (uint32_t i = 0; i < n; ++i) {
            if (g_snap.rows[i].state != 4 && StrCaseEq(g_snap.rows[i].imageName, targetIm)) {
                targetPid = g_snap.rows[i].pid;
                hasPid = true;
                break;
            }
        }
        if (!hasPid) {
            char err[128]{};
            AppendStr(err, sizeof(err), "[taskkill] ERROR: Live process '");
            AppendStr(err, sizeof(err), targetIm);
            AppendStr(err, sizeof(err), "' not found.");
            PrintLine(hOut, err);
            ExitProcess(1);
        }
    }

    if (targetPid == GetCurrentProcessId()) {
        PrintLine(hOut, "[taskkill] ERROR: Refusing self-termination.");
        ExitProcess(1);
    }

    int32_t ok = TerminateProcess(reinterpret_cast<void*>(static_cast<uintptr_t>(targetPid)), 1u);
    char msg[140]{};
    if (ok) {
        AppendStr(msg, sizeof(msg), "[taskkill] SUCCESS: Terminated live process PID=");
        AppendU64(msg, sizeof(msg), targetPid);
        AppendStr(msg, sizeof(msg), " via kernel32!TerminateProcess.");
        PrintLine(hOut, msg);
        ExitProcess(0);
    } else {
        AppendStr(msg, sizeof(msg), "[taskkill] ERROR: Live process PID=");
        AppendU64(msg, sizeof(msg), targetPid);
        AppendStr(msg, sizeof(msg), " not active or already exited.");
        PrintLine(hOut, msg);
        ExitProcess(1);
    }
}
