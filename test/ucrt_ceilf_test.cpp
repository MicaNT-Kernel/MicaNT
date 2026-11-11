#include "micant/ucrt/ceilf.hpp"

#include <cassert>
#include <cmath>
#include <limits>

int main() {
    using micant::ucrt::ceilf;

    assert(ceilf(0.0f) == 0.0f && !std::signbit(ceilf(0.0f)));
    assert(ceilf(-0.0f) == 0.0f && std::signbit(ceilf(-0.0f)));
    assert(ceilf(0.25f) == 1.0f);
    assert(ceilf(-0.25f) == 0.0f && std::signbit(ceilf(-0.25f)));
    assert(ceilf(1.0f) == 1.0f);
    assert(ceilf(1.25f) == 2.0f);
    assert(ceilf(-1.25f) == -1.0f);
    assert(ceilf(3.75f) == 4.0f);
    assert(ceilf(-3.75f) == -3.0f);
    assert(ceilf(std::numeric_limits<float>::max()) ==
           std::numeric_limits<float>::max());
    assert(std::isinf(ceilf(std::numeric_limits<float>::infinity())));
    assert(std::isnan(ceilf(std::numeric_limits<float>::quiet_NaN())));
    assert(ceilf(std::numeric_limits<float>::denorm_min()) == 1.0f);
    assert(ceilf(-std::numeric_limits<float>::denorm_min()) == 0.0f &&
           std::signbit(ceilf(-std::numeric_limits<float>::denorm_min())));
}
