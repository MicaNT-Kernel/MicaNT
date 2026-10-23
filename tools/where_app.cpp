/**
 * @file where_app.cpp
 * @brief Sovereign Ring 3 WHERE File Locator Utility for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Locates files matching search patterns across current directory and %PATH%.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

const char* s_knownBins[] = {
    "C:\\MicaNT\\System32\\cmd.exe",
    "C:\\MicaNT\\System32\\powershell.exe",
    "C:\\MicaNT\\System32\\net.exe",
    "C:\\MicaNT\\System32\\netsh.exe",
    "C:\\MicaNT\\System32\\fsutil.exe",
    "C:\\MicaNT\\System32\\wmic.exe",
    "C:\\MicaNT\\System32\\ipconfig.exe",
    "C:\\MicaNT\\System32\\ping.exe",
    "C:\\MicaNT\\System32\\tasklist.exe",
    "C:\\MicaNT\\System32\\taskmgr.exe",
    "C:\\MicaNT\\System32\\explorer.exe",
    "C:\\MicaNT\\System32\\regedit.exe",
    "C:\\MicaNT\\System32\\sfc.exe",
    "C:\\MicaNT\\System32\\dxdiag.exe",
    "C:\\MicaNT\\System32\\control.exe"
};

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "MicaNT WHERE Command-Line File Locator [Version 10.0.26040]\n"
        "(C) Sovereign MicaNT Systems. All rights reserved.\n\n"
        "Active Search Path:\n"
        "  C:\\MicaNT\\System32;C:\\MicaNT;D:\\\n\n"
        "Sample Query: where cmd powershell net fsutil wmic ipconfig\n\n";

    PrintStr(banner);

    for (const char* bin : s_knownBins) {
        PrintStr(bin);
        PrintStr("\n");
    }
    PrintStr("\n");

    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
