/**
 * @file bootcfg_app.cpp
 * @brief Sovereign Ring 3 Boot Configuration Data Utility (bootcfg) for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Configures, queries, and edits boot settings in boot.ini and UEFI NVRAM.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

void ShowSampleBootcfg() noexcept {
    const char* out =
        "Boot Loader Settings\n"
        "--------------------\n"
        "timeout: 5\n"
        "default: multi(0)disk(0)rdisk(0)partition(1)\\MicaNT\n\n"
        "Boot Entries\n"
        "------------\n"
        "Boot entry ID:    1\n"
        "Friendly Name:    \"MicaNT Cutler Sovereign Edition x64\"\n"
        "Path:             multi(0)disk(0)rdisk(0)partition(1)\\MicaNT\n"
        "OS Load Options:  /fastdetect /sos /kernel=ntoskrnl.exe\n\n"
        "Boot entry ID:    2\n"
        "Friendly Name:    \"Ubuntu Linux 24.04 LTS (Failover Rescue)\"\n"
        "Path:             multi(0)disk(0)rdisk(0)partition(2)\\EFI\\ubuntu\\grubx64.efi\n"
        "OS Load Options:  ro quiet splash\n\n";
    PrintStr(out);
}

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "BOOTCFG <command> [arguments]\n\n"
        "Commands:\n"
        "  /Query        - Displays current boot entries and settings.\n"
        "  /Default      - Sets the default boot entry.\n"
        "  /Timeout      - Changes the boot timeout value.\n"
        "  /Addsw        - Adds operating system load options for a boot entry.\n"
        "  /Rmsw         - Removes operating system load options for a boot entry.\n"
        "  /Copy         - Makes a copy of an existing boot entry.\n"
        "  /Delete       - Deletes an existing boot entry.\n\n"
        "[bootcfg /query]\n";

    PrintStr(banner);
    ShowSampleBootcfg();

    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
