#include "micant/km/executive/freestanding.hpp"
#include <__verbose_abort>

_LIBCPP_BEGIN_NAMESPACE_STD

[[noreturn]] void __libcpp_verbose_abort(const char* format, ...) noexcept {
    // Fail closed on library contract violations; no hosted printf or unwind.
    // Log the original format verbatim rather than claiming formatted output.
    micant::ex::stopBootstrapRuntime(format ? format : "C++ library contract violation");
}

_LIBCPP_END_NAMESPACE_STD

extern "C" __attribute__((naked)) void ___chkstk_ms() noexcept {
    __asm__ __volatile__(
        "leaq 8(%rsp), %r11\n"
        "movq %rax, %r10\n"
        "1:\n"
        "cmpq $4096, %r10\n"
        "jb 2f\n"
        "subq $4096, %r11\n"
        "testb $0, (%r11)\n"
        "subq $4096, %r10\n"
        "jmp 1b\n"
        "2:\n"
        "subq %r10, %r11\n"
        "testb $0, (%r11)\n"
        "retq\n");
}
