#include "micant/km/core/ke.hpp"
#include <cassert>
#include <cstdio>

int main() {
    using namespace micant::ke;
    KThread first;
    KThread second;
    first.tid = 4;
    second.tid = 8;
    first.state = ThreadState::Running;
    second.state = ThreadState::Ready;
    first.context.rip = 0x1234;
    first.context.rsp = 0x5678;
    second.directoryTableBase = 0x9000;
    g_CurrentCpu.currentThread = &first;
    g_CurrentCpu.currentIrql = PASSIVE_LEVEL;
    const auto initialSwitches = g_CurrentCpu.contextSwitches;
    assert(!KiSwapContext(&first, nullptr));
    assert(g_CurrentCpu.currentThread == &first);
    assert(g_CurrentCpu.contextSwitches == initialSwitches);
    assert(KiSwapContext(&first, &second));
    assert(first.state == ThreadState::Ready);
    assert(second.state == ThreadState::Running);
    assert(first.context.rip == 0x1234 && first.context.rsp == 0x5678);
    assert(KeGetCurrentThread() == &second);
    assert(g_CurrentCpu.currentCr3 == 0x9000);
    assert(KeGetCurrentIrql() == PASSIVE_LEVEL);
    assert(g_CurrentCpu.contextSwitches == initialSwitches + 1);
    g_CurrentCpu.currentIrql = DISPATCH_LEVEL;
    assert(KiSwapContext(&second, &first));
    assert(KeGetCurrentThread() == &first);
    assert(KeGetCurrentIrql() == DISPATCH_LEVEL);
    g_CurrentCpu.currentIrql = PASSIVE_LEVEL;
    g_CurrentCpu.currentThread = nullptr;
    std::puts("Hosted canonical context regression passed");
}
