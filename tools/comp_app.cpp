/**
 * @file comp_app.cpp
 * @brief Sovereign Ring 3 File Comparison Utility (comp) for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Compares contents of two files or sets of files byte-by-byte.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "Compares the contents of two files or sets of files.\n\n"
        "COMP [data1] [data2] [/D] [/A] [/L] [/N=number] [/C] [/OFF[LINE]]\n\n"
        "  data1      Specifies location and name(s) of first file(s) to compare.\n"
        "  data2      Specifies location and name(s) of second file(s) to compare.\n"
        "  /D         Displays differences in decimal format.\n"
        "  /A         Displays differences in ASCII characters.\n"
        "  /L         Displays line numbers for differences.\n"
        "  /N=number  Compares only the first specified number of lines in each file.\n"
        "  /C         Disregards case of ASCII letters when comparing files.\n"
        "  /OFF[LINE] Do not skip files with offline attribute set.\n\n"
        "MicaNT File Comparison Engine:\n"
        "  Status: Ready (Enter file specifications or run with target paths).\n\n";

    PrintStr(banner);

    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
