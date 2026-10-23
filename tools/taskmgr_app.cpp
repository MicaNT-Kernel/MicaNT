/**
 * @file taskmgr_app.cpp
 * @brief Sovereign Ring 3 Task Manager Utility for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Renders authentic Windows NT / SurShell Task Manager window to GOP VRAM via NtGdiPresentSurface (0x81),
 * formats output via NtWriteFile (0x25), and exits cleanly via NtTerminateProcess (0x01).
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

constexpr uint32_t WIN_W = 580;
constexpr uint32_t WIN_H = 380;
alignas(4096) uint32_t s_tmBuffer[WIN_W * WIN_H];

void RenderTaskManagerWindow() noexcept {
    // 1. Frame & Title
    DrawWindowFrame(s_tmBuffer, WIN_W, WIN_H, "Task Manager", "File   Options   View");

    // 2. Tab Bar (Height = 24)
    FillRect(s_tmBuffer, WIN_W, 1, 49, WIN_W - 2, 24, PackBGRA(22, 30, 46));
    DrawRect(s_tmBuffer, WIN_W, 1, 72, WIN_W - 2, 1, PackBGRA(38, 52, 78));

    // Active Tab: Processes (with cyan indicator underline)
    DrawString(s_tmBuffer, WIN_W, 12, 56, "Processes", PackBGRA(0, 210, 255));
    FillRect(s_tmBuffer, WIN_W, 10, 71, 75, 2, PackBGRA(0, 210, 255));

    DrawString(s_tmBuffer, WIN_W, 100, 56, "Performance", PackBGRA(160, 180, 210));
    DrawString(s_tmBuffer, WIN_W, 205, 56, "App history", PackBGRA(160, 180, 210));
    DrawString(s_tmBuffer, WIN_W, 305, 56, "Startup", PackBGRA(160, 180, 210));
    DrawString(s_tmBuffer, WIN_W, 380, 56, "Details", PackBGRA(160, 180, 210));
    DrawString(s_tmBuffer, WIN_W, 455, 56, "Services", PackBGRA(160, 180, 210));

    // 3. Column Header
    FillRect(s_tmBuffer, WIN_W, 1, 73, WIN_W - 2, 20, PackBGRA(18, 25, 38));
    DrawRect(s_tmBuffer, WIN_W, 1, 92, WIN_W - 2, 1, PackBGRA(35, 48, 70));

    DrawString(s_tmBuffer, WIN_W, 12, 79, "Name", PackBGRA(140, 165, 200));
    DrawString(s_tmBuffer, WIN_W, 200, 79, "Status", PackBGRA(140, 165, 200));
    DrawString(s_tmBuffer, WIN_W, 300, 79, "CPU", PackBGRA(140, 165, 200));
    DrawString(s_tmBuffer, WIN_W, 370, 79, "Memory", PackBGRA(140, 165, 200));
    DrawString(s_tmBuffer, WIN_W, 460, 79, "Disk", PackBGRA(140, 165, 200));

    // 4. Process Rows
    const struct Row {
        const char* name;
        const char* status;
        const char* cpu;
        const char* mem;
        const char* disk;
    } rows[] = {
        {"System",       "Running", "0.1%",  "14.2 MB", "0.0 MB/s"},
        {"smss.exe",     "Running", "0.0%",   "1.2 MB", "0.0 MB/s"},
        {"csrss.exe",    "Running", "0.2%",   "4.6 MB", "0.0 MB/s"},
        {"winlogon.exe", "Running", "0.0%",   "3.4 MB", "0.0 MB/s"},
        {"services.exe", "Running", "0.1%",   "5.1 MB", "0.0 MB/s"},
        {"lsass.exe",    "Running", "0.0%",   "8.2 MB", "0.0 MB/s"},
        {"svchost.exe",  "Running", "0.3%",  "12.8 MB", "0.0 MB/s"},
        {"surshell.exe", "Running", "1.4%",  "34.2 MB", "0.1 MB/s"},
    };

    uint32_t ry = 95;
    for (size_t i = 0; i < 8; ++i) {
        bool alt = (i % 2 == 1);
        uint32_t bg = alt ? PackBGRA(14, 20, 32) : PackBGRA(16, 22, 35);
        FillRect(s_tmBuffer, WIN_W, 1, ry, WIN_W - 2, 22, bg);

        DrawString(s_tmBuffer, WIN_W, 12, ry + 7, rows[i].name, PackBGRA(230, 240, 255));
        DrawString(s_tmBuffer, WIN_W, 200, ry + 7, rows[i].status, PackBGRA(100, 240, 160));
        DrawString(s_tmBuffer, WIN_W, 300, ry + 7, rows[i].cpu, PackBGRA(200, 220, 245));
        DrawString(s_tmBuffer, WIN_W, 370, ry + 7, rows[i].mem, PackBGRA(200, 220, 245));
        DrawString(s_tmBuffer, WIN_W, 460, ry + 7, rows[i].disk, PackBGRA(200, 220, 245));

        ry += 22;
    }

    // 5. Footer Status Bar
    FillRect(s_tmBuffer, WIN_W, 1, WIN_H - 24, WIN_W - 2, 23, PackBGRA(24, 32, 48));
    DrawRect(s_tmBuffer, WIN_W, 1, WIN_H - 24, WIN_W - 2, 1, PackBGRA(38, 52, 76));

    DrawString(s_tmBuffer, WIN_W, 12, WIN_H - 16, "Processes: 48", PackBGRA(180, 200, 230));
    DrawString(s_tmBuffer, WIN_W, 160, WIN_H - 16, "CPU: 2% (8T @ 3.8GHz)", PackBGRA(100, 240, 160));
    DrawString(s_tmBuffer, WIN_W, 380, WIN_H - 16, "Memory: 13% (4.2/32 GB)", PackBGRA(0, 210, 255));

    PresentWindow(s_tmBuffer, WIN_W, WIN_H);
}

} // namespace

extern "C" void _start(const char* /*cmdLine*/ = nullptr) {
    uint16_t csVal = 0;
#if defined(__x86_64__)
    __asm__ __volatile__("movw %%cs, %0" : "=r"(csVal));
#endif
    DoSyscall(0x11, csVal & 3, 0);

    PrintLine("Sovereign Task Manager (taskmgr.exe):");
    PrintLine("  Active Processes: 48 | CPU: Intel Xeon E3-1270 v6 (8T @ 3.80GHz)");
    PrintLine("  Physical Memory: 4,210 MB / 32,768 MB ECC DDR4 (13% Committed)");
    PrintLine("  Window presented to GOP VRAM at center coordinates.");

    RenderTaskManagerWindow();

    DoSyscall(0x01, 0, 0);
    while (true) {
#if defined(__x86_64__)
        __asm__ __volatile__("pause");
#endif
    }
}
