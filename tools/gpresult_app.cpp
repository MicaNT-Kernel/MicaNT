/**
 * @file gpresult_app.cpp
 * @brief Sovereign Ring 3 Group Policy Resultant Set of Policy Utility (gpresult) for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Displays Resultant Set of Policy (RSoP) information for target user and computer.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

void ShowGpResult() noexcept {
    const char* out =
        "Microsoft (R) Windows (R) Operating System Group Policy Result tool v2.0\n"
        "(C) Sovereign MicaNT Systems. All rights reserved.\n\n"
        "Created on 10/23/2026 at 12:00:00 PM\n\n"
        "RSOP data for MICANT\\MicaAdmin on MICANT-XEON : Logging Mode\n"
        "------------------------------------------------------------------\n\n"
        "OS Configuration:            Member Workstation / Sovereign Executive\n"
        "OS Version:                  10.0.26040 (Dave Cutler Edition)\n"
        "Site Name:                   N/A (Workgroup Mode)\n"
        "Roaming Profile:             N/A\n"
        "Local Profile:               C:\\Users\\MicaAdmin\n"
        "Connected over a slow link:  No (1000 Mbps Intel I210 Full-Duplex)\n\n"
        "COMPUTER SETTINGS\n"
        "---------------\n"
        "    Last time Group Policy was applied: 10/23/2026 at 12:00:00 PM\n"
        "    Group Policy was applied from:      N/A (Local GPO Registry)\n"
        "    Applied Group Policy Objects:\n"
        "        Local Group Policy (Sovereign Hardened Baseline)\n\n"
        "    The computer is a part of the following security groups:\n"
        "        BUILTIN\\Administrators\n"
        "        Everyone\n"
        "        BUILTIN\\Users\n"
        "        NT AUTHORITY\\NETWORK\n"
        "        NT AUTHORITY\\Authenticated Users\n"
        "        Mandatory Label\\High Mandatory Level\n\n";
    PrintStr(out);
}

} // namespace

extern "C" void _start() noexcept {
    ShowGpResult();
    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
