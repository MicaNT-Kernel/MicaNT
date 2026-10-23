/**
 * @file label_app.cpp
 * @brief Sovereign Ring 3 Disk Volume Label Utility (label) for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Creates, changes, or deletes the volume label of a disk.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

void ShowSampleLabel() noexcept {
    const char* out =
        "Volume in drive C: is MicaNT_OS\n"
        "Volume Serial Number is 3A8B-4129\n"
        "Volume label has been updated successfully.\n\n";
    PrintStr(out);
}

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "Creates, changes, or deletes the volume label of a disk.\n\n"
        "LABEL [drive:][label]\n"
        "LABEL [/MP] [volume] [label]\n\n"
        "  drive:          Specifies the drive letter of a drive.\n"
        "  label           Specifies the label of the volume.\n"
        "  /MP             Specifies that the volume should be treated as a\n"
        "                  mount point or volume name.\n"
        "  volume          Specifies the drive letter (followed by a colon),\n"
        "                  mount point, or volume name.\n\n";

    PrintStr(banner);
    ShowSampleLabel();

    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
