#include "micant/km/executive/ps.hpp"
#include "micant/km/executive/ps_objects.hpp"
#include "micant/km/executive/image_metadata.hpp"
#include "micant/km/executive/usermode.hpp"
#include "micant/km/core/syscalls.hpp"
#include "micant/km/hal/amd64_bootstrap.hpp"
#include "micant/um/native/pe.hpp"
#include <cstring>
#include <span>

namespace micant::sys {
uint32_t bootstrapSectionObjectCount() noexcept;
uint32_t bootstrapEventObjectCount() noexcept;
}

namespace {
constexpr std::u16string_view executablePath = u"\\??\\C:\\MicaNTTest\\fixture.exe";
constexpr std::u16string_view dllPath = u"\\??\\C:\\MicaNTTest\\ntdll.dll";
constexpr std::u16string_view tlsPath = u"\\??\\C:\\MicaNTTest\\tls.dll";
constexpr std::u16string_view commandLine = u"fixture.exe --test";
const uint8_t nativeDll[]{
#include "ntdll-process.inc"
};
}

extern "C" {
extern const uint8_t MicantTwoUserThreadsEnd[];
extern const uint8_t MicantNativeThreadStart[];
__attribute__((naked, section(".uthreads$a"))) void MicantTwoUserThreads() noexcept {
    __asm__ __volatile__(
        "movq %rsp, %r14\nmovq 0(%r14), %rbx\nmovq 8(%r14), %r12\nmovq 16(%r14), %r13\nmovq 24(%r14), %r15\n"
        "subq $0x60, %rsp\n"
        "movq %gs:0x60, %rax\ntestq %rax, %rax\njz 9f\ncmpb $0, 2(%rax)\njne 9f\n"
        "movq 24(%rax), %rax\ntestq %rax, %rax\njz 9f\ncmpq $0, 32(%rax)\nje 9f\n"
        // Static PE TLS vector slot zero points to a private template copy.
        "movq %gs:0x58, %rax\ntestq %rax, %rax\njz 9f\nmovq (%rax), %rax\ntestq %rax, %rax\njz 9f\n"
        "movabsq $0x8877665544332211, %rdx\ncmpq %rdx, (%rax)\njne 9f\ncmpq $0, 8(%rax)\njne 9f\n"
        "testq %r13, %r13\njnz 4f\nmovq $0x1234, (%rax)\njmp 5f\n"
        "4: movq $0x5678, (%rax)\n5:\n"
        "movq %gs:0x58, %rax\nmovq 8(%rax), %rax\ntestq %rax, %rax\njz 9f\n"
        "movabsq $0x1020304050607080, %rdx\ncmpq %rdx, (%rax)\njne 9f\n"
        "cmpq $0, 8(%rax)\njne 9f\ntestq %r13, %r13\njnz 11f\n"
        "movq $0xabcd, (%rax)\njmp 12f\n11: movq $0xdcba, (%rax)\n12:\n"
        "movabsq $0x1111222233334444, %rax\ntestq %r13, %r13\njz 1f\n"
        "movabsq $0xaaaabbbbccccdddd, %rax\n"
        "1: movq %rax, %gs:0x1480\nmovq 32(%r14), %rdx\ntestq %rdx, %rdx\njz 6f\n"
        // Both contexts must advance before either makes a blocking syscall.
        "movabsq $0x5a5a12345678abcd, %rdi\nxorq %r13, %rdi\nmovq %rdi, %xmm6\n"
        "7: movq %xmm6, %rax\ncmpq %rdi, %rax\njne 9f\n"
        "incq (%rdx,%r13,8)\ncmpq $0, (%rdx)\nje 7b\ncmpq $0, 8(%rdx)\nje 7b\n"
        "movabsq $0x5a5a12345678abcd, %rax\nxorq %r13, %rax\ncmpq %rdi, %rax\njne 9f\n"
        // The early diagnostic syscall path clears IF until the runtime
        // clock is active, so exercise the output probes after preemption.
        "6: movq $-2, %rcx\nleaq 49(%rsp), %rdx\nmovq %rcx, %r10\nmovl $0x52, %eax\nsyscall\n"
        "cmpl $0x80000002, %eax\njne 9f\n"
        "leaq 49(%rsp), %rcx\nxorl %edx, %edx\nxorl %r8d, %r8d\nmovq $-1, %r9\n"
        "movq %rcx, %r10\nmovl $0xb4, %eax\nsyscall\ncmpl $0x80000002, %eax\njne 9f\n"
        "testq %r13, %r13\njnz 3f\n"
        // Thread A allocates and checks a page through the native syscall.
        "movq $0, 64(%rsp)\nmovq $4096, 72(%rsp)\n"
        "movq $0x3000, 32(%rsp)\nmovq $4, 40(%rsp)\n"
        "movq $-1, %rcx\nleaq 64(%rsp), %rdx\nxorl %r8d, %r8d\n"
        "leaq 72(%rsp), %r9\ncallq *0(%r15)\n"
        "testl %eax, %eax\njnz 9f\nmovq 64(%rsp), %rax\ntestq %rax, %rax\njz 9f\n"
        "movq $0x12345678, (%rax)\ncmpq $0x12345678, (%rax)\njne 9f\n"
        // Thread A blocks on the first event and later wakes thread B.
        "movq %rbx, %rcx\nxorl %edx, %edx\nxorl %r8d, %r8d\ncallq *8(%r15)\n"
        "testl %eax, %eax\njnz 9f\nmovabsq $0x1111222233334444, %rax\n"
        "cmpq %gs:0x1480, %rax\njne 9f\n"
        "movq %gs:0x58, %rax\nmovq (%rax), %rax\ncmpq $0x1234, (%rax)\njne 9f\n"
        "movq %gs:0x58, %rax\nmovq 8(%rax), %rax\ncmpq $0xabcd, (%rax)\njne 9f\n"
        "cmpq $0, 40(%r14)\njne 10f\n"
        "movq %r12, %rcx\nxorl %edx, %edx\ncallq *16(%r15)\n"
        "testl %eax, %eax\njnz 9f\n10: movl $0xc0000022, %edx\njmp 8f\n"
        // Thread B signals A, then blocks until A signals its event.
        "3: movq %rbx, %rcx\nxorl %edx, %edx\ncallq *16(%r15)\n"
        "testl %eax, %eax\njnz 9f\n"
        "movq %r12, %rcx\nxorl %edx, %edx\nxorl %r8d, %r8d\ncallq *8(%r15)\n"
        "ud2\n"
        "9: movl $0xc000000d, %edx\n"
        "8: movq $-1, %rcx\ncallq *24(%r15)\nud2\n"
        ".globl MicantNativeThreadStart\n"
        "MicantNativeThreadStart: movq $0x12345678, (%rcx)\nmovl $0xc0000022, %eax\nretq\n");
}
__asm__(".section .uthreads$z,\"xr\"\n.globl MicantTwoUserThreadsEnd\n"
        "MicantTwoUserThreadsEnd:\n.text\n");
}

namespace {
__attribute__((noinline)) bool testNativePeerThread(micant::Handle process, uintptr_t entry) noexcept {
    using namespace micant;
    Handle peer{};
    ClientId client{};
    uintptr_t teb{};
    size_t clientLength{}, tebLength{};
    struct Attributes { size_t totalLength; PsAttribute entries[2]; } attrs{
        sizeof(Attributes), {{PS_ATTRIBUTE_CLIENT_ID, sizeof(client), reinterpret_cast<uintptr_t>(&client), &clientLength},
                             {PS_ATTRIBUTE_TEB_ADDRESS, sizeof(teb), reinterpret_cast<uintptr_t>(&teb), &tebLength}}};
    Handle restricted{};
    if (ob::detail::currentHandleTable().duplicateHandle(process, ob::detail::currentHandleTable(),
            restricted, ob::SYNCHRONIZE) != NtStatus::Success ||
        sys::NtCreateThreadEx(&peer, ob::SYNCHRONIZE, nullptr, restricted,
            entry, 0, 0, 0, mm::PageSize4KB, mm::PageSize4KB, nullptr) != NtStatus::AccessDenied ||
        sys::NtClose(restricted) != NtStatus::Success || peer ||
        sys::NtCreateThreadEx(&peer, ob::SYNCHRONIZE, nullptr, process,
            1, 0, 0, 0, mm::PageSize4KB, mm::PageSize4KB, nullptr) != NtStatus::BadInitialPc || peer)
        return false;
    if (sys::NtCreateThreadEx(&peer, ob::SYNCHRONIZE | 2, nullptr, process,
            entry, 0x1234, 1, 0, mm::PageSize4KB, mm::PageSize4KB, &attrs) != NtStatus::Success)
        return false;
    ps::EThread* object{};
    ob::ObjectHeader* reference{};
    uint32_t count{};
    if (ps::detail::referenceThreadHandle(peer, ob::SYNCHRONIZE, object, reference) != NtStatus::Success ||
        !object || object->kernelThread()->state != ke::ThreadState::Initialized ||
        client.uniqueThread != object->kernelThread()->tid || !client.uniqueProcess ||
        teb != object->userTebAddress() || clientLength != sizeof(client) || tebLength != sizeof(teb) ||
        sys::NtResumeThread(peer, &count) != NtStatus::Success || count != 1 ||
        sys::NtResumeThread(peer, &count) != NtStatus::Success || count != 0 ||
        object->cancelBeforeStart() != NtStatus::Success || object->reap() != NtStatus::Success)
        return false;
    ob::ObDereferenceObject(reference);
    return sys::NtClose(peer) == NtStatus::Success;
}

__attribute__((noinline)) bool testClonedImageLifetime(micant::Handle process) noexcept {
    using namespace micant;
    ob::detail::HandleTableGuard interruptGuard;
    ps::EProcess* child{};
    ob::ObjectHeader* reference{};
    if (ps::detail::referenceProcessHandle(process, ob::SYNCHRONIZE, child, reference) != NtStatus::Success ||
        !child || !child->nativeImagesFinalized() || !child->nativeImages()) return false;
    uintptr_t original{};
    __asm__ __volatile__("movq %%cr3, %0" : "=r"(original));
    if (!hal::amd64::switchProcessPagingRootByPhysical(child->getDirectoryTableBase())) return false;
    uint64_t thunk{};
    const bool valid = child->userPebAddress() &&
        hal::amd64::bootstrapUserRange(child->userPebAddress(), 1, false) &&
        child->nativeImages()->resolve("ntdll.dll", "RtlUserThreadStart", 0, thunk) == NtStatus::Success &&
        hal::amd64::bootstrapUserRange(thunk, 1, false, true);
    const auto* image = child->nativeImages()->module(0);
    const uintptr_t entry = image ? image->base + image->view.headers().optionalHeader.addressOfEntryPoint : 0;
    if (!hal::amd64::switchProcessPagingRootByPhysical(original)) return false;
    Handle threadHandle{};
    if (!valid || !entry || sys::NtCreateThreadEx(&threadHandle, ob::SYNCHRONIZE, nullptr, process,
            entry, 0, 1, 0, mm::PageSize4KB, mm::PageSize4KB, nullptr) != NtStatus::Success) return false;
    ps::EThread* thread{};
    ob::ObjectHeader* threadReference{};
    if (ps::detail::referenceThreadHandle(threadHandle, ob::SYNCHRONIZE, thread, threadReference) != NtStatus::Success ||
        !thread || !hal::amd64::switchProcessPagingRootByPhysical(child->getDirectoryTableBase())) return false;
    const auto tls = thread->userTlsStorage();
    const auto* teb = reinterpret_cast<const um::Teb64*>(thread->userTebAddress());
    const auto* tlsVector = teb ? reinterpret_cast<const uint64_t*>(teb->threadLocalStoragePointer) : nullptr;
    const bool threadValid = teb && teb->processEnvironmentBlock == child->userPebAddress() && tls &&
        *reinterpret_cast<const uint64_t*>(tls) == 0x8877665544332211ULL && tlsVector && tlsVector[1] &&
        *reinterpret_cast<const uint64_t*>(tlsVector[1]) == 0x1020304050607080ULL;
    if (!hal::amd64::switchProcessPagingRootByPhysical(original)) return false;
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    const auto rolledBack = thread->rollbackBeforeStart();
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    ob::ObDereferenceObject(threadReference);
    ob::ObDereferenceObject(reference);
    if (!threadValid || rolledBack != NtStatus::Success || sys::NtClose(threadHandle) != NtStatus::Success ||
        sys::NtTerminateProcess(process, NtStatus::Success) != NtStatus::Success) return false;
    // Once the runtime clock is active process teardown is deferred to the
    // reaper. Wait on the process object before checking the PFN baseline.
    if (hal::amd64::bootstrapSchedulerClockActive()) {
        auto* current = ke::KeGetCurrentThread();
        if (!current || !hal::amd64::switchProcessPagingRootByPhysical(current->directoryTableBase)) return false;
        if (ke::g_CurrentCpu.serviceDeferredCleanup) ke::g_CurrentCpu.serviceDeferredCleanup();
        const auto waited = sys::NtWaitForSingleObject(process, false, nullptr);
        if (!hal::amd64::switchProcessPagingRootByPhysical(original) || waited != NtStatus::Success) return false;
    }
    return sys::NtClose(process) == NtStatus::Success;
}

alignas(16) uint8_t launcherStack[16384]{};
void launch(void* argument) noexcept {
    using namespace micant::ke;
    auto* boot = static_cast<KThread*>(argument);
    auto* current = KeGetCurrentThread();
    KeLowerIrql(PASSIVE_LEVEL);
    if (!PriorityScheduler::get().readyKThread(boot)) __builtin_trap();
    current->state = ThreadState::Terminated;
    KfRaiseIrql(DISPATCH_LEVEL);
    auto* next = PriorityScheduler::get().selectNextThread();
    if (!next || !KiSwapContext(current, next)) __builtin_trap();
    __builtin_trap();
}

__attribute__((noinline)) bool testNativeThreadExecution(micant::Handle processHandle,
    micant::Handle initialHandle, uintptr_t imageEntry, uintptr_t bootRoot, uintptr_t enclosingRoot) noexcept {
    using namespace micant;
    using namespace micant::ke;
    ob::detail::HandleTableGuard interruptGuard;
    auto* boot = KeGetCurrentThread();
    ps::EProcess* process{};
    ob::ObjectHeader* processReference{};
    ps::EThread* initial{};
    ob::ObjectHeader* initialReference{};
    if (!boot || ps::detail::referenceProcessHandle(processHandle, 0, process, processReference) != NtStatus::Success ||
        ps::detail::referenceThreadHandle(initialHandle, 0, initial, initialReference) != NtStatus::Success ||
        !process || !initial || !initial->kernelThread()) return false;
    if (!hal::amd64::switchProcessPagingRootByPhysical(process->getDirectoryTableBase())) return false;
    uintptr_t resultAddress{};
    const auto allocated = process->getAddressSpace().allocate(resultAddress, mm::PageSize4KB,
        mm::MEM_RESERVE | mm::MEM_COMMIT, mm::PAGE_READWRITE);
    if (!hal::amd64::switchProcessPagingRootByPhysical(enclosingRoot) || allocated != NtStatus::Success) return false;
    const uintptr_t offset = reinterpret_cast<uintptr_t>(MicantNativeThreadStart) -
                             reinterpret_cast<uintptr_t>(MicantTwoUserThreads);
    Handle peer{};
    if (sys::NtCreateThreadEx(&peer, ob::SYNCHRONIZE | 2, nullptr, processHandle, imageEntry + offset,
            resultAddress, 1, 0, mm::PageSize4KB, mm::PageSize4KB, nullptr) != NtStatus::Success) return false;
    ps::EThread* thread{};
    ob::ObjectHeader* reference{};
    if (ps::detail::referenceThreadHandle(peer, ob::SYNCHRONIZE, thread, reference) != NtStatus::Success ||
        !thread || !thread->kernelThread()) return false;
    auto& scheduler = PriorityScheduler::get();
    auto* first = initial->kernelThread();
    const auto firstPriority = first->priority;
    if (!scheduler.removeReadyThread(first)) return false;
    first->state = ThreadState::Ready;
    first->priority = PRIORITY_IDLE;
    if (!scheduler.readyKThread(first)) return false;
    thread->kernelThread()->priority = boot->priority + 1;
    uint32_t count{};
    if (sys::NtResumeThread(peer, &count) != NtStatus::Success || count != 1 ||
        !hal::amd64::switchProcessPagingRootByPhysical(bootRoot)) return false;
    KThread launcher;
    launcher.directoryTableBase = bootRoot;
    if (!detail::prepareBootstrapThread(launcher, launcherStack, sizeof(launcherStack), launch, boot)) return false;
    const auto old = KfRaiseIrql(DISPATCH_LEVEL);
    const bool switched = KiSwapContext(boot, &launcher);
    KeLowerIrql(old);
    LargeInteger poll{};
    const bool terminated = switched && thread->exitStatus() == NtStatus::AccessDenied &&
        sys::NtWaitForSingleObject(peer, false, &poll) == NtStatus::Success;
    if (!scheduler.removeReadyThread(first)) return false;
    first->state = ThreadState::Ready;
    first->priority = firstPriority;
    if (!scheduler.readyKThread(first) || !hal::amd64::switchProcessPagingRootByPhysical(process->getDirectoryTableBase()))
        return false;
    const bool called = *reinterpret_cast<const uint64_t*>(resultAddress) == 0x12345678;
    const auto freed = process->getAddressSpace().free(resultAddress, 0, mm::MEM_RELEASE);
    if (!hal::amd64::switchProcessPagingRootByPhysical(enclosingRoot)) return false;
    const auto reaped = thread->kernelThread() ? thread->reap() : NtStatus::Success;
    ob::ObDereferenceObject(reference);
    ob::ObDereferenceObject(initialReference);
    ob::ObDereferenceObject(processReference);
    return terminated && called && freed == NtStatus::Success && reaped == NtStatus::Success &&
        sys::NtClose(peer) == NtStatus::Success;
}
}

static bool runUserThreads(bool preemptive, size_t threadCount) noexcept {
    using namespace micant;
    using namespace micant::ke;
    auto& scheduler = PriorityScheduler::get();
    auto* boot = KeGetCurrentThread();
    if (!boot || scheduler.getReadyThreadCount() || threadCount < 2 || threadCount > 3 ||
        (preemptive && threadCount != 2)) return false;
    const size_t active = mm::PfnDatabase::get().getActiveCount();
    const uint32_t sectionsBefore = sys::bootstrapSectionObjectCount();
    const uint32_t eventsBefore = sys::bootstrapEventObjectCount();
    uintptr_t previous{};
    __asm__ __volatile__("movq %%cr3, %0" : "=r"(previous));
    {
        auto processStorage = std::unique_ptr<ps::EProcess>(new (std::nothrow_t{}) ps::EProcess{});
        if (!processStorage) return false;
        auto& process = *processStorage;
        ps::EThread first;
        ps::EThread second;
        ps::EThread third;
        if (process.initializeAddressSpace() != NtStatus::Success) return false;
        Handle sourceFirst{}, sourceSecond{}, eventFirst{}, eventSecond{};
        constexpr uint32_t access = ob::SYNCHRONIZE | 2;
        auto& source = ob::detail::currentHandleTable();
        if (sys::NtCreateEvent(&sourceFirst, access, nullptr, 1, false) != NtStatus::Success ||
            sys::NtCreateEvent(&sourceSecond, access, nullptr, 0, false) != NtStatus::Success ||
            source.duplicateHandle(sourceFirst, process.getHandleTable(), eventFirst) != NtStatus::Success ||
            source.duplicateHandle(sourceSecond, process.getHandleTable(), eventSecond) != NtStatus::Success ||
            sys::NtClose(sourceFirst) != NtStatus::Success || sys::NtClose(sourceSecond) != NtStatus::Success ||
            !hal::amd64::switchProcessPagingRootByPhysical(process.getDirectoryTableBase())) return false;
        const uintptr_t code = hal::amd64::BootstrapManagedBase;
        const auto* program = reinterpret_cast<const uint8_t*>(MicantTwoUserThreads);
        const size_t programSize = reinterpret_cast<uintptr_t>(MicantTwoUserThreadsEnd) - reinterpret_cast<uintptr_t>(program);
        if (!programSize || programSize > mm::PageSize4KB) return false;
        static constinit uint8_t file[512 + 2 * mm::PageSize4KB]{};
        pe::ImageDosHeader dos{};
        dos.e_magic = pe::DOS_MAGIC;
        dos.e_lfanew = 64;
        std::memcpy(file, &dos, sizeof(dos));
        static constinit pe::ImageNtHeaders64 nt{};
        nt = {};
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
        nt.optionalHeader.dataDirectory[pe::IMAGE_DIRECTORY_ENTRY_IMPORT] =
            {2 * mm::PageSize4KB + 0x80, 2 * sizeof(pe::ImageImportDescriptor)};
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
        constexpr uint32_t uninitializedTlsIndex = 0xdeadbeef;
        std::memcpy(file + 512 + mm::PageSize4KB + 0x50, &uninitializedTlsIndex, sizeof(uninitializedTlsIndex));
        auto* rawData = file + 512 + mm::PageSize4KB;
        pe::ImageImportDescriptor imports{};
        imports.originalFirstThunk = 2 * mm::PageSize4KB + 0xe0;
        imports.name = 2 * mm::PageSize4KB + 0xc0;
        imports.firstThunk = 2 * mm::PageSize4KB + 0x120;
        std::memcpy(rawData + 0x80, &imports, sizeof(imports));
        constexpr char dllName[] = "ntdll.dll";
        std::memcpy(rawData + 0xc0, dllName, sizeof(dllName));
        constexpr const char* names[]{"NtAllocateVirtualMemory", "NtWaitForSingleObject", "NtSetEvent", "NtTerminateProcess"};
        for (size_t i = 0; i < 4; ++i) {
            const uint64_t rva = 2 * mm::PageSize4KB + 0x200 + i * 0x40;
            std::memcpy(rawData + 0xe0 + i * 8, &rva, sizeof(rva));
            std::memcpy(rawData + 0x120 + i * 8, &rva, sizeof(rva));
            std::memcpy(rawData + 0x202 + i * 0x40, names[i], std::strlen(names[i]) + 1);
        }
        const size_t beforeImage = mm::PfnDatabase::get().getActiveCount();
        static constexpr uint8_t invalidImage[]{0};
        if (process.loadNativeImage("bad.exe", {}) != NtStatus::InvalidImageFormat ||
            process.loadNativeImage("bad.exe", invalidImage) != NtStatus::InvalidImageFormat ||
            process.nativeImages() ||
            mm::PfnDatabase::get().getActiveCount() != beforeImage ||
            process.loadNativeImage("fixture.exe", std::span<const uint8_t>{file}, executablePath) != NtStatus::Success)
            return false;
        const auto* images = process.nativeImages();
        const auto* image = images ? images->module(0) : nullptr;
        const auto* imageVad = process.getAddressSpace().findVad(code);
        if (!image || !imageVad || !imageVad->isSectionView || !imageVad->isImageView) return false;
        const uint8_t originalMagic = file[0];
        file[0] = 0;
        const bool ownsFile = image->view.fileBytes()[0] == originalMagic;
        file[0] = originalMagic;
        if (!ownsFile) return false;
        pe::PeImageView64::TlsLayout tlsLayout{};
        if (image->view.tlsLayout(std::span<const uint8_t>{reinterpret_cast<const uint8_t*>(code), 3 * mm::PageSize4KB},
                            code, tlsLayout) != NtStatus::Success || !tlsLayout.present || tlsLayout.callbackCount ||
            tlsLayout.indexRva != 2 * mm::PageSize4KB + 0x50) return false;
        const auto unboundFirst = *reinterpret_cast<uint64_t*>(code + imports.firstThunk);
        if (process.finalizeNativeImages() != NtStatus::NoSuchFile ||
            *reinterpret_cast<uint64_t*>(code + imports.firstThunk) != unboundFirst) return false;
        pe::PeImageView64 dll;
        if (dll.inspect(std::span<const uint8_t>{nativeDll}) != NtStatus::Success) return false;
        const size_t beforeDuplicate = mm::PfnDatabase::get().getActiveCount();
        if (process.loadNativeImage("fixture.exe", std::span<const uint8_t>{nativeDll}) != NtStatus::ObjectNameCollision ||
            mm::PfnDatabase::get().getActiveCount() != beforeDuplicate) return false;
        text.characteristics |= pe::IMAGE_SCN_MEM_WRITE;
        std::memcpy(file + 64 + sizeof(nt), &text, sizeof(text));
        const auto wxStatus = process.loadNativeImage("wx.exe", std::span<const uint8_t>{file});
        text.characteristics &= ~pe::IMAGE_SCN_MEM_WRITE;
        std::memcpy(file + 64 + sizeof(nt), &text, sizeof(text));
        if (wxStatus != NtStatus::NotSupported || mm::PfnDatabase::get().getActiveCount() != beforeDuplicate)
            return false;
        if (process.loadNativeImage("ntdll.dll", std::span<const uint8_t>{nativeDll}, dllPath) != NtStatus::Success)
            return false;
        static constinit uint8_t tlsDllFile[1024]{};
        constexpr uintptr_t tlsDllBase = hal::amd64::BootstrapManagedBase + 0xe00000;
        static constinit pe::ImageNtHeaders64 tlsNt{};
        tlsNt = {};
        tlsNt.signature = pe::NT_SIGNATURE;
        tlsNt.fileHeader.machine = pe::MACHINE_AMD64;
        tlsNt.fileHeader.numberOfSections = 1;
        tlsNt.fileHeader.sizeOfOptionalHeader = sizeof(pe::ImageOptionalHeader64);
        tlsNt.fileHeader.characteristics = pe::IMAGE_FILE_EXECUTABLE_IMAGE | 0x2000;
        tlsNt.optionalHeader.magic = pe::PE32PLUS_MAGIC;
        tlsNt.optionalHeader.imageBase = tlsDllBase;
        tlsNt.optionalHeader.sectionAlignment = mm::PageSize4KB;
        tlsNt.optionalHeader.fileAlignment = 512;
        tlsNt.optionalHeader.sizeOfImage = 2 * mm::PageSize4KB;
        tlsNt.optionalHeader.sizeOfHeaders = 512;
        tlsNt.optionalHeader.numberOfRvaAndSizes = 16;
        tlsNt.optionalHeader.dataDirectory[pe::IMAGE_DIRECTORY_ENTRY_TLS] =
            {mm::PageSize4KB, sizeof(pe::ImageTlsDirectory64)};
        std::memcpy(tlsDllFile, &dos, sizeof(dos));
        std::memcpy(tlsDllFile + 64, &tlsNt, sizeof(tlsNt));
        pe::ImageSectionHeader tlsData{};
        tlsData.virtualAddress = mm::PageSize4KB;
        tlsData.misc.virtualSize = mm::PageSize4KB;
        tlsData.pointerToRawData = 512;
        tlsData.sizeOfRawData = 512;
        tlsData.characteristics = pe::IMAGE_SCN_MEM_READ | pe::IMAGE_SCN_MEM_WRITE;
        std::memcpy(tlsDllFile + 64 + sizeof(tlsNt), &tlsData, sizeof(tlsData));
        pe::ImageTlsDirectory64 secondTls{};
        secondTls.startAddressOfRawData = tlsDllBase + mm::PageSize4KB + 0x80;
        secondTls.endAddressOfRawData = secondTls.startAddressOfRawData + 8;
        secondTls.addressOfIndex = tlsDllBase + mm::PageSize4KB + 0x90;
        secondTls.sizeOfZeroFill = 8;
        secondTls.characteristics = 5u << 20;
        std::memcpy(tlsDllFile + 512, &secondTls, sizeof(secondTls));
        constexpr uint64_t secondSeed = 0x1020304050607080ULL;
        std::memcpy(tlsDllFile + 512 + 0x80, &secondSeed, sizeof(secondSeed));
        std::memcpy(tlsDllFile + 512 + 0x90, &uninitializedTlsIndex, sizeof(uninitializedTlsIndex));
        if (process.loadNativeImage("tls.dll", std::span<const uint8_t>{tlsDllFile}, tlsPath) != NtStatus::Success)
            return false;
        if (process.setNativeProcessParameters(executablePath, commandLine) != NtStatus::Success) return false;
        auto* loadedTls = reinterpret_cast<pe::ImageTlsDirectory64*>(code + 2 * mm::PageSize4KB);
        auto* callbacks = reinterpret_cast<uint64_t*>(code + 2 * mm::PageSize4KB + 0x68);
        loadedTls->addressOfCallBacks = reinterpret_cast<uintptr_t>(callbacks);
        callbacks[0] = code + mm::PageSize4KB;
        callbacks[1] = 0;
        const auto callbackStatus = process.finalizeNativeImages();
        loadedTls->addressOfCallBacks = 0;
        callbacks[0] = 0;
        if (callbackStatus != NtStatus::NotImplemented ||
            *reinterpret_cast<uint32_t*>(code + tlsLayout.indexRva) != uninitializedTlsIndex ||
            *reinterpret_cast<uint32_t*>(tlsDllBase + mm::PageSize4KB + 0x90) != uninitializedTlsIndex ||
            process.finalizeNativeImages() != NtStatus::Success ||
            process.userTlsLayouts().size() != 2 || *reinterpret_cast<uint32_t*>(code + tlsLayout.indexRva) != 0 ||
            *reinterpret_cast<uint32_t*>(tlsDllBase + mm::PageSize4KB + 0x90) != 1 ||
            sys::bootstrapSectionObjectCount() != sectionsBefore + 3 ||
            hal::amd64::bootstrapUserRange(code, 1, true) ||
            hal::amd64::bootstrapUserRange(code + mm::PageSize4KB, 1, true) ||
            !hal::amd64::bootstrapUserRange(code + mm::PageSize4KB, 1, false, true) ||
            !hal::amd64::bootstrapUserRange(code + 2 * mm::PageSize4KB, 1, true) ||
            process.loadNativeImage("late.dll", std::span<const uint8_t>{nativeDll}) != NtStatus::InvalidParameter)
            return false;
        uintptr_t counters{};
        if (preemptive && process.getAddressSpace().allocate(counters, mm::PageSize4KB,
                mm::MEM_RESERVE | mm::MEM_COMMIT, mm::PAGE_READWRITE) != NtStatus::Success) return false;
        for (size_t i = 0; i < 4; ++i) {
            uint64_t address{};
            if (images->resolve("ntdll.dll", names[i], 0, address) != NtStatus::Success ||
                *reinterpret_cast<uint64_t*>(code + imports.firstThunk + i * 8) != address) return false;
        }
        const uintptr_t entry = code + nt.optionalHeader.addressOfEntryPoint;
        auto invalidTls = tlsLayout;
        invalidTls.callbackCount = 1;
        const size_t beforeThread = mm::PfnDatabase::get().getActiveCount();
        if (first.initializeUser(process, entry, 32768, mm::PageSize4KB, &invalidTls) != NtStatus::NotImplemented ||
            mm::PfnDatabase::get().getActiveCount() != beforeThread) return false;
        invalidTls = tlsLayout;
        invalidTls.alignment = 3; // Fails after user pages were allocated; reap must roll them back.
        if (first.initializeUser(process, entry, 32768, mm::PageSize4KB, &invalidTls) != NtStatus::InvalidParameter ||
            first.kernelThread() || mm::PfnDatabase::get().getActiveCount() != beforeThread) return false;
        // Compose a separate process and initial user thread through the
        // private Ps factory, then roll it back before its entry executes.
        Handle createdProcess{}, createdThread{};
        const ps::detail::BootstrapProcessImage processImages[]{
            {"fixture.exe", std::span<const uint8_t>{file}, executablePath},
            {"ntdll.dll", std::span<const uint8_t>{nativeDll}, dllPath},
            {"tls.dll", std::span<const uint8_t>{tlsDllFile}, tlsPath},
        };
        if (ps::detail::createBootstrapUserProcess({}, executablePath, commandLine, 32768,
                mm::PageSize4KB, preemptive, ob::SYNCHRONIZE, ob::SYNCHRONIZE,
                createdProcess, createdThread) != NtStatus::InvalidParameter || createdProcess || createdThread ||
            ps::detail::createBootstrapUserProcess(processImages, executablePath, commandLine, 32768,
                mm::PageSize4KB, preemptive, ob::SYNCHRONIZE | 2 | 0x80, ob::SYNCHRONIZE,
                createdProcess, createdThread) != NtStatus::Success) return false;
        if (!testNativePeerThread(createdProcess, entry)) return false;
        if (!testNativeThreadExecution(createdProcess, createdThread, entry, previous,
                                       process.getDirectoryTableBase())) return false;
        Handle clonedProcess{};
        if (sys::NtCreateProcessEx(&clonedProcess, ob::SYNCHRONIZE | 1 | 2, nullptr, createdProcess,
                                   0, 0, 0, 0, false) != NtStatus::Success) return false;
        ps::EThread* createdThreadObject{};
        ob::ObjectHeader* createdThreadRef{};
        if (ps::detail::referenceThreadHandle(createdThread, ob::SYNCHRONIZE,
                createdThreadObject, createdThreadRef) != NtStatus::Success || !createdThreadObject ||
            createdThreadObject->cancelBeforeStart() != NtStatus::Success ||
            createdThreadObject->reap() != NtStatus::Success) return false;
        ob::ObDereferenceObject(createdThreadRef);
        if (sys::NtClose(createdThread) != NtStatus::Success) return false;
        if (sys::NtClose(createdProcess) != NtStatus::Success) return false;
        if (!testClonedImageLifetime(clonedProcess)) return false;
        if (mm::PfnDatabase::get().getActiveCount() != beforeThread) return false;
        // Exercise Ob ownership for a user context before the real workers.
        Handle objectHandle{};
        ps::EThread* objectThread{};
        if (ps::detail::createUserThreadHandle(process, entry, 32768, mm::PageSize4KB,
                                               preemptive, ob::SYNCHRONIZE,
                                               objectHandle, objectThread) != NtStatus::Success ||
            !objectThread || objectThread->cancelBeforeStart() != NtStatus::Success)
            return false;
        LargeInteger objectPoll{};
        if (sys::NtWaitForSingleObject(objectHandle, false, &objectPoll) != NtStatus::Success ||
            objectThread->reap() != NtStatus::Success || sys::NtClose(objectHandle) != NtStatus::Success ||
            mm::PfnDatabase::get().getActiveCount() != beforeThread) return false;
        ps::EThread* threads[]{&first, &second, &third};
        uintptr_t tebs[3]{}, tlsBlocks[3]{}, tlsSecond[3]{};
        for (size_t i = 0; i < threadCount; ++i) {
            if (threads[i]->initializeUser(process, entry, 32768, mm::PageSize4KB, preemptive) != NtStatus::Success)
                return false;
            tebs[i] = threads[i]->userTebAddress();
            const auto* teb = reinterpret_cast<const um::Teb64*>(tebs[i]);
            if (!teb || teb->uniqueProcessId != process.processId() ||
                !teb->uniqueThreadId || teb->uniqueThreadId != threads[i]->kernelThread()->tid ||
                (i && teb->uniqueThreadId == reinterpret_cast<const um::Teb64*>(tebs[0])->uniqueThreadId))
                return false;
            tlsBlocks[i] = threads[i]->userTlsStorage();
            tlsSecond[i] = reinterpret_cast<const uint64_t*>(teb->threadLocalStoragePointer)[1];
            auto* arguments = reinterpret_cast<uint64_t*>(threads[i]->userStackPointer());
            arguments[0] = eventFirst;
            arguments[1] = eventSecond;
            arguments[2] = i;
            arguments[3] = code + imports.firstThunk;
            arguments[4] = counters;
            arguments[5] = threadCount == 3 || preemptive ? 1 : 0;
        }
        if (!hal::amd64::switchProcessPagingRootByPhysical(previous)) return false;
        for (size_t i = 0; i < threadCount; ++i) {
            threads[i]->kernelThread()->priority = boot->priority + 1;
            if (!threads[i]->kernelThread()->priority || threads[i]->ready() != NtStatus::Success) return false;
        }
        if (preemptive) {
            first.kernelThread()->quantumRemaining = 1;
            second.kernelThread()->quantumRemaining = 1000;
        }
        auto launcher = std::unique_ptr<KThread>(new (std::nothrow_t{}) KThread{});
        if (!launcher) return false;
        launcher->directoryTableBase = g_CurrentCpu.currentCr3;
        if (!detail::prepareBootstrapThread(*launcher, launcherStack, sizeof(launcherStack), launch, boot)) return false;
        const KIRQL oldIrql = KfRaiseIrql(DISPATCH_LEVEL);
        const bool switched = KiSwapContext(boot, launcher.get());
        KeLowerIrql(oldIrql);
        if (!switched || !process.isTerminating() || process.exitStatus() != NtStatus::AccessDenied ||
            scheduler.getReadyThreadCount() ||
            !hal::amd64::switchProcessPagingRootByPhysical(process.getDirectoryTableBase())) return false;
        bool tls = process.userPebAddress() != 0;
        const uintptr_t loaderAddress = *reinterpret_cast<const uint64_t*>(process.userPebAddress() + 0x18);
        if (!hal::amd64::bootstrapUserRange(loaderAddress, mm::PageSize4KB, false)) return false;
        const uintptr_t listHead = loaderAddress + 0x20;
        const auto* head = reinterpret_cast<const uint64_t*>(listHead);
        uintptr_t link = head[0];
        uintptr_t previousLink = listHead;
        const uintptr_t expectedBases[]{code, tlsDllBase, dll.headers().optionalHeader.imageBase};
        const std::u16string_view expectedPaths[]{executablePath, tlsPath, dllPath};
        const auto matches = [](const ps::detail::UserUnicodeString& value, std::u16string_view expected) {
            return value.length == expected.size() * sizeof(char16_t) &&
                value.maximumLength == value.length + sizeof(char16_t) &&
                hal::amd64::bootstrapUserRange(value.buffer, value.maximumLength, false) &&
                std::memcmp(reinterpret_cast<const void*>(value.buffer), expected.data(), value.length) == 0 &&
                *reinterpret_cast<const char16_t*>(value.buffer + value.length) == 0;
        };
        const auto parametersAddress = *reinterpret_cast<const uint64_t*>(process.userPebAddress() + 0x20);
        if (!hal::amd64::bootstrapUserRange(parametersAddress, sizeof(ps::detail::UserProcessParameters), false)) return false;
        const auto& parameters = *reinterpret_cast<const ps::detail::UserProcessParameters*>(parametersAddress);
        tls = tls && matches(parameters.imagePathName, executablePath) && matches(parameters.commandLine, commandLine);
        size_t pathIndex = 0;
        for (const auto expectedBase : expectedBases) {
            if (link == listHead || !hal::amd64::bootstrapUserRange(link, 0x48, false)) {
                tls = false;
                break;
            }
            const auto* entry = reinterpret_cast<const uint64_t*>(link);
            if (entry[1] != previousLink || entry[4] != expectedBase ||
                !matches(*reinterpret_cast<const ps::detail::UserUnicodeString*>(link + 0x38), expectedPaths[pathIndex++])) {
                tls = false;
                break;
            }
            previousLink = link;
            link = entry[0];
        }
        tls = tls && link == listHead && head[1] == previousLink;
        for (size_t i = 0; i < threadCount; ++i) {
            const auto* teb = reinterpret_cast<um::Teb64*>(tebs[i]);
            tls = tls && threads[i]->kernelThread()->state == ThreadState::Terminated &&
                  teb->processEnvironmentBlock == process.userPebAddress() &&
                  teb->tlsSlots[0] == (i == 0 ? 0x1111222233334444ULL : 0xaaaabbbbccccddddULL) &&
                  *reinterpret_cast<uint64_t*>(tlsBlocks[i]) == (i == 0 ? 0x1234u : 0x5678u) &&
                  *reinterpret_cast<uint64_t*>(tlsSecond[i]) == (i == 0 ? 0xabcdu : 0xdcbau);
        }
        tls = tls && (!preemptive || (reinterpret_cast<volatile uint64_t*>(counters)[0] &&
                                      reinterpret_cast<volatile uint64_t*>(counters)[1]));
        if (!hal::amd64::switchProcessPagingRootByPhysical(previous) || !tls) return false;
        for (size_t i = 0; i < threadCount; ++i)
            if (threads[i]->reap() != NtStatus::Success) return false;
        if (process.closeAddressSpace() != NtStatus::Success) return false;
    }
    return mm::PfnDatabase::get().getActiveCount() == active &&
        sys::bootstrapSectionObjectCount() == sectionsBefore &&
        sys::bootstrapEventObjectCount() == eventsBefore;
}

extern "C" bool MicantTestPsTwoUserThreads() noexcept { return runUserThreads(false, 2); }
extern "C" bool MicantTestPsThreeUserThreads() noexcept { return runUserThreads(false, 3); }
extern "C" bool MicantTestPsUserPreemption() noexcept { return runUserThreads(true, 2); }
