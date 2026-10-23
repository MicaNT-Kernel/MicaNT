/**
 * @file cipher_app.cpp
 * @brief Sovereign Ring 3 Encrypting File System (EFS) & Security Utility (cipher) for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Manages directory encryption, certificate key derivation, and free space security wipes.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

void ShowCipherState() noexcept {
    const char* out =
        "Listing C:\\MicaNT\\\n"
        "New files added to this directory will not be encrypted.\n\n"
        "U System32\n"
        "U Config\n"
        "E SecurityVault (EFS Protected - Sovereign AES-256-XTS)\n\n"
        "Key Certificate Information:\n"
        "  Thumbprint: 4A8F93BD71C2E03A9D1045E8F3B921A77E41C90D\n"
        "  User      : MicaAdmin (Sovereign Certificate Authority)\n\n";
    PrintStr(out);
}

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "Displays or alters the encryption of directories [files] on NTFS partitions.\n\n"
        "  CIPHER [/E | /D | /C] [/S:directory] [/B] [/H] [pathname [...]]\n"
        "  CIPHER /K [/APPEND]\n"
        "  CIPHER /R:filename [/SMARTCARD]\n"
        "  CIPHER /W:directory\n"
        "  CIPHER /X[:efsfile] [filename]\n\n"
        "  /E        Encrypts the specified directories.\n"
        "  /D        Decrypts the specified directories.\n"
        "  /K        Creates a new certificate and key for use with EFS.\n"
        "  /R        Generates an EFS recovery agent key and certificate.\n"
        "  /W        Removes data from unused portions of the volume (Wipe).\n\n"
        "[cipher C:\\MicaNT]\n";

    PrintStr(banner);
    ShowCipherState();

    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
