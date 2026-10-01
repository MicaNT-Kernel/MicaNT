#include <windows.h>

/**
 * @file unmodified_sample.cpp
 * @brief Standalone, unmodified 64-bit Windows console application.
 *
 * This file has ZERO MicaNT includes or dependencies.
 * It compiles strictly against standard Microsoft Windows SDK headers (<windows.h>)
 * to test clean-room PE loading, dynamic IAT binding, and execution on MicaNT.
 */

extern "C" void entry() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE && hOut != NULL) {
        const char msg[] = "MicaNT: Unmodified third-party Windows PE executing successfully!\n";
        DWORD written = 0;
        WriteFile(hOut, msg, (DWORD)(sizeof(msg) - 1), &written, NULL);
    }

    ULONGLONG ticks = GetTickCount64();
    ExitProcess((UINT)(ticks > 0 ? 0 : 1));
}
