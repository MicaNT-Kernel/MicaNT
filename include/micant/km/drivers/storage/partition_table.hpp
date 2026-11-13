#pragma once

// Freestanding MBR and UEFI GPT discovery for canonical MicaNT block devices.
// On-disk integers are decoded explicitly; no packed host structures are used.
#include <cstddef>
#include <cstdint>

namespace micant::storage::partition_table {

inline constexpr uint64_t MaximumEntryArrayBytes = 1024 * 1024;

enum class TableKind : uint8_t { None, Mbr, Gpt };
enum class Status : uint8_t { Success, InvalidArgument, IoError, Corrupt, TooManyPartitions };
struct Extent {
    uint64_t firstLba{};
    uint64_t sectorCount{};
    uint8_t mbrType{};
    uint8_t typeGuid[16]{};
    uint8_t uniqueGuid[16]{};
    uint64_t attributes{};
    char16_t name[36]{};
    bool bootable{};
};
struct Result { TableKind kind{TableKind::None}; size_t count{}; };
using Read = bool (*)(void*, uint64_t, uint16_t, void*) noexcept;
struct Device {
    void* context{}; Read read{}; uint64_t sectorCount{}; uint32_t sectorSize{512};
    uint8_t* scratch{}; size_t scratchBytes{}; // caller-owned; at least 8192 bytes
};

inline uint16_t le16(const uint8_t* p) noexcept { return uint16_t(p[0]) | (uint16_t(p[1]) << 8); }
inline uint32_t le32(const uint8_t* p) noexcept { return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24); }
inline uint64_t le64(const uint8_t* p) noexcept { return uint64_t(le32(p)) | (uint64_t(le32(p + 4)) << 32); }
inline uint32_t crc32Update(uint32_t crc, const uint8_t* p, size_t n) noexcept {
    for (size_t i = 0; i < n; ++i) { crc ^= p[i]; for (unsigned b = 0; b < 8; ++b) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u))); }
    return crc;
}

inline Status parseMbr(const Device& d, Extent* out, size_t capacity, Result& result) noexcept {
    result = {};
    if (!d.read || !out || !capacity || !d.scratch || d.scratchBytes < 8192 ||
        d.sectorSize < 512 || d.sectorSize > 4096 || !d.sectorCount) return Status::InvalidArgument;
    uint8_t* sector = d.scratch;
    if (!d.read(d.context, 0, 1, sector)) return Status::IoError;
    if (sector[510] != 0x55 || sector[511] != 0xAA) return Status::Corrupt;
    struct Container { uint64_t first; uint64_t count; } extended[4]{};
    uint64_t primaryStarts[4]{}, primaryEnds[4]{};
    uint8_t primaryTypes[4]{};
    size_t extendedCount = 0, n = 0;
    for (size_t i = 0; i < 4; ++i) {
        const uint8_t* p = sector + 446 + i * 16;
        const uint8_t type = p[4]; const uint64_t first = le32(p + 8), count = le32(p + 12);
        if (!type && !count) continue;
        if (!type || !count || (p[0] != 0 && p[0] != 0x80) || first == 0 || first >= d.sectorCount || count > d.sectorCount - first) return Status::Corrupt;
        for (size_t j = 0; j < i; ++j)
            if (primaryEnds[j] && first < primaryEnds[j] && primaryStarts[j] < first + count) return Status::Corrupt;
        primaryStarts[i] = first; primaryEnds[i] = first + count; primaryTypes[i] = type;
        if (type == 0x05 || type == 0x0F || type == 0x85) {
            extended[extendedCount++] = {first, count};
            continue;
        }
        if (n == capacity) return Status::TooManyPartitions;
        out[n] = {}; out[n].firstLba = first; out[n].sectorCount = count; out[n].mbrType = type; out[n].bootable = p[0] == 0x80; ++n;
    }
    for (size_t containerIndex = 0; containerIndex < extendedCount; ++containerIndex) {
        const uint64_t base = extended[containerIndex].first;
        const uint64_t limit = base + extended[containerIndex].count;
        uint64_t ebrLba = base, visited[128]{}; size_t visitedCount = 0;
        for (;;) {
            if (visitedCount == sizeof(visited) / sizeof(visited[0])) return Status::TooManyPartitions;
            if (ebrLba < base || ebrLba >= limit) return Status::Corrupt;
            for (size_t i = 0; i < visitedCount; ++i) if (visited[i] == ebrLba) return Status::Corrupt;
            visited[visitedCount++] = ebrLba;
            if (!d.read(d.context, ebrLba, 1, sector)) return Status::IoError;
            if (sector[510] != 0x55 || sector[511] != 0xAA) return Status::Corrupt;
            const uint8_t* logical = sector + 446;
            const uint8_t* link = logical + 16;
            const uint8_t logicalType = logical[4];
            const uint64_t relative = le32(logical + 8), count = le32(logical + 12);
            uint64_t logicalFirst = 0, logicalCount = 0;
            if (logicalType || count) {
                if (!logicalType || !count || logicalType == 0x05 || logicalType == 0x0F || logicalType == 0x85 ||
                    (logical[0] != 0 && logical[0] != 0x80) || relative == 0 || relative > UINT64_MAX - ebrLba)
                    return Status::Corrupt;
                logicalFirst = ebrLba + relative; logicalCount = count;
                if (logicalFirst < base || logicalFirst >= limit || count > limit - logicalFirst || count > d.sectorCount - logicalFirst)
                    return Status::Corrupt;
                const uint64_t end = logicalFirst + count;
                for (size_t i = 0; i < 4; ++i)
                    if (primaryEnds[i] && primaryTypes[i] != 0x05 && primaryTypes[i] != 0x0F && primaryTypes[i] != 0x85 &&
                        logicalFirst < primaryEnds[i] && primaryStarts[i] < end) return Status::Corrupt;
                for (size_t i = 0; i < n; ++i)
                    if (logicalFirst < out[i].firstLba + out[i].sectorCount && out[i].firstLba < end) return Status::Corrupt;
                if (n == capacity) return Status::TooManyPartitions;
                out[n] = {}; out[n].firstLba = logicalFirst; out[n].sectorCount = count;
                out[n].mbrType = logicalType; out[n].bootable = logical[0] == 0x80; ++n;
            }
            for (size_t i = 2; i < 4; ++i) {
                const uint8_t* extra = sector + 446 + i * 16;
                for (size_t b = 0; b < 16; ++b) if (extra[b]) return Status::Corrupt;
            }
            const uint8_t linkType = link[4]; const uint64_t linkRelative = le32(link + 8), linkCount = le32(link + 12);
            if (!linkType && !linkCount) break;
            if ((linkType != 0x05 && linkType != 0x0F && linkType != 0x85) || !linkCount || !linkRelative ||
                linkRelative > limit - base || linkCount > extended[containerIndex].count - linkRelative ||
                base > UINT64_MAX - linkRelative) return Status::Corrupt;
            ebrLba = base + linkRelative;
            if (ebrLba >= limit || (logicalCount && ebrLba >= logicalFirst && ebrLba < logicalFirst + logicalCount)) return Status::Corrupt;
            for (size_t i = 0; i < n; ++i)
                if (ebrLba >= out[i].firstLba && ebrLba < out[i].firstLba + out[i].sectorCount) return Status::Corrupt;
        }
    }
    result = {TableKind::Mbr, n}; return Status::Success;
}

inline Status validateGptAt(const Device& d, uint64_t headerLba, Extent* out, size_t capacity, Result& result) noexcept {
    result = {};
    if (!d.read || !out || !capacity || !d.sectorCount || headerLba >= d.sectorCount ||
        d.sectorSize < 512 || d.sectorSize > 4096) return Status::InvalidArgument;
    if (!d.scratch || d.scratchBytes < 8192) return Status::InvalidArgument;
    uint8_t* sector = d.scratch;
    uint8_t* entry = d.scratch + 4096;
    if (!d.read(d.context, headerLba, 1, sector)) return Status::IoError;
    static constexpr uint8_t signature[8] = {'E','F','I',' ','P','A','R','T'};
    for (size_t i = 0; i < 8; ++i) if (sector[i] != signature[i]) return Status::Corrupt;
    const uint32_t headerSize = le32(sector + 12), expectedHeaderCrc = le32(sector + 16);
    const uint64_t current = le64(sector + 24), backup = le64(sector + 32);
    const uint64_t firstUsable = le64(sector + 40), lastUsable = le64(sector + 48), arrayLba = le64(sector + 72);
    const uint32_t entryCount = le32(sector + 80), entrySize = le32(sector + 84), expectedArrayCrc = le32(sector + 88);
    const uint32_t revision = le32(sector + 8), reserved = le32(sector + 20);
    if (headerSize < 92 || headerSize > d.sectorSize || revision < 0x00010000u || reserved != 0 || current != headerLba ||
        backup != (headerLba == 1 ? d.sectorCount - 1 : 1) || backup >= d.sectorCount || backup == current ||
        firstUsable > lastUsable || lastUsable >= d.sectorCount || !entryCount || entrySize < 128 || (entrySize & 7u) || entrySize > 4096) return Status::Corrupt;
    const uint32_t savedCrc = le32(sector + 16);
    for (size_t i = 16; i < 20; ++i) sector[i] = 0;
    const uint32_t actualHeaderCrc = crc32Update(0xFFFFFFFFu, sector, headerSize) ^ 0xFFFFFFFFu;
    for (size_t i = 0; i < 4; ++i) sector[16 + i] = static_cast<uint8_t>(savedCrc >> (i * 8));
    if (actualHeaderCrc != expectedHeaderCrc) return Status::Corrupt;
    const uint64_t arrayBytes = uint64_t(entryCount) * entrySize;
    if (arrayBytes / entrySize != entryCount || arrayBytes > MaximumEntryArrayBytes)
        return Status::Corrupt;
    const uint64_t arraySectors = arrayBytes / d.sectorSize + (arrayBytes % d.sectorSize != 0);
    if (!arraySectors || arrayLba >= d.sectorCount || arraySectors > d.sectorCount - arrayLba) return Status::Corrupt;
    const uint64_t arrayEnd = arrayLba + arraySectors;
    if ((headerLba == 1 && (arrayLba <= current || arrayEnd > firstUsable)) ||
        (headerLba != 1 && (arrayLba <= lastUsable || arrayEnd > current))) return Status::Corrupt;
    // GPT arrays are streamed one sector at a time to keep kernel memory bounded.
    uint32_t entryFill = 0, crc = 0xFFFFFFFFu; uint64_t consumed = 0; size_t found = 0;
    for (uint64_t si = 0; si < arraySectors; ++si) {
        if (!d.read(d.context, arrayLba + si, 1, sector)) return Status::IoError;
        const size_t valid = (arrayBytes - consumed < d.sectorSize) ? static_cast<size_t>(arrayBytes - consumed) : d.sectorSize;
        crc = crc32Update(crc, sector, valid);
        for (size_t k = 0; k < valid; ++k) {
            entry[entryFill++] = sector[k];
            if (entryFill == entrySize) {
                bool used = false; for (size_t b = 0; b < 16; ++b) used |= entry[b] != 0;
                if (used) {
                    const uint64_t first = le64(entry + 32), last = le64(entry + 40);
                    if (first < firstUsable || last > lastUsable || first > last) return Status::Corrupt;
                    if (found == capacity) return Status::TooManyPartitions;
                    const uint64_t count = last - first + 1;
                    if (count == 0) return Status::Corrupt;
                    for (size_t prior = 0; prior < found; ++prior) {
                        const uint64_t priorEnd = out[prior].firstLba + out[prior].sectorCount;
                        if (first < priorEnd && out[prior].firstLba < last + 1) return Status::Corrupt;
                    }
                    out[found] = {}; out[found].firstLba = first; out[found].sectorCount = count;
                    for (size_t b = 0; b < 16; ++b) { out[found].typeGuid[b] = entry[b]; out[found].uniqueGuid[b] = entry[16 + b]; }
                    for (size_t unit = 0; unit < 36; ++unit)
                        out[found].name[unit] = static_cast<char16_t>(le16(entry + 56 + unit * 2));
                    out[found].attributes = le64(entry + 48); ++found;
                }
                entryFill = 0;
            }
        }
        consumed += valid;
    }
    if (entryFill || (crc ^ 0xFFFFFFFFu) != expectedArrayCrc) return Status::Corrupt;
    result = {TableKind::Gpt, found}; return Status::Success;
}

inline Status discover(const Device& d, Extent* out, size_t capacity, Result& result) noexcept {
    result = {};
    if (!d.read || !out || !capacity || !d.scratch || d.scratchBytes < 8192 ||
        d.sectorSize < 512 || d.sectorSize > 4096 || d.sectorCount < 2) return Status::InvalidArgument;
    uint8_t* mbr = d.scratch; if (!d.read(d.context, 0, 1, mbr)) return Status::IoError;
    const bool mbrSignature = mbr[510] == 0x55 && mbr[511] == 0xAA;
    bool protective = false;
    if (mbrSignature) for (size_t i = 0; i < 4; ++i) protective |= mbr[446 + i * 16 + 4] == 0xEE;
    Status primary = validateGptAt(d, 1, out, capacity, result);
    if (primary == Status::Success) return primary;
    const Status backup = validateGptAt(d, d.sectorCount - 1, out, capacity, result);
    if (backup == Status::Success) return backup;
    if (protective) return backup == Status::IoError ? backup : Status::Corrupt;
    if (!mbrSignature) { result = {}; return Status::Success; }
    return parseMbr(d, out, capacity, result);
}

} // namespace micant::storage::partition_table
