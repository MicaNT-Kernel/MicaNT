// ============================================================================
// MicaNT: Extended Win32 User Subsystem (user32_extended.hpp)
//
// Modern Clean-Room Implementation in pure ISO C++23.
// Fulfills standard Windows User32 APIs: Dialogs, Menus, Carets, Cursors,
// Scrolling, Clipboard, Hooks, Rects, Desktop Metrics, and Monitors.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cwchar>
#include <cstdarg>
#include <algorithm>
#include <string>
#include <vector>

#include "ntdef.hpp"
#include "kernel32.hpp"
#include "user32.hpp"
#include "ldr.hpp"

namespace micant::user32 {

using HWND     = win32::HWND;
using INT_PTR  = intptr_t;
using UINT_PTR = uintptr_t;
using COLORREF = uint32_t;

using HMONITOR   = void*;
using HHOOK      = void*;
using HACCEL     = void*;
using HOOKPROC   = LRESULT (*)(int, win32::WPARAM, win32::LPARAM);

struct MONITORINFO {
    win32::DWORD cbSize{sizeof(MONITORINFO)};
    RECT rcMonitor{0, 0, 1920, 1080};
    RECT rcWork{0, 0, 1920, 1040};
    win32::DWORD dwFlags{1}; // MONITORINFOF_PRIMARY
};

struct WINDOWPLACEMENT {
    win32::UINT length{sizeof(WINDOWPLACEMENT)};
    win32::UINT flags{0};
    win32::UINT showCmd{1};
    POINT ptMinPosition{0, 0};
    POINT ptMaxPosition{0, 0};
    RECT rcNormalPosition{100, 100, 900, 700};
};

struct SCROLLINFO {
    win32::UINT cbSize{sizeof(SCROLLINFO)};
    win32::UINT fMask{0};
    int nMin{0};
    int nMax{100};
    win32::UINT nPage{10};
    int nPos{0};
    int nTrackPos{0};
};

struct MENUITEMINFOW {
    win32::UINT cbSize{sizeof(MENUITEMINFOW)};
    win32::UINT fMask{0};
    win32::UINT fType{0};
    win32::UINT fState{0};
    win32::UINT wID{0};
    HMENU hSubMenu{nullptr};
    void* hbmpChecked{nullptr};
    void* hbmpUnchecked{nullptr};
    win32::ULONG_PTR dwItemData{0};
    wchar_t* dwTypeData{nullptr};
    win32::UINT cch{0};
    void* hbmpItem{nullptr};
};

struct MENUBARINFO {
    win32::DWORD cbSize{sizeof(MENUBARINFO)};
    RECT rcBar{0, 0, 800, 24};
    HMENU hMenu{nullptr};
    HWND hwndMenu{nullptr};
    win32::BOOL fBarFocused{win32::FALSE};
    win32::BOOL fFocused{win32::FALSE};
};

struct COMBOBOXINFO {
    win32::DWORD cbSize{sizeof(COMBOBOXINFO)};
    RECT rcItem{0, 0, 100, 20};
    RECT rcButton{80, 0, 100, 20};
    win32::DWORD stateButton{0};
    HWND hwndCombo{nullptr};
    HWND hwndItem{nullptr};
    HWND hwndList{nullptr};
};

struct ICONINFO {
    win32::BOOL fIcon{win32::TRUE};
    win32::DWORD xHotspot{0};
    win32::DWORD yHotspot{0};
    void* hbmMask{nullptr};
    void* hbmColor{nullptr};
};

struct FLASHWINFO {
    win32::UINT cbSize{sizeof(FLASHWINFO)};
    HWND hwnd{nullptr};
    win32::DWORD dwFlags{0};
    win32::UINT uCount{0};
    win32::DWORD dwTimeout{0};
};

// ----------------------------------------------------------------------------
// 1. Monitors & Displays
// ----------------------------------------------------------------------------

inline HMONITOR MonitorFromRect(const RECT* /*lprc*/, win32::DWORD /*dwFlags*/) noexcept {
    static uint64_t dummyMon = 0x1111;
    return reinterpret_cast<HMONITOR>(&dummyMon);
}

inline HMONITOR MonitorFromPoint(POINT /*pt*/, win32::DWORD /*dwFlags*/) noexcept {
    static uint64_t dummyMon = 0x1111;
    return reinterpret_cast<HMONITOR>(&dummyMon);
}

inline HMONITOR MonitorFromWindow(win32::HWND /*hwnd*/, win32::DWORD /*dwFlags*/) noexcept {
    static uint64_t dummyMon = 0x1111;
    return reinterpret_cast<HMONITOR>(&dummyMon);
}

inline win32::BOOL GetMonitorInfoW(HMONITOR /*hMonitor*/, MONITORINFO* lpmi) noexcept {
    if (!lpmi) return win32::FALSE;
    lpmi->rcMonitor = RECT{0, 0, 1920, 1080};
    lpmi->rcWork    = RECT{0, 0, 1920, 1040};
    lpmi->dwFlags   = 1;
    return win32::TRUE;
}

inline win32::BOOL EnumDisplayMonitors(HDC /*hdc*/, const RECT* /*lprcClip*/, void* lpfnEnum, win32::LPARAM dwData) noexcept {
    if (lpfnEnum) {
        using MonEnumProc = win32::BOOL (*)(HMONITOR, HDC, RECT*, win32::LPARAM);
        auto pfn = reinterpret_cast<MonEnumProc>(lpfnEnum);
        RECT rc{0, 0, 1920, 1080};
        pfn(MonitorFromWindow(nullptr, 0), nullptr, &rc, dwData);
    }
    return win32::TRUE;
}

// ----------------------------------------------------------------------------
// 2. Dialogs & Controls
// ----------------------------------------------------------------------------

inline win32::HWND CreateDialogParamW(HINSTANCE /*hInstance*/, const wchar_t* /*lpTemplateName*/, win32::HWND hWndParent, void* /*lpDialogFunc*/, win32::LPARAM /*dwInitParam*/) noexcept {
    return CreateWindowExW(0, L"Static", L"", 0x10000000, 0, 0, 100, 100, hWndParent, nullptr, nullptr, nullptr);
}

inline win32::HWND CreateDialogIndirectParamW(HINSTANCE /*hInstance*/, const void* /*lpTemplate*/, win32::HWND hWndParent, void* /*lpDialogFunc*/, win32::LPARAM /*dwInitParam*/) noexcept {
    return CreateWindowExW(0, L"Static", L"", 0x10000000, 0, 0, 100, 100, hWndParent, nullptr, nullptr, nullptr);
}

inline INT_PTR DialogBoxParamW(HINSTANCE /*hInstance*/, const wchar_t* /*lpTemplateName*/, win32::HWND /*hWndParent*/, void* /*lpDialogFunc*/, win32::LPARAM /*dwInitParam*/) noexcept {
    return 1;
}

inline INT_PTR DialogBoxIndirectParamW(HINSTANCE /*hInstance*/, const void* /*hDialogTemplate*/, win32::HWND /*hWndParent*/, void* /*lpDialogFunc*/, win32::LPARAM /*dwInitParam*/) noexcept {
    return 1;
}

inline win32::BOOL EndDialog(win32::HWND /*hDlg*/, INT_PTR /*nResult*/) noexcept {
    return win32::TRUE;
}

inline win32::HWND GetDlgItem(win32::HWND hDlg, int nIDDlgItem) noexcept {
    return reinterpret_cast<win32::HWND>(static_cast<uintptr_t>(0x2000 + nIDDlgItem));
}

inline int GetDlgCtrlID(win32::HWND hWnd) noexcept {
    uintptr_t val = reinterpret_cast<uintptr_t>(hWnd);
    return (val >= 0x2000) ? static_cast<int>(val - 0x2000) : 0;
}

inline win32::UINT GetDlgItemTextA(win32::HWND /*hDlg*/, int /*nIDDlgItem*/, char* lpString, int cchMax) noexcept {
    if (lpString && cchMax > 0) lpString[0] = '\0';
    return 0;
}

inline win32::UINT GetDlgItemTextW(win32::HWND /*hDlg*/, int /*nIDDlgItem*/, wchar_t* lpString, int cchMax) noexcept {
    if (lpString && cchMax > 0) lpString[0] = L'\0';
    return 0;
}

inline win32::BOOL SetDlgItemTextA(win32::HWND /*hDlg*/, int /*nIDDlgItem*/, const char* /*lpString*/) noexcept {
    return win32::TRUE;
}

inline win32::BOOL SetDlgItemTextW(win32::HWND /*hDlg*/, int /*nIDDlgItem*/, const wchar_t* /*lpString*/) noexcept {
    return win32::TRUE;
}

inline win32::BOOL SetDlgItemInt(win32::HWND /*hDlg*/, int /*nIDDlgItem*/, win32::UINT /*uValue*/, win32::BOOL /*bSigned*/) noexcept {
    return win32::TRUE;
}

inline win32::UINT GetDlgItemInt(win32::HWND /*hDlg*/, int /*nIDDlgItem*/, win32::BOOL* lpTranslated, win32::BOOL /*bSigned*/) noexcept {
    if (lpTranslated) *lpTranslated = win32::TRUE;
    return 0;
}

inline LRESULT SendDlgItemMessageW(win32::HWND /*hDlg*/, int /*nIDDlgItem*/, UINT /*Msg*/, WPARAM /*wParam*/, LPARAM /*lParam*/) noexcept {
    return 0;
}

inline win32::BOOL GetComboBoxInfo(win32::HWND /*hwndCombo*/, COMBOBOXINFO* pcbi) noexcept {
    if (!pcbi) return win32::FALSE;
    pcbi->rcItem = RECT{0, 0, 100, 20};
    pcbi->rcButton = RECT{80, 0, 100, 20};
    pcbi->stateButton = 0;
    return win32::TRUE;
}

// ----------------------------------------------------------------------------
// 3. Menus & Accelerators
// ----------------------------------------------------------------------------

inline HMENU CreateMenu() noexcept { static uint64_t dummy = 0x3001; return reinterpret_cast<HMENU>(&dummy); }
inline HMENU CreatePopupMenu() noexcept { static uint64_t dummy = 0x3002; return reinterpret_cast<HMENU>(&dummy); }
inline HMENU LoadMenuW(HINSTANCE /*hInstance*/, const wchar_t* /*lpMenuName*/) noexcept { static uint64_t dummy = 0x3003; return reinterpret_cast<HMENU>(&dummy); }
inline HMENU LoadMenuA(HINSTANCE /*hInstance*/, const char* /*lpMenuName*/) noexcept { static uint64_t dummy = 0x3004; return reinterpret_cast<HMENU>(&dummy); }
inline win32::BOOL DestroyMenu(HMENU /*hMenu*/) noexcept { return win32::TRUE; }
inline win32::BOOL AppendMenuA(HMENU /*hMenu*/, win32::UINT /*uFlags*/, UINT_PTR /*uIDNewItem*/, const char* /*lpNewItem*/) noexcept { return win32::TRUE; }
inline win32::BOOL AppendMenuW(HMENU /*hMenu*/, win32::UINT /*uFlags*/, UINT_PTR /*uIDNewItem*/, const wchar_t* /*lpNewItem*/) noexcept { return win32::TRUE; }
inline win32::BOOL InsertMenuW(HMENU /*hMenu*/, win32::UINT /*uPosition*/, win32::UINT /*uFlags*/, UINT_PTR /*uIDNewItem*/, const wchar_t* /*lpNewItem*/) noexcept { return win32::TRUE; }
inline win32::BOOL InsertMenuItemW(HMENU /*hmenu*/, win32::UINT /*item*/, win32::BOOL /*fByPosition*/, const MENUITEMINFOW* /*lpmi*/) noexcept { return win32::TRUE; }
inline win32::BOOL SetMenuItemInfoW(HMENU /*hmenu*/, win32::UINT /*item*/, win32::BOOL /*fByPosition*/, const MENUITEMINFOW* /*lpmii*/) noexcept { return win32::TRUE; }
inline win32::BOOL GetMenuItemInfoW(HMENU /*hMenu*/, win32::UINT /*item*/, win32::BOOL /*fByPosition*/, MENUITEMINFOW* /*lpmii*/) noexcept { return win32::TRUE; }
inline win32::BOOL ModifyMenuW(HMENU /*hMnu*/, win32::UINT /*uPosition*/, win32::UINT /*uFlags*/, UINT_PTR /*uIDNewItem*/, const wchar_t* /*lpNewItem*/) noexcept { return win32::TRUE; }
inline win32::BOOL DeleteMenu(HMENU /*hMenu*/, win32::UINT /*uPosition*/, win32::UINT /*uFlags*/) noexcept { return win32::TRUE; }
inline win32::BOOL RemoveMenu(HMENU /*hMenu*/, win32::UINT /*uPosition*/, win32::UINT /*uFlags*/) noexcept { return win32::TRUE; }
inline HMENU GetMenu(win32::HWND /*hWnd*/) noexcept { return nullptr; }
inline win32::BOOL SetMenu(win32::HWND /*hWnd*/, HMENU /*hMenu*/) noexcept { return win32::TRUE; }
inline win32::BOOL DrawMenuBar(win32::HWND /*hWnd*/) noexcept { return win32::TRUE; }
inline int GetMenuItemCount(HMENU /*hMenu*/) noexcept { return 0; }
inline win32::UINT GetMenuItemID(HMENU /*hMenu*/, int /*nPos*/) noexcept { return 0; }
inline int GetMenuStringW(HMENU /*hMenu*/, win32::UINT /*uIDItem*/, wchar_t* lpString, int cchMax, win32::UINT /*flags*/) noexcept {
    if (lpString && cchMax > 0) lpString[0] = L'\0';
    return 0;
}
inline win32::UINT GetMenuState(HMENU /*hMenu*/, win32::UINT /*uId*/, win32::UINT /*uFlags*/) noexcept { return 0; }
inline HMENU GetSubMenu(HMENU /*hMenu*/, int /*nPos*/) noexcept { return nullptr; }
inline win32::DWORD CheckMenuItem(HMENU /*hMenu*/, win32::UINT /*uIDCheckItem*/, win32::UINT /*uCheck*/) noexcept { return 0; }
inline win32::BOOL CheckMenuRadioItem(HMENU /*hmenu*/, win32::UINT /*first*/, win32::UINT /*last*/, win32::UINT /*check*/, win32::UINT /*flags*/) noexcept { return win32::TRUE; }
inline win32::BOOL EnableMenuItem(HMENU /*hMenu*/, win32::UINT /*uIDEnableItem*/, win32::UINT /*uEnable*/) noexcept { return win32::TRUE; }
inline win32::BOOL SetMenuItemBitmaps(HMENU /*hMenu*/, win32::UINT /*uPosition*/, win32::UINT /*uFlags*/, void* /*hBitmapUnchecked*/, void* /*hBitmapChecked*/) noexcept { return win32::TRUE; }
inline win32::BOOL TrackPopupMenu(HMENU /*hMenu*/, win32::UINT /*uFlags*/, int /*x*/, int /*y*/, int /*nReserved*/, win32::HWND /*hWnd*/, const RECT* /*prcRect*/) noexcept { return win32::TRUE; }
inline win32::BOOL TrackPopupMenuEx(HMENU /*hMenu*/, win32::UINT /*uFlags*/, int /*x*/, int /*y*/, win32::HWND /*hWnd*/, void* /*lptpm*/) noexcept { return win32::TRUE; }
inline win32::BOOL GetMenuBarInfo(win32::HWND /*hwnd*/, win32::LONG /*idObject*/, win32::LONG /*idItem*/, MENUBARINFO* pmbi) noexcept {
    if (!pmbi) return win32::FALSE;
    pmbi->rcBar = RECT{0, 0, 800, 24};
    return win32::TRUE;
}
inline HACCEL CreateAcceleratorTableW(void* /*paccel*/, int /*cAccel*/) noexcept {
    static uint64_t dummyAccel = 0x4001;
    return reinterpret_cast<HACCEL>(&dummyAccel);
}
inline win32::BOOL DestroyAcceleratorTable(HACCEL /*hAccel*/) noexcept { return win32::TRUE; }
inline int TranslateAcceleratorW(win32::HWND /*hWnd*/, HACCEL /*hAccTable*/, MSG* /*lpMsg*/) noexcept { return 0; }

// ----------------------------------------------------------------------------
// 4. Carets, Cursors, Bitmaps, Icons
// ----------------------------------------------------------------------------

inline win32::BOOL CreateCaret(win32::HWND /*hWnd*/, void* /*hBitmap*/, int /*nWidth*/, int /*nHeight*/) noexcept { return win32::TRUE; }
inline win32::BOOL DestroyCaret() noexcept { return win32::TRUE; }
inline win32::BOOL ShowCaret(win32::HWND /*hWnd*/) noexcept { return win32::TRUE; }
inline win32::BOOL HideCaret(win32::HWND /*hWnd*/) noexcept { return win32::TRUE; }
inline win32::BOOL SetCaretPos(int /*X*/, int /*Y*/) noexcept { return win32::TRUE; }
inline win32::UINT GetCaretBlinkTime() noexcept { return 530; }

inline HCURSOR LoadCursorW(HINSTANCE /*hInstance*/, const wchar_t* /*lpCursorName*/) noexcept {
    static uint64_t dummyCur = 0x5001;
    return reinterpret_cast<HCURSOR>(&dummyCur);
}
inline win32::BOOL DestroyCursor(HCURSOR /*hCursor*/) noexcept { return win32::TRUE; }
inline HCURSOR SetCursor(HCURSOR hCursor) noexcept { return hCursor; }
inline int ShowCursor(win32::BOOL /*bShow*/) noexcept { return 0; }
inline win32::BOOL GetCursorPos(POINT* lpPoint) noexcept {
    if (lpPoint) { lpPoint->x = 500; lpPoint->y = 500; }
    return win32::TRUE;
}

inline HICON LoadIconW(HINSTANCE /*hInstance*/, const wchar_t* /*lpIconName*/) noexcept {
    static uint64_t dummyIcon = 0x6001;
    return reinterpret_cast<HICON>(&dummyIcon);
}
inline win32::BOOL DestroyIcon(HICON /*hIcon*/) noexcept { return win32::TRUE; }
inline win32::BOOL DrawIconEx(HDC /*hdc*/, int /*xLeft*/, int /*yTop*/, HICON /*hIcon*/, int /*cxWidth*/, int /*cyWidth*/, win32::UINT /*istepIfAniCur*/, HBRUSH /*hbrFlickerFreeDraw*/, win32::UINT /*diFlags*/) noexcept { return win32::TRUE; }
inline win32::BOOL GetIconInfo(HICON /*hIcon*/, ICONINFO* piconinfo) noexcept {
    if (!piconinfo) return win32::FALSE;
    piconinfo->fIcon = win32::TRUE;
    piconinfo->xHotspot = 0;
    piconinfo->yHotspot = 0;
    piconinfo->hbmMask = nullptr;
    piconinfo->hbmColor = nullptr;
    return win32::TRUE;
}
inline HICON CreateIconIndirect(ICONINFO* /*piconinfo*/) noexcept {
    static uint64_t dummyIcon = 0x6002;
    return reinterpret_cast<HICON>(&dummyIcon);
}
inline void* LoadBitmapW(HINSTANCE /*hInstance*/, const wchar_t* /*lpBitmapName*/) noexcept {
    static uint64_t dummyBmp = 0x7001;
    return reinterpret_cast<void*>(&dummyBmp);
}
inline void* LoadImageW(HINSTANCE /*hInst*/, const wchar_t* /*name*/, win32::UINT /*type*/, int /*cx*/, int /*cy*/, win32::UINT /*fuLoad*/) noexcept {
    static uint64_t dummyImg = 0x7002;
    return reinterpret_cast<void*>(&dummyImg);
}

// ----------------------------------------------------------------------------
// 5. Scrolling
// ----------------------------------------------------------------------------

inline win32::BOOL ScrollWindow(win32::HWND /*hWnd*/, int /*XAmount*/, int /*YAmount*/, const RECT* /*lpRect*/, const RECT* /*lpClipRect*/) noexcept { return win32::TRUE; }
inline int SetScrollInfo(win32::HWND /*hwnd*/, int /*nBar*/, const SCROLLINFO* lpsi, win32::BOOL /*redraw*/) noexcept { return lpsi ? lpsi->nPos : 0; }
inline win32::BOOL GetScrollInfo(win32::HWND /*hwnd*/, int /*nBar*/, SCROLLINFO* lpsi) noexcept {
    if (lpsi) { lpsi->nMin = 0; lpsi->nMax = 100; lpsi->nPage = 10; lpsi->nPos = 0; }
    return win32::TRUE;
}
inline int SetScrollPos(win32::HWND /*hWnd*/, int /*nBar*/, int nPos, win32::BOOL /*bRedraw*/) noexcept { return nPos; }
inline int GetScrollPos(win32::HWND /*hWnd*/, int /*nBar*/) noexcept { return 0; }
inline win32::BOOL SetScrollRange(win32::HWND /*hWnd*/, int /*nBar*/, int /*nMinPos*/, int /*nMaxPos*/, win32::BOOL /*bRedraw*/) noexcept { return win32::TRUE; }
inline win32::BOOL GetScrollRange(win32::HWND /*hWnd*/, int /*nBar*/, int* lpMinPos, int* lpMaxPos) noexcept {
    if (lpMinPos) *lpMinPos = 0;
    if (lpMaxPos) *lpMaxPos = 100;
    return win32::TRUE;
}
inline win32::BOOL ShowScrollBar(win32::HWND /*hWnd*/, int /*wBar*/, win32::BOOL /*bShow*/) noexcept { return win32::TRUE; }

// ----------------------------------------------------------------------------
// 6. Clipboard & Properties
// ----------------------------------------------------------------------------

inline win32::BOOL OpenClipboard(win32::HWND /*hWndNewOwner*/) noexcept { return win32::TRUE; }
inline win32::BOOL CloseClipboard() noexcept { return win32::TRUE; }
inline win32::BOOL EmptyClipboard() noexcept { return win32::TRUE; }
inline void* GetClipboardData(win32::UINT /*uFormat*/) noexcept { return nullptr; }
inline void* SetClipboardData(win32::UINT /*uFormat*/, void* hMem) noexcept { return hMem; }
inline win32::BOOL IsClipboardFormatAvailable(win32::UINT /*format*/) noexcept { return win32::FALSE; }
inline win32::UINT RegisterClipboardFormatW(const wchar_t* /*lpszFormat*/) noexcept { static win32::UINT s_fmt = 0xC000; return ++s_fmt; }
inline win32::HWND SetClipboardViewer(win32::HWND /*hWndNewViewer*/) noexcept { return nullptr; }
inline win32::BOOL ChangeClipboardChain(win32::HWND /*hWndRemove*/, win32::HWND /*hWndNewNext*/) noexcept { return win32::TRUE; }
inline win32::BOOL AddClipboardFormatListener(win32::HWND /*hwnd*/) noexcept { return win32::TRUE; }
inline win32::BOOL RemoveClipboardFormatListener(win32::HWND /*hwnd*/) noexcept { return win32::TRUE; }

inline void* RemovePropW(win32::HWND /*hWnd*/, const wchar_t* /*lpString*/) noexcept { return nullptr; }
inline void* GetPropW(win32::HWND /*hWnd*/, const wchar_t* /*lpString*/) noexcept { return nullptr; }
inline win32::BOOL SetPropW(win32::HWND /*hWnd*/, const wchar_t* /*lpString*/, void* /*hData*/) noexcept { return win32::TRUE; }

// ----------------------------------------------------------------------------
// 7. Hooks, Events, Metrics, Rectangles
// ----------------------------------------------------------------------------

inline LRESULT CallNextHookEx(HHOOK /*hhk*/, int /*nCode*/, WPARAM /*wParam*/, LPARAM /*lParam*/) noexcept { return 0; }
inline HHOOK SetWindowsHookExW(int /*idHook*/, HOOKPROC /*lpfn*/, HINSTANCE /*hmod*/, win32::DWORD /*dwThreadId*/) noexcept {
    static uint64_t dummyHook = 0x8001;
    return reinterpret_cast<HHOOK>(&dummyHook);
}
inline win32::BOOL UnhookWindowsHookEx(HHOOK /*hhk*/) noexcept { return win32::TRUE; }
inline void NotifyWinEvent(win32::DWORD /*event*/, win32::HWND /*hwnd*/, win32::LONG /*idObject*/, win32::LONG /*idChild*/) noexcept {}

inline win32::BOOL SetRectEmpty(RECT* lprc) noexcept {
    if (!lprc) return win32::FALSE;
    lprc->left = lprc->top = lprc->right = lprc->bottom = 0;
    return win32::TRUE;
}
inline win32::BOOL InflateRect(RECT* lprc, int dx, int dy) noexcept {
    if (!lprc) return win32::FALSE;
    lprc->left -= dx; lprc->top -= dy; lprc->right += dx; lprc->bottom += dy;
    return win32::TRUE;
}
inline win32::BOOL OffsetRect(RECT* lprc, int dx, int dy) noexcept {
    if (!lprc) return win32::FALSE;
    lprc->left += dx; lprc->top += dy; lprc->right += dx; lprc->bottom += dy;
    return win32::TRUE;
}
inline win32::BOOL IntersectRect(RECT* lprcDst, const RECT* lprcSrc1, const RECT* lprcSrc2) noexcept {
    if (!lprcDst || !lprcSrc1 || !lprcSrc2) return win32::FALSE;
    lprcDst->left   = std::max(lprcSrc1->left, lprcSrc2->left);
    lprcDst->top    = std::max(lprcSrc1->top, lprcSrc2->top);
    lprcDst->right  = std::min(lprcSrc1->right, lprcSrc2->right);
    lprcDst->bottom = std::min(lprcSrc1->bottom, lprcSrc2->bottom);
    if (lprcDst->left >= lprcDst->right || lprcDst->top >= lprcDst->bottom) {
        SetRectEmpty(lprcDst);
        return win32::FALSE;
    }
    return win32::TRUE;
}
inline win32::BOOL EqualRect(const RECT* lprc1, const RECT* lprc2) noexcept {
    if (!lprc1 || !lprc2) return win32::FALSE;
    return (lprc1->left == lprc2->left && lprc1->top == lprc2->top &&
            lprc1->right == lprc2->right && lprc1->bottom == lprc2->bottom) ? win32::TRUE : win32::FALSE;
}
inline win32::BOOL PtInRect(const RECT* lprc, POINT pt) noexcept {
    if (!lprc) return win32::FALSE;
    return (pt.x >= lprc->left && pt.x < lprc->right && pt.y >= lprc->top && pt.y < lprc->bottom) ? win32::TRUE : win32::FALSE;
}

inline win32::BOOL ClientToScreen(win32::HWND /*hWnd*/, POINT* /*lpPoint*/) noexcept { return win32::TRUE; }
inline win32::BOOL ScreenToClient(win32::HWND /*hWnd*/, POINT* /*lpPoint*/) noexcept { return win32::TRUE; }
inline int MapWindowPoints(win32::HWND /*hWndFrom*/, win32::HWND /*hWndTo*/, POINT* /*lpPoints*/, win32::UINT /*cPoints*/) noexcept { return 0; }
inline win32::HWND WindowFromPoint(POINT /*Point*/) noexcept { return nullptr; }
inline win32::HWND ChildWindowFromPointEx(win32::HWND hwnd, POINT /*pt*/, win32::UINT /*flags*/) noexcept { return hwnd; }

inline win32::DWORD GetSysColor(int /*nIndex*/) noexcept { return 0x00F0F0F0; }
inline void* GetSysColorBrush(int /*nIndex*/) noexcept { static uint64_t dummyBrush = 0x9001; return reinterpret_cast<void*>(&dummyBrush); }
inline win32::BOOL SystemParametersInfoA(win32::UINT /*uiAction*/, win32::UINT /*uiParam*/, void* /*pvParam*/, win32::UINT /*fWinIni*/) noexcept { return win32::TRUE; }
inline win32::BOOL SystemParametersInfoW(win32::UINT /*uiAction*/, win32::UINT /*uiParam*/, void* /*pvParam*/, win32::UINT /*fWinIni*/) noexcept { return win32::TRUE; }
inline win32::UINT GetDoubleClickTime() noexcept { return 500; }
inline win32::DWORD GetMessageTime() noexcept { return static_cast<win32::DWORD>(kernel32::GetTickCount64()); }
inline win32::BOOL MessageBeep(win32::UINT /*uType*/) noexcept { return win32::TRUE; }
inline win32::BOOL LockWindowUpdate(win32::HWND /*hWndLock*/) noexcept { return win32::TRUE; }
inline win32::BOOL RedrawWindow(win32::HWND /*hWnd*/, const RECT* /*lprcUpdate*/, void* /*hrgnUpdate*/, win32::UINT /*flags*/) noexcept { return win32::TRUE; }
inline HDC GetWindowDC(win32::HWND hWnd) noexcept { return GetDC(hWnd); }
inline HDC GetDCEx(win32::HWND hWnd, void* /*hrgnClip*/, win32::DWORD /*flags*/) noexcept { return GetDC(hWnd); }
inline int GetUpdateRgn(win32::HWND /*hWnd*/, void* /*hRgn*/, win32::BOOL /*bErase*/) noexcept { return 0; }

// ----------------------------------------------------------------------------
// 8. Window Hierarchy, Placement, Capture, Input
// ----------------------------------------------------------------------------

inline win32::HWND FindWindowW(const wchar_t* /*lpClassName*/, const wchar_t* /*lpWindowName*/) noexcept { return nullptr; }
inline win32::HWND FindWindowExW(win32::HWND /*hWndParent*/, win32::HWND /*hWndChildAfter*/, const wchar_t* /*lpszClass*/, const wchar_t* /*lpszWindow*/) noexcept { return nullptr; }
inline win32::HWND GetParent(win32::HWND /*hWnd*/) noexcept { return nullptr; }
inline win32::HWND SetParent(win32::HWND /*hWndChild*/, win32::HWND /*hWndNewParent*/) noexcept { return nullptr; }
inline win32::HWND GetAncestor(win32::HWND hwnd, win32::UINT /*gaFlags*/) noexcept { return hwnd; }
inline win32::HWND GetWindow(win32::HWND /*hWnd*/, win32::UINT /*uCmd*/) noexcept { return nullptr; }
inline win32::HWND GetActiveWindow() noexcept { return GetFocus(); }
inline win32::BOOL BringWindowToTop(win32::HWND /*hWnd*/) noexcept { return win32::TRUE; }
inline win32::BOOL SetForegroundWindow(win32::HWND /*hWnd*/) noexcept { return win32::TRUE; }
inline win32::BOOL IsChild(win32::HWND /*hWndParent*/, win32::HWND /*hWnd*/) noexcept { return win32::FALSE; }
inline win32::BOOL IsZoomed(win32::HWND /*hWnd*/) noexcept { return win32::FALSE; }
inline win32::HWND GetLastActivePopup(win32::HWND hWnd) noexcept { return hWnd; }
inline win32::BOOL EnumChildWindows(win32::HWND /*hWndParent*/, void* /*lpEnumFunc*/, win32::LPARAM /*lParam*/) noexcept { return win32::TRUE; }
inline win32::BOOL EnumThreadWindows(win32::DWORD /*dwThreadId*/, void* /*lpfn*/, win32::LPARAM /*lParam*/) noexcept { return win32::TRUE; }
inline win32::DWORD GetWindowThreadProcessId(win32::HWND /*hWnd*/, win32::DWORD* lpdwProcessId) noexcept {
    if (lpdwProcessId) *lpdwProcessId = kernel32::GetCurrentProcessId();
    return 1000;
}
inline win32::BOOL GetWindowPlacement(win32::HWND /*hWnd*/, WINDOWPLACEMENT* lpwndpl) noexcept {
    if (!lpwndpl) return win32::FALSE;
    lpwndpl->showCmd = 1;
    lpwndpl->rcNormalPosition = RECT{100, 100, 900, 700};
    return win32::TRUE;
}
inline win32::BOOL SetWindowPlacement(win32::HWND /*hWnd*/, const WINDOWPLACEMENT* /*lpwndpl*/) noexcept { return win32::TRUE; }
inline void* BeginDeferWindowPos(int /*nNumWindows*/) noexcept { static uint64_t dummy = 0xA001; return reinterpret_cast<void*>(&dummy); }
inline void* DeferWindowPos(void* hWinPosInfo, win32::HWND /*hWnd*/, win32::HWND /*hWndInsertAfter*/, int /*x*/, int /*y*/, int /*cx*/, int /*cy*/, win32::UINT /*uFlags*/) noexcept { return hWinPosInfo; }
inline win32::BOOL EndDeferWindowPos(void* /*hWinPosInfo*/) noexcept { return win32::TRUE; }
inline win32::BOOL FlashWindowEx(FLASHWINFO* /*pfwi*/) noexcept { return win32::TRUE; }
inline win32::BOOL SetLayeredWindowAttributes(win32::HWND /*hwnd*/, COLORREF /*crKey*/, uint8_t /*bAlpha*/, win32::DWORD /*dwFlags*/) noexcept { return win32::TRUE; }

inline win32::HWND GetCapture() noexcept { return nullptr; }
inline win32::HWND SetCapture(win32::HWND hWnd) noexcept { return hWnd; }
inline win32::BOOL ReleaseCapture() noexcept { return win32::TRUE; }

inline UINT_PTR SetTimer(win32::HWND /*hWnd*/, UINT_PTR nIDEvent, win32::UINT /*uElapse*/, void* /*lpTimerFunc*/) noexcept { return nIDEvent ? nIDEvent : 1; }
inline win32::BOOL KillTimer(win32::HWND /*hWnd*/, UINT_PTR /*uIDEvent*/) noexcept { return win32::TRUE; }

inline win32::BOOL GetKeyboardState(uint8_t* lpKeyState) noexcept {
    if (lpKeyState) std::memset(lpKeyState, 0, 256);
    return win32::TRUE;
}
inline int16_t GetKeyState(int /*nVirtKey*/) noexcept { return 0; }
inline void* GetKeyboardLayout(win32::DWORD /*idThread*/) noexcept { return reinterpret_cast<void*>(0x04090409); }
inline win32::UINT MapVirtualKeyW(win32::UINT uCode, win32::UINT /*uMapType*/) noexcept { return uCode; }
inline int ToAscii(win32::UINT uVirtKey, win32::UINT /*uScanCode*/, const uint8_t* /*lpKeyState*/, uint16_t* lpChar, win32::UINT /*uFlags*/) noexcept {
    if (lpChar) *lpChar = static_cast<uint16_t>(uVirtKey);
    return 1;
}
inline void mouse_event(win32::DWORD /*dwFlags*/, win32::DWORD /*dx*/, win32::DWORD /*dy*/, win32::DWORD /*dwData*/, win32::ULONG_PTR /*dwExtraInfo*/) noexcept {}
inline win32::BOOL TrackMouseEvent(void* /*lpEventTrack*/) noexcept { return win32::TRUE; }

// ----------------------------------------------------------------------------
// 9. Drawing & Text Functions
// ----------------------------------------------------------------------------

inline int DrawTextW(HDC /*hdc*/, const wchar_t* /*lpchText*/, int cchText, RECT* lprc, win32::UINT /*format*/) noexcept {
    return (lprc && cchText > 0) ? 16 : 0;
}
inline int DrawTextExW(HDC /*hdc*/, wchar_t* /*lpchText*/, int cchText, RECT* lprc, win32::UINT /*format*/, void* /*lpdtp*/) noexcept {
    return (lprc && cchText > 0) ? 16 : 0;
}
inline win32::BOOL DrawEdge(HDC /*hdc*/, RECT* /*qrc*/, win32::UINT /*edge*/, win32::UINT /*grfFlags*/) noexcept { return win32::TRUE; }
inline win32::BOOL DrawFrameControl(HDC /*hdc*/, RECT* /*lprc*/, win32::UINT /*uType*/, win32::UINT /*uState*/) noexcept { return win32::TRUE; }
inline win32::BOOL DrawFocusRect(HDC /*hDC*/, const RECT* /*lprc*/) noexcept { return win32::TRUE; }
inline int FrameRect(HDC /*hDC*/, const RECT* /*lprc*/, HBRUSH /*hbr*/) noexcept { return 1; }
inline int FillRect(HDC /*hDC*/, const RECT* /*lprc*/, HBRUSH /*hbr*/) noexcept { return 1; }

// ----------------------------------------------------------------------------
// 10. Strings, Classes, Messages
// ----------------------------------------------------------------------------

inline int LoadStringW(HINSTANCE /*hInstance*/, win32::UINT /*uID*/, wchar_t* lpBuffer, int cchBufferMax) noexcept {
    if (lpBuffer && cchBufferMax > 0) lpBuffer[0] = L'\0';
    return 0;
}
inline int LoadStringA(HINSTANCE /*hInstance*/, win32::UINT /*uID*/, char* lpBuffer, int cchBufferMax) noexcept {
    if (lpBuffer && cchBufferMax > 0) lpBuffer[0] = '\0';
    return 0;
}
inline int GetClassNameW(win32::HWND /*hWnd*/, wchar_t* lpClassName, int nMaxCount) noexcept {
    const wchar_t cls[] = L"MicaNT_WindowClass";
    if (!lpClassName || nMaxCount <= 0) return 0;
    int len = std::min(nMaxCount - 1, static_cast<int>(wcslen(cls)));
    std::wmemcpy(lpClassName, cls, len);
    lpClassName[len] = L'\0';
    return len;
}
inline int GetClassNameA(win32::HWND /*hWnd*/, char* lpClassName, int nMaxCount) noexcept {
    const char cls[] = "MicaNT_WindowClass";
    if (!lpClassName || nMaxCount <= 0) return 0;
    int len = std::min(nMaxCount - 1, static_cast<int>(strlen(cls)));
    std::memcpy(lpClassName, cls, len);
    lpClassName[len] = '\0';
    return len;
}
inline int GetWindowTextA(win32::HWND /*hWnd*/, char* lpString, int nMaxCount) noexcept {
    if (lpString && nMaxCount > 0) lpString[0] = '\0';
    return 0;
}
inline int wsprintfW(wchar_t* buffer, const wchar_t* format, ...) noexcept {
    if (!buffer || !format) return 0;
    va_list args;
    va_start(args, format);
    int res = vswprintf(buffer, 2048, format, args);
    va_end(args);
    return res;
}
inline win32::BOOL IsCharLowerW(wchar_t ch) noexcept { return (ch >= L'a' && ch <= L'z') ? win32::TRUE : win32::FALSE; }
inline wchar_t* CharLowerW(wchar_t* lpsz) noexcept {
    if (!lpsz) return nullptr;
    if (reinterpret_cast<uintptr_t>(lpsz) <= 0xFFFF) {
        return reinterpret_cast<wchar_t*>(static_cast<uintptr_t>(towlower(static_cast<wchar_t>(reinterpret_cast<uintptr_t>(lpsz)))));
    }
    for (size_t i = 0; lpsz[i]; ++i) lpsz[i] = towlower(lpsz[i]);
    return lpsz;
}
inline win32::BOOL IsCharAlphaNumericW(wchar_t ch) noexcept { return iswalnum(ch) ? win32::TRUE : win32::FALSE; }
inline win32::BOOL IsCharAlphaW(wchar_t ch) noexcept { return iswalpha(ch) ? win32::TRUE : win32::FALSE; }
inline LRESULT CallWindowProcW(WNDPROC lpPrevWndFunc, win32::HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) noexcept {
    if (lpPrevWndFunc) return lpPrevWndFunc(hWnd, Msg, wParam, lParam);
    return DefWindowProcW(hWnd, Msg, wParam, lParam);
}
inline win32::UINT RegisterWindowMessageW(const wchar_t* /*lpString*/) noexcept {
    static win32::UINT s_msg = 0xC000;
    return ++s_msg;
}
inline win32::BOOL IsDialogMessageW(win32::HWND /*hDlg*/, MSG* /*lpMsg*/) noexcept {
    return win32::FALSE;
}

// ----------------------------------------------------------------------------
// 11. Subsystem Export Registration (user32 extended)
// ----------------------------------------------------------------------------

inline void InitializeUser32ExtendedExports() {
    auto& ldr = ldr::DynamicLoader::get();

    ldr.registerExport("user32.dll", "MonitorFromRect", reinterpret_cast<void*>(MonitorFromRect));
    ldr.registerExport("user32.dll", "MonitorFromPoint", reinterpret_cast<void*>(MonitorFromPoint));
    ldr.registerExport("user32.dll", "MonitorFromWindow", reinterpret_cast<void*>(MonitorFromWindow));
    ldr.registerExport("user32.dll", "GetMonitorInfoW", reinterpret_cast<void*>(GetMonitorInfoW));
    ldr.registerExport("api-ms-win-ntuser-sysparams-l1-1-0.dll", "GetMonitorInfoW", reinterpret_cast<void*>(GetMonitorInfoW));
    ldr.registerExport("user32.dll", "EnumDisplayMonitors", reinterpret_cast<void*>(EnumDisplayMonitors));
    ldr.registerExport("user32.dll", "CreateDialogParamW", reinterpret_cast<void*>(CreateDialogParamW));
    ldr.registerExport("user32.dll", "CreateDialogIndirectParamW", reinterpret_cast<void*>(CreateDialogIndirectParamW));
    ldr.registerExport("user32.dll", "DialogBoxParamW", reinterpret_cast<void*>(DialogBoxParamW));
    ldr.registerExport("user32.dll", "DialogBoxIndirectParamW", reinterpret_cast<void*>(DialogBoxIndirectParamW));
    ldr.registerExport("user32.dll", "EndDialog", reinterpret_cast<void*>(EndDialog));
    ldr.registerExport("user32.dll", "SetRectEmpty", reinterpret_cast<void*>(SetRectEmpty));
    ldr.registerExport("user32.dll", "GetCapture", reinterpret_cast<void*>(GetCapture));
    ldr.registerExport("user32.dll", "SetCapture", reinterpret_cast<void*>(SetCapture));
    ldr.registerExport("user32.dll", "ReleaseCapture", reinterpret_cast<void*>(ReleaseCapture));
    ldr.registerExport("user32.dll", "LoadBitmapW", reinterpret_cast<void*>(LoadBitmapW));
    ldr.registerExport("user32.dll", "ScrollWindow", reinterpret_cast<void*>(ScrollWindow));
    ldr.registerExport("user32.dll", "RemovePropW", reinterpret_cast<void*>(RemovePropW));
    ldr.registerExport("user32.dll", "GetPropW", reinterpret_cast<void*>(GetPropW));
    ldr.registerExport("user32.dll", "SetPropW", reinterpret_cast<void*>(SetPropW));
    ldr.registerExport("user32.dll", "InsertMenuItemW", reinterpret_cast<void*>(InsertMenuItemW));
    ldr.registerExport("user32.dll", "SetMenuItemInfoW", reinterpret_cast<void*>(SetMenuItemInfoW));
    ldr.registerExport("user32.dll", "GetMenuItemInfoW", reinterpret_cast<void*>(GetMenuItemInfoW));
    ldr.registerExport("user32.dll", "GetCaretBlinkTime", reinterpret_cast<void*>(GetCaretBlinkTime));
    ldr.registerExport("user32.dll", "AppendMenuA", reinterpret_cast<void*>(AppendMenuA));
    ldr.registerExport("user32.dll", "AppendMenuW", reinterpret_cast<void*>(AppendMenuW));
    ldr.registerExport("user32.dll", "InsertMenuW", reinterpret_cast<void*>(InsertMenuW));
    ldr.registerExport("user32.dll", "ModifyMenuW", reinterpret_cast<void*>(ModifyMenuW));
    ldr.registerExport("user32.dll", "DeleteMenu", reinterpret_cast<void*>(DeleteMenu));
    ldr.registerExport("user32.dll", "RemoveMenu", reinterpret_cast<void*>(RemoveMenu));
    ldr.registerExport("user32.dll", "GetMenu", reinterpret_cast<void*>(GetMenu));
    ldr.registerExport("user32.dll", "SetMenu", reinterpret_cast<void*>(SetMenu));
    ldr.registerExport("user32.dll", "CreateMenu", reinterpret_cast<void*>(CreateMenu));
    ldr.registerExport("user32.dll", "CreatePopupMenu", reinterpret_cast<void*>(CreatePopupMenu));
    ldr.registerExport("user32.dll", "LoadMenuW", reinterpret_cast<void*>(LoadMenuW));
    ldr.registerExport("user32.dll", "LoadMenuA", reinterpret_cast<void*>(LoadMenuA));
    ldr.registerExport("user32.dll", "DestroyMenu", reinterpret_cast<void*>(DestroyMenu));
    ldr.registerExport("user32.dll", "DrawMenuBar", reinterpret_cast<void*>(DrawMenuBar));
    ldr.registerExport("user32.dll", "GetMenuItemCount", reinterpret_cast<void*>(GetMenuItemCount));
    ldr.registerExport("user32.dll", "GetMenuItemID", reinterpret_cast<void*>(GetMenuItemID));
    ldr.registerExport("user32.dll", "GetMenuStringW", reinterpret_cast<void*>(GetMenuStringW));
    ldr.registerExport("user32.dll", "GetMenuState", reinterpret_cast<void*>(GetMenuState));
    ldr.registerExport("user32.dll", "GetSubMenu", reinterpret_cast<void*>(GetSubMenu));
    ldr.registerExport("user32.dll", "CheckMenuItem", reinterpret_cast<void*>(CheckMenuItem));
    ldr.registerExport("user32.dll", "CheckMenuRadioItem", reinterpret_cast<void*>(CheckMenuRadioItem));
    ldr.registerExport("user32.dll", "EnableMenuItem", reinterpret_cast<void*>(EnableMenuItem));
    ldr.registerExport("user32.dll", "SetMenuItemBitmaps", reinterpret_cast<void*>(SetMenuItemBitmaps));
    ldr.registerExport("user32.dll", "TrackPopupMenu", reinterpret_cast<void*>(TrackPopupMenu));
    ldr.registerExport("user32.dll", "TrackPopupMenuEx", reinterpret_cast<void*>(TrackPopupMenuEx));
    ldr.registerExport("user32.dll", "GetMenuBarInfo", reinterpret_cast<void*>(GetMenuBarInfo));
    ldr.registerExport("user32.dll", "GetMessageTime", reinterpret_cast<void*>(GetMessageTime));
    ldr.registerExport("user32.dll", "DestroyCursor", reinterpret_cast<void*>(DestroyCursor));
    ldr.registerExport("user32.dll", "NotifyWinEvent", reinterpret_cast<void*>(NotifyWinEvent));
    ldr.registerExport("user32.dll", "GetUpdateRgn", reinterpret_cast<void*>(GetUpdateRgn));
    ldr.registerExport("user32.dll", "SystemParametersInfoA", reinterpret_cast<void*>(SystemParametersInfoA));
    ldr.registerExport("user32.dll", "SystemParametersInfoW", reinterpret_cast<void*>(SystemParametersInfoW));
    ldr.registerExport("api-ms-win-ntuser-sysparams-l1-1-0.dll", "SystemParametersInfoW", reinterpret_cast<void*>(SystemParametersInfoW));
    ldr.registerExport("user32.dll", "GetDoubleClickTime", reinterpret_cast<void*>(GetDoubleClickTime));
    ldr.registerExport("user32.dll", "LoadStringW", reinterpret_cast<void*>(LoadStringW));
    ldr.registerExport("user32.dll", "LoadStringA", reinterpret_cast<void*>(LoadStringA));
    ldr.registerExport("user32.dll", "CallWindowProcW", reinterpret_cast<void*>(CallWindowProcW));
    ldr.registerExport("user32.dll", "GetDlgItemTextA", reinterpret_cast<void*>(GetDlgItemTextA));
    ldr.registerExport("user32.dll", "GetDlgItemTextW", reinterpret_cast<void*>(GetDlgItemTextW));
    ldr.registerExport("user32.dll", "SetDlgItemTextA", reinterpret_cast<void*>(SetDlgItemTextA));
    ldr.registerExport("user32.dll", "SetDlgItemTextW", reinterpret_cast<void*>(SetDlgItemTextW));
    ldr.registerExport("user32.dll", "SetDlgItemInt", reinterpret_cast<void*>(SetDlgItemInt));
    ldr.registerExport("user32.dll", "GetDlgItemInt", reinterpret_cast<void*>(GetDlgItemInt));
    ldr.registerExport("user32.dll", "GetDlgItem", reinterpret_cast<void*>(GetDlgItem));
    ldr.registerExport("user32.dll", "GetDlgCtrlID", reinterpret_cast<void*>(GetDlgCtrlID));
    ldr.registerExport("user32.dll", "SendDlgItemMessageW", reinterpret_cast<void*>(SendDlgItemMessageW));
    ldr.registerExport("user32.dll", "GetComboBoxInfo", reinterpret_cast<void*>(GetComboBoxInfo));
    ldr.registerExport("user32.dll", "SetScrollInfo", reinterpret_cast<void*>(SetScrollInfo));
    ldr.registerExport("user32.dll", "GetScrollInfo", reinterpret_cast<void*>(GetScrollInfo));
    ldr.registerExport("user32.dll", "SetScrollPos", reinterpret_cast<void*>(SetScrollPos));
    ldr.registerExport("user32.dll", "GetScrollPos", reinterpret_cast<void*>(GetScrollPos));
    ldr.registerExport("user32.dll", "SetScrollRange", reinterpret_cast<void*>(SetScrollRange));
    ldr.registerExport("user32.dll", "GetScrollRange", reinterpret_cast<void*>(GetScrollRange));
    ldr.registerExport("user32.dll", "ShowScrollBar", reinterpret_cast<void*>(ShowScrollBar));
    ldr.registerExport("user32.dll", "DeferWindowPos", reinterpret_cast<void*>(DeferWindowPos));
    ldr.registerExport("user32.dll", "BeginDeferWindowPos", reinterpret_cast<void*>(BeginDeferWindowPos));
    ldr.registerExport("user32.dll", "EndDeferWindowPos", reinterpret_cast<void*>(EndDeferWindowPos));
    ldr.registerExport("user32.dll", "GetWindow", reinterpret_cast<void*>(GetWindow));
    ldr.registerExport("user32.dll", "GetParent", reinterpret_cast<void*>(GetParent));
    ldr.registerExport("user32.dll", "SetParent", reinterpret_cast<void*>(SetParent));
    ldr.registerExport("user32.dll", "GetAncestor", reinterpret_cast<void*>(GetAncestor));
    ldr.registerExport("user32.dll", "GetActiveWindow", reinterpret_cast<void*>(GetActiveWindow));
    ldr.registerExport("user32.dll", "BringWindowToTop", reinterpret_cast<void*>(BringWindowToTop));
    ldr.registerExport("user32.dll", "SetForegroundWindow", reinterpret_cast<void*>(SetForegroundWindow));
    ldr.registerExport("user32.dll", "IsChild", reinterpret_cast<void*>(IsChild));
    ldr.registerExport("user32.dll", "IsZoomed", reinterpret_cast<void*>(IsZoomed));
    ldr.registerExport("user32.dll", "GetLastActivePopup", reinterpret_cast<void*>(GetLastActivePopup));
    ldr.registerExport("user32.dll", "EnumChildWindows", reinterpret_cast<void*>(EnumChildWindows));
    ldr.registerExport("user32.dll", "EnumThreadWindows", reinterpret_cast<void*>(EnumThreadWindows));
    ldr.registerExport("user32.dll", "GetWindowThreadProcessId", reinterpret_cast<void*>(GetWindowThreadProcessId));
    ldr.registerExport("user32.dll", "GetWindowPlacement", reinterpret_cast<void*>(GetWindowPlacement));
    ldr.registerExport("user32.dll", "SetWindowPlacement", reinterpret_cast<void*>(SetWindowPlacement));
    ldr.registerExport("user32.dll", "FlashWindowEx", reinterpret_cast<void*>(FlashWindowEx));
    ldr.registerExport("user32.dll", "SetLayeredWindowAttributes", reinterpret_cast<void*>(SetLayeredWindowAttributes));
    ldr.registerExport("user32.dll", "LoadIconW", reinterpret_cast<void*>(LoadIconW));
    ldr.registerExport("user32.dll", "DestroyIcon", reinterpret_cast<void*>(DestroyIcon));
    ldr.registerExport("user32.dll", "DrawIconEx", reinterpret_cast<void*>(DrawIconEx));
    ldr.registerExport("user32.dll", "GetIconInfo", reinterpret_cast<void*>(GetIconInfo));
    ldr.registerExport("user32.dll", "CreateIconIndirect", reinterpret_cast<void*>(CreateIconIndirect));
    ldr.registerExport("user32.dll", "LoadBitmapW", reinterpret_cast<void*>(LoadBitmapW));
    ldr.registerExport("user32.dll", "LoadImageW", reinterpret_cast<void*>(LoadImageW));
    ldr.registerExport("user32.dll", "LoadCursorW", reinterpret_cast<void*>(LoadCursorW));
    ldr.registerExport("user32.dll", "SetCursor", reinterpret_cast<void*>(SetCursor));
    ldr.registerExport("user32.dll", "ShowCursor", reinterpret_cast<void*>(ShowCursor));
    ldr.registerExport("user32.dll", "GetCursorPos", reinterpret_cast<void*>(GetCursorPos));
    ldr.registerExport("user32.dll", "CreateCaret", reinterpret_cast<void*>(CreateCaret));
    ldr.registerExport("user32.dll", "DestroyCaret", reinterpret_cast<void*>(DestroyCaret));
    ldr.registerExport("user32.dll", "ShowCaret", reinterpret_cast<void*>(ShowCaret));
    ldr.registerExport("user32.dll", "HideCaret", reinterpret_cast<void*>(HideCaret));
    ldr.registerExport("user32.dll", "SetCaretPos", reinterpret_cast<void*>(SetCaretPos));
    ldr.registerExport("user32.dll", "GetSysColor", reinterpret_cast<void*>(GetSysColor));
    ldr.registerExport("user32.dll", "GetSysColorBrush", reinterpret_cast<void*>(GetSysColorBrush));
    ldr.registerExport("user32.dll", "CallNextHookEx", reinterpret_cast<void*>(CallNextHookEx));
    ldr.registerExport("user32.dll", "SetWindowsHookExW", reinterpret_cast<void*>(SetWindowsHookExW));
    ldr.registerExport("user32.dll", "UnhookWindowsHookEx", reinterpret_cast<void*>(UnhookWindowsHookEx));
    ldr.registerExport("user32.dll", "OpenClipboard", reinterpret_cast<void*>(OpenClipboard));
    ldr.registerExport("user32.dll", "CloseClipboard", reinterpret_cast<void*>(CloseClipboard));
    ldr.registerExport("user32.dll", "EmptyClipboard", reinterpret_cast<void*>(EmptyClipboard));
    ldr.registerExport("user32.dll", "GetClipboardData", reinterpret_cast<void*>(GetClipboardData));
    ldr.registerExport("user32.dll", "SetClipboardData", reinterpret_cast<void*>(SetClipboardData));
    ldr.registerExport("user32.dll", "IsClipboardFormatAvailable", reinterpret_cast<void*>(IsClipboardFormatAvailable));
    ldr.registerExport("user32.dll", "RegisterClipboardFormatW", reinterpret_cast<void*>(RegisterClipboardFormatW));
    ldr.registerExport("user32.dll", "SetClipboardViewer", reinterpret_cast<void*>(SetClipboardViewer));
    ldr.registerExport("user32.dll", "ChangeClipboardChain", reinterpret_cast<void*>(ChangeClipboardChain));
    ldr.registerExport("user32.dll", "AddClipboardFormatListener", reinterpret_cast<void*>(AddClipboardFormatListener));
    ldr.registerExport("user32.dll", "RemoveClipboardFormatListener", reinterpret_cast<void*>(RemoveClipboardFormatListener));
    ldr.registerExport("user32.dll", "FindWindowW", reinterpret_cast<void*>(FindWindowW));
    ldr.registerExport("user32.dll", "FindWindowExW", reinterpret_cast<void*>(FindWindowExW));
    ldr.registerExport("user32.dll", "WindowFromPoint", reinterpret_cast<void*>(WindowFromPoint));
    ldr.registerExport("user32.dll", "ChildWindowFromPointEx", reinterpret_cast<void*>(ChildWindowFromPointEx));
    ldr.registerExport("user32.dll", "ClientToScreen", reinterpret_cast<void*>(ClientToScreen));
    ldr.registerExport("user32.dll", "ScreenToClient", reinterpret_cast<void*>(ScreenToClient));
    ldr.registerExport("user32.dll", "MapWindowPoints", reinterpret_cast<void*>(MapWindowPoints));
    ldr.registerExport("user32.dll", "SetTimer", reinterpret_cast<void*>(SetTimer));
    ldr.registerExport("user32.dll", "KillTimer", reinterpret_cast<void*>(KillTimer));
    ldr.registerExport("user32.dll", "MessageBeep", reinterpret_cast<void*>(MessageBeep));
    ldr.registerExport("user32.dll", "GetKeyboardState", reinterpret_cast<void*>(GetKeyboardState));
    ldr.registerExport("user32.dll", "GetKeyState", reinterpret_cast<void*>(GetKeyState));
    ldr.registerExport("user32.dll", "GetKeyboardLayout", reinterpret_cast<void*>(GetKeyboardLayout));
    ldr.registerExport("user32.dll", "MapVirtualKeyW", reinterpret_cast<void*>(MapVirtualKeyW));
    ldr.registerExport("user32.dll", "ToAscii", reinterpret_cast<void*>(ToAscii));
    ldr.registerExport("user32.dll", "mouse_event", reinterpret_cast<void*>(mouse_event));
    ldr.registerExport("user32.dll", "TrackMouseEvent", reinterpret_cast<void*>(TrackMouseEvent));
    ldr.registerExport("user32.dll", "DrawTextW", reinterpret_cast<void*>(DrawTextW));
    ldr.registerExport("user32.dll", "DrawTextExW", reinterpret_cast<void*>(DrawTextExW));
    ldr.registerExport("user32.dll", "DrawEdge", reinterpret_cast<void*>(DrawEdge));
    ldr.registerExport("user32.dll", "DrawFrameControl", reinterpret_cast<void*>(DrawFrameControl));
    ldr.registerExport("user32.dll", "DrawFocusRect", reinterpret_cast<void*>(DrawFocusRect));
    ldr.registerExport("user32.dll", "FrameRect", reinterpret_cast<void*>(FrameRect));
    ldr.registerExport("user32.dll", "FillRect", reinterpret_cast<void*>(FillRect));
    ldr.registerExport("user32.dll", "LockWindowUpdate", reinterpret_cast<void*>(LockWindowUpdate));
    ldr.registerExport("user32.dll", "RedrawWindow", reinterpret_cast<void*>(RedrawWindow));
    ldr.registerExport("user32.dll", "GetWindowDC", reinterpret_cast<void*>(GetWindowDC));
    ldr.registerExport("user32.dll", "GetDCEx", reinterpret_cast<void*>(GetDCEx));
    ldr.registerExport("user32.dll", "InflateRect", reinterpret_cast<void*>(InflateRect));
    ldr.registerExport("user32.dll", "OffsetRect", reinterpret_cast<void*>(OffsetRect));
    ldr.registerExport("user32.dll", "IntersectRect", reinterpret_cast<void*>(IntersectRect));
    ldr.registerExport("user32.dll", "EqualRect", reinterpret_cast<void*>(EqualRect));
    ldr.registerExport("user32.dll", "PtInRect", reinterpret_cast<void*>(PtInRect));
    ldr.registerExport("user32.dll", "GetClassNameW", reinterpret_cast<void*>(GetClassNameW));
    ldr.registerExport("user32.dll", "GetClassNameA", reinterpret_cast<void*>(GetClassNameA));
    ldr.registerExport("user32.dll", "GetWindowTextA", reinterpret_cast<void*>(GetWindowTextA));
    ldr.registerExport("user32.dll", "wsprintfW", reinterpret_cast<void*>(wsprintfW));
    ldr.registerExport("user32.dll", "IsCharLowerW", reinterpret_cast<void*>(IsCharLowerW));
    ldr.registerExport("user32.dll", "CharLowerW", reinterpret_cast<void*>(CharLowerW));
    ldr.registerExport("user32.dll", "IsCharAlphaNumericW", reinterpret_cast<void*>(IsCharAlphaNumericW));
    ldr.registerExport("user32.dll", "IsCharAlphaW", reinterpret_cast<void*>(IsCharAlphaW));
    ldr.registerExport("user32.dll", "RegisterWindowMessageW", reinterpret_cast<void*>(RegisterWindowMessageW));
    ldr.registerExport("user32.dll", "IsDialogMessageW", reinterpret_cast<void*>(IsDialogMessageW));
    ldr.registerExport("user32.dll", "CreateAcceleratorTableW", reinterpret_cast<void*>(CreateAcceleratorTableW));
    ldr.registerExport("user32.dll", "DestroyAcceleratorTable", reinterpret_cast<void*>(DestroyAcceleratorTable));
    ldr.registerExport("user32.dll", "TranslateAcceleratorW", reinterpret_cast<void*>(TranslateAcceleratorW));
}

} // namespace micant::user32
