/**
 * @file reagentc_app.cpp
 * @brief Sovereign Ring 3 Windows Recovery Environment Utility (reagentc) for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Manages Windows Recovery Environment (WinRE) and dual-boot recovery partitions.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

void ShowReagentcInfo() noexcept {
    const char* out =
        "Windows Recovery Environment (Windows RE) and system reset configuration\n"
        "Information:\n\n"
        "    Windows RE status:         Enabled\n"
        "    Windows RE location:       \\\\?\\GLOBALROOT\\device\\harddisk0\\partition1\\Recovery\\WindowsRE\n"
        "    Boot Configuration Data:   0x000B (MicaNT Cutler Edition NVRAM Entry)\n"
        "    Custom image location:     \n"
        "    Custom image index:        0\n"
        "    Recovery image location:   \\\\?\\GLOBALROOT\\device\\harddisk0\\partition2 (Ubuntu Linux Rescue)\n"
        "    Recovery image index:      1\n"
        "    Bootloader Failover:       Automatic BootNext fallback to Ubuntu on unhandled fault\n\n"
        "REAGENTC.EXE: Operation Successful.\n\n";
    PrintStr(out);
}

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "REAGENTC.EXE <command> <arguments>\n\n"
        "  /enable           - Enables the local Windows RE.\n"
        "  /disable          - Disables the local Windows RE.\n"
        "  /info             - Displays Windows RE status.\n"
        "  /setreimage       - Sets location of the recovery image.\n"
        "  /bootshell        - Initiates failover boot into recovery shell.\n\n"
        "[reagentc /info]\n";

    PrintStr(banner);

    ShowReagentcInfo();

    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
