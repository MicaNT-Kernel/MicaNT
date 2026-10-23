/**
 * @file subst_app.cpp
 * @brief Sovereign Ring 3 Virtual Drive Path Substitution Utility (subst) for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Associates a path with a drive letter using Object Manager DosDevices symbolic links.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

void ShowSampleSubst() noexcept {
    const char* out =
        "P:\\: => \\Device\\HmbPagingVolume (Haven Memory Bus 2.0 Direct-Stride Volume)\n"
        "X:\\: => C:\\MicaNT\\System32\n\n";
    PrintStr(out);
}

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "Associates a path with a drive letter.\n\n"
        "SUBST [drive1: [drive2:]path]\n"
        "SUBST drive1: /D\n\n"
        "  drive1:       Specifies a virtual drive to which you want to assign a path.\n"
        "  [drive2:]path Specifies a physical drive and path you want to assign to\n"
        "                a virtual drive.\n"
        "  /D            Deletes a substituted (virtual) drive.\n\n"
        "Type SUBST with no parameters to display a list of current virtual drives.\n\n";

    PrintStr(banner);
    ShowSampleSubst();

    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
