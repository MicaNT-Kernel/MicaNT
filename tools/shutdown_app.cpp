/**
 * @file shutdown_app.cpp
 * @brief Sovereign Ring 3 Shutdown & System Reset Utility for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Initiates system reboot or poweroff via NtShutdownSystem (0x08)
 * or exits cleanly via NtTerminateProcess (0x01).
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

void PrintUsage() noexcept {
    PrintLine("Usage: shutdown [/i | /l | /s | /sg | /r | /g | /a | /p | /h | /e | /o]");
    PrintLine("                [/hybrid] [/soft] [/fw] [/f] [/m \\\\computer] [/t xxx]");
    PrintLine("                [/d [p|u:]xx:yy] [/c \"comment\"]");
    PrintLine("");
    PrintLine("    No args    Display help. This is the same as typing /?.");
    PrintLine("    /?         Display help. This is the same as not typing any options.");
    PrintLine("    /i         Display the graphical user interface (GUI).");
    PrintLine("    /l         Log off. This cannot be used with /m or /d options.");
    PrintLine("    /s         Shutdown the computer.");
    PrintLine("    /r         Full shutdown and restart the computer.");
    PrintLine("    /g         Full shutdown and restart the computer and restart apps.");
    PrintLine("    /a         Abort a system shutdown.");
    PrintLine("    /p         Turn off the local computer with no time-out or warning.");
    PrintLine("    /h         Hibernate the local computer.");
    PrintLine("    /t xxx     Set the time-out period before shutdown to xxx seconds.");
    PrintLine("    /f         Force running applications to close without warning users.");
}

} // namespace

extern "C" void _start(const char* cmdLine = nullptr) {
    // Syscall 0x11: NtQuerySystemInformation (Verify Ring 3 CPL=3)
    uint16_t csVal = 0;
#if defined(__x86_64__)
    __asm__ __volatile__("movw %%cs, %0" : "=r"(csVal));
#endif
    DoSyscall(0x11, csVal & 3, 0);

    if (!cmdLine || *cmdLine == '\0' || HasSubstr(cmdLine, "/?") || HasSubstr(cmdLine, "-?")) {
        PrintUsage();
        DoSyscall(0x01, 0, 0);
        return;
    }

    if (HasSubstr(cmdLine, "/a") || HasSubstr(cmdLine, "/A") || HasSubstr(cmdLine, "-a") || HasSubstr(cmdLine, "-A")) {
        PrintLine("Unable to abort the system shutdown because no shutdown was in progress.(1116)");
        DoSyscall(0x01, 1, 0);
        return;
    }

    if (HasSubstr(cmdLine, "/r") || HasSubstr(cmdLine, "/R") || HasSubstr(cmdLine, "-r") || HasSubstr(cmdLine, "-R")) {
        PrintLine("MicaNT Cutler Edition is shutting down for restart.");
        PrintLine("Hardware CPU reboot initiated via Intel PCH Port 0xCF9...");
        // SSN 0x08: NtShutdownSystem (Action 1 = Reboot)
        DoSyscall(0x08, 1, 0);
        DoSyscall(0x01, 0, 0);
        return;
    }

    if (HasSubstr(cmdLine, "/s") || HasSubstr(cmdLine, "/S") || HasSubstr(cmdLine, "-s") || HasSubstr(cmdLine, "-S") ||
        HasSubstr(cmdLine, "/p") || HasSubstr(cmdLine, "/P") || HasSubstr(cmdLine, "-p") || HasSubstr(cmdLine, "-P")) {
        PrintLine("MicaNT Cutler Edition is shutting down.");
        PrintLine("System halted safely. It is now safe to turn off your computer.");
        DoSyscall(0x08, 0, 0);
        DoSyscall(0x01, 0, 0);
        return;
    }

    PrintUsage();
    DoSyscall(0x01, 1, 0);
}
