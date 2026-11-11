#include "micant/km/executive/mm.hpp"

#include <limits>
#include <memory>
#include <new>

using namespace micant::mm;

static std::unique_ptr<VirtualAddressDescriptor> descriptor(uintptr_t start, uintptr_t end) {
    void* memory = ::operator new(sizeof(VirtualAddressDescriptor), std::nothrow_t{});
    if (!memory) return nullptr;
    auto node = std::unique_ptr<VirtualAddressDescriptor>(::new (memory) VirtualAddressDescriptor{});
    node->startingAddress = start;
    node->endingAddress = end;
    return node;
}

#define CHECK(expression) do { if (!(expression)) return __LINE__; } while (false)

extern "C" int mm_avl_freestanding_test() {
    constexpr uintptr_t max = std::numeric_limits<uintptr_t>::max();
    MmAvlTable tree;
    CHECK(tree.insert(descriptor(0x4000, 0x4fff)));
    CHECK(tree.insert(descriptor(0x2000, 0x2fff)));
    CHECK(tree.insert(descriptor(0x6000, 0x6fff)));
    CHECK(tree.insert(descriptor(0x5000, 0x5fff)));
    CHECK(tree.insert(descriptor(0x7000, 0x7fff)));
    CHECK(!tree.insert(descriptor(0x3000, 0x4fff)));
    CHECK(!tree.insert(descriptor(0x8100, 0x8fff)));
    CHECK(tree.size() == 5);
    CHECK(tree.find(0x4000) && tree.find(0x4fff));
    CHECK(!tree.find(0x3fff));
    CHECK(tree.findFreeRange(0x1000, 0x1000, false, 0x1000, 0x8fff) == 0x1000);
    CHECK(tree.findFreeRange(0x1000, 0x1000, true, 0x1000, 0x8fff) == 0x8000);
    CHECK(tree.findFreeRange(0x2000, 0x1000, false, 0x1000, 0x8fff) == 0);
    CHECK(tree.findFreeRange(0x1000, 6000, false, 0x1000, 0x8fff) == 0);
    CHECK(tree.findFreeRange(max, 0x1000, false, 0x1000, max) == 0);
    CHECK(tree.findFreeRange(0x1000, 0x1000, true, max - 0xfff, max) == max - 0xfff);
    auto* successor = tree.find(0x5000);
    CHECK(successor);
    successor->isSectionView = true;
    successor->sectionContext = &tree;
    successor->sectionOffset = 0x2000;
#if !defined(MICANT_FREESTANDING)
    successor->physicalPages.emplace(5, 42);
#endif
    CHECK(tree.remove(0x4000));
    CHECK(!tree.find(0x4000) && tree.find(0x5000));
    successor = tree.find(0x5000);
    CHECK(successor->isSectionView && successor->sectionContext == &tree &&
          successor->sectionOffset == 0x2000);
#if !defined(MICANT_FREESTANDING)
    CHECK(successor->physicalPages.at(5) == 42);
#endif
    CHECK(tree.findFreeRange(0x1000, 0x1000, false, 0x3000, 0x4fff) == 0x3000);
    CHECK(tree.findFreeRange(0x1000, 0x1000, true, 0x3000, 0x4fff) == 0x4000);
    CHECK(!tree.remove(0xa000));
    CHECK(tree.size() == 4);
    tree.clear();
    CHECK(tree.size() == 0 && !tree.find(0x5000));
    CHECK(!tree.insert(nullptr) && tree.size() == 0);
    CHECK(tree.insert(descriptor(0x1000, 0x1fff)));

    tree.clear();
    constexpr uintptr_t base = 0x10000;
    constexpr unsigned pages = 32;
    bool occupied[pages]{};
    uint32_t random = 1;
    for (unsigned iteration = 0; iteration < 1000; ++iteration) {
        random = random * 1664525u + 1013904223u;
        const unsigned page = (random >> 16) % pages;
        const uintptr_t address = base + page * PageSize4KB;
        if (occupied[page]) {
            CHECK(tree.remove(address));
        } else {
            CHECK(tree.insert(descriptor(address, address + PageMask4KB)));
        }
        occupied[page] = !occupied[page];
        for (unsigned length = 1; length <= 4; ++length) {
            for (unsigned alignmentPages : {1u, 2u, 4u}) {
                uintptr_t first = 0, last = 0;
                for (unsigned start = 0; start + length <= pages; start += alignmentPages) {
                    bool free = true;
                    for (unsigned offset = 0; offset < length; ++offset)
                        free = free && !occupied[start + offset];
                    if (free) {
                        if (!first) first = base + start * PageSize4KB;
                        last = base + start * PageSize4KB;
                    }
                }
                CHECK(tree.findFreeRange(length * PageSize4KB, alignmentPages * PageSize4KB,
                                         false, base, base + pages * PageSize4KB - 1) == first);
                CHECK(tree.findFreeRange(length * PageSize4KB, alignmentPages * PageSize4KB,
                                         true, base, base + pages * PageSize4KB - 1) == last);
            }
        }
    }
    return 0;
}

#if !defined(_WIN32)
int main() {
    return mm_avl_freestanding_test();
}
#endif
