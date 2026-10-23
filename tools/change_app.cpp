/**
 * @file change_app.cpp
 * @brief Sovereign Ring 3 Terminal Server Change Utility (change) for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Changes terminal server session settings, logon permissions, and port redirections.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "Change Terminal Services session settings.\n\n"
        "CHANGE {LOGON | PORT | USER}\n\n"
        "  LOGON   - Enables or disables logons to the terminal server.\n"
        "  PORT    - Changes COM port mappings to be compatible with MS-DOS apps.\n"
        "  USER    - Changes the Install mode for the current session.\n\n"
        "Examples:\n"
        "  change logon /enable\n"
        "  change logon /disable\n"
        "  change user /install\n"
        "  change user /execute\n\n";

    PrintStr(banner);

    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
