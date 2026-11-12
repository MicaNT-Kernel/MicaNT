#include "micant/km/executive/ps.hpp"
#include "micant/km/executive/usermode.hpp"
#include "micant/km/core/syscalls.hpp"
#include "micant/km/hal/amd64_bootstrap.hpp"
#include "micant/um/native/pe.hpp"
#include <cstring>
#include <span>

extern "C" {
extern const uint8_t MicantTwoUserThreadsEnd[];
__attribute__((naked, section(".uthreads$a"))) void MicantTwoUserThreads() noexcept {
    __asm__ __volatile__(
        "movq 0(%rsp), %rbx\nmovq 8(%rsp), %r12\nmovq 16(%rsp), %r13\n"
        "movabsq $0x1111222233334444, %rax\ntestq %r13, %r13\njz 1f\n"
        "movabsq $0xaaaabbbbccccdddd, %rax\n"
        "1: movq %rax, %gs:0x1480\ntestq %r13, %r13\njnz 3f\n"
        // Thread A allocates and checks a page through the native syscall.
        "subq $0x60, %rsp\nmovq $0, 64(%rsp)\nmovq $4096, 72(%rsp)\n"
        "movq $0x3000, 40(%rsp)\nmovq $4, 48(%rsp)\n"
        "movq $-1, %r10\nleaq 64(%rsp), %rdx\nxorl %r8d, %r8d\n"
        "leaq 72(%rsp), %r9\nmovl $0x18, %eax\nsyscall\n"
        "testl %eax, %eax\njnz 9f\nmovq 64(%rsp), %rax\ntestq %rax, %rax\njz 9f\n"
        "movq $0x12345678, (%rax)\ncmpq $0x12345678, (%rax)\njne 9f\n"
        // Thread A blocks on the first event and later wakes thread B.
        "movq %rbx, %r10\nxorl %edx, %edx\nxorl %r8d, %r8d\nmovl $4, %eax\nsyscall\n"
        "testl %eax, %eax\njnz 9f\nmovabsq $0x1111222233334444, %rax\n"
        "cmpq %gs:0x1480, %rax\njne 9f\n"
        "movq %r12, %r10\nxorl %edx, %edx\nmovl $0x4e, %eax\nsyscall\n"
        "testl %eax, %eax\njnz 9f\nmovl $0xc0000022, %edx\njmp 8f\n"
        // Thread B signals A, then blocks until A signals its event.
        "3: movq %rbx, %r10\nxorl %edx, %edx\nmovl $0x4e, %eax\nsyscall\n"
        "testl %eax, %eax\njnz 9f\n"
        "movq %r12, %r10\nxorl %edx, %edx\nxorl %r8d, %r8d\nmovl $4, %eax\nsyscall\n"
        "ud2\n"
        "9: movl $0xc000000d, %edx\n"
        "8: movq $-1, %r10\nmovl $0x2c, %eax\nsyscall\nud2\n");
}
__asm__(".section .uthreads$z,\"xr\"\n.globl MicantTwoUserThreadsEnd\n"
        "MicantTwoUserThreadsEnd:\n.text\n");
}

namespace {
struct Start {
    micant::ke::KThread* boot;
    uintptr_t entry;
    uintptr_t stack;
    bool queueBoot;
};
void enter(void* argument) noexcept {
    auto& start = *static_cast<Start*>(argument);
    if (start.queueBoot && !micant::ke::PriorityScheduler::get().readyKThread(start.boot)) __builtin_trap();
    micant::hal::amd64::enterBootstrapUserThread(start.entry, start.stack);
}
}

extern "C" bool MicantTestPsTwoUserThreads() noexcept {
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
        ps::EThread first;
        ps::EThread second;
        if (process.initializeAddressSpace() != NtStatus::Success) return false;
        Handle sourceFirst{}, sourceSecond{}, eventFirst{}, eventSecond{};
        constexpr uint32_t access = ob::SYNCHRONIZE | 2;
        auto& source = ob::detail::currentHandleTable();
        if (sys::NtCreateEvent(&sourceFirst, access, nullptr, 1, false) != NtStatus::Success ||
            sys::NtCreateEvent(&sourceSecond, access, nullptr, 1, false) != NtStatus::Success ||
            source.duplicateHandle(sourceFirst, process.getHandleTable(), eventFirst) != NtStatus::Success ||
            source.duplicateHandle(sourceSecond, process.getHandleTable(), eventSecond) != NtStatus::Success ||
            sys::NtClose(sourceFirst) != NtStatus::Success || sys::NtClose(sourceSecond) != NtStatus::Success ||
            !hal::amd64::switchProcessPagingRootByPhysical(process.getDirectoryTableBase())) return false;
        auto& space = process.getAddressSpace();
        uintptr_t code{}, stacks[2]{}, tebs[2]{};
        if (space.allocate(code, 2 * mm::PageSize4KB, mm::MEM_RESERVE | mm::MEM_COMMIT, mm::PAGE_READWRITE) != NtStatus::Success)
            return false;
        for (size_t i = 0; i < 2; ++i) {
            if (space.allocate(stacks[i], mm::PageSize4KB, mm::MEM_RESERVE | mm::MEM_COMMIT, mm::PAGE_READWRITE) != NtStatus::Success ||
                space.allocate(tebs[i], 2 * mm::PageSize4KB, mm::MEM_RESERVE | mm::MEM_COMMIT, mm::PAGE_READWRITE) != NtStatus::Success)
                return false;
            auto* teb = reinterpret_cast<um::Teb64*>(tebs[i]);
            teb->self = tebs[i];
            teb->stackBase = stacks[i] + mm::PageSize4KB;
            teb->stackLimit = stacks[i];
            auto* arguments = reinterpret_cast<uint64_t*>(stacks[i] + mm::PageSize4KB - 32);
            arguments[0] = eventFirst;
            arguments[1] = eventSecond;
            arguments[2] = i;
        }
        const auto* program = reinterpret_cast<const uint8_t*>(MicantTwoUserThreads);
        const size_t programSize = reinterpret_cast<uintptr_t>(MicantTwoUserThreadsEnd) - reinterpret_cast<uintptr_t>(program);
        if (!programSize || programSize > mm::PageSize4KB) return false;
        static constinit uint8_t file[512 + mm::PageSize4KB]{};
        pe::ImageDosHeader dos{};
        dos.e_magic = pe::DOS_MAGIC;
        dos.e_lfanew = 64;
        std::memcpy(file, &dos, sizeof(dos));
        pe::ImageNtHeaders64 nt{};
        nt.signature = pe::NT_SIGNATURE;
        nt.fileHeader.machine = pe::MACHINE_AMD64;
        nt.fileHeader.numberOfSections = 1;
        nt.fileHeader.sizeOfOptionalHeader = sizeof(pe::ImageOptionalHeader64);
        nt.fileHeader.characteristics = pe::IMAGE_FILE_EXECUTABLE_IMAGE;
        nt.optionalHeader.magic = pe::PE32PLUS_MAGIC;
        nt.optionalHeader.imageBase = code;
        nt.optionalHeader.addressOfEntryPoint = mm::PageSize4KB;
        nt.optionalHeader.sectionAlignment = mm::PageSize4KB;
        nt.optionalHeader.fileAlignment = 512;
        nt.optionalHeader.sizeOfImage = 2 * mm::PageSize4KB;
        nt.optionalHeader.sizeOfHeaders = 512;
        nt.optionalHeader.subsystem = pe::IMAGE_SUBSYSTEM_WINDOWS_CUI;
        std::memcpy(file + 64, &nt, sizeof(nt));
        pe::ImageSectionHeader text{};
        text.virtualAddress = mm::PageSize4KB;
        text.misc.virtualSize = mm::PageSize4KB;
        text.pointerToRawData = 512;
        text.sizeOfRawData = mm::PageSize4KB;
        text.characteristics = pe::IMAGE_SCN_MEM_EXECUTE | pe::IMAGE_SCN_MEM_READ;
        std::memcpy(file + 64 + sizeof(nt), &text, sizeof(text));
        std::memcpy(file + 512, program, programSize);
        pe::PeImageView64 image;
        if (image.inspect(std::span<const uint8_t>{file}) != NtStatus::Success ||
            image.map(std::span<uint8_t>{reinterpret_cast<uint8_t*>(code), 2 * mm::PageSize4KB}, code) != NtStatus::Success)
            return false;
        uint32_t old{};
        if (space.protect(code, mm::PageSize4KB, mm::PAGE_READONLY, &old) != NtStatus::Success ||
            space.protect(code + mm::PageSize4KB, mm::PageSize4KB, mm::PAGE_EXECUTE_READ, &old) != NtStatus::Success ||
            !hal::amd64::switchProcessPagingRootByPhysical(previous)) return false;
        const uintptr_t entry = code + nt.optionalHeader.addressOfEntryPoint;
        Start starts[2]{{boot, entry, stacks[0] + mm::PageSize4KB - 32, true},
                        {boot, entry, stacks[1] + mm::PageSize4KB - 32, false}};
        if (first.initialize(process, 32768, enter, &starts[0]) != NtStatus::Success ||
            second.initialize(process, 32768, enter, &starts[1]) != NtStatus::Success) return false;
        first.kernelThread()->tebAddress = tebs[0];
        second.kernelThread()->tebAddress = tebs[1];
        first.kernelThread()->priority = second.kernelThread()->priority = boot->priority + 1;
        if (!first.kernelThread()->priority || first.ready() != NtStatus::Success ||
            second.ready() != NtStatus::Success) return false;
        auto* next = scheduler.selectNextThread();
        if (next != first.kernelThread()) return false;
        const KIRQL oldIrql = KfRaiseIrql(DISPATCH_LEVEL);
        const bool switched = KiSwapContext(boot, next);
        KeLowerIrql(oldIrql);
        if (!switched || !process.isTerminating() || process.exitStatus() != NtStatus::AccessDenied ||
            first.kernelThread()->state != ThreadState::Terminated ||
            second.kernelThread()->state != ThreadState::Terminated || scheduler.getReadyThreadCount() ||
            !hal::amd64::switchProcessPagingRootByPhysical(process.getDirectoryTableBase())) return false;
        const bool tls = reinterpret_cast<um::Teb64*>(tebs[0])->tlsSlots[0] == 0x1111222233334444ULL &&
                         reinterpret_cast<um::Teb64*>(tebs[1])->tlsSlots[0] == 0xaaaabbbbccccddddULL;
        if (!hal::amd64::switchProcessPagingRootByPhysical(previous) || !tls ||
            first.reap() != NtStatus::Success || second.reap() != NtStatus::Success ||
            process.closeAddressSpace() != NtStatus::Success) return false;
    }
    return mm::PfnDatabase::get().getActiveCount() == active;
}
