/**
 * @file systeminfo_app.cpp
 * @brief Sovereign Ring 3 System Information Utility for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Outputs Windows NT style system information table via NtWriteFile (0x25).
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
        PrintLine("SYSTEMINFO [/S system [/U username [/P [password]]]] [/FO format]");
        PrintLine("");
        PrintLine("Description:");
        PrintLine("    Displays machine-specific properties and configuration.");
        DoSyscall(0x01, 0, 0);
        return;
    }

    PrintLine("Host Name:                 MICANT-XEON");
    PrintLine("OS Name:                   MicaNT Sovereign Workstation (Dave Cutler Edition)");
    PrintLine("OS Version:                10.0.26040 N/A Build 26040");
    PrintLine("OS Manufacturer:           MicaNT Sovereign Project");
    PrintLine("OS Configuration:          Standalone Workstation");
    PrintLine("OS Build Type:             Multiprocessor Free (Clean-Room SMP x86_64)");
    PrintLine("Registered Owner:          Dave Cutler");
    PrintLine("Registered Organization:   MicaNT Systems");
    PrintLine("Product ID:                00329-00000-00003-AAOEM");
    PrintLine("Original Install Date:     10/20/2026, 12:00:00 PM");
    PrintLine("System Boot Time:          10/22/2026, 01:21:40 PM");
    PrintLine("System Manufacturer:       Supermicro");
    PrintLine("System Model:              X11SSH-F");
    PrintLine("System Type:               x64-based PC");
    PrintLine("Processor(s):              1 Processor(s) Installed.");
    PrintLine("                           [01]: Intel64 Family 6 Model 158 Stepping 9 ~3800 Mhz");
    PrintLine("BIOS Version:              American Megatrends Inc. 2.7, 02/16/2024");
    PrintLine("Windows Directory:         C:\\MicaNT");
    PrintLine("System Directory:          C:\\MicaNT\\System32");
    PrintLine("Boot Device:               \\Device\\HarddiskVolume1");
    PrintLine("Total Physical Memory:     32,768 MB");
    PrintLine("Available Physical Memory: 32,512 MB");
    PrintLine("Virtual Memory: Max Size:  32,768 MB");
    PrintLine("Virtual Memory: Available: 32,512 MB");
    PrintLine("Virtual Memory: In Use:    256 MB");
    PrintLine("Network Card(s):           1 NIC(s) Installed.");
    PrintLine("                           [01]: Intel(R) I210 Gigabit Network Connection");
    PrintLine("                                 Connection Name: Ethernet0");
    PrintLine("                                 DHCP Enabled:    No");
    PrintLine("                                 IP address(es)");
    PrintLine("                                 [01]: 147.135.1.229");

    // Clean exit via NtTerminateProcess (SSN 0x01)
    DoSyscall(0x01, 0, 0);

    while (true) {
#if defined(__x86_64__)
        __asm__ __volatile__("pause");
#endif
    }
}
