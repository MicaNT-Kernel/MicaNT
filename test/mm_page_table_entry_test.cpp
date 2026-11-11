#include "micant/km/executive/mm.hpp"

#include <cassert>
#include <cstdint>

int main() {
    using micant::mm::PageTableEntry;
    constexpr uint64_t nx = uint64_t{1} << 63;
    constexpr uint64_t address = 0x123456789000ULL;
    const auto data = PageTableEntry::create(address >> 12, true, true, true);
    assert(data.value == (address | nx | 7));
    assert(data.getPhysicalAddress() == address);

    const auto code = PageTableEntry::create(address >> 12, false, true, false);
    assert(code.value == (address | 5));
    assert(code.getPhysicalAddress() == address);

    auto modified = code;
    modified.noExecute = 1;
    assert(modified.value == (address | nx | 5));
    modified.noExecute = 0;
    assert(modified.value == code.value);
}
