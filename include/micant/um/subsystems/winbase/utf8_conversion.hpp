#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

namespace micant::um::winbase::utf8 {

enum class Error : uint8_t {
    none,
    invalid_parameter,
    no_unicode_translation,
    insufficient_buffer,
};

struct Result {
    int count;
    Error error;
};

struct Scalar {
    uint32_t value;
    size_t consumed;
    bool valid;
};

inline Scalar decode(const uint8_t* input, size_t remaining) noexcept {
    const uint8_t first = input[0];
    if (first < 0x80) return {first, 1, true};

    size_t length = 0;
    uint32_t value = 0;
    uint32_t minimum = 0;
    if (first >= 0xC2 && first <= 0xDF) {
        length = 2;
        value = first & 0x1FU;
        minimum = 0x80;
    } else if (first >= 0xE0 && first <= 0xEF) {
        length = 3;
        value = first & 0x0FU;
        minimum = 0x800;
    } else if (first >= 0xF0 && first <= 0xF4) {
        length = 4;
        value = first & 0x07U;
        minimum = 0x10000;
    } else {
        return {0xFFFD, 1, false};
    }

    if (length > remaining) return {0xFFFD, 1, false};
    for (size_t index = 1; index < length; ++index) {
        const uint8_t next = input[index];
        if ((next & 0xC0U) != 0x80U) return {0xFFFD, 1, false};
        value = (value << 6) | (next & 0x3FU);
    }
    if (value < minimum || value > 0x10FFFF ||
        (value >= 0xD800 && value <= 0xDFFF)) {
        return {0xFFFD, 1, false};
    }
    return {value, length, true};
}

inline bool source_length(const char* source, int requested, size_t& length) noexcept {
    if (!source || requested == 0 || requested < -1) return false;
    if (requested > 0) {
        length = static_cast<size_t>(requested);
        return true;
    }
    length = 0;
    while (source[length] != '\0') {
        if (length == static_cast<size_t>(std::numeric_limits<int>::max() - 1)) return false;
        ++length;
    }
    ++length;
    return true;
}

inline Result to_utf16(const char* source, int source_count, uint16_t* output,
                       int output_capacity, bool strict) noexcept {
    size_t length = 0;
    if (!source_length(source, source_count, length) || output_capacity < 0) {
        return {0, Error::invalid_parameter};
    }

    size_t required = 0;
    for (size_t offset = 0; offset < length;) {
        const Scalar scalar = decode(
            reinterpret_cast<const uint8_t*>(source) + offset, length - offset);
        if (!scalar.valid && strict) return {0, Error::no_unicode_translation};
        const size_t units = scalar.value > 0xFFFF ? 2 : 1;
        if (required > static_cast<size_t>(std::numeric_limits<int>::max()) - units) {
            return {0, Error::invalid_parameter};
        }
        required += units;
        offset += scalar.consumed;
    }
    if (required > static_cast<size_t>(std::numeric_limits<int>::max())) {
        return {0, Error::invalid_parameter};
    }
    if (!output && output_capacity == 0) return {static_cast<int>(required), Error::none};
    if (!output || static_cast<size_t>(output_capacity) < required) {
        return {0, output_capacity == 0 ? Error::invalid_parameter : Error::insufficient_buffer};
    }

    size_t written = 0;
    for (size_t offset = 0; offset < length;) {
        const Scalar scalar = decode(
            reinterpret_cast<const uint8_t*>(source) + offset, length - offset);
        uint32_t value = scalar.value;
        if (value <= 0xFFFF) {
            output[written++] = static_cast<uint16_t>(value);
        } else {
            value -= 0x10000;
            output[written++] = static_cast<uint16_t>(0xD800U + (value >> 10));
            output[written++] = static_cast<uint16_t>(0xDC00U + (value & 0x3FFU));
        }
        offset += scalar.consumed;
    }
    return {static_cast<int>(written), Error::none};
}

inline Result to_utf8(const uint16_t* source, int source_count, char* output,
                      int output_capacity, bool strict) noexcept {
    if (!source || source_count == 0 || source_count < -1 || output_capacity < 0) {
        return {0, Error::invalid_parameter};
    }
    size_t length = 0;
    if (source_count > 0) {
        length = static_cast<size_t>(source_count);
    } else {
        while (source[length] != 0) {
            if (length == static_cast<size_t>(std::numeric_limits<int>::max() - 1)) {
                return {0, Error::invalid_parameter};
            }
            ++length;
        }
        ++length;
    }

    size_t required = 0;
    for (size_t offset = 0; offset < length;) {
        uint32_t value = source[offset++];
        if (value >= 0xD800 && value <= 0xDBFF) {
            if (offset < length && source[offset] >= 0xDC00 && source[offset] <= 0xDFFF) {
                value = 0x10000 + ((value - 0xD800) << 10) + (source[offset++] - 0xDC00);
            } else if (strict) {
                return {0, Error::no_unicode_translation};
            } else {
                value = 0xFFFD;
            }
        } else if (value >= 0xDC00 && value <= 0xDFFF) {
            if (strict) return {0, Error::no_unicode_translation};
            value = 0xFFFD;
        }
        const size_t bytes = value < 0x80 ? 1 : value < 0x800 ? 2 : value < 0x10000 ? 3 : 4;
        if (required > static_cast<size_t>(std::numeric_limits<int>::max()) - bytes) {
            return {0, Error::invalid_parameter};
        }
        required += bytes;
    }
    if (!output && output_capacity == 0) return {static_cast<int>(required), Error::none};
    if (!output || static_cast<size_t>(output_capacity) < required) {
        return {0, output_capacity == 0 ? Error::invalid_parameter : Error::insufficient_buffer};
    }

    size_t written = 0;
    for (size_t offset = 0; offset < length;) {
        uint32_t value = source[offset++];
        if (value >= 0xD800 && value <= 0xDBFF && offset < length &&
            source[offset] >= 0xDC00 && source[offset] <= 0xDFFF) {
            value = 0x10000 + ((value - 0xD800) << 10) + (source[offset++] - 0xDC00);
        } else if ((value >= 0xD800 && value <= 0xDFFF) && !strict) {
            value = 0xFFFD;
        }
        if (value < 0x80) {
            output[written++] = static_cast<char>(value);
        } else if (value < 0x800) {
            output[written++] = static_cast<char>(0xC0U | (value >> 6));
            output[written++] = static_cast<char>(0x80U | (value & 0x3FU));
        } else if (value < 0x10000) {
            output[written++] = static_cast<char>(0xE0U | (value >> 12));
            output[written++] = static_cast<char>(0x80U | ((value >> 6) & 0x3FU));
            output[written++] = static_cast<char>(0x80U | (value & 0x3FU));
        } else {
            output[written++] = static_cast<char>(0xF0U | (value >> 18));
            output[written++] = static_cast<char>(0x80U | ((value >> 12) & 0x3FU));
            output[written++] = static_cast<char>(0x80U | ((value >> 6) & 0x3FU));
            output[written++] = static_cast<char>(0x80U | (value & 0x3FU));
        }
    }
    return {static_cast<int>(written), Error::none};
}

} // namespace micant::um::winbase::utf8
