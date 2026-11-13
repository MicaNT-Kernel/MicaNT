#include "micant/km/executive/mm.hpp"

#include <memory>

int main() {
    using namespace micant::mm;
    MmAvlTable table;
    auto first = std::make_shared<int>(1);
    auto successor = std::make_shared<int>(2);
    std::weak_ptr<int> firstWeak = first;
    std::weak_ptr<int> successorWeak = successor;

    auto insert = [&](uintptr_t address, const std::shared_ptr<int>& owner) {
        auto vad = std::make_unique<VirtualAddressDescriptor>();
        vad->startingAddress = address;
        vad->endingAddress = address + PageSize4KB - 1;
        vad->isSectionView = owner != nullptr;
        vad->sectionContext = owner.get();
        vad->sectionOwner = owner;
        return table.insert(std::move(vad));
    };
    if (!insert(0x20000, first) || !insert(0x10000, {}) ||
        !insert(0x30000, successor)) return 1;
    first.reset();
    successor.reset();
    const auto* successorIdentity = table.find(0x30000);
    if (firstWeak.expired() || successorWeak.expired()) return 2;
    if (!table.remove(0x20000) || !firstWeak.expired() || successorWeak.expired())
        return 3;
    const auto* moved = table.find(0x30000);
    if (!moved || moved != successorIdentity ||
        moved->sectionContext != successorWeak.lock().get() ||
        moved->sectionOwner.get() != moved->sectionContext) return 4;
    if (!table.remove(0x30000) || !successorWeak.expired()) return 5;
    return 0;
}
