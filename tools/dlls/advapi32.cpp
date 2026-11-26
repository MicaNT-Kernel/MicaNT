/**
 * @file advapi32.cpp
 * @brief Sovereign Advanced Windows 32 Security & Registry DLL (advapi32.dll) for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. ZERO direct syscall instructions!
 * Dynamically imports native NtOpenKey / NtCreateKey / NtQueryValueKey / NtSetValueKey /
 * NtDeleteValueKey / NtEnumerateValueKey / NtEnumerateKey / NtFlushKey / NtClose
 * from ntdll.dll via genuine PE32+ IAT chaining:
 *   reg.exe -> advapi32.dll (IAT) -> ntdll.dll (IAT) -> Ring 0 CmBareMetalEngine -> NTFS 3.1 regf Hives!
 */

#include <cstdint>
#include <cstddef>

extern "C" {

// ============================================================================
// 0. Dynamic Imports from C:\Windows\System32\ntdll.dll (Real PE32+ IAT)
// ============================================================================

__declspec(dllimport) uint64_t NtOpenKey(void** keyHandle, void* rootHandle, const char* subKeyAscii) noexcept;
__declspec(dllimport) uint64_t NtCreateKey(void** keyHandle, void* rootHandle, const char* subKeyAscii, uint32_t* outDisposition) noexcept;
__declspec(dllimport) uint64_t NtQueryValueKey(void* keyHandle, const char* valueNameAscii, uint32_t* outType, void* dataBuf, uint32_t* inOutDataLen) noexcept;
__declspec(dllimport) uint64_t NtSetValueKey(void* keyHandle, const char* valueNameAscii, uint32_t type, const void* dataBuf, uint32_t dataLen) noexcept;
__declspec(dllimport) uint64_t NtDeleteValueKey(void* keyHandle, const char* valueNameAscii) noexcept;
__declspec(dllimport) uint64_t NtEnumerateValueKey(void* keyHandle, uint32_t index, char* outNameBuf, uint32_t nameCap, uint32_t* outType, void* outDataBuf, uint32_t* inOutDataLen) noexcept;
__declspec(dllimport) uint64_t NtEnumerateKey(void* keyHandle, uint32_t index, char* outNameBuf, uint32_t nameCap) noexcept;
__declspec(dllimport) uint64_t NtFlushKey(void* keyHandle) noexcept;
__declspec(dllimport) uint64_t NtClose(void* handle) noexcept;

static constexpr uint32_t WIN_ERROR_SUCCESS         = 0u;
static constexpr uint32_t WIN_ERROR_FILE_NOT_FOUND  = 2u;
static constexpr uint32_t WIN_ERROR_INVALID_PARAM   = 87u;
static constexpr uint32_t WIN_ERROR_NO_MORE_ITEMS   = 259u;

static constexpr uint32_t WIN_REG_SZ        = 1u;
static constexpr uint32_t WIN_REG_EXPAND_SZ = 2u;

static inline bool NtOk(uint64_t st) noexcept {
    return static_cast<int32_t>(static_cast<uint32_t>(st)) >= 0;
}

static void wideToAscii(const wchar_t* src, char* dst, size_t cap) noexcept {
    size_t i = 0;
    if (src) {
        while (src[i] && i + 1 < cap) {
            dst[i] = (src[i] < 128) ? static_cast<char>(src[i]) : '?';
            ++i;
        }
    }
    dst[i] = '\0';
}

bool DllMain(void*, unsigned long, void*) noexcept {
    return true;
}

static uint64_t s_scmHandleCounter = 0x5000;

// ============================================================================
// 1. Live Binary Registry Hive APIs (Routed via ntdll.dll -> Ring 0 Cm)
// ============================================================================

__declspec(dllexport) int32_t RegOpenKeyExA(
    void* hKey,
    const char* lpSubKey,
    uint32_t /*ulOptions*/,
    uint32_t /*samDesired*/,
    void** phkResult
) noexcept {
    if (!phkResult) return static_cast<int32_t>(WIN_ERROR_INVALID_PARAM);
    uint64_t st = NtOpenKey(phkResult, hKey, lpSubKey ? lpSubKey : "");
    return NtOk(st) ? static_cast<int32_t>(WIN_ERROR_SUCCESS) : static_cast<int32_t>(WIN_ERROR_FILE_NOT_FOUND);
}

__declspec(dllexport) int32_t RegOpenKeyExW(
    void* hKey,
    const wchar_t* lpSubKey,
    uint32_t ulOptions,
    uint32_t samDesired,
    void** phkResult
) noexcept {
    char subA[192];
    wideToAscii(lpSubKey, subA, sizeof(subA));
    return RegOpenKeyExA(hKey, subA, ulOptions, samDesired, phkResult);
}

__declspec(dllexport) int32_t RegCreateKeyExA(
    void* hKey,
    const char* lpSubKey,
    uint32_t /*Reserved*/,
    char* /*lpClass*/,
    uint32_t /*dwOptions*/,
    uint32_t /*samDesired*/,
    void* /*lpSecurityAttributes*/,
    void** phkResult,
    uint32_t* lpdwDisposition
) noexcept {
    if (!phkResult) return static_cast<int32_t>(WIN_ERROR_INVALID_PARAM);
    uint32_t disp = 0;
    uint64_t st = NtCreateKey(phkResult, hKey, lpSubKey ? lpSubKey : "", &disp);
    if (!NtOk(st)) return static_cast<int32_t>(WIN_ERROR_FILE_NOT_FOUND);
    if (lpdwDisposition) *lpdwDisposition = disp;
    return static_cast<int32_t>(WIN_ERROR_SUCCESS);
}

__declspec(dllexport) int32_t RegCreateKeyExW(
    void* hKey,
    const wchar_t* lpSubKey,
    uint32_t Reserved,
    wchar_t* /*lpClass*/,
    uint32_t dwOptions,
    uint32_t samDesired,
    void* lpSecurityAttributes,
    void** phkResult,
    uint32_t* lpdwDisposition
) noexcept {
    char subA[192];
    wideToAscii(lpSubKey, subA, sizeof(subA));
    return RegCreateKeyExA(hKey, subA, Reserved, nullptr, dwOptions, samDesired, lpSecurityAttributes, phkResult, lpdwDisposition);
}

__declspec(dllexport) int32_t RegQueryValueExW(
    void* hKey,
    const wchar_t* lpValueName,
    uint32_t* /*lpReserved*/,
    uint32_t* lpType,
    uint8_t* lpData,
    uint32_t* lpcbData
) noexcept {
    if (!lpcbData) return static_cast<int32_t>(WIN_ERROR_INVALID_PARAM);
    char nameA[64];
    wideToAscii(lpValueName, nameA, sizeof(nameA));
    uint32_t type = 0;
    uint32_t len = *lpcbData;
    uint64_t st = NtQueryValueKey(hKey, nameA, &type, lpData, &len);
    if (!NtOk(st)) return static_cast<int32_t>(WIN_ERROR_FILE_NOT_FOUND);
    if (lpType) *lpType = type;
    *lpcbData = len;
    return static_cast<int32_t>(WIN_ERROR_SUCCESS);
}

__declspec(dllexport) int32_t RegQueryValueExA(
    void* hKey,
    const char* lpValueName,
    uint32_t* /*lpReserved*/,
    uint32_t* lpType,
    uint8_t* lpData,
    uint32_t* lpcbData
) noexcept {
    if (!lpcbData) return static_cast<int32_t>(WIN_ERROR_INVALID_PARAM);
    uint8_t rawBuf[160]{};
    uint32_t rawLen = sizeof(rawBuf);
    uint32_t type = 0;
    uint64_t st = NtQueryValueKey(hKey, lpValueName ? lpValueName : "", &type, rawBuf, &rawLen);
    if (!NtOk(st)) return static_cast<int32_t>(WIN_ERROR_FILE_NOT_FOUND);
    if (lpType) *lpType = type;

    if (type == WIN_REG_SZ || type == WIN_REG_EXPAND_SZ) {
        uint32_t wchars = rawLen / 2;
        uint32_t needed = (wchars > 0) ? wchars : 1;
        if (lpData) {
            uint32_t cap = *lpcbData;
            uint32_t out = 0;
            for (uint32_t i = 0; i < wchars && out < cap; ++i) {
                uint16_t ch = static_cast<uint16_t>(rawBuf[i * 2]) |
                              (static_cast<uint16_t>(rawBuf[i * 2 + 1]) << 8);
                lpData[out++] = (ch < 128) ? static_cast<uint8_t>(ch) : '?';
            }
            if (cap > 0 && (out == 0 || lpData[out - 1] != 0)) {
                lpData[(out < cap) ? out : (cap - 1)] = 0;
            }
        }
        *lpcbData = needed;
        return static_cast<int32_t>(WIN_ERROR_SUCCESS);
    }

    if (lpData) {
        uint32_t cp = (*lpcbData < rawLen) ? *lpcbData : rawLen;
        for (uint32_t i = 0; i < cp; ++i) lpData[i] = rawBuf[i];
    }
    *lpcbData = rawLen;
    return static_cast<int32_t>(WIN_ERROR_SUCCESS);
}

__declspec(dllexport) int32_t RegSetValueExW(
    void* hKey,
    const wchar_t* lpValueName,
    uint32_t /*Reserved*/,
    uint32_t dwType,
    const uint8_t* lpData,
    uint32_t cbData
) noexcept {
    char nameA[64];
    wideToAscii(lpValueName, nameA, sizeof(nameA));
    uint64_t st = NtSetValueKey(hKey, nameA, dwType, lpData, cbData);
    return NtOk(st) ? static_cast<int32_t>(WIN_ERROR_SUCCESS) : static_cast<int32_t>(WIN_ERROR_INVALID_PARAM);
}

__declspec(dllexport) int32_t RegSetValueExA(
    void* hKey,
    const char* lpValueName,
    uint32_t /*Reserved*/,
    uint32_t dwType,
    const uint8_t* lpData,
    uint32_t cbData
) noexcept {
    if ((dwType == WIN_REG_SZ || dwType == WIN_REG_EXPAND_SZ) && lpData) {
        // Store authentic UTF-16LE string inside binary regf hive!
        uint8_t utf16[160]{};
        uint32_t out = 0;
        for (uint32_t i = 0; i < cbData && out + 2 <= sizeof(utf16); ++i) {
            utf16[out++] = lpData[i];
            utf16[out++] = 0;
            if (lpData[i] == 0) break;
        }
        if (out == 0 || (out >= 2 && (utf16[out - 2] != 0 || utf16[out - 1] != 0))) {
            if (out + 2 <= sizeof(utf16)) { utf16[out++] = 0; utf16[out++] = 0; }
        }
        uint64_t st = NtSetValueKey(hKey, lpValueName ? lpValueName : "", dwType, utf16, out);
        return NtOk(st) ? static_cast<int32_t>(WIN_ERROR_SUCCESS) : static_cast<int32_t>(WIN_ERROR_INVALID_PARAM);
    }
    uint64_t st = NtSetValueKey(hKey, lpValueName ? lpValueName : "", dwType, lpData, cbData);
    return NtOk(st) ? static_cast<int32_t>(WIN_ERROR_SUCCESS) : static_cast<int32_t>(WIN_ERROR_INVALID_PARAM);
}

__declspec(dllexport) int32_t RegDeleteValueA(void* hKey, const char* lpValueName) noexcept {
    uint64_t st = NtDeleteValueKey(hKey, lpValueName ? lpValueName : "");
    return NtOk(st) ? static_cast<int32_t>(WIN_ERROR_SUCCESS) : static_cast<int32_t>(WIN_ERROR_FILE_NOT_FOUND);
}

__declspec(dllexport) int32_t RegDeleteValueW(void* hKey, const wchar_t* lpValueName) noexcept {
    char nameA[64];
    wideToAscii(lpValueName, nameA, sizeof(nameA));
    return RegDeleteValueA(hKey, nameA);
}

__declspec(dllexport) int32_t RegEnumValueA(
    void* hKey,
    uint32_t dwIndex,
    char* lpValueName,
    uint32_t* lpcchValueName,
    uint32_t* /*lpReserved*/,
    uint32_t* lpType,
    uint8_t* lpData,
    uint32_t* lpcbData
) noexcept {
    if (!lpValueName || !lpcchValueName) return static_cast<int32_t>(WIN_ERROR_INVALID_PARAM);
    uint8_t rawBuf[160]{};
    uint32_t rawLen = sizeof(rawBuf);
    uint32_t type = 0;
    uint64_t st = NtEnumerateValueKey(hKey, dwIndex, lpValueName, *lpcchValueName, &type, rawBuf, &rawLen);
    if (!NtOk(st)) return static_cast<int32_t>(WIN_ERROR_NO_MORE_ITEMS);

    uint32_t nlen = 0;
    while (lpValueName[nlen] && nlen < *lpcchValueName) ++nlen;
    *lpcchValueName = nlen;
    if (lpType) *lpType = type;

    if (lpcbData) {
        if (type == WIN_REG_SZ || type == WIN_REG_EXPAND_SZ) {
            uint32_t wchars = rawLen / 2;
            if (lpData) {
                uint32_t cap = *lpcbData;
                uint32_t out = 0;
                for (uint32_t i = 0; i < wchars && out < cap; ++i) {
                    uint16_t ch = static_cast<uint16_t>(rawBuf[i * 2]) |
                                  (static_cast<uint16_t>(rawBuf[i * 2 + 1]) << 8);
                    lpData[out++] = (ch < 128) ? static_cast<uint8_t>(ch) : '?';
                }
                if (cap > 0 && (out == 0 || lpData[out - 1] != 0)) {
                    lpData[(out < cap) ? out : (cap - 1)] = 0;
                }
            }
            *lpcbData = (wchars > 0) ? wchars : 1;
        } else {
            if (lpData) {
                uint32_t cp = (*lpcbData < rawLen) ? *lpcbData : rawLen;
                for (uint32_t i = 0; i < cp; ++i) lpData[i] = rawBuf[i];
            }
            *lpcbData = rawLen;
        }
    }
    return static_cast<int32_t>(WIN_ERROR_SUCCESS);
}

__declspec(dllexport) int32_t RegEnumKeyExA(
    void* hKey,
    uint32_t dwIndex,
    char* lpName,
    uint32_t* lpcchName,
    uint32_t* /*lpReserved*/,
    char* /*lpClass*/,
    uint32_t* /*lpcchClass*/,
    void* /*lpftLastWriteTime*/
) noexcept {
    if (!lpName || !lpcchName) return static_cast<int32_t>(WIN_ERROR_INVALID_PARAM);
    uint64_t st = NtEnumerateKey(hKey, dwIndex, lpName, *lpcchName);
    if (!NtOk(st)) return static_cast<int32_t>(WIN_ERROR_NO_MORE_ITEMS);
    uint32_t nlen = 0;
    while (lpName[nlen] && nlen < *lpcchName) ++nlen;
    *lpcchName = nlen;
    return static_cast<int32_t>(WIN_ERROR_SUCCESS);
}

__declspec(dllexport) int32_t RegFlushKey(void* hKey) noexcept {
    uint64_t st = NtFlushKey(hKey);
    return NtOk(st) ? static_cast<int32_t>(WIN_ERROR_SUCCESS) : static_cast<int32_t>(WIN_ERROR_INVALID_PARAM);
}

__declspec(dllexport) int32_t RegCloseKey(void* hKey) noexcept {
    uint64_t st = NtClose(hKey);
    return NtOk(st) ? static_cast<int32_t>(WIN_ERROR_SUCCESS) : static_cast<int32_t>(WIN_ERROR_INVALID_PARAM);
}

// ============================================================================
// 2. Security & Token APIs
// ============================================================================

__declspec(dllexport) bool OpenProcessToken(void* /*ProcessHandle*/, uint32_t /*DesiredAccess*/, void** TokenHandle) noexcept {
    if (TokenHandle) *TokenHandle = reinterpret_cast<void*>(0x100);
    return true;
}

__declspec(dllexport) bool GetTokenInformation(
    void* /*TokenHandle*/, int /*TokenInformationClass*/, void* /*TokenInformation*/,
    uint32_t /*TokenInformationLength*/, uint32_t* ReturnLength
) noexcept {
    if (ReturnLength) *ReturnLength = 32;
    return true;
}

// ============================================================================
// 3. Service Control Manager APIs
// ============================================================================

__declspec(dllexport) void* OpenSCManagerW(const wchar_t*, const wchar_t*, uint32_t) noexcept {
    return reinterpret_cast<void*>(++s_scmHandleCounter);
}
__declspec(dllexport) void* OpenSCManagerA(const char*, const char*, uint32_t) noexcept {
    return reinterpret_cast<void*>(++s_scmHandleCounter);
}
__declspec(dllexport) void* OpenServiceW(void*, const wchar_t*, uint32_t) noexcept {
    return reinterpret_cast<void*>(++s_scmHandleCounter);
}
__declspec(dllexport) void* OpenServiceA(void*, const char*, uint32_t) noexcept {
    return reinterpret_cast<void*>(++s_scmHandleCounter);
}
__declspec(dllexport) bool StartServiceW(void*, uint32_t, const wchar_t**) noexcept { return true; }
__declspec(dllexport) bool CloseServiceHandle(void*) noexcept { return true; }

#pragma pack(push, 1)
struct ServiceStatus {
    uint32_t dwServiceType{0x00000010};
    uint32_t dwCurrentState{0x00000004};
    uint32_t dwControlsAccepted{0x00000001};
    uint32_t dwWin32ExitCode{0};
    uint32_t dwServiceSpecificExitCode{0};
    uint32_t dwCheckPoint{0};
    uint32_t dwWaitHint{0};
};
#pragma pack(pop)

__declspec(dllexport) bool QueryServiceStatus(void*, ServiceStatus* lpServiceStatus) noexcept {
    if (!lpServiceStatus) return false;
    *lpServiceStatus = ServiceStatus{};
    return true;
}

} // extern "C"
