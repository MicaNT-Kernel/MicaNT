#include "micant/km/executive/ps.hpp"
#include "micant/km/core/ke.hpp"
#include "micant/km/hal/amd64_bootstrap.hpp"

extern "C" {
extern const uint8_t MicantPsUserProgramEnd[];
__attribute__((naked, section(".uproc$a"))) void MicantPsUserProgram() noexcept {
    __asm__ __volatile__(
        "subq $0x60, %rsp\n"
        "movq $0, 64(%rsp)\nmovq $4096, 72(%rsp)\n"
        "movq $0x3000, 40(%rsp)\nmovq $4, 48(%rsp)\n"
        "movq $-1, %r10\nleaq 64(%rsp), %rdx\nxorq %r8, %r8\n"
        "leaq 72(%rsp), %r9\nmovl $0x18, %eax\nsyscall\n"
        "testl %eax, %eax\njnz 1f\nmovq 64(%rsp), %rax\ntestq %rax, %rax\njz 1f\n"
        "movabsq $0x123456789abcdef0, %rcx\nmovq %rcx, (%rax)\n"
        "cmpq (%rax), %rcx\njne 1f\nmovl $0xc0000022, %edx\njmp 2f\n"
        "1: movl $0xc000000d, %edx\n"
        "2: movq $-1, %r10\nmovl $0x2c, %eax\nsyscall\nud2\n");
}
__asm__(".section .uproc$z,\"xr\"\n.globl MicantPsUserProgramEnd\n"
        "MicantPsUserProgramEnd:\n.text\n");
}

namespace {
struct UserStart {
    micant::ke::KThread* boot;
    uintptr_t entry;
    uintptr_t stack;
};

void enterUser(void* argument) noexcept {
    auto& start = *static_cast<UserStart*>(argument);
    if (!micant::ke::PriorityScheduler::get().readyKThread(start.boot)) __builtin_trap();
    micant::hal::amd64::enterBootstrapUserThread(start.entry, start.stack);
}
}

extern "C" bool MicantTestPsUserProcess() noexcept {
    using namespace micant;
    using namespace micant::ke;
    auto& scheduler = PriorityScheduler::get();
    auto* boot = KeGetCurrentThread();
    if (!boot || scheduler.getReadyThreadCount()) return false;
    const size_t active = mm::PfnDatabase::get().getActiveCount();
    uintptr_t previous{};
    __asm__ __volatile__("movq %%cr3, %0" : "=r"(previous));
    {
        ps::EProcess process;
        ps::EThread thread;
        if (process.initializeAddressSpace() != NtStatus::Success ||
            !hal::amd64::switchProcessPagingRootByPhysical(process.getDirectoryTableBase())) return false;
        uintptr_t code{};
        uintptr_t stack{};
        auto& space = process.getAddressSpace();
        if (space.allocate(code, mm::PageSize4KB, mm::MEM_RESERVE | mm::MEM_COMMIT,
                           mm::PAGE_READWRITE) != NtStatus::Success ||
            space.allocate(stack, mm::PageSize4KB, mm::MEM_RESERVE | mm::MEM_COMMIT,
                           mm::PAGE_READWRITE) != NtStatus::Success) return false;
        const auto* program = reinterpret_cast<const uint8_t*>(MicantPsUserProgram);
        const size_t programSize = reinterpret_cast<uintptr_t>(MicantPsUserProgramEnd) -
                                   reinterpret_cast<uintptr_t>(program);
        if (!programSize || programSize > mm::PageSize4KB) return false;
        auto* destination = reinterpret_cast<uint8_t*>(code);
        for (size_t i = 0; i < programSize; ++i) destination[i] = program[i];
        uint32_t old{};
        if (space.protect(code, mm::PageSize4KB, mm::PAGE_EXECUTE_READ, &old) != NtStatus::Success ||
            old != mm::PAGE_READWRITE || !hal::amd64::switchProcessPagingRootByPhysical(previous)) return false;
        UserStart start{boot, code, stack + mm::PageSize4KB - 16};
        if (thread.initialize(process, 32768, enterUser, &start) != NtStatus::Success ||
            thread.ready() != NtStatus::Success) return false;
        auto* next = scheduler.selectNextThread();
        if (next != thread.kernelThread()) return false;
        const KIRQL oldIrql = KfRaiseIrql(DISPATCH_LEVEL);
        const bool switched = KiSwapContext(boot, next);
        KeLowerIrql(oldIrql);
        if (!switched || KeGetCurrentThread() != boot || !process.isTerminating() ||
            process.exitStatus() != NtStatus::AccessDenied || next->state != ThreadState::Terminated ||
            scheduler.getReadyThreadCount() || thread.reap() != NtStatus::Success ||
            process.closeAddressSpace() != NtStatus::Success) return false;
    }
    return mm::PfnDatabase::get().getActiveCount() == active;
}
