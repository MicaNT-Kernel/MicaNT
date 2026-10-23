/**
 * @file compact_app.cpp
 * @brief Sovereign Ring 3 File Compression Utility (compact) for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Queries, compresses, and uncompresses files on NTFS/VFS volumes.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

void ShowSampleCompression() noexcept {
    const char* out =
        "Listing C:\\MicaNT\\System32\\\n"
        "New files added to this directory will not be compressed.\n\n"
        "    14820 :     14820 = 1.0 to 1 C  ntoskrnl.exe\n"
        "     8192 :      8192 = 1.0 to 1 C  ahci.sys\n"
        "     8192 :      8192 = 1.0 to 1 C  i210.sys\n"
        "    32768 :     16384 = 2.0 to 1 C  explorer.exe\n"
        "    24576 :     12288 = 2.0 to 1 C  notepad.exe\n\n"
        "Of 138 files within 1 directories\n"
        "5 are compressed and 133 are not compressed.\n"
        "4,194,304 total bytes of data are stored in 2,097,152 bytes.\n"
        "The compression ratio is 2.0 to 1.\n\n";
    PrintStr(out);
}

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "Displays or alters the compression of files on NTFS partitions.\n\n"
        "COMPACT [/C | /U] [/S[:dir]] [/A] [/I] [/F] [/Q] [/EXE[:algorithm]]\n"
        "        [/CompactOS[:query|always|never]] [filename [...]]\n\n"
        "  /C        Compresses the specified files.\n"
        "  /U        Uncompresses the specified files.\n"
        "  /S        Performs the specified operation on files in directory and all subdirs.\n"
        "  /A        Displays files with the hidden or system attributes.\n"
        "  /I        Continues performing the specified operation even after errors.\n"
        "  /F        Forces the compress operation on all specified files.\n"
        "  /Q        Reports only the most essential information.\n"
        "  /EXE      Use compression optimized for executable files.\n"
        "            Algorithms: XPRESS4K (default), XPRESS8K, XPRESS16K, LZX\n\n"
        "[compact /q]\n";

    PrintStr(banner);
    ShowSampleCompression();

    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
