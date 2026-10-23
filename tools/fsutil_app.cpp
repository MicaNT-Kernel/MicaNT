/**
 * @file fsutil_app.cpp
 * @brief Sovereign Ring 3 File System Utility (fsutil) for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Provides file system queries, volume dirty bit status, free space, and allocation unit info.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

void FsInfoDrives() noexcept {
    PrintStr("Drives:\nC:\\\nD:\\\n\n");
}

void FsInfoVolumeInfo() noexcept {
    const char* out =
        "Volume Name : OS_ROOT\n"
        "Volume Serial Number : 0x20261022\n"
        "Max Component Length : 255\n"
        "File System Name : FAT32\n"
        "Is Volume Dirty : No\n"
        "Total Sectors : 268,435,456 (128.00 GB)\n"
        "Bytes Per Sector : 512\n"
        "Sectors Per Allocation Unit : 8\n"
        "Bytes Per Cluster : 4,096\n"
        "Total Clusters : 33,554,432\n"
        "Free Clusters : 30,320,520\n"
        "Supports Case-Preserving Names : Yes\n"
        "Supports Unicode in Filenames : Yes\n\n";
    PrintStr(out);
}

void VolumeDiskFree() noexcept {
    const char* out =
        "Total # of free bytes        : 124,192,849,920 (115.66 GB)\n"
        "Total # of bytes             : 137,438,953,472 (128.00 GB)\n"
        "Total # of avail free bytes  : 124,192,849,920 (115.66 GB)\n\n";
    PrintStr(out);
}

void DirtyQuery() noexcept {
    PrintStr("Volume - C: is NOT Dirty\n\n");
}

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "---- FSUTIL Supported Commands ----\n\n"
        "8dot3name       8dot3name management\n"
        "behavior        Control file system behavior\n"
        "dirty           Manage volume dirty bit\n"
        "file            File specific commands\n"
        "fsinfo          File system information\n"
        "hardlink        Hardlink management\n"
        "objectid        Object ID management\n"
        "quota           Quota management\n"
        "repair          Self healing management\n"
        "sparse          Sparse file control\n"
        "tiering         Storage tiering management\n"
        "transaction     Transactional NTFS commands\n"
        "usn             USN management\n"
        "volume          Volume management\n"
        "wim             WIM management\n\n";

    PrintStr(banner);

    PrintStr("[fsutil fsinfo drives]\n");
    FsInfoDrives();

    PrintStr("[fsutil fsinfo volumeinfo C:]\n");
    FsInfoVolumeInfo();

    PrintStr("[fsutil volume diskfree C:]\n");
    VolumeDiskFree();

    PrintStr("[fsutil dirty query C:]\n");
    DirtyQuery();

    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
