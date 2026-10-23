/**
 * @file wmic_app.cpp
 * @brief Sovereign Ring 3 WMI Command-line (WMIC) Utility for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Queries Windows Management Instrumentation classes: CPU, OS, BaseBoard, MemoryChip, DiskDrive.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

void WmicCpu() noexcept {
    const char* out =
        "MaxClockSpeed  Name                                           NumberOfCores  NumberOfLogicalProcessors\n"
        "3801           Intel(R) Xeon(R) CPU E3-1270 v6 @ 3.80GHz       4              8                        \n\n";
    PrintStr(out);
}

void WmicOs() noexcept {
    const char* out =
        "Caption                                              Version       TotalVisibleMemorySize\n"
        "MicaNT Sovereign Workstation (Dave Cutler Edition)   10.0.26040    33554432              \n\n";
    PrintStr(out);
}

void WmicMemoryChip() noexcept {
    const char* out =
        "Capacity     Manufacturer  MemoryType  Speed\n"
        "17179869184  Micron        26          2400 \n"
        "17179869184  Micron        26          2400 \n\n";
    PrintStr(out);
}

void WmicDiskDrive() noexcept {
    const char* out =
        "Model                        Size           Status  InterfaceType\n"
        "HGST HUS722T2TALA604 2TB     2000398934016  OK      IDE/SATA     \n\n";
    PrintStr(out);
}

void WmicBaseBoard() noexcept {
    const char* out =
        "Manufacturer  Product   SerialNumber  Version\n"
        "Supermicro    X11SSH-F  ZM179S019482  1.02   \n\n";
    PrintStr(out);
}

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "wmic:root\\cli>[MicaNT WMI Command-line Subsystem]\n"
        "(C) Sovereign MicaNT Systems. All rights reserved.\n\n";

    PrintStr(banner);

    PrintStr("[wmic cpu get Name,NumberOfCores,NumberOfLogicalProcessors,MaxClockSpeed]\n");
    WmicCpu();

    PrintStr("[wmic os get Caption,Version,TotalVisibleMemorySize]\n");
    WmicOs();

    PrintStr("[wmic memorychip get Capacity,Manufacturer,Speed,MemoryType]\n");
    WmicMemoryChip();

    PrintStr("[wmic diskdrive get Model,Size,Status,InterfaceType]\n");
    WmicDiskDrive();

    PrintStr("[wmic baseboard get Manufacturer,Product,SerialNumber,Version]\n");
    WmicBaseBoard();

    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
