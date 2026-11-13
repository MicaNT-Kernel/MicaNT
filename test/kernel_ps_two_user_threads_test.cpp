#include "micant/km/executive/ps.hpp"
#include "micant/km/executive/ps_objects.hpp"
#include "micant/km/executive/image_metadata.hpp"
#include "micant/km/executive/io_file.hpp"
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

namespace micant::ex {
void dumpHeapPfnLifetimeTrace(uint64_t pfn = 0) noexcept;
size_t heapPfnLifetimeTraceCount() noexcept;
}

namespace {
void serialWrite(const char* text) noexcept {
    while (*text) {
        uint8_t ready{};
        do {
            __asm__ __volatile__("inb %1, %0" : "=a"(ready) : "Nd"(uint16_t{0x3fd}));
        } while (!(ready & 0x20));
        __asm__ __volatile__("outb %0, %1" : : "a"(static_cast<uint8_t>(*text++)), "Nd"(uint16_t{0x3f8}));
    }
}
void serialHex(uint64_t value) noexcept {
    char text[17] = "0000000000000000";
    constexpr char digits[] = "0123456789ABCDEF";
    for (size_t i = 0; i < 16; ++i) {
        text[15 - i] = digits[value & 15];
        value >>= 4;
    }
    serialWrite(text);
}
void serialNumber(uint64_t value) noexcept {
    serialHex(value);
    serialWrite("\r\n");
}
struct WatchedThreadPfnStage {
    const char* name{};
    size_t activeCount{};
    size_t heapTraceCount{};
    uint8_t presentMask{};
    bool complete{};
};
WatchedThreadPfnStage watchedThreadPfnStages[32]{};
size_t watchedThreadPfnStageCount{};
uintptr_t trackedCloneNativeImageSet{};
void resetWatchedThreadPfnTrace() noexcept {
    watchedThreadPfnStageCount = 0;
}
void traceWatchedThreadPfns(const char* stage) noexcept {
    constexpr uint64_t watchedPfns[]{0x1A8, 0x1C9, 0x262, 0x2F6};
    static micant::mm::Mmpfn entries[1024]{};
    const auto count = micant::mm::PfnDatabase::get().snapshotActiveEntries(entries);
    if (watchedThreadPfnStageCount == 32) return;
    auto& snapshot = watchedThreadPfnStages[watchedThreadPfnStageCount++];
    snapshot.name = stage;
    snapshot.activeCount = count;
    snapshot.heapTraceCount = micant::ex::heapPfnLifetimeTraceCount();
    snapshot.complete = count <= 1024;
    if (snapshot.complete) {
        for (size_t watchedIndex = 0; watchedIndex < sizeof(watchedPfns) / sizeof(watchedPfns[0]); ++watchedIndex) {
            for (size_t i = 0; i < count; ++i) {
                if (entries[i].pfn == watchedPfns[watchedIndex]) {
                    snapshot.presentMask |= static_cast<uint8_t>(1u << watchedIndex);
                    break;
                }
            }
        }
    }
}
void printWatchedThreadPfnTrace() noexcept {
    for (size_t i = 0; i < watchedThreadPfnStageCount; ++i) {
        const auto& snapshot = watchedThreadPfnStages[i];
        serialWrite("[MicaNT DBG] PFN trace stage/count/mask/complete=");
        serialWrite(snapshot.name);
        serialWrite("/");
        serialHex(snapshot.activeCount);
        serialWrite("/");
        serialHex(snapshot.presentMask);
        serialWrite("/");
        serialHex(snapshot.complete);
        serialWrite("/");
        serialHex(snapshot.heapTraceCount);
        serialWrite("\r\n");
    }
}
void printWatchedPfnPresence(size_t index, uint64_t pfn) noexcept {
    for (size_t i = 0; i < watchedThreadPfnStageCount; ++i) {
        const auto& snapshot = watchedThreadPfnStages[i];
        serialWrite("[MicaNT DBG] PFN presence/pfn/stage/count=");
        serialHex((snapshot.presentMask >> index) & 1);
        serialWrite("/");
        serialHex(pfn);
        serialWrite("/");
        serialWrite(snapshot.name);
        serialWrite("/");
        serialHex(snapshot.activeCount);
        serialWrite("/");
        serialHex(snapshot.heapTraceCount);
        serialWrite("\r\n");
    }
}
bool drainDeferredDeletes(uintptr_t kernelRoot, uintptr_t processRoot) noexcept {
    if (!micant::hal::amd64::switchProcessPagingRootByPhysical(kernelRoot)) return false;
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; sti" : "=r"(flags) : : "memory");
    const uint64_t start = micant::hal::amd64::bootstrapSchedulerClockTicks();
    for (size_t spin = 0; spin < 50000000 && micant::io::IoDeferredFileDeletesPending() &&
         micant::hal::amd64::bootstrapSchedulerClockTicks() - start < 100; ++spin)
        __asm__ __volatile__("pause");
    __asm__ __volatile__("cli" : : : "memory");
    const bool drained = !micant::io::IoDeferredFileDeletesPending();
    const bool restored = micant::hal::amd64::switchProcessPagingRootByPhysical(processRoot);
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    return drained && restored;
}
bool waitForPfnBaselineAndDeferredDeletes(uintptr_t kernelRoot, uintptr_t processRoot,
                                          std::span<const micant::mm::Mmpfn> baseline,
                                          std::span<micant::mm::Mmpfn> observed) noexcept {
    if (baseline.size() > observed.size()) return false;
    auto* current = micant::ke::KeGetCurrentThread();
    if (!current) return false;
    if (!micant::hal::amd64::switchProcessPagingRootByPhysical(kernelRoot)) return false;
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    if (micant::ke::g_CurrentCpu.serviceDeferredCleanup)
        micant::ke::g_CurrentCpu.serviceDeferredCleanup();
    const auto originalPriority = current->priority;
    // Let the normal-priority deferred reaper run while this bootstrap test waits.
    current->priority = micant::ke::PRIORITY_IDLE;
    if (!(flags & (uint64_t{1} << 9)))
        serialWrite("[MicaNT DBG] PFN wait enabling previously masked interrupts\r\n");
    __asm__ __volatile__("sti" : : : "memory");
    const uint64_t start = micant::hal::amd64::bootstrapSchedulerClockTicks();
    uint64_t lastSnapshotTick = UINT64_MAX;
    bool drained = false;
    for (size_t spin = 0; spin < 50000000 &&
         micant::hal::amd64::bootstrapSchedulerClockTicks() - start < 100; ++spin) {
        const auto tick = micant::hal::amd64::bootstrapSchedulerClockTicks();
        if (tick != lastSnapshotTick) {
            lastSnapshotTick = tick;
            const auto count = micant::mm::PfnDatabase::get().snapshotActiveEntries(observed);
            bool matchesBaseline = count == baseline.size() && count <= observed.size();
            for (size_t i = 0; matchesBaseline && i < baseline.size(); ++i) {
                bool found = false;
                for (size_t j = 0; j < count; ++j) {
                    if (observed[j].pfn == baseline[i].pfn) {
                        found = true;
                        break;
                    }
                }
                matchesBaseline = found;
            }
            if (matchesBaseline && !micant::io::IoDeferredFileDeletesPending() &&
                !micant::ps::detail::deferredCleanupPending()) {
                drained = true;
                break;
            }
        }
        __asm__ __volatile__("pause");
    }
    __asm__ __volatile__("cli" : : : "memory");
    const auto observedCount = drained ? baseline.size() : micant::mm::PfnDatabase::get().snapshotActiveEntries(observed);
    const auto elapsedTicks = micant::hal::amd64::bootstrapSchedulerClockTicks() - start;
    const bool pendingFileDeletes = micant::io::IoDeferredFileDeletesPending();
    current->priority = originalPriority;
    const bool restored = micant::hal::amd64::switchProcessPagingRootByPhysical(processRoot);
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    if (!drained) {
        serialWrite("[MicaNT DBG] cleanup wait timeout ticks/priority/PFN/baseline/Io=");
        serialNumber(elapsedTicks);
        serialNumber(originalPriority);
        serialNumber(observedCount);
        serialNumber(baseline.size());
        serialNumber(pendingFileDeletes);
        size_t reported = 0;
        if (observedCount <= observed.size()) {
            for (size_t i = 0; i < observedCount && reported < 16; ++i) {
                bool found = false;
                for (const auto& entry : baseline) {
                    if (entry.pfn == observed[i].pfn) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    serialWrite("[MicaNT DBG] cleanup-only PFN/ref/PTE=");
                    serialNumber(observed[i].pfn);
                    serialNumber(observed[i].referenceCount);
                    serialNumber(observed[i].pteAddress);
                    micant::ex::dumpHeapPfnLifetimeTrace(observed[i].pfn);
                    ++reported;
                }
            }
            for (const auto& entry : baseline) {
                if (reported == 16) break;
                bool found = false;
                for (size_t i = 0; i < observedCount; ++i) {
                    if (observed[i].pfn == entry.pfn) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    serialWrite("[MicaNT DBG] cleanup-missing PFN/ref/PTE=");
                    serialNumber(entry.pfn);
                    serialNumber(entry.referenceCount);
                    serialNumber(entry.pteAddress);
                    micant::ex::dumpHeapPfnLifetimeTrace(entry.pfn);
                    ++reported;
                }
            }
        } else {
            serialWrite("[MicaNT DBG] cleanup PFN snapshot truncated\r\n");
        }
        printWatchedPfnPresence(0, 0x1A8);
        for (const auto& entry : baseline) {
            if (entry.pfn != 0x1A8) continue;
            serialWrite("[MicaNT DBG] baseline PFN 0x1A8 ref/PTE=");
            serialHex(entry.referenceCount);
            serialHex(entry.pteAddress);
            serialWrite("\r\n");
            break;
        }
        serialWrite("[MicaNT DBG] clone native image set at PFN wait=");
        serialHex(trackedCloneNativeImageSet);
        serialWrite("\r\n");
        micant::ex::dumpHeapPfnLifetimeTrace();
    }
    return drained && restored;
}

struct ProcessAddressSpaceCleanup {
    micant::ps::EProcess& process;
    uintptr_t kernelRoot;
    bool active{true};

    ~ProcessAddressSpaceCleanup() {
        if (!active || !process.getDirectoryTableBase()) return;
        if (!micant::hal::amd64::switchProcessPagingRootByPhysical(kernelRoot)) {
            serialWrite("[MicaNT DBG] test process cleanup could not switch to kernel root\r\n");
            return;
        }
        const auto status = process.closeAddressSpace();
        if (status != micant::NtStatus::Success) {
            serialWrite("[MicaNT DBG] test process cleanup status=");
            serialNumber(static_cast<uint32_t>(status));
        }
    }
};
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
extern const uint8_t MicantGrowStackThreadStart[];
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
        "MicantNativeThreadStart: movq $0x12345678, (%rcx)\nmovl $0xc0000022, %eax\nretq\n"
        ".globl MicantGrowStackThreadStart\n"
        "MicantGrowStackThreadStart: movabsq $0x5a5a12345678abcd, %rax\nmovq %rax, %xmm0\n"
        // Probe downward page by page through the initial commit and across
        // multiple guard pages. A single 8 KiB frame remains inside the
        // guaranteed exception-dispatch commit and does not exercise growth.
        "movl $10, %r8d\n1: subq $4096, %rsp\nmovb %r8b, (%rsp)\n"
        "decl %r8d\njnz 1b\naddq $40960, %rsp\n"
        "movq %xmm0, %rdx\ncmpq %rax, %rdx\njne 13f\n"
        "movq %gs:0x10, %rdx\nmovq %rdx, 8(%rcx)\n"
        "movq $0x12345678, (%rcx)\nmovl $0xc0000022, %eax\nretq\n"
        "13: movl $0xc000000d, %eax\nretq\n");
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
    const auto duplicateStatus = ob::detail::currentHandleTable().duplicateHandle(process,
        ob::detail::currentHandleTable(), restricted, ob::SYNCHRONIZE);
    const auto restrictedCreateStatus = duplicateStatus == NtStatus::Success
        ? sys::NtCreateThreadEx(&peer, ob::SYNCHRONIZE, nullptr, restricted,
            entry, 0, 0, 0, mm::PageSize4KB, mm::PageSize4KB, nullptr)
        : duplicateStatus;
    const auto restrictedCloseStatus = restricted ? sys::NtClose(restricted) : NtStatus::Success;
    const auto invalidPcStatus = sys::NtCreateThreadEx(&peer, ob::SYNCHRONIZE, nullptr, process,
        1, 0, 1, 0, mm::PageSize4KB, 32768, nullptr);
    serialWrite("[MicaNT DBG] peer duplicate/restricted/close/invalid status:\r\n");
    serialNumber(static_cast<uint32_t>(duplicateStatus));
    serialNumber(static_cast<uint32_t>(restrictedCreateStatus));
    serialNumber(static_cast<uint32_t>(restrictedCloseStatus));
    serialNumber(static_cast<uint32_t>(invalidPcStatus));
    serialWrite("[MicaNT DBG] peer handle="); serialNumber(peer);
    if (duplicateStatus != NtStatus::Success || restrictedCreateStatus != NtStatus::AccessDenied ||
        restrictedCloseStatus != NtStatus::Success || invalidPcStatus != NtStatus::Success || !peer)
        return false;
    ps::EThread* invalidThread{};
    ob::ObjectHeader* invalidReference{};
    if (ps::detail::referenceThreadHandle(peer, ob::SYNCHRONIZE, invalidThread, invalidReference) != NtStatus::Success ||
        !invalidThread || invalidThread->cancelBeforeStart() != NtStatus::Success ||
        invalidThread->reap() != NtStatus::Success) return false;
    ob::ObDereferenceObject(invalidReference);
    if (sys::NtClose(peer) != NtStatus::Success) return false;
    serialWrite("[MicaNT DBG] invalid pc thread cleaned\r\n");
    peer = {};
    if (sys::NtCreateThreadEx(&peer, ob::SYNCHRONIZE | 2, nullptr, process,
            entry, 0x1234, 1, 0, mm::PageSize4KB, 32768, &attrs) != NtStatus::Success)
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
    static mm::Mmpfn baselineEntries[512]{};
    static mm::Mmpfn rollbackEntries[512]{};
    const auto initialPfnCount = mm::PfnDatabase::get().getActiveCount();
    const auto baselineEntryCount = mm::PfnDatabase::get().snapshotActiveEntries(baselineEntries);
    serialWrite("[MicaNT DBG] clone lifetime start PFN="); serialNumber(initialPfnCount);
    traceWatchedThreadPfns("clone lifetime start");
    ob::detail::HandleTableGuard interruptGuard;
    ps::EProcess* child{};
    ob::ObjectHeader* reference{};
    if (ps::detail::referenceProcessHandle(process, ob::SYNCHRONIZE, child, reference) != NtStatus::Success ||
        !child || !child->nativeImagesFinalized() || !child->nativeImages()) return false;
    trackedCloneNativeImageSet = reinterpret_cast<uintptr_t>(child->nativeImages());
    serialWrite("[MicaNT DBG] clone native image set/size=");
    serialHex(reinterpret_cast<uintptr_t>(child->nativeImages()));
    serialHex(sizeof(*child->nativeImages()));
    serialWrite("\r\n");
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
            entry, 0, 1, 0, mm::PageSize4KB, 32768, nullptr) != NtStatus::Success) return false;
    serialWrite("[MicaNT DBG] clone lifetime thread PFN="); serialNumber(mm::PfnDatabase::get().getActiveCount());
    traceWatchedThreadPfns("clone image thread creation");
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
    serialWrite("[MicaNT DBG] clone lifetime rollback PFN="); serialNumber(mm::PfnDatabase::get().getActiveCount());
    traceWatchedThreadPfns("clone image thread rollback");
    const auto rollbackEntryCount = mm::PfnDatabase::get().snapshotActiveEntries(rollbackEntries);
    if (baselineEntryCount <= 512 && rollbackEntryCount <= 512) {
        for (size_t i = 0; i < rollbackEntryCount; ++i) {
            bool existed = false;
            for (size_t j = 0; j < baselineEntryCount; ++j) {
                if (rollbackEntries[i].pfn == baselineEntries[j].pfn) {
                    existed = true;
                    break;
                }
            }
            if (!existed) {
                serialWrite("[MicaNT DBG] rollback-only PFN/ref/PTE=");
                serialNumber(rollbackEntries[i].pfn);
                serialNumber(rollbackEntries[i].referenceCount);
                serialNumber(rollbackEntries[i].pteAddress);
            }
        }
    } else {
        serialWrite("[MicaNT DBG] active PFN snapshot truncated\r\n");
    }
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    ob::ObDereferenceObject(threadReference);
    ob::ObDereferenceObject(reference);
    const auto threadCloseStatus = sys::NtClose(threadHandle);
    serialWrite("[MicaNT DBG] clone lifetime thread close PFN="); serialNumber(mm::PfnDatabase::get().getActiveCount());
    traceWatchedThreadPfns("clone image thread handle close");
    if (!threadValid || rolledBack != NtStatus::Success || threadCloseStatus != NtStatus::Success ||
        sys::NtTerminateProcess(process, NtStatus::Success) != NtStatus::Success) return false;
    traceWatchedThreadPfns("clone image process termination");
    // Once the runtime clock is active process teardown is deferred to the
    // reaper. Wait on the process object before checking the PFN baseline.
    if (hal::amd64::bootstrapSchedulerClockActive()) {
        auto* current = ke::KeGetCurrentThread();
        if (!current || !hal::amd64::switchProcessPagingRootByPhysical(current->directoryTableBase)) return false;
        if (ke::g_CurrentCpu.serviceDeferredCleanup) ke::g_CurrentCpu.serviceDeferredCleanup();
        const auto waited = sys::NtWaitForSingleObject(process, false, nullptr);
            serialWrite("[MicaNT DBG] clone lifetime wait PFN="); serialNumber(mm::PfnDatabase::get().getActiveCount());
        if (!hal::amd64::switchProcessPagingRootByPhysical(original) || waited != NtStatus::Success) return false;
        traceWatchedThreadPfns("clone image process wait");
    }
    const auto closeStatus = sys::NtClose(process);
    serialWrite("[MicaNT DBG] clone lifetime close PFN="); serialNumber(mm::PfnDatabase::get().getActiveCount());
    traceWatchedThreadPfns("clone image process handle close");
    const auto finalEntryCount = mm::PfnDatabase::get().snapshotActiveEntries(rollbackEntries);
    if (baselineEntryCount <= 512 && finalEntryCount <= 512) {
        for (size_t i = 0; i < finalEntryCount; ++i) {
            bool existed = false;
            for (size_t j = 0; j < baselineEntryCount; ++j) {
                if (rollbackEntries[i].pfn == baselineEntries[j].pfn) {
                    existed = true;
                    break;
                }
            }
            if (!existed) {
                serialWrite("[MicaNT DBG] final-only PFN/ref/PTE=");
                serialNumber(rollbackEntries[i].pfn);
                serialNumber(rollbackEntries[i].referenceCount);
                serialNumber(rollbackEntries[i].pteAddress);
            }
        }
    } else {
        serialWrite("[MicaNT DBG] final active PFN snapshot truncated\r\n");
    }
    return closeStatus == NtStatus::Success;
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
    micant::Handle initialHandle, uintptr_t imageEntry, uintptr_t bootRoot, uintptr_t enclosingRoot,
    bool growStack = false) noexcept {
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
    serialWrite("[MicaNT DBG] execution references ready\r\n");
    if (!hal::amd64::switchProcessPagingRootByPhysical(process->getDirectoryTableBase())) return false;
    uintptr_t resultAddress{};
    const auto allocated = process->getAddressSpace().allocate(resultAddress, mm::PageSize4KB,
        mm::MEM_RESERVE | mm::MEM_COMMIT, mm::PAGE_READWRITE);
    if (!hal::amd64::switchProcessPagingRootByPhysical(enclosingRoot) || allocated != NtStatus::Success) return false;
    serialWrite("[MicaNT DBG] execution result allocated\r\n");
    const uintptr_t offset = reinterpret_cast<uintptr_t>(growStack ? MicantGrowStackThreadStart : MicantNativeThreadStart) -
                             reinterpret_cast<uintptr_t>(MicantTwoUserThreads);
    Handle peer{};
    uintptr_t initialStackLimit{};
    if (growStack) {
        if (!hal::amd64::switchProcessPagingRootByPhysical(process->getDirectoryTableBase())) return false;
        uint64_t thunk{};
        auto result = process->nativeImages()->resolve("ntdll.dll", "RtlUserThreadStart", 0, thunk);
        ps::EThread* created{};
        if (result == NtStatus::Success)
            result = ps::detail::createUserThreadHandle(*process, thunk, 32768, 24 * mm::PageSize4KB,
                hal::amd64::bootstrapSchedulerClockActive(), ob::SYNCHRONIZE | 2, peer, created, mm::PageSize4KB);
        if (result == NtStatus::Success) result = created->configureUserStart(imageEntry + offset, resultAddress);
        if (result == NtStatus::Success) result = created->suspendInitially();
        if (result == NtStatus::Success)
            initialStackLimit = reinterpret_cast<const um::Teb64*>(created->userTebAddress())->stackLimit;
        if (!hal::amd64::switchProcessPagingRootByPhysical(enclosingRoot) || result != NtStatus::Success) return false;
    } else {
        const auto created = sys::NtCreateThreadEx(&peer, ob::SYNCHRONIZE | 2, nullptr, processHandle, imageEntry + offset,
               resultAddress, 1, 0, mm::PageSize4KB, 8 * mm::PageSize4KB, nullptr);
        serialWrite("[MicaNT DBG] execution create status="); serialNumber(static_cast<uint32_t>(created));
        if (created != NtStatus::Success) return false;
    }
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
    serialWrite("[MicaNT DBG] execution first reprioritized\r\n");
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
    serialWrite("[MicaNT DBG] execution returned from scheduler\r\n");
    LargeInteger poll{};
    const bool terminated = switched && thread->exitStatus() == NtStatus::AccessDenied &&
        sys::NtWaitForSingleObject(peer, false, &poll) == NtStatus::Success;
    serialWrite("[MicaNT DBG] execution exit status="); serialNumber(static_cast<uint32_t>(thread->exitStatus()));
    if (!scheduler.removeReadyThread(first)) return false;
    first->state = ThreadState::Ready;
    first->priority = firstPriority;
    if (!scheduler.readyKThread(first) || !hal::amd64::switchProcessPagingRootByPhysical(process->getDirectoryTableBase()))
        return false;
    if (growStack) {
        serialWrite("[MicaNT DBG] stack initial/current="); serialNumber(initialStackLimit);
        serialNumber(*(reinterpret_cast<const uint64_t*>(resultAddress) + 1));
        serialWrite("[MicaNT DBG] execution result="); serialNumber(*reinterpret_cast<const uint64_t*>(resultAddress));
    }
    const bool called = *reinterpret_cast<const uint64_t*>(resultAddress) == 0x12345678 &&
        (!growStack || *(reinterpret_cast<const uint64_t*>(resultAddress) + 1) ==
                           initialStackLimit - 5 * mm::PageSize4KB);
    const auto freed = process->getAddressSpace().free(resultAddress, 0, mm::MEM_RELEASE);
    if (!hal::amd64::switchProcessPagingRootByPhysical(enclosingRoot)) return false;
    const auto reaped = thread->kernelThread() ? thread->reap() : NtStatus::Success;
    serialWrite("[MicaNT DBG] execution terminated/called/freed/reaped:\r\n");
    serialNumber(terminated); serialNumber(called); serialNumber(static_cast<uint32_t>(freed));
    serialNumber(static_cast<uint32_t>(reaped));
    ob::ObDereferenceObject(reference);
    ob::ObDereferenceObject(initialReference);
    ob::ObDereferenceObject(processReference);
    return terminated && called && freed == NtStatus::Success && reaped == NtStatus::Success &&
        sys::NtClose(peer) == NtStatus::Success;
}
}

static bool drainPreviousDeferredCleanup() noexcept {
    using namespace micant;
    if (!ps::detail::deferredCleanupPending() && !io::IoDeferredFileDeletesPending())
        return true;
    auto* current = ke::KeGetCurrentThread();
    if (!current || !hal::amd64::bootstrapSchedulerClockActive()) return false;
    uintptr_t previousRoot{};
    __asm__ __volatile__("movq %%cr3, %0" : "=r"(previousRoot));
    if (!hal::amd64::switchProcessPagingRootByPhysical(
            hal::amd64::bootstrapKernelPagingRoot())) return false;
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    const auto priority = current->priority;
    current->priority = ke::PRIORITY_IDLE;
    if (ke::g_CurrentCpu.serviceDeferredCleanup)
        ke::g_CurrentCpu.serviceDeferredCleanup();
    const auto start = hal::amd64::bootstrapSchedulerClockTicks();
    __asm__ __volatile__("sti" : : : "memory");
    bool drained = false;
    for (size_t spin = 0; spin < 50000000 &&
         hal::amd64::bootstrapSchedulerClockTicks() - start < 100; ++spin) {
        if (!ps::detail::deferredCleanupPending() && !io::IoDeferredFileDeletesPending()) {
            drained = true;
            break;
        }
        __asm__ __volatile__("pause");
    }
    __asm__ __volatile__("cli" : : : "memory");
    current->priority = priority;
    const bool restored = hal::amd64::switchProcessPagingRootByPhysical(previousRoot);
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    serialWrite(drained ? "[MicaNT DBG] prior deferred cleanup drained before PFN baseline\r\n"
                        : "[MicaNT DBG] prior deferred cleanup timeout\r\n");
    return drained && restored;
}

static bool runUserThreads(bool preemptive, size_t threadCount) noexcept {
    using namespace micant;
    using namespace micant::ke;
    struct Progress {
        const char* stage{"entry"};
        bool complete{};
        ~Progress() {
            if (complete) return;
            serialWrite("[MicaNT DBG] user thread test failed at stage: ");
            serialWrite(stage);
            serialWrite("\r\n");
            ex::dumpHeapPfnLifetimeTrace();
        }
    } progress;
    auto& scheduler = PriorityScheduler::get();
    auto* boot = KeGetCurrentThread();
    if (!boot || scheduler.getReadyThreadCount() || threadCount < 2 || threadCount > 3 ||
        (preemptive && threadCount != 2)) return false;
    // A lifetime baseline must not include retired objects from earlier tests.
    if (!drainPreviousDeferredCleanup()) return false;
    const size_t active = mm::PfnDatabase::get().getActiveCount();
    const uint32_t sectionsBefore = sys::bootstrapSectionObjectCount();
    const uint32_t eventsBefore = sys::bootstrapEventObjectCount();
    uintptr_t previous{};
    __asm__ __volatile__("movq %%cr3, %0" : "=r"(previous));
    {
        progress.stage = "process and image setup";
        auto processStorage = std::unique_ptr<ps::EProcess>(new (std::nothrow_t{}) ps::EProcess{});
        if (!processStorage) return false;
        auto& process = *processStorage;
        if (process.initializeAddressSpace() != NtStatus::Success) return false;
        ProcessAddressSpaceCleanup processCleanup{process, previous};
        ps::EThread first;
        ps::EThread second;
        ps::EThread third;
        serialWrite("[MicaNT DBG] threads address space initialized\r\n");
        Handle sourceFirst{}, sourceSecond{}, eventFirst{}, eventSecond{};
        constexpr uint32_t access = ob::SYNCHRONIZE | 2;
        auto& source = ob::detail::currentHandleTable();
        if (sys::NtCreateEvent(&sourceFirst, access, nullptr, 1, false) != NtStatus::Success ||
            sys::NtCreateEvent(&sourceSecond, access, nullptr, 0, false) != NtStatus::Success ||
            source.duplicateHandle(sourceFirst, process.getHandleTable(), eventFirst) != NtStatus::Success ||
            source.duplicateHandle(sourceSecond, process.getHandleTable(), eventSecond) != NtStatus::Success ||
            sys::NtClose(sourceFirst) != NtStatus::Success || sys::NtClose(sourceSecond) != NtStatus::Success ||
            !hal::amd64::switchProcessPagingRootByPhysical(process.getDirectoryTableBase())) return false;
        serialWrite("[MicaNT DBG] threads events ready\r\n");
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
        serialWrite("[MicaNT DBG] threads loading image\r\n");
        static constexpr uint8_t invalidImage[]{0};
        if (process.loadNativeImage("bad.exe", {}) != NtStatus::InvalidImageFormat ||
            process.loadNativeImage("bad.exe", invalidImage) != NtStatus::InvalidImageFormat ||
            process.nativeImages() ||
            mm::PfnDatabase::get().getActiveCount() != beforeImage ||
            process.loadNativeImage("fixture.exe", std::span<const uint8_t>{file}, executablePath) != NtStatus::Success)
            return false;
        serialWrite("[MicaNT DBG] threads image loaded\r\n");
        const auto* images = process.nativeImages();
        const auto* image = images ? images->module(0) : nullptr;
        const auto* imageVad = process.getAddressSpace().findVad(code);
        if (!image || !imageVad || !imageVad->isSectionView || !imageVad->isImageView) return false;
        const uint8_t originalMagic = file[0];
        file[0] = 0;
        const bool ownsFile = image->view.fileBytes()[0] == originalMagic;
        file[0] = originalMagic;
        if (!ownsFile) return false;
        serialWrite("[MicaNT DBG] threads ownership verified\r\n");
        pe::PeImageView64::TlsLayout tlsLayout{};
        if (image->view.tlsLayout(std::span<const uint8_t>{reinterpret_cast<const uint8_t*>(code), 3 * mm::PageSize4KB},
                            code, tlsLayout) != NtStatus::Success || !tlsLayout.present || tlsLayout.callbackCount ||
            tlsLayout.indexRva != 2 * mm::PageSize4KB + 0x50) return false;
        serialWrite("[MicaNT DBG] threads tls layout verified\r\n");
        const auto unboundFirst = *reinterpret_cast<uint64_t*>(code + imports.firstThunk);
        if (process.finalizeNativeImages() != NtStatus::NoSuchFile ||
            *reinterpret_cast<uint64_t*>(code + imports.firstThunk) != unboundFirst) return false;
        serialWrite("[MicaNT DBG] threads missing import verified\r\n");
        pe::PeImageView64 dll;
        if (dll.inspect(std::span<const uint8_t>{nativeDll}) != NtStatus::Success) return false;
        const size_t beforeDuplicate = mm::PfnDatabase::get().getActiveCount();
        const auto duplicateStatus = process.loadNativeImage("fixture.exe", std::span<const uint8_t>{nativeDll});
        if (duplicateStatus != NtStatus::ObjectNameCollision ||
            mm::PfnDatabase::get().getActiveCount() != beforeDuplicate) return false;
        serialWrite("[MicaNT DBG] threads duplicate verified\r\n");
        text.characteristics |= pe::IMAGE_SCN_MEM_WRITE;
        std::memcpy(file + 64 + sizeof(nt), &text, sizeof(text));
        const auto wxStatus = process.loadNativeImage("wx.exe", std::span<const uint8_t>{file});
        text.characteristics &= ~pe::IMAGE_SCN_MEM_WRITE;
        std::memcpy(file + 64 + sizeof(nt), &text, sizeof(text));
        if (!drainDeferredDeletes(previous, process.getDirectoryTableBase())) return false;
        serialWrite("[MicaNT DBG] wx status="); serialNumber(static_cast<uint32_t>(wxStatus));
        serialWrite("[MicaNT DBG] wx PFN before="); serialNumber(beforeDuplicate);
        serialWrite("[MicaNT DBG] wx PFN after="); serialNumber(mm::PfnDatabase::get().getActiveCount());
        // W+X is a valid PE protection; this image still conflicts with the
        // fixture already mapped at the same preferred address.
        if (wxStatus != NtStatus::Conflict || mm::PfnDatabase::get().getActiveCount() != beforeDuplicate)
            return false;
        serialWrite("[MicaNT DBG] threads wx verified\r\n");
        if (process.loadNativeImage("ntdll.dll", std::span<const uint8_t>{nativeDll}, dllPath) != NtStatus::Success)
            return false;
        serialWrite("[MicaNT DBG] threads ntdll loaded\r\n");
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
        serialWrite("[MicaNT DBG] threads tls loaded\r\n");
        if (process.setNativeProcessParameters(executablePath, commandLine) != NtStatus::Success) return false;
        // Finalization reads the immutable image snapshot, not the writable
        // process mapping. Inject the unsupported metadata into that snapshot.
        auto* loadedTls = reinterpret_cast<pe::ImageTlsDirectory64*>(image->metadata.get() + 2 * mm::PageSize4KB);
        auto* callbacks = reinterpret_cast<uint64_t*>(image->metadata.get() + 2 * mm::PageSize4KB + 0x68);
        loadedTls->addressOfCallBacks = code + 2 * mm::PageSize4KB + 0x68;
        callbacks[0] = code + mm::PageSize4KB;
        callbacks[1] = 0;
        const auto callbackStatus = process.finalizeNativeImages();
        loadedTls->addressOfCallBacks = 0;
        callbacks[0] = 0;
        const bool callbackRejectedWithoutTlsMutation =
            *reinterpret_cast<uint32_t*>(code + tlsLayout.indexRva) == uninitializedTlsIndex &&
            *reinterpret_cast<uint32_t*>(tlsDllBase + mm::PageSize4KB + 0x90) == uninitializedTlsIndex;
        const auto finalizeStatus = process.finalizeNativeImages();
        serialWrite("[MicaNT DBG] threads images finalized\r\n");
        serialWrite("[MicaNT DBG] callback status="); serialNumber(static_cast<uint32_t>(callbackStatus));
        serialWrite("[MicaNT DBG] callback unchanged="); serialNumber(callbackRejectedWithoutTlsMutation);
        serialWrite("[MicaNT DBG] finalize status="); serialNumber(static_cast<uint32_t>(finalizeStatus));
        serialWrite("[MicaNT DBG] layouts="); serialNumber(process.userTlsLayouts().size());
        serialWrite("[MicaNT DBG] tls indices="); serialNumber(*reinterpret_cast<uint32_t*>(code + tlsLayout.indexRva));
        serialNumber(*reinterpret_cast<uint32_t*>(tlsDllBase + mm::PageSize4KB + 0x90));
        serialWrite("[MicaNT DBG] sections before/current="); serialNumber(sectionsBefore);
        serialNumber(sys::bootstrapSectionObjectCount());
        if (callbackStatus != NtStatus::NotImplemented || !callbackRejectedWithoutTlsMutation ||
            finalizeStatus != NtStatus::Success || process.userTlsLayouts().size() != 2 ||
            *reinterpret_cast<uint32_t*>(code + tlsLayout.indexRva) != 0 ||
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
        progress.stage = "thread PFN baseline";
        if (!drainPreviousDeferredCleanup()) return false;
        static mm::Mmpfn beforeThreadEntries[1024]{};
        static mm::Mmpfn finalThreadEntries[1024]{};
        const size_t beforeThread = mm::PfnDatabase::get().getActiveCount();
        const size_t beforeThreadRegionCount = process.getAddressSpace().getRegionCount();
        const auto beforeThreadEntryCount =
            mm::PfnDatabase::get().snapshotActiveEntries(beforeThreadEntries);
        resetWatchedThreadPfnTrace();
        constexpr uintptr_t watchedImageBase = 0x400020E00000ULL;
        constexpr uint64_t trackedPfns[]{0x1A8, 0x1C9, 0x262, 0x2F6};
        const char* firstPresentStage[4]{};
        const auto traceThreadPfnStage = [&](const char* stage) {
            const auto entryCount =
                mm::PfnDatabase::get().snapshotActiveEntries(finalThreadEntries);
            if (entryCount > 1024 || beforeThreadEntryCount > 1024) {
                serialWrite("[MicaNT DBG] PFN stage snapshot truncated after ");
                serialWrite(stage);
                serialWrite("\r\n");
                return;
            }
            size_t added{}, removed{};
            for (size_t i = 0; i < entryCount; ++i) {
                bool existed = false;
                for (size_t j = 0; j < beforeThreadEntryCount; ++j) {
                    if (beforeThreadEntries[j].pfn == finalThreadEntries[i].pfn) {
                        existed = true;
                        break;
                    }
                }
                if (!existed) ++added;
            }
            for (size_t i = 0; i < beforeThreadEntryCount; ++i) {
                bool remains = false;
                for (size_t j = 0; j < entryCount; ++j) {
                    if (beforeThreadEntries[i].pfn == finalThreadEntries[j].pfn) {
                        remains = true;
                        break;
                    }
                }
                if (!remains) ++removed;
            }
            serialWrite("[MicaNT DBG] PFN stage added/removed after ");
            serialWrite(stage);
            serialNumber(added);
            serialNumber(removed);
            serialWrite("[MicaNT DBG] VAD regions stage/current/baseline=");
            serialWrite(stage);
            serialWrite("/");
            serialNumber(process.getAddressSpace().getRegionCount());
            serialNumber(beforeThreadRegionCount);
            for (size_t trackedIndex = 0; trackedIndex < 4; ++trackedIndex) {
                const uint64_t trackedPfn = trackedPfns[trackedIndex];
                const mm::Mmpfn* beforeEntry{};
                const mm::Mmpfn* stageEntry{};
                for (size_t i = 0; i < beforeThreadEntryCount; ++i)
                    if (beforeThreadEntries[i].pfn == trackedPfn) {
                        beforeEntry = &beforeThreadEntries[i];
                        break;
                    }
                for (size_t i = 0; i < entryCount; ++i)
                    if (finalThreadEntries[i].pfn == trackedPfn) {
                        stageEntry = &finalThreadEntries[i];
                        break;
                    }
                if (stageEntry && !firstPresentStage[trackedIndex])
                    firstPresentStage[trackedIndex] = stage;
                serialWrite("[MicaNT DBG] PFN tracked/stage/before/ref/PTE=");
                serialHex(trackedPfn);
                serialWrite("/");
                serialWrite(stage);
                serialWrite("/");
                serialHex(beforeEntry ? beforeEntry->referenceCount : UINT64_MAX);
                serialWrite("/");
                serialHex(stageEntry ? stageEntry->referenceCount : UINT64_MAX);
                serialWrite("/");
                serialHex(stageEntry ? stageEntry->pteAddress : UINT64_MAX);
                serialWrite("\r\n");
            }
            const auto* imageVad = process.getAddressSpace().findVad(watchedImageBase);
            serialWrite("[MicaNT DBG] image VAD stage/address/section=");
            serialWrite(stage);
            serialWrite("/");
            serialHex(reinterpret_cast<uintptr_t>(imageVad));
            serialWrite("/");
            serialHex(imageVad ? imageVad->startingAddress : 0);
            serialWrite("/");
            serialHex(imageVad ? imageVad->isSectionView : 0);
            serialWrite("\r\n");
            serialWrite("[MicaNT DBG] native image set stage/address=");
            serialWrite(stage);
            serialWrite("/");
            serialHex(reinterpret_cast<uintptr_t>(process.nativeImages()));
            serialWrite("\r\n");
            traceWatchedThreadPfns(stage);
        };
        traceThreadPfnStage("thread baseline");
        const auto callbackFailure =
            first.initializeUser(process, entry, 32768, 32768, &invalidTls);
        traceThreadPfnStage("thread callback rejection");
        if (callbackFailure != NtStatus::NotImplemented ||
            mm::PfnDatabase::get().getActiveCount() != beforeThread) return false;
        invalidTls = tlsLayout;
        invalidTls.alignment = 3; // Fails after user pages were allocated; reap must roll them back.
        const auto alignmentFailure =
            first.initializeUser(process, entry, 32768, 32768, &invalidTls);
        traceThreadPfnStage("thread alignment rollback");
        if (alignmentFailure != NtStatus::InvalidParameter ||
            first.kernelThread() || mm::PfnDatabase::get().getActiveCount() != beforeThread) return false;
        // Reserve enough space for the user exception dispatcher while
        // committing only its guaranteed initial stack area.
        const auto partialStackStatus = first.initializeUser(
            process, entry, 32768, 8 * mm::PageSize4KB, mm::PageSize4KB, false);
        if (partialStackStatus != NtStatus::Success) return false;
        traceThreadPfnStage("partial stack allocation");
        const auto* partialTeb = reinterpret_cast<const um::Teb64*>(first.userTebAddress());
        if (!partialTeb || !hal::amd64::bootstrapUserRange(partialTeb->stackLimit, 1, true) ||
            hal::amd64::bootstrapUserRange(partialTeb->stackLimit - mm::PageSize4KB, 1, false) ||
            first.resolveUserStackGuard(partialTeb->stackLimit - mm::PageSize4KB) != NtStatus::GuardPageViolation)
            return false;
        const auto partialStackReapStatus = first.reap();
        if (partialStackReapStatus != NtStatus::Success ||
            mm::PfnDatabase::get().getActiveCount() != beforeThread) return false;
        traceThreadPfnStage("partial stack rollback");
        // Compose a separate process and initial user thread through the
        // private Ps factory, then roll it back before its entry executes.
        Handle createdProcess{}, createdThread{};
        const ps::detail::BootstrapProcessImage processImages[]{
            {"fixture.exe", std::span<const uint8_t>{file}, executablePath},
            {"ntdll.dll", std::span<const uint8_t>{nativeDll}, dllPath},
            {"tls.dll", std::span<const uint8_t>{tlsDllFile}, tlsPath},
        };
        const auto invalidFactoryStatus = ps::detail::createBootstrapUserProcess({}, executablePath, commandLine, 32768,
                32768, preemptive, ob::SYNCHRONIZE, ob::SYNCHRONIZE,
                createdProcess, createdThread);
        serialWrite("[MicaNT DBG] PFN before/invalid factory="); serialNumber(beforeThread);
        serialNumber(mm::PfnDatabase::get().getActiveCount());
        if (invalidFactoryStatus != NtStatus::InvalidParameter || createdProcess || createdThread) return false;
        const auto factoryStatus = ps::detail::createBootstrapUserProcess(processImages, executablePath, commandLine, 32768,
                32768, preemptive, ob::SYNCHRONIZE | 1 | 2 | 0x80, ob::SYNCHRONIZE,
                createdProcess, createdThread);
        if (factoryStatus != NtStatus::Success || !createdProcess || !createdThread) return false;
        serialWrite("[MicaNT DBG] threads factory complete\r\n");
        serialWrite("[MicaNT DBG] PFN factory="); serialNumber(mm::PfnDatabase::get().getActiveCount());
        traceThreadPfnStage("process factory");
        if (!testNativePeerThread(createdProcess, entry)) return false;
            serialWrite("[MicaNT DBG] peer thread test complete\r\n");
        serialWrite("[MicaNT DBG] PFN peer="); serialNumber(mm::PfnDatabase::get().getActiveCount());
        traceThreadPfnStage("peer thread test");
        if (!testNativeThreadExecution(createdProcess, createdThread, entry, previous,
                                       process.getDirectoryTableBase())) return false;
        serialWrite("[MicaNT DBG] PFN execution="); serialNumber(mm::PfnDatabase::get().getActiveCount());
        traceThreadPfnStage("first execution test");
        if (!testNativeThreadExecution(createdProcess, createdThread, entry, previous,
                                       process.getDirectoryTableBase(), true)) return false;
        serialWrite("[MicaNT DBG] PFN stack="); serialNumber(mm::PfnDatabase::get().getActiveCount());
        traceThreadPfnStage("stack growth test");
        serialWrite("[MicaNT DBG] threads execution tests complete\r\n");
        Handle clonedProcess{};
        const auto cloneStatus = sys::NtCreateProcessEx(&clonedProcess,
            ob::SYNCHRONIZE | 1 | 2, nullptr, createdProcess, 0, 0, 0, 0, false);
        serialWrite("[MicaNT DBG] clone status="); serialNumber(static_cast<uint32_t>(cloneStatus));
        serialWrite("[MicaNT DBG] PFN clone="); serialNumber(mm::PfnDatabase::get().getActiveCount());
        if (cloneStatus != NtStatus::Success) return false;
        traceWatchedThreadPfns("clone creation");
        ps::EThread* createdThreadObject{};
        ob::ObjectHeader* createdThreadRef{};
        if (ps::detail::referenceThreadHandle(createdThread, ob::SYNCHRONIZE,
                createdThreadObject, createdThreadRef) != NtStatus::Success || !createdThreadObject) return false;
        const auto cancelStatus = createdThreadObject->cancelBeforeStart();
        serialWrite("[MicaNT DBG] clone initial thread cancel="); serialNumber(static_cast<uint32_t>(cancelStatus));
        if (cancelStatus != NtStatus::Success) return false;
        traceWatchedThreadPfns("initial thread cancellation");
        const auto reapStatus = createdThreadObject->reap();
        serialWrite("[MicaNT DBG] clone initial thread reap="); serialNumber(static_cast<uint32_t>(reapStatus));
        if (reapStatus != NtStatus::Success) return false;
        traceWatchedThreadPfns("initial thread reap");
        ob::ObDereferenceObject(createdThreadRef);
        const auto closeThreadStatus = sys::NtClose(createdThread);
        if (closeThreadStatus != NtStatus::Success) return false;
        traceWatchedThreadPfns("initial thread handle close");
        const auto parentTerminateStatus = sys::NtTerminateProcess(createdProcess, NtStatus::Success);
        LargeInteger parentPoll{};
        const auto parentWaitStatus = sys::NtWaitForSingleObject(createdProcess, false, &parentPoll);
        serialWrite("[MicaNT DBG] clone parent terminate/wait="); serialNumber(static_cast<uint32_t>(parentTerminateStatus));
        serialNumber(static_cast<uint32_t>(parentWaitStatus));
        if ((parentTerminateStatus != NtStatus::Success && parentTerminateStatus != NtStatus::ProcessIsTerminating) ||
            parentWaitStatus != NtStatus::Success) return false;
        traceWatchedThreadPfns("parent process termination");
        const auto closeProcessStatus = sys::NtClose(createdProcess);
        serialWrite("[MicaNT DBG] clone parent close="); serialNumber(static_cast<uint32_t>(closeProcessStatus));
        serialWrite("[MicaNT DBG] PFN parent close="); serialNumber(mm::PfnDatabase::get().getActiveCount());
        if (closeProcessStatus != NtStatus::Success) return false;
        traceThreadPfnStage("parent process close");
        progress.stage = "clone image lifetime";
        const auto cloneLifetime = testClonedImageLifetime(clonedProcess);
        serialWrite("[MicaNT DBG] clone lifetime="); serialNumber(cloneLifetime);
        if (!cloneLifetime) return false;
        if (!waitForPfnBaselineAndDeferredDeletes(previous, process.getDirectoryTableBase(),
                std::span<const mm::Mmpfn>{beforeThreadEntries, beforeThreadEntryCount},
                finalThreadEntries)) return false;
        traceThreadPfnStage("clone lifetime close");
        serialWrite("[MicaNT DBG] clone PFN baseline/current="); serialNumber(beforeThread);
        serialNumber(mm::PfnDatabase::get().getActiveCount());
        const auto finalThreadEntryCount =
            mm::PfnDatabase::get().snapshotActiveEntries(finalThreadEntries);
        for (size_t trackedIndex = 0; trackedIndex < 4; ++trackedIndex) {
            serialWrite("[MicaNT DBG] PFN first present/stage=");
            serialHex(trackedPfns[trackedIndex]);
            serialWrite("/");
            serialWrite(firstPresentStage[trackedIndex]
                ? firstPresentStage[trackedIndex] : "not observed");
            serialWrite("\r\n");
        }
        if (finalThreadEntryCount <= 1024 && beforeThreadEntryCount <= 1024) {
            traceThreadPfnStage("final comparison");
            const auto* watchedImageVad = process.getAddressSpace().findVad(watchedImageBase);
            serialWrite("[MicaNT DBG] final process/image/VAD=");
            serialHex(process.processId());
            serialWrite("/");
            serialHex(reinterpret_cast<uintptr_t>(process.nativeImages()));
            serialWrite("/");
            serialHex(reinterpret_cast<uintptr_t>(watchedImageVad));
            serialWrite("\r\n");
            if (watchedImageVad) {
                serialWrite("[MicaNT DBG] final image VAD range/section=");
                serialHex(watchedImageVad->startingAddress);
                serialWrite("/");
                serialHex(watchedImageVad->endingAddress);
                serialWrite("/");
                serialHex(watchedImageVad->isSectionView);
                serialWrite("\r\n");
            }
            if (const auto* imageSet = process.nativeImages()) {
                serialWrite("[MicaNT DBG] native image set/module size=");
                serialHex(sizeof(*imageSet));
                serialWrite("/");
                serialHex(sizeof(um::NativeUserImageSet64::Module));
                serialWrite("\r\n");
                if (const auto* module = imageSet->module(0)) {
                    serialWrite("[MicaNT DBG] native module metadata/base=");
                    serialHex(reinterpret_cast<uintptr_t>(module->metadata.get()));
                    serialWrite("/");
                    serialHex(module->base);
                    serialWrite("\r\n");
                }
            }
            uintptr_t activeRoot{};
            __asm__ __volatile__("movq %%cr3, %0" : "=r"(activeRoot));
            const auto* pml4 = reinterpret_cast<const uint64_t*>(
                activeRoot & 0x000ffffffffff000ULL);
            for (size_t slot = 129; slot <= 131; ++slot) {
                const uint64_t entry = pml4[slot];
                if (!(entry & 1)) continue;
                const uint64_t childPfn = (entry & 0x000ffffffffff000ULL) >> 12;
                const auto* child = reinterpret_cast<const uint64_t*>(childPfn << 12);
                size_t childEntries{};
                for (size_t i = 0; i < 512; ++i)
                    if (child[i] & 1) ++childEntries;
                serialWrite("[MicaNT DBG] shared kernel PML4 slot/PFN/entries=");
                serialHex(slot);
                serialWrite("/");
                serialHex(childPfn);
                serialWrite("/");
                serialHex(childEntries);
                serialWrite("\r\n");
            }
            for (size_t i = 0; i < finalThreadEntryCount; ++i) {
                bool existed = false;
                for (size_t j = 0; j < beforeThreadEntryCount; ++j) {
                    if (beforeThreadEntries[j].pfn == finalThreadEntries[i].pfn) {
                        existed = true;
                        break;
                    }
                }
                if (existed) continue;
                serialWrite("[MicaNT DBG] final-only PFN/ref/PTE=");
                serialNumber(finalThreadEntries[i].pfn);
                serialNumber(finalThreadEntries[i].referenceCount);
                serialNumber(finalThreadEntries[i].pteAddress);
                for (size_t slot = 129; slot <= 131; ++slot) {
                    const uint64_t entry = pml4[slot];
                    if (!(entry & 1)) continue;
                    const uint64_t childPfn = (entry & 0x000ffffffffff000ULL) >> 12;
                    if (childPfn == finalThreadEntries[i].pfn) {
                        serialWrite("[MicaNT DBG] final-only PFN is shared kernel PML4 child slot=");
                        serialHex(slot);
                        serialWrite("\r\n");
                    }
                }
                const auto* page = reinterpret_cast<const uint64_t*>(
                    finalThreadEntries[i].pfn << mm::PageShift4KB);
                size_t nonzeroEntries{};
                for (size_t entry = 0; entry < 512; ++entry)
                    if (page[entry]) ++nonzeroEntries;
                serialWrite("[MicaNT DBG] final-only PFN nonzero qwords=");
                serialHex(nonzeroEntries);
                serialWrite("\r\n");
                serialWrite("[MicaNT DBG] final-only PFN first qwords=");
                for (size_t entry = 0; entry < 16; ++entry) {
                    if (entry) serialWrite("/");
                    serialHex(page[entry]);
                }
                serialWrite("\r\n");
            }
        } else {
            serialWrite("[MicaNT DBG] final thread PFN snapshot truncated\r\n");
        }
        printWatchedThreadPfnTrace();
        if (mm::PfnDatabase::get().getActiveCount() != beforeThread) return false;
        // Exercise Ob ownership for a user context before the real workers.
        progress.stage = "Ob-owned user thread lifecycle";
        Handle objectHandle{};
        ps::EThread* objectThread{};
        const auto objectCreateStatus = ps::detail::createUserThreadHandle(process, entry, 32768, 32768,
                                               preemptive, ob::SYNCHRONIZE,
                                               objectHandle, objectThread);
        if (objectCreateStatus != NtStatus::Success || !objectThread) return false;
        const auto objectCancelStatus = objectThread->cancelBeforeStart();
        if (objectCancelStatus != NtStatus::Success) return false;
        LargeInteger objectPoll{};
        const auto objectWaitStatus = sys::NtWaitForSingleObject(objectHandle, false, &objectPoll);
        const auto objectReapStatus = objectThread->reap();
        const auto objectCloseStatus = sys::NtClose(objectHandle);
        if (objectWaitStatus != NtStatus::Success || objectReapStatus != NtStatus::Success ||
            objectCloseStatus != NtStatus::Success) {
            serialWrite("[MicaNT DBG] owned user thread wait/reap/close=");
            serialNumber(static_cast<uint32_t>(objectWaitStatus));
            serialNumber(static_cast<uint32_t>(objectReapStatus));
            serialNumber(static_cast<uint32_t>(objectCloseStatus));
            return false;
        }
        if (!waitForPfnBaselineAndDeferredDeletes(previous, process.getDirectoryTableBase(),
                std::span<const mm::Mmpfn>{beforeThreadEntries, beforeThreadEntryCount},
                finalThreadEntries)) return false;
        serialWrite("[MicaNT DBG] owned user thread cleanup matched exact PFN baseline\r\n");
        ps::EThread* threads[]{&first, &second, &third};
        progress.stage = "user workers initialization";
        uintptr_t tebs[3]{}, tlsBlocks[3]{}, tlsSecond[3]{};
        for (size_t i = 0; i < threadCount; ++i) {
            if (threads[i]->initializeUser(process, entry, 32768, 8 * mm::PageSize4KB,
                                           mm::PageSize4KB, preemptive) != NtStatus::Success)
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
        progress.stage = "user workers ready";
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
        progress.stage = "user workers execution and termination";
        const KIRQL oldIrql = KfRaiseIrql(DISPATCH_LEVEL);
        const bool switched = KiSwapContext(boot, launcher.get());
        KeLowerIrql(oldIrql);
        if (!switched || !process.isTerminating() || process.exitStatus() != NtStatus::AccessDenied ||
            scheduler.getReadyThreadCount() ||
            !hal::amd64::switchProcessPagingRootByPhysical(process.getDirectoryTableBase())) return false;
        progress.stage = "user workers PEB and TLS validation";
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
        progress.stage = "user workers reap";
        for (size_t i = 0; i < threadCount; ++i)
            if (threads[i]->reap() != NtStatus::Success) return false;
        progress.stage = "user process address space close";
        const auto processCloseStatus = process.closeAddressSpace();
        if (processCloseStatus != NtStatus::Success) return false;
        processCleanup.active = false;
    }
    progress.stage = "final resource recovery";
    if (!drainPreviousDeferredCleanup()) return false;
    const auto finalPfnCount = mm::PfnDatabase::get().getActiveCount();
    const auto finalSectionCount = sys::bootstrapSectionObjectCount();
    const auto finalEventCount = sys::bootstrapEventObjectCount();
    const bool resourcesRestored = finalPfnCount == active &&
        finalSectionCount == sectionsBefore && finalEventCount == eventsBefore;
    if (!resourcesRestored) {
        serialWrite("[MicaNT DBG] final PFN/section/event counts before/after=");
        serialNumber(active);
        serialNumber(finalPfnCount);
        serialNumber(sectionsBefore);
        serialNumber(finalSectionCount);
        serialNumber(eventsBefore);
        serialNumber(finalEventCount);
        ex::dumpHeapPfnLifetimeTrace();
    }
    progress.complete = resourcesRestored;
    return resourcesRestored;
}

extern "C" bool MicantTestPsTwoUserThreads() noexcept { return runUserThreads(false, 2); }
extern "C" bool MicantTestPsThreeUserThreads() noexcept { return runUserThreads(false, 3); }
extern "C" bool MicantTestPsUserPreemption() noexcept { return runUserThreads(true, 2); }
