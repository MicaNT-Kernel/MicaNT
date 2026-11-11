#if defined(MICANT_FREESTANDING)
#include "micant/km/core/ke.hpp"

static_assert(offsetof(micant::ke::KThread, tid) == 0);

extern "C" bool MicantContextSwitchForCanary(
    micant::ke::KThread* outgoing, micant::ke::KThread* incoming) noexcept {
    return micant::ke::KiSwapContext(outgoing, incoming);
}

// Keep the caller's full nonvolatile ABI intact while replacing it with
// canaries across a genuine yield/resume, including both FP control words.
extern "C" __attribute__((naked)) bool MicantVerifyContextSwitch(
    micant::ke::KThread*, micant::ke::KThread*) noexcept {
    __asm__ __volatile__(
        "pushq %rbx\npushq %rbp\npushq %rsi\npushq %rdi\n"
        "pushq %r12\npushq %r13\npushq %r14\npushq %r15\n"
        "subq $232, %rsp\n"
        "movdqu %xmm6, 32(%rsp)\nmovdqu %xmm7, 48(%rsp)\n"
        "movdqu %xmm8, 64(%rsp)\nmovdqu %xmm9, 80(%rsp)\n"
        "movdqu %xmm10, 96(%rsp)\nmovdqu %xmm11, 112(%rsp)\n"
        "movdqu %xmm12, 128(%rsp)\nmovdqu %xmm13, 144(%rsp)\n"
        "movdqu %xmm14, 160(%rsp)\nmovdqu %xmm15, 176(%rsp)\n"
        "stmxcsr 192(%rsp)\nfnstcw 196(%rsp)\n"
        "movq (%rcx), %rax\nandl $3, %eax\nshll $13, %eax\n"
        "orl $0x1f80, %eax\nmovl %eax, 204(%rsp)\nldmxcsr 204(%rsp)\n"
        "movq (%rcx), %rax\nandl $3, %eax\nshll $10, %eax\n"
        "orl $0x037f, %eax\nmovw %ax, 208(%rsp)\nfldcw 208(%rsp)\n"
        "movq (%rcx), %rax\nshlq $8, %rax\nmovq %rax, 216(%rsp)\n"
        "addq $11, %rax\nmovq %rax, %rbx\nincq %rax\nmovq %rax, %rbp\n"
        "incq %rax\nmovq %rax, %rsi\nincq %rax\nmovq %rax, %rdi\n"
        "incq %rax\nmovq %rax, %r12\nincq %rax\nmovq %rax, %r13\n"
        "incq %rax\nmovq %rax, %r14\nincq %rax\nmovq %rax, %r15\n"
        "movq 216(%rsp), %xmm1\npunpcklqdq %xmm1, %xmm1\n"
        "movdqa %xmm1, %xmm6\nmovdqa %xmm1, %xmm7\n"
        "movdqa %xmm1, %xmm8\nmovdqa %xmm1, %xmm9\n"
        "movdqa %xmm1, %xmm10\nmovdqa %xmm1, %xmm11\n"
        "movdqa %xmm1, %xmm12\nmovdqa %xmm1, %xmm13\n"
        "movdqa %xmm1, %xmm14\nmovdqa %xmm1, %xmm15\n"
        "callq MicantContextSwitchForCanary\n"
        "testb %al, %al\njz 9f\n"
        "movq 216(%rsp), %rax\naddq $11, %rax\n"
        "cmpq %rax, %rbx\njne 9f\nincq %rax\ncmpq %rax, %rbp\njne 9f\n"
        "incq %rax\ncmpq %rax, %rsi\njne 9f\nincq %rax\ncmpq %rax, %rdi\njne 9f\n"
        "incq %rax\ncmpq %rax, %r12\njne 9f\nincq %rax\ncmpq %rax, %r13\njne 9f\n"
        "incq %rax\ncmpq %rax, %r14\njne 9f\nincq %rax\ncmpq %rax, %r15\njne 9f\n"
        "movq 216(%rsp), %xmm1\npunpcklqdq %xmm1, %xmm1\n"
        "movdqa %xmm6, %xmm0\npcmpeqb %xmm1, %xmm0\n"
        "pmovmskb %xmm0, %eax\ncmpl $65535, %eax\njne 9f\n"
        "movdqa %xmm7, %xmm0\npcmpeqb %xmm1, %xmm0\n"
        "pmovmskb %xmm0, %eax\ncmpl $65535, %eax\njne 9f\n"
        "movdqa %xmm8, %xmm0\npcmpeqb %xmm1, %xmm0\n"
        "pmovmskb %xmm0, %eax\ncmpl $65535, %eax\njne 9f\n"
        "movdqa %xmm9, %xmm0\npcmpeqb %xmm1, %xmm0\n"
        "pmovmskb %xmm0, %eax\ncmpl $65535, %eax\njne 9f\n"
        "movdqa %xmm10, %xmm0\npcmpeqb %xmm1, %xmm0\n"
        "pmovmskb %xmm0, %eax\ncmpl $65535, %eax\njne 9f\n"
        "movdqa %xmm11, %xmm0\npcmpeqb %xmm1, %xmm0\n"
        "pmovmskb %xmm0, %eax\ncmpl $65535, %eax\njne 9f\n"
        "movdqa %xmm12, %xmm0\npcmpeqb %xmm1, %xmm0\n"
        "pmovmskb %xmm0, %eax\ncmpl $65535, %eax\njne 9f\n"
        "movdqa %xmm13, %xmm0\npcmpeqb %xmm1, %xmm0\n"
        "pmovmskb %xmm0, %eax\ncmpl $65535, %eax\njne 9f\n"
        "movdqa %xmm14, %xmm0\npcmpeqb %xmm1, %xmm0\n"
        "pmovmskb %xmm0, %eax\ncmpl $65535, %eax\njne 9f\n"
        "movdqa %xmm15, %xmm0\npcmpeqb %xmm1, %xmm0\n"
        "pmovmskb %xmm0, %eax\ncmpl $65535, %eax\njne 9f\n"
        "movq 216(%rsp), %rcx\nshrq $8, %rcx\nandl $3, %ecx\n"
        "movl %ecx, %edx\nshll $13, %ecx\norl $0x1f80, %ecx\n"
        "stmxcsr 204(%rsp)\ncmpl %ecx, 204(%rsp)\njne 9f\n"
        "shll $10, %edx\norl $0x037f, %edx\n"
        "fnstcw 208(%rsp)\ncmpw %dx, 208(%rsp)\njne 9f\n"
        "movl $1, %eax\njmp 8f\n9:\nxorl %eax, %eax\n8:\n"
        "ldmxcsr 192(%rsp)\nfldcw 196(%rsp)\n"
        "movdqu 32(%rsp), %xmm6\nmovdqu 48(%rsp), %xmm7\n"
        "movdqu 64(%rsp), %xmm8\nmovdqu 80(%rsp), %xmm9\n"
        "movdqu 96(%rsp), %xmm10\nmovdqu 112(%rsp), %xmm11\n"
        "movdqu 128(%rsp), %xmm12\nmovdqu 144(%rsp), %xmm13\n"
        "movdqu 160(%rsp), %xmm14\nmovdqu 176(%rsp), %xmm15\n"
        "addq $232, %rsp\n"
        "popq %r15\npopq %r14\npopq %r13\npopq %r12\n"
        "popq %rdi\npopq %rsi\npopq %rbp\npopq %rbx\nretq\n"
    );
}
#endif
