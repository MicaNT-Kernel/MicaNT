#include "micant/km/fsrtl/fat_boot.hpp"
#include "micant/km/executive/io_block_device.hpp"
#include "micant/km/executive/io_file.hpp"
#include "micant/km/core/sync_baremetal.hpp"
#include "micant/shared/boot_driver.hpp"
#include <cstring>

namespace micant::fsrtl::fat {
namespace {
constexpr int32_t STATUS_SUCCESS = 0;
constexpr int32_t STATUS_DEVICE_NOT_READY = static_cast<int32_t>(0xC00000A3u);
constexpr int32_t STATUS_DEVICE_DATA_ERROR = static_cast<int32_t>(0xC000009Cu);
constexpr int32_t STATUS_UNRECOGNIZED_VOLUME = static_cast<int32_t>(0xC000014Fu);
constexpr size_t kSectorSize = 512, kMaximumPath = 256;
constexpr uint32_t kMaximumSectorsPerCommand = 128;
struct FatDirEntry {
    char Name[128]{};
    char ShortName[13]{};
    uint32_t FirstCluster{};
    uint32_t Size{};
    bool Directory{};
};

struct FatVolume {
    uint64_t PartitionStart{};
    uint64_t PartitionSectors{};
    uint64_t TotalSectors{};
    uint32_t FatStart{};
    uint32_t DataStart{};
    uint32_t SectorsPerFat{};
    uint32_t RootCluster{};
    uint32_t DataClusterCount{};
    uint32_t CachedFatSector{UINT32_MAX};
    uint16_t ReservedSectors{};
    uint16_t BytesPerSector{};
    uint8_t SectorsPerCluster{};
    uint8_t NumberOfFats{};
    char Drive{};
    bool Mounted{};
    uint8_t FatCache[kSectorSize]{};
};

struct Mount;
struct FileContext { Mount* owner{}; FatVolume* volume{}; FatDirEntry entry{}; bool inUse{}; };
struct Mount { const void* owner{}; io::BlockDeviceObject* device{}; ob::ObjectHeader* reference{}; uint64_t sectors{}, token{}; FatVolume volumes[3]{}; uint32_t volumeCount{}; FileContext files[32]{}; bool offline{}; };
Mount mounts[4]{};
Mount* current{};
uint64_t nextToken{1};
bool initialized{};
sync::detail::BootstrapEvent gate{false, true};
struct Gate { bool held{}; Gate() noexcept : held(gate.wait() == NtStatus::Success) {} ~Gate() { if (held) (void)gate.set(); } };
alignas(512) uint8_t g_sectorBuffer[kSectorSize]{};
alignas(512) uint8_t g_gptEntries[128 * 128]{};
alignas(512) uint8_t g_backupGptEntries[128 * 128]{};
alignas(4096) uint8_t g_clusterBuffer[65536]{};
#define g_volumes (current->volumes)
#define g_volumeCount (current->volumeCount)
uint16_t loadLe16(const uint8_t* bytes) noexcept {
    return static_cast<uint16_t>(bytes[0]) |
           (static_cast<uint16_t>(bytes[1]) << 8);
}

uint32_t loadLe32(const uint8_t* bytes) noexcept {
    return static_cast<uint32_t>(bytes[0]) |
           (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) |
           (static_cast<uint32_t>(bytes[3]) << 24);
}

uint64_t loadLe64(const uint8_t* bytes) noexcept {
    return static_cast<uint64_t>(loadLe32(bytes)) |
           (static_cast<uint64_t>(loadLe32(bytes + 4)) << 32);
}

uint32_t crc32(const uint8_t* bytes, size_t size, uint32_t crc = 0xFFFFFFFFu) noexcept {
    for (size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (uint32_t bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return crc;
}

int32_t readIoBlocks(uint64_t lba, uint32_t count, void* buffer) noexcept {
    if (!current || !current->device || current->offline) return STATUS_DEVICE_NOT_READY;
    uint32_t transferred{};
    const auto status = current->device->read(lba, count, buffer, &transferred);
    return status == NtStatus::Success && transferred == count ? STATUS_SUCCESS :
        (status == NtStatus::Success ? STATUS_DEVICE_DATA_ERROR : static_cast<int32_t>(status));
}
uint32_t sectorsPerCluster(const FatVolume& volume) noexcept {
    return volume.SectorsPerCluster;
}

bool readVolume(FatVolume& volume, uint64_t relativeLba, uint32_t count,
                void* output) noexcept {
    if (!volume.Mounted || !output || count == 0 || relativeLba >= volume.TotalSectors ||
        count > volume.TotalSectors - relativeLba ||
        volume.PartitionStart > UINT64_MAX - relativeLba) return false;
    const int32_t status = readIoBlocks(volume.PartitionStart + relativeLba, count, output);
    return status == STATUS_SUCCESS;
}

bool clusterToLba(const FatVolume& volume, uint32_t cluster,
                  uint32_t& lba) noexcept {
    if (cluster < 2 || cluster - 2 >= volume.DataClusterCount) return false;
    const uint64_t value = static_cast<uint64_t>(volume.DataStart) +
        static_cast<uint64_t>(cluster - 2) * volume.SectorsPerCluster;
    if (value >= volume.TotalSectors || value > UINT32_MAX) return false;
    lba = static_cast<uint32_t>(value);
    return true;
}

uint32_t nextCluster(FatVolume& volume, uint32_t cluster) noexcept {
    if (cluster < 2 || cluster - 2 >= volume.DataClusterCount) return 0x0FFFFFFFu;
    const uint64_t byteOffset = static_cast<uint64_t>(cluster) * 4;
    const uint64_t fatSector64 = static_cast<uint64_t>(volume.FatStart) +
                                 byteOffset / kSectorSize;
    if (fatSector64 >= static_cast<uint64_t>(volume.FatStart) + volume.SectorsPerFat ||
        fatSector64 > UINT32_MAX) return 0x0FFFFFFFu;
    const uint32_t fatSector = static_cast<uint32_t>(fatSector64);
    if (volume.CachedFatSector != fatSector) {
        if (!readVolume(volume, fatSector, 1, volume.FatCache)) return 0x0FFFFFFFu;
        volume.CachedFatSector = fatSector;
    }
    return loadLe32(volume.FatCache + (byteOffset % kSectorSize)) & 0x0FFFFFFFu;
}

bool mountVolume(FatVolume& volume, uint64_t startLba,
                 uint64_t partitionSectors, char drive) noexcept {
    volume = {};
    if (!partitionSectors || startLba >= current->sectors ||
        partitionSectors > current->sectors - startLba) return false;
    volume.PartitionStart = startLba;
    volume.PartitionSectors = partitionSectors;
    if (readIoBlocks(startLba, 1, g_sectorBuffer) != STATUS_SUCCESS) return false;
    if (g_sectorBuffer[510] != 0x55 || g_sectorBuffer[511] != 0xAA) return false;

    volume.BytesPerSector = loadLe16(g_sectorBuffer + 11);
    volume.SectorsPerCluster = g_sectorBuffer[13];
    volume.ReservedSectors = loadLe16(g_sectorBuffer + 14);
    volume.NumberOfFats = g_sectorBuffer[16];
    volume.SectorsPerFat = loadLe32(g_sectorBuffer + 36);
    volume.RootCluster = loadLe32(g_sectorBuffer + 44);
    const uint32_t total32 = loadLe32(g_sectorBuffer + 32);
    const uint16_t total16 = loadLe16(g_sectorBuffer + 19);
    volume.TotalSectors = total32 ? total32 : total16;
    if (volume.BytesPerSector != kSectorSize || !volume.SectorsPerCluster ||
        volume.SectorsPerCluster > 128 ||
        (volume.SectorsPerCluster & (volume.SectorsPerCluster - 1)) != 0 ||
        !volume.ReservedSectors || !volume.NumberOfFats || !volume.SectorsPerFat ||
        !volume.TotalSectors || volume.TotalSectors > partitionSectors ||
        volume.RootCluster < 2 || volume.PartitionStart > UINT64_MAX - volume.TotalSectors)
        return false;
    const uint64_t dataStart = static_cast<uint64_t>(volume.ReservedSectors) +
        static_cast<uint64_t>(volume.NumberOfFats) * volume.SectorsPerFat;
    if (dataStart >= volume.TotalSectors || dataStart > UINT32_MAX) return false;
    const uint64_t clusterCount = (volume.TotalSectors - dataStart) /
                                  volume.SectorsPerCluster;
    if (clusterCount == 0 || clusterCount > 0x0FFFFFF5u ||
        clusterCount > (static_cast<uint64_t>(volume.SectorsPerFat) * kSectorSize) / 4 - 2)
        return false;
    volume.FatStart = volume.ReservedSectors;
    volume.DataStart = static_cast<uint32_t>(dataStart);
    volume.DataClusterCount = static_cast<uint32_t>(clusterCount);
    if (volume.RootCluster - 2 >= volume.DataClusterCount) return false;
    volume.CachedFatSector = UINT32_MAX;
    volume.Drive = drive;
    volume.Mounted = true;
    return true;
}

bool equalAsciiInsensitive(const char* left, const char* right) noexcept {
    size_t i{};
    while (left[i] && right[i]) {
        char a = left[i], b = right[i];
        if (a >= 'a' && a <= 'z') a = static_cast<char>(a - ('a' - 'A'));
        if (b >= 'a' && b <= 'z') b = static_cast<char>(b - ('a' - 'A'));
        if (a != b) return false;
        ++i;
    }
    return left[i] == right[i];
}

void formatShortName(const uint8_t* entry, char* output, size_t outputSize) noexcept {
    size_t at{};
    for (size_t i = 0; i < 8 && entry[i] != ' '; ++i)
        if (at + 1 < outputSize) output[at++] = static_cast<char>(entry[i]);
    size_t ext{};
    while (ext < 3 && entry[8 + ext] == ' ') ++ext;
    if (ext < 3 && at + 1 < outputSize) output[at++] = '.';
    for (size_t i = ext; i < 3 && entry[8 + i] != ' '; ++i)
        if (at + 1 < outputSize) output[at++] = static_cast<char>(entry[8 + i]);
    output[at] = 0;
}

bool findInDirectory(FatVolume& volume, uint32_t directoryCluster,
                     const char* component, FatDirEntry& found) noexcept {
    char longName[256]{};
    uint8_t lfnExpected{}, lfnChecksum{};
    bool lfnValid{};
    const uint32_t maxClusters = volume.DataClusterCount + 1;
    uint32_t cluster = directoryCluster;
    for (uint32_t traversed = 0; traversed < maxClusters; ++traversed) {
        if (cluster < 2 || cluster - 2 >= volume.DataClusterCount) return false;
        const uint32_t sectors = sectorsPerCluster(volume);
        uint32_t clusterLba{};
        if (!clusterToLba(volume, cluster, clusterLba) ||
            !readVolume(volume, clusterLba, sectors, g_clusterBuffer)) return false;
        const size_t entries = static_cast<size_t>(sectors) * kSectorSize / 32;
        for (size_t i = 0; i < entries; ++i) {
            const uint8_t* item = g_clusterBuffer + i * 32;
            if (item[0] == 0) return false;
            if (item[0] == 0xE5) { longName[0] = 0; lfnValid = false; continue; }
            if (item[11] == 0x0F) {
                const uint8_t sequence = item[0] & 0x1F;
                if (sequence == 0 || sequence > 19 || (item[0] & 0xA0) || item[12] ||
                    loadLe16(item + 26)) { longName[0] = 0; lfnValid = false; continue; }
                if (item[0] & 0x40) {
                    std::memset(longName, 0, sizeof(longName));
                    lfnExpected = sequence; lfnChecksum = item[13]; lfnValid = true;
                }
                if (!lfnValid || lfnExpected != sequence || item[13] != lfnChecksum) {
                    longName[0] = 0; lfnValid = false; continue;
                }
                static constexpr uint8_t offsets[13] = {1,3,5,7,9,14,16,18,20,22,24,28,30};
                const size_t base = static_cast<size_t>(sequence - 1) * 13;
                for (size_t c = 0; c < 13 && base + c + 1 < sizeof(longName); ++c) {
                    const uint16_t character = loadLe16(item + offsets[c]);
                    if (character == 0 || character == 0xFFFF) {
                        longName[base + c] = 0;
                        break;
                    }
                    if (character > 0x7f) { lfnValid = false; break; }
                    longName[base + c] = static_cast<char>(character);
                }
                --lfnExpected;
                continue;
            }
            if (item[11] & 0x08) { longName[0] = 0; lfnValid = false; continue; }

            FatDirEntry candidate{};
            candidate.Directory = (item[11] & 0x10) != 0;
            candidate.FirstCluster = (static_cast<uint32_t>(loadLe16(item + 20)) << 16) |
                                      loadLe16(item + 26);
            candidate.Size = loadLe32(item + 28);
            formatShortName(item, candidate.ShortName, sizeof(candidate.ShortName));
            uint8_t shortChecksum{};
            for (unsigned c = 0; c < 11; ++c)
                shortChecksum = static_cast<uint8_t>(((shortChecksum & 1u) << 7) +
                    (shortChecksum >> 1) + item[c]);
            const char* name = lfnValid && !lfnExpected && shortChecksum == lfnChecksum && longName[0]
                ? longName : candidate.ShortName;
            size_t n{};
            while (name[n] && n + 1 < sizeof(candidate.Name)) {
                candidate.Name[n] = name[n];
                ++n;
            }
            candidate.Name[n] = 0;
            longName[0] = 0;
            lfnValid = false;
            if (equalAsciiInsensitive(component, candidate.Name) ||
                equalAsciiInsensitive(component, candidate.ShortName)) {
                found = candidate;
                return true;
            }
        }
        const uint32_t next = nextCluster(volume, cluster);
        if (next >= 0x0FFFFFF8u) return false;
        cluster = next;
    }
    return false;
}

bool findFile(FatVolume& volume, const char* path, FatDirEntry& result) noexcept {
    if (!volume.Mounted || !path) return false;
    const char* current = path;
    while (*current == '\\' || *current == '/') ++current;
    uint32_t directory = volume.RootCluster;
    if (*current == 0) return false;
    for (;;) {
        char component[128]{};
        size_t length{};
        while (*current && *current != '\\' && *current != '/') {
            if (length + 1 >= sizeof(component)) return false;
            component[length++] = *current++;
        }
        component[length] = 0;
        while (*current == '\\' || *current == '/') ++current;
        if (!length || equalAsciiInsensitive(component, ".") ||
            equalAsciiInsensitive(component, "..")) return false;
        FatDirEntry entry{};
        if (!findInDirectory(volume, directory, component, entry)) return false;
        if (*current == 0) { result = entry; return true; }
        if (!entry.Directory || entry.FirstCluster < 2) return false;
        directory = entry.FirstCluster;
    }
}

bool readFileAt(FatVolume& volume, const FatDirEntry& entry, uint64_t offset,
                void* output, size_t length, size_t& bytesRead) noexcept {
    bytesRead = 0;
    if (entry.Directory || (!output && length)) return false;
    if (offset >= entry.Size || length == 0) return true;
    const uint32_t clusterBytes = static_cast<uint32_t>(volume.BytesPerSector) *
                                  volume.SectorsPerCluster;
    const size_t wanted = static_cast<size_t>((entry.Size - offset) < length
        ? (entry.Size - offset) : length);
    uint64_t skip = offset / clusterBytes;
    uint32_t cluster = entry.FirstCluster;
    const uint32_t maxClusters = volume.DataClusterCount + 1;
    if (skip >= maxClusters) return false;
    while (skip--) {
        if (cluster < 2 || cluster - 2 >= volume.DataClusterCount) return false;
        cluster = nextCluster(volume, cluster);
    }
    size_t within = static_cast<size_t>(offset % clusterBytes);
    auto* destination = static_cast<uint8_t*>(output);
    uint32_t traversed{};
    while (bytesRead < wanted) {
        if (cluster < 2 || cluster - 2 >= volume.DataClusterCount ||
            ++traversed > maxClusters) return false;
        uint32_t clusterLba{};
        if (!clusterToLba(volume, cluster, clusterLba) ||
            !readVolume(volume, clusterLba,
                        volume.SectorsPerCluster, g_clusterBuffer)) return false;
        const size_t amount = (wanted - bytesRead) < (clusterBytes - within)
            ? wanted - bytesRead : clusterBytes - within;
        std::memcpy(destination + bytesRead, g_clusterBuffer + within, amount);
        bytesRead += amount;
        within = 0;
        if (bytesRead < wanted) cluster = nextCluster(volume, cluster);
    }
    return true;
}

int32_t mountGptVolumes() noexcept {
    if (current->sectors < 2 || readIoBlocks(1, 1, g_sectorBuffer) != STATUS_SUCCESS)
        return STATUS_DEVICE_DATA_ERROR;
    const uint8_t* header = g_sectorBuffer;
    if (std::memcmp(header, "EFI PART", 8) != 0) return STATUS_UNRECOGNIZED_VOLUME;
    const uint32_t headerSize = loadLe32(header + 12);
    if (headerSize < 92 || headerSize > kSectorSize || loadLe64(header + 24) != 1)
        return STATUS_DEVICE_DATA_ERROR;
    const uint64_t backupLba = loadLe64(header + 32);
    const uint64_t firstUsable = loadLe64(header + 40);
    const uint64_t lastUsable = loadLe64(header + 48);
    const uint64_t entriesLba = loadLe64(header + 72);
    const uint32_t entryCount = loadLe32(header + 80);
    const uint32_t entrySize = loadLe32(header + 84);
    const uint32_t expectedEntriesCrc = loadLe32(header + 88);
    uint8_t headerCopy[kSectorSize];
    std::memcpy(headerCopy, header, sizeof(headerCopy));
    const uint32_t expectedHeaderCrc = loadLe32(headerCopy + 16);
    std::memset(headerCopy + 16, 0, sizeof(uint32_t));
    if ((crc32(headerCopy, headerSize) ^ 0xFFFFFFFFu) != expectedHeaderCrc ||
        backupLba <= 1 || backupLba >= current->sectors || firstUsable > lastUsable ||
        lastUsable >= current->sectors || entriesLba < 2 ||
        entriesLba >= current->sectors || entrySize != 128 ||
        entryCount == 0 || entryCount > 128) return STATUS_DEVICE_DATA_ERROR;

    const uint64_t entriesBytes = static_cast<uint64_t>(entryCount) * entrySize;
    const uint64_t entriesSectors = (entriesBytes + kSectorSize - 1) / kSectorSize;
    if (!entriesSectors || entriesSectors > kMaximumSectorsPerCommand ||
        entriesLba > current->sectors - entriesSectors ||
        entriesLba + entriesSectors > firstUsable || backupLba <= lastUsable ||
        readIoBlocks(entriesLba, static_cast<uint32_t>(entriesSectors), g_gptEntries) != STATUS_SUCCESS)
        return STATUS_DEVICE_DATA_ERROR;
    if ((crc32(g_gptEntries, static_cast<size_t>(entriesBytes)) ^ 0xFFFFFFFFu) !=
        expectedEntriesCrc) return STATUS_DEVICE_DATA_ERROR;

    // GPT requires a reciprocal backup header and a duplicate partition table.
    // Validate both before exposing any partition; accepting only the primary
    // copy can mount a disk whose recovery metadata is already inconsistent.
    if (readIoBlocks(backupLba, 1, g_sectorBuffer) != STATUS_SUCCESS)
        return STATUS_DEVICE_DATA_ERROR;
    const uint8_t* backup = g_sectorBuffer;
    const uint32_t backupHeaderSize = loadLe32(backup + 12);
    uint8_t backupHeaderCopy[kSectorSize];
    if (std::memcmp(backup, "EFI PART", 8) != 0 || backupHeaderSize < 92 ||
        backupHeaderSize > kSectorSize || loadLe64(backup + 24) != backupLba ||
        loadLe64(backup + 32) != 1 || loadLe64(backup + 40) != firstUsable ||
        loadLe64(backup + 48) != lastUsable ||
        std::memcmp(backup + 56, headerCopy + 56, 16) != 0 ||
        loadLe32(backup + 80) != entryCount || loadLe32(backup + 84) != entrySize ||
        loadLe32(backup + 88) != expectedEntriesCrc) return STATUS_DEVICE_DATA_ERROR;
    std::memcpy(backupHeaderCopy, backup, sizeof(backupHeaderCopy));
    const uint32_t backupHeaderCrc = loadLe32(backupHeaderCopy + 16);
    std::memset(backupHeaderCopy + 16, 0, sizeof(uint32_t));
    if ((crc32(backupHeaderCopy, backupHeaderSize) ^ 0xFFFFFFFFu) != backupHeaderCrc)
        return STATUS_DEVICE_DATA_ERROR;
    const uint64_t backupEntriesLba = loadLe64(backup + 72);
    if (backupEntriesLba < lastUsable + 1 || backupEntriesLba >= backupLba ||
        entriesSectors > backupLba - backupEntriesLba ||
        readIoBlocks(backupEntriesLba, static_cast<uint32_t>(entriesSectors),
                     g_backupGptEntries) != STATUS_SUCCESS ||
        (crc32(g_backupGptEntries, static_cast<size_t>(entriesBytes)) ^ 0xFFFFFFFFu) !=
            expectedEntriesCrc ||
        std::memcmp(g_gptEntries, g_backupGptEntries,
                    static_cast<size_t>(entriesBytes)) != 0)
        return STATUS_DEVICE_DATA_ERROR;

    // Microsoft Basic Data type GUID in GPT on-disk byte order.
    static constexpr uint8_t basicDataGuid[16] = {
        0xA2,0xA0,0xD0,0xEB,0xE5,0xB9,0x33,0x44,
        0x87,0xC0,0x68,0xB6,0xB7,0x26,0x99,0xC7};
    static constexpr uint8_t zeroGuid[16]{};
    for (uint32_t i = 0; i < entryCount; ++i) {
        const uint8_t* entry = g_gptEntries + static_cast<size_t>(i) * entrySize;
        if (std::memcmp(entry, zeroGuid, sizeof(zeroGuid)) == 0) continue;
        const uint64_t first = loadLe64(entry + 32);
        const uint64_t last = loadLe64(entry + 40);
        if (first < firstUsable || last > lastUsable || last < first ||
            last >= current->sectors) return STATUS_DEVICE_DATA_ERROR;
        for (uint32_t j = 0; j < i; ++j) {
            const uint8_t* previous = g_gptEntries + static_cast<size_t>(j) * entrySize;
            if (std::memcmp(previous, zeroGuid, sizeof(zeroGuid)) == 0) continue;
            const uint64_t previousFirst = loadLe64(previous + 32);
            const uint64_t previousLast = loadLe64(previous + 40);
            if (first <= previousLast && previousFirst <= last)
                return STATUS_DEVICE_DATA_ERROR;
        }
    }
    constexpr char driveLetters[3] = {'S', 'C', 'D'};
    bool mounted{};
    for (uint32_t i = 0; i < entryCount && g_volumeCount < 3; ++i) {
        const uint8_t* entry = g_gptEntries + static_cast<size_t>(i) * entrySize;
        if (std::memcmp(entry, basicDataGuid, sizeof(basicDataGuid)) != 0) continue;
        const uint64_t first = loadLe64(entry + 32);
        const uint64_t last = loadLe64(entry + 40);
        if (first < firstUsable || last > lastUsable || last < first ||
            last >= current->sectors) continue;
        if (mountVolume(g_volumes[g_volumeCount], first, last - first + 1,
                        driveLetters[g_volumeCount])) {
            ++g_volumeCount;
            mounted = true;
        }
    }
    return mounted ? STATUS_SUCCESS : STATUS_UNRECOGNIZED_VOLUME;
}

int32_t mountMbrVolumes() noexcept {
    if (readIoBlocks(0, 1, g_sectorBuffer) != STATUS_SUCCESS) return STATUS_DEVICE_DATA_ERROR;
    if (g_sectorBuffer[510] != 0x55 || g_sectorBuffer[511] != 0xAA) {
        if (!mountVolume(g_volumes[0], 0, current->sectors, 'C'))
            return STATUS_UNRECOGNIZED_VOLUME;
        g_volumeCount = 1;
        return STATUS_SUCCESS;
    }
    uint8_t partitions[64];
    std::memcpy(partitions, g_sectorBuffer + 446, sizeof(partitions));
    bool hasPartition{};
    for (uint32_t slot = 0; slot < 4; ++slot) {
        const uint8_t* entry = partitions + slot * 16;
        if (entry[4] == 0xEE) return mountGptVolumes();
        if (entry[4] != 0) hasPartition = true;
    }
    bool mounted{};
    constexpr char driveLetters[3] = {'S', 'C', 'D'};
    for (uint32_t slot = 0; slot < 4 && g_volumeCount < 3; ++slot) {
        const uint8_t* entry = partitions + slot * 16;
        const uint8_t type = entry[4];
        const uint64_t start = loadLe32(entry + 8);
        const uint64_t count = loadLe32(entry + 12);
        if ((type != 0x0B && type != 0x0C) || !start || !count ||
            start >= current->sectors || count > current->sectors - start) continue;
        bool overlapsAnotherPartition{};
        const uint64_t end = start + count;
        for (uint32_t otherSlot = 0; otherSlot < 4; ++otherSlot) {
            if (otherSlot == slot) continue;
            const uint8_t* other = partitions + otherSlot * 16;
            const uint64_t otherStart = loadLe32(other + 8);
            const uint64_t otherCount = loadLe32(other + 12);
            if (other[4] == 0 || otherCount == 0 || otherStart >= current->sectors ||
                otherCount > current->sectors - otherStart) continue;
            const uint64_t otherEnd = otherStart + otherCount;
            if (start < otherEnd && otherStart < end) {
                overlapsAnotherPartition = true;
                break;
            }
        }
        if (overlapsAnotherPartition) continue;
        if (mountVolume(g_volumes[g_volumeCount], start, count,
                        driveLetters[g_volumeCount])) {
            ++g_volumeCount;
            mounted = true;
        }
    }
    if (!mounted && !hasPartition && mountVolume(g_volumes[0], 0,
            current->sectors, 'C')) {
        g_volumeCount = 1;
        mounted = true;
    }
    return mounted ? STATUS_SUCCESS : STATUS_UNRECOGNIZED_VOLUME;
}

bool convertPath(const char16_t* name, size_t length, char (&path)[kMaximumPath],
                 FatVolume*& volume) noexcept {
    volume = nullptr;
    if (!name || !length) return false;
    size_t offset{};
    if (length >= 4 && name[0] == u'\\' && name[1] == u'?' &&
        name[2] == u'?' && name[3] == u'\\') offset = 4;
    else if (length >= 4 && name[0] == u'\\' && name[1] == u'\\' &&
             name[2] == u'?' && name[3] == u'\\') offset = 4;

    if (length - offset < 2 || name[offset + 1] != u':') return false;
    const char16_t drive = name[offset];
    for (uint32_t i = 0; i < 3; ++i)
        if (g_volumes[i].Mounted && (drive == g_volumes[i].Drive ||
            drive == static_cast<char16_t>(g_volumes[i].Drive + ('a' - 'A'))))
            volume = &g_volumes[i];
    if (!volume) return false;
    offset += 2;
    while (offset < length && (name[offset] == u'\\' || name[offset] == u'/')) ++offset;
    if (offset == length) return false;
    size_t output{};
    size_t component{};
    for (size_t i = offset; i < length; ++i) {
        const char16_t ch = name[i];
        if (ch == 0 || ch > 0x7F || output + 1 >= sizeof(path)) return false;
        if (ch == u'\\' || ch == u'/') {
            if (!component || component > 127) return false;
            if (component == 1 && path[output - 1] == '.') return false;
            if (component == 2 && path[output - 1] == '.' && path[output - 2] == '.') return false;
            component = 0;
            path[output++] = '\\';
        } else {
            ++component;
            path[output++] = static_cast<char>(ch);
        }
    }
    if (!component || component > 127) return false;
    path[output] = 0;
    return true;
}


#undef g_volumes
#undef g_volumeCount
NtStatus readFile(void* opaque, uint64_t offset, void* buffer, size_t length, size_t* done) noexcept {
    if (!opaque || !done || (!buffer && length)) return NtStatus::InvalidParameter;
    *done = 0; Gate guard; if (!guard.held) return NtStatus::DeviceNotReady;
    auto& file = *static_cast<FileContext*>(opaque);
    if (!file.inUse || !file.owner || file.owner->offline) return NtStatus::DeviceNotReady;
    current = file.owner; size_t transferred{};
    const bool ok = readFileAt(*file.volume, file.entry, offset, buffer, length, transferred);
    current = nullptr; *done = transferred;
    return !ok ? NtStatus::DeviceDataError : transferred < length ? NtStatus::EndOfFile : NtStatus::Success;
}
void releaseFile(void* opaque) noexcept {
    if (!opaque) return; Gate guard; if (!guard.held) __builtin_trap();
    *static_cast<FileContext*>(opaque) = {};
}
NtStatus openFile(void*, std::u16string_view name, io::FileBackend& backend) noexcept {
    backend = {}; Gate guard; if (!guard.held) return NtStatus::DeviceNotReady;
    char pathBuffer[kMaximumPath]{}; FatVolume* volume{};
    for (auto& mount : mounts) {
        if (!mount.token || mount.offline) continue;
        current = &mount;
        if (!convertPath(name.data(), name.size(), pathBuffer, volume)) continue;
        FatDirEntry entry{};
        if (!findFile(*volume, pathBuffer, entry)) { current = nullptr; return NtStatus::ObjectNameNotFound; }
        if (entry.Directory) { current = nullptr; return NtStatus::FileIsADirectory; }
        for (auto& file : mount.files) if (!file.inUse) {
            file = {&mount, volume, entry, true};
            backend = {&file, entry.Size, name, readFile, releaseFile};
            current = nullptr; return NtStatus::Success;
        }
        current = nullptr; return NtStatus::NoMemory;
    }
    current = nullptr; return NtStatus::ObjectPathNotFound;
}
}
NtStatus initialize() noexcept {
    if (initialized) return NtStatus::Conflict;
    initialized = true;
    return NtStatus::Success;
}
NtStatus open(std::u16string_view name, io::FileBackend& backend) noexcept {
    if (!initialized) return NtStatus::DeviceNotReady;
    return openFile(nullptr, name, backend);
}
NtStatus mount(const void* owner, Handle handle, uint64_t& token) noexcept {
    token = 0; if (!owner) return NtStatus::InvalidParameter;
    if (!initialized) return NtStatus::DeviceNotReady;
    io::BlockDeviceObject* device{}; ob::ObjectHeader* reference{};
    auto status = io::IoReferenceBlockDeviceHandle(handle, 1, device, reference);
    if (status != NtStatus::Success) return status;
    if (device->blockSize() != kSectorSize) { ob::ObDereferenceObject(reference); return NtStatus::NotSupported; }
    {
        Gate guard;
        if (!guard.held) status = NtStatus::DeviceNotReady;
        else {
            Mount* slot{}; for (auto& m : mounts) if (!m.token) { slot = &m; break; }
            if (!slot || !nextToken) status = NtStatus::InsufficientResources;
            else {
                *slot = {}; slot->owner = owner; slot->device = device; slot->reference = reference; slot->sectors = device->totalBlocks();
                current = slot;
                status = static_cast<NtStatus>(mountMbrVolumes());
                current = nullptr;
                if (status == NtStatus::Success) {
                    bool used[26]{};
                    for (const auto& other : mounts) if (other.token)
                        for (uint32_t i = 0; i < other.volumeCount; ++i)
                            if (other.volumes[i].Drive >= 'A' && other.volumes[i].Drive <= 'Z') used[other.volumes[i].Drive - 'A'] = true;
                    for (uint32_t i = 0; i < slot->volumeCount; ++i) {
                        auto& drive = slot->volumes[i].Drive;
                        if (drive < 'A' || drive > 'Z' || used[drive - 'A']) {
                            drive = 0; for (unsigned j = 2; j < 26; ++j) if (!used[j]) { drive = static_cast<char>('A' + j); break; }
                        }
                        if (!drive) { status = NtStatus::InsufficientResources; break; }
                        used[drive - 'A'] = true;
                    }
                }
                if (status == NtStatus::Success) { slot->token = nextToken++; token = slot->token; reference = nullptr; }
                else *slot = {};
            }
        }
    }
    if (reference) ob::ObDereferenceObject(reference);
    return status;
}
NtStatus unmount(const void* owner, uint64_t token) noexcept {
    if (!owner || !token) return NtStatus::InvalidParameter;
    ob::ObjectHeader* reference{}; NtStatus status = NtStatus::InvalidHandle;
    {
        Gate guard; if (!guard.held) return NtStatus::DeviceNotReady;
        for (auto& m : mounts) if (m.token == token && m.owner == owner) {
            m.offline = true;
            for (const auto& file : m.files) if (file.inUse) return NtStatus::DeviceBusy;
            reference = m.reference; m = {}; status = NtStatus::Success; break;
        }
    }
    if (reference) ob::ObDereferenceObject(reference);
    return status;
}
}
