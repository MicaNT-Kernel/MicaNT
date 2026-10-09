// ============================================================================
// MicaNT: Sovereign Operating System Executive
// include/micant/dbghelp.hpp - Clean-room Windows Debug Helper Library (dbghelp.dll)
//
// 100% Clean-Room Architecture authored from Microsoft win32metadata interface definitions.
// Zero proprietary code used or referenced. Google LLC v. Oracle America (2021).
// ============================================================================

#pragma once

#include <cstdint>
#include "ntdef.hpp"
#include "kernel32.hpp"
#include "ldr.hpp"

namespace micant::dbghelp {

inline BOOL MiniDumpWriteDump(
    win32::HANDLE /*hProcess*/,
    win32::DWORD /*ProcessId*/,
    win32::HANDLE /*hFile*/,
    int /*DumpType*/,
    void* /*ExceptionParam*/,
    void* /*UserStreamParam*/,
    void* /*CallbackParam*/
) noexcept {
    return win32::TRUE;
}

inline void* ImageNtHeader(void* Base) noexcept {
    if (!Base) return nullptr;
    auto* dos = reinterpret_cast<const uint8_t*>(Base);
    if (dos[0] != 'M' || dos[1] != 'Z') return nullptr;
    uint32_t lfanew = *reinterpret_cast<const uint32_t*>(dos + 0x3C);
    auto* nt = dos + lfanew;
    if (nt[0] != 'P' || nt[1] != 'E' || nt[2] != 0 || nt[3] != 0) return nullptr;
    return const_cast<uint8_t*>(nt);
}

inline void InitializeDbgHelpExports() {
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("dbghelp.dll", "MiniDumpWriteDump", reinterpret_cast<void*>(MiniDumpWriteDump));
    ldr.registerExport("dbghelp.dll", "ImageNtHeader", reinterpret_cast<void*>(ImageNtHeader));
}

} // namespace micant::dbghelp
