// ============================================================================
// MicaNT: PuTTY 0.82+ Win32 Subsystem Graduation Bridge (satellite_putty.hpp)
//
// Modern Clean-Room Forwarding Bridge in pure ISO C++23.
// All 70 PuTTY Win32 subsystem exports have graduated directly to Canonical
// Core MicaNT Subsystems:
//   - ADVAPI32.dll -> include/micant/advapi32.hpp (CopySid, GetUserNameA, RegDeleteKeyA, RegEnumKeyA)
//   - COMDLG32.dll -> include/micant/comdlg32.hpp (ChooseColorA, ChooseFontA, GetOpenFileNameA, GetSaveFileNameA)
//   - IMM32.dll    -> include/micant/tsf.hpp      (ImmSetCompositionFontA)
//   - KERNEL32.dll -> include/micant/kernel32.hpp (DCB, COMMTIMEOUTS, MEMORYSTATUS, OVERLAPPED, serial, sync, pipe, time, temp APIs)
//   - GDI32.dll    -> include/micant/gdi32.hpp    (LOGFONTA, TEXTMETRICA, ABCFLOAT, CHARSETINFO, font & character metric APIs)
//   - USER32.dll   -> include/micant/user32.hpp   (WNDCLASSA, dialog, message, window text, clipboard, keyboard APIs)
//
// Zero exports remain defined in this satellite header (0 registered exports).
// Backward-compatible type aliases and symbol forwarders preserved for test suites.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>
#include <cwchar>
#include <cstring>
#include <algorithm>
#include <unordered_map>
#include <mutex>

#include "ntdef.hpp"
#include "kernel32.hpp"
#include "user32.hpp"
#include "gdi32.hpp"
#include "advapi32.hpp"
#include "comdlg32.hpp"
#include "tsf.hpp"
#include "ldr.hpp"

namespace micant::satellite::putty {

using BOOL = int32_t;
using DWORD = uint32_t;
using HANDLE = void*;
using HWND = void*;
using HDC = void*;
using HGDIOBJ = void*;
using HFONT = void*;
using HMENU = void*;
using HICON = void*;
using HCURSOR = void*;
using HBITMAP = void*;
using HBRUSH = void*;
using HPEN = void*;
using LPARAM = int64_t;
using WPARAM = uint64_t;
using LRESULT = int64_t;
using SIZE_T = size_t;
using UINT = uint32_t;
using LONG = int32_t;
using BYTE = uint8_t;
using WORD = uint16_t;
using ULONG_PTR = uintptr_t;
using LONG_PTR = intptr_t;
using UINT_PTR = uintptr_t;

// ----------------------------------------------------------------------------
// Type Aliases to Canonical Core Subsystems
// ----------------------------------------------------------------------------

using LOGFONTA = micant::gdi32::LOGFONTA;
using TEXTMETRICA = micant::gdi32::TEXTMETRICA;
using ABCFLOAT = micant::gdi32::ABCFLOAT;
using CHARSETINFO = micant::gdi32::CHARSETINFO;

using DCB = micant::win32::DCB;
using COMMTIMEOUTS = micant::win32::COMMTIMEOUTS;
using MEMORYSTATUS = micant::win32::MEMORYSTATUS;
using OVERLAPPED = micant::win32::OVERLAPPED;

using WNDCLASSA = micant::user32::WNDCLASSA;

// ----------------------------------------------------------------------------
// Helper: ANSI to Wide String Conversion
// ----------------------------------------------------------------------------
inline std::wstring AnsiToWide(const char* str) {
    if (!str) return L"";
    std::wstring result;
    result.reserve(std::strlen(str));
    while (*str) {
        result.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*str++)));
    }
    return result;
}

// ----------------------------------------------------------------------------
// Function Forwarders to Canonical Core Subsystems
// ----------------------------------------------------------------------------

// 1. ADVAPI32.dll
using micant::advapi32::CopySid;
using micant::advapi32::GetUserNameA;
using micant::advapi32::RegDeleteKeyA;
using micant::advapi32::RegEnumKeyA;

// 2. COMDLG32.dll
using micant::comdlg32::ChooseColorA;
using micant::comdlg32::ChooseFontA;
using micant::comdlg32::GetOpenFileNameA;
using micant::comdlg32::GetSaveFileNameA;

// 3. IMM32.dll
using micant::tsf::ImmSetCompositionFontA;

// 4. KERNEL32.dll
using micant::win32::Beep;
using micant::win32::ClearCommBreak;
using micant::win32::SetCommBreak;
using micant::win32::GetCommState;
using micant::win32::SetCommState;
using micant::win32::SetCommTimeouts;
using micant::win32::SetHandleInformation;
using micant::win32::CreateEventA;
using micant::win32::CreateMutexA;
using micant::win32::CreateFileMappingA;
using micant::win32::CreateNamedPipeA;
using micant::win32::WaitNamedPipeA;
using micant::win32::CreatePipe;
using micant::win32::FindResourceA;
using micant::win32::GetOverlappedResult;
using micant::win32::GetSystemDirectoryA;
using micant::win32::GetWindowsDirectoryA;
using micant::win32::GetTempPathA;
using micant::win32::GetThreadTimes;
using micant::win32::GlobalMemoryStatus;
using micant::win32::LocalFileTimeToFileTime;

// 5. GDI32.dll
using micant::gdi32::CreateFontA;
using micant::gdi32::CreateFontIndirectA;
using micant::gdi32::GetCharABCWidthsFloatA;
using micant::gdi32::GetCharWidth32A;
using micant::gdi32::GetCharWidth32W;
using micant::gdi32::GetCharWidthA;
using micant::gdi32::GetCharWidthW;
using micant::gdi32::GetCharacterPlacementW;
using micant::gdi32::GetObjectA;
using micant::gdi32::GetOutlineTextMetricsA;
using micant::gdi32::GetTextExtentPointA;
using micant::gdi32::GetTextMetricsA;
using micant::gdi32::TranslateCharsetInfo;
using micant::gdi32::UpdateColors;

// 6. USER32.dll
using micant::user32::CreateDialogParamA;
using micant::user32::DefDlgProcA;
using micant::user32::DefWindowProcA;
using micant::user32::DialogBoxParamA;
using micant::user32::FindWindowA;
using micant::user32::FlashWindow;
using micant::user32::GetClipboardOwner;
using micant::user32::GetMessageA;
using micant::user32::GetQueueStatus;
using micant::user32::GetWindowLongPtrA;
using micant::user32::GetWindowTextLengthA;
using micant::user32::GetWindowTextA;
using micant::user32::InsertMenuA;
using micant::user32::LoadCursorA;
using micant::user32::LoadIconA;
using micant::user32::LoadImageA;
using micant::user32::MessageBoxIndirectW;
using micant::user32::PostMessageA;
using micant::user32::RegisterClassA;
using micant::user32::RegisterClipboardFormatA;
using micant::user32::RegisterWindowMessageA;
using micant::user32::SendDlgItemMessageA;
using micant::user32::SetClassLongPtrA;
using micant::user32::SetWindowLongPtrA;
using micant::user32::SetWindowTextA;
using micant::user32::ToAsciiEx;

// ============================================================================
// Master Registration Function (0 satellite exports registered)
// All 70 PuTTY Win32 exports are registered directly by canonical Core NT
// initializers:
//   - micant::advapi32::InitializeAdvapi32SubsystemExports()
//   - micant::comdlg32::InitializeComDlg32SubsystemExports()
//   - micant::tsf::InitializeTextServicesExports()
//   - micant::win32::InitializeWin32SubsystemExports()
//   - micant::gdi32::InitializeGdi32SubsystemExports()
//   - micant::user32::InitializeUser32SubsystemExports()
// ============================================================================

template <typename TLoader>
inline void registerPuTTYExports([[maybe_unused]] TLoader& ldr) {
    // 0 satellite exports registered - all exports graduated to Core MicaNT
}

inline void InitializePuTTYWin32Exports() {
    // 0 satellite exports registered - all exports graduated to Core MicaNT
}

} // namespace micant::satellite::putty
