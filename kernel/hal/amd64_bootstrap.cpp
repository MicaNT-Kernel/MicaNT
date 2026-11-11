#include "micant/km/hal/amd64_bootstrap.hpp"
#include "micant/km/hal/cpu.hpp"
#include "micant/shared/user_graphics.hpp"
#include "micant/shared/user_syscall.hpp"
#include "micant/km/executive/usermode.hpp"

namespace {

constexpr uint64_t kPresent = 1;
constexpr uint64_t kWritable = 2;
constexpr uint64_t kUser = 4;
constexpr uint64_t kNoExecute = uint64_t{1} << 63;
constexpr uint64_t kPhysicalMask = 0x000FFFFFFFFFF000ULL;
constexpr uint64_t kUserBase = uint64_t{128} << 39;
constexpr uint64_t kDeviceBase = uint64_t{129} << 39;
alignas(4096) uint64_t g_pml4[512]{};
alignas(4096) uint64_t g_tables[256][512]{};
size_t g_tableCount{};
uint64_t g_deviceNext = kDeviceBase;
bool g_addressSpaceReady{};
alignas(4096) uint8_t g_userCode[4096]{};
alignas(4096) uint8_t g_userStack[4096]{};
alignas(4096) uint8_t g_nativeProbeImage[8192]{};
uint8_t g_nativeProbeFile[512 + 4096]{};
micant::um::NativeUserImageSet64 g_nativeProbeModules{};
alignas(16) uint8_t g_kernelStack[32768]{};
volatile bool g_probeSucceeded{};
uint32_t g_probeMode{};
uint64_t g_probeReturnRip{};
micant::hal::amd64::UserServiceRoutine g_dispatch{};
alignas(16) uint8_t g_syscallStack[65536]{};
struct SyscallCpuState {
    uint64_t userRsp;
    uint64_t kernelRsp;
};
SyscallCpuState g_syscallCpu{};
struct SyscallFrame {
    uint64_t r15, r14, r13, r12, r10, r9, r8, rdx;
    uint64_t rsi, rdi, rbp, rbx, rax;
    uint64_t rip, cs, flags, rsp, ss;
};
static_assert(sizeof(SyscallFrame) == 144);
static_assert(offsetof(SyscallFrame, rax) == 96);
static_assert(offsetof(SyscallFrame, rip) == 104);
void (*g_log)(const char*) noexcept{};

#pragma pack(push, 1)
struct InterruptGate {
    uint16_t offsetLow{};
    uint16_t selector{};
    uint8_t ist{};
    uint8_t access{};
    uint16_t offsetMiddle{};
    uint32_t offsetHigh{};
    uint32_t reserved{};
};
struct Idtr {
    uint16_t limit;
    uint64_t base;
};
#pragma pack(pop)

alignas(16) InterruptGate g_idt[256]{};

uint64_t walk(uint64_t root, uint64_t address) noexcept {
    auto* table = reinterpret_cast<const uint64_t*>(root & kPhysicalMask);
    for (int shift = 39; shift >= 12; shift -= 9) {
        const uint64_t entry = table[(address >> shift) & 511];
        if ((entry & kPresent) == 0) {
            return 0;
        }
        if ((shift == 30 || shift == 21) && (entry & 128)) {
            const uint64_t offsetMask = (uint64_t{1} << shift) - 1;
            return (entry & kPhysicalMask & ~offsetMask) | (address & offsetMask);
        }
        if (shift == 12) {
            return (entry & kPhysicalMask) | (address & 4095);
        }
        table = reinterpret_cast<const uint64_t*>(entry & kPhysicalMask);
    }
    return 0;
}

bool mappedRange(uint64_t address, size_t size, bool user, bool writable,
                 bool executable) noexcept {
    if (!g_addressSpaceReady || !size || size - 1 > UINT64_MAX - address) {
        return false;
    }
    const uint64_t end = address + size - 1;
    const uint64_t region = address >> 47;
    if ((region != 0 && region != 0x1FFFF) || (end >> 47) != region) {
        return false;
    }
    for (uint64_t page = address & ~uint64_t{4095};; page += 4096) {
        const uint64_t* table = g_pml4;
        bool userAccessible = true;
        for (int shift = 39; shift >= 12; shift -= 9) {
            const uint64_t entry = table[(page >> shift) & 511];
            userAccessible = userAccessible && (entry & kUser);
            if (!(entry & kPresent) || (user && !userAccessible) ||
                (writable && !(entry & kWritable)) ||
                (executable && (entry & kNoExecute))) {
                return false;
            }
            if (shift > 12 && (entry & 128)) {
                if (user || (shift != 30 && shift != 21)) return false;
                break;
            }
            if (shift == 12) break;
            table = reinterpret_cast<const uint64_t*>(entry & kPhysicalMask);
        }
        if (userAccessible != user) return false;
        if (end - page < 4096) return true;
    }
}

bool mapPage(uint64_t address, uint64_t physical, uint64_t flags) noexcept {
    uint64_t* table = g_pml4;
    for (int shift = 39; shift > 12; shift -= 9) {
        uint64_t& entry = table[(address >> shift) & 511];
        if ((entry & kPresent) == 0) {
            if (g_tableCount == sizeof(g_tables) / sizeof(g_tables[0])) {
                return false;
            }
            entry = reinterpret_cast<uint64_t>(g_tables[g_tableCount++]) |
                    kPresent | kWritable | (flags & kUser);
        }
        if (entry & 128) {
            return false;
        }
        table = reinterpret_cast<uint64_t*>(entry & kPhysicalMask);
    }
    uint64_t& leaf = table[(address >> 12) & 511];
    if (leaf & kPresent) {
        return false;
    }
    leaf = (physical & kPhysicalMask) | flags | kPresent;
    return true;
}

[[noreturn]] void stopOnTrap() noexcept {
    if (g_log) {
        g_log("[MicaNT Hal] FATAL: unexpected bootstrap trap\r\n");
    }
    for (;;) {
        __asm__ __volatile__("cli; hlt");
    }
}

} // namespace

extern "C" {
uint64_t MicantProbeKernelRsp{};
uint64_t MicantProbeUserRip = kUserBase;

void MicantBootstrapDispatch(SyscallFrame* frame) noexcept {
    using micant::hal::amd64::bootstrapUserRange;
    if (!g_dispatch ||
        !bootstrapUserRange(frame->rip, 1, false, true) ||
        !bootstrapUserRange(frame->rsp, sizeof(uint64_t), true)) {
        stopOnTrap();
    }
    frame->rax = g_dispatch(frame->rax, frame->r10, frame->rdx);
    // Interrupts remain disabled until the bootstrap owns hardware IRQ routing.
    frame->flags = (frame->flags & 0xCD5) | 2;
}

__attribute__((naked)) void MicantBootstrapSyscallEntry() {
    __asm__ __volatile__(
        "swapgs\n"
        "movq %rsp, %gs:0\nmovq %gs:8, %rsp\n"
        "pushq $0x2b\npushq %gs:0\npushq %r11\npushq $0x33\npushq %rcx\n"
        "pushq %rax\npushq %rbx\npushq %rbp\npushq %rdi\npushq %rsi\n"
        "pushq %rdx\npushq %r8\npushq %r9\npushq %r10\n"
        "pushq %r12\npushq %r13\npushq %r14\npushq %r15\n"
        "movq %rsp, %rcx\nsubq $288, %rsp\n"
        "movdqu %xmm0, 32(%rsp)\nmovdqu %xmm1, 48(%rsp)\n"
        "movdqu %xmm2, 64(%rsp)\nmovdqu %xmm3, 80(%rsp)\n"
        "movdqu %xmm4, 96(%rsp)\nmovdqu %xmm5, 112(%rsp)\n"
        "movdqu %xmm6, 128(%rsp)\nmovdqu %xmm7, 144(%rsp)\n"
        "movdqu %xmm8, 160(%rsp)\nmovdqu %xmm9, 176(%rsp)\n"
        "movdqu %xmm10, 192(%rsp)\nmovdqu %xmm11, 208(%rsp)\n"
        "movdqu %xmm12, 224(%rsp)\nmovdqu %xmm13, 240(%rsp)\n"
        "movdqu %xmm14, 256(%rsp)\nmovdqu %xmm15, 272(%rsp)\n"
        "callq MicantBootstrapDispatch\n"
        "movdqu 32(%rsp), %xmm0\nmovdqu 48(%rsp), %xmm1\n"
        "movdqu 64(%rsp), %xmm2\nmovdqu 80(%rsp), %xmm3\n"
        "movdqu 96(%rsp), %xmm4\nmovdqu 112(%rsp), %xmm5\n"
        "movdqu 128(%rsp), %xmm6\nmovdqu 144(%rsp), %xmm7\n"
        "movdqu 160(%rsp), %xmm8\nmovdqu 176(%rsp), %xmm9\n"
        "movdqu 192(%rsp), %xmm10\nmovdqu 208(%rsp), %xmm11\n"
        "movdqu 224(%rsp), %xmm12\nmovdqu 240(%rsp), %xmm13\n"
        "movdqu 256(%rsp), %xmm14\nmovdqu 272(%rsp), %xmm15\n"
        "addq $288, %rsp\n"
        "popq %r15\npopq %r14\npopq %r13\npopq %r12\n"
        "popq %r10\npopq %r9\npopq %r8\npopq %rdx\n"
        "popq %rsi\npopq %rdi\npopq %rbp\npopq %rbx\npopq %rax\n"
        "swapgs\niretq\n"
    );
}

extern const uint8_t MicantSyscallProbeEnd[];
extern const uint8_t MicantSyscallClientEnd[];
extern const uint8_t MicantSyscallClientBreakpoint[];

__attribute__((noinline, section(".uclient$a")))
void MicantSyscallClientProgram() {
    auto* video = reinterpret_cast<micant::um::UserVideoInfo64*>(kUserBase + 4096);
    if (micant::sys::invokeUserService(0x80, reinterpret_cast<uint64_t>(video)) ||
        video->width != 1024 || video->height != 768 || video->physicalBase) {
        __builtin_trap();
    }
    auto* message = reinterpret_cast<char*>(kUserBase + 4096 + 64);
    message[0] = 'C'; message[1] = 'P'; message[2] = 'L';
    message[3] = '3'; message[4] = '\r'; message[5] = '\n';
    if (micant::sys::invokeUserService(0x25, reinterpret_cast<uint64_t>(message), 6)) {
        __builtin_trap();
    }
    __asm__ __volatile__(
        ".globl MicantSyscallClientBreakpoint\nMicantSyscallClientBreakpoint:\n"
        "int3\nud2\n"
    );
    __builtin_unreachable();
}

__asm__(".section .uclient$z,\"xr\"\n"
        ".globl MicantSyscallClientEnd\nMicantSyscallClientEnd:\n.text\n");

__attribute__((naked)) void MicantSyscallProbeProgram() {
    __asm__ __volatile__(
        "movabsq $0x400000001000, %r10\n"
        "movq $0x1234, %rdx\nmovq $0x5678, %r8\nmovq $0x9abc, %r9\n"
        "movq $0x1357, %rbx\nmovq $0x2468, %r12\n"
        "movq %rdx, %xmm0\nmovq %r8, %xmm6\nmovq %r9, %xmm15\n"
        "std\nmovl $0x80, %eax\nsyscall\ncld\n"
        "testq %rax, %rax\njnz 9f\n"
        "cmpl $1024, (%r10)\njne 9f\ncmpl $768, 4(%r10)\njne 9f\n"
        "cmpq $0, 24(%r10)\njne 9f\n" // No physical display pointer escapes.
        "cmpq $0x1234, %rdx\njne 9f\ncmpq $0x5678, %r8\njne 9f\n"
        "cmpq $0x9abc, %r9\njne 9f\ncmpq $0x1357, %rbx\njne 9f\n"
        "cmpq $0x2468, %r12\njne 9f\n"
        "movq %xmm0, %rax\ncmpq %rdx, %rax\njne 9f\n"
        "movq %xmm6, %rax\ncmpq %r8, %rax\njne 9f\n"
        "movq %xmm15, %rax\ncmpq %r9, %rax\njne 9f\n"
        "xorq %r10, %r10\nmovl $0x80, %eax\nsyscall\n"
        "cmpl $0xc0000005, %eax\njne 9f\n" // Null output.
        "movabsq $0x400000000000, %r10\nmovl $0x80, %eax\nsyscall\n"
        "cmpl $0xc0000005, %eax\njne 9f\n" // Read-only code.
        "movabsq $0x400000001ff0, %r10\nmovl $0x80, %eax\nsyscall\n"
        "cmpl $0xc0000005, %eax\njne 9f\n" // Crosses into an unmapped page.
        "movq $0x1000, %r10\nmovl $0x80, %eax\nsyscall\n"
        "cmpl $0xc0000005, %eax\njne 9f\n" // Supervisor address.
        "movq $-16, %r10\nmovl $0x80, %eax\nsyscall\n"
        "cmpl $0xc0000005, %eax\njne 9f\n" // Wraparound.
        "movabsq $0x400000001000, %r10\n"
        "movabsq $0x00000a0d334c5043, %rax\nmovq %rax, 64(%r10)\n"
        "addq $64, %r10\nmovq $6, %rdx\nmovl $0x25, %eax\nsyscall\n"
        "testq %rax, %rax\njnz 9f\ncmpq $6, %rdx\njne 9f\n"
        "movabsq $0x400000001000, %r10\n"
        "leaq 128(%r10), %rax\nmovq %rax, (%r10)\n"
        "movq $0, 8(%r10)\n"
        "movl $2, 16(%r10)\nmovl $2, 20(%r10)\n"
        "movl $2, 24(%r10)\nmovl $0, 28(%r10)\n"
        "movabsq $0xff112233ff112233, %rax\n"
        "movq %rax, 128(%r10)\nmovq %rax, 136(%r10)\n"
        "movl $0x81, %eax\nsyscall\ntestq %rax, %rax\njnz 9f\n"
        "movl $0, 16(%r10)\nmovl $0x81, %eax\nsyscall\n"
        "cmpl $0xc000000d, %eax\njne 9f\n" // Zero width.
        "movl $2, 16(%r10)\nmovq $0, (%r10)\n"
        "movl $0x81, %eax\nsyscall\ncmpl $0xc0000005, %eax\njne 9f\n"
        "movabsq $0x400000001ff8, %rax\nmovq %rax, (%r10)\n"
        "movl $0x81, %eax\nsyscall\ncmpl $0xc0000005, %eax\njne 9f\n"
        "movl $0x7fffffff, %eax\nsyscall\n"
        "cmpl $0xc00000bb, %eax\njne 9f\n"
        "int3\n9: ud2\n"
        ".globl MicantSyscallProbeEnd\nMicantSyscallProbeEnd:\n"
    );
}

[[noreturn]] __attribute__((naked)) void MicantProbeReturn() {
    __asm__ __volatile__(
        "movq MicantProbeKernelRsp(%rip), %rsp\n"
        "movdqu 0(%rsp), %xmm6\nmovdqu 16(%rsp), %xmm7\n"
        "movdqu 32(%rsp), %xmm8\nmovdqu 48(%rsp), %xmm9\n"
        "movdqu 64(%rsp), %xmm10\nmovdqu 80(%rsp), %xmm11\n"
        "movdqu 96(%rsp), %xmm12\nmovdqu 112(%rsp), %xmm13\n"
        "movdqu 128(%rsp), %xmm14\nmovdqu 144(%rsp), %xmm15\n"
        "addq $160, %rsp\n"
        "popq %r15\npopq %r14\npopq %r13\npopq %r12\n"
        "popq %rsi\npopq %rdi\npopq %rbp\npopq %rbx\n"
        "retq\n"
    );
}

void MicantProbeTrap(const uint64_t* frame) noexcept {
    // The trap frame is written by the CPU, not supplied by user memory.
    g_probeSucceeded = (g_probeMode == 0 || g_probeMode == 5 || g_probeMode == 6) &&
                       frame[0] == g_probeReturnRip &&
                       frame[1] == micant::cpu::SELECTOR_UCODE64 &&
                       frame[4] == micant::cpu::SELECTOR_UDATA64 &&
                       (g_probeMode == 6
                           ? micant::hal::amd64::bootstrapUserRange(frame[3], 8, true)
                           : frame[3] == kUserBase + 8192 - 16) &&
                       (frame[2] & 0x3000) == 0;
    if (!g_probeSucceeded) {
        stopOnTrap();
    }
    MicantProbeReturn();
}

void MicantProbePageFault(const uint64_t* frame) noexcept {
    uint64_t faultAddress{};
    __asm__ __volatile__("movq %%cr2, %0" : "=r"(faultAddress));
    const uint64_t expectedAddress = g_probeMode == 1
        ? reinterpret_cast<uint64_t>(g_pml4)
        : g_probeMode == 2 ? kUserBase
        : g_probeMode == 3 ? kUserBase + 4096 : kDeviceBase;
    const uint64_t expectedError = g_probeMode == 2 ? 7 : g_probeMode == 3 ? 21 : 5;
    const uint64_t expectedRip = g_probeMode == 3 ? expectedAddress : kUserBase + 10;
    g_probeSucceeded = g_probeMode >= 1 && g_probeMode <= 4 &&
                       faultAddress == expectedAddress &&
                       frame[0] == expectedError &&
                       frame[1] == expectedRip &&
                       frame[2] == micant::cpu::SELECTOR_UCODE64 &&
                       frame[5] == micant::cpu::SELECTOR_UDATA64 &&
                       frame[4] == kUserBase + 8192 - 16 &&
                       (frame[3] & 0x3000) == 0;
    if (!g_probeSucceeded) {
        stopOnTrap();
    }
    MicantProbeReturn();
}

__attribute__((naked)) void MicantProbeTrapEntry() {
    __asm__ __volatile__(
        "cld\nmovq %rsp, %rcx\nandq $-16, %rsp\nsubq $32, %rsp\ncallq MicantProbeTrap\n"
        "ud2\n"
    );
}

__attribute__((naked)) void MicantProbeFaultEntry() {
    __asm__ __volatile__(
        "cli\ncld\nandq $-16, %rsp\nsubq $32, %rsp\ncallq MicantProbeUnexpectedTrap\nud2\n"
    );
}

void MicantProbeUnexpectedTrap() noexcept {
    stopOnTrap();
}

__attribute__((naked)) void MicantProbePageFaultEntry() {
    __asm__ __volatile__(
        "cld\nmovq %rsp, %rcx\nandq $-16, %rsp\nsubq $32, %rsp\n"
        "callq MicantProbePageFault\nud2\n"
    );
}

__attribute__((naked)) void MicantProbeEnter() {
    __asm__ __volatile__(
        "pushq %rbx\npushq %rbp\npushq %rdi\npushq %rsi\n"
        "pushq %r12\npushq %r13\npushq %r14\npushq %r15\n"
        "subq $160, %rsp\n"
        "movdqu %xmm6, 0(%rsp)\nmovdqu %xmm7, 16(%rsp)\n"
        "movdqu %xmm8, 32(%rsp)\nmovdqu %xmm9, 48(%rsp)\n"
        "movdqu %xmm10, 64(%rsp)\nmovdqu %xmm11, 80(%rsp)\n"
        "movdqu %xmm12, 96(%rsp)\nmovdqu %xmm13, 112(%rsp)\n"
        "movdqu %xmm14, 128(%rsp)\nmovdqu %xmm15, 144(%rsp)\n"
        "movq %rsp, MicantProbeKernelRsp(%rip)\n"
        "movw $0x2b, %ax\nmovw %ax, %ds\nmovw %ax, %es\n"
        "pushq $0x2b\n"
        "movabsq $0x400000001ff0, %rax\npushq %rax\n"
        "pushq $2\npushq $0x33\n"
        "movq MicantProbeUserRip(%rip), %rax\npushq %rax\niretq\n"
    );
}
}

namespace micant::hal::amd64 {

bool initializeBootstrapAddressSpace(void (*log)(const char*) noexcept) noexcept {
    g_log = log;
    if (g_addressSpaceReady) {
        return true;
    }
    uint64_t cr3{};
    uint64_t cr4{};
    __asm__ __volatile__("movq %%cr3, %0; movq %%cr4, %1" : "=r"(cr3), "=r"(cr4));
    if (cr4 & (uint64_t{1} << 12)) {
        return false;
    }
    const auto* oldRoot = reinterpret_cast<const uint64_t*>(cr3 & kPhysicalMask);
    if (oldRoot[128] || oldRoot[129] ||
        walk(cr3, reinterpret_cast<uint64_t>(g_pml4)) !=
            reinterpret_cast<uint64_t>(g_pml4)) {
        return false;
    }
    for (size_t index = 0; index < 512; ++index) {
        g_pml4[index] = oldRoot[index] & ~kUser;
    }
    cpu::cpuWrmsr(cpu::MSR_EFER, cpu::cpuRdmsr(cpu::MSR_EFER) | cpu::EFER_NXE);
    // CR3 reload alone can retain firmware global translations.
    __asm__ __volatile__("movq %0, %%cr4" : : "r"(cr4 & ~uint64_t{128}) : "memory");
    __asm__ __volatile__("movq %0, %%cr3" : : "r"(g_pml4) : "memory");
    __asm__ __volatile__("movq %0, %%cr4" : : "r"(cr4) : "memory");
    g_addressSpaceReady = true;
    return true;
}

uint64_t translateBootstrapAddress(const void* address) noexcept {
    if (!g_addressSpaceReady) {
        return 0;
    }
    return walk(reinterpret_cast<uint64_t>(g_pml4),
                reinterpret_cast<uint64_t>(address));
}

bool bootstrapUserRange(uint64_t address, size_t size, bool writable,
                        bool executable) noexcept {
    if (!g_addressSpaceReady || size == 0 || address < kUserBase ||
        address >= kUserBase + (uint64_t{1} << 39) ||
        size > kUserBase + (uint64_t{1} << 39) - address) {
        return false;
    }
    return mappedRange(address, size, true, writable, executable);
}

bool bootstrapKernelRange(uint64_t address, size_t size, bool writable) noexcept {
    return mappedRange(address, size, false, writable, false);
}

bool mapBootstrapUserMemory(uint64_t address, const void* backing, size_t size,
                            bool writable, bool executable) noexcept {
    const uint64_t physical = reinterpret_cast<uint64_t>(backing);
    if (!g_addressSpaceReady || !size || (size & 4095) || (address & 4095) ||
        (physical & 4095) || !physical || address < kUserBase ||
        address >= kUserBase + (uint64_t{1} << 39) ||
        size > kUserBase + (uint64_t{1} << 39) - address ||
        physical > kPhysicalMask || size - 1 > kPhysicalMask + 4095 - physical ||
        (size / 4096 + 511) / 512 + 5 >
            sizeof(g_tables) / sizeof(g_tables[0]) - g_tableCount) {
        return false;
    }
    const uint64_t root = reinterpret_cast<uint64_t>(g_pml4);
    for (size_t offset = 0; offset < size; offset += 4096) {
        if (walk(root, address + offset) ||
            walk(root, physical + offset) != physical + offset) {
            return false;
        }
    }
    const uint64_t flags = kUser | (writable ? kWritable : 0) |
                           (executable ? 0 : kNoExecute);
    for (size_t offset = 0; offset < size; offset += 4096) {
        if (!mapPage(address + offset, physical + offset, flags)) return false;
    }
    return true;
}

bool initializeBootstrapSyscalls(UserServiceRoutine dispatch) noexcept {
    if (!g_addressSpaceReady || !g_probeSucceeded || !dispatch || g_dispatch) {
        return false;
    }
    g_syscallCpu.kernelRsp = reinterpret_cast<uint64_t>(
        g_syscallStack + sizeof(g_syscallStack));
    g_dispatch = dispatch;
    cpu::cpuWrmsr(cpu::MSR_GS_BASE, 0);
    cpu::cpuWrmsr(cpu::MSR_KERNEL_GS_BASE, reinterpret_cast<uint64_t>(&g_syscallCpu));
    cpu::cpuWrmsr(cpu::MSR_STAR, uint64_t{cpu::KGDT64_R3_CMCODE} << 48 |
                                   uint64_t{cpu::SELECTOR_KCODE64} << 32);
    cpu::cpuWrmsr(cpu::MSR_LSTAR, reinterpret_cast<uint64_t>(MicantBootstrapSyscallEntry));
    cpu::cpuWrmsr(cpu::MSR_SFMASK, cpu::SFMASK_DEFAULT | (uint64_t{1} << 18));
    cpu::cpuWrmsr(cpu::MSR_EFER, cpu::cpuRdmsr(cpu::MSR_EFER) | cpu::EFER_SCE);
    return true;
}

bool verifyBootstrapSyscalls() noexcept {
    if (!g_dispatch ||
        !bootstrapKernelRange(reinterpret_cast<uint64_t>(g_nativeProbeImage),
                              sizeof(g_nativeProbeImage), true) ||
        bootstrapKernelRange(kUserBase, 1, false) ||
        bootstrapKernelRange(kUserBase + 4096, 1, true) ||
        bootstrapKernelRange(0x0000800000000000ULL, 1, false) ||
        bootstrapKernelRange(UINT64_MAX - 8, 16, false) ||
        bootstrapKernelRange(reinterpret_cast<uint64_t>(g_nativeProbeImage), 0, true)) {
        return false;
    }
    const auto* begin = reinterpret_cast<const uint8_t*>(MicantSyscallProbeProgram);
    const size_t size = reinterpret_cast<uint64_t>(MicantSyscallProbeEnd) -
                        reinterpret_cast<uint64_t>(begin);
    if (size > sizeof(g_userCode) || size < 3) {
        return false;
    }
    for (size_t byte = 0; byte < size; ++byte) {
        g_userCode[byte] = begin[byte];
    }
    g_probeMode = 5;
    g_probeSucceeded = false;
    g_probeReturnRip = kUserBase + size - 2;
    MicantProbeEnter();
    if (!g_probeSucceeded) {
        return false;
    }
    __asm__ __volatile__(
        "movw $0x18, %%ax\nmovw %%ax, %%ds\nmovw %%ax, %%es\n"
        : : : "rax", "memory"
    );
    return true;
}

bool verifyNativeImageLoading() noexcept {
    using namespace micant::pe;
    if (!g_dispatch || !g_probeSucceeded) return false;
    constexpr uint64_t imageBase = kUserBase + 0x10000;
    const auto* client = reinterpret_cast<const uint8_t*>(MicantSyscallClientProgram);
    const size_t clientSize = reinterpret_cast<uint64_t>(MicantSyscallClientEnd) -
                              reinterpret_cast<uint64_t>(client);
    const size_t breakpointOffset =
        reinterpret_cast<uint64_t>(MicantSyscallClientBreakpoint) -
        reinterpret_cast<uint64_t>(client);
    if (clientSize > 4096 - 16 || breakpointOffset >= clientSize) {
        return false;
    }
    ImageDosHeader dos{};
    dos.e_magic = DOS_MAGIC;
    dos.e_lfanew = 64;
    memcpy(g_nativeProbeFile, &dos, sizeof(dos));
    ImageNtHeaders64 nt{};
    nt.signature = NT_SIGNATURE;
    nt.fileHeader.machine = MACHINE_AMD64;
    nt.fileHeader.numberOfSections = 1;
    nt.fileHeader.sizeOfOptionalHeader = sizeof(ImageOptionalHeader64);
    nt.fileHeader.characteristics = IMAGE_FILE_EXECUTABLE_IMAGE;
    nt.optionalHeader.magic = PE32PLUS_MAGIC;
    nt.optionalHeader.imageBase = imageBase;
    nt.optionalHeader.addressOfEntryPoint = 4096;
    nt.optionalHeader.sectionAlignment = 4096;
    nt.optionalHeader.fileAlignment = 512;
    nt.optionalHeader.sizeOfImage = sizeof(g_nativeProbeImage);
    nt.optionalHeader.sizeOfHeaders = 512;
    nt.optionalHeader.subsystem = IMAGE_SUBSYSTEM_WINDOWS_GUI;
    memcpy(g_nativeProbeFile + 64, &nt, sizeof(nt));
    ImageSectionHeader text{};
    text.virtualAddress = 4096;
    text.misc.virtualSize = 4096;
    text.pointerToRawData = 512;
    text.sizeOfRawData = 4096;
    text.characteristics = IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ;
    memcpy(g_nativeProbeFile + 64 + sizeof(nt), &text, sizeof(text));
    // CALL gives a compiled Windows x64 function the required entry alignment.
    constexpr uint8_t call[]{0x48, 0x83, 0xEC, 0x20, 0xE8, 7, 0, 0, 0, 0x0F, 0x0B};
    memcpy(g_nativeProbeFile + 512, call, sizeof(call));
    for (size_t byte = 0; byte < clientSize; ++byte) {
        g_nativeProbeFile[512 + 16 + byte] = client[byte];
    }
    if (g_nativeProbeModules.addModule("syscall-probe.exe", g_nativeProbeFile,
            g_nativeProbeImage, imageBase) != micant::NtStatus::Success ||
        g_nativeProbeModules.bindAll() != micant::NtStatus::Success ||
        !mapBootstrapUserMemory(imageBase, g_nativeProbeImage, 4096, false, false) ||
        !mapBootstrapUserMemory(imageBase + 4096, g_nativeProbeImage + 4096,
                                4096, false, true)) {
        return false;
    }
    MicantProbeUserRip = imageBase + 4096;
    g_probeMode = 6;
    g_probeSucceeded = false;
    g_probeReturnRip = imageBase + 4096 + 16 + breakpointOffset + 1;
    MicantProbeEnter();
    MicantProbeUserRip = kUserBase;
    __asm__ __volatile__(
        "movw $0x18, %%ax\nmovw %%ax, %%ds\nmovw %%ax, %%es\n"
        : : : "rax", "memory"
    );
    return g_probeSucceeded;
}

void* mapBootstrapDeviceMemory(uint64_t physical, size_t size) noexcept {
    if (!g_addressSpaceReady || size == 0 || size > 1024 * 1024 ||
        physical > kPhysicalMask || size > kPhysicalMask - physical) {
        return nullptr;
    }
    const uint64_t offset = physical & 4095;
    const uint64_t length = (size + offset + 4095) & ~uint64_t{4095};
    const uint64_t start = g_deviceNext;
    if (length > 16 * 1024 * 1024 - (start - kDeviceBase)) {
        return nullptr;
    }
    for (uint64_t page = 0; page < length; page += 4096) {
        // PWT/PCD avoid caching device registers; no user-accessible MMIO.
        if (!mapPage(start + page, (physical & ~uint64_t{4095}) + page,
                     kWritable | kNoExecute | 0x18)) {
            return nullptr;
        }
    }
    g_deviceNext += length;
    return reinterpret_cast<void*>(start + offset);
}

bool verifyRing3Transition() noexcept {
    if (!g_addressSpaceReady ||
        !mapPage(kUserBase, reinterpret_cast<uint64_t>(g_userCode), kUser) ||
        !mapPage(kUserBase + 4096, reinterpret_cast<uint64_t>(g_userStack),
                 kUser | kWritable | kNoExecute)) {
        return false;
    }
    // Read CS at CPL=3, verify RPL=3, then enter the kernel via #BP.
    constexpr uint8_t code[]{
        0x8c, 0xc8,             // mov ax, cs
        0x83, 0xe0, 0x03,       // and eax, 3
        0x83, 0xf8, 0x03,       // cmp eax, 3
        0x75, 0x02,             // jne fault
        0xcc,                   // int3
        0x90,                   // nop (unreachable)
        0x0f, 0x0b,             // ud2
    };
    for (size_t index = 0; index < sizeof(code); ++index) {
        g_userCode[index] = code[index];
    }
    g_userStack[0] = 0xCC; // A missing NX bit must trap, not execute arbitrary bytes.
    auto& hardware = cpu::CpuHardwareEngine::get();
    hardware.initialize(reinterpret_cast<uint64_t>(g_kernelStack + sizeof(g_kernelStack)), 0);
    const auto& gdtr = hardware.getGdtr();
    __asm__ __volatile__(
        "cli\nlgdt %0\n"
        "pushq $0x10\nleaq 1f(%%rip), %%rax\npushq %%rax\nlretq\n1:\n"
        "movw $0x18, %%ax\nmovw %%ax, %%ds\nmovw %%ax, %%es\nmovw %%ax, %%ss\n"
        "movw $0x40, %%ax\nltr %%ax\n"
        : : "m"(gdtr) : "rax", "memory"
    );
    for (size_t vector = 0; vector < 256; ++vector) {
        const uint64_t handler = vector == 3
            ? reinterpret_cast<uint64_t>(MicantProbeTrapEntry)
            : vector == 14 ? reinterpret_cast<uint64_t>(MicantProbePageFaultEntry)
                          : reinterpret_cast<uint64_t>(MicantProbeFaultEntry);
        g_idt[vector] = {
            static_cast<uint16_t>(handler),
            cpu::SELECTOR_KCODE64, 1,
            static_cast<uint8_t>(vector == 3 ? 0xEE : 0x8E),
            static_cast<uint16_t>(handler >> 16),
            static_cast<uint32_t>(handler >> 32), 0,
        };
    }
    const Idtr idtr{sizeof(g_idt) - 1, reinterpret_cast<uint64_t>(g_idt)};
    __asm__ __volatile__("lidt %0" : : "m"(idtr) : "memory");
    g_probeReturnRip = kUserBase + 11;
    MicantProbeEnter();
    if (!g_probeSucceeded) {
        return false;
    }
    // User accesses must fault: kernel/MMIO reads, code writes, stack execution.
    for (uint32_t mode = 1; mode <= 4; ++mode) {
        g_probeMode = mode;
        g_probeSucceeded = false;
        g_userCode[0] = 0x48;
        g_userCode[1] = 0xB8; // movabs rax, address
        const uint64_t address = mode == 1
            ? reinterpret_cast<uint64_t>(g_pml4)
            : mode == 2 ? kUserBase
            : mode == 3 ? kUserBase + 4096 : kDeviceBase;
        for (size_t byte = 0; byte < sizeof(address); ++byte) {
            g_userCode[2 + byte] = static_cast<uint8_t>(address >> (byte * 8));
        }
        g_userCode[10] = mode == 3 ? 0xFF : mode == 2 ? 0x89 : 0x8B;
        g_userCode[11] = mode == 3 ? 0xE0 : 0x00; // jmp rax / write / read
        g_userCode[12] = 0x0F;
        g_userCode[13] = 0x0B; // ud2 if protection was bypassed
        MicantProbeEnter();
        if (!g_probeSucceeded) {
            return false;
        }
    }
    __asm__ __volatile__(
        "movw $0x18, %%ax\nmovw %%ax, %%ds\nmovw %%ax, %%es\n"
        : : : "rax", "memory"
    );
    return g_probeSucceeded;
}

} // namespace micant::hal::amd64
