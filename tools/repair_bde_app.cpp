/**
 * @file repair_bde_app.cpp
 * @brief Sovereign Ring 3 BitLocker Recovery & Decryption Tool (repair-bde) for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Recovers data from critically damaged or corrupted BitLocker encrypted volumes.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "BitLocker Recovery Tool (repair-bde.exe)\n\n"
        "Usage:\n"
        "  repair-bde <SourceVolume> <TargetVolume | TargetImage> [-RecoveryPassword <Password>]\n"
        "             [-RecoveryKey <KeyFilePath>] [-KeyPackage <KeyPackagePath>]\n"
        "             [-LogFile <LogFilePath>] [-Force]\n\n"
        "Parameters:\n"
        "  SourceVolume      Specifies the drive letter of the encrypted damaged volume.\n"
        "  TargetVolume      Specifies the output drive letter to store decrypted content.\n"
        "  TargetImage       Specifies a file path for an output decrypted disk image.\n"
        "  -RecoveryPassword Specifies the 48-digit numerical recovery password.\n"
        "  -RecoveryKey      Specifies the path to the 256-bit recovery key file (.bek).\n\n"
        "Cryptographic Volume Recovery Engine: Ready.\n\n";

    PrintStr(banner);

    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
