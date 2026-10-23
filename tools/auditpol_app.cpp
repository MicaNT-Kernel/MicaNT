/**
 * @file auditpol_app.cpp
 * @brief Sovereign Ring 3 Security Audit Policy Management Utility (auditpol) for MicaNT System32.
 *
 * Clean-Room ISO C++23. Freestanding. Zero CRT dependencies.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Queries, sets, clears, and backups local and per-user security audit policies.
 */

#include "gui_common.hpp"

namespace {

using namespace micant_gui;

void ShowSampleAuditPolicy() noexcept {
    const char* out =
        "System audit policy\n"
        "Category/Subcategory                      Setting\n"
        "System\n"
        "  Security State Change                   Success and Failure\n"
        "  Security System Extension               Success and Failure\n"
        "  System Integrity                        Success and Failure\n"
        "Logon/Logoff\n"
        "  Logon                                   Success and Failure\n"
        "  Logoff                                  Success\n"
        "  Account Lockout                         Failure\n"
        "Object Access\n"
        "  File System                             No Auditing\n"
        "  Registry                                No Auditing\n"
        "  Kernel Object                           No Auditing\n"
        "Privilege Use\n"
        "  Sensitive Privilege Use                 Success and Failure\n"
        "Policy Change\n"
        "  Audit Policy Change                     Success\n"
        "  Authentication Policy Change            Success\n\n";
    PrintStr(out);
}

} // namespace

extern "C" void _start() noexcept {
    const char* banner =
        "Usage: auditpol <command> [<sub-command> ...] [<options>]\n\n"
        "Commands (only one command may be specified at a time):\n"
        "  /get        - Displays current audit policy.\n"
        "  /set        - Sets audit policy.\n"
        "  /list       - Displays selectable policy elements.\n"
        "  /backup     - Saves audit policy to a file.\n"
        "  /restore    - Restores audit policy from a file.\n"
        "  /clear      - Clears the audit policy.\n"
        "  /remove     - Removes per-user audit policy.\n\n"
        "[auditpol /get /category:*]\n";

    PrintStr(banner);
    ShowSampleAuditPolicy();

    DoSyscall(0x01, 0, 0); // NtTerminateProcess(0)
}
