#include "micant/km/core/ke.hpp"

#if !defined(_M_X64) && !defined(__x86_64__)
extern "C" void MicantAmd64BootstrapThread() noexcept {}
#endif

namespace {
void entry(void*) noexcept {}
}

extern "C" bool MicantTestKeBootstrapThreadPreparation() noexcept {
    using namespace micant::ke;
    alignas(16) unsigned char stack[512]{};
    KThread thread;
    int argument = 42;
    const auto base = reinterpret_cast<uintptr_t>(stack);
    auto prepare = [&](void* p, size_t size, detail::BootstrapThreadEntry routine) {
        return detail::prepareBootstrapThread(thread, p, size, routine, &argument);
    };
    if (prepare(nullptr, sizeof(stack), entry) ||
        prepare(stack, 255, entry) || prepare(stack, sizeof(stack), nullptr) ||
        prepare(reinterpret_cast<void*>(UINTPTR_MAX - 100), 256, entry) ||
        thread.context.rip || thread.context.rsp || thread.kernelStackBase ||
        thread.kernelStackTop || thread.state != ThreadState::Initialized) return false;

    thread.state = ThreadState::Waiting;
    if (prepare(stack, sizeof(stack), entry)) return false;
    thread.state = ThreadState::Initialized;
    KThread* previous = g_CurrentCpu.currentThread;
    g_CurrentCpu.currentThread = &thread;
    const bool acceptedCurrent = prepare(stack, sizeof(stack), entry);
    g_CurrentCpu.currentThread = previous;
    if (acceptedCurrent || !prepare(stack + 1, sizeof(stack) - 1, entry)) return false;

    const uintptr_t top = (base + sizeof(stack)) & ~uintptr_t{15};
    if (thread.kernelStackBase != base + 1 || thread.kernelStackTop != top ||
        thread.context.rsp != top - 32 || (thread.context.rsp & 15) ||
        thread.context.rip != reinterpret_cast<uintptr_t>(&detail::MicantAmd64BootstrapThread) ||
        thread.context.r12 != reinterpret_cast<uintptr_t>(&entry) ||
        thread.context.r13 != reinterpret_cast<uintptr_t>(&argument) ||
        thread.context.rflags != 2 || thread.context.mxcsr != 0x1f80 ||
        thread.context.x87ControlWord != 0x037f ||
        thread.state != ThreadState::Initialized ||
        prepare(stack, sizeof(stack), entry)) return false;

    auto& scheduler = PriorityScheduler::get();
    if (scheduler.getReadyThreadCount() != 0 ||
        !scheduler.readyKThread(&thread)) return false;
    if (prepare(stack, sizeof(stack), entry)) {
        (void)scheduler.removeReadyThread(&thread);
        return false;
    }
    return scheduler.selectNextThread() == &thread &&
           thread.state == ThreadState::Standby && !thread.readyQueued &&
           scheduler.getReadyThreadCount() == 0;
}

#if defined(MICANT_X64_HOST_SWITCH_TEST) && defined(__APPLE__) && defined(__x86_64__)
extern "C" bool __attribute__((ms_abi)) SwitchWindowsAbi(
    micant::ke::KThreadContext*, micant::ke::KThreadContext*) noexcept
    __asm("_MicantAmd64SwitchContext");

namespace {
struct SwitchProbe {
    micant::ke::KThreadContext* caller;
    micant::ke::KThreadContext* worker;
    uintptr_t localAddress;
    bool entered;
};

void __attribute__((ms_abi)) switchEntry(void* opaque) noexcept {
    auto* probe = static_cast<SwitchProbe*>(opaque);
    volatile unsigned char local = 1;
    probe->localAddress = reinterpret_cast<uintptr_t>(&local);
    probe->entered = true;
    (void)SwitchWindowsAbi(probe->worker, probe->caller);
    __builtin_trap();
}

bool testHardwareSwitch() {
    using namespace micant::ke;
    alignas(16) unsigned char stack[16384]{};
    KThread caller;
    KThread worker;
    SwitchProbe probe{&caller.context, &worker.context, 0, false};
    auto entryPoint = reinterpret_cast<detail::BootstrapThreadEntry>(&switchEntry);
    if (!detail::prepareBootstrapThread(worker, stack, sizeof(stack), entryPoint, &probe) ||
        !SwitchWindowsAbi(&caller.context, &worker.context)) return false;
    const auto base = reinterpret_cast<uintptr_t>(stack);
    return probe.entered && probe.localAddress >= base &&
           probe.localAddress < base + sizeof(stack);
}
}
#endif

#if defined(MICANT_STANDALONE_TEST)
int main() {
    if (!MicantTestKeBootstrapThreadPreparation()) return 1;
#if defined(MICANT_X64_HOST_SWITCH_TEST) && defined(__APPLE__) && defined(__x86_64__)
    if (!testHardwareSwitch()) return 1;
#endif
    return 0;
}
#endif
