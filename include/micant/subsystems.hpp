// ============================================================================
// MicaNT - Sovereign Windows NT Operating System Kernel & Subsystems
// Canonical Win32 & NT Subsystem Initialization Dispatcher
// (include/micant/subsystems.hpp)
// ============================================================================

#pragma once

#include "micant/ntdef.hpp"
#include "micant/ntstatus.hpp"
#include "micant/kernel32.hpp"
#include "micant/user32.hpp"
#include "micant/gdi32.hpp"
#include "micant/gdiplus.hpp"
#include "micant/advapi32.hpp"
#include "micant/shell32.hpp"
#include "micant/comctl32.hpp"
#include "micant/comdlg32.hpp"
#include "micant/sensapi.hpp"
#include "micant/msvcrt.hpp"
#include "micant/msi.hpp"
#include "micant/sspi.hpp"
#include "micant/crypt32.hpp"
#include "micant/ws2_32.hpp"
#include "micant/ole32.hpp"
#include "micant/oleaut32.hpp"
#include "micant/uxtheme.hpp"
#include "micant/propsys.hpp"
#include "micant/urlmon.hpp"
#include "micant/wininet.hpp"
#include "micant/winspool.hpp"
#include "micant/uiautomation.hpp"
#include "micant/tsf.hpp"
#include "micant/ldr.hpp"
#include "micant/mpr.hpp"
#include "micant/virtdisk.hpp"
#include "micant/wintrust.hpp"
#include "micant/setupapi.hpp"
#include "micant/ntdll.hpp"
#include "micant/aclui.hpp"
#include "micant/winsta.hpp"
#include "micant/icuuc.hpp"
#include "micant/authz.hpp"
#include "micant/userenv.hpp"
#include "micant/winhttp.hpp"
#include "micant/imm32.hpp"
#include "micant/powrprof.hpp"
#include "micant/dbgeng.hpp"
#include "micant/winrt.hpp"
#include "micant/iphlpapi.hpp"
#include "micant/cipherksp.hpp"
#include "micant/prism3d12.hpp"
#include "micant/dwmapi.hpp"
#include "micant/version.hpp"
#include "micant/dbghelp.hpp"
#include "micant/winmm.hpp"
#include "micant/cfgmgr32.hpp"

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
