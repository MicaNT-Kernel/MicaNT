// ============================================================================
// MicaNT: Sovereign Operating System Executive
// include/micant/mpr.hpp - Multiple Provider Router (mpr.dll) Subsystem
//
// 100% Clean-Room Architecture authored from Microsoft win32metadata interface definitions.
// Zero proprietary code used or referenced. Google LLC v. Oracle America (2021).
// ============================================================================

#pragma once

#include <cstdint>
#include <cwchar>
#include <cstring>
#include "ntdef.hpp"
#include "ldr.hpp"

namespace micant::mpr {

using DWORD = uint32_t;
using HANDLE = void*;
using BOOL = int32_t;

// Standard WNet Error Codes
inline constexpr DWORD WN_SUCCESS          = 0;
inline constexpr DWORD WN_NO_MORE_ENTRIES  = 259;
inline constexpr DWORD ERROR_NO_NETWORK    = 1222;
inline constexpr DWORD ERROR_BAD_NET_NAME  = 67;
inline constexpr DWORD ERROR_NOT_CONTAINER = 1207;

// NetResource structure definition
struct NETRESOURCEW {
    DWORD    dwScope;
    DWORD    dwType;
    DWORD    dwDisplayType;
    DWORD    dwUsage;
    wchar_t* lpLocalName;
    wchar_t* lpRemoteName;
    wchar_t* lpComment;
    wchar_t* lpProvider;
};

inline DWORD __stdcall WNetOpenEnumW([[maybe_unused]] DWORD dwScope,
                                     [[maybe_unused]] DWORD dwType,
                                     [[maybe_unused]] DWORD dwUsage,
                                     [[maybe_unused]] NETRESOURCEW* lpNetResource,
                                     HANDLE* lphEnum) noexcept {
    if (lphEnum) {
        *lphEnum = reinterpret_cast<HANDLE>(0xFEED);
    }
    return WN_SUCCESS;
}

inline DWORD __stdcall WNetEnumResourceW([[maybe_unused]] HANDLE hEnum,
                                         DWORD* lpcCount,
                                         [[maybe_unused]] void* lpBuffer,
                                         [[maybe_unused]] DWORD* lpBufferSize) noexcept {
    if (lpcCount) {
        *lpcCount = 0;
    }
    return WN_NO_MORE_ENTRIES;
}

inline DWORD __stdcall WNetCloseEnum([[maybe_unused]] HANDLE hEnum) noexcept {
    return WN_SUCCESS;
}

inline DWORD __stdcall WNetAddConnection2W([[maybe_unused]] NETRESOURCEW* lpNetResource,
                                           [[maybe_unused]] const wchar_t* lpPassword,
                                           [[maybe_unused]] const wchar_t* lpUserName,
                                           [[maybe_unused]] DWORD dwFlags) noexcept {
    return WN_SUCCESS;
}

inline DWORD __stdcall WNetGetResourceParentW([[maybe_unused]] NETRESOURCEW* lpNetResource,
                                              [[maybe_unused]] void* lpBuffer,
                                              [[maybe_unused]] DWORD* lpcbBuffer) noexcept {
    return ERROR_NOT_CONTAINER;
}

inline DWORD __stdcall WNetGetResourceInformationW([[maybe_unused]] NETRESOURCEW* lpNetResource,
                                                   [[maybe_unused]] void* lpBuffer,
                                                   [[maybe_unused]] DWORD* lpcbBuffer,
                                                   wchar_t** lplpSystem) noexcept {
    if (lplpSystem) {
        *lplpSystem = nullptr;
    }
    return ERROR_BAD_NET_NAME;
}

inline DWORD __stdcall WNetGetConnectionW([[maybe_unused]] const wchar_t* lpLocalName,
                                          wchar_t* lpRemoteName,
                                          DWORD* lpnLength) noexcept {
    if (lpnLength && *lpnLength > 0 && lpRemoteName) {
        lpRemoteName[0] = L'\0';
    }
    return ERROR_NO_NETWORK;
}

inline void InitializeMprSubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("mpr.dll", "WNetOpenEnumW", reinterpret_cast<void*>(WNetOpenEnumW));
    ldr.registerExport("mpr.dll", "WNetEnumResourceW", reinterpret_cast<void*>(WNetEnumResourceW));
    ldr.registerExport("mpr.dll", "WNetCloseEnum", reinterpret_cast<void*>(WNetCloseEnum));
    ldr.registerExport("mpr.dll", "WNetAddConnection2W", reinterpret_cast<void*>(WNetAddConnection2W));
    ldr.registerExport("mpr.dll", "WNetGetResourceParentW", reinterpret_cast<void*>(WNetGetResourceParentW));
    ldr.registerExport("mpr.dll", "WNetGetResourceInformationW", reinterpret_cast<void*>(WNetGetResourceInformationW));
    ldr.registerExport("mpr.dll", "WNetGetConnectionW", reinterpret_cast<void*>(WNetGetConnectionW));
}

} // namespace micant::mpr
