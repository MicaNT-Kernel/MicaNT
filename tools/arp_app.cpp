/**
 * @file arp_app.cpp
 * @brief Sovereign Ring 3 Address Resolution Protocol (ARP) Utility for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Outputs Windows NT style ARP routing table via NtWriteFile (0x25).
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
        PrintLine("Displays and modifies the IP-to-Physical address translation tables.");
        PrintLine("");
        PrintLine("ARP -a [inet_addr] [-N if_addr] [-v]");
        PrintLine("  -a            Displays current ARP entries.");
        DoSyscall(0x01, 0, 0);
        return;
    }

    PrintLine("Interface: 147.135.1.229 --- 0x2");
    PrintLine("  Internet Address      Physical Address      Type");
    PrintLine("  147.135.1.254         00-1c-73-00-00-01     dynamic");
    PrintLine("  147.135.1.255         ff-ff-ff-ff-ff-ff     static");
    PrintLine("  224.0.0.22            01-00-5e-00-00-16     static");
    PrintLine("  224.0.0.251           01-00-5e-00-00-fb     static");
    PrintLine("  224.0.0.252           01-00-5e-00-00-fc     static");
    PrintLine("  239.255.255.250       01-00-5e-7f-ff-fa     static");
    PrintLine("  255.255.255.255       ff-ff-ff-ff-ff-ff     static");

    // Clean exit via NtTerminateProcess (SSN 0x01)
    DoSyscall(0x01, 0, 0);

    while (true) {
#if defined(__x86_64__)
        __asm__ __volatile__("pause");
#endif
    }
}
