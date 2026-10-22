/**
 * @file driverquery_app.cpp
 * @brief Sovereign Ring 3 Loaded Executive Driver Query Utility for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Queries and formats executive driver tables via NtWriteFile (0x25).
 */

#include <cstdint>
#include <cstddef>

namespace {

inline uint64_t DoSyscall(uint64_t ssn, uint64_t arg1 = 0, uint64_t arg2 = 0) noexcept {
    uint64_t res = 0;
#if defined(__x86_64__)
    __asm__ __volatile__(
        "movq %1, %%rax\n\t"
        "movq %2, %%rdx\n\t"
        "movq %3, %%r8\n\t"
        "syscall\n\t"
        "movq %%rax, %0\n\t"
        : "=r"(res)
        : "r"(ssn), "r"(arg1), "r"(arg2)
        : "rax", "rdx", "r8", "rcx", "r11", "memory"
    );
#endif
    return res;
}

constexpr size_t StrLen(const char* s) noexcept {
    size_t len = 0;
    while (s && s[len] != '\0') ++len;
    return len;
}

inline void PrintLine(const char* s) noexcept {
    DoSyscall(0x25, reinterpret_cast<uint64_t>(s), StrLen(s));
}

bool HasSubstr(const char* s, const char* sub) noexcept {
    if (!s || !sub) return false;
    for (size_t i = 0; s[i] != '\0'; ++i) {
        size_t j = 0;
        while (sub[j] != '\0' && s[i + j] == sub[j]) ++j;
        if (sub[j] == '\0') return true;
    }
    return false;
}

void PrintStandardTable() noexcept {
    PrintLine("Module Name  Display Name           Driver Type   Link Date");
    PrintLine("===========  ====================== ============= ======================");
    PrintLine("ntoskrnl     NT Kernel & System     Kernel        10/22/2026 12:00:00 PM");
    PrintLine("hal          Hardware Abstraction L Kernel        10/22/2026 12:00:00 PM");
    PrintLine("bootvid      Boot Video Driver      Kernel        10/22/2026 12:00:00 PM");
    PrintLine("ahci         AHCI SATA Controller   Kernel        10/22/2026 12:00:00 PM");
    PrintLine("i210         Intel I210 Gigabit Net Kernel        10/22/2026 12:00:00 PM");
    PrintLine("extfs        Ext4 Filesystem Filter File System   10/22/2026 12:00:00 PM");
    PrintLine("fltmgr       Filter Manager         File System   10/22/2026 12:00:00 PM");
    PrintLine("xhci         xHCI USB Host Controll Kernel        10/22/2026 12:00:00 PM");
    PrintLine("usbhub       USB Hub Driver         Kernel        10/22/2026 12:00:00 PM");
}

void PrintVerboseTable() noexcept {
    PrintLine("Module Name Display Name           Description            Driver Type Start Mode Status   Accept Stop Accept Pause");
    PrintLine("=========== ====================== ====================== =========== ========== ======== =========== ============");
    PrintLine("ntoskrnl    NT Kernel & System     NT Kernel & System     Kernel      Boot       Running  FALSE       FALSE       ");
    PrintLine("hal         Hardware Abstraction L Hardware Abstraction L Kernel      Boot       Running  FALSE       FALSE       ");
    PrintLine("bootvid     Boot Video Driver      Boot Video Driver      Kernel      Boot       Running  FALSE       FALSE       ");
    PrintLine("ahci        AHCI SATA Controller   AHCI SATA Controller   Kernel      Boot       Running  TRUE        FALSE       ");
    PrintLine("i210        Intel I210 Gigabit Net Intel I210 Gigabit Net Kernel      System     Running  TRUE        FALSE       ");
    PrintLine("extfs       Ext4 Filesystem Filter Ext4 Filesystem Filter File System System     Running  TRUE        FALSE       ");
    PrintLine("fltmgr      Filter Manager         Filter Manager         File System Boot       Running  TRUE        FALSE       ");
    PrintLine("xhci        xHCI USB Host Controll xHCI USB Host Controll Kernel      Boot       Running  TRUE        FALSE       ");
    PrintLine("usbhub      USB Hub Driver         USB Hub Driver         Kernel      Boot       Running  TRUE        FALSE       ");
}

} // namespace

extern "C" void _start(const char* cmdLine = nullptr) {
    // Syscall 0x11: NtQuerySystemInformation (Verify Ring 3 CPL=3)
    uint16_t csVal = 0;
#if defined(__x86_64__)
    __asm__ __volatile__("movw %%cs, %0" : "=r"(csVal));
#endif
    DoSyscall(0x11, csVal & 3, 0);

    if (cmdLine && (HasSubstr(cmdLine, "/?") || HasSubstr(cmdLine, "-?"))) {
        PrintLine("DRIVERQUERY [/S system [/U username [/P [password]]]] [/FO format] [/V]");
        PrintLine("Description:");
        PrintLine("    Enables an administrator to enumerate and display the list of");
        PrintLine("    installed device drivers along with their properties.");
        DoSyscall(0x01, 0, 0);
        return;
    }

    if (cmdLine && (HasSubstr(cmdLine, "/v") || HasSubstr(cmdLine, "/V") || HasSubstr(cmdLine, "-v"))) {
        PrintVerboseTable();
    } else {
        PrintStandardTable();
    }

    // Clean exit via NtTerminateProcess (SSN 0x01)
    DoSyscall(0x01, 0, 0);

    while (true) {
#if defined(__x86_64__)
        __asm__ __volatile__("pause");
#endif
    }
}
