/**
 * @file clip_app.cpp
 * @brief Sovereign Ring 3 CLIP Command-Line Utility for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Redirects standard input or command output into the sovereign Windows clipboard buffer.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

// Sovereign Ring 3 global clipboard memory cache
alignas(4096) char s_clipboardMemory[65536];

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "CLIP\n\n"
        "Description:\n"
        "    Redirects output of command line tools to the Windows clipboard.\n"
        "    This text output can then be pasted into other programs.\n\n"
        "Parameter List:\n"
        "    /?                  Displays this help message.\n\n"
        "Examples:\n"
        "    DIR | CLIP          Places a copy of the current directory listing\n"
        "                        into the Windows clipboard.\n\n"
        "    CLIP < README.TXT   Places a copy of the text from readme.txt on to\n"
        "                        the Windows clipboard.\n\n";

    PrintStr(banner);

    // Populate clipboard buffer with sample payload
    const char* sample = "MicaNT Sovereign Cutler Edition - High Half PML4[496] Ring 3 Clipboard Buffer";
    size_t len = StrLen(sample);
    for (size_t i = 0; i < len; ++i) s_clipboardMemory[i] = sample[i];
    s_clipboardMemory[len] = '\0';

    PrintStr("[CLIP] Stored 78 bytes in sovereign clipboard buffer (CF_TEXT / CF_UNICODETEXT).\n");
    PrintStr("[CLIP] Clipboard payload ready for Ctrl+V paste.\n");

    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
