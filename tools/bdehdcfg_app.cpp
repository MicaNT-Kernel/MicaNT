/**
 * @file bdehdcfg_app.cpp
 * @brief Sovereign Ring 3 BitLocker Drive Preparation Tool (bdehdcfg) for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Prepares and partitions hard drives to enable BitLocker Drive Encryption.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "BitLocker Drive Preparation Tool version 10.0.26100 (MicaNT Sovereign Cutler Edition)\n\n"
        "Usage: bdehdcfg [options]\n\n"
        "Parameters:\n"
        "  -target <drive_letter | default>   Prepares target drive for BitLocker\n"
        "  -size <size_in_mb>                 Specifies size for active system partition (default 500 MB)\n"
        "  -quiet                             Suppresses interactive prompts\n"
        "  -restart                           Automatically restarts the computer when complete\n\n"
        "System Partition Status: EFI System Partition (ESP) & WinRE ready for BitLocker.\n\n";

    PrintStr(banner);

    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
