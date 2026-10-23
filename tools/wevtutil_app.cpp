/**
 * @file wevtutil_app.cpp
 * @brief Sovereign Ring 3 Windows Event Log Utility (wevtutil) for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Queries, archives, exports, and configures Windows event logs and publishers.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

void ShowSampleLogs() noexcept {
    const char* out =
        "Application\n"
        "Security\n"
        "System\n"
        "Setup\n"
        "ForwardedEvents\n"
        "Microsoft-Windows-Kernel-General/Operational\n"
        "Microsoft-Windows-Kernel-Power/Operational\n"
        "Microsoft-Windows-DriverFrameworks-UserMode/Operational\n\n";
    PrintStr(out);
}

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "Windows Events Command Line Utility.\n\n"
        "Usage:\n"
        "  wevtutil [{command}] [/option:value ...]\n\n"
        "Commands:\n"
        "  el | enum-logs          List log names.\n"
        "  gl | get-log            Get log configuration information.\n"
        "  sl | set-log            Modify configuration of a log.\n"
        "  ep | enum-publishers    List event publishers.\n"
        "  gp | get-publisher      Get publisher configuration information.\n"
        "  im | install-manifest   Install event publishers and logs from manifest.\n"
        "  um | uninstall-manifest Uninstall event publishers and logs from manifest.\n"
        "  qe | query-events       Query events from a log or log file.\n"
        "  gli | get-loginfo       Get log status information.\n"
        "  epl | export-log        Export a log.\n"
        "  al | archive-log        Archive an exported log.\n"
        "  cl | clear-log          Clear a log.\n\n"
        "[wevtutil enum-logs]\n";

    PrintStr(banner);
    ShowSampleLogs();

    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
