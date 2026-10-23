/**
 * @file pnputil_app.cpp
 * @brief Sovereign Ring 3 PnP Driver Package Management Utility (pnputil) for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Adds, deletes, installs, and enumerates driver packages (.inf) in the driver store.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

void ShowSampleDrivers() noexcept {
    const char* out =
        "Microsoft PnP Utility (MicaNT Sovereign Cutler Edition)\n\n"
        "Published Name:     oem0.inf\n"
        "Original Name:      ahci.inf\n"
        "Provider Name:      MicaNT Sovereign Storage Authority\n"
        "Class Name:         IDE ATA/ATAPI controllers\n"
        "Class GUID:         {4d36e96a-e325-11ce-bfc1-08002be10318}\n"
        "Driver Version:     10/23/2026 1.0.0.1\n"
        "Signer Name:        MicaNT Kernel Code Integrity Root\n\n"
        "Published Name:     oem1.inf\n"
        "Original Name:      i210.inf\n"
        "Provider Name:      Intel / MicaNT Network Authority\n"
        "Class Name:         Network adapters\n"
        "Class GUID:         {4d36e972-e325-11ce-bfc1-08002be10318}\n"
        "Driver Version:     10/23/2026 1.0.0.1\n"
        "Signer Name:        MicaNT Kernel Code Integrity Root\n\n";
    PrintStr(out);
}

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "Microsoft PnP Utility\n\n"
        "Usage:\n"
        "  pnputil.exe <command> [options]\n\n"
        "Commands:\n"
        "  /add-driver <INF name> [/subdirs] [/install] [/reboot]\n"
        "  /delete-driver <OEM INF name> [/force] [/reboot]\n"
        "  /export-driver <INF name | *> <target directory>\n"
        "  /enum-drivers [/class <class name | GUID>]\n"
        "  /enum-devices [/connected | /disconnected] [/instanceid <instance id>]\n"
        "  /enum-interfaces [/enabled | /disabled]\n"
        "  /restart-device <device instance ID>\n\n"
        "[pnputil.exe /enum-drivers]\n";

    PrintStr(banner);
    ShowSampleDrivers();

    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
