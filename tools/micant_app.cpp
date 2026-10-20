/**
 * @file micant_app.cpp
 * @brief Standalone Freestanding Ring 3 Native PE32+ Application for MicaNT.
 */

extern "C" __attribute__((naked)) void _start() {
#if defined(__x86_64__)
    __asm__ __volatile__(
        // 1. NtQuerySystemInformation (SSN 0x11) - Verifies CPL == 3
        "movq $0x11, %rax\n\t"
        "movw %cs, %dx\n\t"
        "movzwq %dx, %rdx\n\t"
        "movq %rdx, %r8\n\t"
        "andq $3, %r8\n\t"
        "syscall\n\t"

        // 2. NtWriteFile (SSN 0x25) - 'MICAR3OK' (0x4D49434152334F4B)
        "movq $0x25, %rax\n\t"
        "movabsq $0x4D49434152334F4B, %rdx\n\t"
        "movq $8, %r8\n\t"
        "syscall\n\t"

        // 3. NtYieldExecution (SSN 0x46)
        "movq $0x46, %rax\n\t"
        "syscall\n\t"

        // 4. NtWriteFile (SSN 0x25) - 'APP_DONE' (0x4150505F444F4E45)
        "movq $0x25, %rax\n\t"
        "movabsq $0x4150505F444F4E45, %rdx\n\t"
        "movq $8, %r8\n\t"
        "syscall\n\t"

        // 5. NtTerminateProcess (SSN 0x01) - Clean exit
        "movq $0x01, %rax\n\t"
        "xorq %rdx, %rdx\n\t"
        "xorq %r8, %r8\n\t"
        "syscall\n\t"

        "1: pause\n\t"
        "jmp 1b\n\t"
    );
#endif
}
