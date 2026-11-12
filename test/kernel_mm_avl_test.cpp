#include "micant/km/executive/mm.hpp"

#include <memory>
#include <new>

namespace {

using micant::mm::MmAvlTable;
using micant::mm::PageMask4KB;
using micant::mm::PageSize4KB;
using micant::mm::VirtualAddressDescriptor;

std::unique_ptr<VirtualAddressDescriptor> makeVad(uintptr_t start, uintptr_t end) noexcept {
    void* memory = ::operator new(sizeof(VirtualAddressDescriptor), std::nothrow_t{});
    if (!memory) return nullptr;
    auto node = std::unique_ptr<VirtualAddressDescriptor>(
        ::new (memory) VirtualAddressDescriptor{});
    node->startingAddress = start;
    node->endingAddress = end;
    return node;
}

int exerciseTable(size_t initialActive) noexcept {
#if !defined(MICANT_FREESTANDING)
    (void)initialActive;
#endif
    MmAvlTable tree;
    auto middle = makeVad(0x4000, 0x4fff);
    auto left = makeVad(0x2000, 0x2fff);
    auto right = makeVad(0x6000, 0x6fff);
    if (!middle || !left || !right) return 1;
    if (!tree.insert(std::move(middle)) || !tree.insert(std::move(left)) ||
        !tree.insert(std::move(right))) return 2;
    if (tree.size() != 3 || !tree.find(0x4000) || !tree.find(0x4fff) ||
        tree.find(0x3000)) return 3;
#if defined(MICANT_FREESTANDING)
    auto& database = micant::mm::PfnDatabase::get();
    const size_t activeWithThree = database.getActiveCount();
    if (activeWithThree <= initialActive) return 8;
#endif

    auto overlap = makeVad(0x3000, 0x4fff);
    if (!overlap) return 1;
    if (tree.insert(std::move(overlap)) || tree.size() != 3 ||
        !tree.find(0x4000) || tree.find(0x3000)) return 4;
    // Failed insertion leaves the candidate with the caller.
    overlap.reset();
#if defined(MICANT_FREESTANDING)
    if (database.getActiveCount() != activeWithThree) return 8;
#endif

    if (tree.findFreeRange(PageSize4KB, PageSize4KB, false, 0x2000, 0x6fff) != 0x3000 ||
        tree.findFreeRange(PageSize4KB, PageSize4KB, true, 0x2000, 0x6fff) != 0x5000 ||
        tree.findFreeRange(2 * PageSize4KB, PageSize4KB, false, 0x2000, 0x6fff) != 0)
        return 5;

    auto* successor = tree.find(0x6000);
    if (!successor) return 3;
    successor->sectionOffset = PageSize4KB;
    if (!tree.remove(0x4000) || tree.remove(0x4000) || tree.size() != 2 ||
        tree.find(0x4000) || !tree.find(0x6000) ||
        tree.find(0x6000)->sectionOffset != PageSize4KB ||
        tree.findFreeRange(2 * PageSize4KB, PageSize4KB, false, 0x2000, 0x6fff) != 0x3000)
        return 6;
#if defined(MICANT_FREESTANDING)
    if (database.getActiveCount() >= activeWithThree) return 8;
#endif

    tree.clear();
    if (tree.size() != 0 || tree.find(0x2000) || tree.find(0x6000)) return 7;
#if defined(MICANT_FREESTANDING)
    if (database.getActiveCount() != initialActive) return 8;
#endif

    // Leave one descriptor for the table destructor to release.
    auto last = makeVad(0x8000, 0x8000 + PageMask4KB);
    if (!last) return 1;
    if (!tree.insert(std::move(last))) return 2;
    return 0;
}

} // namespace

// Call only after bootstrap heap initialization, on one CPU with IRQs disabled.
extern "C" int MicantTestMmAvlTable() noexcept {
#if defined(MICANT_FREESTANDING)
    auto& database = micant::mm::PfnDatabase::get();
    const size_t active = database.getActiveCount();
    const size_t available = database.getFreeCount() + database.getZeroedCount();
#else
    constexpr size_t active = 0;
#endif
    const int result = exerciseTable(active);
#if defined(MICANT_FREESTANDING)
    if (database.getActiveCount() != active ||
        database.getFreeCount() + database.getZeroedCount() != available) return 8;
#endif
    return result;
}

#if defined(MICANT_MM_AVL_TEST_STANDALONE)
int main() {
    return MicantTestMmAvlTable();
}
#endif
