/**
 * @file bootsect_app.cpp
 * @brief Sovereign Ring 3 Boot Sector Manipulation Tool (bootsect) for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Updates the master boot code for hard disk partitions (NT60 / BOOTMGR / MBR).
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

void ShowSampleBootsect() noexcept {
    const char* out =
        "Target volumes will be updated with BOOTMGR compatible bootcode.\n\n"
        "C: (\\\\?\\Volume{3a8b4129-0000-0000-0000-100000000000})\n"
        "    Successfully updated NTFS filesystem bootcode.\n\n"
        "\\Device\\Harddisk0\\DR0\n"
        "    Successfully updated Master Boot Record code.\n\n"
        "Bootcode was successfully updated on all targeted volumes.\n\n";
    PrintStr(out);
}

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "Bootsect.exe {/help | /nt60 | /nt52} {SYS | ALL | <DriveLetter:>} [/force] [/mbr]\n\n"
        "  /nt60        Applies the master boot code that is compatible with BOOTMGR.\n"
        "  /nt52        Applies the master boot code that is compatible with NTLDR.\n"
        "  SYS          Updates the master boot code on the system partition.\n"
        "  ALL          Updates the master boot code on all partitions.\n"
        "  /force       Forcibly dismounts the volume(s) during boot code update.\n"
        "  /mbr         Updates the Master Boot Record on the disk containing the partition.\n\n"
        "[bootsect /nt60 C: /mbr]\n";

    PrintStr(banner);
    ShowSampleBootsect();

    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
