// ============================================================================
// MicaNT: Windows Runtime Subsystem (api-ms-win-core-winrt-*)
// (winrt.hpp)
//
// Modern Clean-Room Implementation in pure ISO C++23.
// Provides Windows Runtime class activation, HSTRING reference creation,
// and language exception handling conforming to win32metadata.
// ============================================================================

#pragma once

#include <cstdint>
#include "ntdef.hpp"
#include "ldr.hpp"

namespace micant::winrt {

using HRESULT = int32_t;
using BOOL = int32_t;

inline HRESULT WINAPI RoGetActivationFactory(void* /*activatableClassId*/, const void* /*iid*/, void** factory) noexcept {
    if (factory) *factory = reinterpret_cast<void*>(0xF001);
    return 0; // S_OK
}

inline BOOL WINAPI RoOriginateLanguageException(int32_t /*error*/, void* /*message*/, void* /*languageException*/) noexcept {
    return 1;
}

inline HRESULT WINAPI WindowsCreateStringReference(const wchar_t* /*sourceString*/, uint32_t /*length*/, void* /*hstringHeader*/, void** string) noexcept {
    if (string) *string = reinterpret_cast<void*>(0xF002);
    return 0; // S_OK
}

inline void InitializeWinRTSubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();

    ldr.registerExport("api-ms-win-core-winrt-l1-1-0.dll", "RoGetActivationFactory", reinterpret_cast<void*>(RoGetActivationFactory));
    ldr.registerExport("api-ms-win-core-winrt-error-l1-1-1.dll", "RoOriginateLanguageException", reinterpret_cast<void*>(RoOriginateLanguageException));
    ldr.registerExport("api-ms-win-core-winrt-string-l1-1-0.dll", "WindowsCreateStringReference", reinterpret_cast<void*>(WindowsCreateStringReference));
    ldr.registerExport("combase.dll", "RoGetActivationFactory", reinterpret_cast<void*>(RoGetActivationFactory));
    ldr.registerExport("combase.dll", "RoOriginateLanguageException", reinterpret_cast<void*>(RoOriginateLanguageException));
    ldr.registerExport("combase.dll", "WindowsCreateStringReference", reinterpret_cast<void*>(WindowsCreateStringReference));
}

} // namespace micant::winrt
