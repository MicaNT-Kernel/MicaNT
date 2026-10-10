// ============================================================================
// MicaNT: Windows User Environment Subsystem (userenv.dll)
// (userenv.hpp)
//
// Modern Clean-Room Implementation in pure ISO C++23.
// Provides User Profile Directory path discovery conforming to win32metadata.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstring>
#include "ntdef.hpp"
#include "ldr.hpp"

namespace micant::userenv {

using BOOL = int32_t;
using DWORD = uint32_t;
using HANDLE = void*;

inline BOOL WINAPI GetUserProfileDirectoryW(HANDLE /*hToken*/, wchar_t* lpProfileDir, DWORD* lpcchSize) noexcept {
    static const wchar_t defaultProfile[] = L"C:\\Users\\admin";
    if (!lpcchSize) return 0;
    if (!lpProfileDir || *lpcchSize < 16) {
        *lpcchSize = 16;
        return 0;
    }
    std::memcpy(lpProfileDir, defaultProfile, sizeof(defaultProfile));
    *lpcchSize = 15;
    return 1;
}

inline void InitializeUserenvSubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("userenv.dll", "GetUserProfileDirectoryW", reinterpret_cast<void*>(GetUserProfileDirectoryW));
}

} // namespace micant::userenv
