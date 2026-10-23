/**
 * @file at_app.cpp
 * @brief Sovereign Ring 3 AT Schedule Job Utility (at) for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Schedules commands and programs to run on a computer at a specified time and date.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

void ShowSampleAtList() noexcept {
    const char* out =
        "Status ID   Day                     Time          Command Line\n"
        "-------------------------------------------------------------------------------\n"
        "        1   Each M T W Th F         04:00 AM      defrag C: /O\n"
        "        2   Today                   11:00 PM      cleanmgr /sagerun:1\n\n";
    PrintStr(out);
}

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "The AT command schedules commands and programs to run on a computer at\n"
        "a specified time and date. The Schedule service must be running to use\n"
        "the AT command.\n\n"
        "AT [\\\\computername] [ [id] [/DELETE] | [/DELETE [/YES]] ]\n"
        "AT [\\\\computername] time [/INTERACTIVE]\n"
        "    [ /EVERY:date[,...] | /NEXT:date[,...] ] \"command\"\n\n"
        "[at]\n";

    PrintStr(banner);
    ShowSampleAtList();

    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
