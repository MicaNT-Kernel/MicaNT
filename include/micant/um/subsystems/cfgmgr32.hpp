// ============================================================================
// MicaNT: Sovereign Operating System Executive
// include/micant/cfgmgr32.hpp - Windows Configuration Manager API (cfgmgr32.dll)
//
// 100% Clean-Room Architecture authored from Microsoft win32metadata interface definitions.
// Zero proprietary code used or referenced. Google LLC v. Oracle America (2021).
// ============================================================================

#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "ntdef.hpp"
#include "ldr.hpp"

namespace micant::cfgmgr32 {

using CONFIGRET = uint32_t;
using DEVINST   = uint32_t;
using PDEVINST  = uint32_t*;
using PCSTR     = const char*;
using PSTR      = char*;

inline constexpr CONFIGRET CR_SUCCESS                = 0x00000000;
inline constexpr CONFIGRET CR_DEFAULT                = 0x00000001;
inline constexpr CONFIGRET CR_OUT_OF_MEMORY          = 0x00000002;
inline constexpr CONFIGRET CR_INVALID_POINTER        = 0x00000003;
inline constexpr CONFIGRET CR_INVALID_FLAG           = 0x00000004;
inline constexpr CONFIGRET CR_INVALID_DEVNODE        = 0x00000005;
inline constexpr CONFIGRET CR_NO_SUCH_DEVNODE        = 0x0000000D;
inline constexpr CONFIGRET CR_BUFFER_SMALL           = 0x0000001A;

inline CONFIGRET CM_Get_Device_ID_List_SizeA(uint32_t* pulLen, [[maybe_unused]] PCSTR pszFilter, [[maybe_unused]] uint32_t ulFlags) noexcept {
    if (!pulLen) return CR_INVALID_POINTER;
    *pulLen = 2; // Empty double-null terminated multi-string
    return CR_SUCCESS;
}

inline CONFIGRET CM_Get_Device_ID_ListA([[maybe_unused]] PCSTR pszFilter, PSTR Buffer, uint32_t BufferLen, [[maybe_unused]] uint32_t ulFlags) noexcept {
    if (!Buffer) return CR_INVALID_POINTER;
    if (BufferLen < 2) return CR_BUFFER_SMALL;
    Buffer[0] = '\0';
    Buffer[1] = '\0';
    return CR_SUCCESS;
}

inline CONFIGRET CM_Locate_DevNodeA(PDEVINST pdnDevInst, [[maybe_unused]] PCSTR pDeviceID, [[maybe_unused]] uint32_t ulFlags) noexcept {
    if (!pdnDevInst) return CR_INVALID_POINTER;
    *pdnDevInst = 0x1000;
    return CR_SUCCESS;
}

inline CONFIGRET CM_Open_DevNode_Key([[maybe_unused]] DEVINST dnDevInst,
                                     [[maybe_unused]] uint32_t samDesired,
                                     [[maybe_unused]] uint32_t ulHardwareProfile,
                                     [[maybe_unused]] uint32_t Disposition,
                                     void* phkDevice,
                                     [[maybe_unused]] uint32_t ulFlags) noexcept {
    if (!phkDevice) return CR_INVALID_POINTER;
    *reinterpret_cast<void**>(phkDevice) = reinterpret_cast<void*>(0x2000);
    return CR_SUCCESS;
}

inline void InitializeCfgMgr32SubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("cfgmgr32.dll", "CM_Get_Device_ID_List_SizeA", reinterpret_cast<void*>(CM_Get_Device_ID_List_SizeA));
    ldr.registerExport("cfgmgr32.dll", "CM_Get_Device_ID_ListA", reinterpret_cast<void*>(CM_Get_Device_ID_ListA));
    ldr.registerExport("cfgmgr32.dll", "CM_Locate_DevNodeA", reinterpret_cast<void*>(CM_Locate_DevNodeA));
    ldr.registerExport("cfgmgr32.dll", "CM_Open_DevNode_Key", reinterpret_cast<void*>(CM_Open_DevNode_Key));
}

} // namespace micant::cfgmgr32
