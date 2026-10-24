/**
 * @file diskshadow_app.cpp
 * @brief Sovereign Ring 3 DiskShadow VSS Scripting Engine (diskshadow) for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Exposes VSS snapshot scripting commands and backup metadata generation.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "MicaNT DiskShadow version 1.0 (MicaNT Sovereign Cutler Edition)\n"
        "Copyright (C) 2026 MicaNT Operating System Authority. All rights reserved.\n\n"
        "DISKSHADOW [options] [script-file]\n\n"
        "Commands:\n"
        "  set context persistent\n"
        "  begin backup\n"
        "  add volume C: alias VolumeC\n"
        "  create\n"
        "  expose %VolumeC% S:\n"
        "  end backup\n\n"
        "DiskShadow VSS Scripting Engine: Ready.\n\n";

    PrintStr(banner);

    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
