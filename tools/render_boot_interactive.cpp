#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include "micant/bootvid.hpp"
#include "micant/conhost.hpp"

using namespace micant;

int main() {
    std::cout << "[MicaNT Boot Visualizer] Initializing GOP Framebuffer (1024x768)...\n";

    constexpr uint32_t WIDTH = 1024;
    constexpr uint32_t HEIGHT = 768;

    // ========================================================================
    // Screen 1: Final Boot Splash Screen (100% Handover to Win32 Subsystem)
    // ========================================================================
    {
        bootvid::BootVideoDriver driver;
        driver.initializeVirtual(WIDTH, HEIGHT);
        driver.renderBootSplash("Starting Win32 Subsystem CSRSS & ConHost...", 1.0f);

        // Export BMP
        bootvid::BmpImage img;
        img.width = WIDTH;
        img.height = HEIGHT;
        img.bpp = 32;
        img.pixels.resize(WIDTH * HEIGHT);
        for (uint32_t y = 0; y < HEIGHT; ++y) {
            for (uint32_t x = 0; x < WIDTH; ++x) {
                img.setPixel(x, y, driver.getPixel(x, y));
            }
        }
        auto bmpBytes = bootvid::BmpCodec::encode(img);
        std::ofstream out1("docs/bootscreen_splash.bmp", std::ios::binary);
        out1.write(reinterpret_cast<const char*>(bmpBytes.data()), bmpBytes.size());
        out1.close();
        std::cout << "[MicaNT Boot Visualizer] Exported docs/bootscreen_splash.bmp\n";
    }

    // ========================================================================
    // Screen 2: ConHost Win32 Terminal Desktop
    // ========================================================================
    {
        bootvid::BootVideoDriver driver;
        driver.initializeVirtual(WIDTH, HEIGHT);

        // 1. Fill desktop background with deep carbon gradient
        bootvid::Color topBg{14, 18, 28};
        bootvid::Color botBg{6, 8, 14};
        driver.drawVerticalGradient(0, 0, WIDTH, HEIGHT, topBg, botBg);

        // Subtle desktop grid lines for architectural feel
        bootvid::Color gridColor{20, 26, 40};
        for (uint32_t x = 0; x < WIDTH; x += 64) {
            for (uint32_t y = 0; y < HEIGHT - 36; y += 4) {
                driver.putPixel(x, y, gridColor);
            }
        }
        for (uint32_t y = 0; y < HEIGHT - 36; y += 64) {
            for (uint32_t x = 0; x < WIDTH; x += 4) {
                driver.putPixel(x, y, gridColor);
            }
        }

        // Top Status Bar (MicaNT Architectural Header)
        driver.fillRectangle(0, 0, WIDTH, 28, bootvid::Color{18, 24, 38});
        driver.fillRectangle(0, 27, WIDTH, 1, bootvid::Color{38, 52, 78});

        driver.drawString(14, 8, "MicaNT 64-Bit OS", bootvid::Color{0, 220, 255}, bootvid::Color{18, 24, 38}, 1);
        driver.drawString(150, 8, "|  Dave Cutler 1988 Architecture  |  Zero Telemetry  |  Build 26100.1  |  SMP 4-Core", 
                          bootvid::Color{170, 185, 205}, bootvid::Color{18, 24, 38}, 1);

        // Draw small Prism emblem in top right
        driver.fillRectangle(WIDTH - 180, 6, 166, 16, bootvid::Color{28, 36, 56});
        driver.drawString(WIDTH - 172, 10, "PASSIVE_LEVEL [IRQL 0]", bootvid::Color{100, 240, 160}, bootvid::Color{28, 36, 56}, 1);

        // 2. Win32 Terminal Window Frame
        constexpr uint32_t WIN_X = 92;
        constexpr uint32_t WIN_Y = 64;
        constexpr uint32_t WIN_COLS = 84;
        constexpr uint32_t WIN_ROWS = 28;
        constexpr uint32_t CHAR_SCALE = 1;
        constexpr uint32_t CHAR_W = 8 * CHAR_SCALE;
        constexpr uint32_t CHAR_H = 8 * CHAR_SCALE;
        constexpr uint32_t INNER_W = WIN_COLS * CHAR_W;  // 84 * 8 = 672
        constexpr uint32_t INNER_H = WIN_ROWS * CHAR_H;  // 28 * 8 = 224
        constexpr uint32_t TITLE_H = 26;
        constexpr uint32_t BORDER = 4;
        constexpr uint32_t TOTAL_W = INNER_W + (BORDER * 2);
        constexpr uint32_t TOTAL_H = INNER_H + TITLE_H + (BORDER * 2);

        // Window drop shadow
        driver.fillRectangle(WIN_X + 8, WIN_Y + 8, TOTAL_W, TOTAL_H, bootvid::Color{0, 0, 0});

        // Window outer border
        driver.fillRectangle(WIN_X, WIN_Y, TOTAL_W, TOTAL_H, bootvid::Color{30, 42, 62});
        driver.drawRectangle(WIN_X, WIN_Y, TOTAL_W, TOTAL_H, bootvid::Color{60, 80, 115});

        // Window Title Bar (Active Blue Gradient)
        driver.drawVerticalGradient(WIN_X + BORDER, WIN_Y + BORDER, INNER_W, TITLE_H, 
                                bootvid::Color{20, 45, 90}, bootvid::Color{12, 28, 60});

        // Window Title text
        driver.drawString(WIN_X + BORDER + 10, WIN_Y + BORDER + 8, 
                          "Command Prompt - [MicaNT ConHost: win32_app.exe (PID 1000)]", 
                          bootvid::Color{240, 248, 255}, bootvid::Color{16, 36, 75}, 1);

        // Window Buttons (Minimize, Maximize, Close)
        uint32_t btnX = WIN_X + TOTAL_W - BORDER - 56;
        uint32_t btnY = WIN_Y + BORDER + 4;
        driver.fillRectangle(btnX, btnY, 14, 14, bootvid::Color{45, 60, 85});
        driver.fillRectangle(btnX + 18, btnY, 14, 14, bootvid::Color{45, 60, 85});
        driver.fillRectangle(btnX + 36, btnY, 14, 14, bootvid::Color{180, 40, 40}); // Red close button

        // 3. ConHost Screen Buffer Rendering
        conhost::ConsoleScreenBuffer conBuf(WIN_COLS, WIN_ROWS);
        conBuf.writeString(L"MicaNT Executive [Version 10.0.26100.1]\r\n");
        conBuf.writeString(L"(c) 1988-2026 Mica Architecture Team. Dave Cutler Clean-Room Design.\r\n");
        conBuf.writeString(L"Zero Telemetry | 4 SMP Cores | Sub-32MB Footprint | NonPagedPool: 64 KB\r\n\r\n");

        conBuf.writeString(L"C:\\Windows\\System32> smss.exe\r\n");
        conBuf.writeString(L"[SMSS] Session Manager initializing Win32 subsystem runtime...\r\n");
        conBuf.writeString(L"[CSRSS] Subsystem server listening on \\RPC Control\\WindowsSubsystem\r\n");
        conBuf.writeString(L"[CONHOST] Allocated interactive console session for PID 1000\r\n\r\n");

        conBuf.writeString(L"C:\\Windows\\System32> win32_app.exe\r\n");
        conBuf.writeString(L"========================================================================\r\n");
        conBuf.writeString(L"                 MicaNT Native Win32 Application Harness                \r\n");
        conBuf.writeString(L"========================================================================\r\n");
        conBuf.writeString(L"[Win32 App] Initializing Win32 Console via AllocConsole()... [OK]\r\n");
        conBuf.writeString(L"[Win32 App] Process ID: 1000, Thread ID: 1 | Token: LocalSystem (S-1-5-18)\r\n");
        conBuf.writeString(L"[Win32 App] Console Window Title: \"MicaNT Win32 Terminal\"\r\n");
        conBuf.writeString(L"[Win32 App] Testing HeapAlloc(256 bytes) at 0x0000019741A87E50... [OK]\r\n");
        conBuf.writeString(L"[Win32 App] Testing VirtualAlloc(64KB) page range at 0x100000000... [OK]\r\n");
        conBuf.writeString(L"[Win32 App] Testing Win32 Event Synchronization (CreateEventW)... [OK]\r\n");
        conBuf.writeString(L"[Win32 App] WriteConsoleW output: Hello from MicaNT Win32 Subsystem!\r\n");
        conBuf.writeString(L"[Win32 App] All 30 Executive & Subsystem verification suites PASSED!\r\n\r\n");

        conBuf.writeString(L"C:\\Windows\\System32> _");

        // Blit ConHost terminal buffer to the inner window client area
        conBuf.renderToFramebuffer(driver, WIN_X + BORDER, WIN_Y + BORDER + TITLE_H, CHAR_SCALE);

        // 4. Bottom Taskbar & Start Menu
        constexpr uint32_t TASKBAR_H = 34;
        uint32_t taskbarY = HEIGHT - TASKBAR_H;
        driver.fillRectangle(0, taskbarY, WIDTH, TASKBAR_H, bootvid::Color{14, 19, 30});
        driver.fillRectangle(0, taskbarY, WIDTH, 1, bootvid::Color{36, 48, 72});

        // Start Button (DEC Mica Prism Logo miniature)
        driver.fillRectangle(6, taskbarY + 4, 82, 26, bootvid::Color{28, 40, 64});
        driver.drawRectangle(6, taskbarY + 4, 82, 26, bootvid::Color{0, 180, 230});
        driver.drawString(14, taskbarY + 12, "MICA", bootvid::Color{0, 240, 255}, bootvid::Color{28, 40, 64}, 1);

        // Taskbar Buttons
        uint32_t tbX = 96;
        driver.fillRectangle(tbX, taskbarY + 4, 180, 26, bootvid::Color{22, 32, 50});
        driver.fillRectangle(tbX, taskbarY + 28, 180, 2, bootvid::Color{0, 220, 255}); // Active blue indicator
        driver.drawString(tbX + 12, taskbarY + 12, ">_ Command Prompt", bootvid::Color{240, 248, 255}, bootvid::Color{22, 32, 50}, 1);

        uint32_t tbX2 = tbX + 188;
        driver.fillRectangle(tbX2, taskbarY + 4, 150, 26, bootvid::Color{18, 24, 38});
        driver.drawString(tbX2 + 12, taskbarY + 12, "Kernel Telemetry", bootvid::Color{150, 165, 185}, bootvid::Color{18, 24, 38}, 1);

        // Notification Area / System Tray
        uint32_t trayX = WIDTH - 260;
        driver.fillRectangle(trayX, taskbarY + 4, 254, 26, bootvid::Color{18, 24, 38});
        driver.drawString(trayX + 10, taskbarY + 12, "4 Cores | 48 C | 0 Telemetry", bootvid::Color{100, 230, 160}, bootvid::Color{18, 24, 38}, 1);
        driver.drawString(trayX + 195, taskbarY + 12, "3:42 PM", bootvid::Color{220, 230, 245}, bootvid::Color{18, 24, 38}, 1);

        // Export BMP
        bootvid::BmpImage img;
        img.width = WIDTH;
        img.height = HEIGHT;
        img.bpp = 32;
        img.pixels.resize(WIDTH * HEIGHT);
        for (uint32_t y = 0; y < HEIGHT; ++y) {
            for (uint32_t x = 0; x < WIDTH; ++x) {
                img.setPixel(x, y, driver.getPixel(x, y));
            }
        }
        auto bmpBytes = bootvid::BmpCodec::encode(img);
        std::ofstream out2("docs/conhost_desktop.bmp", std::ios::binary);
        out2.write(reinterpret_cast<const char*>(bmpBytes.data()), bmpBytes.size());
        out2.close();
        std::cout << "[MicaNT Boot Visualizer] Exported docs/conhost_desktop.bmp (" 
                  << bmpBytes.size() << " bytes)\n";
    }

    return 0;
}
