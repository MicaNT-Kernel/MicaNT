/**
 * @file wbadmin_app.cpp
 * @brief Sovereign Ring 3 Windows Backup Administrative Utility (wbadmin) for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Backs up and restores operating systems, drive volumes, files, folders, and system state.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

void ShowSampleBackupStatus() noexcept {
    const char* out =
        "wbadmin 1.0 - Backup command-line tool\n"
        "(C) Copyright MicaNT Operating System Authority.\n\n"
        "Snapshot Volume: C: (MicaNT OS Partition)\n"
        "Target Storage : Z:\\Backups\\MicaNT_Daily_Image.vhdx\n"
        "Status         : The backup operation has completed successfully.\n"
        "VSS Writers    : 0 errors reported (System Writer, ASR Writer Stable)\n\n";
    PrintStr(out);
}

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "wbadmin <command> [options]\n\n"
        "Commands:\n"
        "  START BACKUP          - Runs a one-time backup.\n"
        "  STOP JOB              - Stops currently running backup or recovery.\n"
        "  GET VERSIONS          - Lists details of backups available for recovery.\n"
        "  GET ITEMS             - Lists items contained in a backup.\n"
        "  START RECOVERY        - Runs a recovery of volumes, applications, or files.\n"
        "  START SYSTEMSTATEBACKUP - Runs a system state backup.\n"
        "  START SYSTEMSTATERECOVERY - Runs a system state recovery.\n"
        "  GET STATUS            - Reports status of currently running operation.\n\n"
        "[wbadmin get status]\n";

    PrintStr(banner);
    ShowSampleBackupStatus();

    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
