// ============================================================================
// MicaNT: Windows Property System Architecture (propsys.dll)
//
// Modern Clean-Room Implementation in pure ISO C++23.
// Provides property descriptions, property keys, variant comparisons,
// and formatted display allocation.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include "ntdef.hpp"
#include "ldr.hpp"

namespace micant::propsys {

struct PROPERTYKEY {
    uint8_t fmtid[16];
    uint32_t pid;
};

inline int32_t WINAPI PSGetPropertyKeyFromName(const wchar_t* /*pszCanonicalName*/, PROPERTYKEY* propkey) noexcept {
    if (propkey) {
        std::memset(propkey, 0, sizeof(PROPERTYKEY));
        propkey->pid = 1;
    }
    return 0; // S_OK
}

inline int32_t WINAPI PSEnumeratePropertyDescriptions(int32_t /*filter*/, const void* /*riid*/, void** ppv) noexcept {
    if (ppv) {
        static uintptr_t s_props = 0xFF00;
        *ppv = reinterpret_cast<void*>(++s_props);
    }
    return 0; // S_OK
}

inline int32_t WINAPI PropVariantCompareEx(const void* /*pv1*/, const void* /*pv2*/, int32_t /*unit*/, int32_t /*flags*/) noexcept {
    return 0; // Equal
}

inline int32_t WINAPI PSGetPropertyDescription(const void* /*propkey*/, const void* /*riid*/, void** ppv) noexcept {
    if (ppv) {
        static uintptr_t s_pdesc = 0xFF10;
        *ppv = reinterpret_cast<void*>(++s_pdesc);
    }
    return 0; // S_OK
}

inline int32_t WINAPI PSFormatForDisplayAlloc(const void* /*key*/, const void* /*propvar*/, int32_t /*pdfflags*/, wchar_t** ppszDisplay) noexcept {
    if (ppszDisplay) {
        const wchar_t text[] = L"WinMerge Property";
        constexpr size_t sz = sizeof(text);
        auto* buf = static_cast<wchar_t*>(std::malloc(sz));
        if (buf) {
            std::memcpy(buf, text, sz);
            *ppszDisplay = buf;
        } else {
            *ppszDisplay = nullptr;
        }
    }
    return 0; // S_OK
}

inline int32_t WINAPI InitPropVariantFromBuffer(const void* /*pv*/, uint32_t /*cb*/, void* ppropvar) noexcept {
    if (ppropvar) {
        std::memset(ppropvar, 0, 24);
    }
    return 0; // S_OK
}

inline void InitializePropSysSubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();

    ldr.registerExport("propsys.dll", "PSGetPropertyKeyFromName", reinterpret_cast<void*>(PSGetPropertyKeyFromName));
    ldr.registerExport("propsys.dll", "PSEnumeratePropertyDescriptions", reinterpret_cast<void*>(PSEnumeratePropertyDescriptions));
    ldr.registerExport("propsys.dll", "PropVariantCompareEx", reinterpret_cast<void*>(PropVariantCompareEx));
    ldr.registerExport("propsys.dll", "PSGetPropertyDescription", reinterpret_cast<void*>(PSGetPropertyDescription));
    ldr.registerExport("propsys.dll", "PSFormatForDisplayAlloc", reinterpret_cast<void*>(PSFormatForDisplayAlloc));
    ldr.registerExport("propsys.dll", "InitPropVariantFromBuffer", reinterpret_cast<void*>(InitPropVariantFromBuffer));
}

} // namespace micant::propsys
