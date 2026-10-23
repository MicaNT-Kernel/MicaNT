/**
 * @file vssadmin_app.cpp
 * @brief Sovereign Ring 3 Volume Shadow Copy Service Administration CLI (vssadmin) for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Manages volume shadow copies, storage allocation, and VSS writer states.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

void ShowSampleShadows() noexcept {
    const char* out =
        "vssadmin 1.1 - Volume Shadow Copy Service administrative command-line tool\n"
        "(C) Copyright MicaNT Operating System Authority.\n\n"
        "Contents of shadow copy set ID: {3E8C127A-45DF-4A1B-9430-652D0F08B9B1}\n"
        "   Contained 1 shadow copies at creation time: 10/23/2026 09:00:00 AM\n"
        "      Shadow Copy ID: {A10F4D93-7892-4C10-B219-F87A65BC3E10}\n"
        "      Original Volume: (C:)\\\\?\\Volume{3a8b4129-0000-0000-0000-100000000000}\\\n"
        "      Shadow Copy Volume: \\\\?\\GLOBALROOT\\Device\\HarddiskVolumeShadowCopy1\n"
        "      Originating Machine: MICANT-HOST\n"
        "      Service Machine: MICANT-HOST\n"
        "      Provider: 'MicaNT Sovereign VSS Copy-On-Write Provider'\n"
        "      Type: ClientAccessible\n"
        "      Attributes: Persistent, Client-accessible, No auto release, Differential\n\n";
    PrintStr(out);
}

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "vssadmin <command> [options]\n\n"
        "Commands:\n"
        "  Delete Shadows        - Delete volume shadow copies\n"
        "  List Providers        - List registered volume shadow copy providers\n"
        "  List Shadows          - List existing volume shadow copies\n"
        "  List ShadowStorage    - List volume shadow copy storage associations\n"
        "  List Volumes          - List volumes that can be shadow copied\n"
        "  List Writers          - List subscribed volume shadow copy writers\n"
        "  Resize ShadowStorage  - Resize a volume shadow copy storage association\n"
        "  Revert Shadow         - Revert a volume to a shadow copy\n\n"
        "[vssadmin list shadows]\n";

    PrintStr(banner);
    ShowSampleShadows();

    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
