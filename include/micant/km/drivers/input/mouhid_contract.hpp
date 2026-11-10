#pragma once

#include <cstddef>
#include <cstdint>

namespace micant::km::hid {
struct _HIDP_PREPARSED_DATA;

void MouHidProcessInputReport(uintptr_t collectionId, _HIDP_PREPARSED_DATA* preparsedData,
    const uint8_t* report, size_t reportLength);
}
