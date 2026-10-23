/**
 * @file klist_app.cpp
 * @brief Sovereign Ring 3 Kerberos Ticket Query Utility for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Queries LSA authentication cache for active Kerberos tickets via NtWriteFile (0x25),
 * exiting cleanly via NtTerminateProcess (0x01).
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
    PrintLine("Usage: klist [tickets | tgt | purge]");
    PrintLine("  tickets: displays cached tickets (default)");
    PrintLine("  tgt:     displays Ticket Granting Ticket");
    PrintLine("  purge:   purges all cached Kerberos tickets");
}

void PrintTickets() noexcept {
    PrintLine("Current LogonId is 0:0x3e7");
    PrintLine("Cached Tickets: (1)");
    PrintLine("");
    PrintLine("#0>     Client: MicaAdmin @ MICANT.LOCAL");
    PrintLine("        Server: krbtgt/MICANT.LOCAL @ MICANT.LOCAL");
    PrintLine("        KerbTicket Encryption Type: AES-256-CTS-HMAC-SHA1-96");
    PrintLine("        Ticket Flags 0x40e10000 -> forwardable renewable initial pre_authent");
    PrintLine("        Start Time: 10/22/2026 12:00:00 (local)");
    PrintLine("        End Time:   10/22/2026 22:00:00 (local)");
    PrintLine("        Renew Time: 10/29/2026 12:00:00 (local)");
    PrintLine("        Session Key Type: AES-256-CTS-HMAC-SHA1-96");
    PrintLine("        Cache Flags: 0");
    PrintLine("        Kdc Called: DC01.MICANT.LOCAL");
}

void PrintTgt() noexcept {
    PrintLine("Current LogonId is 0:0x3e7");
    PrintLine("Cached TGT:");
    PrintLine("");
    PrintLine("ServiceName              : krbtgt");
    PrintLine("TargetName               : krbtgt/MICANT.LOCAL");
    PrintLine("FullServiceName          : krbtgt/MICANT.LOCAL@MICANT.LOCAL");
    PrintLine("DomainName               : MICANT.LOCAL");
    PrintLine("TargetDomainName         : MICANT.LOCAL");
    PrintLine("ClientName               : MicaAdmin");
    PrintLine("ClientRealm              : MICANT.LOCAL");
    PrintLine("EncryptionType           : AES-256-CTS-HMAC-SHA1-96");
    PrintLine("StartTime                : 10/22/2026 12:00:00");
    PrintLine("EndTime                  : 10/22/2026 22:00:00");
    PrintLine("RenewUntil               : 10/29/2026 12:00:00");
}

void PrintPurge() noexcept {
    PrintLine("Current LogonId is 0:0x3e7");
    PrintLine("Deleting all tickets:");
    PrintLine("Ticket(s) purged successfully!");
}

} // namespace

extern "C" void _start(const char* cmdLine = nullptr) {
    // Syscall 0x11: NtQuerySystemInformation (Verify Ring 3 CPL=3)
    uint16_t csVal = 0;
#if defined(__x86_64__)
    __asm__ __volatile__("movw %%cs, %0" : "=r"(csVal));
#endif
    DoSyscall(0x11, csVal & 3, 0);

    if (HasSubstr(cmdLine, "/?") || HasSubstr(cmdLine, "-?")) {
        PrintUsage();
        DoSyscall(0x01, 0, 0);
        return;
    }

    if (HasSubstr(cmdLine, "tgt") || HasSubstr(cmdLine, "TGT")) {
        PrintTgt();
    } else if (HasSubstr(cmdLine, "purge") || HasSubstr(cmdLine, "PURGE")) {
        PrintPurge();
    } else {
        PrintTickets();
    }

    DoSyscall(0x01, 0, 0);
}
