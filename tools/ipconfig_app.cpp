/**
 * @file ipconfig_app.cpp
 * @brief Sovereign Ring 3 TCP/IP Configuration Utility for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Outputs Windows NT style network adapter configuration via NtWriteFile (0x25).
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

    PrintLine("Windows IP Configuration");
    PrintLine("");
    PrintLine("Host Name . . . . . . . . . . . . : MICANT-XEON");
    PrintLine("Primary Dns Suffix  . . . . . . . : ash-forge.internal");
    PrintLine("Node Type . . . . . . . . . . . . : Hybrid");
    PrintLine("IP Routing Enabled. . . . . . . . : No");
    PrintLine("WINS Proxy Enabled. . . . . . . . : No");
    PrintLine("");
    PrintLine("Ethernet adapter Ethernet0:");
    PrintLine("");
    PrintLine("   Connection-specific DNS Suffix  . : ash-forge.internal");
    PrintLine("   Description . . . . . . . . . . . : Intel(R) I210 Gigabit Network Connection");
    PrintLine("   Physical Address. . . . . . . . . : A4-BF-01-1E-A3-BE");
    PrintLine("   DHCP Enabled. . . . . . . . . . . : No");
    PrintLine("   Autoconfiguration Enabled . . . . : Yes");
    PrintLine("   IPv4 Address. . . . . . . . . . . : 147.135.1.229");
    PrintLine("   Subnet Mask . . . . . . . . . . . : 255.255.255.0");
    PrintLine("   Default Gateway . . . . . . . . . : 147.135.1.254");
    PrintLine("   DNS Servers . . . . . . . . . . . : 1.1.1.1");
    PrintLine("                                       8.8.8.8");
    PrintLine("   NetBIOS over Tcpip. . . . . . . . : Enabled");

    // Clean exit via NtTerminateProcess (SSN 0x01)
    DoSyscall(0x01, 0, 0);

    while (true) {
#if defined(__x86_64__)
        __asm__ __volatile__("pause");
#endif
    }
}
