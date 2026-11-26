/**
 * @file win32_gui_app.cpp
 * @brief Sovereign Ring-3 Win32 GUI Application (C:\Windows\System32\win32_gui_app.exe).
 *
 * 100% Unmodified-ABI Win32 PE32+ GUI Executable:
 *   - ZERO MicaNT kernel headers
 *   - ZERO inline assembly
 *   - ZERO direct syscall instructions
 *   - Dynamically imports ONLY standard Win32 APIs across 3 System32 DLLs:
 *       kernel32.dll + user32.dll + gdi32.dll -> ntdll.dll -> Ring 0 Win32k Blitter
 *
 * Demonstrates live Ring-3 Win32 GUI composition on bare-metal 1920x1080 UEFI GOP:
 *   - RegisterClassExW + CreateWindowExW + ShowWindow + UpdateWindow
 *   - Private CR3 PFN-backed 32-bpp BGRA pixel surface allocated via gdi32.dll
 *   - Software-rasterized Mica window chrome + rotating 3D PrismX octahedron wireframe
 *   - Real-time hardware PS/2 WM_CHAR / WM_KEYDOWN event loop via GetMessageW/DispatchMessageW
 *   - EndPaint -> gdi32!BitBlt -> ntdll!NtGdiPresentSurface (SSN 0x81) VRAM burst blit
 */

#include <cstdint>
#include <cstddef>

extern "C" {

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
    uint32_t       cbSize;
    uint32_t       style;
    WndProcFn      lpfnWndProc;
    int32_t        cbClsExtra;
    int32_t        cbWndExtra;
    void*          hInstance;
    void*          hIcon;
    void*          hCursor;
    void*          hbrBackground;
    const wchar_t* lpszMenuName;
    const wchar_t* lpszClassName;
    void*          hIconSm;
};

struct PaintStruct {
    void*    hdc;
    int32_t  fErase;
    Rect     rcPaint;
    int32_t  fRestore;
    int32_t  fIncUpdate;
    uint8_t  rgbReserved[32];
};

// kernel32.dll imports
__declspec(dllimport) void*    GetStdHandle(uint32_t nStdHandle);
__declspec(dllimport) int32_t  WriteFile(void* hFile, const void* lpBuffer, uint32_t nBytes, uint32_t* lpWritten, void* lpOverlapped);
__declspec(dllimport) uint32_t GetCurrentProcessId();
__declspec(dllimport) void     ExitProcess(uint32_t uExitCode);

// user32.dll imports
__declspec(dllimport) int32_t  GetSystemMetrics(int32_t nIndex);
__declspec(dllimport) uint16_t RegisterClassExW(const WndClassExW* lpWndClass);
__declspec(dllimport) void*    CreateWindowExW(uint32_t dwExStyle, const wchar_t* lpClassName, const wchar_t* lpWindowName,
                                               uint32_t dwStyle, int X, int Y, int nWidth, int nHeight,
                                               void* hWndParent, void* hMenu, void* hInstance, void* lpParam);
__declspec(dllimport) bool     ShowWindow(void* hWnd, int nCmdShow);
__declspec(dllimport) bool     UpdateWindow(void* hWnd);
__declspec(dllimport) bool     InvalidateRect(void* hWnd, const Rect* lpRect, bool bErase);
__declspec(dllimport) bool     DestroyWindow(void* hWnd);
__declspec(dllimport) void*    BeginPaint(void* hWnd, PaintStruct* lpPaint);
__declspec(dllimport) bool     EndPaint(void* hWnd, const PaintStruct* lpPaint);
__declspec(dllimport) bool     GetMessageW(Msg* lpMsg, void* hWnd, uint32_t min, uint32_t max);
__declspec(dllimport) bool     TranslateMessage(const Msg* lpMsg);
__declspec(dllimport) int64_t  DispatchMessageW(const Msg* lpMsg);
__declspec(dllimport) int64_t  DefWindowProcW(void* hWnd, uint32_t msg, uint64_t wParam, int64_t lParam);
__declspec(dllimport) void     PostQuitMessage(int nExitCode);

// gdi32.dll imports
__declspec(dllimport) bool     GdiFillSolidRect(void* hdc, int x, int y, int w, int h, uint32_t color);
__declspec(dllimport) bool     GdiDrawRectBorder(void* hdc, int x, int y, int w, int h, uint32_t color);
__declspec(dllimport) uint32_t SetDCPenColor(void* hdc, uint32_t color);
__declspec(dllimport) uint32_t SetTextColor(void* hdc, uint32_t color);
__declspec(dllimport) bool     MoveToEx(void* hdc, int x, int y, void* lpPrev);
__declspec(dllimport) bool     LineTo(void* hdc, int x, int y);
__declspec(dllimport) bool     TextOutA(void* hdc, int x, int y, const char* lpString, int c);
__declspec(dllimport) bool     GdiTextOutScaledA(void* hdc, int x, int y, const char* str, int len, int scale);

static constexpr uint32_t WM_PAINT   = 0x000Fu;
static constexpr uint32_t WM_KEYDOWN = 0x0100u;
static constexpr uint32_t WM_CHAR    = 0x0102u;

static constexpr int WIN_W = 1080;
static constexpr int WIN_H = 620;

static uint32_t g_pid = 0;
static int32_t  g_screenW = 1920;
static int32_t  g_screenH = 1080;
static uint32_t g_framesPainted = 0;
static uint32_t g_keyEvents = 0;
static int32_t  g_rotStep = 0;
static char     g_typedText[96];
static size_t   g_typedLen = 0;

// Fixed-point sin/cos table (32 steps around 360 deg, scaled by 1000)
static const int32_t kSin32[32] = {
       0,  195,  383,  556,  707,  831,  924,  981,
    1000,  981,  924,  831,  707,  556,  383,  195,
       0, -195, -383, -556, -707, -831, -924, -981,
   -1000, -981, -924, -831, -707, -556, -383, -195
};

static size_t cstrlen(const char* s) noexcept {
    size_t n = 0;
    while (s[n]) ++n;
    return n;
}

static void appendStr(char* dst, size_t& pos, size_t cap, const char* s) noexcept {
    while (*s && pos + 1 < cap) dst[pos++] = *s++;
    dst[pos] = '\0';
}

static void appendDec(char* dst, size_t& pos, size_t cap, uint64_t val) noexcept {
    char tmp[24];
    size_t n = 0;
    if (val == 0) tmp[n++] = '0';
    else {
        while (val > 0 && n < 22) {
            tmp[n++] = static_cast<char>('0' + (val % 10));
            val /= 10;
        }
    }
    while (n > 0 && pos + 1 < cap) dst[pos++] = tmp[--n];
    dst[pos] = '\0';
}

static void drawPrismOctahedron(void* hdc, int cx, int cy, int step) noexcept {
    // 6 octahedron vertices in fixed-point * 1000: top, bottom, and 4 equator points
    int32_t s = kSin32[step & 31];
    int32_t c = kSin32[(step + 8) & 31];

    int32_t vx[6] = { 0, 0,  140,    0, -140,    0 };
    int32_t vy[6] = { -145, 145, 0, 0, 0, 0 };
    int32_t vz[6] = { 0, 0,    0,  140,    0, -140 };

    int px[6], py[6];
    for (int i = 0; i < 6; ++i) {
        int32_t rx = (vx[i] * c - vz[i] * s) / 1000;
        int32_t rz = (vx[i] * s + vz[i] * c) / 1000;
        int32_t ry = (vy[i] * 940 - rz * 340) / 1000;
        int32_t pz = (vy[i] * 340 + rz * 940) / 1000 + 460;
        if (pz < 120) pz = 120;
        px[i] = cx + (rx * 360) / pz;
        py[i] = cy + (ry * 360) / pz;
    }

    // Horizon grid inside viewport
    SetDCPenColor(hdc, 0xFF1C3050u);
    for (int gy = cy + 60; gy <= cy + 160; gy += 20) {
        MoveToEx(hdc, cx - 220, gy, nullptr);
        LineTo(hdc, cx + 220, gy);
    }
    for (int gx = -200; gx <= 200; gx += 40) {
        MoveToEx(hdc, cx + gx / 2, cy + 60, nullptr);
        LineTo(hdc, cx + gx, cy + 160);
    }

    // Top pyramid edges (Barrer Cyan)
    SetDCPenColor(hdc, 0xFF00E5FFu);
    for (int e = 2; e <= 5; ++e) {
        MoveToEx(hdc, px[0], py[0], nullptr);
        LineTo(hdc, px[e], py[e]);
    }
    // Equator ring (Spectral Amber / Gold)
    SetDCPenColor(hdc, 0xFFFFD700u);
    for (int e = 0; e < 4; ++e) {
        int a = 2 + e;
        int b = 2 + ((e + 1) & 3);
        MoveToEx(hdc, px[a], py[a], nullptr);
        LineTo(hdc, px[b], py[b]);
    }
    // Bottom pyramid edges (Prism Violet)
    SetDCPenColor(hdc, 0xFFB060FFu);
    for (int e = 2; e <= 5; ++e) {
        MoveToEx(hdc, px[1], py[1], nullptr);
        LineTo(hdc, px[e], py[e]);
    }
}

static void renderWindow(void* hwnd) noexcept {
    PaintStruct ps;
    void* hdc = BeginPaint(hwnd, &ps);
    if (!hdc) return;

    g_framesPainted++;

    // 1. Mica Carbon-Glass Window Background + Crisp Glowing Cyan Outer Frame
    GdiFillSolidRect(hdc, 0, 0, WIN_W, WIN_H, 0xFF0F1522u);
    GdiDrawRectBorder(hdc, 0, 0, WIN_W, WIN_H, 0xFF00E5FFu);
    GdiDrawRectBorder(hdc, 1, 1, WIN_W - 2, WIN_H - 2, 0xFF0078B4u);

    // 2. Authentic Mica Title Bar (height 40px)
    GdiFillSolidRect(hdc, 2, 2, WIN_W - 4, 40, 0xFF162238u);
    GdiFillSolidRect(hdc, 2, 42, WIN_W - 4, 2, 0xFF00E5FFu);
    // Accent emblem square
    GdiFillSolidRect(hdc, 16, 12, 18, 18, 0xFF00E5FFu);
    GdiFillSolidRect(hdc, 20, 16, 10, 10, 0xFF162238u);

    char title[120];
    size_t tp = 0;
    appendStr(title, tp, sizeof(title), "MicaNT Sovereign Ring-3 Win32 GUI Window  [PID=");
    appendDec(title, tp, sizeof(title), g_pid);
    appendStr(title, tp, sizeof(title), " | CR3 Isolated | 0 Syscalls]");
    SetTextColor(hdc, 0xFF00E5FFu);
    GdiTextOutScaledA(hdc, 46, 14, title, -1, 1);

    // Caption Buttons ([_] [[]] [X])
    GdiFillSolidRect(hdc, WIN_W - 116, 10, 28, 22, 0xFF223452u);
    GdiFillSolidRect(hdc, WIN_W - 82,  10, 28, 22, 0xFF223452u);
    GdiFillSolidRect(hdc, WIN_W - 48,  10, 32, 22, 0xFFB8283Au);
    SetTextColor(hdc, 0xFFE6F0FFu);
    TextOutA(hdc, WIN_W - 106, 17, "_", 1);
    TextOutA(hdc, WIN_W - 72,  17, "O", 1);
    TextOutA(hdc, WIN_W - 36,  17, "X", 1);

    // 3. Left Card: Win32 Architecture & Live Telemetry
    GdiFillSolidRect(hdc, 20, 62, 500, 380, 0xFF131C2Eu);
    GdiDrawRectBorder(hdc, 20, 62, 500, 380, 0xFF2B4268u);
    GdiFillSolidRect(hdc, 21, 63, 498, 28, 0xFF1B2942u);
    SetTextColor(hdc, 0xFFFFD700u);
    TextOutA(hdc, 34, 73, "RING-3 WIN32 SUBSYSTEM ARCHITECTURE", -1);

    SetTextColor(hdc, 0xFF64F0A0u);
    GdiTextOutScaledA(hdc, 36, 106, "WIN32 GUI LIVE!", -1, 2);

    char line[96];
    size_t lp = 0;
    SetTextColor(hdc, 0xFFC8DCF5u);

    lp = 0; appendStr(line, lp, sizeof(line), "Process ID (TEB):   PID "); appendDec(line, lp, sizeof(line), g_pid);
    appendStr(line, lp, sizeof(line), " (Private Hardware CR3)");
    TextOutA(hdc, 36, 142, line, -1);

    lp = 0; appendStr(line, lp, sizeof(line), "GOP Resolution:     ");
    appendDec(line, lp, sizeof(line), static_cast<uint64_t>(g_screenW));
    appendStr(line, lp, sizeof(line), "x");
    appendDec(line, lp, sizeof(line), static_cast<uint64_t>(g_screenH));
    appendStr(line, lp, sizeof(line), " (NtGdiGetVideoInfo)");
    TextOutA(hdc, 36, 166, line, -1);

    TextOutA(hdc, 36, 190, "Surface Backbuffer: 1080x620 32-bpp BGRA (2.55 MB PFN)", -1);
    TextOutA(hdc, 36, 214, "Blit Pipeline:      EndPaint -> gdi32!BitBlt -> SSN 0x81", -1);
    TextOutA(hdc, 36, 238, "Event Pipeline:     GetMessageW -> NtUserGetMessage 0x82", -1);
    TextOutA(hdc, 36, 262, "Direct Syscalls:    0 in app, user32, gdi32, kernel32!", -1);

    GdiFillSolidRect(hdc, 36, 294, 468, 2, 0xFF283C5Eu);

    SetTextColor(hdc, 0xFF00E5FFu);
    lp = 0; appendStr(line, lp, sizeof(line), "Frames Blitted to VRAM:   "); appendDec(line, lp, sizeof(line), g_framesPainted);
    TextOutA(hdc, 36, 314, line, -1);

    lp = 0; appendStr(line, lp, sizeof(line), "Live PS/2 WM_CHAR Events: "); appendDec(line, lp, sizeof(line), g_keyEvents);
    TextOutA(hdc, 36, 338, line, -1);

    lp = 0; appendStr(line, lp, sizeof(line), "Prism3D Rotation Step:    "); appendDec(line, lp, sizeof(line), static_cast<uint64_t>(g_rotStep & 31));
    appendStr(line, lp, sizeof(line), " / 32");
    TextOutA(hdc, 36, 362, line, -1);

    SetTextColor(hdc, 0xFF64F0A0u);
    TextOutA(hdc, 36, 398, "STATUS: 100% Ring-3 Userland Pixels -> Zero VRAM Exposure", -1);

    // 4. Right Card: Live Ring-3 Userland PrismX 3D Wireframe Canvas
    GdiFillSolidRect(hdc, 540, 62, 520, 380, 0xFF0B101Cu);
    GdiDrawRectBorder(hdc, 540, 62, 520, 380, 0xFF2B4268u);
    GdiFillSolidRect(hdc, 541, 63, 518, 28, 0xFF1B2942u);
    SetTextColor(hdc, 0xFF00E5FFu);
    TextOutA(hdc, 554, 73, "RING-3 USERLAND GDI32 WIREFRAME VIEWPORT", -1);

    drawPrismOctahedron(hdc, 800, 260, g_rotStep);

    SetTextColor(hdc, 0xFFB0C8E8u);
    TextOutA(hdc, 558, 418, "Rasterized in Ring-3 gdi32.dll (MoveToEx + LineTo)", -1);

    // 5. Bottom Interactive PS/2 WM_CHAR Input Box
    GdiFillSolidRect(hdc, 20, 460, WIN_W - 40, 108, 0xFF131C2Eu);
    GdiDrawRectBorder(hdc, 20, 460, WIN_W - 40, 108, 0xFF00E5FFu);
    GdiFillSolidRect(hdc, 21, 461, WIN_W - 42, 24, 0xFF1B2942u);
    SetTextColor(hdc, 0xFFFFD700u);
    TextOutA(hdc, 34, 469, "LIVE HARDWARE PS/2 INPUT (user32!GetMessageW -> WndProc WM_CHAR)", -1);

    GdiFillSolidRect(hdc, 36, 498, WIN_W - 72, 48, 0xFF0A0E18u);
    GdiDrawRectBorder(hdc, 36, 498, WIN_W - 72, 48, 0xFF385A88u);

    char inputDisplay[112];
    size_t ip = 0;
    appendStr(inputDisplay, ip, sizeof(inputDisplay), "> ");
    appendStr(inputDisplay, ip, sizeof(inputDisplay), g_typedText);
    SetTextColor(hdc, 0xFF64F0A0u);
    GdiTextOutScaledA(hdc, 50, 514, inputDisplay, -1, 2);
    // Glowing block cursor
    int cursorX = 50 + static_cast<int>(ip * 16) + 2;
    if (cursorX + 12 < WIN_W - 44) {
        GdiFillSolidRect(hdc, cursorX, 514, 10, 16, 0xFF00E5FFu);
    }

    // 6. Status Footer
    GdiFillSolidRect(hdc, 2, WIN_H - 36, WIN_W - 4, 34, 0xFF162238u);
    GdiFillSolidRect(hdc, 2, WIN_H - 36, WIN_W - 4, 1, 0xFF2B4268u);
    SetTextColor(hdc, 0xFF96B8E0u);
    TextOutA(hdc, 20, WIN_H - 23, "Type keys to render live WM_CHAR + spin 3D Prism | Press 'q' or ESC -> PostQuitMessage(0)", -1);

    // Present entire 1080x620 Ring-3 pixel surface straight to 1920x1080 UEFI GOP VRAM!
    EndPaint(hwnd, &ps);
}

static int64_t MainWndProc(void* hwnd, uint32_t msg, uint64_t wParam, int64_t lParam) noexcept {
    switch (msg) {
        case WM_PAINT:
            renderWindow(hwnd);
            return 0;
        case WM_KEYDOWN:
            if (wParam == 0x1Bu) { // VK_ESCAPE
                PostQuitMessage(0);
                return 0;
            }
            break;
        case WM_CHAR: {
            char ch = static_cast<char>(wParam & 0x7Fu);
            if (ch == 'q' || ch == 'Q' || ch == 27) {
                PostQuitMessage(0);
                return 0;
            }
            g_keyEvents++;
            g_rotStep += 2;
            if (ch == '\b' || ch == 127) {
                if (g_typedLen > 0) g_typedText[--g_typedLen] = '\0';
            } else if (ch >= 32 && ch < 127 && g_typedLen + 1 < 54) {
                g_typedText[g_typedLen++] = ch;
                g_typedText[g_typedLen] = '\0';
            }
            InvalidateRect(hwnd, nullptr, false);
            return 0;
        }
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void WinMainEntry() {
    g_pid = GetCurrentProcessId();
    g_screenW = GetSystemMetrics(0);
    g_screenH = GetSystemMetrics(1);
    g_typedText[0] = '\0';
    g_typedLen = 0;

    WndClassExW wc;
    wc.cbSize = sizeof(WndClassExW);
    wc.style = 0;
    wc.lpfnWndProc = MainWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = nullptr;
    wc.hIcon = nullptr;
    wc.hCursor = nullptr;
    wc.hbrBackground = nullptr;
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = L"MicaWin32Window";
    wc.hIconSm = nullptr;
    RegisterClassExW(&wc);

    int wx = (g_screenW > WIN_W) ? ((g_screenW - WIN_W) / 2) : 40;
    int wy = 110;
    void* hwnd = CreateWindowExW(
        0, L"MicaWin32Window", L"MicaNT Ring-3 Win32 GUI", 0x10CF0000u,
        wx, wy, WIN_W, WIN_H, nullptr, nullptr, nullptr, nullptr
    );
    if (!hwnd) ExitProcess(1);

    ShowWindow(hwnd, 5);
    UpdateWindow(hwnd);

    Msg msg;
    msg.hwnd = nullptr;
    msg.message = 0;
    msg.wParam = 0;
    msg.lParam = 0;
    while (GetMessageW(&msg, hwnd, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DestroyWindow(hwnd);

    void* hOut = GetStdHandle(0xFFFFFFF5u);
    char summary[160];
    size_t sp = 0;
    appendStr(summary, sp, sizeof(summary), "[Win32 GUI PID=");
    appendDec(summary, sp, sizeof(summary), g_pid);
    appendStr(summary, sp, sizeof(summary), "] Clean PostQuitMessage(0): Frames=");
    appendDec(summary, sp, sizeof(summary), g_framesPainted);
    appendStr(summary, sp, sizeof(summary), " | Keys=");
    appendDec(summary, sp, sizeof(summary), g_keyEvents);
    appendStr(summary, sp, sizeof(summary), " | Text='");
    appendStr(summary, sp, sizeof(summary), g_typedText);
    appendStr(summary, sp, sizeof(summary), "' PASS!\r\n");
    uint32_t nw = 0;
    WriteFile(hOut, summary, static_cast<uint32_t>(cstrlen(summary)), &nw, nullptr);

    ExitProcess(0);
}

} // extern "C"
