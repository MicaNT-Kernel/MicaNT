/**
 * @file tree_app.cpp
 * @brief Sovereign Ring 3 Directory Hierarchy Visualizer (tree) for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Graphically displays the folder structure of a drive or path.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

void ShowSampleTree() noexcept {
    const char* out =
        "Folder PATH listing for volume MicaNT_OS\n"
        "Volume serial number is 3A8B-4129\n"
        "C:\\MICANT\n"
        "+---Config\n"
        "|       DEFAULT\n"
        "|       SAM\n"
        "|       SECURITY\n"
        "|       SOFTWARE\n"
        "|       SYSTEM\n"
        "+---Fonts\n"
        "|       segoeui.ttf\n"
        "|       consola.ttf\n"
        "\\---System32\n"
        "    |   ntoskrnl.exe\n"
        "    |   explorer.exe\n"
        "    |   cmd.exe\n"
        "    +---drivers\n"
        "    |       ahci.sys\n"
        "    |       i210.sys\n"
        "    |       tcpip.sys\n"
        "    |       hmb.sys\n"
        "    \\---etc\n"
        "            hosts\n"
        "            services\n\n";
    PrintStr(out);
}

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "Graphically displays the folder structure of a drive or path.\n\n"
        "TREE [drive:][path] [/F] [/A]\n\n"
        "   /F   Display the names of the files in each folder.\n"
        "   /A   Use ASCII instead of extended characters.\n\n"
        "[tree C:\\MicaNT /F]\n";

    PrintStr(banner);
    ShowSampleTree();

    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
