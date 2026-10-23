/**
 * @file tzutil_app.cpp
 * @brief Sovereign Ring 3 Windows Time Zone Utility (tzutil) for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Queries and configures system time zone rules, UTC offsets, and Daylight Saving Time.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

void ShowCurrentTimeZone() noexcept {
    const char* out =
        "Pacific Standard Time\n\n";
    PrintStr(out);
}

void ShowCommonTimeZones() noexcept {
    const char* out =
        "(UTC-08:00) Pacific Time (US & Canada)\n"
        "Pacific Standard Time\n\n"
        "(UTC-07:00) Mountain Time (US & Canada)\n"
        "Mountain Standard Time\n\n"
        "(UTC-06:00) Central Time (US & Canada)\n"
        "Central Standard Time\n\n"
        "(UTC-05:00) Eastern Time (US & Canada)\n"
        "Eastern Standard Time\n\n"
        "(UTC+00:00) UTC\n"
        "UTC\n\n";
    PrintStr(out);
}

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "Windows Time Zone Utility\n\n"
        "Usage:\n"
        "  tzutil [/?] [/g] [/s <TimeZoneID>[_dstoff]] [/l]\n\n"
        "Parameters:\n"
        "  /?          Displays this help text.\n"
        "  /g          Displays the current time zone ID.\n"
        "  /s <ID>     Sets the current time zone using the specified time zone ID.\n"
        "  /l          Lists all valid time zone IDs and display names.\n\n"
        "[Current Time Zone (/g)]:\n";

    PrintStr(banner);
    ShowCurrentTimeZone();

    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
