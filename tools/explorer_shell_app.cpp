/**
 * @file explorer_shell_app.cpp
 * @brief Persistent Windows XP-style MicaNT desktop shell (explorer.exe).
 *
 * The shell owns the desktop, Start menu, taskbar and notification area as
 * user-mode Win32 windows. Each surface is composed by win32k.dll.
 */

#define MICANT_WIN32K_IMPORTS
#include "micant/shared/win32k_client.h"
#include "gui_common.hpp"

namespace {

using namespace micant_gui;

constexpr uint32_t WS_VISIBLE = 0x10000000U;
constexpr uint32_t WS_POPUP = 0x80000000U;
constexpr uint32_t WS_OVERLAPPEDWINDOW = 0x00CF0000U;
constexpr uint32_t SW_HIDE = 0;
constexpr uint32_t SW_SHOW = 5;
constexpr uint32_t SW_RESTORE = 9;
constexpr uint32_t SWP_NOSIZE = 0x0001U;
constexpr uint32_t SWP_NOMOVE = 0x0002U;
constexpr uint32_t WM_NCCREATE = 0x0081U;
constexpr uint32_t WM_CREATE = 0x0001U;
constexpr uint32_t WM_CLOSE = 0x0010U;
constexpr uint32_t WM_LBUTTONUP = 0x0202U;
constexpr uint32_t TASKBAR_H = 40;
constexpr uint32_t MENU_W = 344;
constexpr uint32_t MENU_H = 390;
constexpr size_t MAX_APP_WINDOWS = 8;

enum class View : uint8_t { MyComputer, MyDocuments, ControlPanel, Network, RecycleBin };

struct AppWindow {
    void* hwnd{};
    void* dc{};
    uint32_t* pixels{};
    uint32_t width{500};
    uint32_t height{320};
    View view{View::MyComputer};
    bool open{};
};

struct ShellState {
    uint32_t screenW{1024}, screenH{768};
    void* desktop{};
    void* desktopDc{};
    uint32_t* desktopPixels{};
    void* startMenu{};
    void* menuDc{};
    uint32_t* menuPixels{};
    bool startOpen{};
    AppWindow apps[MAX_APP_WINDOWS]{};
    uint8_t appCount{};
    uint8_t clockSecond{0xFF};
    char clockText[12]{"--:--"};
};

ShellState g{};
constexpr wchar_t kClassName[] = L"MicaNT.Explorer.Desktop";
constexpr wchar_t kDesktopTitle[] = L"MicaNT Desktop";
constexpr wchar_t kStartTitle[] = L"Start";

inline uint32_t rgb(uint8_t r, uint8_t green, uint8_t b) noexcept {
    return PackBGRA(r, green, b);
}

void fill(uint32_t* p, uint32_t w, uint32_t h, int32_t x, int32_t y,
          int32_t rw, int32_t rh, uint32_t color) noexcept {
    if (!p || rw <= 0 || rh <= 0) return;
    const int32_t x0 = x < 0 ? 0 : x;
    const int32_t y0 = y < 0 ? 0 : y;
    const int32_t x1 = x + rw > static_cast<int32_t>(w) ? static_cast<int32_t>(w) : x + rw;
    const int32_t y1 = y + rh > static_cast<int32_t>(h) ? static_cast<int32_t>(h) : y + rh;
    for (int32_t yy = y0; yy < y1; ++yy) {
        uint32_t* row = p + static_cast<size_t>(yy) * w + x0;
        for (int32_t xx = x0; xx < x1; ++xx) row[xx - x0] = color;
    }
}

void text(uint32_t* p, uint32_t w, uint32_t h, int32_t x, int32_t y,
          const char* value, uint32_t color) noexcept {
    if (!p || !value) return;
    for (size_t i = 0; value[i]; ++i) {
        const int32_t gx = x + static_cast<int32_t>(i * 8);
        if (gx < 0 || gx + 8 > static_cast<int32_t>(w) || y < 0 || y + 8 > static_cast<int32_t>(h)) break;
        DrawChar(p, w, static_cast<uint32_t>(gx), static_cast<uint32_t>(y), value[i], color);
    }
}

void icon(uint32_t* p, uint32_t w, uint32_t h, int32_t x, int32_t y, View kind) noexcept {
    fill(p, w, h, x + 4, y + 5, 34, 28, rgb(0, 0, 0));
    if (kind == View::MyComputer) {
        fill(p, w, h, x + 5, y + 2, 29, 19, rgb(28, 93, 170));
        fill(p, w, h, x + 8, y + 5, 23, 13, rgb(117, 199, 244));
        fill(p, w, h, x + 1, y + 22, 37, 8, rgb(210, 218, 225));
        fill(p, w, h, x + 4, y + 24, 31, 3, rgb(94, 107, 121));
    } else if (kind == View::MyDocuments) {
        fill(p, w, h, x + 7, y + 3, 25, 27, rgb(240, 197, 59));
        fill(p, w, h, x + 11, y + 8, 16, 2, rgb(255, 255, 240));
        fill(p, w, h, x + 11, y + 13, 16, 2, rgb(255, 255, 240));
        fill(p, w, h, x + 11, y + 18, 12, 2, rgb(255, 255, 240));
    } else if (kind == View::ControlPanel) {
        fill(p, w, h, x + 5, y + 4, 29, 25, rgb(215, 221, 228));
        fill(p, w, h, x + 9, y + 8, 8, 8, rgb(68, 137, 199));
        fill(p, w, h, x + 21, y + 8, 8, 8, rgb(228, 116, 52));
        fill(p, w, h, x + 9, y + 19, 8, 7, rgb(91, 165, 91));
        fill(p, w, h, x + 21, y + 19, 8, 7, rgb(190, 79, 79));
    } else if (kind == View::Network) {
        fill(p, w, h, x + 5, y + 6, 29, 23, rgb(232, 236, 240));
        fill(p, w, h, x + 8, y + 9, 23, 17, rgb(59, 129, 196));
        fill(p, w, h, x + 11, y + 12, 17, 11, rgb(128, 202, 240));
    } else {
        fill(p, w, h, x + 8, y + 5, 24, 25, rgb(195, 210, 221));
        fill(p, w, h, x + 11, y + 9, 18, 17, rgb(238, 243, 247));
    }
}

void desktopIcon(uint32_t* p, int32_t y, View view, const char* label) noexcept {
    icon(p, g.screenW, g.screenH - TASKBAR_H, 22, y, view);
    text(p, g.screenW, g.screenH - TASKBAR_H, 16, y + 36, label, rgb(255, 255, 255));
}

void drawWallpaper() noexcept {
    const uint32_t w = g.screenW, h = g.screenH - TASKBAR_H;
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            uint32_t c;
            const int32_t horizon = static_cast<int32_t>(h * 48 / 100);
            if (static_cast<int32_t>(y) < horizon) {
                const uint32_t t = static_cast<uint32_t>(y) * 100 / (horizon ? horizon : 1);
                c = rgb(static_cast<uint8_t>(46 + t / 4), static_cast<uint8_t>(133 + t / 3),
                        static_cast<uint8_t>(205 + t / 6));
                const int32_t sx = static_cast<int32_t>(x) - static_cast<int32_t>(w * 78 / 100);
                const int32_t sy = static_cast<int32_t>(y) - static_cast<int32_t>(h * 15 / 100);
                if (sx * sx + sy * sy < static_cast<int32_t>(h * h / 150)) c = rgb(248, 222, 141);
            } else {
                const int32_t dx = static_cast<int32_t>(x) - static_cast<int32_t>(w * 57 / 100);
                const int32_t dy = static_cast<int32_t>(y) - horizon;
                const int32_t curve = (dx * dx) / static_cast<int32_t>(w * 5) + horizon - static_cast<int32_t>(h * 6 / 100);
                if (static_cast<int32_t>(y) < curve) c = rgb(37, 126, 49);
                else c = rgb(static_cast<uint8_t>(52 + (x % 23)), static_cast<uint8_t>(151 + (x % 31)), 59);
                const int32_t dx2 = static_cast<int32_t>(x) - static_cast<int32_t>(w * 15 / 100);
                const int32_t curve2 = (dx2 * dx2) / static_cast<int32_t>(w * 4) + horizon + static_cast<int32_t>(h * 1 / 100);
                if (static_cast<int32_t>(y) > curve2) c = rgb(41, 139, 48);
            }
            g.desktopPixels[static_cast<size_t>(y) * w + x] = c;
        }
    }

    desktopIcon(g.desktopPixels, 28, View::MyComputer, "My Computer");
    desktopIcon(g.desktopPixels, 124, View::MyDocuments, "My Documents");
    desktopIcon(g.desktopPixels, 220, View::Network, "Network Places");
    desktopIcon(g.desktopPixels, 316, View::RecycleBin, "Recycle Bin");

    const int32_t barY = static_cast<int32_t>(g.screenH - TASKBAR_H);
    fill(g.desktopPixels, g.screenW, g.screenH, 0, barY, g.screenW, TASKBAR_H, rgb(36, 91, 174));
    fill(g.desktopPixels, g.screenW, g.screenH, 0, barY, g.screenW, 3, rgb(91, 154, 227));
    fill(g.desktopPixels, g.screenW, g.screenH, 0, barY + 4, g.screenW, 1, rgb(30, 70, 142));

    // XP Start orb and pill.
    fill(g.desktopPixels, g.screenW, g.screenH, 0, barY + 3, 102, TASKBAR_H - 3, rgb(45, 147, 49));
    fill(g.desktopPixels, g.screenW, g.screenH, 0, barY + 3, 8, TASKBAR_H - 3, rgb(82, 183, 68));
    fill(g.desktopPixels, g.screenW, g.screenH, 12, barY + 8, 24, 24, rgb(225, 238, 243));
    fill(g.desktopPixels, g.screenW, g.screenH, 15, barY + 11, 8, 8, rgb(235, 59, 47));
    fill(g.desktopPixels, g.screenW, g.screenH, 24, barY + 11, 8, 8, rgb(75, 165, 66));
    fill(g.desktopPixels, g.screenW, g.screenH, 15, barY + 20, 8, 8, rgb(53, 125, 202));
    fill(g.desktopPixels, g.screenW, g.screenH, 24, barY + 20, 8, 8, rgb(250, 197, 50));
    text(g.desktopPixels, g.screenW, g.screenH, 42, barY + 15, "start", rgb(255, 255, 255));

    int32_t taskX = 112;
    for (size_t i = 0; i < MAX_APP_WINDOWS; ++i) {
        if (!g.apps[i].open) continue;
        fill(g.desktopPixels, g.screenW, g.screenH, taskX, barY + 8, 146, 25, rgb(61, 118, 199));
        fill(g.desktopPixels, g.screenW, g.screenH, taskX + 1, barY + 9, 144, 1, rgb(116, 166, 225));
        icon(g.desktopPixels, g.screenW, g.screenH, taskX + 3, barY + 7, g.apps[i].view);
        const char* title = g.apps[i].view == View::MyComputer ? "My Computer" :
                            g.apps[i].view == View::MyDocuments ? "My Documents" :
                            g.apps[i].view == View::ControlPanel ? "Control Panel" :
                            g.apps[i].view == View::RecycleBin ? "Recycle Bin" : "Network Places";
        text(g.desktopPixels, g.screenW, g.screenH, taskX + 34, barY + 16, title, rgb(255, 255, 255));
        taskX += 151;
    }

    const int32_t trayX = static_cast<int32_t>(g.screenW) - 122;
    fill(g.desktopPixels, g.screenW, g.screenH, trayX, barY + 5, 122, TASKBAR_H - 5, rgb(29, 79, 151));
    fill(g.desktopPixels, g.screenW, g.screenH, trayX, barY + 7, 1, TASKBAR_H - 9, rgb(25, 62, 122));
    // Volume glyph.
    fill(g.desktopPixels, g.screenW, g.screenH, trayX + 12, barY + 17, 5, 8, rgb(239, 245, 252));
    fill(g.desktopPixels, g.screenW, g.screenH, trayX + 17, barY + 14, 4, 14, rgb(239, 245, 252));
    // Network status glyph.
    fill(g.desktopPixels, g.screenW, g.screenH, trayX + 34, barY + 24, 4, 4, rgb(247, 247, 247));
    fill(g.desktopPixels, g.screenW, g.screenH, trayX + 33, barY + 20, 6, 3, rgb(247, 247, 247));
    fill(g.desktopPixels, g.screenW, g.screenH, trayX + 31, barY + 16, 10, 3, rgb(247, 247, 247));
    text(g.desktopPixels, g.screenW, g.screenH, trayX + 50, barY + 16, g.clockText, rgb(255, 255, 255));
}

void drawStartMenu() noexcept {
    if (!g.menuPixels) return;
    const uint32_t w = MENU_W, h = MENU_H;
    fill(g.menuPixels, w, h, 0, 0, w, h, rgb(243, 243, 243));
    fill(g.menuPixels, w, h, 0, 0, w, 50, rgb(31, 89, 166));
    fill(g.menuPixels, w, h, 8, 8, 34, 34, rgb(235, 246, 250));
    fill(g.menuPixels, w, h, 12, 12, 12, 12, rgb(228, 65, 48));
    fill(g.menuPixels, w, h, 25, 12, 12, 12, rgb(77, 164, 65));
    fill(g.menuPixels, w, h, 12, 25, 12, 12, rgb(55, 124, 206));
    fill(g.menuPixels, w, h, 25, 25, 12, 12, rgb(242, 192, 42));
    text(g.menuPixels, w, h, 53, 19, "MicaNT User", rgb(255, 255, 255));
    fill(g.menuPixels, w, h, 0, 50, 205, h - 92, rgb(255, 255, 255));
    fill(g.menuPixels, w, h, 205, 50, w - 205, h - 92, rgb(220, 231, 244));
    const char* left[] = {"Internet", "E-mail", "----------------", "My Documents", "My Pictures", "My Music", "My Computer", "Control Panel", "Connect To", "Printers", "Help and Support", "Search", "Run..."};
    for (size_t i = 0; i < sizeof(left) / sizeof(left[0]); ++i) {
        const int32_t y = 63 + static_cast<int32_t>(i) * 23;
        if (left[i][0] == '-') fill(g.menuPixels, w, h, 12, y + 8, 178, 1, rgb(215, 215, 215));
        else text(g.menuPixels, w, h, 18, y, left[i], rgb(30, 30, 30));
    }
    const char* right[] = {"My Recent Documents", "My Pictures", "My Music", "My Computer", "Control Panel", "Set Program Access", "Connect To", "Printers and Faxes", "Help and Support", "Search", "Run..."};
    for (size_t i = 0; i < sizeof(right) / sizeof(right[0]); ++i)
        text(g.menuPixels, w, h, 216, 69 + static_cast<int32_t>(i) * 25, right[i], rgb(29, 65, 116));
    fill(g.menuPixels, w, h, 0, h - 42, w, 42, rgb(31, 89, 166));
    text(g.menuPixels, w, h, 18, h - 25, "Log Off", rgb(255, 255, 255));
    text(g.menuPixels, w, h, 214, h - 25, "Turn Off Computer", rgb(255, 255, 255));
}

void drawAppWindow(AppWindow& app) noexcept {
    if (!app.pixels) return;
    uint32_t* p = app.pixels;
    const uint32_t w = app.width, h = app.height;
    fill(p, w, h, 0, 0, w, h, rgb(255, 255, 255));
    fill(p, w, h, 0, 0, 158, h, rgb(236, 241, 247));
    fill(p, w, h, 0, 0, w, 27, rgb(242, 246, 251));
    fill(p, w, h, 0, 26, w, 1, rgb(207, 216, 228));
    fill(p, w, h, 12, 8, 8, 8, rgb(53, 121, 191));
    text(p, w, h, 27, 8, "Address", rgb(40, 59, 81));
    fill(p, w, h, 12, 32, w - 24, 25, rgb(255, 255, 255));
    DrawRect(p, w, 12, 32, w - 24, 25, rgb(191, 202, 215));
    const char* address = app.view == View::MyComputer ? "My Computer" :
                          app.view == View::MyDocuments ? "C:\\Documents and Settings\\MicaNT\\My Documents" :
                          app.view == View::ControlPanel ? "Control Panel" :
                          app.view == View::RecycleBin ? "Recycle Bin" : "Network Places";
    text(p, w, h, 22, 40, address, rgb(35, 74, 117));
    text(p, w, h, 174, 71, "Name", rgb(82, 98, 116));
    text(p, w, h, 370, 71, "Type", rgb(82, 98, 116));
    fill(p, w, h, 168, 88, w - 180, 1, rgb(211, 218, 226));

    const char* rowsComputer[] = {"Local Disk (C:)", "Data Volume (D:)", "Linux Root (E:)"};
    const char* rowsDocuments[] = {"My Pictures", "My Music", "Readme.txt"};
    const char* rowsControl[] = {"Appearance and Themes", "Network Connections", "User Accounts", "Date and Time"};
    const char* rowsNetwork[] = {"MicaNT Network", "Workgroup Computers"};
    const char* rowsRecycle[] = {"The Recycle Bin is empty."};
    const char** rows = rowsComputer;
    size_t count = 3;
    if (app.view == View::MyDocuments) rows = rowsDocuments;
    else if (app.view == View::ControlPanel) { rows = rowsControl; count = 4; }
    else if (app.view == View::Network) { rows = rowsNetwork; count = 2; }
    else if (app.view == View::RecycleBin) { rows = rowsRecycle; count = 1; }
    for (size_t i = 0; i < count; ++i) {
        const int32_t y = 100 + static_cast<int32_t>(i) * 26;
        fill(p, w, h, 174, y, 18, 15, i == 2 && app.view == View::MyComputer ? rgb(57, 135, 69) : rgb(236, 190, 67));
        text(p, w, h, 199, y + 4, rows[i], rgb(34, 42, 53));
        text(p, w, h, 370, y + 4, app.view == View::MyComputer ? "Disk drive" : "System folder", rgb(100, 108, 118));
    }
    fill(p, w, h, 0, h - 24, w, 24, rgb(244, 246, 249));
    fill(p, w, h, 0, h - 25, w, 1, rgb(212, 219, 228));
    text(p, w, h, 12, h - 17, "3 objects", rgb(90, 98, 108));
}

void present(void* dc) noexcept {
    if (dc) MicaWin32kPresentDC(dc);
}

void redrawDesktop() noexcept {
    if (!g.desktopPixels) return;
    drawWallpaper();
    present(g.desktopDc);
}

AppWindow* appFromHwnd(void* hwnd) noexcept {
    for (auto& app : g.apps) if (app.open && app.hwnd == hwnd) return &app;
    return nullptr;
}

void openView(View view) noexcept {
    for (auto& existing : g.apps) {
        if (existing.open && existing.view == view) {
            MicaWin32kShowWindow(existing.hwnd, SW_RESTORE);
            MicaWin32kSetWindowPos(existing.hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            return;
        }
    }
    AppWindow* app = nullptr;
    for (auto& candidate : g.apps) if (!candidate.open) { app = &candidate; break; }
    if (!app) return;
    app->view = view;
    app->width = 500;
    app->height = 320;
    app->hwnd = MicaWin32kCreateWindowExW(0, kClassName,
        view == View::MyComputer ? L"My Computer" :
        view == View::MyDocuments ? L"My Documents" :
        view == View::ControlPanel ? L"Control Panel" :
        view == View::RecycleBin ? L"Recycle Bin" : L"Network Places",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, 190 + static_cast<int32_t>(g.appCount % 4) * 32,
        90 + static_cast<int32_t>(g.appCount % 4) * 28, app->width, app->height,
        nullptr, nullptr, nullptr, nullptr);
    if (!app->hwnd) return;
    app->open = true;
    ++g.appCount;
    app->dc = MicaWin32kGetDC(app->hwnd);
    void* appBits = nullptr;
    void* appBitmap = MicaWin32kCreateDibSection(app->dc, static_cast<int32_t>(app->width),
                                                 static_cast<int32_t>(app->height), &appBits);
    if (appBitmap && appBits) {
        MicaWin32kSelectObject(app->dc, appBitmap);
        app->pixels = static_cast<uint32_t*>(appBits);
    }
    drawAppWindow(*app);
    present(app->dc);
    redrawDesktop();
}

void toggleStart() noexcept {
    g.startOpen = !g.startOpen;
    if (g.startOpen) {
        drawStartMenu();
        MicaWin32kShowWindow(g.startMenu, SW_SHOW);
        present(g.menuDc);
    } else {
        MicaWin32kShowWindow(g.startMenu, SW_HIDE);
    }
    redrawDesktop();
}

void updateClock() noexcept {
    MicaWin32kSystemTime value{};
    uint64_t ok = 0;
#if defined(__x86_64__)
    __asm__ __volatile__("movq %1, %%r10; movl $0x83, %%eax; syscall"
                         : "=a"(ok) : "r"(&value) : "r10", "rcx", "r11", "memory");
#endif
    if (!ok || value.second == g.clockSecond) return;
    g.clockSecond = value.second;
    uint8_t hour = value.hour;
    const bool pm = hour >= 12;
    hour = static_cast<uint8_t>(hour % 12);
    if (!hour) hour = 12;
    g.clockText[0] = static_cast<char>('0' + hour / 10);
    g.clockText[1] = static_cast<char>('0' + hour % 10);
    g.clockText[2] = ':';
    g.clockText[3] = static_cast<char>('0' + value.minute / 10);
    g.clockText[4] = static_cast<char>('0' + value.minute % 10);
    g.clockText[5] = ' ';
    g.clockText[6] = pm ? 'P' : 'A';
    g.clockText[7] = 'M';
    g.clockText[8] = '\0';
    redrawDesktop();
}

intptr_t WindowProc(void* hwnd, uint32_t message, uintptr_t, intptr_t lParam) {
    if (message == WM_NCCREATE) return 1;
    if (hwnd == g.desktop && message == WM_LBUTTONUP) {
        const int32_t x = static_cast<int16_t>(static_cast<uint32_t>(lParam) & 0xFFFFU);
        const int32_t y = static_cast<int16_t>((static_cast<uint32_t>(lParam) >> 16) & 0xFFFFU);
        if (y >= static_cast<int32_t>(g.screenH - TASKBAR_H)) {
            if (x < 104) toggleStart();
            else if (x < static_cast<int32_t>(g.screenW) - 124) {
                int32_t slotLeft = 112;
                for (auto& app : g.apps) {
                    if (!app.open) continue;
                    if (x >= slotLeft && x < slotLeft + 146) {
                        MicaWin32kShowWindow(app.hwnd, SW_RESTORE);
                        MicaWin32kSetWindowPos(app.hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
                        break;
                    }
                    slotLeft += 151;
                }
            }
        } else if (g.startOpen) {
            g.startOpen = false;
            MicaWin32kShowWindow(g.startMenu, SW_HIDE);
            redrawDesktop();
        } else if (x < 86 && y >= 28 && y < 112) openView(View::MyComputer);
        else if (x < 86 && y >= 124 && y < 208) openView(View::MyDocuments);
        else if (x < 86 && y >= 220 && y < 304) openView(View::Network);
        else if (x < 86 && y >= 316 && y < 400) openView(View::RecycleBin);
        return 0;
    }
    if (hwnd == g.startMenu && message == WM_LBUTTONUP) {
        const int32_t x = static_cast<int16_t>(static_cast<uint32_t>(lParam) & 0xFFFFU);
        const int32_t y = static_cast<int16_t>((static_cast<uint32_t>(lParam) >> 16) & 0xFFFFU);
        View view = View::MyComputer;
        if (y < 0 || y >= static_cast<int32_t>(MENU_H)) return 0;
        if (x >= 205) {
            const int32_t row = (y - 69) / 25;
            if (row == 0 || row == 1) view = View::MyDocuments;
            else if (row == 4) view = View::ControlPanel;
            else if (row == 6) view = View::Network;
        } else {
            const int32_t row = (y - 63) / 23;
            if (row == 3 || row == 4 || row == 5) view = View::MyDocuments;
            else if (row == 7) view = View::ControlPanel;
            else if (row == 8) view = View::Network;
        }
        g.startOpen = false;
        MicaWin32kShowWindow(g.startMenu, SW_HIDE);
        openView(view);
        redrawDesktop();
        return 0;
    }
    if (message == WM_CLOSE) {
        if (auto* app = appFromHwnd(hwnd)) {
            app->open = false;
            app->hwnd = nullptr;
            app->dc = nullptr;
            app->pixels = nullptr;
            MicaWin32kDestroyWindow(hwnd);
            redrawDesktop();
        }
        return 0;
    }
    return 0;
}

} // namespace

extern "C" void _start(const char* = nullptr) {
    UserVideoInfo64 video{};
    DoSyscall(0x80, reinterpret_cast<uint64_t>(&video), 0);
    if (video.width) g.screenW = video.width;
    if (video.height) g.screenH = video.height;
    if (g.screenW < 640 || g.screenH < 480) { g.screenW = 1024; g.screenH = 768; }

    if (!MicaWin32kRegisterClassW(kClassName, reinterpret_cast<void*>(&WindowProc))) {
        for (;;) __asm__ __volatile__("pause");
    }

    g.desktop = MicaWin32kCreateWindowExW(0, kClassName, kDesktopTitle,
        WS_POPUP | WS_VISIBLE, 0, 0, static_cast<int32_t>(g.screenW), static_cast<int32_t>(g.screenH),
        nullptr, nullptr, nullptr, nullptr);
    if (!g.desktop) for (;;) __asm__ __volatile__("pause");
    g.desktopDc = MicaWin32kGetDC(g.desktop);
    void* desktopBits = nullptr;
    void* desktopBitmap = MicaWin32kCreateDibSection(g.desktopDc, static_cast<int32_t>(g.screenW),
        static_cast<int32_t>(g.screenH), &desktopBits);
    if (!desktopBitmap || !desktopBits) for (;;) __asm__ __volatile__("pause");
    MicaWin32kSelectObject(g.desktopDc, desktopBitmap);
    g.desktopPixels = static_cast<uint32_t*>(desktopBits);

    const int32_t menuY = static_cast<int32_t>(g.screenH) - static_cast<int32_t>(MENU_H) - static_cast<int32_t>(TASKBAR_H);
    g.startMenu = MicaWin32kCreateWindowExW(0, kClassName, kStartTitle, WS_POPUP,
        0, menuY, MENU_W, MENU_H, nullptr, nullptr, nullptr, nullptr);
    if (g.startMenu) {
        g.menuDc = MicaWin32kGetDC(g.startMenu);
        void* menuBits = nullptr;
        void* menuBitmap = MicaWin32kCreateDibSection(g.menuDc, MENU_W, MENU_H, &menuBits);
        if (menuBitmap && menuBits) {
            MicaWin32kSelectObject(g.menuDc, menuBitmap);
            g.menuPixels = static_cast<uint32_t*>(menuBits);
        }
    }

    redrawDesktop();
    for (;;) {
        updateClock();
        MicaWin32kMessage msg{};
        while (MicaWin32kPeekMessageW(&msg, nullptr, 0, 0, 1)) {
            MicaWin32kDispatchMessageW(&msg);
            updateClock();
        }
        __asm__ __volatile__("pause");
    }
}
