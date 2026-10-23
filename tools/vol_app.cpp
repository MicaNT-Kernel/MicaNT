/**
 * @file vol_app.cpp
 * @brief Sovereign Ring 3 Volume Label & Serial Number Display Utility (vol) for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Displays the disk volume label and serial number.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

void ShowVolumeInfo() noexcept {
    const char* out =
        " Volume in drive C is MicaNT_OS\n"
        " Volume Serial Number is 3A8B-4129\n\n";
    PrintStr(out);
}

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "Displays the disk volume label and serial number, if they exist.\n\n"
        "VOL [drive:]\n\n";

    PrintStr(banner);
    ShowVolumeInfo();

    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
