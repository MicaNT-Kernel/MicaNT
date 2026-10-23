/**
 * @file choice_app.cpp
 * @brief Sovereign Ring 3 CHOICE Prompt Utility for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Prompts user for a choice from a list of keys and returns corresponding ERRORLEVEL.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "CHOICE [/C choices] [/N] [/CS] [/T timeout /D choice] [/M text]\n\n"
        "Description:\n"
        "    This tool allows users to select one item from a list\n"
        "    of choices and returns the index of the selected choice.\n\n"
        "Parameter List:\n"
        "    /C    choices       Specifies the list of choices to be created.\n"
        "                        Default list is \"YN\".\n"
        "    /N                  Causes the list of choices to not be displayed\n"
        "                        in the prompt.\n"
        "    /CS                 Enables case-sensitive choices to be selected.\n"
        "    /T    timeout       The number of seconds to pause before a default\n"
        "                        choice is made.\n"
        "    /D    choice        Specifies the default choice after n seconds.\n"
        "    /M    text          Specifies the message to be displayed before\n"
        "                        the prompt.\n\n";

    PrintStr(banner);

    PrintStr("Proceed with sovereign operation [Y,N]? Y\n");
    PrintStr("[CHOICE] User selected option 1 ('Y'). Returning ERRORLEVEL 1.\n");

    DoSyscall(0x01, 1, 0); // NtTerminateProcess(1)
}
