#include "micant/ucrt/bounded_arena.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>

int main() {
    micant::ucrt::BoundedArena<128> defaultConstructed;
    assert(defaultConstructed.allocate(16) != nullptr);

    micant::ucrt::BoundedArena<2048> arena{};

    auto* bytes = static_cast<unsigned char*>(arena.allocate(64));
    assert(bytes != nullptr);
    assert(reinterpret_cast<std::uintptr_t>(bytes) % alignof(std::max_align_t) == 0);
    for (std::size_t i = 0; i < 64; ++i) {
        bytes[i] = static_cast<unsigned char>(i);
    }

    auto* grown = static_cast<unsigned char*>(arena.reallocate(bytes, 192));
    assert(grown == bytes);
    for (std::size_t i = 0; i < 64; ++i) {
        assert(grown[i] == static_cast<unsigned char>(i));
    }

    auto* blocker = arena.allocate(64);
    assert(blocker != nullptr);
    const auto oldAddress = reinterpret_cast<std::uintptr_t>(grown);
    auto* moved = static_cast<unsigned char*>(arena.reallocate(grown, 320));
    assert(moved != nullptr);
    assert(reinterpret_cast<std::uintptr_t>(moved) != oldAddress);
    for (std::size_t i = 0; i < 64; ++i) {
        assert(moved[i] == static_cast<unsigned char>(i));
    }
    arena.release(blocker);

    auto* aligned = arena.allocate(48, 256);
    assert(aligned != nullptr);
    assert(reinterpret_cast<std::uintptr_t>(aligned) % 256 == 0);
    assert(arena.allocate(16, 3) == nullptr);
    arena.release(aligned);

    auto* zeroed = static_cast<unsigned char*>(arena.allocateZeroed(32, 2));
    assert(zeroed != nullptr);
    for (std::size_t i = 0; i < 64; ++i) {
        assert(zeroed[i] == 0);
    }
    assert(arena.allocateZeroed(std::numeric_limits<std::size_t>::max(), 2) == nullptr);
    void* zeroedEmpty = arena.allocateZeroed(0, 0);
    assert(zeroedEmpty != nullptr);
    arena.release(zeroedEmpty);
    assert(arena.allocate(std::numeric_limits<std::size_t>::max()) == nullptr);

    void* zeroSize = arena.allocate(0);
    assert(zeroSize != nullptr);
    arena.release(zeroSize);
    void* zeroFromNull = arena.reallocate(nullptr, 0);
    assert(zeroFromNull != nullptr);
    arena.release(zeroFromNull);

    auto* zeroSizedRealloc = arena.allocate(48);
    assert(zeroSizedRealloc != nullptr);
    assert(arena.reallocate(zeroSizedRealloc, 0) == nullptr);

    micant::ucrt::BoundedArena<2048> coalescing{};
    void* first = coalescing.allocate(96);
    void* second = coalescing.allocate(96);
    void* retained = coalescing.allocate(96);
    assert(first && second && retained);
    coalescing.release(first);
    coalescing.release(second);
    assert(coalescing.allocate(192) != nullptr);

    micant::ucrt::BoundedArena<1024> exhausted{};
    auto* preserved = static_cast<unsigned char*>(exhausted.allocate(400));
    assert(preserved != nullptr);
    preserved[0] = 0xA5;
    assert(exhausted.allocate(400) != nullptr);
    assert(exhausted.reallocate(preserved, 900) == nullptr);
    assert(preserved[0] == 0xA5);

    exhausted.release(reinterpret_cast<void*>(static_cast<std::uintptr_t>(1)));
    assert(preserved[0] == 0xA5);

    micant::ucrt::BoundedArena<1024> errnoHeap{};
    int error = 77;
    void* zeroLength = micant::ucrt::detail::allocateWithErrno(errnoHeap, 0, error);
    assert(zeroLength != nullptr && error == 77);
    void* zeroedSuccess =
        micant::ucrt::detail::allocateZeroedWithErrno(errnoHeap, 4, 4, error);
    assert(zeroedSuccess != nullptr && error == 77);
    assert(micant::ucrt::detail::allocateZeroedWithErrno(
               errnoHeap, std::numeric_limits<std::size_t>::max(), 2, error) == nullptr);
    assert(error == micant::ucrt::detail::kErrnoNoMemory);

    error = 91;
    void* zeroedNullRealloc =
        micant::ucrt::detail::reallocateWithErrno(errnoHeap, nullptr, 0, error);
    assert(zeroedNullRealloc != nullptr && error == 91);
    assert(micant::ucrt::detail::reallocateWithErrno(
               errnoHeap, zeroedNullRealloc, 0, error) == nullptr);
    assert(error == 91);

    error = 103;
    void* errnoFirst =
        micant::ucrt::detail::allocateWithErrno(errnoHeap, 400, error);
    void* errnoSecond =
        micant::ucrt::detail::allocateWithErrno(errnoHeap, 400, error);
    assert(errnoFirst && errnoSecond && error == 103);
    error = 111;
    assert(micant::ucrt::detail::reallocateWithErrno(
               errnoHeap, errnoFirst, 200, error) == errnoFirst);
    assert(error == 111);
    assert(micant::ucrt::detail::allocateWithErrno(
               errnoHeap, std::numeric_limits<std::size_t>::max(), error) == nullptr);
    assert(error == micant::ucrt::detail::kErrnoNoMemory);

    error = 123;
    void* alignedWithErrno =
        micant::ucrt::detail::allocateAlignedWithErrno(errnoHeap, 48, 256, error);
    assert(alignedWithErrno != nullptr && error == 123);
    assert(reinterpret_cast<std::uintptr_t>(alignedWithErrno) % 256 == 0);
    errnoHeap.release(alignedWithErrno);

    error = 124;
    assert(micant::ucrt::detail::allocateAlignedWithErrno(
               errnoHeap, 48, 0, error) == nullptr);
    assert(error == micant::ucrt::detail::kErrnoInvalidArgument);
    error = 125;
    assert(micant::ucrt::detail::allocateAlignedWithErrno(
               errnoHeap, 0, 16, error) == nullptr);
    assert(error == micant::ucrt::detail::kErrnoInvalidArgument);

    error = 126;
    assert(micant::ucrt::detail::allocateAlignedWithErrno(
               exhausted, 900, 64, error) == nullptr);
    assert(error == micant::ucrt::detail::kErrnoNoMemory);

    error = 117;
    assert(micant::ucrt::detail::reallocateWithErrno(
               errnoHeap, errnoFirst, 900, error) == nullptr);
    assert(errnoFirst != nullptr && error == micant::ucrt::detail::kErrnoNoMemory);
}
