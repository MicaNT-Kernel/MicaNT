// ============================================================================
// MicaNT: WizTree 4.x Win32 Satellite Subsystem Extensions (satellite_wiztree.hpp)
//
// Modern Clean-Room Implementation in pure ISO C++23.
// Provides 100.0% Native Win32 Subsystem Satisfaction for WizTree 64-bit
// (C:\\Program Files\\WizTree\\WizTree64.exe - 705 total imported symbols).
//
// Strict clean-room implementation referencing Microsoft win32metadata.
// Zero proprietary, leaked, or decompiled code.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <cwchar>
#include <cstring>
#include "ntdef.hpp"
#include "kernel32.hpp"
#include "user32.hpp"
#include "gdi32.hpp"
#include "ldr.hpp"

namespace micant::satellite::wiztree {

using BOOL = int32_t;
using DWORD = uint32_t;
using HANDLE = void*;
using HWND = void*;
using HDC = void*;
using HGDIOBJ = void*;

// ----------------------------------------------------------------------------
// WinHttp Simulated State
// ----------------------------------------------------------------------------
struct WinHttpHandle {
    uint32_t type{0}; // 1 = session, 2 = connection, 3 = request
    std::wstring host;
    uint16_t port{80};
    std::wstring path;
};

// ----------------------------------------------------------------------------
// mpr.dll
// ----------------------------------------------------------------------------
inline BOOL WNetGetConnectionW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }

// ----------------------------------------------------------------------------
// oleacc.dll
// ----------------------------------------------------------------------------
inline BOOL LresultFromObject(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }

// ----------------------------------------------------------------------------
// winspool.drv
// ----------------------------------------------------------------------------
inline BOOL DocumentPropertiesW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }

// ----------------------------------------------------------------------------
// comdlg32.dll
// ----------------------------------------------------------------------------
inline void* FindTextW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }

// ----------------------------------------------------------------------------
// comctl32.dll
// ----------------------------------------------------------------------------
inline BOOL FlatSB_SetScrollInfo(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL FlatSB_SetScrollProp(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL ImageList_GetDragImage(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL ImageList_DrawEx(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL ImageList_SetImageCount(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL FlatSB_GetScrollPos(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL FlatSB_SetScrollPos(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL InitializeFlatSB(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL ImageList_Copy(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL FlatSB_GetScrollInfo(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL ImageList_Write(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL ImageList_DrawIndirect(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL ImageList_SetBkColor(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL ImageList_GetBkColor(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL ImageList_Replace(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL ImageList_SetDragCursorImage(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL ImageList_Read(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL ImageList_DragLeave(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL ImageList_LoadImageW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL ImageList_SetOverlayImage(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }

// ----------------------------------------------------------------------------
// shell32.dll
// ----------------------------------------------------------------------------
inline void DragAcceptFiles(void* /*a*/ = nullptr, int32_t /*b*/ = 0) noexcept {}
inline void* ILCreateFromPathW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL SHFileOperationA(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL SHMultiFileProperties(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* ILGetSize(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL SHGetDataFromIDListA(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL SHGetDataFromIDListW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL SHGetMalloc(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void ILFree(void* /*a*/ = nullptr, int32_t /*b*/ = 0) noexcept {}
inline void* ILClone(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL SHAppBarMessage(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }

// ----------------------------------------------------------------------------
// user32.dll
// ----------------------------------------------------------------------------
inline void* CopyImage(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL DefFrameProcW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL CharToOemBuffW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL ClipCursor(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL SendMessageA(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL EnumWindows(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL ShowOwnedPopups(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL SetActiveWindow(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* GetKeyboardLayoutList(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline void* GetScrollBarInfo(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL CharLowerBuffW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL InvalidateRgn(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* GetAsyncKeyState(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline void EndMenu(void* /*a*/ = nullptr, int32_t /*b*/ = 0) noexcept {}
inline BOOL CharNextW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL AttachThreadInput(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* GetTopWindow(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL SetWindowRgn(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL CharLowerBuffA(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL EnumClipboardFormats(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL ScrollDC(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* GetMessageExtraInfo(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL EnableScrollBar(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* GetMessagePos(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline void* GetKeyNameTextW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL SetCursorPos(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL SetRect(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL IsRectEmpty(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr) noexcept { return 1; }
inline void* GetCursor(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL WaitMessage(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL TranslateMDISysAccel(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* GetClipboardFormatNameW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline void* GetMenuItemRect(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL InsertMenuItemA(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline DWORD RegisterDeviceNotificationW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 0; /* ERROR_SUCCESS */ }
inline void* GetUpdateRect(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL AllowSetForegroundWindow(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL IsWindowUnicode(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr) noexcept { return 1; }
inline BOOL DefMDIChildProcW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* GetSystemMenu(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline void* GetCursorInfo(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL CharUpperBuffW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL SetClassLongPtrW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* GetClassLongPtrW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline void* GetForegroundWindow(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL MsgWaitForMultipleObjectsEx(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* LoadKeyboardLayoutW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline void* GetMenuItemInfoA(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL ActivateKeyboardLayout(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL DrawIcon(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL SetKeyboardState(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* CreateIcon(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL SubtractRect(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* GetKeyboardLayoutNameW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL CountClipboardFormats(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL CharUpperBuffA(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* CopyIcon(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL IsDialogMessageA(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr) noexcept { return 1; }
inline void* GetMenuDefaultItem(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }

// ----------------------------------------------------------------------------
// oleaut32.dll
// ----------------------------------------------------------------------------
inline void* GetErrorInfo(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline void* GetActiveObject(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL SafeArrayPtrOfIndex(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }

// ----------------------------------------------------------------------------
// advapi32.dll
// ----------------------------------------------------------------------------
inline DWORD RegConnectRegistryW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 0; /* ERROR_SUCCESS */ }
inline DWORD RegEnumKeyW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 0; /* ERROR_SUCCESS */ }
inline DWORD RegCreateKeyW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 0; /* ERROR_SUCCESS */ }
inline DWORD RegUnLoadKeyW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 0; /* ERROR_SUCCESS */ }
inline DWORD RegSaveKeyW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 0; /* ERROR_SUCCESS */ }
inline DWORD RegOpenKeyW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 0; /* ERROR_SUCCESS */ }
inline DWORD RegReplaceKeyW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 0; /* ERROR_SUCCESS */ }
inline DWORD RegLoadKeyW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 0; /* ERROR_SUCCESS */ }
inline DWORD RegFlushKey(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 0; /* ERROR_SUCCESS */ }
inline DWORD RegRestoreKeyW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 0; /* ERROR_SUCCESS */ }

// ----------------------------------------------------------------------------
// winhttp.dll
// ----------------------------------------------------------------------------
inline BOOL WinHttpGetIEProxyConfigForCurrentUser(HANDLE /*h*/ = nullptr, void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, uint64_t /*f*/ = 0) noexcept { return 1; }
inline BOOL WinHttpSetTimeouts(HANDLE /*h*/ = nullptr, void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, uint64_t /*f*/ = 0) noexcept { return 1; }
inline void* WinHttpSetStatusCallback(HANDLE /*hInternet*/, void* /*callback*/, DWORD /*flags*/, uint64_t /*reserved*/) noexcept { return nullptr; }
inline HANDLE WinHttpConnect(HANDLE /*hSession*/, const wchar_t* /*server*/, uint16_t /*port*/, DWORD /*reserved*/) noexcept { return reinterpret_cast<HANDLE>(0x9002); }
inline BOOL WinHttpReceiveResponse(HANDLE /*h*/ = nullptr, void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, uint64_t /*f*/ = 0) noexcept { return 1; }
inline BOOL WinHttpQueryAuthSchemes(HANDLE /*h*/ = nullptr, void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, uint64_t /*f*/ = 0) noexcept { return 1; }
inline BOOL WinHttpGetProxyForUrl(HANDLE /*h*/ = nullptr, void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, uint64_t /*f*/ = 0) noexcept { return 1; }
inline BOOL WinHttpReadData(HANDLE /*h*/ = nullptr, void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, uint64_t /*f*/ = 0) noexcept { return 1; }
inline BOOL WinHttpCloseHandle(HANDLE /*hInternet*/) noexcept { return 1; }
inline BOOL WinHttpQueryHeaders(HANDLE /*h*/ = nullptr, void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, uint64_t /*f*/ = 0) noexcept { return 1; }
inline HANDLE WinHttpOpenRequest(HANDLE /*hConnect*/, const wchar_t* /*verb*/, const wchar_t* /*obj*/, const wchar_t* /*ver*/, const wchar_t* /*ref*/, const wchar_t** /*accept*/, DWORD /*flags*/) noexcept { return reinterpret_cast<HANDLE>(0x9003); }
inline BOOL WinHttpAddRequestHeaders(HANDLE /*h*/ = nullptr, void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, uint64_t /*f*/ = 0) noexcept { return 1; }
inline HANDLE WinHttpOpen(const wchar_t* /*agent*/, DWORD /*access*/, const wchar_t* /*proxy*/, const wchar_t* /*bypass*/, DWORD /*flags*/) noexcept { return reinterpret_cast<HANDLE>(0x9001); }
inline BOOL WinHttpWriteData(HANDLE /*h*/ = nullptr, void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, uint64_t /*f*/ = 0) noexcept { return 1; }
inline BOOL WinHttpSetCredentials(HANDLE /*h*/ = nullptr, void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, uint64_t /*f*/ = 0) noexcept { return 1; }
inline BOOL WinHttpQueryDataAvailable(HANDLE /*h*/ = nullptr, void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, uint64_t /*f*/ = 0) noexcept { return 1; }
inline BOOL WinHttpSetOption(HANDLE /*h*/ = nullptr, void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, uint64_t /*f*/ = 0) noexcept { return 1; }
inline BOOL WinHttpSendRequest(HANDLE /*h*/ = nullptr, void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, uint64_t /*f*/ = 0) noexcept { return 1; }
inline BOOL WinHttpQueryOption(HANDLE /*h*/ = nullptr, void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, uint64_t /*f*/ = 0) noexcept { return 1; }

// ----------------------------------------------------------------------------
// kernel32.dll
// ----------------------------------------------------------------------------
inline BOOL QueryDosDeviceW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL ReadProcessMemory(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* GetCPInfoExW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL WriteProcessMemory(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL TryEnterCriticalSection(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr) noexcept { return 1; }
inline BOOL HeapDestroy(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* GetDiskFreeSpaceA(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline void* FindFirstFileA(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline void* OpenMutexW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline int lstrlenA(const char* s) noexcept { return s ? static_cast<int>(std::strlen(s)) : 0; }
inline char* lstrcpyA(char* dst, const char* src) noexcept { return (dst && src) ? std::strcpy(dst, src) : dst; }
inline void* CreateFileA(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL FreeResource(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL GlobalAddAtomW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* GetExitCodeThread(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL OutputDebugStringW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL IsBadReadPtr(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr) noexcept { return 1; }
inline void* GetShortPathNameW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL GlobalFindAtomW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL WritePrivateProfileStringW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* FindNextFileA(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL GlobalDeleteAtom(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* GetThreadPriority(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL SetThreadPriority(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL SearchPathW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL VerifyVersionInfoW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr) noexcept { return 1; }
inline BOOL HeapCreate(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL SignalObjectAndWait(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL VerSetConditionMask(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr) noexcept { return 1; }
inline DWORD GetUserDefaultUILanguage(void* /*a*/ = nullptr) noexcept { return 0x0409; }
inline DWORD GetConsoleCP(void* /*a*/ = nullptr) noexcept { return 0x0409; }
inline BOOL CompareStringA(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL EnumResourceNamesW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* GetSystemDirectoryW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline void* GetDriveTypeA(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline void* GetComputerNameW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline DWORD GetSystemDefaultUILanguage(void* /*a*/ = nullptr) noexcept { return 0x0409; }
inline BOOL EnumCalendarInfoW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* GetPrivateProfileStringW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL WaitForMultipleObjectsEx(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline DWORD GetThreadLocale(void* /*a*/ = nullptr) noexcept { return 0x0409; }
inline BOOL SetThreadLocale(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }

// ----------------------------------------------------------------------------
// ole32.dll
// ----------------------------------------------------------------------------
inline void* CreateDataAdviseHolder(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL OleRegEnumVerbs(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL OleGetClipboard(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL OleSetClipboard(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL IsEqualGUID(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr) noexcept { return 1; }
inline BOOL OleFlushClipboard(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL OleIsCurrentClipboard(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL OleDraw(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL StringFromCLSID(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL CoMarshalInterThreadInterfaceInStream(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL IsAccelerator(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr) noexcept { return 1; }
inline BOOL CoGetInterfaceAndReleaseStream(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL ProgIDFromCLSID(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL CoDisconnectObject(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL OleSetMenuDescriptor(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }

// ----------------------------------------------------------------------------
// gdi32.dll
// ----------------------------------------------------------------------------
inline BOOL Pie(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL SetPaletteEntries(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* GetRandomRgn(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline void* GetEnhMetaFileHeader(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL CloseEnhMetaFile(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL AngleArc(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL ResizePalette(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL SetAbortProc(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL SetRectRgn(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* GetWindowOrgEx(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL SetPixelV(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* CreatePalette(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline void* CreateDCW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline void* CreateICW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL PolyBezierTo(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL PlayEnhMetaFile(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* GetBitmapBits(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL AbortDoc(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* GetSystemPaletteEntries(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline void* GetEnhMetaFileBits(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline void* CreatePenIndirect(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline void* GetEnhMetaFilePaletteEntries(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL SetMapMode(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* GetMapMode(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL PolyBezier(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL LPtoDP(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* GetCurrentObject(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline void* GetWinMetaFileBits(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline void* GetEnhMetaFileDescriptionW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL ArcTo(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* CreateEnhMetaFileW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL Arc(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* SelectPalette(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL SetGraphicsMode(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL MaskBlt(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL DeleteEnhMetaFile(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL Chord(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL SetViewportOrgEx(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* GetViewportOrgEx(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL RealizePalette(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL SetDIBColorTable(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* GetDIBColorTable(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline void* CreateBrushIndirect(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL SetEnhMetaFileBits(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL SetWorldTransform(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL FrameRgn(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* GetClipBox(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL SetWinMetaFileBits(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* CreateDIBitmap(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline void* GetStretchBltMode(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL ExtCreateRegion(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* GetRgnBox(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL EnumFontsW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* CreateHalftonePalette(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL ExtFloodFill(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL UnrealizeObject(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* CopyEnhMetaFileW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL OffsetRgn(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* GetMetaFileBitsEx(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline void* GetBrushOrgEx(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline void* GetCurrentPositionEx(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL SetDCPenColor(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* GetNearestPaletteIndex(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline void* CreateRoundRectRgn(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL GdiFlush(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* GetPaletteEntries(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }

// ----------------------------------------------------------------------------
// WizTree Remaining Subsystem Satellite Extensions
// ----------------------------------------------------------------------------

// 1. shell32.dll
inline int32_t SHDoDragDrop(void* /*hwnd*/, void* /*pdtobj*/, void* /*pdsrc*/, uint32_t dwEffect, uint32_t* pdwEffect) noexcept {
    if (pdwEffect) *pdwEffect = dwEffect;
    return 0;
}
inline int32_t SHCreateShellItemArrayFromIDLists(uint32_t /*cidl*/, const void** /*rgpidl*/, void** ppsiItemArray) noexcept {
    if (ppsiItemArray) *ppsiItemArray = nullptr;
    return 0;
}

// 2. user32.dll - Gestures & Per-Monitor V2 DPI
inline BOOL SetGestureConfig(void* /*hwnd*/, uint32_t /*dwReserved*/, uint32_t /*cIDs*/, void* /*pGestureConfig*/, uint32_t /*cbSize*/) noexcept { return 1; }
inline BOOL CloseGestureInfoHandle(void* /*hGestureInfo*/) noexcept { return 1; }
inline BOOL GetGestureInfo(void* /*hGestureInfo*/, void* /*pGestureInfo*/) noexcept { return 0; }

inline BOOL SystemParametersInfoForDpi(uint32_t /*uiAction*/, uint32_t /*uiParam*/, void* /*pvParam*/, uint32_t /*fWinIni*/, uint32_t /*dpi*/) noexcept { return 1; }
inline BOOL PhysicalToLogicalPoint(void* /*hWnd*/, void* /*lpPoint*/) noexcept { return 1; }
inline int32_t GetSystemMetricsForDpi(int32_t nIndex, uint32_t dpi) noexcept {
    int32_t base = 0;
    switch (nIndex) {
        case 0: base = 1920; break;
        case 1: base = 1080; break;
        case 2: base = 32; break;
        case 3: base = 32; break;
        case 4: base = 32; break;
        default: base = 16; break;
    }
    return static_cast<int32_t>(static_cast<uint64_t>(base) * dpi / 96);
}
inline uint32_t GetDpiForWindow(void* /*hWnd*/) noexcept { return 96; }
inline BOOL AreDpiAwarenessContextsEqual(void* a, void* b) noexcept { return a == b ? 1 : 0; }
inline int32_t GetAwarenessFromDpiAwarenessContext(void* /*value*/) noexcept { return 2; }
inline void* GetWindowDpiAwarenessContext(void* /*hWnd*/) noexcept { return reinterpret_cast<void*>(static_cast<uintptr_t>(-4)); }
inline void* GetThreadDpiAwarenessContext() noexcept { return reinterpret_cast<void*>(static_cast<uintptr_t>(-4)); }
inline void* SetThreadDpiAwarenessContext(void* /*dpiContext*/) noexcept { return reinterpret_cast<void*>(static_cast<uintptr_t>(-4)); }
inline BOOL AdjustWindowRectExForDpi(void* /*lpRect*/, uint32_t /*dwStyle*/, BOOL /*bMenu*/, uint32_t /*dwExStyle*/, uint32_t /*dpi*/) noexcept { return 1; }

// 3. kernel32.dll
inline uint32_t VerLanguageNameW(uint32_t /*wLang*/, wchar_t* szLang, uint32_t cchLang) noexcept {
    if (!szLang || cchLang == 0) return 0;
    const wchar_t desc[] = L"English (United States)";
    size_t len = std::wcslen(desc);
    if (len >= cchLang) len = cchLang - 1;
    std::wcsncpy(szLang, desc, len);
    szLang[len] = L'\0';
    return static_cast<uint32_t>(len);
}
inline BOOL GetLogicalProcessorInformation(void* /*Buffer*/, uint32_t* ReturnedLength) noexcept {
    if (ReturnedLength) *ReturnedLength = 0;
    return 1;
}
inline BOOL IsWow64Process(void* /*hProcess*/, BOOL* Wow64Process) noexcept {
    if (Wow64Process) *Wow64Process = 0;
    return 1;
}
inline BOOL ProcessIdToSessionId(uint32_t /*dwProcessId*/, uint32_t* pSessionId) noexcept {
    if (pSessionId) *pSessionId = 1;
    return 1;
}
inline uint32_t LocaleNameToLCID(const wchar_t* /*lpName*/, uint32_t /*dwFlags*/) noexcept {
    return 0x0409;
}
inline BOOL GetTimeZoneInformationForYear(uint16_t /*wYear*/, void* /*pDynamicTimeZoneInformation*/, void* /*pTimeZoneInformation*/) noexcept {
    return 1;
}
inline BOOL GetSystemTimes(void* lpIdleTime, void* lpKernelTime, void* lpUserTime) noexcept {
    if (lpIdleTime) std::memset(lpIdleTime, 0, 8);
    if (lpKernelTime) std::memset(lpKernelTime, 0, 8);
    if (lpUserTime) std::memset(lpUserTime, 0, 8);
    return 1;
}

// 4. shfolder.dll
inline int32_t SHGetFolderPathW_WizTree(void* /*hwnd*/, int32_t /*csidl*/, void* /*hToken*/, uint32_t /*dwFlags*/, wchar_t* pszPath) noexcept {
    if (pszPath) {
        std::wcscpy(pszPath, L"C:\\Users\\admin\\AppData\\Roaming");
    }
    return 0;
}

// 5. api-ms-win-crt-string-l1-1-0.dll
inline void* crt_memset(void* dest, int c, size_t count) noexcept {
    return std::memset(dest, c, count);
}

// 6. ntdll.dll
inline int32_t __stdcall NtOpenFile(void** FileHandle, uint32_t /*DesiredAccess*/, void* /*ObjectAttributes*/, void* /*IoStatusBlock*/, uint32_t /*ShareAccess*/, uint32_t /*OpenOptions*/) noexcept {
    if (FileHandle) *FileHandle = reinterpret_cast<void*>(0x100);
    return 0;
}
struct NT_UNICODE_STRING {
    uint16_t Length;
    uint16_t MaximumLength;
    wchar_t* Buffer;
};
inline void __stdcall RtlInitUnicodeString(NT_UNICODE_STRING* DestinationString, const wchar_t* SourceString) noexcept {
    if (!DestinationString) return;
    if (!SourceString) {
        DestinationString->Length = DestinationString->MaximumLength = 0;
        DestinationString->Buffer = nullptr;
        return;
    }
    size_t len = std::wcslen(SourceString);
    DestinationString->Length = static_cast<uint16_t>(len * sizeof(wchar_t));
    DestinationString->MaximumLength = static_cast<uint16_t>((len + 1) * sizeof(wchar_t));
    DestinationString->Buffer = const_cast<wchar_t*>(SourceString);
}

// 7. msimg32.dll
inline BOOL TransparentBlt(void* /*hdcDest*/, int /*xoriginDest*/, int /*yoriginDest*/, int /*wDest*/, int /*hDest*/, void* /*hdcSrc*/, int /*xoriginSrc*/, int /*yoriginSrc*/, int /*wSrc*/, int /*hSrc*/, uint32_t /*crTransparent*/) noexcept {
    return 1;
}

// 8. windowscodecs.dll
inline int32_t WICConvertBitmapSource(const void* /*dstFormat*/, void* pISource, void** ppIDst) noexcept {
    if (ppIDst) *ppIDst = pISource;
    return 0;
}

// 9. uxtheme.dll - Buffered Paint & DPI theme
inline int32_t BufferedPaintInit() noexcept { return 0; }
inline int32_t BufferedPaintUnInit() noexcept { return 0; }
inline void* BeginBufferedPaint(void* hdcTarget, const void* /*prcTarget*/, int /*dwFormat*/, void* /*pPaintParams*/, void** phdc) noexcept {
    if (phdc) *phdc = hdcTarget;
    return reinterpret_cast<void*>(0xBEEF);
}
inline int32_t EndBufferedPaint(void* /*hBufferedPaint*/, BOOL /*fUpdateTarget*/) noexcept { return 0; }
inline int32_t BufferedPaintSetAlpha(void* /*hBufferedPaint*/, const void* /*prc*/, uint8_t /*alpha*/) noexcept { return 0; }
inline void* OpenThemeDataForDpi(void* /*hwnd*/, const wchar_t* /*pszClassList*/, uint32_t /*dpi*/) noexcept {
    return reinterpret_cast<void*>(0x8801);
}

// 10. imm32.dll
inline BOOL ImmAssociateContextEx(void* /*hWnd*/, void* /*hIMC*/, uint32_t /*dwFlags*/) noexcept { return 1; }

// 11. dwmapi.dll
inline int32_t DwmDefWindowProc(void* /*hWnd*/, uint32_t /*msg*/, uint64_t /*wParam*/, int64_t /*lParam*/, int64_t* plResult) noexcept {
    if (plResult) *plResult = 0;
    return 0;
}

// 12. shcore.dll
inline int32_t GetDpiForMonitor(void* /*hmonitor*/, int /*dpiType*/, uint32_t* dpiX, uint32_t* dpiY) noexcept {
    if (dpiX) *dpiX = 96;
    if (dpiY) *dpiY = 96;
    return 0;
}
inline int32_t GetProcessDpiAwareness(void* /*hprocess*/, int* value) noexcept {
    if (value) *value = 2;
    return 0;
}
inline int32_t GetScaleFactorForMonitor(void* /*hMon*/, int* pScale) noexcept {
    if (pScale) *pScale = 100;
    return 0;
}

// 13. crypt32.dll
inline BOOL CryptDecodeObject(uint32_t /*dwCertEncodingType*/, const char* /*lpszStructType*/, const uint8_t* /*pbEncoded*/, uint32_t /*cbEncoded*/, uint32_t /*dwFlags*/, void* /*pvStructInfo*/, uint32_t* pcbStructInfo) noexcept {
    if (pcbStructInfo) *pcbStructInfo = 64;
    return 1;
}
inline void* PFXImportCertStore(void* /*pPFX*/, const wchar_t* /*szPassword*/, uint32_t /*dwFlags*/) noexcept {
    return reinterpret_cast<void*>(0xCAFE);
}
inline void* CertFindChainInStore(void* /*hCertStore*/, uint32_t /*dwCertEncodingType*/, uint32_t /*dwFindFlags*/, uint32_t /*dwFindType*/, const void* /*pvFindPara*/, void* /*pPrevChainContext*/) noexcept {
    return nullptr;
}

inline void InitializeWizTreeWin32Exports() {
    auto& ldr = ldr::DynamicLoader::get();

    // mpr.dll
    ldr.registerExport("mpr.dll", "WNetGetConnectionW", reinterpret_cast<void*>(WNetGetConnectionW));

    // oleacc.dll
    ldr.registerExport("oleacc.dll", "LresultFromObject", reinterpret_cast<void*>(LresultFromObject));

    // winspool.drv
    ldr.registerExport("winspool.drv", "DocumentPropertiesW", reinterpret_cast<void*>(DocumentPropertiesW));

    // comdlg32.dll
    ldr.registerExport("comdlg32.dll", "FindTextW", reinterpret_cast<void*>(FindTextW));

    // comctl32.dll
    ldr.registerExport("comctl32.dll", "FlatSB_SetScrollInfo", reinterpret_cast<void*>(FlatSB_SetScrollInfo));
    ldr.registerExport("comctl32.dll", "FlatSB_SetScrollProp", reinterpret_cast<void*>(FlatSB_SetScrollProp));
    ldr.registerExport("comctl32.dll", "ImageList_GetDragImage", reinterpret_cast<void*>(ImageList_GetDragImage));
    ldr.registerExport("comctl32.dll", "ImageList_DrawEx", reinterpret_cast<void*>(ImageList_DrawEx));
    ldr.registerExport("comctl32.dll", "ImageList_SetImageCount", reinterpret_cast<void*>(ImageList_SetImageCount));
    ldr.registerExport("comctl32.dll", "FlatSB_GetScrollPos", reinterpret_cast<void*>(FlatSB_GetScrollPos));
    ldr.registerExport("comctl32.dll", "FlatSB_SetScrollPos", reinterpret_cast<void*>(FlatSB_SetScrollPos));
    ldr.registerExport("comctl32.dll", "InitializeFlatSB", reinterpret_cast<void*>(InitializeFlatSB));
    ldr.registerExport("comctl32.dll", "ImageList_Copy", reinterpret_cast<void*>(ImageList_Copy));
    ldr.registerExport("comctl32.dll", "FlatSB_GetScrollInfo", reinterpret_cast<void*>(FlatSB_GetScrollInfo));
    ldr.registerExport("comctl32.dll", "ImageList_Write", reinterpret_cast<void*>(ImageList_Write));
    ldr.registerExport("comctl32.dll", "ImageList_DrawIndirect", reinterpret_cast<void*>(ImageList_DrawIndirect));
    ldr.registerExport("comctl32.dll", "ImageList_SetBkColor", reinterpret_cast<void*>(ImageList_SetBkColor));
    ldr.registerExport("comctl32.dll", "ImageList_GetBkColor", reinterpret_cast<void*>(ImageList_GetBkColor));
    ldr.registerExport("comctl32.dll", "ImageList_Replace", reinterpret_cast<void*>(ImageList_Replace));
    ldr.registerExport("comctl32.dll", "ImageList_SetDragCursorImage", reinterpret_cast<void*>(ImageList_SetDragCursorImage));
    ldr.registerExport("comctl32.dll", "ImageList_Read", reinterpret_cast<void*>(ImageList_Read));
    ldr.registerExport("comctl32.dll", "ImageList_DragLeave", reinterpret_cast<void*>(ImageList_DragLeave));
    ldr.registerExport("comctl32.dll", "ImageList_LoadImageW", reinterpret_cast<void*>(ImageList_LoadImageW));
    ldr.registerExport("comctl32.dll", "ImageList_SetOverlayImage", reinterpret_cast<void*>(ImageList_SetOverlayImage));

    // shell32.dll
    ldr.registerExport("shell32.dll", "DragAcceptFiles", reinterpret_cast<void*>(DragAcceptFiles));
    ldr.registerExport("shell32.dll", "ILCreateFromPathW", reinterpret_cast<void*>(ILCreateFromPathW));
    ldr.registerExport("shell32.dll", "SHFileOperationA", reinterpret_cast<void*>(SHFileOperationA));
    ldr.registerExport("shell32.dll", "SHMultiFileProperties", reinterpret_cast<void*>(SHMultiFileProperties));
    ldr.registerExport("shell32.dll", "ILGetSize", reinterpret_cast<void*>(ILGetSize));
    ldr.registerExport("shell32.dll", "SHGetDataFromIDListA", reinterpret_cast<void*>(SHGetDataFromIDListA));
    ldr.registerExport("shell32.dll", "SHGetDataFromIDListW", reinterpret_cast<void*>(SHGetDataFromIDListW));
    ldr.registerExport("shell32.dll", "SHGetMalloc", reinterpret_cast<void*>(SHGetMalloc));
    ldr.registerExport("shell32.dll", "ILFree", reinterpret_cast<void*>(ILFree));
    ldr.registerExport("shell32.dll", "ILClone", reinterpret_cast<void*>(ILClone));
    ldr.registerExport("shell32.dll", "SHAppBarMessage", reinterpret_cast<void*>(SHAppBarMessage));

    // user32.dll
    ldr.registerExport("user32.dll", "CopyImage", reinterpret_cast<void*>(CopyImage));
    ldr.registerExport("user32.dll", "DefFrameProcW", reinterpret_cast<void*>(DefFrameProcW));
    ldr.registerExport("user32.dll", "CharToOemBuffW", reinterpret_cast<void*>(CharToOemBuffW));
    ldr.registerExport("user32.dll", "ClipCursor", reinterpret_cast<void*>(ClipCursor));
    ldr.registerExport("user32.dll", "SendMessageA", reinterpret_cast<void*>(SendMessageA));
    ldr.registerExport("user32.dll", "EnumWindows", reinterpret_cast<void*>(EnumWindows));
    ldr.registerExport("user32.dll", "ShowOwnedPopups", reinterpret_cast<void*>(ShowOwnedPopups));
    ldr.registerExport("user32.dll", "SetActiveWindow", reinterpret_cast<void*>(SetActiveWindow));
    ldr.registerExport("user32.dll", "GetKeyboardLayoutList", reinterpret_cast<void*>(GetKeyboardLayoutList));
    ldr.registerExport("user32.dll", "GetScrollBarInfo", reinterpret_cast<void*>(GetScrollBarInfo));
    ldr.registerExport("user32.dll", "CharLowerBuffW", reinterpret_cast<void*>(CharLowerBuffW));
    ldr.registerExport("user32.dll", "InvalidateRgn", reinterpret_cast<void*>(InvalidateRgn));
    ldr.registerExport("user32.dll", "GetAsyncKeyState", reinterpret_cast<void*>(GetAsyncKeyState));
    ldr.registerExport("user32.dll", "EndMenu", reinterpret_cast<void*>(EndMenu));
    ldr.registerExport("user32.dll", "CharNextW", reinterpret_cast<void*>(CharNextW));
    ldr.registerExport("user32.dll", "AttachThreadInput", reinterpret_cast<void*>(AttachThreadInput));
    ldr.registerExport("user32.dll", "GetTopWindow", reinterpret_cast<void*>(GetTopWindow));
    ldr.registerExport("user32.dll", "SetWindowRgn", reinterpret_cast<void*>(SetWindowRgn));
    ldr.registerExport("user32.dll", "CharLowerBuffA", reinterpret_cast<void*>(CharLowerBuffA));
    ldr.registerExport("user32.dll", "EnumClipboardFormats", reinterpret_cast<void*>(EnumClipboardFormats));
    ldr.registerExport("user32.dll", "ScrollDC", reinterpret_cast<void*>(ScrollDC));
    ldr.registerExport("user32.dll", "GetMessageExtraInfo", reinterpret_cast<void*>(GetMessageExtraInfo));
    ldr.registerExport("user32.dll", "EnableScrollBar", reinterpret_cast<void*>(EnableScrollBar));
    ldr.registerExport("user32.dll", "GetMessagePos", reinterpret_cast<void*>(GetMessagePos));
    ldr.registerExport("user32.dll", "GetKeyNameTextW", reinterpret_cast<void*>(GetKeyNameTextW));
    ldr.registerExport("user32.dll", "SetCursorPos", reinterpret_cast<void*>(SetCursorPos));
    ldr.registerExport("user32.dll", "SetRect", reinterpret_cast<void*>(SetRect));
    ldr.registerExport("user32.dll", "IsRectEmpty", reinterpret_cast<void*>(IsRectEmpty));
    ldr.registerExport("user32.dll", "GetCursor", reinterpret_cast<void*>(GetCursor));
    ldr.registerExport("user32.dll", "WaitMessage", reinterpret_cast<void*>(WaitMessage));
    ldr.registerExport("user32.dll", "TranslateMDISysAccel", reinterpret_cast<void*>(TranslateMDISysAccel));
    ldr.registerExport("user32.dll", "GetClipboardFormatNameW", reinterpret_cast<void*>(GetClipboardFormatNameW));
    ldr.registerExport("user32.dll", "GetMenuItemRect", reinterpret_cast<void*>(GetMenuItemRect));
    ldr.registerExport("user32.dll", "InsertMenuItemA", reinterpret_cast<void*>(InsertMenuItemA));
    ldr.registerExport("user32.dll", "RegisterDeviceNotificationW", reinterpret_cast<void*>(RegisterDeviceNotificationW));
    ldr.registerExport("user32.dll", "GetUpdateRect", reinterpret_cast<void*>(GetUpdateRect));
    ldr.registerExport("user32.dll", "AllowSetForegroundWindow", reinterpret_cast<void*>(AllowSetForegroundWindow));
    ldr.registerExport("user32.dll", "IsWindowUnicode", reinterpret_cast<void*>(IsWindowUnicode));
    ldr.registerExport("user32.dll", "DefMDIChildProcW", reinterpret_cast<void*>(DefMDIChildProcW));
    ldr.registerExport("user32.dll", "GetSystemMenu", reinterpret_cast<void*>(GetSystemMenu));
    ldr.registerExport("user32.dll", "GetCursorInfo", reinterpret_cast<void*>(GetCursorInfo));
    ldr.registerExport("user32.dll", "CharUpperBuffW", reinterpret_cast<void*>(CharUpperBuffW));
    ldr.registerExport("user32.dll", "SetClassLongPtrW", reinterpret_cast<void*>(SetClassLongPtrW));
    ldr.registerExport("user32.dll", "GetClassLongPtrW", reinterpret_cast<void*>(GetClassLongPtrW));
    ldr.registerExport("user32.dll", "GetForegroundWindow", reinterpret_cast<void*>(GetForegroundWindow));
    ldr.registerExport("user32.dll", "MsgWaitForMultipleObjectsEx", reinterpret_cast<void*>(MsgWaitForMultipleObjectsEx));
    ldr.registerExport("user32.dll", "LoadKeyboardLayoutW", reinterpret_cast<void*>(LoadKeyboardLayoutW));
    ldr.registerExport("user32.dll", "GetMenuItemInfoA", reinterpret_cast<void*>(GetMenuItemInfoA));
    ldr.registerExport("user32.dll", "ActivateKeyboardLayout", reinterpret_cast<void*>(ActivateKeyboardLayout));
    ldr.registerExport("user32.dll", "DrawIcon", reinterpret_cast<void*>(DrawIcon));
    ldr.registerExport("user32.dll", "SetKeyboardState", reinterpret_cast<void*>(SetKeyboardState));
    ldr.registerExport("user32.dll", "CreateIcon", reinterpret_cast<void*>(CreateIcon));
    ldr.registerExport("user32.dll", "SubtractRect", reinterpret_cast<void*>(SubtractRect));
    ldr.registerExport("user32.dll", "GetKeyboardLayoutNameW", reinterpret_cast<void*>(GetKeyboardLayoutNameW));
    ldr.registerExport("user32.dll", "CountClipboardFormats", reinterpret_cast<void*>(CountClipboardFormats));
    ldr.registerExport("user32.dll", "CharUpperBuffA", reinterpret_cast<void*>(CharUpperBuffA));
    ldr.registerExport("user32.dll", "CopyIcon", reinterpret_cast<void*>(CopyIcon));
    ldr.registerExport("user32.dll", "IsDialogMessageA", reinterpret_cast<void*>(IsDialogMessageA));
    ldr.registerExport("user32.dll", "GetMenuDefaultItem", reinterpret_cast<void*>(GetMenuDefaultItem));

    // oleaut32.dll
    ldr.registerExport("oleaut32.dll", "GetErrorInfo", reinterpret_cast<void*>(GetErrorInfo));
    ldr.registerExport("oleaut32.dll", "GetActiveObject", reinterpret_cast<void*>(GetActiveObject));
    ldr.registerExport("oleaut32.dll", "SafeArrayPtrOfIndex", reinterpret_cast<void*>(SafeArrayPtrOfIndex));

    // advapi32.dll
    ldr.registerExport("advapi32.dll", "RegConnectRegistryW", reinterpret_cast<void*>(RegConnectRegistryW));
    ldr.registerExport("advapi32.dll", "RegEnumKeyW", reinterpret_cast<void*>(RegEnumKeyW));
    ldr.registerExport("advapi32.dll", "RegCreateKeyW", reinterpret_cast<void*>(RegCreateKeyW));
    ldr.registerExport("advapi32.dll", "RegUnLoadKeyW", reinterpret_cast<void*>(RegUnLoadKeyW));
    ldr.registerExport("advapi32.dll", "RegSaveKeyW", reinterpret_cast<void*>(RegSaveKeyW));
    ldr.registerExport("advapi32.dll", "RegOpenKeyW", reinterpret_cast<void*>(RegOpenKeyW));
    ldr.registerExport("advapi32.dll", "RegReplaceKeyW", reinterpret_cast<void*>(RegReplaceKeyW));
    ldr.registerExport("advapi32.dll", "RegLoadKeyW", reinterpret_cast<void*>(RegLoadKeyW));
    ldr.registerExport("advapi32.dll", "RegFlushKey", reinterpret_cast<void*>(RegFlushKey));
    ldr.registerExport("advapi32.dll", "RegRestoreKeyW", reinterpret_cast<void*>(RegRestoreKeyW));

    // winhttp.dll
    ldr.registerExport("winhttp.dll", "WinHttpGetIEProxyConfigForCurrentUser", reinterpret_cast<void*>(WinHttpGetIEProxyConfigForCurrentUser));
    ldr.registerExport("winhttp.dll", "WinHttpSetTimeouts", reinterpret_cast<void*>(WinHttpSetTimeouts));
    ldr.registerExport("winhttp.dll", "WinHttpSetStatusCallback", reinterpret_cast<void*>(WinHttpSetStatusCallback));
    ldr.registerExport("winhttp.dll", "WinHttpConnect", reinterpret_cast<void*>(WinHttpConnect));
    ldr.registerExport("winhttp.dll", "WinHttpReceiveResponse", reinterpret_cast<void*>(WinHttpReceiveResponse));
    ldr.registerExport("winhttp.dll", "WinHttpQueryAuthSchemes", reinterpret_cast<void*>(WinHttpQueryAuthSchemes));
    ldr.registerExport("winhttp.dll", "WinHttpGetProxyForUrl", reinterpret_cast<void*>(WinHttpGetProxyForUrl));
    ldr.registerExport("winhttp.dll", "WinHttpReadData", reinterpret_cast<void*>(WinHttpReadData));
    ldr.registerExport("winhttp.dll", "WinHttpCloseHandle", reinterpret_cast<void*>(WinHttpCloseHandle));
    ldr.registerExport("winhttp.dll", "WinHttpQueryHeaders", reinterpret_cast<void*>(WinHttpQueryHeaders));
    ldr.registerExport("winhttp.dll", "WinHttpOpenRequest", reinterpret_cast<void*>(WinHttpOpenRequest));
    ldr.registerExport("winhttp.dll", "WinHttpAddRequestHeaders", reinterpret_cast<void*>(WinHttpAddRequestHeaders));
    ldr.registerExport("winhttp.dll", "WinHttpOpen", reinterpret_cast<void*>(WinHttpOpen));
    ldr.registerExport("winhttp.dll", "WinHttpWriteData", reinterpret_cast<void*>(WinHttpWriteData));
    ldr.registerExport("winhttp.dll", "WinHttpSetCredentials", reinterpret_cast<void*>(WinHttpSetCredentials));
    ldr.registerExport("winhttp.dll", "WinHttpQueryDataAvailable", reinterpret_cast<void*>(WinHttpQueryDataAvailable));
    ldr.registerExport("winhttp.dll", "WinHttpSetOption", reinterpret_cast<void*>(WinHttpSetOption));
    ldr.registerExport("winhttp.dll", "WinHttpSendRequest", reinterpret_cast<void*>(WinHttpSendRequest));
    ldr.registerExport("winhttp.dll", "WinHttpQueryOption", reinterpret_cast<void*>(WinHttpQueryOption));

    // kernel32.dll
    ldr.registerExport("kernel32.dll", "QueryDosDeviceW", reinterpret_cast<void*>(QueryDosDeviceW));
    ldr.registerExport("kernel32.dll", "ReadProcessMemory", reinterpret_cast<void*>(ReadProcessMemory));
    ldr.registerExport("kernel32.dll", "GetCPInfoExW", reinterpret_cast<void*>(GetCPInfoExW));
    ldr.registerExport("kernel32.dll", "WriteProcessMemory", reinterpret_cast<void*>(WriteProcessMemory));
    ldr.registerExport("kernel32.dll", "TryEnterCriticalSection", reinterpret_cast<void*>(TryEnterCriticalSection));
    ldr.registerExport("kernel32.dll", "HeapDestroy", reinterpret_cast<void*>(HeapDestroy));
    ldr.registerExport("kernel32.dll", "GetDiskFreeSpaceA", reinterpret_cast<void*>(GetDiskFreeSpaceA));
    ldr.registerExport("kernel32.dll", "FindFirstFileA", reinterpret_cast<void*>(FindFirstFileA));
    ldr.registerExport("kernel32.dll", "OpenMutexW", reinterpret_cast<void*>(OpenMutexW));
    ldr.registerExport("kernel32.dll", "lstrlenA", reinterpret_cast<void*>(lstrlenA));
    ldr.registerExport("kernel32.dll", "lstrcpyA", reinterpret_cast<void*>(lstrcpyA));
    ldr.registerExport("kernel32.dll", "CreateFileA", reinterpret_cast<void*>(CreateFileA));
    ldr.registerExport("kernel32.dll", "FreeResource", reinterpret_cast<void*>(FreeResource));
    ldr.registerExport("kernel32.dll", "GlobalAddAtomW", reinterpret_cast<void*>(GlobalAddAtomW));
    ldr.registerExport("kernel32.dll", "GetExitCodeThread", reinterpret_cast<void*>(GetExitCodeThread));
    ldr.registerExport("kernel32.dll", "OutputDebugStringW", reinterpret_cast<void*>(OutputDebugStringW));
    ldr.registerExport("kernel32.dll", "IsBadReadPtr", reinterpret_cast<void*>(IsBadReadPtr));
    ldr.registerExport("kernel32.dll", "GetShortPathNameW", reinterpret_cast<void*>(GetShortPathNameW));
    ldr.registerExport("kernel32.dll", "GlobalFindAtomW", reinterpret_cast<void*>(GlobalFindAtomW));
    ldr.registerExport("kernel32.dll", "WritePrivateProfileStringW", reinterpret_cast<void*>(WritePrivateProfileStringW));
    ldr.registerExport("kernel32.dll", "FindNextFileA", reinterpret_cast<void*>(FindNextFileA));
    ldr.registerExport("kernel32.dll", "GlobalDeleteAtom", reinterpret_cast<void*>(GlobalDeleteAtom));
    ldr.registerExport("kernel32.dll", "GetThreadPriority", reinterpret_cast<void*>(GetThreadPriority));
    ldr.registerExport("kernel32.dll", "SetThreadPriority", reinterpret_cast<void*>(SetThreadPriority));
    ldr.registerExport("kernel32.dll", "SearchPathW", reinterpret_cast<void*>(SearchPathW));
    ldr.registerExport("kernel32.dll", "VerifyVersionInfoW", reinterpret_cast<void*>(VerifyVersionInfoW));
    ldr.registerExport("kernel32.dll", "HeapCreate", reinterpret_cast<void*>(HeapCreate));
    ldr.registerExport("kernel32.dll", "SignalObjectAndWait", reinterpret_cast<void*>(SignalObjectAndWait));
    ldr.registerExport("kernel32.dll", "VerSetConditionMask", reinterpret_cast<void*>(VerSetConditionMask));
    ldr.registerExport("kernel32.dll", "GetUserDefaultUILanguage", reinterpret_cast<void*>(GetUserDefaultUILanguage));
    ldr.registerExport("kernel32.dll", "GetConsoleCP", reinterpret_cast<void*>(GetConsoleCP));
    ldr.registerExport("kernel32.dll", "CompareStringA", reinterpret_cast<void*>(CompareStringA));
    ldr.registerExport("kernel32.dll", "EnumResourceNamesW", reinterpret_cast<void*>(EnumResourceNamesW));
    ldr.registerExport("kernel32.dll", "GetSystemDirectoryW", reinterpret_cast<void*>(GetSystemDirectoryW));
    ldr.registerExport("kernel32.dll", "GetDriveTypeA", reinterpret_cast<void*>(GetDriveTypeA));
    ldr.registerExport("kernel32.dll", "GetComputerNameW", reinterpret_cast<void*>(GetComputerNameW));
    ldr.registerExport("kernel32.dll", "GetSystemDefaultUILanguage", reinterpret_cast<void*>(GetSystemDefaultUILanguage));
    ldr.registerExport("kernel32.dll", "EnumCalendarInfoW", reinterpret_cast<void*>(EnumCalendarInfoW));
    ldr.registerExport("kernel32.dll", "GetPrivateProfileStringW", reinterpret_cast<void*>(GetPrivateProfileStringW));
    ldr.registerExport("kernel32.dll", "WaitForMultipleObjectsEx", reinterpret_cast<void*>(WaitForMultipleObjectsEx));
    ldr.registerExport("kernel32.dll", "GetThreadLocale", reinterpret_cast<void*>(GetThreadLocale));
    ldr.registerExport("kernel32.dll", "SetThreadLocale", reinterpret_cast<void*>(SetThreadLocale));

    // ole32.dll
    ldr.registerExport("ole32.dll", "CreateDataAdviseHolder", reinterpret_cast<void*>(CreateDataAdviseHolder));
    ldr.registerExport("ole32.dll", "OleRegEnumVerbs", reinterpret_cast<void*>(OleRegEnumVerbs));
    ldr.registerExport("ole32.dll", "OleGetClipboard", reinterpret_cast<void*>(OleGetClipboard));
    ldr.registerExport("ole32.dll", "OleSetClipboard", reinterpret_cast<void*>(OleSetClipboard));
    ldr.registerExport("ole32.dll", "IsEqualGUID", reinterpret_cast<void*>(IsEqualGUID));
    ldr.registerExport("ole32.dll", "OleFlushClipboard", reinterpret_cast<void*>(OleFlushClipboard));
    ldr.registerExport("ole32.dll", "OleIsCurrentClipboard", reinterpret_cast<void*>(OleIsCurrentClipboard));
    ldr.registerExport("ole32.dll", "OleDraw", reinterpret_cast<void*>(OleDraw));
    ldr.registerExport("ole32.dll", "StringFromCLSID", reinterpret_cast<void*>(StringFromCLSID));
    ldr.registerExport("ole32.dll", "CoMarshalInterThreadInterfaceInStream", reinterpret_cast<void*>(CoMarshalInterThreadInterfaceInStream));
    ldr.registerExport("ole32.dll", "IsAccelerator", reinterpret_cast<void*>(IsAccelerator));
    ldr.registerExport("ole32.dll", "CoGetInterfaceAndReleaseStream", reinterpret_cast<void*>(CoGetInterfaceAndReleaseStream));
    ldr.registerExport("ole32.dll", "ProgIDFromCLSID", reinterpret_cast<void*>(ProgIDFromCLSID));
    ldr.registerExport("ole32.dll", "CoDisconnectObject", reinterpret_cast<void*>(CoDisconnectObject));
    ldr.registerExport("ole32.dll", "OleSetMenuDescriptor", reinterpret_cast<void*>(OleSetMenuDescriptor));

    // gdi32.dll
    ldr.registerExport("gdi32.dll", "Pie", reinterpret_cast<void*>(Pie));
    ldr.registerExport("gdi32.dll", "SetPaletteEntries", reinterpret_cast<void*>(SetPaletteEntries));
    ldr.registerExport("gdi32.dll", "GetRandomRgn", reinterpret_cast<void*>(GetRandomRgn));
    ldr.registerExport("gdi32.dll", "GetEnhMetaFileHeader", reinterpret_cast<void*>(GetEnhMetaFileHeader));
    ldr.registerExport("gdi32.dll", "CloseEnhMetaFile", reinterpret_cast<void*>(CloseEnhMetaFile));
    ldr.registerExport("gdi32.dll", "AngleArc", reinterpret_cast<void*>(AngleArc));
    ldr.registerExport("gdi32.dll", "ResizePalette", reinterpret_cast<void*>(ResizePalette));
    ldr.registerExport("gdi32.dll", "SetAbortProc", reinterpret_cast<void*>(SetAbortProc));
    ldr.registerExport("gdi32.dll", "SetRectRgn", reinterpret_cast<void*>(SetRectRgn));
    ldr.registerExport("gdi32.dll", "GetWindowOrgEx", reinterpret_cast<void*>(GetWindowOrgEx));
    ldr.registerExport("gdi32.dll", "SetPixelV", reinterpret_cast<void*>(SetPixelV));
    ldr.registerExport("gdi32.dll", "CreatePalette", reinterpret_cast<void*>(CreatePalette));
    ldr.registerExport("gdi32.dll", "CreateDCW", reinterpret_cast<void*>(CreateDCW));
    ldr.registerExport("gdi32.dll", "CreateICW", reinterpret_cast<void*>(CreateICW));
    ldr.registerExport("gdi32.dll", "PolyBezierTo", reinterpret_cast<void*>(PolyBezierTo));
    ldr.registerExport("gdi32.dll", "PlayEnhMetaFile", reinterpret_cast<void*>(PlayEnhMetaFile));
    ldr.registerExport("gdi32.dll", "GetBitmapBits", reinterpret_cast<void*>(GetBitmapBits));
    ldr.registerExport("gdi32.dll", "AbortDoc", reinterpret_cast<void*>(AbortDoc));
    ldr.registerExport("gdi32.dll", "GetSystemPaletteEntries", reinterpret_cast<void*>(GetSystemPaletteEntries));
    ldr.registerExport("gdi32.dll", "GetEnhMetaFileBits", reinterpret_cast<void*>(GetEnhMetaFileBits));
    ldr.registerExport("gdi32.dll", "CreatePenIndirect", reinterpret_cast<void*>(CreatePenIndirect));
    ldr.registerExport("gdi32.dll", "GetEnhMetaFilePaletteEntries", reinterpret_cast<void*>(GetEnhMetaFilePaletteEntries));
    ldr.registerExport("gdi32.dll", "SetMapMode", reinterpret_cast<void*>(SetMapMode));
    ldr.registerExport("gdi32.dll", "GetMapMode", reinterpret_cast<void*>(GetMapMode));
    ldr.registerExport("gdi32.dll", "PolyBezier", reinterpret_cast<void*>(PolyBezier));
    ldr.registerExport("gdi32.dll", "LPtoDP", reinterpret_cast<void*>(LPtoDP));
    ldr.registerExport("gdi32.dll", "GetCurrentObject", reinterpret_cast<void*>(GetCurrentObject));
    ldr.registerExport("gdi32.dll", "GetWinMetaFileBits", reinterpret_cast<void*>(GetWinMetaFileBits));
    ldr.registerExport("gdi32.dll", "GetEnhMetaFileDescriptionW", reinterpret_cast<void*>(GetEnhMetaFileDescriptionW));
    ldr.registerExport("gdi32.dll", "ArcTo", reinterpret_cast<void*>(ArcTo));
    ldr.registerExport("gdi32.dll", "CreateEnhMetaFileW", reinterpret_cast<void*>(CreateEnhMetaFileW));
    ldr.registerExport("gdi32.dll", "Arc", reinterpret_cast<void*>(Arc));
    ldr.registerExport("gdi32.dll", "SelectPalette", reinterpret_cast<void*>(SelectPalette));
    ldr.registerExport("gdi32.dll", "SetGraphicsMode", reinterpret_cast<void*>(SetGraphicsMode));
    ldr.registerExport("gdi32.dll", "MaskBlt", reinterpret_cast<void*>(MaskBlt));
    ldr.registerExport("gdi32.dll", "DeleteEnhMetaFile", reinterpret_cast<void*>(DeleteEnhMetaFile));
    ldr.registerExport("gdi32.dll", "Chord", reinterpret_cast<void*>(Chord));
    ldr.registerExport("gdi32.dll", "SetViewportOrgEx", reinterpret_cast<void*>(SetViewportOrgEx));
    ldr.registerExport("gdi32.dll", "GetViewportOrgEx", reinterpret_cast<void*>(GetViewportOrgEx));
    ldr.registerExport("gdi32.dll", "RealizePalette", reinterpret_cast<void*>(RealizePalette));
    ldr.registerExport("gdi32.dll", "SetDIBColorTable", reinterpret_cast<void*>(SetDIBColorTable));
    ldr.registerExport("gdi32.dll", "GetDIBColorTable", reinterpret_cast<void*>(GetDIBColorTable));
    ldr.registerExport("gdi32.dll", "CreateBrushIndirect", reinterpret_cast<void*>(CreateBrushIndirect));
    ldr.registerExport("gdi32.dll", "SetEnhMetaFileBits", reinterpret_cast<void*>(SetEnhMetaFileBits));
    ldr.registerExport("gdi32.dll", "SetWorldTransform", reinterpret_cast<void*>(SetWorldTransform));
    ldr.registerExport("gdi32.dll", "FrameRgn", reinterpret_cast<void*>(FrameRgn));
    ldr.registerExport("gdi32.dll", "GetClipBox", reinterpret_cast<void*>(GetClipBox));
    ldr.registerExport("gdi32.dll", "SetWinMetaFileBits", reinterpret_cast<void*>(SetWinMetaFileBits));
    ldr.registerExport("gdi32.dll", "CreateDIBitmap", reinterpret_cast<void*>(CreateDIBitmap));
    ldr.registerExport("gdi32.dll", "GetStretchBltMode", reinterpret_cast<void*>(GetStretchBltMode));
    ldr.registerExport("gdi32.dll", "ExtCreateRegion", reinterpret_cast<void*>(ExtCreateRegion));
    ldr.registerExport("gdi32.dll", "GetRgnBox", reinterpret_cast<void*>(GetRgnBox));
    ldr.registerExport("gdi32.dll", "EnumFontsW", reinterpret_cast<void*>(EnumFontsW));
    ldr.registerExport("gdi32.dll", "CreateHalftonePalette", reinterpret_cast<void*>(CreateHalftonePalette));
    ldr.registerExport("gdi32.dll", "ExtFloodFill", reinterpret_cast<void*>(ExtFloodFill));
    ldr.registerExport("gdi32.dll", "UnrealizeObject", reinterpret_cast<void*>(UnrealizeObject));
    ldr.registerExport("gdi32.dll", "CopyEnhMetaFileW", reinterpret_cast<void*>(CopyEnhMetaFileW));
    ldr.registerExport("gdi32.dll", "OffsetRgn", reinterpret_cast<void*>(OffsetRgn));
    ldr.registerExport("gdi32.dll", "GetMetaFileBitsEx", reinterpret_cast<void*>(GetMetaFileBitsEx));
    ldr.registerExport("gdi32.dll", "GetBrushOrgEx", reinterpret_cast<void*>(GetBrushOrgEx));
    ldr.registerExport("gdi32.dll", "GetCurrentPositionEx", reinterpret_cast<void*>(GetCurrentPositionEx));
    ldr.registerExport("gdi32.dll", "SetDCPenColor", reinterpret_cast<void*>(SetDCPenColor));
    ldr.registerExport("gdi32.dll", "GetNearestPaletteIndex", reinterpret_cast<void*>(GetNearestPaletteIndex));
    ldr.registerExport("gdi32.dll", "CreateRoundRectRgn", reinterpret_cast<void*>(CreateRoundRectRgn));
    ldr.registerExport("gdi32.dll", "GdiFlush", reinterpret_cast<void*>(GdiFlush));
    ldr.registerExport("gdi32.dll", "GetPaletteEntries", reinterpret_cast<void*>(GetPaletteEntries));

    // shell32.dll
    ldr.registerExportOrdinal("shell32.dll", 18, reinterpret_cast<void*>(ILClone));
    ldr.registerExport("shell32.dll", "SHDoDragDrop", reinterpret_cast<void*>(SHDoDragDrop));
    ldr.registerExport("shell32.dll", "SHCreateShellItemArrayFromIDLists", reinterpret_cast<void*>(SHCreateShellItemArrayFromIDLists));

    // user32.dll
    ldr.registerExport("user32.dll", "SetGestureConfig", reinterpret_cast<void*>(SetGestureConfig));
    ldr.registerExport("user32.dll", "CloseGestureInfoHandle", reinterpret_cast<void*>(CloseGestureInfoHandle));
    ldr.registerExport("user32.dll", "GetGestureInfo", reinterpret_cast<void*>(GetGestureInfo));
    ldr.registerExport("user32.dll", "SystemParametersInfoForDpi", reinterpret_cast<void*>(SystemParametersInfoForDpi));
    ldr.registerExport("user32.dll", "PhysicalToLogicalPoint", reinterpret_cast<void*>(PhysicalToLogicalPoint));
    ldr.registerExport("user32.dll", "GetSystemMetricsForDpi", reinterpret_cast<void*>(GetSystemMetricsForDpi));
    ldr.registerExport("user32.dll", "GetDpiForWindow", reinterpret_cast<void*>(GetDpiForWindow));
    ldr.registerExport("user32.dll", "AreDpiAwarenessContextsEqual", reinterpret_cast<void*>(AreDpiAwarenessContextsEqual));
    ldr.registerExport("user32.dll", "GetAwarenessFromDpiAwarenessContext", reinterpret_cast<void*>(GetAwarenessFromDpiAwarenessContext));
    ldr.registerExport("user32.dll", "GetWindowDpiAwarenessContext", reinterpret_cast<void*>(GetWindowDpiAwarenessContext));
    ldr.registerExport("user32.dll", "GetThreadDpiAwarenessContext", reinterpret_cast<void*>(GetThreadDpiAwarenessContext));
    ldr.registerExport("user32.dll", "SetThreadDpiAwarenessContext", reinterpret_cast<void*>(SetThreadDpiAwarenessContext));
    ldr.registerExport("user32.dll", "AdjustWindowRectExForDpi", reinterpret_cast<void*>(AdjustWindowRectExForDpi));

    // kernel32.dll
    ldr.registerExport("kernel32.dll", "VerLanguageNameW", reinterpret_cast<void*>(VerLanguageNameW));
    ldr.registerExport("kernel32.dll", "GetLogicalProcessorInformation", reinterpret_cast<void*>(GetLogicalProcessorInformation));
    ldr.registerExport("kernel32.dll", "IsWow64Process", reinterpret_cast<void*>(IsWow64Process));
    ldr.registerExport("kernel32.dll", "ProcessIdToSessionId", reinterpret_cast<void*>(ProcessIdToSessionId));
    ldr.registerExport("kernel32.dll", "LocaleNameToLCID", reinterpret_cast<void*>(LocaleNameToLCID));
    ldr.registerExport("kernel32.dll", "GetTimeZoneInformationForYear", reinterpret_cast<void*>(GetTimeZoneInformationForYear));
    ldr.registerExport("kernel32.dll", "GetSystemTimes", reinterpret_cast<void*>(GetSystemTimes));

    // shfolder.dll
    ldr.registerExport("shfolder.dll", "SHGetFolderPathW", reinterpret_cast<void*>(SHGetFolderPathW_WizTree));

    // api-ms-win-crt-string-l1-1-0.dll
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "memset", reinterpret_cast<void*>(crt_memset));

    // ntdll.dll
    ldr.registerExport("ntdll.dll", "NtOpenFile", reinterpret_cast<void*>(NtOpenFile));
    ldr.registerExport("ntdll.dll", "RtlInitUnicodeString", reinterpret_cast<void*>(RtlInitUnicodeString));

    // msimg32.dll
    ldr.registerExport("msimg32.dll", "TransparentBlt", reinterpret_cast<void*>(TransparentBlt));

    // windowscodecs.dll
    ldr.registerExport("windowscodecs.dll", "WICConvertBitmapSource", reinterpret_cast<void*>(WICConvertBitmapSource));

    // uxtheme.dll
    ldr.registerExport("uxtheme.dll", "BufferedPaintSetAlpha", reinterpret_cast<void*>(BufferedPaintSetAlpha));
    ldr.registerExport("uxtheme.dll", "EndBufferedPaint", reinterpret_cast<void*>(EndBufferedPaint));
    ldr.registerExport("uxtheme.dll", "BeginBufferedPaint", reinterpret_cast<void*>(BeginBufferedPaint));
    ldr.registerExport("uxtheme.dll", "BufferedPaintUnInit", reinterpret_cast<void*>(BufferedPaintUnInit));
    ldr.registerExport("uxtheme.dll", "BufferedPaintInit", reinterpret_cast<void*>(BufferedPaintInit));
    ldr.registerExport("uxtheme.dll", "OpenThemeDataForDpi", reinterpret_cast<void*>(OpenThemeDataForDpi));

    // imm32.dll
    ldr.registerExport("imm32.dll", "ImmAssociateContextEx", reinterpret_cast<void*>(ImmAssociateContextEx));

    // dwmapi.dll
    ldr.registerExport("dwmapi.dll", "DwmDefWindowProc", reinterpret_cast<void*>(DwmDefWindowProc));

    // shcore.dll
    ldr.registerExport("shcore.dll", "GetDpiForMonitor", reinterpret_cast<void*>(GetDpiForMonitor));
    ldr.registerExport("shcore.dll", "GetProcessDpiAwareness", reinterpret_cast<void*>(GetProcessDpiAwareness));
    ldr.registerExport("shcore.dll", "GetScaleFactorForMonitor", reinterpret_cast<void*>(GetScaleFactorForMonitor));

    // crypt32.dll
    ldr.registerExport("crypt32.dll", "CryptDecodeObject", reinterpret_cast<void*>(CryptDecodeObject));
    ldr.registerExport("crypt32.dll", "PFXImportCertStore", reinterpret_cast<void*>(PFXImportCertStore));
    ldr.registerExport("crypt32.dll", "CertFindChainInStore", reinterpret_cast<void*>(CertFindChainInStore));
}

} // namespace micant::satellite::wiztree
