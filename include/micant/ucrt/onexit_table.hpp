#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

namespace micant::ucrt {

using OnExitRoutine = int (*)(void);

struct OnExitTable {
    OnExitRoutine* _first{};
    OnExitRoutine* _last{};
    OnExitRoutine* _end{};
};

[[nodiscard]] int32_t initializeOnExitTable(OnExitTable* table) noexcept {
    if (!table) {
        return -1;
    }
    *table = {};
    return 0;
}

template <typename Arena>
[[nodiscard]] int32_t registerOnExitFunction(
    OnExitTable* table, OnExitRoutine function, Arena& arena) noexcept {
    if (!table || !function) {
        return -1;
    }

    std::size_t count = 0;
    std::size_t capacity = 0;
    if (table->_first) {
        if (!table->_last || !table->_end) {
            return -1;
        }
        const auto first = reinterpret_cast<std::uintptr_t>(table->_first);
        const auto last = reinterpret_cast<std::uintptr_t>(table->_last);
        const auto end = reinterpret_cast<std::uintptr_t>(table->_end);
        if (last < first || end < last ||
            (last - first) % sizeof(OnExitRoutine) != 0 ||
            (end - first) % sizeof(OnExitRoutine) != 0) {
            return -1;
        }
        count = static_cast<std::size_t>((last - first) / sizeof(OnExitRoutine));
        capacity = static_cast<std::size_t>((end - first) / sizeof(OnExitRoutine));
    } else if (table->_last || table->_end) {
        return -1;
    }

    if (count == capacity) {
        constexpr std::size_t initialCapacity = 8;
        const std::size_t maxCapacity =
            std::numeric_limits<std::size_t>::max() / sizeof(OnExitRoutine);
        if (capacity > maxCapacity / 2) {
            return -1;
        }
        const std::size_t newCapacity =
            capacity == 0 ? initialCapacity : capacity * 2;
        if (newCapacity > maxCapacity) {
            return -1;
        }
        auto* storage = static_cast<OnExitRoutine*>(arena.reallocate(
            table->_first, newCapacity * sizeof(OnExitRoutine)));
        if (!storage) {
            return -1;
        }
        table->_first = storage;
        table->_last = storage + count;
        table->_end = storage + newCapacity;
    }

    *table->_last++ = function;
    return 0;
}

template <typename Arena>
[[nodiscard]] int32_t executeOnExitTable(
    OnExitTable* table, Arena& arena) noexcept {
    if (!table) {
        return -1;
    }
    if (!table->_first && !table->_last && !table->_end) {
        return 0;
    }
    if (!table->_first || !table->_last || !table->_end) {
        return -1;
    }

    const auto first = reinterpret_cast<std::uintptr_t>(table->_first);
    auto last = reinterpret_cast<std::uintptr_t>(table->_last);
    const auto end = reinterpret_cast<std::uintptr_t>(table->_end);
    if (last < first || end < last ||
        (last - first) % sizeof(OnExitRoutine) != 0 ||
        (end - first) % sizeof(OnExitRoutine) != 0) {
        return -1;
    }

    while (last > first) {
        const OnExitRoutine function = *--table->_last;
        last -= sizeof(OnExitRoutine);
        if (function) {
            (void)function();
        }
    }
    arena.release(table->_first);
    *table = {};
    return 0;
}

inline void initializeFunctionTable(
    void (**first)(void), void (**last)(void)) noexcept {
    if (!first || !last) {
        return;
    }
    for (auto function = first; function < last; ++function) {
        if (*function) {
            (*function)();
        }
    }
}

inline int32_t initializeFunctionTableWithStatus(
    int32_t (**first)(void), int32_t (**last)(void)) noexcept {
    if (!first || !last) {
        return 0;
    }
    for (auto function = first; function < last; ++function) {
        if (*function) {
            const int32_t status = (*function)();
            if (status != 0) {
                return status;
            }
        }
    }
    return 0;
}

} // namespace micant::ucrt
