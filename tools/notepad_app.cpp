/**
 * @file notepad_app.cpp
 * @brief Sovereign Ring-3 Win32 Interactive Notepad with Live NTFS 3.1 Save (C:\Windows\System32\notepad.exe).
 *
 * 100% Unmodified-ABI Dynamic Ring-3 Win32 GUI Executable (0 direct syscalls):
 *   - Imports ONLY kernel32.dll + user32.dll + gdi32.dll -> ntdll.dll -> Ring 0 DWM Compositor + NTFS 3.1
 *   - Receives live hardware PS/2 WM_CHAR keyboard input when focused in the multi-window DWM desktop
 *   - Renders multi-line editable text with line-number gutter & glowing cyan caret
 *   - Pressing F2 or Ctrl+S flushes the edited document straight to C:\MicaNT\desktop_note.txt via
 *     kernel32!CreateFileW + WriteFile + CloseHandle (authentic NTFS 3.1 AHCI DMA persistence!)
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
    void* hwnd; uint32_t message; uint64_t wParam; int64_t lParam; uint32_t time; Point pt; uint32_t lPrivate;
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

// kernel32.dll
__declspec(dllimport) void*    GetStdHandle(uint32_t nStdHandle);
__declspec(dllimport) void*    CreateFileW(const wchar_t* lpFileName, uint32_t dwAccess, uint32_t dwShare,
                                           void*, uint32_t dwDisp, uint32_t dwFlags, void*);
__declspec(dllimport) int32_t  WriteFile(void* hFile, const void* lpBuf, uint32_t nBytes, uint32_t* lpWritten, void*);
__declspec(dllimport) int32_t  CloseHandle(void* hObject);
__declspec(dllimport) uint32_t GetCurrentProcessId();
__declspec(dllimport) void     Sleep(uint32_t dwMilliseconds);
__declspec(dllimport) void     ExitProcess(uint32_t uExitCode);

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
static constexpr uint32_t WM_CHAR          = 0x0102u;
static constexpr uint32_t VK_F2            = 0x71u;

static constexpr int WIN_W = 720;
static constexpr int WIN_H = 480;

static char     g_text[1024];
static size_t   g_textLen = 0;
static uint32_t g_pid = 0;
static uint32_t g_keyCount = 0;
static bool     g_savedOk = false;
static uint32_t g_savedBytes = 0;
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

static void saveToNtfs() noexcept {
    // Write live edited text directly to C:\MicaNT\desktop_note.txt via real NTFS 3.1 AHCI DMA!
    void* hFile = CreateFileW(
        L"\\MicaNT\\desktop_note.txt",
        0x40000000u /*GENERIC_WRITE*/,
        0, nullptr,
        2u /*CREATE_ALWAYS*/,
        0x80u, nullptr
    );
    if (!hFile || hFile == reinterpret_cast<void*>(-1)) {
        g_savedOk = false;
        return;
    }
    uint32_t written = 0;
    bool ok = WriteFile(hFile, g_text, static_cast<uint32_t>(g_textLen), &written, nullptr) != 0;
    CloseHandle(hFile);
    g_savedOk = ok;
    g_savedBytes = written;

    char msg[160];
    size_t p = 0;
    appendStr(msg, p, sizeof(msg), "[notepad.exe PID=");
    appendDec(msg, p, sizeof(msg), g_pid);
    appendStr(msg, p, sizeof(msg), "] NTFS 3.1 Save C:\\MicaNT\\desktop_note.txt -> ");
    appendDec(msg, p, sizeof(msg), written);
    appendStr(msg, p, sizeof(msg), ok ? " bytes SAVED OK!" : " FAILED");
    writeOutLine(msg);
}

static void paintNotepad(void* hwnd) noexcept {
    PaintStruct ps{};
    void* hdc = BeginPaint(hwnd, &ps);
    if (!hdc) return;

    // Background & Menu Bar
    GdiFillSolidRect(hdc, 0, 0, WIN_W, WIN_H, 0xFF0C121Eu);
    GdiFillSolidRect(hdc, 0, 0, WIN_W, 28, 0xFF152036u);
    GdiFillSolidRect(hdc, 0, 28, WIN_W, 1, 0xFF283C5Eu);
    SetTextColor(hdc, 0xFFC6DEF8u);
    TextOutA(hdc, 12, 10, "File (F2 Save)    Edit    Format    View    Help", -1);

    // Line-Number Gutter
    int gutterW = 44;
    GdiFillSolidRect(hdc, 0, 29, gutterW, WIN_H - 59, 0xFF111A2Bu);
    GdiFillSolidRect(hdc, gutterW, 29, 1, WIN_H - 59, 0xFF283C5Eu);

    // Render multi-line document with syntax-tinted header line & glowing caret
    int lineNum = 1;
    int colNum = 1;
    int y = 42;
    char lineBuf[96];
    size_t lbLen = 0;
    int caretX = gutterW + 14;
    int caretY = y;

    auto flushLine = [&](int ln, int ly, const char* txt, size_t len) noexcept {
        char numStr[8];
        size_t np = 0;
        appendDec(numStr, np, sizeof(numStr), static_cast<uint64_t>(ln));
        SetTextColor(hdc, 0xFF6A88AEu);
        TextOutA(hdc, (ln >= 10) ? 14 : 20, ly, numStr, -1);

        uint32_t col = (ln == 1) ? 0xFF00E5FFu : ((ln == 2) ? 0xFF64F0A0u : 0xFFE6F0FFu);
        SetTextColor(hdc, col);
        if (len > 0) TextOutA(hdc, gutterW + 14, ly, txt, static_cast<int>(len));
    };

    for (size_t i = 0; i < g_textLen; ++i) {
        char c = g_text[i];
        if (c == '\r') continue;
        if (c == '\n' || lbLen >= 78) {
            lineBuf[lbLen] = '\0';
            if (y + 18 < WIN_H - 36) flushLine(lineNum, y, lineBuf, lbLen);
            lineNum++;
            colNum = 1;
            y += 20;
            lbLen = 0;
            if (c != '\n') {
                lineBuf[lbLen++] = c;
                colNum++;
            }
        } else {
            lineBuf[lbLen++] = c;
            colNum++;
        }
    }
    lineBuf[lbLen] = '\0';
    if (y + 18 < WIN_H - 36) flushLine(lineNum, y, lineBuf, lbLen);
    caretX = gutterW + 14 + static_cast<int>(lbLen * 8);
    caretY = y;

    // Glowing Cyan Block Caret
    if (caretX + 8 < WIN_W - 10 && caretY + 14 < WIN_H - 34) {
        GdiFillSolidRect(hdc, caretX + 1, caretY - 1, 8, 12, 0xFF00E5FFu);
    }

    // Save Confirmation Banner (if saved to NTFS 3.1!)
    if (g_savedOk) {
        GdiFillSolidRect(hdc, 54, WIN_H - 64, WIN_W - 68, 26, 0xFF103428u);
        GdiDrawRectBorder(hdc, 54, WIN_H - 64, WIN_W - 68, 26, 0xFF3CE69Au);
        char sb[120];
        size_t sp = 0;
        appendStr(sb, sp, sizeof(sb), "[NTFS 3.1 SAVED] C:\\MicaNT\\desktop_note.txt (");
        appendDec(sb, sp, sizeof(sb), g_savedBytes);
        appendStr(sb, sp, sizeof(sb), " bytes flushed via AHCI DMA)");
        SetTextColor(hdc, 0xFF64F0A0u);
        TextOutA(hdc, 66, WIN_H - 55, sb, -1);
    }

    // Bottom Status Bar
    GdiFillSolidRect(hdc, 0, WIN_H - 30, WIN_W, 30, 0xFF152036u);
    GdiFillSolidRect(hdc, 0, WIN_H - 30, WIN_W, 1, 0xFF283C5Eu);
    char foot[128];
    size_t fp = 0;
    appendStr(foot, fp, sizeof(foot), "Ln ");
    appendDec(foot, fp, sizeof(foot), static_cast<uint64_t>(lineNum));
    appendStr(foot, fp, sizeof(foot), ", Col ");
    appendDec(foot, fp, sizeof(foot), static_cast<uint64_t>(colNum));
    appendStr(foot, fp, sizeof(foot), "  |  Keys: ");
    appendDec(foot, fp, sizeof(foot), g_keyCount);
    appendStr(foot, fp, sizeof(foot), "  |  UTF-8 CRLF  |  F2/Ctrl+S: Save NTFS");
    SetTextColor(hdc, 0xFF9AC4EAu);
    TextOutA(hdc, 12, WIN_H - 19, foot, -1);

    EndPaint(hwnd, &ps);
}

static int64_t NotepadWndProc(void* hwnd, uint32_t msg, uint64_t wParam, int64_t lParam) noexcept {
    switch (msg) {
        case WM_PAINT:
            paintNotepad(hwnd);
            return 0;
        case WM_SETFOCUS:
            InvalidateRect(hwnd, nullptr, false);
            return 0;
        case WM_CLOSE:
            g_running = false;
            PostQuitMessage(0);
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_F2) {
                saveToNtfs();
                InvalidateRect(hwnd, nullptr, false);
                return 0;
            }
            break;
        case WM_CHAR: {
            char ch = static_cast<char>(wParam & 0x7Fu);
            if (ch == 19) { // Ctrl+S
                saveToNtfs();
                InvalidateRect(hwnd, nullptr, false);
                return 0;
            }
            g_keyCount++;
            if (ch == '\b' || ch == 127) {
                if (g_textLen > 0) g_text[--g_textLen] = '\0';
            } else if (ch == '\r' || ch == '\n') {
                if (g_textLen + 2 < sizeof(g_text)) {
                    g_text[g_textLen++] = '\r';
                    g_text[g_textLen++] = '\n';
                    g_text[g_textLen] = '\0';
                }
            } else if (ch >= 32 && ch < 127 && g_textLen + 1 < sizeof(g_text)) {
                g_text[g_textLen++] = ch;
                g_text[g_textLen] = '\0';
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
    const char* header = "# MicaNT Sovereign Desktop Note (Ring-3 Notepad)\r\n";
    g_textLen = 0;
    appendStr(g_text, g_textLen, sizeof(g_text), header);

    WndClassExW wc{};
    wc.cbSize = sizeof(WndClassExW);
    wc.lpfnWndProc = NotepadWndProc;
    wc.lpszClassName = L"MicaNotepadWnd";
    RegisterClassExW(&wc);

    void* hwnd = CreateWindowExW(
        WS_EX_COMPOSITED, L"MicaNotepadWnd", L"desktop_note.txt - MicaNT Notepad",
        0x10CF0000u, 1140, 110, WIN_W, WIN_H, nullptr, nullptr, nullptr, nullptr
    );
    if (!hwnd) ExitProcess(1);

    ShowWindow(hwnd, 5);
    UpdateWindow(hwnd);

    Msg msg{};
    while (g_running) {
        if (PeekMessageW(&msg, hwnd, 0, 0, 1u)) {
            if (msg.message == WM_QUIT) break;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        } else {
            Sleep(0);
        }
    }

    DestroyWindow(hwnd);
    ExitProcess(0);
}

} // extern "C"
