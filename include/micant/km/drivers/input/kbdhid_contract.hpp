#pragma once

#include <cstddef>
#include <cstdint>

namespace micant::km::hid {
struct _HIDP_PREPARSED_DATA;
using PHIDP_PREPARSED_DATA = _HIDP_PREPARSED_DATA*;
void KbdHidProcessInputReport(uintptr_t collectionId, PHIDP_PREPARSED_DATA preparsedData,
    const uint8_t* report, size_t reportLength);
} // namespace micant::km::hid
