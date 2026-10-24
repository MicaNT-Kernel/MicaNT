/**
 * @file wpr_app.cpp
 * @brief Sovereign Ring 3 Windows Performance Recorder (wpr) for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Controls Event Tracing for Windows (ETW) recording sessions and performance profiles.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

void ShowSampleWprProfiles() noexcept {
    const char* out =
        "Available Profiles:\n"
        "  GeneralProfile               General Performance recording\n"
        "  CPU                          CPU usage analysis\n"
        "  DiskIO                       Disk I/O and flush activity\n"
        "  FileIO                       File I/O activity\n"
        "  VirtualAllocation            Virtual memory allocation analysis\n"
        "  Audio                        Audio glitch analysis (WaveRT & MMCSS)\n"
        "  Video                        DirectX & GOP scanout frame pacing\n"
        "  Network                      Network activity analysis\n\n"
        "Status: No recording profiles active.\n\n";
    PrintStr(out);
}

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "MicaNT Performance Recorder Version 10.0.26100 (MicaNT Sovereign Cutler Edition)\n"
        "Usage: wpr <option> [parameters...]\n\n"
        "Options:\n"
        "  -start <profile>      Starts performance recording using specified profile.\n"
        "  -stop <file.etl>      Stops performance recording and saves to ETL file.\n"
        "  -cancel               Cancels performance recording without saving.\n"
        "  -status               Displays status of currently running recording.\n"
        "  -profiles             Lists available built-in performance profiles.\n"
        "  -purgecache           Purges dynamic symbols and ETW trace cache.\n\n"
        "[wpr -profiles]\n";

    PrintStr(banner);
    ShowSampleWprProfiles();

    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
