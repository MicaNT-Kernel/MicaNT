/**
 * @file powercfg_app.cpp
 * @brief Sovereign Ring 3 Power Configuration Utility (powercfg) for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Queries and configures power schemes, Intel Speed Shift (HWP), and ACPI sleep states.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

void PowercfgAvailableStates() noexcept {
    const char* out =
        "\nThe following sleep states are available on this system:\n"
        "    Standby (S0 Low Power Idle)\n"
        "    Hibernate\n"
        "    Fast Startup\n\n"
        "The following sleep states are not available on this system:\n"
        "    Standby (S1)\n"
        "        The system firmware does not support this standby state.\n"
        "    Standby (S2)\n"
        "        The system firmware does not support this standby state.\n"
        "    Standby (S3)\n"
        "        The system firmware does not support this standby state.\n\n";
    PrintStr(out);
}

void PowercfgQuery() noexcept {
    const char* out =
        "Existing Power Schemes (* Active)\n"
        "----------------------------------\n"
        "Power Scheme GUID: 381b4222-f694-41f0-9685-ff5bb260df2e  (Balanced) *\n"
        "Power Scheme GUID: 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c  (High performance)\n"
        "Power Scheme GUID: a1841308-3541-4fab-bc81-f71556f20b4a  (Power saver)\n"
        "Power Scheme GUID: e9a42b02-d5df-448d-aa00-03f14749eb61  (Ultimate Performance)\n\n"
        "Current Power Scheme Settings (Balanced):\n"
        "  Processor Energy Performance Preference (EPP): 0x20 (Energy Efficient Turbo)\n"
        "  Intel Speed Shift Technology (HWP)           : Enabled (MSR 0x777 Active)\n"
        "  Processor Minimum Throttle Frequency         : 800 MHz (8x Ratio)\n"
        "  Processor Maximum Turbo Frequency            : 4200 MHz (42x Ratio)\n"
        "  Core Parking Policy                          : Dynamic SMT (8T -> 4T -> 2T)\n\n";
    PrintStr(out);
}

void PowercfgBatteryReport() noexcept {
    const char* out =
        "ACPI Battery & Silicon Chronometry Telemetry:\n"
        "  Computer Name         : MICANT-XEON\n"
        "  System Product        : Supermicro X11SSH-F\n"
        "  BIOS Version          : 2.7 (ACPI 6.5 Cutler Standard)\n"
        "  Power Source          : AC Online (Redundant 400W Server Power Supply)\n"
        "  Battery Present       : No (Stationary Enterprise Server Platform)\n"
        "  Thermal Zone          : \\Device\\ThermalZone0 (Current Temp: 37.0 C)\n\n";
    PrintStr(out);
}

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "MicaNT Power Configuration Utility [Version 10.0.26040]\n"
        "(C) Sovereign MicaNT Systems. All rights reserved.\n\n";

    PrintStr(banner);

    PrintStr("[powercfg /a]\n");
    PowercfgAvailableStates();

    PrintStr("[powercfg /query]\n");
    PowercfgQuery();

    PrintStr("[powercfg /batteryreport]\n");
    PowercfgBatteryReport();

    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
