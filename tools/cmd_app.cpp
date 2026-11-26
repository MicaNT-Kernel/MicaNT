/**
 * @file cmd_app.cpp
 * @brief Sovereign Ring-3 Win32 Command Processor (C:\Windows\System32\cmd.exe).
 *
 * 100% Unmodified-ABI Win32 PE32+ Console Executable:
 *   - ZERO MicaNT kernel headers
 *   - ZERO inline assembly
 *   - ZERO direct syscall instructions
 *   - Dynamically imports ONLY standard Win32 APIs from KERNEL32.dll
 *     (chaining through NTDLL.dll -> Ring 0 KiSystemCall64 -> live NTFS 3.1 AHCI DMA)
 *
 * Interactive Built-ins & Live Process Execution:
 *   ver, help, cls, cd/chdir, dir, type, echo [> file], mkdir/md, del, exit,
 *   plus live Child Process execution via CreateProcessW + WaitForSingleObject + GetExitCodeProcess!
 */

#include <cstdint>
#include <cstddef>

extern "C" {

struct ProcessInformation {
    void*    hProcess;
    void*    hThread;
    uint32_t dwProcessId;
    uint32_t dwThreadId;
};

struct Win32FindDataW {
    uint32_t dwFileAttributes;
    uint64_t ftCreationTime;
    uint64_t ftLastAccessTime;
    uint64_t ftLastWriteTime;
    uint32_t nFileSizeHigh;
    uint32_t nFileSizeLow;
    uint32_t dwReserved0;
    uint32_t dwReserved1;
    wchar_t  cFileName[260];
    wchar_t  cAlternateFileName[14];
};

__declspec(dllimport) void*    GetStdHandle(uint32_t nStdHandle);
__declspec(dllimport) void*    CreateFileW(const wchar_t* lpFileName, uint32_t dwDesiredAccess, uint32_t dwShareMode,
                                           void* lpSecurityAttributes, uint32_t dwCreationDisposition,
                                           uint32_t dwFlagsAndAttributes, void* hTemplateFile);
__declspec(dllimport) int32_t  ReadFile(void* hFile, void* lpBuffer, uint32_t nNumberOfBytesToRead,
                                        uint32_t* lpNumberOfBytesRead, void* lpOverlapped);
__declspec(dllimport) int32_t  WriteFile(void* hFile, const void* lpBuffer, uint32_t nNumberOfBytesToWrite,
                                         uint32_t* lpNumberOfBytesWritten, void* lpOverlapped);
__declspec(dllimport) int32_t  CloseHandle(void* hObject);
__declspec(dllimport) int32_t  CreateDirectoryW(const wchar_t* lpPathName, void* lpSecurityAttributes);
__declspec(dllimport) int32_t  DeleteFileW(const wchar_t* lpFileName);
__declspec(dllimport) void*    FindFirstFileW(const wchar_t* lpFileName, Win32FindDataW* lpFindFileData);
__declspec(dllimport) int32_t  FindNextFileW(void* hFindFile, Win32FindDataW* lpFindFileData);
__declspec(dllimport) int32_t  FindClose(void* hFindFile);
__declspec(dllimport) void*    VirtualAlloc(void* lpAddress, size_t dwSize, uint32_t flAllocationType, uint32_t flProtect);
__declspec(dllimport) int32_t  VirtualFree(void* lpAddress, size_t dwSize, uint32_t dwFreeType);
__declspec(dllimport) uint32_t GetCurrentProcessId();
__declspec(dllimport) int32_t  CreateProcessW(const wchar_t* lpApplicationName, wchar_t* lpCommandLine,
                                              void* lpProcessAttributes, void* lpThreadAttributes,
                                              int32_t bInheritHandles, uint32_t dwCreationFlags,
                                              void* lpEnvironment, const wchar_t* lpCurrentDirectory,
                                              void* lpStartupInfo, ProcessInformation* lpProcessInformation);
__declspec(dllimport) uint32_t WaitForSingleObject(void* hHandle, uint32_t dwMilliseconds);
__declspec(dllimport) int32_t  GetExitCodeProcess(void* hProcess, uint32_t* lpExitCode);
__declspec(dllimport) void     ExitProcess(uint32_t uExitCode);

static constexpr uint32_t STD_INPUT_HANDLE   = 0xFFFFFFF6u;
static constexpr uint32_t STD_OUTPUT_HANDLE  = 0xFFFFFFF5u;
static constexpr uint32_t GENERIC_READ       = 0x80000000u;
static constexpr uint32_t GENERIC_WRITE      = 0x40000000u;
static constexpr uint32_t FILE_SHARE_READ    = 0x00000001u;
static constexpr uint32_t CREATE_ALWAYS      = 2u;
static constexpr uint32_t OPEN_EXISTING      = 3u;
static constexpr uint32_t FILE_ATTR_DIR      = 0x00000010u;
static constexpr uint32_t FILE_ATTR_NORMAL   = 0x00000080u;
static constexpr uint32_t MEM_COMMIT_RESERVE = 0x00003000u;
static constexpr uint32_t MEM_RELEASE        = 0x00008000u;
static constexpr uint32_t PAGE_READWRITE     = 0x00000004u;
static constexpr uint32_t INFINITE_TIMEOUT   = 0xFFFFFFFFu;

static size_t cstrlen(const char* s) noexcept {
    size_t n = 0;
    while (s[n]) ++n;
    return n;
}

static char toLowerCh(char c) noexcept {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
}

static bool matchCmd(const char* line, const char* word) noexcept {
    size_t i = 0;
    while (word[i]) {
        if (toLowerCh(line[i]) != toLowerCh(word[i])) return false;
        ++i;
    }
    return line[i] == '\0' || line[i] == ' ' || line[i] == '\t';
}

static const char* skipWordAndSpaces(const char* s) noexcept {
    while (*s && *s != ' ' && *s != '\t') ++s;
    while (*s == ' ' || *s == '\t') ++s;
    return s;
}

static void appendStr(char* dst, size_t& pos, size_t cap, const char* s) noexcept {
    while (*s && pos + 1 < cap) dst[pos++] = *s++;
    dst[pos] = '\0';
}

static void appendDec(char* dst, size_t& pos, size_t cap, uint64_t val) noexcept {
    char tmp[24];
    size_t n = 0;
    if (val == 0) tmp[n++] = '0';
    else {
        while (val > 0 && n < 22) {
            tmp[n++] = static_cast<char>('0' + (val % 10));
            val /= 10;
        }
    }
    while (n > 0 && pos + 1 < cap) dst[pos++] = tmp[--n];
    dst[pos] = '\0';
}

static void asciiToWide(const char* src, wchar_t* dst, size_t cap) noexcept {
    size_t i = 0;
    while (src[i] && i + 1 < cap) {
        dst[i] = static_cast<wchar_t>(static_cast<unsigned char>(src[i]));
        ++i;
    }
    dst[i] = L'\0';
}

static void wideToAscii(const wchar_t* src, char* dst, size_t cap) noexcept {
    size_t i = 0;
    while (src[i] && i + 1 < cap) {
        dst[i] = (src[i] < 128) ? static_cast<char>(src[i]) : '?';
        ++i;
    }
    dst[i] = '\0';
}

static void writeOut(void* hOut, const char* s) noexcept {
    uint32_t nw = 0;
    WriteFile(hOut, s, static_cast<uint32_t>(cstrlen(s)), &nw, nullptr);
}

static void writeLine(void* hOut, const char* s) noexcept {
    char buf[160];
    size_t p = 0;
    appendStr(buf, p, sizeof(buf) - 2, s);
    buf[p++] = '\r';
    buf[p++] = '\n';
    buf[p] = '\0';
    uint32_t nw = 0;
    WriteFile(hOut, buf, static_cast<uint32_t>(p), &nw, nullptr);
}

static void resolvePath(const char* cwd, const char* arg, char* out, size_t cap) noexcept {
    size_t p = 0;
    if (!arg || arg[0] == '\0') {
        appendStr(out, p, cap, cwd);
        return;
    }
    if (((arg[0] >= 'A' && arg[0] <= 'Z') || (arg[0] >= 'a' && arg[0] <= 'z')) && arg[1] == ':') {
        appendStr(out, p, cap, arg);
    } else if (arg[0] == '\\' || arg[0] == '/') {
        appendStr(out, p, cap, "C:");
        appendStr(out, p, cap, arg);
    } else {
        appendStr(out, p, cap, cwd);
        if (p > 0 && out[p - 1] != '\\') appendStr(out, p, cap, "\\");
        appendStr(out, p, cap, arg);
    }
    for (size_t i = 0; out[i]; ++i) {
        if (out[i] == '/') out[i] = '\\';
    }
}

static void cmdDir(void* hOut, const char* cwd, const char* arg) noexcept {
    char target[160];
    resolvePath(cwd, arg, target, sizeof(target));

    char pattern[168];
    size_t pp = 0;
    appendStr(pattern, pp, sizeof(pattern), target);
    if (pp > 0 && pattern[pp - 1] != '\\') appendStr(pattern, pp, sizeof(pattern), "\\");
    appendStr(pattern, pp, sizeof(pattern), "*");

    wchar_t wPat[168];
    asciiToWide(pattern, wPat, sizeof(wPat) / sizeof(wPat[0]));

    Win32FindDataW fd;
    fd.dwFileAttributes = 0;
    fd.nFileSizeHigh = 0;
    fd.nFileSizeLow = 0;
    fd.cFileName[0] = L'\0';

    char hdr[160];
    size_t hp = 0;
    appendStr(hdr, hp, sizeof(hdr), " Directory of ");
    appendStr(hdr, hp, sizeof(hdr), target);
    writeLine(hOut, hdr);

    void* hFind = FindFirstFileW(wPat, &fd);
    if (hFind == reinterpret_cast<void*>(-1) || hFind == nullptr) {
        writeLine(hOut, "  (Directory empty or not found)");
        return;
    }

    uint32_t files = 0, dirs = 0;
    uint64_t totalBytes = 0;
    do {
        char nameA[128];
        wideToAscii(fd.cFileName, nameA, sizeof(nameA));
        char row[160];
        size_t rp = 0;
        if (fd.dwFileAttributes & FILE_ATTR_DIR) {
            dirs++;
            appendStr(row, rp, sizeof(row), "  <DIR>        ");
            appendStr(row, rp, sizeof(row), nameA);
        } else {
            files++;
            uint64_t sz = (static_cast<uint64_t>(fd.nFileSizeHigh) << 32) | fd.nFileSizeLow;
            totalBytes += sz;
            char szBuf[24];
            size_t sp = 0;
            appendDec(szBuf, sp, sizeof(szBuf), sz);
            appendStr(row, rp, sizeof(row), "  ");
            for (size_t pad = sp; pad < 10; ++pad) appendStr(row, rp, sizeof(row), " ");
            appendStr(row, rp, sizeof(row), szBuf);
            appendStr(row, rp, sizeof(row), "   ");
            appendStr(row, rp, sizeof(row), nameA);
        }
        writeLine(hOut, row);
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);

    char sum[160];
    size_t sp = 0;
    appendStr(sum, sp, sizeof(sum), "     ");
    appendDec(sum, sp, sizeof(sum), files);
    appendStr(sum, sp, sizeof(sum), " File(s) ");
    appendDec(sum, sp, sizeof(sum), totalBytes);
    appendStr(sum, sp, sizeof(sum), " B | ");
    appendDec(sum, sp, sizeof(sum), dirs);
    appendStr(sum, sp, sizeof(sum), " Dir(s) [Ring-3 FindFirstFileW]");
    writeLine(hOut, sum);
}

static void cmdType(void* hOut, const char* cwd, const char* arg) noexcept {
    if (!arg || !*arg) {
        writeLine(hOut, "Usage: type <file>");
        return;
    }
    char target[160];
    resolvePath(cwd, arg, target, sizeof(target));
    wchar_t wPath[160];
    asciiToWide(target, wPath, sizeof(wPath) / sizeof(wPath[0]));

    void* hFile = CreateFileW(wPath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTR_NORMAL, nullptr);
    if (hFile == reinterpret_cast<void*>(-1) || hFile == nullptr) {
        writeLine(hOut, "The system cannot find the file specified.");
        return;
    }
    auto* buf = static_cast<char*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT_RESERVE, PAGE_READWRITE));
    if (buf) {
        uint32_t nr = 0;
        if (ReadFile(hFile, buf, 4095, &nr, nullptr) && nr > 0) {
            buf[nr] = '\0';
            uint32_t nw = 0;
            WriteFile(hOut, buf, nr, &nw, nullptr);
            if (buf[nr - 1] != '\n') writeOut(hOut, "\r\n");
        }
        VirtualFree(buf, 0, MEM_RELEASE);
    }
    CloseHandle(hFile);
}

static void cmdEcho(void* hOut, const char* cwd, const char* arg) noexcept {
    if (!arg || !*arg) {
        writeLine(hOut, "ECHO is on.");
        return;
    }
    const char* gt = nullptr;
    for (const char* p = arg; *p; ++p) {
        if (*p == '>') { gt = p; break; }
    }
    if (!gt) {
        writeLine(hOut, arg);
        return;
    }

    char payload[160];
    size_t plen = static_cast<size_t>(gt - arg);
    while (plen > 0 && (arg[plen - 1] == ' ' || arg[plen - 1] == '\t')) --plen;
    if (plen + 3 >= sizeof(payload)) plen = sizeof(payload) - 3;
    for (size_t i = 0; i < plen; ++i) payload[i] = arg[i];
    payload[plen++] = '\r';
    payload[plen++] = '\n';
    payload[plen] = '\0';

    const char* fn = gt + 1;
    while (*fn == ' ' || *fn == '\t') ++fn;
    char target[160];
    resolvePath(cwd, fn, target, sizeof(target));
    wchar_t wPath[160];
    asciiToWide(target, wPath, sizeof(wPath) / sizeof(wPath[0]));

    void* hFile = CreateFileW(wPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTR_NORMAL, nullptr);
    if (hFile == reinterpret_cast<void*>(-1) || hFile == nullptr) {
        writeLine(hOut, "ERROR: CreateFileW(CREATE_ALWAYS) failed on NTFS 3.1.");
        return;
    }
    uint32_t nw = 0;
    bool ok = WriteFile(hFile, payload, static_cast<uint32_t>(plen), &nw, nullptr) && nw == plen;
    CloseHandle(hFile);
    if (ok) {
        char msg[160];
        size_t mp = 0;
        appendStr(msg, mp, sizeof(msg), "[cmd.exe] Wrote ");
        appendDec(msg, mp, sizeof(msg), nw);
        appendStr(msg, mp, sizeof(msg), " B -> ");
        appendStr(msg, mp, sizeof(msg), target);
        appendStr(msg, mp, sizeof(msg), " (NTFS 3.1 DMA OK)");
        writeLine(hOut, msg);
    } else {
        writeLine(hOut, "ERROR: WriteFile failed.");
    }
}

static void cmdMkdir(void* hOut, const char* cwd, const char* arg) noexcept {
    if (!arg || !*arg) {
        writeLine(hOut, "Usage: mkdir <directory>");
        return;
    }
    char target[160];
    resolvePath(cwd, arg, target, sizeof(target));
    wchar_t wPath[160];
    asciiToWide(target, wPath, sizeof(wPath) / sizeof(wPath[0]));

    if (CreateDirectoryW(wPath, nullptr)) {
        char msg[160];
        size_t mp = 0;
        appendStr(msg, mp, sizeof(msg), "[cmd.exe] Created directory: ");
        appendStr(msg, mp, sizeof(msg), target);
        appendStr(msg, mp, sizeof(msg), " (NTFS 3.1 MFT+$I30 OK)");
        writeLine(hOut, msg);
    } else {
        writeLine(hOut, "ERROR: CreateDirectoryW failed (exists or invalid parent).");
    }
}

static void cmdDel(void* hOut, const char* cwd, const char* arg) noexcept {
    if (!arg || !*arg) {
        writeLine(hOut, "Usage: del <file>");
        return;
    }
    char target[160];
    resolvePath(cwd, arg, target, sizeof(target));
    wchar_t wPath[160];
    asciiToWide(target, wPath, sizeof(wPath) / sizeof(wPath[0]));

    if (DeleteFileW(wPath)) {
        char msg[160];
        size_t mp = 0;
        appendStr(msg, mp, sizeof(msg), "[cmd.exe] Deleted file: ");
        appendStr(msg, mp, sizeof(msg), target);
        appendStr(msg, mp, sizeof(msg), " (MFT+Bitmap+$I30 reclaimed)");
        writeLine(hOut, msg);
    } else {
        writeLine(hOut, "ERROR: DeleteFileW failed (file not found).");
    }
}

static void cmdCd(void* hOut, char* cwd, size_t cwdCap, const char* arg) noexcept {
    if (!arg || !*arg) {
        writeLine(hOut, cwd);
        return;
    }
    if (arg[0] == '\\' && arg[1] == '\0') {
        size_t p = 0;
        appendStr(cwd, p, cwdCap, "C:\\");
        return;
    }
    if (arg[0] == '.' && arg[1] == '.' && arg[2] == '\0') {
        size_t len = cstrlen(cwd);
        while (len > 3 && cwd[len - 1] == '\\') cwd[--len] = '\0';
        while (len > 3 && cwd[len - 1] != '\\') cwd[--len] = '\0';
        if (len > 3 && cwd[len - 1] == '\\') cwd[--len] = '\0';
        return;
    }
    char nextDir[128];
    resolvePath(cwd, arg, nextDir, sizeof(nextDir));
    size_t p = 0;
    appendStr(cwd, p, cwdCap, nextDir);
}

static void cmdSpawnExe(void* hOut, const char* fullCmdLine) noexcept {
    while (*fullCmdLine == ' ' || *fullCmdLine == '\t') ++fullCmdLine;
    char exeToken[96];
    size_t ti = 0;
    while (fullCmdLine[ti] && fullCmdLine[ti] != ' ' && fullCmdLine[ti] != '\t' && ti + 1 < sizeof(exeToken)) {
        exeToken[ti] = fullCmdLine[ti];
        ++ti;
    }
    exeToken[ti] = '\0';
    if (ti == 0) return;

    char exePath[160];
    size_t ep = 0;
    bool hasSlash = false, hasExe = false;
    size_t tlen = ti;
    for (size_t i = 0; i < tlen; ++i) {
        if (exeToken[i] == '\\' || exeToken[i] == '/') hasSlash = true;
    }
    if (tlen > 4 && exeToken[tlen - 4] == '.' &&
        toLowerCh(exeToken[tlen - 3]) == 'e' &&
        toLowerCh(exeToken[tlen - 2]) == 'x' &&
        toLowerCh(exeToken[tlen - 1]) == 'e') {
        hasExe = true;
    }
    if (!hasSlash) appendStr(exePath, ep, sizeof(exePath), "\\Windows\\System32\\");
    appendStr(exePath, ep, sizeof(exePath), exeToken);
    if (!hasExe) appendStr(exePath, ep, sizeof(exePath), ".exe");

    wchar_t wExe[160];
    asciiToWide(exePath, wExe, sizeof(wExe) / sizeof(wExe[0]));

    wchar_t wCmdLine[256];
    size_t cl = 0;
    while (fullCmdLine[cl] && cl + 1 < 256) {
        wCmdLine[cl] = static_cast<wchar_t>(static_cast<unsigned char>(fullCmdLine[cl]));
        ++cl;
    }
    wCmdLine[cl] = L'\0';

    ProcessInformation pi{nullptr, nullptr, 0, 0};
    if (!CreateProcessW(wExe, wCmdLine, nullptr, nullptr, 0, 0, nullptr, nullptr, nullptr, &pi)) {
        char err[160];
        size_t p = 0;
        appendStr(err, p, sizeof(err), "'");
        appendStr(err, p, sizeof(err), exeToken);
        appendStr(err, p, sizeof(err), "' is not recognized as an internal command or System32 PE32+ executable.");
        writeLine(hOut, err);
        return;
    }

    uint32_t waitRes = WaitForSingleObject(pi.hProcess, INFINITE_TIMEOUT);
    uint32_t exitCode = 0xBADu;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    char done[160];
    size_t dp = 0;
    appendStr(done, dp, sizeof(done), "[cmd.exe] Child '");
    appendStr(done, dp, sizeof(done), exeToken);
    appendStr(done, dp, sizeof(done), "' (PID=");
    appendDec(done, dp, sizeof(done), pi.dwProcessId);
    appendStr(done, dp, sizeof(done), ") Wait=");
    appendDec(done, dp, sizeof(done), waitRes);
    appendStr(done, dp, sizeof(done), " ExitCode=");
    appendDec(done, dp, sizeof(done), exitCode);
    writeLine(hOut, done);
}

void WinMainEntry() {
    void* hIn  = GetStdHandle(STD_INPUT_HANDLE);
    void* hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    uint32_t myPid = GetCurrentProcessId();

    char cwd[128];
    size_t cp = 0;
    appendStr(cwd, cp, sizeof(cwd), "C:\\");

    char banner[160];
    size_t bp = 0;
    appendStr(banner, bp, sizeof(banner), "MicaNT Sovereign Ring-3 cmd.exe [PID=");
    appendDec(banner, bp, sizeof(banner), myPid);
    appendStr(banner, bp, sizeof(banner), " | Isolated CR3 | Dynamic KERNEL32->NTDLL IAT]");
    writeLine(hOut, banner);
    writeLine(hOut, "Type 'ver', 'dir', 'cd', 'type', 'echo >', 'mkdir', 'del', '<app>.exe', or 'exit'.");

    char lineBuf[160];
    while (true) {
        char prompt[140];
        size_t pp = 0;
        appendStr(prompt, pp, sizeof(prompt), "[R3 cmd] ");
        appendStr(prompt, pp, sizeof(prompt), cwd);
        appendStr(prompt, pp, sizeof(prompt), "> ");
        writeOut(hOut, prompt);

        uint32_t nRead = 0;
        if (!ReadFile(hIn, lineBuf, sizeof(lineBuf) - 1, &nRead, nullptr) || nRead == 0) {
            break;
        }
        while (nRead > 0 && (lineBuf[nRead - 1] == '\r' || lineBuf[nRead - 1] == '\n' || lineBuf[nRead - 1] == ' ')) {
            --nRead;
        }
        lineBuf[nRead] = '\0';

        const char* cmd = lineBuf;
        while (*cmd == ' ' || *cmd == '\t') ++cmd;
        if (*cmd == '\0') continue;

        if (matchCmd(cmd, "exit")) {
            writeLine(hOut, "[cmd.exe] Exiting Ring-3 Command Processor cleanly -> ExitProcess(0).");
            ExitProcess(0);
        } else if (matchCmd(cmd, "ver")) {
            char v[160];
            size_t vp = 0;
            appendStr(v, vp, sizeof(v), "MicaNT [Version 10.0.26100.1] Ring-3 cmd.exe PID=");
            appendDec(v, vp, sizeof(v), myPid);
            appendStr(v, vp, sizeof(v), " (0 direct syscalls | Live NTFS 3.1)");
            writeLine(hOut, v);
        } else if (matchCmd(cmd, "help")) {
            writeLine(hOut, "Ring-3 cmd.exe Commands: ver, dir [p], cd [p], type <f>, echo <t> [>f], mkdir <d>, del <f>, <exe>, exit");
        } else if (matchCmd(cmd, "dir")) {
            cmdDir(hOut, cwd, skipWordAndSpaces(cmd));
        } else if (matchCmd(cmd, "cd") || matchCmd(cmd, "chdir")) {
            cmdCd(hOut, cwd, sizeof(cwd), skipWordAndSpaces(cmd));
        } else if (matchCmd(cmd, "type")) {
            cmdType(hOut, cwd, skipWordAndSpaces(cmd));
        } else if (matchCmd(cmd, "echo")) {
            cmdEcho(hOut, cwd, skipWordAndSpaces(cmd));
        } else if (matchCmd(cmd, "mkdir") || matchCmd(cmd, "md")) {
            cmdMkdir(hOut, cwd, skipWordAndSpaces(cmd));
        } else if (matchCmd(cmd, "del")) {
            cmdDel(hOut, cwd, skipWordAndSpaces(cmd));
        } else if (matchCmd(cmd, "run")) {
            cmdSpawnExe(hOut, skipWordAndSpaces(cmd));
        } else {
            cmdSpawnExe(hOut, cmd);
        }
    }

    ExitProcess(0);
}

} // extern "C"
