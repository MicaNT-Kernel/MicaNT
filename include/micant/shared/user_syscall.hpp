#pragma once

#include <cstdint>

namespace micant::sys {

// Private service transport; service numbers are not a public Windows ABI.
inline uint64_t invokeUserService(uint64_t number, uint64_t first = 0,
                                 uint64_t second = 0) noexcept {
#if defined(__x86_64__)
    register uint64_t argument asm("r10") = first;
    asm volatile("syscall"
                 : "+a"(number), "+r"(argument), "+d"(second)
                 :
                 : "rcx", "r11", "memory", "cc");
    return number;
#else
    return 0xC00000BB; // STATUS_NOT_SUPPORTED
#endif
}

} // namespace micant::sys
