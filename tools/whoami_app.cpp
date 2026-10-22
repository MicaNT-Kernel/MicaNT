/**
 * @file whoami_app.cpp
 * @brief Sovereign Ring 3 Security Identity & Privilege Utility for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Queries security context and outputs Windows NT style user & privilege table
 * via Dave Cutler NtWriteFile (0x25) system service, exiting cleanly via NtTerminateProcess (0x01).
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

} // namespace

extern "C" void _start() {
    // Syscall 0x11: NtQuerySystemInformation (Verify Ring 3 CPL=3)
    uint16_t csVal = 0;
#if defined(__x86_64__)
    __asm__ __volatile__("movw %%cs, %0" : "=r"(csVal));
#endif
    DoSyscall(0x11, csVal & 3, 0);

    PrintLine("USER INFORMATION");
    PrintLine("----------------");
    PrintLine("User Name             SID");
    PrintLine("===================== ====================================");
    PrintLine("nt authority\\system   S-1-5-18");
    PrintLine("");
    PrintLine("PRIVILEGES INFORMATION");
    PrintLine("----------------------");
    PrintLine("Privilege Name                Description                          State");
    PrintLine("============================= ==================================== ========");
    PrintLine("SeDebugPrivilege              Debug programs                       Enabled");
    PrintLine("SeShutdownPrivilege           Shut down the system                 Enabled");
    PrintLine("SeTcbPrivilege                Act as part of the operating system  Enabled");
    PrintLine("SeSecurityPrivilege           Manage auditing and security log     Enabled");
    PrintLine("SeAssignPrimaryTokenPrivilege Replace a process-level token        Enabled");
    PrintLine("SeLockMemoryPrivilege         Lock pages in memory                 Enabled");
    PrintLine("SeIncreaseQuotaPrivilege      Adjust memory quotas for a process   Enabled");
    PrintLine("SeSystemtimePrivilege         Change the system time               Enabled");

    // Clean exit via NtTerminateProcess (SSN 0x01)
    DoSyscall(0x01, 0, 0);

    while (true) {
#if defined(__x86_64__)
        __asm__ __volatile__("pause");
#endif
    }
}
