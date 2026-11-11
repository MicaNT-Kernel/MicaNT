#include "micant/km/hal/amd64_bootstrap.hpp"
#include "micant/km/hal/cpu.hpp"

extern "C" void* memset(void* destination, int value, size_t count) {
    auto* bytes = static_cast<volatile uint8_t*>(destination);
    for (size_t index = 0; index < count; ++index) {
        bytes[index] = static_cast<uint8_t>(value);
    }
    return destination;
}

namespace {

constexpr uint64_t kPresent = 1;
constexpr uint64_t kWritable = 2;
constexpr uint64_t kUser = 4;
constexpr uint64_t kNoExecute = uint64_t{1} << 63;
constexpr uint64_t kPhysicalMask = 0x000FFFFFFFFFF000ULL;
constexpr uint64_t kUserBase = uint64_t{128} << 39;
constexpr uint64_t kDeviceBase = uint64_t{129} << 39;
alignas(4096) uint64_t g_pml4[512]{};
alignas(4096) uint64_t g_tables[32][512]{};
size_t g_tableCount{};
uint64_t g_deviceNext = kDeviceBase;
bool g_addressSpaceReady{};
alignas(4096) uint8_t g_userCode[4096]{};
alignas(4096) uint8_t g_userStack[4096]{};
alignas(16) uint8_t g_kernelStack[32768]{};
volatile bool g_probeSucceeded{};
uint32_t g_probeMode{};
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

bool mapPage(uint64_t address, uint64_t physical, uint64_t flags) noexcept {
    uint64_t* table = g_pml4;
    for (int shift = 39; shift > 12; shift -= 9) {
        uint64_t& entry = table[(address >> shift) & 511];
        if ((entry & kPresent) == 0) {
            if (g_tableCount == 32) {
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
    g_probeSucceeded = g_probeMode == 0 &&
                       frame[0] == kUserBase + 11 &&
                       frame[1] == micant::cpu::SELECTOR_UCODE64 &&
                       frame[4] == micant::cpu::SELECTOR_UDATA64 &&
                       frame[3] == kUserBase + 8192 - 16 &&
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
        "movabsq $0x400000000000, %rax\npushq %rax\niretq\n"
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
