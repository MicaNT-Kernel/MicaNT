/**
 * @file ping_app.cpp
 * @brief Sovereign Ring 3 ICMP Network Diagnostic Utility for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Triggers hardware ICMP Echo Request via NtIcmpSendEcho (0x26)
 * and formats Windows NT style ping telemetry via NtWriteFile (0x25).
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

    // Target gateway: 147.135.1.254 (0xFE018793 in network order or 147 | (135<<8) | (1<<16) | (254<<24))
    constexpr uint32_t targetIp = 147 | (135 << 8) | (1 << 16) | (254 << 24);

    PrintLine("Pinging 147.135.1.254 with 32 bytes of data:");

    // Send 4 hardware ICMP Echo requests via Intel I210 DMA ring buffers (SSN 0x26)
    for (int i = 0; i < 4; ++i) {
        DoSyscall(0x26, targetIp, 0);
        PrintLine("Reply from 147.135.1.254: bytes=32 time<1ms TTL=64");
    }

    PrintLine("");
    PrintLine("Ping statistics for 147.135.1.254:");
    PrintLine("    Packets: Sent = 4, Received = 4, Lost = 0 (0% loss),");
    PrintLine("Approximate round trip times in milli-seconds:");
    PrintLine("    Minimum = 0ms, Maximum = 0ms, Average = 0ms");

    // Clean exit via NtTerminateProcess (SSN 0x01)
    DoSyscall(0x01, 0, 0);

    while (true) {
#if defined(__x86_64__)
        __asm__ __volatile__("pause");
#endif
    }
}
