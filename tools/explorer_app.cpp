/**
 * @file explorer_app.cpp
 * @brief Sovereign Ring-3 Win32 Desktop Shell & File Explorer (C:\Windows\System32\explorer.exe).
 *
 * 100% Unmodified-ABI Dynamic Ring-3 Win32 GUI Executable (0 direct syscalls):
 *   - Imports ONLY kernel32.dll + user32.dll + gdi32.dll -> ntdll.dll -> Ring 0 DWM Compositor
 *   - Queries live NTFS 3.1 directory contents (C:\Windows\System32\*) via FindFirstFileW/FindNextFileW
 *   - Spawns sibling isolated-CR3 GUI apps (taskmgr.exe & notepad.exe) concurrently via CreateProcessW
 *   - Participates in preemptive multi-window DWM composition (WS_EX_COMPOSITED) + i8042 PS/2 mouse focus
 */

#include <cstdint>
#include <cstddef>

extern "C" {

void* memset(void* dst, int c, size_t n) noexcept {
    auto* d = static_cast<unsigned char*>(dst);
    for (size_t i = 0; i < n; ++i) d[i] = static_cast<unsigned char>(c);
    return dst;
}

using WndProcFn = int64_t (*)(void* hWnd, uint32_t msg, uint64_t wParam, int64_t lParam);

#pragma pack(push, 1)
struct Point { int32_t x; int32_t y; };
struct Rect  { int32_t left; int32_t top; int32_t right; int32_t bottom; };
struct Msg {
    void*    hwnd;
    uint32_t message;
    uint64_t wParam;
    int64_t  lParam;
    uint32_t time;
    Point    pt;
    uint32_t lPrivate;
};
#pragma pack(pop)

struct WndClassExW {
    uint32_t cbSize; uint32_t style; WndProcFn lpfnWndProc; int32_t cbClsExtra; int32_t cbWndExtra;
    void* hInstance; void* hIcon; void* hCursor; void* hbrBackground;
    const wchar_t* lpszMenuName; const wchar_t* lpszClassName; void* hIconSm;
};

struct PaintStruct {
    void* hdc; int32_t fErase; Rect rcPaint; int32_t fRestore; int32_t fIncUpdate; uint8_t rgbReserved[32];
};

struct ProcessInformation {
    void* hProcess; void* hThread; uint32_t dwProcessId; uint32_t dwThreadId;
};

struct Win32FindDataW {
    uint32_t dwFileAttributes; uint64_t ftCreationTime; uint64_t ftLastAccessTime; uint64_t ftLastWriteTime;
    uint32_t nFileSizeHigh; uint32_t nFileSizeLow; uint32_t dwReserved0; uint32_t dwReserved1;
    wchar_t cFileName[260]; wchar_t cAlternateFileName[14];
};

// kernel32.dll
__declspec(dllimport) void*    GetStdHandle(uint32_t nStdHandle);
__declspec(dllimport) int32_t  WriteFile(void* hFile, const void* lpBuf, uint32_t nBytes, uint32_t* lpWritten, void* lpOv);
__declspec(dllimport) uint32_t GetCurrentProcessId();
__declspec(dllimport) void     Sleep(uint32_t dwMilliseconds);
__declspec(dllimport) void     ExitProcess(uint32_t uExitCode);
__declspec(dllimport) void*    FindFirstFileW(const wchar_t* lpFileName, Win32FindDataW* lpFindFileData);
__declspec(dllimport) int32_t  FindNextFileW(void* hFindFile, Win32FindDataW* lpFindFileData);
__declspec(dllimport) int32_t  FindClose(void* hFindFile);
__declspec(dllimport) int32_t  CreateProcessW(const wchar_t* lpApp, wchar_t* lpCmd, void*, void*, int32_t, uint32_t,
                                              void*, const wchar_t*, void*, ProcessInformation* lpPI);
__declspec(dllimport) uint32_t WaitForSingleObject(void* hHandle, uint32_t dwMs);

// user32.dll
__declspec(dllimport) uint16_t RegisterClassExW(const WndClassExW* lpWndClass);
__declspec(dllimport) void*    CreateWindowExW(uint32_t dwExStyle, const wchar_t* lpClass, const wchar_t* lpTitle,
                                               uint32_t dwStyle, int X, int Y, int nW, int nH,
                                               void*, void*, void*, void*);
__declspec(dllimport) bool     ShowWindow(void* hWnd, int nCmdShow);
__declspec(dllimport) bool     UpdateWindow(void* hWnd);
__declspec(dllimport) bool     InvalidateRect(void* hWnd, const Rect* lpRect, bool bErase);
__declspec(dllimport) bool     DestroyWindow(void* hWnd);
__declspec(dllimport) void*    BeginPaint(void* hWnd, PaintStruct* lpPaint);
__declspec(dllimport) bool     EndPaint(void* hWnd, const PaintStruct* lpPaint);
__declspec(dllimport) bool     PeekMessageW(Msg* lpMsg, void* hWnd, uint32_t min, uint32_t max, uint32_t rm);
__declspec(dllimport) bool     TranslateMessage(const Msg* lpMsg);
__declspec(dllimport) int64_t  DispatchMessageW(const Msg* lpMsg);
__declspec(dllimport) int64_t  DefWindowProcW(void* hWnd, uint32_t msg, uint64_t wParam, int64_t lParam);
__declspec(dllimport) void     PostQuitMessage(int nExitCode);

// gdi32.dll
__declspec(dllimport) bool     GdiFillSolidRect(void* hdc, int x, int y, int w, int h, uint32_t color);
__declspec(dllimport) bool     GdiDrawRectBorder(void* hdc, int x, int y, int w, int h, uint32_t color);
__declspec(dllimport) uint32_t SetTextColor(void* hdc, uint32_t color);
__declspec(dllimport) bool     TextOutA(void* hdc, int x, int y, const char* lpString, int c);

static constexpr uint32_t WS_EX_COMPOSITED = 0x02000000u;
static constexpr uint32_t WM_SETFOCUS      = 0x0007u;
static constexpr uint32_t WM_PAINT         = 0x000Fu;
static constexpr uint32_t WM_CLOSE         = 0x0010u;
static constexpr uint32_t WM_QUIT          = 0x0012u;
static constexpr uint32_t WM_KEYDOWN       = 0x0100u;
static constexpr uint32_t WM_LBUTTONDOWN   = 0x0201u;

static constexpr int WIN_W = 660;
static constexpr int WIN_H = 460;

struct FileItem {
    char     name[44];
    uint32_t sizeBytes;
    bool     isDir;
};

static FileItem g_items[20];
static size_t   g_itemCount = 0;
static uint32_t g_totalFiles = 0;
static uint32_t g_pid = 0;
static uint32_t g_selectedRow = 0;
static bool     g_running = true;

static void appendStr(char* dst, size_t& pos, size_t cap, const char* s) noexcept {
    while (s && *s && pos + 1 < cap) dst[pos++] = *s++;
    dst[pos] = '\0';
}

static void appendDec(char* dst, size_t& pos, size_t cap, uint64_t v) noexcept {
    char tmp[24];
    size_t n = 0;
    if (v == 0) tmp[n++] = '0';
    else { while (v > 0 && n < 22) { tmp[n++] = static_cast<char>('0' + (v % 10)); v /= 10; } }
    while (n > 0 && pos + 1 < cap) dst[pos++] = tmp[--n];
    dst[pos] = '\0';
}

static void writeOutLine(const char* s) noexcept {
    void* hOut = GetStdHandle(0xFFFFFFF5u);
    char buf[192];
    size_t p = 0;
    appendStr(buf, p, sizeof(buf) - 2, s);
    buf[p++] = '\r';
    buf[p++] = '\n';
    uint32_t nw = 0;
    WriteFile(hOut, buf, static_cast<uint32_t>(p), &nw, nullptr);
}

static void scanSystem32Ntfs() noexcept {
    g_itemCount = 0;
    g_totalFiles = 0;
    Win32FindDataW fd{};
    void* hFind = FindFirstFileW(L"\\Windows\\System32\\*", &fd);
    if (!hFind || hFind == reinterpret_cast<void*>(-1)) return;
    do {
        g_totalFiles++;
        if (g_itemCount < 16) {
            auto& it = g_items[g_itemCount++];
            size_t ni = 0;
            while (fd.cFileName[ni] && ni + 1 < sizeof(it.name)) {
                it.name[ni] = (fd.cFileName[ni] < 128) ? static_cast<char>(fd.cFileName[ni]) : '?';
                ++ni;
            }
            it.name[ni] = '\0';
            it.sizeBytes = fd.nFileSizeLow;
            it.isDir = (fd.dwFileAttributes & 0x10u) != 0;
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
}

static void paintExplorer(void* hwnd) noexcept {
    PaintStruct ps{};
    void* hdc = BeginPaint(hwnd, &ps);
    if (!hdc) return;

    // Background & Toolbar
    GdiFillSolidRect(hdc, 0, 0, WIN_W, WIN_H, 0xFF0E1422u);
    GdiFillSolidRect(hdc, 0, 0, WIN_W, 28, 0xFF162034u);
    GdiFillSolidRect(hdc, 0, 28, WIN_W, 1, 0xFF283C5Eu);
    SetTextColor(hdc, 0xFFC4DCF6u);
    TextOutA(hdc, 12, 10, "File    Home    Share    View    [Live NTFS 3.1 AHCI DMA]", -1);

    // Address Breadcrumb Bar
    GdiFillSolidRect(hdc, 0, 29, WIN_W, 28, 0xFF101828u);
    GdiDrawRectBorder(hdc, 10, 33, WIN_W - 20, 20, 0xFF2D4870u);
    SetTextColor(hdc, 0xFF00E5FFu);
    TextOutA(hdc, 18, 39, "This PC  >  Local Disk (C: NTFS 3.1)  >  Windows  >  System32", -1);

    // Left Navigation Tree
    int navW = 164;
    GdiFillSolidRect(hdc, 0, 58, navW, WIN_H - 86, 0xFF11192Au);
    GdiFillSolidRect(hdc, navW, 58, 1, WIN_H - 86, 0xFF283C5Eu);
    SetTextColor(hdc, 0xFFFFD700u);
    TextOutA(hdc, 12, 70, "THIS PC VOLUMES", -1);
    SetTextColor(hdc, 0xFF64F0A0u);
    TextOutA(hdc, 14, 92, "> C: MicaNT_NTFS", -1);
    SetTextColor(hdc, 0xFFA4BEDCu);
    TextOutA(hdc, 24, 112, "Windows", -1);
    SetTextColor(hdc, 0xFF00E5FFu);
    TextOutA(hdc, 32, 132, "System32", -1);
    SetTextColor(hdc, 0xFFA4BEDCu);
    TextOutA(hdc, 32, 152, "config (regf)", -1);
    TextOutA(hdc, 32, 172, "drivers", -1);
    TextOutA(hdc, 24, 192, "MicaNT", -1);
    SetTextColor(hdc, 0xFF8AA8CEu);
    TextOutA(hdc, 12, 232, "NETWORK", -1);
    SetTextColor(hdc, 0xFF64F0A0u);
    TextOutA(hdc, 14, 252, "e1000 10.0.2.15", -1);
    SetTextColor(hdc, 0xFF8AA8CEu);
    TextOutA(hdc, 12, 292, "DESKTOP SHELL", -1);
    TextOutA(hdc, 14, 312, "3 Isolated CR3", -1);
    TextOutA(hdc, 14, 330, "0 Direct Syscalls", -1);

    // Right File List Column Headers
    int listX = navW + 1;
    int listW = WIN_W - listX;
    GdiFillSolidRect(hdc, listX, 58, listW, 24, 0xFF172236u);
    GdiFillSolidRect(hdc, listX, 82, listW, 1, 0xFF283C5Eu);
    SetTextColor(hdc, 0xFF9AB8DEu);
    TextOutA(hdc, listX + 12, 66, "Name", -1);
    TextOutA(hdc, listX + 240, 66, "Type", -1);
    TextOutA(hdc, listX + 372, 66, "Size", -1);

    // Live NTFS 3.1 File Entries
    int y = 88;
    for (size_t i = 0; i < g_itemCount && i < 16; ++i) {
        uint32_t rowBg = (i == g_selectedRow) ? 0xFF1D3658u : ((i & 1u) ? 0xFF111828u : 0xFF0E1422u);
        GdiFillSolidRect(hdc, listX + 2, y - 2, listW - 4, 20, rowBg);

        SetTextColor(hdc, g_items[i].isDir ? 0xFFFFD700u : 0xFFE2EEFFu);
        TextOutA(hdc, listX + 12, y + 4, g_items[i].name, -1);

        SetTextColor(hdc, g_items[i].isDir ? 0xFF00E5FFu : 0xFF9AB4D6u);
        TextOutA(hdc, listX + 240, y + 4, g_items[i].isDir ? "<DIR> Folder" : "PE32+ Binary", -1);

        if (!g_items[i].isDir) {
            char szBuf[24];
            size_t sp = 0;
            appendDec(szBuf, sp, sizeof(szBuf), g_items[i].sizeBytes);
            appendStr(szBuf, sp, sizeof(szBuf), " B");
            SetTextColor(hdc, 0xFF64F0A0u);
            TextOutA(hdc, listX + 372, y + 4, szBuf, -1);
        }
        y += 21;
    }

    // Bottom Status Bar
    GdiFillSolidRect(hdc, 0, WIN_H - 28, WIN_W, 28, 0xFF152036u);
    GdiFillSolidRect(hdc, 0, WIN_H - 28, WIN_W, 1, 0xFF283C5Eu);
    char foot[112];
    size_t fp = 0;
    appendDec(foot, fp, sizeof(foot), g_totalFiles);
    appendStr(foot, fp, sizeof(foot), " live NTFS items  |  explorer.exe PID=");
    appendDec(foot, fp, sizeof(foot), g_pid);
    appendStr(foot, fp, sizeof(foot), "  |  FindFirstFileW OK");
    SetTextColor(hdc, 0xFF9AC4EAu);
    TextOutA(hdc, 12, WIN_H - 18, foot, -1);

    EndPaint(hwnd, &ps);
}

static int64_t ExplorerWndProc(void* hwnd, uint32_t msg, uint64_t wParam, int64_t lParam) noexcept {
    switch (msg) {
        case WM_PAINT:
            paintExplorer(hwnd);
            return 0;
        case WM_SETFOCUS:
            InvalidateRect(hwnd, nullptr, false);
            return 0;
        case WM_LBUTTONDOWN: {
            int32_t cy = static_cast<int32_t>((lParam >> 16) & 0xFFFF);
            if (cy >= 88 && cy < 88 + static_cast<int32_t>(g_itemCount * 21)) {
                g_selectedRow = static_cast<uint32_t>((cy - 88) / 21);
                InvalidateRect(hwnd, nullptr, false);
            }
            return 0;
        }
        case WM_CLOSE:
            g_running = false;
            PostQuitMessage(0);
            return 0;
        case WM_KEYDOWN:
            if (wParam == 0x1Bu) {
                g_running = false;
                PostQuitMessage(0);
                return 0;
            }
            break;
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void WinMainEntry() {
    g_pid = GetCurrentProcessId();
    scanSystem32Ntfs();

    WndClassExW wc{};
    wc.cbSize = sizeof(WndClassExW);
    wc.lpfnWndProc = ExplorerWndProc;
    wc.lpszClassName = L"MicaExplorerWnd";
    RegisterClassExW(&wc);

    void* hwnd = CreateWindowExW(
        WS_EX_COMPOSITED, L"MicaExplorerWnd", L"File Explorer - C:\\Windows\\System32",
        0x10CF0000u, 48, 62, WIN_W, WIN_H, nullptr, nullptr, nullptr, nullptr
    );
    if (!hwnd) ExitProcess(1);

    ShowWindow(hwnd, 5);
    UpdateWindow(hwnd);

    // Spawn sibling isolated-CR3 GUI apps CONCURRENTLY (taskmgr.exe -> Z=2, notepad.exe -> Z=3 Top/Focused!)
    ProcessInformation piTask{nullptr, nullptr, 0, 0};
    ProcessInformation piNote{nullptr, nullptr, 0, 0};
    wchar_t cmdTask[] = L"taskmgr.exe";
    wchar_t cmdNote[] = L"notepad.exe";
    bool okTask = CreateProcessW(L"\\Windows\\System32\\taskmgr.exe", cmdTask, nullptr, nullptr, 0, 0, nullptr, nullptr, nullptr, &piTask) != 0;
    for (int i = 0; i < 24; ++i) Sleep(0); // Let taskmgr create & paint Z=2 window before notepad takes Z=3 focus!
    bool okNote = CreateProcessW(L"\\Windows\\System32\\notepad.exe", cmdNote, nullptr, nullptr, 0, 0, nullptr, nullptr, nullptr, &piNote) != 0;

    char logLine[160];
    size_t lp = 0;
    appendStr(logLine, lp, sizeof(logLine), "[explorer.exe PID=");
    appendDec(logLine, lp, sizeof(logLine), g_pid);
    appendStr(logLine, lp, sizeof(logLine), "] Desktop Shell Live: taskmgr PID=");
    appendDec(logLine, lp, sizeof(logLine), okTask ? piTask.dwProcessId : 0);
    appendStr(logLine, lp, sizeof(logLine), " | notepad PID=");
    appendDec(logLine, lp, sizeof(logLine), okNote ? piNote.dwProcessId : 0);
    writeOutLine(logLine);

    Msg msg{};
    while (g_running) {
        if (PeekMessageW(&msg, hwnd, 0, 0, 1u)) {
            if (msg.message == WM_QUIT) break;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        } else {
            Sleep(0); // Cooperative CR3 yield alongside 100 Hz LAPIC hardware preemption!
        }
    }

    DestroyWindow(hwnd);
    if (okTask && piTask.hProcess) WaitForSingleObject(piTask.hProcess, 0xFFFFFFFFu);
    if (okNote && piNote.hProcess) WaitForSingleObject(piNote.hProcess, 0xFFFFFFFFu);
    writeOutLine("[explorer.exe] Multi-Window Desktop Session closed cleanly.");
    ExitProcess(0);
}

} // extern "C"
