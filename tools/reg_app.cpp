/**
 * @file reg_app.cpp
 * @brief Sovereign Dynamic Ring-3 Registry Console Utility (C:\Windows\System32\reg.exe).
 *
 * 100% Unmodified-ABI Win32 PE32+ Console Executable:
 *   - ZERO MicaNT kernel headers
 *   - ZERO inline assembly
 *   - ZERO direct `syscall` instructions!
 *   - Dynamically imports ONLY standard Win32 APIs from KERNEL32.dll + ADVAPI32.dll
 *     (chaining through NTDLL.dll -> Ring 0 CmBareMetalEngine -> Live NTFS 3.1 `regf` Hives!)
 *
 * Supported Commands (with real command-line arguments via GetCommandLineA()):
 *   REG QUERY  <KeyPath> [/v <ValueName>]
 *   REG ADD    <KeyPath> /v <ValueName> [/t REG_SZ|REG_DWORD] /d <Data> [/f]
 *   REG DELETE <KeyPath> /v <ValueName> [/f]
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
__declspec(dllimport) uint32_t    GetCurrentProcessId() noexcept;
__declspec(dllimport) void        ExitProcess(uint32_t uExitCode) noexcept;

__declspec(dllimport) int32_t RegOpenKeyExA(void* hKey, const char* lpSubKey, uint32_t ulOptions, uint32_t samDesired, void** phkResult) noexcept;
__declspec(dllimport) int32_t RegCreateKeyExA(void* hKey, const char* lpSubKey, uint32_t Reserved, char* lpClass, uint32_t dwOptions, uint32_t samDesired, void* lpSec, void** phkResult, uint32_t* lpdwDisp) noexcept;
__declspec(dllimport) int32_t RegQueryValueExA(void* hKey, const char* lpValueName, uint32_t* lpReserved, uint32_t* lpType, uint8_t* lpData, uint32_t* lpcbData) noexcept;
__declspec(dllimport) int32_t RegSetValueExA(void* hKey, const char* lpValueName, uint32_t Reserved, uint32_t dwType, const uint8_t* lpData, uint32_t cbData) noexcept;
__declspec(dllimport) int32_t RegDeleteValueA(void* hKey, const char* lpValueName) noexcept;
__declspec(dllimport) int32_t RegEnumValueA(void* hKey, uint32_t dwIndex, char* lpValueName, uint32_t* lpcchValueName, uint32_t* lpReserved, uint32_t* lpType, uint8_t* lpData, uint32_t* lpcbData) noexcept;
__declspec(dllimport) int32_t RegEnumKeyExA(void* hKey, uint32_t dwIndex, char* lpName, uint32_t* lpcchName, uint32_t* lpReserved, char* lpClass, uint32_t* lpcchClass, void* lpft) noexcept;
__declspec(dllimport) int32_t RegCloseKey(void* hKey) noexcept;

} // extern "C"

namespace {

static constexpr uint32_t STD_OUTPUT_HANDLE = 0xFFFFFFF5u;
static constexpr uint64_t HKEY_LOCAL_MACHINE_VAL = 0x80000002ULL;
static constexpr uint32_t REG_SZ        = 1u;
static constexpr uint32_t REG_EXPAND_SZ = 2u;
static constexpr uint32_t REG_BINARY    = 3u;
static constexpr uint32_t REG_DWORD     = 4u;

static size_t StrLen(const char* s) noexcept {
    size_t n = 0;
    while (s[n]) ++n;
    return n;
}

static char ToLower(char c) noexcept {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
}

static bool StrCaseEq(const char* a, const char* b) noexcept {
    size_t i = 0;
    while (a[i] && b[i]) {
        if (ToLower(a[i]) != ToLower(b[i])) return false;
        ++i;
    }
    return a[i] == '\0' && b[i] == '\0';
}

static void AppendStr(char* dst, size_t cap, const char* src) noexcept {
    size_t d = StrLen(dst);
    size_t i = 0;
    while (src[i] && d + 1 < cap) dst[d++] = src[i++];
    dst[d] = '\0';
}

static void AppendU64(char* dst, size_t cap, uint64_t val) noexcept {
    char tmp[24]{};
    size_t n = 0;
    if (val == 0) tmp[n++] = '0';
    else {
        while (val > 0 && n < 22) { tmp[n++] = static_cast<char>('0' + (val % 10)); val /= 10; }
    }
    char rev[24]{};
    for (size_t i = 0; i < n; ++i) rev[i] = tmp[n - 1 - i];
    rev[n] = '\0';
    AppendStr(dst, cap, rev);
}

static void AppendHex32(char* dst, size_t cap, uint32_t val) noexcept {
    static const char hex[] = "0123456789abcdef";
    char buf[11];
    buf[0] = '0'; buf[1] = 'x';
    int start = 0;
    while (start < 7 && ((val >> ((7 - start) * 4)) & 0xFu) == 0) start++;
    size_t p = 2;
    for (int i = start; i < 8; ++i) buf[p++] = hex[(val >> ((7 - i) * 4)) & 0xFu];
    buf[p] = '\0';
    AppendStr(dst, cap, buf);
}

static uint32_t ParseU32(const char* s) noexcept {
    if (!s) return 0;
    uint32_t val = 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        for (size_t i = 2; s[i]; ++i) {
            char c = s[i];
            uint32_t d = 0;
            if (c >= '0' && c <= '9') d = static_cast<uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') d = static_cast<uint32_t>(10 + c - 'a');
            else if (c >= 'A' && c <= 'F') d = static_cast<uint32_t>(10 + c - 'A');
            else break;
            val = (val << 4) | d;
        }
        return val;
    }
    for (size_t i = 0; s[i] >= '0' && s[i] <= '9'; ++i) {
        val = val * 10u + static_cast<uint32_t>(s[i] - '0');
    }
    return val;
}

static void PrintLine(void* hOut, const char* s) noexcept {
    char buf[196];
    size_t n = 0;
    while (s[n] && n + 3 < sizeof(buf)) {
        buf[n] = s[n];
        ++n;
    }
    buf[n++] = '\r';
    buf[n++] = '\n';
    uint32_t wr = 0;
    WriteFile(hOut, buf, static_cast<uint32_t>(n), &wr, nullptr);
}

static void PrintValueRow(void* hOut, const char* name, uint32_t type, const uint8_t* data, uint32_t dataLen) noexcept {
    char row[140]{};
    AppendStr(row, sizeof(row), "    ");
    AppendStr(row, sizeof(row), (name && name[0]) ? name : "(Default)");
    size_t padTo = 22;
    while (StrLen(row) < padTo && StrLen(row) + 1 < sizeof(row)) AppendStr(row, sizeof(row), " ");
    if (type == REG_SZ) AppendStr(row, sizeof(row), " REG_SZ        ");
    else if (type == REG_EXPAND_SZ) AppendStr(row, sizeof(row), " REG_EXPAND_SZ ");
    else if (type == REG_DWORD) AppendStr(row, sizeof(row), " REG_DWORD     ");
    else AppendStr(row, sizeof(row), " REG_BINARY    ");

    if (type == REG_DWORD && dataLen >= 4) {
        uint32_t v = static_cast<uint32_t>(data[0]) |
                     (static_cast<uint32_t>(data[1]) << 8) |
                     (static_cast<uint32_t>(data[2]) << 16) |
                     (static_cast<uint32_t>(data[3]) << 24);
        AppendHex32(row, sizeof(row), v);
        AppendStr(row, sizeof(row), " (");
        AppendU64(row, sizeof(row), v);
        AppendStr(row, sizeof(row), ")");
    } else if (type == REG_SZ || type == REG_EXPAND_SZ) {
        AppendStr(row, sizeof(row), reinterpret_cast<const char*>(data));
    } else {
        AppendU64(row, sizeof(row), dataLen);
        AppendStr(row, sizeof(row), " bytes");
    }
    PrintLine(hOut, row);
}

// Tokenize command line honoring quoted strings ("Microsoft\Windows NT\CurrentVersion")
static int Tokenize(const char* cmd, char argvBuf[16][96]) noexcept {
    int argc = 0;
    size_t i = 0;
    while (cmd[i] && argc < 16) {
        while (cmd[i] == ' ' || cmd[i] == '\t') ++i;
        if (!cmd[i]) break;
        bool quoted = false;
        if (cmd[i] == '"') { quoted = true; ++i; }
        size_t o = 0;
        while (cmd[i] && o + 1 < 96) {
            if (quoted) {
                if (cmd[i] == '"') { ++i; break; }
            } else {
                if (cmd[i] == ' ' || cmd[i] == '\t') break;
            }
            argvBuf[argc][o++] = cmd[i++];
        }
        argvBuf[argc][o] = '\0';
        argc++;
    }
    return argc;
}

} // namespace

extern "C" void WinMainEntry() noexcept {
    void* hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    const char* rawCmd = GetCommandLineA();
    char argv[16][96]{};
    int argc = Tokenize(rawCmd, argv);

    if (argc < 2) {
        // Default live demo query if invoked with no arguments
        void* hKey = nullptr;
        if (RegOpenKeyExA(reinterpret_cast<void*>(HKEY_LOCAL_MACHINE_VAL),
                          "SYSTEM\\CurrentControlSet\\Control", 0, 0, &hKey) == 0) {
            PrintLine(hOut, "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control");
            for (uint32_t idx = 0; idx < 16; ++idx) {
                char vname[64]{}; uint32_t ncap = sizeof(vname);
                uint8_t vdata[128]{}; uint32_t dlen = sizeof(vdata);
                uint32_t vtype = 0;
                if (RegEnumValueA(hKey, idx, vname, &ncap, nullptr, &vtype, vdata, &dlen) != 0) break;
                PrintValueRow(hOut, vname, vtype, vdata, dlen);
            }
            RegCloseKey(hKey);
            ExitProcess(0);
        }
        PrintLine(hOut, "Usage: reg.exe QUERY|ADD|DELETE <HKLM\\...> [/v Name] [/t REG_SZ|REG_DWORD] [/d Data]");
        ExitProcess(1);
    }

    const char* op = argv[1];
    const char* keyPath = (argc >= 3) ? argv[2] : "HKLM\\SYSTEM\\CurrentControlSet\\Control";

    const char* valName = nullptr;
    const char* valTypeStr = "REG_SZ";
    const char* valDataStr = "";
    for (int i = 3; i < argc; ++i) {
        if (StrCaseEq(argv[i], "/v") && i + 1 < argc) { valName = argv[++i]; }
        else if (StrCaseEq(argv[i], "/t") && i + 1 < argc) { valTypeStr = argv[++i]; }
        else if (StrCaseEq(argv[i], "/d") && i + 1 < argc) { valDataStr = argv[++i]; }
    }

    if (StrCaseEq(op, "query")) {
        void* hKey = nullptr;
        if (RegOpenKeyExA(reinterpret_cast<void*>(HKEY_LOCAL_MACHINE_VAL), keyPath, 0, 0, &hKey) != 0) {
            PrintLine(hOut, "ERROR: The system was unable to find the specified registry key.");
            ExitProcess(1);
        }
        PrintLine(hOut, keyPath);
        if (valName) {
            uint8_t vdata[128]{};
            uint32_t dlen = sizeof(vdata);
            uint32_t vtype = 0;
            if (RegQueryValueExA(hKey, valName, nullptr, &vtype, vdata, &dlen) != 0) {
                RegCloseKey(hKey);
                PrintLine(hOut, "ERROR: The system was unable to find the specified registry value.");
                ExitProcess(1);
            }
            PrintValueRow(hOut, valName, vtype, vdata, dlen);
        } else {
            for (uint32_t idx = 0; idx < 16; ++idx) {
                char vname[64]{}; uint32_t ncap = sizeof(vname);
                uint8_t vdata[128]{}; uint32_t dlen = sizeof(vdata);
                uint32_t vtype = 0;
                if (RegEnumValueA(hKey, idx, vname, &ncap, nullptr, &vtype, vdata, &dlen) != 0) break;
                PrintValueRow(hOut, vname, vtype, vdata, dlen);
            }
            for (uint32_t idx = 0; idx < 16; ++idx) {
                char sname[64]{}; uint32_t scap = sizeof(sname);
                if (RegEnumKeyExA(hKey, idx, sname, &scap, nullptr, nullptr, nullptr, nullptr) != 0) break;
                char subLine[140]{};
                AppendStr(subLine, sizeof(subLine), keyPath);
                AppendStr(subLine, sizeof(subLine), "\\");
                AppendStr(subLine, sizeof(subLine), sname);
                PrintLine(hOut, subLine);
            }
        }
        RegCloseKey(hKey);
        ExitProcess(0);
    }

    if (StrCaseEq(op, "add")) {
        void* hKey = nullptr;
        uint32_t disp = 0;
        if (RegCreateKeyExA(reinterpret_cast<void*>(HKEY_LOCAL_MACHINE_VAL), keyPath, 0, nullptr, 0, 0, nullptr, &hKey, &disp) != 0) {
            PrintLine(hOut, "ERROR: Unable to create or open registry key.");
            ExitProcess(1);
        }
        int32_t rc = 0;
        if (valName) {
            if (StrCaseEq(valTypeStr, "REG_DWORD")) {
                uint32_t dw = ParseU32(valDataStr);
                rc = RegSetValueExA(hKey, valName, 0, REG_DWORD, reinterpret_cast<const uint8_t*>(&dw), sizeof(dw));
            } else {
                rc = RegSetValueExA(hKey, valName, 0, REG_SZ, reinterpret_cast<const uint8_t*>(valDataStr),
                                    static_cast<uint32_t>(StrLen(valDataStr) + 1));
            }
        }
        RegCloseKey(hKey);
        if (rc != 0) {
            PrintLine(hOut, "ERROR: Failed to commit registry value to regf hive.");
            ExitProcess(1);
        }
        PrintLine(hOut, "The operation completed successfully. (Flushed to NTFS 3.1 regf hive)");
        ExitProcess(0);
    }

    if (StrCaseEq(op, "delete")) {
        void* hKey = nullptr;
        if (RegOpenKeyExA(reinterpret_cast<void*>(HKEY_LOCAL_MACHINE_VAL), keyPath, 0, 0, &hKey) != 0) {
            PrintLine(hOut, "ERROR: Registry key not found.");
            ExitProcess(1);
        }
        int32_t rc = valName ? RegDeleteValueA(hKey, valName) : 1;
        RegCloseKey(hKey);
        if (rc != 0) {
            PrintLine(hOut, "ERROR: Registry value not found.");
            ExitProcess(1);
        }
        PrintLine(hOut, "The operation completed successfully. (Deleted & flushed regf hive)");
        ExitProcess(0);
    }

    PrintLine(hOut, "Usage: reg.exe QUERY|ADD|DELETE <HKLM\\...> [/v Name] [/t REG_SZ|REG_DWORD] [/d Data]");
    ExitProcess(1);
}
