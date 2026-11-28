#pragma once
// Canonical freestanding Ring-0 <-> Ring-3 Directory Enumeration ABI

#include <cstdint>

namespace micant::abi {

struct NtDirEntryInfo {
    uint64_t fileSize;
    uint32_t fileAttributes;
    uint32_t nameLengthChars;
    uint16_t fileName[128];
};

static_assert(sizeof(NtDirEntryInfo) == 272, "NtDirEntryInfo ABI size mismatch");

} // namespace micant::abi
