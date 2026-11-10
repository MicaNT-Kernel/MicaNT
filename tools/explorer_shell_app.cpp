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
#include "explorer_shell/taskbar.hpp"
#include "explorer_shell/start_menu.hpp"
#include "explorer_shell/desktop.hpp"

namespace {

using namespace micant_gui;

constexpr uint32_t WS_VISIBLE = 0x10000000U;
constexpr uint32_t WS_POPUP = 0x80000000U;
constexpr uint32_t WS_OVERLAPPEDWINDOW = 0x00CF0000U;
constexpr uint32_t WS_EX_TOPMOST = 0x00000008U;
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
    void* taskbar{};
    void* taskbarDc{};
    uint32_t* taskbarPixels{};
    void* startMenu{};
    void* menuDc{};
    uint32_t* menuPixels{};
    bool startOpen{};
    AppWindow apps[MAX_APP_WINDOWS]{};
    uint8_t appCount{};
    uint8_t activeApp{0xFF};
    uint8_t clockSecond{0xFF};
    char clockText[12]{"--:--"};
};

ShellState g{};
constexpr wchar_t kClassName[] = L"MicaNT.Explorer.Desktop";
constexpr wchar_t kDesktopTitle[] = L"MicaNT Desktop";
constexpr wchar_t kTaskbarTitle[] = L"MicaNT Taskbar";
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

size_t collectTaskbarButtons(micant::explorer::TaskbarButton* buttons) noexcept {
    size_t count = 0;
    for (size_t i = 0; buttons && i < MAX_APP_WINDOWS; ++i) {
        if (!g.apps[i].open) continue;
        auto& button = buttons[count++];
        button.title = g.apps[i].view == View::MyComputer ? "My Computer" :
                       g.apps[i].view == View::MyDocuments ? "My Documents" :
                       g.apps[i].view == View::ControlPanel ? "Control Panel" :
                       g.apps[i].view == View::RecycleBin ? "Recycle Bin" : "Network Places";
        button.iconColor = g.apps[i].view == View::ControlPanel ? rgb(68, 137, 199) :
                           g.apps[i].view == View::RecycleBin ? rgb(195, 210, 221) : rgb(236, 190, 67);
        button.active = g.activeApp == i;
    }
    return count;
}

void redrawTaskbar() noexcept {
    if (!g.taskbarPixels) return;
    micant::explorer::TaskbarButton buttons[MAX_APP_WINDOWS]{};
    const size_t count = collectTaskbarButtons(buttons);
    micant::explorer::DrawTaskbar(g.taskbarPixels, g.screenW, TASKBAR_H, TASKBAR_H,
                                  buttons, count, g.clockText);
    present(g.taskbarDc);
}

void redrawDesktop() noexcept {
    if (!g.desktopPixels) return;
    micant::explorer::DrawDesktop(g.desktopPixels, g.screenW, g.screenH - TASKBAR_H);
    present(g.desktopDc);
    redrawTaskbar();
}

AppWindow* appFromHwnd(void* hwnd) noexcept {
    for (auto& app : g.apps) if (app.open && app.hwnd == hwnd) return &app;
    return nullptr;
}

void activateApp(void* hwnd) noexcept {
    for (size_t i = 0; i < MAX_APP_WINDOWS; ++i) {
        if (g.apps[i].open && g.apps[i].hwnd == hwnd) {
            g.activeApp = static_cast<uint8_t>(i);
            return;
        }
    }
}

void openView(View view) noexcept {
    for (auto& existing : g.apps) {
        if (existing.open && existing.view == view) {
            activateApp(existing.hwnd);
            MicaWin32kShowWindow(existing.hwnd, SW_RESTORE);
            MicaWin32kSetWindowPos(existing.hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            redrawDesktop();
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
    g.activeApp = static_cast<uint8_t>(app - g.apps);
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
        micant::explorer::DrawStartMenu(g.menuPixels, MENU_W, MENU_H, "MicaNT User");
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
    redrawTaskbar();
}

intptr_t WindowProc(void* hwnd, uint32_t message, uintptr_t, intptr_t lParam) {
    if (message == WM_NCCREATE) return 1;
    if (hwnd == g.taskbar && message == WM_LBUTTONUP) {
        const int32_t x = static_cast<int16_t>(static_cast<uint32_t>(lParam) & 0xFFFFU);
        const int32_t y = static_cast<int16_t>((static_cast<uint32_t>(lParam) >> 16) & 0xFFFFU);
        if (y < 0 || y >= static_cast<int32_t>(TASKBAR_H)) return 0;
        if (x < 104) {
            toggleStart();
        } else if (x < static_cast<int32_t>(g.screenW) - 124) {
            size_t taskCount = 0;
            for (const auto& app : g.apps) if (app.open) ++taskCount;
            const int32_t available = static_cast<int32_t>(g.screenW) - 126 - 112;
            const int32_t taskWidth = taskCount == 0 || available <= 0 ? 0 :
                (available / static_cast<int32_t>(taskCount) < 146
                    ? available / static_cast<int32_t>(taskCount) : 146);
            int32_t slotLeft = 112;
            for (size_t i = 0; i < MAX_APP_WINDOWS && taskWidth >= 64; ++i) {
                if (!g.apps[i].open) continue;
                if (x >= slotLeft && x < slotLeft + taskWidth - 4) {
                    g.activeApp = static_cast<uint8_t>(i);
                    MicaWin32kShowWindow(g.apps[i].hwnd, SW_RESTORE);
                    MicaWin32kSetWindowPos(g.apps[i].hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
                    redrawTaskbar();
                    break;
                }
                slotLeft += taskWidth;
            }
        }
        return 0;
    }
    if (hwnd == g.desktop && message == WM_LBUTTONUP) {
        const int32_t x = static_cast<int16_t>(static_cast<uint32_t>(lParam) & 0xFFFFU);
        const int32_t y = static_cast<int16_t>((static_cast<uint32_t>(lParam) >> 16) & 0xFFFFU);
        if (g.startOpen) {
            g.startOpen = false;
            MicaWin32kShowWindow(g.startMenu, SW_HIDE);
            redrawDesktop();
        } else {
            switch (micant::explorer::HitTestDesktop(x, y)) {
                case micant::explorer::DesktopAction::MyComputer: openView(View::MyComputer); break;
                case micant::explorer::DesktopAction::MyDocuments: openView(View::MyDocuments); break;
                case micant::explorer::DesktopAction::Network: openView(View::Network); break;
                case micant::explorer::DesktopAction::RecycleBin: openView(View::RecycleBin); break;
                case micant::explorer::DesktopAction::None: break;
            }
        }
        return 0;
    }
    if (hwnd == g.startMenu && message == WM_LBUTTONUP) {
        const int32_t x = static_cast<int16_t>(static_cast<uint32_t>(lParam) & 0xFFFFU);
        const int32_t y = static_cast<int16_t>((static_cast<uint32_t>(lParam) >> 16) & 0xFFFFU);
        const auto action = micant::explorer::HitTestStartMenu(x, y, MENU_W, MENU_H);
        g.startOpen = false;
        MicaWin32kShowWindow(g.startMenu, SW_HIDE);
        switch (action) {
            case micant::explorer::StartMenuAction::MyComputer: openView(View::MyComputer); break;
            case micant::explorer::StartMenuAction::MyDocuments: openView(View::MyDocuments); break;
            case micant::explorer::StartMenuAction::ControlPanel: openView(View::ControlPanel); break;
            case micant::explorer::StartMenuAction::Network: openView(View::Network); break;
            case micant::explorer::StartMenuAction::RecycleBin: openView(View::RecycleBin); break;
            case micant::explorer::StartMenuAction::Dismiss:
            case micant::explorer::StartMenuAction::Search:
            case micant::explorer::StartMenuAction::Run:
            case micant::explorer::StartMenuAction::LogOff:
            case micant::explorer::StartMenuAction::PowerOff:
                break;
        }
        redrawDesktop();
        return 0;
    }
    if (message == WM_CLOSE) {
        if (auto* app = appFromHwnd(hwnd)) {
            app->open = false;
            if (g.activeApp == static_cast<uint8_t>(app - g.apps)) g.activeApp = 0xFF;
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
        WS_POPUP | WS_VISIBLE, 0, 0, static_cast<int32_t>(g.screenW),
        static_cast<int32_t>(g.screenH - TASKBAR_H),
        nullptr, nullptr, nullptr, nullptr);
    if (!g.desktop) for (;;) __asm__ __volatile__("pause");
    g.desktopDc = MicaWin32kGetDC(g.desktop);
    void* desktopBits = nullptr;
    void* desktopBitmap = MicaWin32kCreateDibSection(g.desktopDc, static_cast<int32_t>(g.screenW),
        static_cast<int32_t>(g.screenH - TASKBAR_H), &desktopBits);
    if (!desktopBitmap || !desktopBits) for (;;) __asm__ __volatile__("pause");
    MicaWin32kSelectObject(g.desktopDc, desktopBitmap);
    g.desktopPixels = static_cast<uint32_t*>(desktopBits);

    const int32_t taskbarY = static_cast<int32_t>(g.screenH - TASKBAR_H);
    g.taskbar = MicaWin32kCreateWindowExW(WS_EX_TOPMOST, kClassName, kTaskbarTitle,
        WS_POPUP | WS_VISIBLE, 0, taskbarY, static_cast<int32_t>(g.screenW), TASKBAR_H,
        nullptr, nullptr, nullptr, nullptr);
    if (!g.taskbar) for (;;) __asm__ __volatile__("pause");
    g.taskbarDc = MicaWin32kGetDC(g.taskbar);
    void* taskbarBits = nullptr;
    void* taskbarBitmap = MicaWin32kCreateDibSection(g.taskbarDc, static_cast<int32_t>(g.screenW),
        TASKBAR_H, &taskbarBits);
    if (!taskbarBitmap || !taskbarBits) for (;;) __asm__ __volatile__("pause");
    MicaWin32kSelectObject(g.taskbarDc, taskbarBitmap);
    g.taskbarPixels = static_cast<uint32_t*>(taskbarBits);

    const int32_t menuY = static_cast<int32_t>(g.screenH) - static_cast<int32_t>(MENU_H) - static_cast<int32_t>(TASKBAR_H);
    g.startMenu = MicaWin32kCreateWindowExW(WS_EX_TOPMOST, kClassName, kStartTitle, WS_POPUP,
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
    updateClock();
    uint32_t clockPollDivider = 0;
    for (;;) {
        // Clock reads cross into the kernel and query the RTC; the taskbar only
        // needs seconds precision, so avoid doing that work on every idle spin.
        if (++clockPollDivider == 2000000U) {
            clockPollDivider = 0;
            updateClock();
        }
        MicaWin32kMessage msg{};
        while (MicaWin32kPeekMessageW(&msg, nullptr, 0, 0, 1)) {
            MicaWin32kDispatchMessageW(&msg);
        }
        __asm__ __volatile__("pause");
    }
}
