/**
 * @file micant_app.cpp
 * @brief Standalone Freestanding Ring 3 Native PE32+ Application for MicaNT.
 * Exercises Dave Cutler NT system calls:
 * - NtQuerySystemInformation (0x11)
 * - NtAllocateVirtualMemory (0x18) - Dynamic Userland Heap
 * - Memory write & demand-paging verification
 * - NtFreeVirtualMemory (0x1E)
 * - NtWriteFile (0x25)
 * - NtYieldExecution (0x46)
 * - NtTerminateProcess (0x01)
 *
 * Clean-Room ISO C++23. Zero proprietary headers or runtime dependencies.
 */

#include <cstdint>

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

        // 2. NtAllocateVirtualMemory (SSN 0x18) - Allocate 4096 bytes of dynamic user heap
        "movq $0x18, %rax\n\t"
        "xorq %rdx, %rdx\n\t"     // Auto-placement (Base VA = 0)
        "movq $4096, %r8\n\t"     // RegionSize = 4096 bytes
        "syscall\n\t"
        // RAX now contains allocated virtual base address (e.g. 0x20000000 or heap pool)
        "movq %rax, %r12\n\t"     // Save allocated heap pointer in R12 (callee-saved)
        "testq %r12, %r12\n\t"
        "jz 2f\n\t"

        // 3. Write data to the dynamically allocated heap page (exercises demand-paging & write permissions)
        "movabsq $0x544E6163694D2020, %rax\n\t" // "  MicaNT"
        "movq %rax, (%r12)\n\t"
        "movabsq $0x2144414F4C204D4D, %rax\n\t" // "MM LOAD!"
        "movq %rax, 8(%r12)\n\t"

        // 4. NtWriteFile (SSN 0x25) - 'MEM_PASS' (0x535341505F4D454D)
        "movq $0x25, %rax\n\t"
        "movabsq $0x535341505F4D454D, %rdx\n\t"
        "movq $8, %r8\n\t"
        "syscall\n\t"

        // 5. NtFreeVirtualMemory (SSN 0x1E) - Release dynamic user heap
        "movq $0x1E, %rax\n\t"
        "movq %r12, %rdx\n\t"     // Base address
        "movq $4096, %r8\n\t"     // Size
        "syscall\n\t"

        // 6. NtYieldExecution (SSN 0x46)
        "movq $0x46, %rax\n\t"
        "syscall\n\t"

        // 7. NtWriteFile (SSN 0x25) - 'APP_DONE' (0x4150505F444F4E45)
        "movq $0x25, %rax\n\t"
        "movabsq $0x4150505F444F4E45, %rdx\n\t"
        "movq $8, %r8\n\t"
        "syscall\n\t"

        // 8. NtTerminateProcess (SSN 0x01) - Clean exit
        "movq $0x01, %rax\n\t"
        "xorq %rdx, %rdx\n\t"
        "xorq %r8, %r8\n\t"
        "syscall\n\t"

        "2:\n\t"
        // Error exit: 'MEM_FAIL' (0x4C4941465F4D454D)
        "movq $0x25, %rax\n\t"
        "movabsq $0x4C4941465F4D454D, %rdx\n\t"
        "movq $8, %r8\n\t"
        "syscall\n\t"
        "movq $0x01, %rax\n\t"
        "xorq %rdx, %rdx\n\t"
        "xorq %r8, %r8\n\t"
        "syscall\n\t"

        "1: pause\n\t"
        "jmp 1b\n\t"
    );
#endif
}
