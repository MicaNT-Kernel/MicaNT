/**
 * @file timeout_app.cpp
 * @brief Sovereign Ring 3 TIMEOUT Utility for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Pauses command execution with an interactive countdown timer.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

void FormatInt(char* buf, uint32_t val) noexcept {
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    char tmp[16];
    int idx = 0;
    while (val > 0) {
        tmp[idx++] = '0' + (val % 10);
        val /= 10;
    }
    int outIdx = 0;
    while (idx > 0) {
        buf[outIdx++] = tmp[--idx];
    }
    buf[outIdx] = '\0';
}

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "TIMEOUT [/T] timeout [/NOBREAK]\n\n"
        "Description:\n"
        "    This utility accepts a timeout parameter to wait for the specified\n"
        "    period of time (in seconds) or until any key is pressed. It also\n"
        "    accepts a parameter to ignore the key press.\n\n";

    PrintStr(banner);

    // Simulate 3-second countdown demonstration
    char numBuf[16];
    for (int sec = 3; sec >= 0; --sec) {
        PrintStr("Waiting for ");
        FormatInt(numBuf, static_cast<uint32_t>(sec));
        PrintStr(numBuf);
        PrintStr(" seconds, press any key to continue ...\n");

        // Small busy loop delay via pause instruction
        for (uint64_t i = 0; i < 20000000ULL; ++i) {
#if defined(__x86_64__)
            __asm__ __volatile__("pause");
#endif
        }
    }

    PrintStr("\n[TIMEOUT] Elapsed. Resuming shell processor with exit code 0.\n");
    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
