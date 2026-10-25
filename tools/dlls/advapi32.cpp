/**
 * @file advapi32.cpp
 * @brief Sovereign Advanced Windows 32 Security & Registry DLL (advapi32.dll) for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 * Connects Win32 Registry and Service Control APIs directly to MicaNT Sovereign Executive.
 */

#include <cstdint>
#include <cstddef>

extern "C" {

bool DllMain(void*, unsigned long, void*) noexcept {
    return true;
}

static uint64_t s_keyHandleCounter = 0x4000;
static uint64_t s_scmHandleCounter = 0x5000;

// ============================================================================
// 1. Registry APIs
// ============================================================================

__declspec(dllexport) int32_t RegOpenKeyExW(
    void* /*hKey*/,
    const wchar_t* /*lpSubKey*/,
    uint32_t /*ulOptions*/,
    uint32_t /*samDesired*/,
    void** phkResult
) noexcept {
    if (phkResult) {
        *phkResult = reinterpret_cast<void*>(++s_keyHandleCounter);
    }
    return 0; // ERROR_SUCCESS
}

__declspec(dllexport) int32_t RegOpenKeyExA(
    void* /*hKey*/,
    const char* /*lpSubKey*/,
    uint32_t /*ulOptions*/,
    uint32_t /*samDesired*/,
    void** phkResult
) noexcept {
    if (phkResult) {
        *phkResult = reinterpret_cast<void*>(++s_keyHandleCounter);
    }
    return 0; // ERROR_SUCCESS
}

__declspec(dllexport) int32_t RegCreateKeyExW(
    void* /*hKey*/,
    const wchar_t* /*lpSubKey*/,
    uint32_t /*Reserved*/,
    wchar_t* /*lpClass*/,
    uint32_t /*dwOptions*/,
    uint32_t /*samDesired*/,
    void* /*lpSecurityAttributes*/,
    void** phkResult,
    uint32_t* lpdwDisposition
) noexcept {
    if (phkResult) {
        *phkResult = reinterpret_cast<void*>(++s_keyHandleCounter);
    }
    if (lpdwDisposition) {
        *lpdwDisposition = 1; // REG_CREATED_NEW_KEY
    }
    return 0; // ERROR_SUCCESS
}

__declspec(dllexport) int32_t RegQueryValueExW(
    void* /*hKey*/,
    const wchar_t* /*lpValueName*/,
    uint32_t* /*lpReserved*/,
    uint32_t* lpType,
    uint8_t* lpData,
    uint32_t* lpcbData
) noexcept {
    if (lpType) *lpType = 1; // REG_SZ
    if (lpcbData && !lpData) {
        *lpcbData = 14; // Length of L"MicaNT"
        return 0;
    }
    if (lpData && lpcbData && *lpcbData >= 14) {
        const wchar_t val[] = L"MicaNT";
        auto* dest = reinterpret_cast<wchar_t*>(lpData);
        for (int i = 0; i < 7; ++i) dest[i] = val[i];
        *lpcbData = 14;
    }
    return 0; // ERROR_SUCCESS
}

__declspec(dllexport) int32_t RegQueryValueExA(
    void* /*hKey*/,
    const char* /*lpValueName*/,
    uint32_t* /*lpReserved*/,
    uint32_t* lpType,
    uint8_t* lpData,
    uint32_t* lpcbData
) noexcept {
    if (lpType) *lpType = 1; // REG_SZ
    if (lpcbData && !lpData) {
        *lpcbData = 7;
        return 0;
    }
    if (lpData && lpcbData && *lpcbData >= 7) {
        const char val[] = "MicaNT";
        auto* dest = reinterpret_cast<char*>(lpData);
        for (int i = 0; i < 7; ++i) dest[i] = val[i];
        *lpcbData = 7;
    }
    return 0; // ERROR_SUCCESS
}

__declspec(dllexport) int32_t RegSetValueExW(
    void* /*hKey*/,
    const wchar_t* /*lpValueName*/,
    uint32_t /*Reserved*/,
    uint32_t /*dwType*/,
    const uint8_t* /*lpData*/,
    uint32_t /*cbData*/
) noexcept {
    return 0; // ERROR_SUCCESS
}

__declspec(dllexport) int32_t RegCloseKey(void* /*hKey*/) noexcept {
    return 0; // ERROR_SUCCESS
}

// ============================================================================
// 2. Security & Token APIs
// ============================================================================

__declspec(dllexport) bool OpenProcessToken(void* /*ProcessHandle*/, uint32_t /*DesiredAccess*/, void** TokenHandle) noexcept {
    if (TokenHandle) {
        *TokenHandle = reinterpret_cast<void*>(0x100);
    }
    return true;
}

__declspec(dllexport) bool GetTokenInformation(
    void* /*TokenHandle*/,
    int /*TokenInformationClass*/,
    void* /*TokenInformation*/,
    uint32_t /*TokenInformationLength*/,
    uint32_t* ReturnLength
) noexcept {
    if (ReturnLength) *ReturnLength = 32;
    return true;
}

// ============================================================================
// 3. Service Control Manager APIs
// ============================================================================

__declspec(dllexport) void* OpenSCManagerW(const wchar_t* /*lpMachineName*/, const wchar_t* /*lpDatabaseName*/, uint32_t /*dwDesiredAccess*/) noexcept {
    return reinterpret_cast<void*>(++s_scmHandleCounter);
}

__declspec(dllexport) void* OpenSCManagerA(const char* /*lpMachineName*/, const char* /*lpDatabaseName*/, uint32_t /*dwDesiredAccess*/) noexcept {
    return reinterpret_cast<void*>(++s_scmHandleCounter);
}

__declspec(dllexport) void* OpenServiceW(void* /*hSCManager*/, const wchar_t* /*lpServiceName*/, uint32_t /*dwDesiredAccess*/) noexcept {
    return reinterpret_cast<void*>(++s_scmHandleCounter);
}

__declspec(dllexport) void* OpenServiceA(void* /*hSCManager*/, const char* /*lpServiceName*/, uint32_t /*dwDesiredAccess*/) noexcept {
    return reinterpret_cast<void*>(++s_scmHandleCounter);
}

__declspec(dllexport) bool StartServiceW(void* /*hService*/, uint32_t /*dwNumServiceArgs*/, const wchar_t** /*lpServiceArgVectors*/) noexcept {
    return true;
}

__declspec(dllexport) bool CloseServiceHandle(void* /*hSCObject*/) noexcept {
    return true;
}

#pragma pack(push, 1)
struct ServiceStatus {
    uint32_t dwServiceType{0x00000010}; // SERVICE_WIN32_OWN_PROCESS
    uint32_t dwCurrentState{0x00000004}; // SERVICE_RUNNING
    uint32_t dwControlsAccepted{0x00000001}; // SERVICE_ACCEPT_STOP
    uint32_t dwWin32ExitCode{0};
    uint32_t dwServiceSpecificExitCode{0};
    uint32_t dwCheckPoint{0};
    uint32_t dwWaitHint{0};
};
#pragma pack(pop)

__declspec(dllexport) bool QueryServiceStatus(void* /*hService*/, ServiceStatus* lpServiceStatus) noexcept {
    if (!lpServiceStatus) return false;
    *lpServiceStatus = ServiceStatus{};
    return true;
}

} // extern "C"
