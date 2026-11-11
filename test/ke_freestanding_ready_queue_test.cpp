#include "micant/km/core/ke.hpp"

#include <cassert>

extern "C" bool MicantTestKeReadyQueue() noexcept;

int main() {
    using namespace micant::ke;
    auto& scheduler = PriorityScheduler::get();
    KThread low;
    KThread first;
    KThread second;
    KThread terminated;
    KThread high;
    low.priority = 2;
    first.priority = 12;
    second.priority = 12;
    terminated.state = ThreadState::Terminated;
    high.priority = 99;
    const auto switches = g_CurrentCpu.contextSwitches;

    assert(!scheduler.readyKThread(nullptr));
    assert(!scheduler.readyKThread(&terminated));
    assert(scheduler.readyKThread(&low));
    assert(scheduler.readyKThread(&first));
    assert(scheduler.readyKThread(&second));
    assert(scheduler.readyKThread(&high));
    assert(!scheduler.readyKThread(&first));
    assert(scheduler.getReadyThreadCount() == 4);
    assert(scheduler.selectNextThread() == &high);
    assert(high.priority == PRIORITY_REALTIME_MAX);
    assert(scheduler.selectNextThread() == &first);
    assert(scheduler.selectNextThread() == &second);
    assert(scheduler.selectNextThread() == &low);
    assert(first.state == ThreadState::Standby && !first.readyQueued);
    assert(scheduler.selectNextThread() == nullptr);
    assert(scheduler.getReadyThreadCount() == 0);
    assert(g_CurrentCpu.contextSwitches == switches);
    assert(KeGetCurrentIrql() == PASSIVE_LEVEL);
    first.state = ThreadState::Waiting;
    assert(scheduler.readyKThread(&first));
    assert(!MicantTestKeReadyQueue());
    assert(scheduler.getReadyThreadCount() == 1 && first.readyQueued);
    assert(scheduler.selectNextThread() == &first);
    assert(MicantTestKeReadyQueue());
    assert(MicantTestKeReadyQueue());
    assert(g_CurrentCpu.contextSwitches == switches);
    assert(KeGetCurrentIrql() == PASSIVE_LEVEL);
}
