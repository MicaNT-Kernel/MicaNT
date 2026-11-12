#include "micant/km/executive/ps.hpp"
#include "micant/km/executive/usermode.hpp"
#include "micant/km/core/syscalls.hpp"
#include "micant/km/hal/amd64_bootstrap.hpp"
#include "micant/um/native/pe.hpp"
#include <cstring>
#include <span>

namespace {
const uint8_t nativeDll[]{
#include "ntdll-process.inc"
};
}

extern "C" {
extern const uint8_t MicantTwoUserThreadsEnd[];
__attribute__((naked, section(".uthreads$a"))) void MicantTwoUserThreads() noexcept {
    __asm__ __volatile__(
        "movq %rsp, %r14\nmovq 0(%r14), %rbx\nmovq 8(%r14), %r12\nmovq 16(%r14), %r13\n"
        "subq $0x60, %rsp\n"
        // Static PE TLS vector slot zero points to a private template copy.
        "movq %gs:0x58, %rax\ntestq %rax, %rax\njz 9f\nmovq (%rax), %rax\ntestq %rax, %rax\njz 9f\n"
        "movabsq $0x8877665544332211, %rdx\ncmpq %rdx, (%rax)\njne 9f\ncmpq $0, 8(%rax)\njne 9f\n"
        "testq %r13, %r13\njnz 4f\nmovq $0x1234, (%rax)\njmp 5f\n"
        "4: movq $0x5678, (%rax)\n5:\n"
        "movabsq $0x1111222233334444, %rax\ntestq %r13, %r13\njz 1f\n"
        "movabsq $0xaaaabbbbccccdddd, %rax\n"
        "1: movq %rax, %gs:0x1480\ntestq %r13, %r13\njnz 3f\n"
        // Thread A allocates and checks a page through the native syscall.
        "movq $0, 64(%rsp)\nmovq $4096, 72(%rsp)\n"
        "movq $0x3000, 32(%rsp)\nmovq $4, 40(%rsp)\n"
        "movq $-1, %rcx\nleaq 64(%rsp), %rdx\nxorl %r8d, %r8d\n"
        "leaq 72(%rsp), %r9\ncallq *24(%r14)\n"
        "testl %eax, %eax\njnz 9f\nmovq 64(%rsp), %rax\ntestq %rax, %rax\njz 9f\n"
        "movq $0x12345678, (%rax)\ncmpq $0x12345678, (%rax)\njne 9f\n"
        // Thread A blocks on the first event and later wakes thread B.
        "movq %rbx, %rcx\nxorl %edx, %edx\nxorl %r8d, %r8d\ncallq *32(%r14)\n"
        "testl %eax, %eax\njnz 9f\nmovabsq $0x1111222233334444, %rax\n"
        "cmpq %gs:0x1480, %rax\njne 9f\n"
        "movq %gs:0x58, %rax\nmovq (%rax), %rax\ncmpq $0x1234, (%rax)\njne 9f\n"
        "movq %r12, %rcx\nxorl %edx, %edx\ncallq *40(%r14)\n"
        "testl %eax, %eax\njnz 9f\nmovl $0xc0000022, %edx\njmp 8f\n"
        // Thread B signals A, then blocks until A signals its event.
        "3: movq %rbx, %rcx\nxorl %edx, %edx\ncallq *40(%r14)\n"
        "testl %eax, %eax\njnz 9f\n"
        "movq %r12, %rcx\nxorl %edx, %edx\nxorl %r8d, %r8d\ncallq *32(%r14)\n"
        "ud2\n"
        "9: movl $0xc000000d, %edx\n"
        "8: movq $-1, %rcx\ncallq *48(%r14)\nud2\n");
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
        uintptr_t code{}, stacks[2]{}, tebs[2]{}, tlsBlocks[2]{};
        if (space.allocate(code, 3 * mm::PageSize4KB, mm::MEM_RESERVE | mm::MEM_COMMIT, mm::PAGE_READWRITE) != NtStatus::Success)
            return false;
        for (size_t i = 0; i < 2; ++i) {
            if (space.allocate(stacks[i], mm::PageSize4KB, mm::MEM_RESERVE | mm::MEM_COMMIT, mm::PAGE_READWRITE) != NtStatus::Success ||
                space.allocate(tebs[i], 2 * mm::PageSize4KB, mm::MEM_RESERVE | mm::MEM_COMMIT, mm::PAGE_READWRITE) != NtStatus::Success ||
                space.allocate(tlsBlocks[i], mm::PageSize4KB, mm::MEM_RESERVE | mm::MEM_COMMIT, mm::PAGE_READWRITE) != NtStatus::Success)
                return false;
            auto* teb = reinterpret_cast<um::Teb64*>(tebs[i]);
            teb->self = tebs[i];
            teb->stackBase = stacks[i] + mm::PageSize4KB;
            teb->stackLimit = stacks[i];
            teb->threadLocalStoragePointer = tlsBlocks[i];
            *reinterpret_cast<uint64_t*>(tlsBlocks[i]) = tlsBlocks[i] + 0x100;
            auto* arguments = reinterpret_cast<uint64_t*>(stacks[i] + mm::PageSize4KB - 128);
            arguments[0] = eventFirst;
            arguments[1] = eventSecond;
            arguments[2] = i;
        }
        const auto* program = reinterpret_cast<const uint8_t*>(MicantTwoUserThreads);
        const size_t programSize = reinterpret_cast<uintptr_t>(MicantTwoUserThreadsEnd) - reinterpret_cast<uintptr_t>(program);
        if (!programSize || programSize > mm::PageSize4KB) return false;
        static constinit uint8_t file[512 + 2 * mm::PageSize4KB]{};
        pe::ImageDosHeader dos{};
        dos.e_magic = pe::DOS_MAGIC;
        dos.e_lfanew = 64;
        std::memcpy(file, &dos, sizeof(dos));
        pe::ImageNtHeaders64 nt{};
        nt.signature = pe::NT_SIGNATURE;
        nt.fileHeader.machine = pe::MACHINE_AMD64;
        nt.fileHeader.numberOfSections = 2;
        nt.fileHeader.sizeOfOptionalHeader = sizeof(pe::ImageOptionalHeader64);
        nt.fileHeader.characteristics = pe::IMAGE_FILE_EXECUTABLE_IMAGE;
        nt.optionalHeader.magic = pe::PE32PLUS_MAGIC;
        nt.optionalHeader.imageBase = code;
        nt.optionalHeader.addressOfEntryPoint = mm::PageSize4KB;
        nt.optionalHeader.sectionAlignment = mm::PageSize4KB;
        nt.optionalHeader.fileAlignment = 512;
        nt.optionalHeader.sizeOfImage = 3 * mm::PageSize4KB;
        nt.optionalHeader.sizeOfHeaders = 512;
        nt.optionalHeader.subsystem = pe::IMAGE_SUBSYSTEM_WINDOWS_CUI;
        nt.optionalHeader.numberOfRvaAndSizes = 16;
        nt.optionalHeader.dataDirectory[pe::IMAGE_DIRECTORY_ENTRY_TLS] =
            {2 * mm::PageSize4KB, sizeof(pe::ImageTlsDirectory64)};
        std::memcpy(file + 64, &nt, sizeof(nt));
        pe::ImageSectionHeader text{};
        text.virtualAddress = mm::PageSize4KB;
        text.misc.virtualSize = mm::PageSize4KB;
        text.pointerToRawData = 512;
        text.sizeOfRawData = mm::PageSize4KB;
        text.characteristics = pe::IMAGE_SCN_MEM_EXECUTE | pe::IMAGE_SCN_MEM_READ;
        std::memcpy(file + 64 + sizeof(nt), &text, sizeof(text));
        pe::ImageSectionHeader data{};
        data.virtualAddress = 2 * mm::PageSize4KB;
        data.misc.virtualSize = mm::PageSize4KB;
        data.pointerToRawData = 512 + mm::PageSize4KB;
        data.sizeOfRawData = mm::PageSize4KB;
        data.characteristics = pe::IMAGE_SCN_MEM_READ | pe::IMAGE_SCN_MEM_WRITE;
        std::memcpy(file + 64 + sizeof(nt) + sizeof(text), &data, sizeof(data));
        std::memcpy(file + 512, program, programSize);
        pe::ImageTlsDirectory64 tlsDirectory{};
        tlsDirectory.startAddressOfRawData = code + 2 * mm::PageSize4KB + 0x40;
        tlsDirectory.endAddressOfRawData = tlsDirectory.startAddressOfRawData + 8;
        tlsDirectory.addressOfIndex = code + 2 * mm::PageSize4KB + 0x50;
        tlsDirectory.sizeOfZeroFill = 8;
        tlsDirectory.characteristics = 5u << 20;
        std::memcpy(file + 512 + mm::PageSize4KB, &tlsDirectory, sizeof(tlsDirectory));
        constexpr uint64_t seed = 0x8877665544332211ULL;
        std::memcpy(file + 512 + mm::PageSize4KB + 0x40, &seed, sizeof(seed));
        pe::PeImageView64 image;
        if (image.inspect(std::span<const uint8_t>{file}) != NtStatus::Success ||
            image.map(std::span<uint8_t>{reinterpret_cast<uint8_t*>(code), 3 * mm::PageSize4KB}, code) != NtStatus::Success)
            return false;
        pe::PeImageView64::TlsLayout tlsLayout{};
        if (image.tlsLayout(std::span<const uint8_t>{reinterpret_cast<const uint8_t*>(code), 3 * mm::PageSize4KB},
                            code, tlsLayout) != NtStatus::Success || !tlsLayout.present || tlsLayout.callbackCount ||
            tlsLayout.indexRva != 2 * mm::PageSize4KB + 0x50) return false;
        *reinterpret_cast<uint32_t*>(code + tlsLayout.indexRva) = 0;
        for (const auto block : tlsBlocks) {
            if (pe::PeImageView64::initializeTlsData(tlsLayout,
                    std::span<uint8_t>{reinterpret_cast<uint8_t*>(block + 0x100), 16}) != NtStatus::Success)
                return false;
        }
        pe::PeImageView64 dll;
        if (dll.inspect(std::span<const uint8_t>{nativeDll}) != NtStatus::Success) return false;
        uintptr_t dllBase = dll.headers().optionalHeader.imageBase;
        const size_t dllSize = dll.headers().optionalHeader.sizeOfImage;
        if (space.allocate(dllBase, dllSize, mm::MEM_RESERVE | mm::MEM_COMMIT, mm::PAGE_READWRITE) != NtStatus::Success ||
            dll.map(std::span<uint8_t>{reinterpret_cast<uint8_t*>(dllBase), dllSize}, dllBase) != NtStatus::Success)
            return false;
        constexpr const char* names[]{"NtAllocateVirtualMemory", "NtWaitForSingleObject", "NtSetEvent", "NtTerminateProcess"};
        for (size_t i = 0; i < 4; ++i) {
            pe::PeImageView64::ExportTarget target{};
            if (dll.findExport(std::span<const uint8_t>{reinterpret_cast<const uint8_t*>(dllBase), dllSize},
                               dllBase, names[i], 0, target) != NtStatus::Success || !target.address ||
                !target.forwarder.empty()) return false;
            for (size_t thread = 0; thread < 2; ++thread) {
                auto* arguments = reinterpret_cast<uint64_t*>(stacks[thread] + mm::PageSize4KB - 128);
                arguments[3 + i] = target.address;
            }
        }
        uint32_t old{};
        if (space.protect(dllBase, mm::PageSize4KB, mm::PAGE_READONLY, &old) != NtStatus::Success) return false;
        for (size_t i = 0; i < dll.headers().fileHeader.numberOfSections; ++i) {
            const auto section = dll.section(i);
            const size_t size = section.misc.virtualSize > section.sizeOfRawData ? section.misc.virtualSize : section.sizeOfRawData;
            if (!size) continue;
            if ((section.virtualAddress & mm::PageMask4KB) || size > SIZE_MAX - mm::PageMask4KB) return false;
            const auto protection = (section.characteristics & pe::IMAGE_SCN_MEM_EXECUTE) ? mm::PAGE_EXECUTE_READ :
                (section.characteristics & pe::IMAGE_SCN_MEM_WRITE) ? mm::PAGE_READWRITE : mm::PAGE_READONLY;
            if (space.protect(dllBase + section.virtualAddress, (size + mm::PageMask4KB) & ~mm::PageMask4KB,
                              protection, &old) != NtStatus::Success) return false;
        }
        if (space.protect(code, mm::PageSize4KB, mm::PAGE_READONLY, &old) != NtStatus::Success ||
            space.protect(code + mm::PageSize4KB, mm::PageSize4KB, mm::PAGE_EXECUTE_READ, &old) != NtStatus::Success ||
            !hal::amd64::switchProcessPagingRootByPhysical(previous)) return false;
        const uintptr_t entry = code + nt.optionalHeader.addressOfEntryPoint;
        Start starts[2]{{boot, entry, stacks[0] + mm::PageSize4KB - 128, true},
                        {boot, entry, stacks[1] + mm::PageSize4KB - 128, false}};
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
                         reinterpret_cast<um::Teb64*>(tebs[1])->tlsSlots[0] == 0xaaaabbbbccccddddULL &&
                         *reinterpret_cast<uint64_t*>(tlsBlocks[0] + 0x100) == 0x1234 &&
                         *reinterpret_cast<uint64_t*>(tlsBlocks[1] + 0x100) == 0x5678;
        if (!hal::amd64::switchProcessPagingRootByPhysical(previous) || !tls ||
            first.reap() != NtStatus::Success || second.reap() != NtStatus::Success ||
            process.closeAddressSpace() != NtStatus::Success) return false;
    }
    return mm::PfnDatabase::get().getActiveCount() == active;
}
