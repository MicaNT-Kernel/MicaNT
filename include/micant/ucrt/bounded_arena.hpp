#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

namespace micant::ucrt {

namespace detail {

inline constexpr int kErrnoNoMemory = 12;

template <typename Arena>
[[nodiscard]] void* allocateWithErrno(Arena& arena, std::size_t size,
                                      int& error) noexcept {
    void* result = arena.allocate(size);
    if (!result) {
        error = kErrnoNoMemory;
    }
    return result;
}

template <typename Arena>
[[nodiscard]] void* allocateZeroedWithErrno(Arena& arena, std::size_t count,
                                            std::size_t size,
                                            int& error) noexcept {
    void* result = arena.allocateZeroed(count, size);
    if (!result) {
        error = kErrnoNoMemory;
    }
    return result;
}

template <typename Arena>
[[nodiscard]] void* reallocateWithErrno(Arena& arena, void* pointer,
                                        std::size_t size,
                                        int& error) noexcept {
    if (pointer && size == 0) {
        return arena.reallocate(pointer, size);
    }
    void* result = arena.reallocate(pointer, size);
    if (!result) {
        error = kErrnoNoMemory;
    }
    return result;
}

}

template <std::size_t Capacity>
class BoundedArena {
    static constexpr std::size_t kAlignment = alignof(std::max_align_t);

    struct alignas(std::max_align_t) Block {
        std::size_t span;
        std::size_t requested;
        std::size_t payloadOffset;
        std::size_t alignment;
        bool free;
    };

    static_assert((kAlignment & (kAlignment - 1)) == 0);
    static_assert(Capacity >= sizeof(Block) + kAlignment);

public:
    constexpr BoundedArena() noexcept
        : m_storage{}, m_initialized(false), m_lock(0) {}

    [[nodiscard]] void* allocate(std::size_t size,
                                 std::size_t alignment = kAlignment) noexcept {
        Lock lock{m_lock};
        return allocateUnlocked(size, alignment);
    }

    [[nodiscard]] void* allocateZeroed(std::size_t count,
                                       std::size_t size) noexcept {
        if (size != 0 && count > std::numeric_limits<std::size_t>::max() / size) {
            return nullptr;
        }

        const std::size_t bytes = count * size;
        auto* result = static_cast<unsigned char*>(allocate(bytes == 0 ? 1 : bytes));
        if (!result) {
            return nullptr;
        }
        for (std::size_t i = 0; i < (bytes == 0 ? 1 : bytes); ++i) {
            result[i] = 0;
        }
        return result;
    }

    void release(void* pointer) noexcept {
        if (!pointer) {
            return;
        }

        Lock lock{m_lock};
        Block* previous = nullptr;
        Block* block = findUnlocked(pointer, &previous);
        if (!block) {
            return;
        }

        block->free = true;
        block->requested = 0;
        block = coalesceForward(block);
        if (previous && previous->free) {
            previous->span += block->span;
            block = coalesceForward(previous);
        }
    }

    [[nodiscard]] void* reallocate(void* pointer, std::size_t size) noexcept {
        if (!pointer) {
            return allocate(size);
        }
        if (size == 0) {
            release(pointer);
            return nullptr;
        }

        Lock lock{m_lock};
        Block* block = findUnlocked(pointer, nullptr);
        if (!block) {
            return nullptr;
        }

        std::size_t needed = 0;
        if (!requiredSpan(block, size, block->alignment, needed)) {
            return nullptr;
        }
        if (needed <= block->span) {
            block->requested = size;
            split(block, needed);
            return pointer;
        }

        std::size_t expandedSpan = block->span;
        auto* end = arenaEnd();
        while (expandedSpan < needed) {
            auto* nextAddress =
                reinterpret_cast<unsigned char*>(block) + expandedSpan;
            if (nextAddress >= end) {
                break;
            }
            auto* next = reinterpret_cast<Block*>(nextAddress);
            if (!next->free || next->span < sizeof(Block) ||
                next->span > static_cast<std::size_t>(end - nextAddress)) {
                break;
            }
            expandedSpan += next->span;
        }
        if (needed <= expandedSpan) {
            block->span = expandedSpan;
            block->requested = size;
            split(block, needed);
            return pointer;
        }

        void* replacement = allocateUnlocked(size, block->alignment);
        if (!replacement) {
            return nullptr;
        }
        const auto* oldBytes = static_cast<const unsigned char*>(pointer);
        auto* newBytes = static_cast<unsigned char*>(replacement);
        const std::size_t copied = block->requested < size ? block->requested : size;
        for (std::size_t i = 0; i < copied; ++i) {
            newBytes[i] = oldBytes[i];
        }
        block->free = true;
        block->requested = 0;
        coalesceForward(block);
        return replacement;
    }

private:
    struct Lock {
        explicit Lock(std::uint8_t& value) noexcept : flag(value) {
            while (__atomic_exchange_n(&flag, 1, __ATOMIC_ACQUIRE) != 0) {
            }
        }

        ~Lock() {
            __atomic_store_n(&flag, 0, __ATOMIC_RELEASE);
        }

        std::uint8_t& flag;
    };

    static bool addOverflow(std::size_t left, std::size_t right,
                            std::size_t& result) noexcept {
        if (left > std::numeric_limits<std::size_t>::max() - right) {
            return true;
        }
        result = left + right;
        return false;
    }

    static bool roundUp(std::size_t value, std::size_t alignment,
                        std::size_t& result) noexcept {
        const std::size_t remainder = value & (alignment - 1);
        if (remainder == 0) {
            result = value;
            return true;
        }
        return !addOverflow(value, alignment - remainder, result);
    }

    static bool isPowerOfTwo(std::size_t value) noexcept {
        return value != 0 && (value & (value - 1)) == 0;
    }

    unsigned char* arenaBegin() noexcept {
        return reinterpret_cast<unsigned char*>(m_storage);
    }

    unsigned char* arenaEnd() noexcept {
        return arenaBegin() + (Capacity / kAlignment) * kAlignment;
    }

    void initialize() noexcept {
        if (m_initialized) {
            return;
        }
        auto* first = reinterpret_cast<Block*>(arenaBegin());
        first->span = static_cast<std::size_t>(arenaEnd() - arenaBegin());
        first->requested = 0;
        first->payloadOffset = sizeof(Block);
        first->alignment = kAlignment;
        first->free = true;
        m_initialized = true;
    }

    bool requiredSpan(Block* block, std::size_t size, std::size_t alignment,
                      std::size_t& result) noexcept {
        if (!isPowerOfTwo(alignment)) {
            return false;
        }
        if (alignment < kAlignment) {
            alignment = kAlignment;
        }

        const auto base = reinterpret_cast<std::uintptr_t>(block);
        std::uintptr_t afterHeader = 0;
        if (base > std::numeric_limits<std::uintptr_t>::max() - sizeof(Block)) {
            return false;
        }
        afterHeader = base + sizeof(Block);
        if (alignment - 1 >
            std::numeric_limits<std::uintptr_t>::max() - afterHeader) {
            return false;
        }
        const std::uintptr_t payload =
            (afterHeader + alignment - 1) & ~(static_cast<std::uintptr_t>(alignment) - 1);
        const std::uintptr_t offset = payload - base;
        if (offset > std::numeric_limits<std::size_t>::max()) {
            return false;
        }
        std::size_t used = 0;
        return !addOverflow(static_cast<std::size_t>(offset), size, used) &&
               roundUp(used, kAlignment, result);
    }

    void* allocateUnlocked(std::size_t size, std::size_t alignment) noexcept {
        initialize();
        if (size == 0) {
            size = 1;
        }
        if (!isPowerOfTwo(alignment)) {
            return nullptr;
        }
        if (alignment < kAlignment) {
            alignment = kAlignment;
        }

        auto* cursor = arenaBegin();
        auto* end = arenaEnd();
        while (cursor < end) {
            auto* block = reinterpret_cast<Block*>(cursor);
            if (block->span < sizeof(Block) ||
                block->span > static_cast<std::size_t>(end - cursor)) {
                return nullptr;
            }
            if (block->free) {
                std::size_t needed = 0;
                if (requiredSpan(block, size, alignment, needed) &&
                    needed <= block->span) {
                    block->requested = size;
                    block->alignment = alignment;
                    const auto base = reinterpret_cast<std::uintptr_t>(block);
                    block->payloadOffset = static_cast<std::size_t>(
                        ((base + sizeof(Block) + alignment - 1) &
                         ~(static_cast<std::uintptr_t>(alignment) - 1)) - base);
                    block->free = false;
                    split(block, needed);
                    return cursor + block->payloadOffset;
                }
            }
            cursor += block->span;
        }
        return nullptr;
    }

    Block* findUnlocked(const void* pointer, Block** previous) noexcept {
        initialize();
        const auto target = reinterpret_cast<std::uintptr_t>(pointer);
        const auto begin = reinterpret_cast<std::uintptr_t>(arenaBegin());
        const auto end = reinterpret_cast<std::uintptr_t>(arenaEnd());
        if (target < begin || target >= end) {
            return nullptr;
        }

        auto* cursor = arenaBegin();
        auto* prior = static_cast<Block*>(nullptr);
        auto* arenaEndPointer = arenaEnd();
        while (cursor < arenaEndPointer) {
            auto* block = reinterpret_cast<Block*>(cursor);
            if (block->span < sizeof(Block) ||
                block->span > static_cast<std::size_t>(arenaEndPointer - cursor) ||
                block->payloadOffset > block->span) {
                return nullptr;
            }
            if (!block->free && cursor + block->payloadOffset == pointer) {
                if (previous) {
                    *previous = prior;
                }
                return block;
            }
            prior = block;
            cursor += block->span;
        }
        return nullptr;
    }

    void split(Block* block, std::size_t used) noexcept {
        const std::size_t minimum = sizeof(Block) + kAlignment;
        if (block->span - used < minimum) {
            return;
        }

        const std::size_t remainder = block->span - used;
        auto* next = reinterpret_cast<Block*>(
            reinterpret_cast<unsigned char*>(block) + used);
        next->span = remainder;
        next->requested = 0;
        next->payloadOffset = sizeof(Block);
        next->alignment = kAlignment;
        next->free = true;
        block->span = used;
    }

    Block* coalesceForward(Block* block) noexcept {
        auto* end = arenaEnd();
        for (;;) {
            auto* nextAddress = reinterpret_cast<unsigned char*>(block) + block->span;
            if (nextAddress >= end) {
                return block;
            }
            auto* next = reinterpret_cast<Block*>(nextAddress);
            if (!next->free) {
                return block;
            }
            block->span += next->span;
        }
    }

    alignas(std::max_align_t) unsigned char m_storage[Capacity];
    bool m_initialized;
    std::uint8_t m_lock;
};

} // namespace micant::ucrt
