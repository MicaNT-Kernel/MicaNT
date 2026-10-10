// ============================================================================
// MicaNT: Win32 Subsystem Graduation Bridge (satellite_win32.hpp)
//
// Modern Clean-Room Forwarding Bridge in pure ISO C++23.
// All exports have graduated directly to Canonical Core MicaNT Subsystems:
//   - SensApi.dll  -> include/micant/sensapi.hpp
//   - COMDLG32.dll -> include/micant/comdlg32.hpp
//   - COMCTL32.dll -> include/micant/comctl32.hpp
//   - MSIMG32.dll  -> include/micant/gdi32.hpp
//   - USER32.dll   -> include/micant/user32.hpp
//   - ADVAPI32.dll -> include/micant/advapi32.hpp
//   - SHELL32.dll  -> include/micant/shell32.hpp
//   - KERNEL32.dll -> include/micant/kernel32.hpp
//   - MSVCRT.dll   -> include/micant/msvcrt.hpp
// ============================================================================

#pragma once

#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <cwchar>
#include "ntdef.hpp"
#include "kernel32.hpp"
#include "user32.hpp"
#include "gdi32.hpp"
#include "advapi32.hpp"
#include "shell32.hpp"
#include "comctl32.hpp"
#include "comdlg32.hpp"
#include "sensapi.hpp"
#include "msvcrt.hpp"
#include "msi.hpp"
#include "sspi.hpp"
#include "crypt32.hpp"
#include "ws2_32.hpp"
#include "ole32.hpp"
#include "urlmon.hpp"
#include "wininet.hpp"
#include "winspool.hpp"
#include "uiautomation.hpp"
#include "tsf.hpp"
#include "ldr.hpp"
#include "mpr.hpp"
#include "satellite_wiztree.hpp"
#include "satellite_putty.hpp"
#include "satellite_gdiplus.hpp"
#include "satellite_sumatra.hpp"
#include "satellite_everything.hpp"
#include "satellite_winmerge.hpp"
#include "satellite_rufus.hpp"
#include "satellite_system_informer.hpp"
#include "satellite_qbittorrent.hpp"
#include "satellite_winscp.hpp"
#include "satellite_wireshark.hpp"
#include "satellite_filezilla.hpp"

namespace micant::satellite {

using LPCWSTR = const wchar_t*;

// Canonical Core Forwarding Aliases
using sensapi::NETWORK_ALIVE_LAN;
using sensapi::NETWORK_ALIVE_WAN;
using sensapi::IsNetworkAlive;
using sensapi::IsDestinationReachableW;

using gdi32::AlphaBlend;
using gdi32::BLENDFUNCTION;

using comdlg32::PrintDlgW;
using comdlg32::ChooseColorW;
using comdlg32::GetOpenFileNameW;
using comdlg32::GetSaveFileNameW;
using comdlg32::CommDlgExtendedError;

using comctl32::CreateToolbarEx;
using comctl32::PropertySheetW;

using user32::MapDialogRect;
using user32::GetMonitorInfoA;
using user32::GetDialogBaseUnits;
using user32::CheckRadioButton;
using user32::IsDlgButtonChecked;
using user32::CheckDlgButton;
using user32::LoadAcceleratorsW;
using user32::GetClassInfoW;

using advapi32::LsaAddAccountRights;
using advapi32::GetUserNameW;
using advapi32::RegDeleteKeyExW;

using shell32::ExtractIconExW;
using shell32::SHGetDesktopFolder;
using shell32::SHGetSpecialFolderLocation;
using shell32::SHChangeNotify;
using shell32::SHGetPathFromIDListW;
using shell32::SHBrowseForFolderW;

using win32::GetWindowsDirectoryW;
using win32::GetDriveTypeW;
using win32::GetVolumeInformationW;
using win32::SetPriorityClass;
using win32::GetSystemDefaultLangID;
using win32::GetUserDefaultLangID;
using win32::GetCompressedFileSizeW;
using win32::FindFirstChangeNotificationW;
using win32::FindNextChangeNotification;
using win32::FindCloseChangeNotification;

inline void Mica_PureCall() noexcept {}
inline void Mica_Srand(unsigned int seed) noexcept { std::srand(seed); }

// ----------------------------------------------------------------------------
// Subsystem Initialization Orchestrator
// ----------------------------------------------------------------------------

inline void InitializeSatelliteWin32Exports() {
    auto& ldr = ldr::DynamicLoader::get();

    // Canonical Core Subsystem Initializations
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
    urlmon::InitializeUrlMonSubsystemExports();
    wininet::InitializeWinINetSubsystemExports();
    winspool::InitializePrintSpoolerSubsystemExports();
    uiautomation::InitializeUIAutomationSubsystemExports();
    tsf::InitializeTextServicesExports();

    // WizTree 4.x Subsystem Extensions
    wiztree::InitializeWizTreeWin32Exports();

    // PuTTY 0.82+ Subsystem Extensions
    putty::registerPuTTYExports(ldr);

    // GDI+ 2D Vector & Imaging Subsystem
    gdiplus::InitializeGdiPlusSatelliteExports();

    // SumatraPDF 3.6+ Subsystem Extensions
    sumatra::InitializeSumatraWin32Exports();

    // Everything 1.4+ Search Subsystem Extensions
    everything::InitializeEverythingExports();

    // WinMerge 2.16+ Visual Diff & Merge Subsystem Extensions
    winmerge::InitializeWinMergeExports();

    // Rufus 4.x Storage & Low-Level Hardware Subsystem Extensions
    rufus::InitializeRufusExports();

    // System Informer 4.0 Native NT & Diagnostics Subsystem Extensions
    system_informer::InitializeSystemInformerExports();

    // qBittorrent 5.2+ High-Throughput I/O & Networking Subsystem Extensions
    qbittorrent::InitializeQBittorrentExports();

    // WinSCP 6.5+ Secure Remote File Management Subsystem Extensions
    winscp::InitializeWinScpExports();

    // Wireshark 4.x / Universal C Runtime & MSVC STL Subsystem Extensions
    wireshark::InitializeWiresharkExports();

    // FileZilla 3.x / Sovereign Networking & Enterprise FTP Subsystem Extensions
    filezilla::InitializeFileZillaExports();

    // MPR 1.0 Network Provider Router Subsystem
    mpr::InitializeMprSubsystemExports();
}

} // namespace micant::satellite
