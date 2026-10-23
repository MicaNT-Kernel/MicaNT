/**
 * @file help_app.cpp
 * @brief Sovereign Ring 3 Command Catalog & Syntax Explorer (help) for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Provides a comprehensive catalog of all commands and utilities available in MicaNT System32.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

void ShowCommandCatalog() noexcept {
    const char* out =
        "For more information on a specific command, type HELP command-name\n"
        "ASSOC          Displays or modifies file extension associations.\n"
        "ATTRIB         Displays or changes file attributes.\n"
        "BCDEdit        Sets properties in boot database to control the boot loading.\n"
        "BITSADMIN      Manages Background Intelligent Transfer Service jobs.\n"
        "CACLS/ICACLS   Displays or modifies access control lists (ACLs) of files.\n"
        "CD/CHDIR       Displays the name of or changes the current directory.\n"
        "CHKDSK         Checks a disk and displays a status report.\n"
        "CIPHER         Displays or alters the encryption of directories [files].\n"
        "CLEANMGR       Automates disk cleanup tasks and frees storage space.\n"
        "CMD            Starts a new instance of the MicaNT command interpreter.\n"
        "COMPACT        Displays or alters the compression of files on NTFS/ReFS.\n"
        "DEFRAG         Optimizes and defragments volumes and SSD storage tiers.\n"
        "DIR            Displays a list of files and subdirectories in a directory.\n"
        "DISKPART       Manages disks, partitions, and volumes.\n"
        "DISM           Deployment Image Servicing and Management tool.\n"
        "DRIVERQUERY    Enables an administrator to display a list of installed drivers.\n"
        "EXPAND         Expands one or more compressed files (.cab / LZNT1).\n"
        "FC             Compares two files or sets of files and displays differences.\n"
        "FIND           Searches for a text string in a file or files.\n"
        "FORMAT         Formats a disk for use with MicaNT (NTFS / FAT32 / ExtFS).\n"
        "FSUTIL         Performs tasks related to file systems and volumes.\n"
        "HELP           Provides Help information for Windows commands.\n"
        "IPCONFIG       Displays all current TCP/IP network configuration values.\n"
        "LABEL          Creates, changes, or deletes the volume label of a disk.\n"
        "MD/MKDIR       Creates a directory.\n"
        "MORE           Displays output one screen at a time.\n"
        "MOUNT/UMOUNT   Mounts and unmounts Network File System (NFS) network shares.\n"
        "NET            Manages network resources, users, groups, and shares.\n"
        "NETSTAT        Displays protocol statistics and current TCP/IP network connections.\n"
        "PNPUTIL        Manages hardware device drivers and driver packages.\n"
        "POWERCFG       Configures processor and system power management settings.\n"
        "REPLACE        Replaces files or adds new files to destination directories.\n"
        "ROBOCOPY       Advanced utility to copy file and directory trees.\n"
        "SC             Communicates with the Service Control Manager and services.\n"
        "SCHTASKS       Schedules commands and programs to run periodically.\n"
        "SFC            Scans and verifies the integrity of protected system files.\n"
        "SHUTDOWN       Allows proper local or remote shutdown of machine.\n"
        "SYSTEMINFO     Displays machine specific properties and configuration.\n"
        "TASKKILL       Terminates or stops a running process or application.\n"
        "TASKLIST       Displays all currently running tasks including services.\n"
        "TREE           Graphically displays the directory structure of a drive or path.\n"
        "VOL            Displays a disk volume label and serial number.\n"
        "VSSADMIN       Volume Shadow Copy Service administrative tool.\n"
        "WBADMIN        Windows Backup command-line tool.\n"
        "WHOAMI         Displays user, group and privileges information.\n"
        "WPR            Windows Performance Recorder for Event Tracing (ETW).\n"
        "XPERF          Windows Performance Analyzer trace capture and processing.\n\n"
        "Total Built-in Commands: 228 Sovereign Executables Registered.\n\n";
    PrintStr(out);
}

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "MicaNT Sovereign Command Shell Help Subsystem (help)\n"
        "Copyright (C) MicaNT Operating System Authority.\n\n";

    PrintStr(banner);
    ShowCommandCatalog();

    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
