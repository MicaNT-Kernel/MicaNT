#include "micant/um/subsystems/winbase/utf8_conversion.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>

using micant::um::winbase::utf8::Error;
using micant::um::winbase::utf8::to_utf16;
using micant::um::winbase::utf8::to_utf8;

int main() {
    constexpr char input[] = "A\xE2\x82\xAC\xF0\x9F\x8C\x8D";
    const auto measured = to_utf16(input, -1, nullptr, 0, true);
    assert(measured.error == Error::none && measured.count == 5);

    uint16_t wide[6]{};
    const auto converted = to_utf16(input, -1, wide, 6, true);
    assert(converted.error == Error::none && converted.count == 5);
    assert(wide[0] == 'A' && wide[1] == 0x20AC);
    assert(wide[2] == 0xD83C && wide[3] == 0xDF0D && wide[4] == 0);

    char output[10]{};
    const auto round_trip = to_utf8(wide, -1, output, sizeof(output), true);
    assert(round_trip.error == Error::none);
    assert(round_trip.count == static_cast<int>(sizeof(input)));
    assert(std::memcmp(input, output, sizeof(input)) == 0);

    const char malformed[] = "\xC0\xAF";
    assert(to_utf16(malformed, 2, wide, 6, true).error == Error::no_unicode_translation);
    const auto replacement = to_utf16(malformed, 2, wide, 6, false);
    assert(replacement.error == Error::none && replacement.count == 2);
    assert(wide[0] == 0xFFFD && wide[1] == 0xFFFD);
    assert(to_utf16(input, -1, wide, 1, true).error == Error::insufficient_buffer);

    const uint16_t unpaired[] = {0xD800};
    assert(to_utf8(unpaired, 1, output, sizeof(output), true).error == Error::no_unicode_translation);
    assert(to_utf8(unpaired, 1, output, sizeof(output), false).error == Error::none);
    assert(static_cast<uint8_t>(output[0]) == 0xEF &&
           static_cast<uint8_t>(output[1]) == 0xBF &&
           static_cast<uint8_t>(output[2]) == 0xBD);
}
