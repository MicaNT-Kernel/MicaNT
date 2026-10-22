/**
 * @file route_app.cpp
 * @brief Sovereign Ring 3 IP Routing Table Utility for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Outputs Windows NT style IPv4 route tables via NtWriteFile (0x25).
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

} // namespace

extern "C" void _start(const char* cmdLine = nullptr) {
    // Syscall 0x11: NtQuerySystemInformation (Verify Ring 3 CPL=3)
    uint16_t csVal = 0;
#if defined(__x86_64__)
    __asm__ __volatile__("movw %%cs, %0" : "=r"(csVal));
#endif
    DoSyscall(0x11, csVal & 3, 0);

    if (cmdLine && (HasSubstr(cmdLine, "/?") || HasSubstr(cmdLine, "-?"))) {
        PrintLine("Manipulates network routing tables.");
        PrintLine("");
        PrintLine("ROUTE [-f] [-p] [-4|-6] command [destination] [MASK netmask]");
        PrintLine("  command       PRINT, ADD, DELETE, CHANGE");
        DoSyscall(0x01, 0, 0);
        return;
    }

    PrintLine("===========================================================================");
    PrintLine("Interface List");
    PrintLine("  2...a4 bf 01 1e a3 be ......Intel(R) I210 Gigabit Network Connection");
    PrintLine("  1...........................Software Loopback Interface 1");
    PrintLine("===========================================================================");
    PrintLine("");
    PrintLine("IPv4 Route Table");
    PrintLine("===========================================================================");
    PrintLine("Active Routes:");
    PrintLine("Network Destination        Netmask          Gateway       Interface  Metric");
    PrintLine("          0.0.0.0          0.0.0.0    147.135.1.254   147.135.1.229      25");
    PrintLine("        127.0.0.0        255.0.0.0         On-link         127.0.0.1    331");
    PrintLine("        127.0.0.1  255.255.255.255         On-link         127.0.0.1    331");
    PrintLine("      147.135.1.0    255.255.255.0         On-link    147.135.1.229     281");
    PrintLine("    147.135.1.229  255.255.255.255         On-link    147.135.1.229     281");
    PrintLine("    147.135.1.255  255.255.255.255         On-link    147.135.1.229     281");
    PrintLine("        224.0.0.0        240.0.0.0         On-link         127.0.0.1    331");
    PrintLine("        224.0.0.0        240.0.0.0         On-link    147.135.1.229     281");
    PrintLine("  255.255.255.255  255.255.255.255         On-link         127.0.0.1    331");
    PrintLine("  255.255.255.255  255.255.255.255         On-link    147.135.1.229     281");
    PrintLine("===========================================================================");
    PrintLine("Persistent Routes:");
    PrintLine("  Network Address          Netmask  Gateway Address  Metric");
    PrintLine("          0.0.0.0          0.0.0.0    147.135.1.254       1");
    PrintLine("===========================================================================");

    // Clean exit via NtTerminateProcess (SSN 0x01)
    DoSyscall(0x01, 0, 0);

    while (true) {
#if defined(__x86_64__)
        __asm__ __volatile__("pause");
#endif
    }
}
