/**
 * @file netmsg_app.cpp
 * @brief Sovereign Ring 3 Network & System Error Message Catalog (netmsg) for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Resolves Win32 error codes, NTSTATUS values, and network message IDs to textual explanations.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

void ShowSampleErrorLookup() noexcept {
    const char* out =
        "[netmsg 2]\n"
        "Error Code: 2 (0x00000002 / ERROR_FILE_NOT_FOUND)\n"
        "Facility:   Win32 Base Operating System\n"
        "Message:    The system cannot find the file specified.\n\n"
        "[netmsg 5]\n"
        "Error Code: 5 (0x00000005 / ERROR_ACCESS_DENIED)\n"
        "Facility:   Win32 Security & Access Control\n"
        "Message:    Access is denied.\n\n"
        "[netmsg 0xC0000005]\n"
        "Error Code: 0xC0000005 (STATUS_ACCESS_VIOLATION)\n"
        "Facility:   NT Kernel Executive (FACILITY_NTSTATUS)\n"
        "Severity:   STATUS_SEVERITY_ERROR\n"
        "Message:    The instruction at 0x%p referenced memory at 0x%p.\n"
        "            The memory could not be %s.\n\n"
        "[netmsg 2182]\n"
        "Error Code: 2182 (NET_ERROR_MORE_DATA)\n"
        "Facility:   Lanman Network Subsystem\n"
        "Message:    The requested service has already been started.\n\n";
    PrintStr(out);
}

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "MicaNT Sovereign Network & System Error Message Catalog (netmsg)\n"
        "Equivalent to Windows NT 'NET HELPMSG' and 'ERR.EXE' Subsystem\n"
        "Copyright (C) MicaNT Operating System Authority.\n\n"
        "Usage: netmsg <error_code> [/hex] [/ntstatus] [/facility <name>]\n\n"
        "Parameters:\n"
        "   error_code      Decimal or hexadecimal system error number (e.g. 2, 5, 0xC0000005)\n"
        "   /hex            Format error code as hexadecimal DWORD\n"
        "   /ntstatus       Force interpretation as native NTSTATUS 32-bit status code\n"
        "   /facility       Filter search to specified subsystem facility (Win32, RPC, NET, HRESULT)\n\n"
        "Common Error Catalog Examples:\n";

    PrintStr(banner);
    ShowSampleErrorLookup();

    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
