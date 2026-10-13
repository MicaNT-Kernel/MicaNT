// ============================================================================
// MicaNT - Sovereign Windows NT Operating System Kernel & Subsystems
// Canonical Win32 & NT Subsystem Initialization Dispatcher
// (include/micant/subsystems.hpp)
// ============================================================================

#pragma once
#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/um/subsystems/kernel32.hpp"
#include "micant/um/subsystems/user32.hpp"
#include "micant/um/subsystems/gdi32.hpp"
#include "micant/um/subsystems/gdiplus.hpp"
#include "micant/um/subsystems/advapi32.hpp"
#include "micant/um/subsystems/shell32.hpp"
#include "micant/um/subsystems/comctl32.hpp"
#include "micant/um/subsystems/comdlg32.hpp"
#include "micant/um/subsystems/sensapi.hpp"
#include "micant/ucrt/msvcrt.hpp"
#include "micant/um/subsystems/msi.hpp"
#include "micant/um/subsystems/sspi.hpp"
#include "micant/um/subsystems/crypt32.hpp"
#include "micant/um/subsystems/ws2_32.hpp"
#include "micant/um/subsystems/ole32.hpp"
#include "micant/um/subsystems/oleaut32.hpp"
#include "micant/um/subsystems/uxtheme.hpp"
#include "micant/um/subsystems/propsys.hpp"
#include "micant/um/subsystems/urlmon.hpp"
#include "micant/um/subsystems/wininet.hpp"
#include "micant/um/subsystems/winspool.hpp"
#include "micant/um/subsystems/uiautomation.hpp"
#include "micant/um/subsystems/tsf.hpp"
#include "micant/um/native/ldr.hpp"
#include "micant/um/subsystems/mpr.hpp"
#include "micant/um/subsystems/virtdisk.hpp"
#include "micant/um/subsystems/wintrust.hpp"
#include "micant/um/subsystems/setupapi.hpp"
#include "micant/um/native/ntdll.hpp"
#include "micant/um/subsystems/aclui.hpp"
#include "micant/um/subsystems/winsta.hpp"
#include "micant/ucrt/icuuc.hpp"
#include "micant/um/subsystems/authz.hpp"
#include "micant/um/subsystems/userenv.hpp"
#include "micant/um/subsystems/winhttp.hpp"
#include "micant/um/subsystems/imm32.hpp"
#include "micant/um/subsystems/powrprof.hpp"
#include "micant/um/subsystems/dbgeng.hpp"
#include "micant/um/subsystems/winrt.hpp"
#include "micant/um/subsystems/iphlpapi.hpp"
#include "micant/um/subsystems/cipherksp.hpp"
#include "micant/um/subsystems/prism3d12.hpp"
#include "micant/um/subsystems/dwmapi.hpp"
#include "micant/um/subsystems/version.hpp"
#include "micant/um/subsystems/dbghelp.hpp"
#include "micant/um/subsystems/winmm.hpp"
#include "micant/um/subsystems/cfgmgr32.hpp"

namespace micant::subsystems {

inline void InitializeAllSubsystemExports() noexcept {
    sensapi::InitializeSensApiSubsystemExports();
    comdlg32::InitializeComDlg32SubsystemExports();
    comctl32::InitializeComCtl32SubsystemExports();
    gdi32::InitializeGdi32SubsystemExports();
    user32::InitializeUser32SubsystemExports();
    advapi32::InitializeAdvapi32SubsystemExports();
    shell32::InitializeShell32SubsystemExports();
    win32::InitializeWin32SubsystemExports();
    msvcrt::InitializeMsvcrtSubsystemExports();
    msi::InitializeMsiSubsystemExports();
    sspi::InitializeSspiSubsystemExports();
    crypt32::InitializeCrypt32SubsystemExports();
    ws2_32::InitializeWs2_32SubsystemExports();
    ole32::InitializeOle32SubsystemExports();
    oleaut32::InitializeOleAut32SubsystemExports();
    propsys::InitializePropSysSubsystemExports();
    uxtheme::InitializeUxThemeSubsystemExports();
    urlmon::InitializeUrlMonSubsystemExports();
    wininet::InitializeWinINetSubsystemExports();
    winspool::InitializePrintSpoolerSubsystemExports();
    uiautomation::InitializeUIAutomationSubsystemExports();
    tsf::InitializeTextServicesExports();
    mpr::InitializeMprSubsystemExports();
    setupapi::InitializeSetupApiSubsystemExports();
    iphlpapi::InitializeIpHlpApiSubsystemExports();
    crypto::InitializeBCryptSubsystemExports();
    prism3d12::InitializePrism3D12SubsystemExports();
    icuuc::InitializeIcuucSubsystemExports();
    authz::InitializeAuthzSubsystemExports();
    userenv::InitializeUserenvSubsystemExports();
    winhttp::InitializeWinHttpSubsystemExports();
    imm32::InitializeImm32SubsystemExports();
    powrprof::InitializePowrProfSubsystemExports();
    dbgeng::InitializeDbgEngSubsystemExports();
    winrt::InitializeWinRTSubsystemExports();
    gdiplus::InitializeGdiPlusExports();
    virtdisk::InitializeVirtualDiskSubsystemExports();
    wintrust::InitializeWinTrustSubsystemExports();
    ntdll::InitializeNtdllSubsystemExports();
    aclui::InitializeAcluiSubsystemExports();
    winsta::InitializeWinStaSubsystemExports();
    dwm::InitializeDWMSubsystemExports();
    version::InitializeVersionExports();
    dbghelp::InitializeDbgHelpExports();
    winmm::InitializeWinMMExports();
    cfgmgr32::InitializeCfgMgr32SubsystemExports();
}

} // namespace micant::subsystems
