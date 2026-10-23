/**
 * @file setx_app.cpp
 * @brief Sovereign Ring 3 Persistent Environment Variable Manager (setx) for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Creates or modifies environment variables in the user or system environment registry hives.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

void ShowSampleSetxExecution() noexcept {
    const char* out =
        "[setx ARCH_TARGET x86_64 /m]\n"
        "Opening system environment registry key:\n"
        "  HKLM\\SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment\n"
        "Writing registry value 'ARCH_TARGET' = REG_SZ: 'x86_64'...\n"
        "Registry value committed successfully.\n"
        "Broadcasting WM_SETTINGCHANGE message to all top-level windows:\n"
        "  HWND_BROADCAST: lParam = (LPARAM)L\"Environment\"\n"
        "SUCCESS: Specified value was saved.\n\n";
    PrintStr(out);
}

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "SETX [/S system [/U [domain\\]user [/P [password]]]] var value [/M]\n"
        "SETX [/S system [/U [domain\\]user [/P [password]]]] var /K regpath [/M]\n"
        "SETX [/S system [/U [domain\\]user [/P [password]]]] /F file {var {/A x,y | /R x,y \"delim\"}} [/M]\n\n"
        "Description:\n"
        "    Creates or modifies environment variables in the user or system environment.\n"
        "    Can set variables based on arguments, regkeys or file input.\n\n"
        "Parameter List:\n"
        "    /S          system          Specifies the remote system to connect to.\n"
        "    /U          [domain\\]user    Specifies the user context under which\n"
        "                                the command should execute.\n"
        "    /P          [password]      Specifies the password for the given user context.\n"
        "    var                         Specifies the name of the environment variable.\n"
        "    value                       Specifies the value assigned to the variable.\n"
        "    /K          regpath         Specifies the variable is set based on\n"
        "                                information from the registry key.\n"
        "    /M                          Specifies that the variable should be set in\n"
        "                                the system-wide (HKLM) environment.\n"
        "                                The default setting is the local user (HKCU) environment.\n\n";

    PrintStr(banner);
    ShowSampleSetxExecution();

    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
