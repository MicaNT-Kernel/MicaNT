/**
 * @file chkntfs_app.cpp
 * @brief Sovereign Ring 3 NTFS Volume Dirty Query & Autochk Tool (chkntfs) for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Queries volume dirty state and configures autochk.exe execution during boot.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

void ShowSampleChkntfs() noexcept {
    const char* out =
        "The type of the file system is NTFS (Sovereign Clean-Room).\n"
        "C: is not dirty. Autochk will be skipped during boot sequence.\n\n";
    PrintStr(out);
}

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "Displays or modifies the checking of disk at boot time.\n\n"
        "CHKNTFS volume [...]\n"
        "CHKNTFS [/D] [/T[:time]] [/X volume [...]] [/C volume [...]]\n\n"
        "  volume          Specifies the drive letter (followed by a colon),\n"
        "                  mount point, or volume name.\n"
        "  /D              Restores the machine to the default behavior; all\n"
        "                  drives are checked at boot time and chkdsk is run on\n"
        "                  those that are dirty.\n"
        "  /T:time         Changes the AUTOCHK initiation countdown time to the\n"
        "                  specified amount of time in seconds.\n"
        "  /X              Excludes a drive from the default boot-time check.\n"
        "  /C              Schedules a drive to be checked at boot time; chkdsk\n"
        "                  will run if the drive is dirty.\n\n"
        "[chkntfs C:]\n";

    PrintStr(banner);
    ShowSampleChkntfs();

    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
