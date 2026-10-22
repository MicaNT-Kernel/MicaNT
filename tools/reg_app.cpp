/**
 * @file reg_app.cpp
 * @brief Sovereign Ring 3 Registry Console Utility for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Outputs Windows NT style registry keys and values via NtWriteFile (0x25).
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

void PrintControlKey() noexcept {
    PrintLine("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control");
    PrintLine("    SystemBootDevice    REG_SZ    \\Device\\HarddiskVolume1");
    PrintLine("    SystemRoot          REG_SZ    C:\\MicaNT");
    PrintLine("    CurrentVersion      REG_SZ    10.0.26040.1000");
    PrintLine("    ComputerName        REG_SZ    MICANT-XEON");
    PrintLine("    HardwareModel       REG_SZ    Supermicro X11SSH-F");
}

void PrintSoftwareKey() noexcept {
    PrintLine("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion");
    PrintLine("    ProductName         REG_SZ    MicaNT Sovereign Workstation");
    PrintLine("    EditionID           REG_SZ    CutlerEdition");
    PrintLine("    CurrentBuild        REG_SZ    26040");
    PrintLine("    ReleaseId           REG_SZ    26H2");
    PrintLine("    BuildLabEx          REG_SZ    26040.1000.amd64fre.micant_flt.261022-1200");
}

void PrintTcpipKey() noexcept {
    PrintLine("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters");
    PrintLine("    Hostname            REG_SZ    MICANT-XEON");
    PrintLine("    Domain              REG_SZ    ash-forge.internal");
    PrintLine("    IPAddress           REG_SZ    147.135.1.229");
    PrintLine("    DefaultGateway      REG_SZ    147.135.1.254");
    PrintLine("    NameServer          REG_SZ    1.1.1.1,8.8.8.8");
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
        PrintLine("REG Operation [Parameter List]");
        PrintLine("  Operation  [ QUERY | ADD | DELETE | COPY | SAVE | LOAD ]");
        PrintLine("  For help on a specific operation: REG Operation /?");
        DoSyscall(0x01, 0, 0);
        return;
    }

    if (cmdLine && HasSubstr(cmdLine, "SOFTWARE")) {
        PrintSoftwareKey();
    } else if (cmdLine && HasSubstr(cmdLine, "Tcpip")) {
        PrintTcpipKey();
    } else if (cmdLine && HasSubstr(cmdLine, "Control")) {
        PrintControlKey();
    } else {
        PrintControlKey();
        PrintLine("");
        PrintSoftwareKey();
        PrintLine("");
        PrintTcpipKey();
    }

    // Clean exit via NtTerminateProcess (SSN 0x01)
    DoSyscall(0x01, 0, 0);

    while (true) {
#if defined(__x86_64__)
        __asm__ __volatile__("pause");
#endif
    }
}
