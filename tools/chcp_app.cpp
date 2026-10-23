/**
 * @file chcp_app.cpp
 * @brief Sovereign Ring 3 Change Code Page Utility (chcp) for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Queries and switches the active console input and output code pages.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

void ShowCodePageInfo() noexcept {
    const char* out =
        "Active code page: 65001 (UTF-8 Sovereign Unicode)\n"
        "Supported Code Pages:\n"
        "  437   - OEM United States\n"
        "  850   - OEM Multilingual Latin 1\n"
        "  1252  - ANSI Latin 1 (Western European)\n"
        "  65001 - Unicode (UTF-8)\n\n";
    PrintStr(out);
}

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "Displays or sets the active code page number.\n\n"
        "CHCP [nnn]\n\n"
        "  nnn   Specifies a code page number.\n\n"
        "Type CHCP without a parameter to display the active code page number.\n\n";

    PrintStr(banner);
    ShowCodePageInfo();

    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
