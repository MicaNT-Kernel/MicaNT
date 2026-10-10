// ============================================================================
// MicaNT: Sovereign Operating System Executive
// include/micant/uiautomation.hpp - UI Automation Core Subsystem (uiautomationcore.dll)
//
// 100% Clean-Room Architecture authored from Microsoft win32metadata interface definitions.
// Zero proprietary code used or referenced. Google LLC v. Oracle America (2021).
// ============================================================================

#pragma once

#include <cstdint>
#include "ntdef.hpp"
#include "ldr.hpp"

namespace micant::uiautomation {

inline int32_t __stdcall UiaRaiseStructureChangedEvent([[maybe_unused]] void* pProvider, [[maybe_unused]] void* structureChangeType, [[maybe_unused]] int* pRuntimeId, [[maybe_unused]] int cRuntimeIdLen) noexcept {
    return 0; // S_OK
}

inline int32_t __stdcall UiaGetReservedNotSupportedValue(void** punkNotSupportedValue) noexcept {
    if (!punkNotSupportedValue) return -2147467261; // E_POINTER
    *punkNotSupportedValue = reinterpret_cast<void*>(0x9910);
    return 0; // S_OK
}

inline int32_t __stdcall UiaRaiseAutomationEvent([[maybe_unused]] void* pProvider, [[maybe_unused]] int id) noexcept {
    return 0; // S_OK
}

inline intptr_t __stdcall UiaReturnRawElementProvider([[maybe_unused]] void* hwnd, [[maybe_unused]] uint64_t wParam, [[maybe_unused]] int64_t lParam, [[maybe_unused]] void* el) noexcept {
    return 1;
}

inline int32_t __stdcall UiaHostProviderFromHwnd([[maybe_unused]] void* hwnd, void** ppProvider) noexcept {
    if (!ppProvider) return -2147467261; // E_POINTER
    *ppProvider = reinterpret_cast<void*>(0x9911);
    return 0; // S_OK
}

inline int32_t __stdcall AccessibleObjectFromWindow(void* /*hwnd*/, uint32_t /*dwId*/, const void* /*riid*/, void** ppvObject) noexcept {
    if (ppvObject) {
        static uintptr_t s_acc = 0xDD00;
        *ppvObject = reinterpret_cast<void*>(++s_acc);
    }
    return 0; // S_OK
}

inline int32_t __stdcall CreateStdAccessibleObject(void* /*hwnd*/, int32_t /*idObject*/, const void* /*riid*/, void** ppvObject) noexcept {
    if (ppvObject) {
        static uintptr_t s_accStd = 0xDE00;
        *ppvObject = reinterpret_cast<void*>(++s_accStd);
    }
    return 0; // S_OK
}

inline int32_t __stdcall UiaRaiseAutomationPropertyChangedEvent(void* /*pProvider*/, int32_t /*id*/, void* /*oldValue*/, void* /*newValue*/) noexcept {
    return 0; // S_OK
}

inline int32_t __stdcall UiaClientsAreListening() noexcept {
    return 0;
}

inline int32_t __stdcall UiaRaiseNotificationEvent(void* /*pProvider*/, int32_t /*notificationKind*/, int32_t /*notificationProcessing*/, void* /*displayString*/, void* /*activityId*/) noexcept {
    return 0; // S_OK
}

inline void InitializeUIAutomationSubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("uiautomationcore.dll", "UiaRaiseStructureChangedEvent", reinterpret_cast<void*>(UiaRaiseStructureChangedEvent));
    ldr.registerExport("uiautomationcore.dll", "UiaGetReservedNotSupportedValue", reinterpret_cast<void*>(UiaGetReservedNotSupportedValue));
    ldr.registerExport("uiautomationcore.dll", "UiaRaiseAutomationEvent", reinterpret_cast<void*>(UiaRaiseAutomationEvent));
    ldr.registerExport("uiautomationcore.dll", "UiaReturnRawElementProvider", reinterpret_cast<void*>(UiaReturnRawElementProvider));
    ldr.registerExport("uiautomationcore.dll", "UiaHostProviderFromHwnd", reinterpret_cast<void*>(UiaHostProviderFromHwnd));
    ldr.registerExport("uiautomationcore.dll", "UiaRaiseAutomationPropertyChangedEvent", reinterpret_cast<void*>(UiaRaiseAutomationPropertyChangedEvent));
    ldr.registerExport("uiautomationcore.dll", "UiaClientsAreListening", reinterpret_cast<void*>(UiaClientsAreListening));
    ldr.registerExport("uiautomationcore.dll", "UiaRaiseNotificationEvent", reinterpret_cast<void*>(UiaRaiseNotificationEvent));

    // oleacc.dll exports
    ldr.registerExport("oleacc.dll", "AccessibleObjectFromWindow", reinterpret_cast<void*>(AccessibleObjectFromWindow));
    ldr.registerExport("oleacc.dll", "CreateStdAccessibleObject", reinterpret_cast<void*>(CreateStdAccessibleObject));
}

} // namespace micant::uiautomation
