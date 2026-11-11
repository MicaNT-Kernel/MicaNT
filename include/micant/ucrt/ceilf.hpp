#pragma once

#include <bit>
#include <cstdint>

namespace micant::ucrt {

[[nodiscard]] inline float ceilf(float value) noexcept {
    uint32_t bits = std::bit_cast<uint32_t>(value);
    const uint32_t magnitude = bits & 0x7FFFFFFFu;
    const uint32_t exponent = (magnitude >> 23) & 0xFFu;
    if (exponent == 0xFFu || magnitude == 0) {
        return value;
    }

    const int unbiasedExponent = static_cast<int>(exponent) - 127;
    if (unbiasedExponent >= 23) {
        return value;
    }
    if (unbiasedExponent < 0) {
        return (bits >> 31) ? std::bit_cast<float>(0x80000000u) : 1.0f;
    }

    const uint32_t fractionalMask =
        (uint32_t{1} << (23 - unbiasedExponent)) - 1;
    if ((bits & fractionalMask) == 0) {
        return value;
    }

    if (bits >> 31) {
        bits &= ~fractionalMask;
    } else {
        bits = (bits & ~fractionalMask) +
               (uint32_t{1} << (23 - unbiasedExponent));
    }
    return std::bit_cast<float>(bits);
}

} // namespace micant::ucrt
