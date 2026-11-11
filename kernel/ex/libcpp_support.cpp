#include "micant/km/executive/freestanding.hpp"
#include <__verbose_abort>

_LIBCPP_BEGIN_NAMESPACE_STD

[[noreturn]] void __libcpp_verbose_abort(const char* format, ...) noexcept {
    // Fail closed on library contract violations; no hosted printf or unwind.
    // Log the original format verbatim rather than claiming formatted output.
    micant::ex::stopBootstrapRuntime(format ? format : "C++ library contract violation");
}

_LIBCPP_END_NAMESPACE_STD
