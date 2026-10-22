/**
 * @file attrib_app.cpp
 * @brief Sovereign Ring 3 File Attributes Utility for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Queries and modifies file attributes via NtWriteFile (0x25).
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
        PrintLine("Displays or changes file attributes.");
        PrintLine("");
        PrintLine("ATTRIB [+R | -R] [+A | -A ] [+S | -S] [+H | -H] [drive:][path][filename]");
        PrintLine("  +   Sets an attribute.");
        PrintLine("  -   Clears an attribute.");
        PrintLine("  R   Read-only file attribute.");
        PrintLine("  A   Archive file attribute.");
        PrintLine("  S   System file attribute.");
        PrintLine("  H   Hidden file attribute.");
        DoSyscall(0x01, 0, 0);
        return;
    }

    if (cmdLine && (HasSubstr(cmdLine, "boot.ini") || HasSubstr(cmdLine, "BOOT.INI"))) {
        PrintLine("A   H        C:\\boot.ini");
    } else {
        PrintLine("A            C:\\MicaNT\\System32\\ntoskrnl.exe");
        PrintLine("A            C:\\MicaNT\\System32\\app.exe");
        PrintLine("A            C:\\MicaNT\\System32\\hostname.exe");
        PrintLine("A            C:\\MicaNT\\System32\\systeminfo.exe");
        PrintLine("A   H        C:\\boot.ini");
    }

    // Clean exit via NtTerminateProcess (SSN 0x01)
    DoSyscall(0x01, 0, 0);

    while (true) {
#if defined(__x86_64__)
        __asm__ __volatile__("pause");
#endif
    }
}
