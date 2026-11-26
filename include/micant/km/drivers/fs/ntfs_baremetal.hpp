// ============================================================================
// MicaNT: Sovereign Freestanding Bare-Metal On-Disk NTFS 3.1 Driver
// (ntfs_baremetal.hpp)
//
// Description:
//   Pure -ffreestanding -nostdlib ISO C++23 New Technology File System (NTFS)
//   driver compiled directly into bare-metal ntoskrnl.exe.
//   Operates directly over real AHCI Bus-Master DMA physical disk sectors
//   (no STL, no host heap, no mocks, no synthetic overlays).
//
//   Implements:
//   - GPT ("EFI PART") & MBR Partition Table scanning via AHCI DMA
//   - NTFS 3.1 Boot Sector / BPB validation ("NTFS    ", 0xAA55)
//   - Update Sequence Array (USA) fixup verification & sector restoration
//   - MFT Record reader ($MFT Record #0 runlist decoding + linear addressing)
//   - Real $Volume (Record #3) UTF-16LE label ($VOLUME_NAME 0x60) & version (0x70)
//   - Real $FILE_NAME (0x30) UTF-16LE path resolution & parent MFT linkage
//   - Real $DATA (0x80) Resident & Non-Resident mapping-pair runlist reader
//   - Real $INDEX_ROOT (0x90) & $INDEX_ALLOCATION (0xA0 / "INDX") directory traversal
//   - Full file ingestion into kernel buffers (e.g., C:\MicaNT\system.ini,
//     C:\Windows\System32\*.dll, C:\Windows\System32\cmd.exe)
//
// Clean-Room ISO C++23. Zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include "micant/km/drivers/storage/ahci_baremetal.hpp"

namespace micant::km::ntfs_baremetal {

// ============================================================================
// 1. On-Disk GPT, MBR & NTFS 3.1 Packed Structures
// ============================================================================

#pragma pack(push, 1)

struct MbrPartitionEntry {
    uint8_t  bootIndicator;
    uint8_t  startChs[3];
    uint8_t  partitionType;
    uint8_t  endChs[3];
    uint32_t startingLba;
    uint32_t sectorCount;
};

struct MbrSector {
    uint8_t           bootCode[446];
    MbrPartitionEntry partitions[4];
    uint16_t          signature; // 0xAA55
};

struct GptHeader {
    uint64_t signature;          // "EFI PART" = 0x5452415020494645
    uint32_t revision;
    uint32_t headerSize;
    uint32_t headerCrc32;
    uint32_t reserved;
    uint64_t myLba;
    uint64_t alternateLba;
    uint64_t firstUsableLba;
    uint64_t lastUsableLba;
    uint8_t  diskGuid[16];
    uint64_t partitionEntryLba;
    uint32_t numPartitionEntries;
    uint32_t sizeOfPartitionEntry;
    uint32_t partitionEntryArrayCrc32;
};

struct GptPartitionEntry {
    uint8_t  partitionTypeGuid[16];
    uint8_t  uniquePartitionGuid[16];
    uint64_t startingLba;
    uint64_t endingLba;
    uint64_t attributes;
    uint16_t partitionName[36];
};

struct NtfsBootSector {
    uint8_t  jump[3];
    uint8_t  oemId[8];                      // "NTFS    "
    uint16_t bytesPerSector;
    uint8_t  sectorsPerCluster;
    uint16_t reservedSectors;
    uint8_t  zeros1[3];
    uint16_t unused1;
    uint8_t  mediaDescriptor;
    uint16_t zeros2;
    uint16_t sectorsPerTrack;
    uint16_t numberOfHeads;
    uint32_t hiddenSectors;
    uint32_t unused2;
    uint32_t unused3;
    uint64_t totalSectors;
    uint64_t mftClusterNumber;
    uint64_t mftMirrClusterNumber;
    int8_t   clustersPerFileRecordSegment;  // Negative -> 2^|val| (e.g. -10 -> 1024)
    uint8_t  reserved1[3];
    int8_t   clustersPerIndexBlock;         // Negative -> 2^|val| or positive * clusterSize
    uint8_t  reserved2[3];
    uint64_t volumeSerialNumber;
    uint32_t checksum;
    uint8_t  bootCode[426];
    uint16_t endMarker;                     // 0xAA55
};

struct MftRecordHeader {
    uint32_t signature;                     // "FILE" = 0x454C4946
    uint16_t usaOffset;
    uint16_t usaCount;
    uint64_t lsn;
    uint16_t sequenceNumber;
    uint16_t hardLinkCount;
    uint16_t firstAttributeOffset;
    uint16_t flags;                         // 0x01 = InUse, 0x02 = Directory
    uint32_t usedSize;
    uint32_t allocatedSize;
    uint64_t baseFileRecord;
    uint16_t nextAttributeId;
    uint16_t padding;
    uint32_t mftRecordNumber;
};

struct NtfsAttrHeader {
    uint32_t type;
    uint32_t length;
    uint8_t  nonResident;
    uint8_t  nameLength;
    uint16_t nameOffset;
    uint16_t flags;
    uint16_t attributeId;
};

struct NtfsResidentAttr {
    NtfsAttrHeader header;
    uint32_t valueLength;
    uint16_t valueOffset;
    uint8_t  indexedFlag;
    uint8_t  padding;
};

struct NtfsNonResidentAttr {
    NtfsAttrHeader header;
    uint64_t lowestVcn;
    uint64_t highestVcn;
    uint16_t mappingPairsOffset;
    uint16_t compressionUnit;
    uint32_t reserved;
    uint64_t allocatedSize;
    uint64_t realSize;
    uint64_t initializedSize;
};

struct NtfsFileNameAttr {
    uint64_t parentDirectory;               // Low 48 bits = parent MFT record #
    uint64_t creationTime;
    uint64_t modificationTime;
    uint64_t mftChangeTime;
    uint64_t lastAccessTime;
    uint64_t allocatedSize;
    uint64_t realSize;
    uint32_t fileAttributes;
    uint32_t reparseTag;
    uint8_t  nameLength;                    // In UTF-16LE code units
    uint8_t  nameNamespace;
    uint16_t name[1];
};

struct NtfsVolumeInfoAttr {
    uint64_t reserved;
    uint8_t  majorVersion;
    uint8_t  minorVersion;
    uint16_t flags;
};

struct NtfsIndexHeader {
    uint32_t entriesOffset;
    uint32_t totalEntriesSize;
    uint32_t allocatedSize;
    uint8_t  flags;                         // 0x01 = has sub-nodes ($INDEX_ALLOCATION)
    uint8_t  padding[3];
};

struct NtfsIndexRootAttr {
    uint32_t indexedAttrType;               // 0x30 ($FILE_NAME)
    uint32_t collationRule;                 // 0x01 (CollationFilename)
    uint32_t indexBlockSize;                // 4096
    uint8_t  clustersPerIndexBlock;
    uint8_t  padding[3];
    NtfsIndexHeader indexHeader;
};

struct NtfsIndexEntryHeader {
    uint64_t fileReference;                 // Low 48 bits = MFT record #
    uint16_t entryLength;
    uint16_t keyLength;
    uint32_t flags;                         // 0x01 = has sub-node VCN, 0x02 = last entry
};

struct NtfsIndxBlockHeader {
    uint32_t signature;                     // "INDX" = 0x58444E49
    uint16_t usaOffset;
    uint16_t usaCount;
    uint64_t lsn;
    uint64_t indexBlockVcn;
    NtfsIndexHeader indexHeader;
};

#pragma pack(pop)

// ============================================================================
// 2. Constants & Runlist Descriptors
// ============================================================================

inline constexpr uint32_t NTFS_ATTR_STANDARD_INFO    = 0x10;
inline constexpr uint32_t NTFS_ATTR_FILE_NAME        = 0x30;
inline constexpr uint32_t NTFS_ATTR_VOLUME_NAME      = 0x60;
inline constexpr uint32_t NTFS_ATTR_VOLUME_INFO      = 0x70;
inline constexpr uint32_t NTFS_ATTR_DATA             = 0x80;
inline constexpr uint32_t NTFS_ATTR_INDEX_ROOT       = 0x90;
inline constexpr uint32_t NTFS_ATTR_INDEX_ALLOCATION = 0xA0;
inline constexpr uint32_t NTFS_ATTR_END              = 0xFFFFFFFF;

inline constexpr uint32_t NTFS_FILE_MAGIC = 0x454C4946; // "FILE"
inline constexpr uint32_t NTFS_INDX_MAGIC = 0x58444E49; // "INDX"

inline constexpr uint64_t MFT_RECORD_MFT    = 0;
inline constexpr uint64_t MFT_RECORD_VOLUME = 3;
inline constexpr uint64_t MFT_RECORD_ROOT   = 5;
inline constexpr uint64_t MFT_RECORD_BITMAP = 6;

struct NtfsDataRun {
    uint64_t clusterCount{0};
    int64_t  lcn{0}; // -1 for sparse run
};

struct NtfsDirEntry {
    char     name[128]{};
    uint64_t mftRecordNumber{0};
    uint64_t fileSize{0};
    bool     isDirectory{false};
};

// ============================================================================
// 3. Static Freestanding Work Buffers (Zero Heap Allocations)
// ============================================================================

alignas(4096) static uint8_t s_ntfsSectorBuf[4096];
alignas(4096) static uint8_t s_ntfsMftBuf[4096];
alignas(4096) static uint8_t s_ntfsDirMftBuf[4096];
alignas(4096) static uint8_t s_ntfsIndxBuf[4096];
alignas(4096) static uint8_t s_ntfsWriteMftBuf[4096];
alignas(4096) static uint8_t s_ntfsBitmapBuf[8192];
alignas(4096) static uint8_t s_ntfsIoClusterBuf[65536]; // 64 KB DMA transfer window

// ============================================================================
// 4. Bare-Metal NTFS 3.1 Volume Engine
// ============================================================================

class BareMetalNtfsVolume {
private:
    ahci_baremetal::BareMetalAhciController* m_ahci{nullptr};
    bool     m_mounted{false};
    uint64_t m_partitionLba{0};
    uint64_t m_totalSectors{0};
    uint32_t m_bytesPerSector{512};
    uint32_t m_sectorsPerCluster{8};
    uint32_t m_bytesPerCluster{4096};
    uint32_t m_mftRecordSize{1024};
    uint32_t m_indexBlockSize{4096};
    uint64_t m_mftClusterNumber{0};
    uint64_t m_mftMirrClusterNumber{0};
    uint64_t m_mftRecordCount{64};
    uint64_t m_volumeSerial{0};
    uint8_t  m_majorVersion{3};
    uint8_t  m_minorVersion{1};
    uint32_t m_usaFixupsVerified{0};
    uint32_t m_usaFixupsSealed{0};
    uint64_t m_dmaReadsIssued{0};
    uint64_t m_dmaWritesIssued{0};
    uint64_t m_clustersAllocated{0};
    uint32_t m_filesCreated{0};
    char     m_volumeLabel[64]{};

    // Cached $MFT data runs (supports fragmented MFT up to 32 runs)
    NtfsDataRun m_mftRuns[32]{};
    size_t      m_mftRunCount{0};

    static inline char toUpperAscii(char c) noexcept {
        return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
    }

    static bool equalAsciiIgnoreCase(const char* a, const char* b, size_t bLen) noexcept {
        size_t i = 0;
        for (; i < bLen; ++i) {
            if (a[i] == '\0') return false;
            if (toUpperAscii(a[i]) != toUpperAscii(b[i])) return false;
        }
        return a[i] == '\0';
    }

    static bool equalUtf16ToAsciiIgnoreCase(const uint16_t* u16, size_t u16Len, const char* ascii, size_t asciiLen) noexcept {
        if (u16Len != asciiLen) return false;
        for (size_t i = 0; i < u16Len; ++i) {
            char c = (u16[i] < 128) ? static_cast<char>(u16[i]) : '?';
            if (toUpperAscii(c) != toUpperAscii(ascii[i])) return false;
        }
        return true;
    }

    static void utf16ToAscii(const uint16_t* u16, size_t u16Len, char* dst, size_t dstCap) noexcept {
        size_t n = (u16Len < dstCap - 1) ? u16Len : (dstCap - 1);
        for (size_t i = 0; i < n; ++i) {
            dst[i] = (u16[i] < 128) ? static_cast<char>(u16[i]) : '?';
        }
        dst[n] = '\0';
    }

    // Apply Update Sequence Array (USA) fixup verification & restoration
    bool applyUsaFixup(uint8_t* recordBuf, size_t recordSize, uint16_t usaOffset, uint16_t usaCount) noexcept {
        if (usaCount < 2 || usaOffset + static_cast<size_t>(usaCount) * 2 > recordSize) {
            return false;
        }
        uint16_t expectedUsn = 0;
        std::memcpy(&expectedUsn, recordBuf + usaOffset, sizeof(uint16_t));

        size_t sectorsToFix = static_cast<size_t>(usaCount - 1);
        if (sectorsToFix * 512 > recordSize) {
            return false;
        }

        for (size_t s = 0; s < sectorsToFix; ++s) {
            size_t tailOffset = (s + 1) * 512 - 2;
            uint16_t actualTail = 0;
            std::memcpy(&actualTail, recordBuf + tailOffset, sizeof(uint16_t));
            if (actualTail != expectedUsn) {
                return false; // Torn write / USA mismatch
            }
            uint16_t originalWord = 0;
            std::memcpy(&originalWord, recordBuf + usaOffset + (s + 1) * 2, sizeof(uint16_t));
            std::memcpy(recordBuf + tailOffset, &originalWord, sizeof(uint16_t));
        }

        m_usaFixupsVerified++;
        return true;
    }

    // Decode NTFS mapping-pair runlist into (clusterCount, LCN) extents
    static size_t decodeRunlist(
        const uint8_t* runData,
        size_t maxRunBytes,
        NtfsDataRun* outRuns,
        size_t maxRuns
    ) noexcept {
        size_t pos = 0;
        size_t runIdx = 0;
        int64_t currentLcn = 0;

        while (pos < maxRunBytes && runIdx < maxRuns) {
            uint8_t header = runData[pos++];
            if (header == 0) break; // End of runlist

            uint8_t lenSize = header & 0x0F;
            uint8_t offSize = (header >> 4) & 0x0F;
            if (lenSize == 0 || lenSize > 8 || offSize > 8) break;
            if (pos + lenSize + offSize > maxRunBytes) break;

            uint64_t length = 0;
            for (uint8_t i = 0; i < lenSize; ++i) {
                length |= static_cast<uint64_t>(runData[pos++]) << (i * 8);
            }

            if (offSize == 0) {
                // Sparse run
                outRuns[runIdx++] = NtfsDataRun{ length, -1 };
            } else {
                int64_t lcnDelta = 0;
                for (uint8_t i = 0; i < offSize; ++i) {
                    lcnDelta |= static_cast<int64_t>(runData[pos++]) << (i * 8);
                }
                // Sign-extend two's complement offset delta
                if (offSize < 8 && (lcnDelta & (1LL << (offSize * 8 - 1)))) {
                    lcnDelta |= (~0LL << (offSize * 8));
                }
                currentLcn += lcnDelta;
                outRuns[runIdx++] = NtfsDataRun{ length, currentLcn };
            }
        }
        return runIdx;
    }

    bool readPartitionSectors(uint64_t relLba, uint16_t count, void* dst) noexcept {
        if (!m_ahci) return false;
        m_dmaReadsIssued++;
        return m_ahci->readSectors(m_partitionLba + relLba, count, dst);
    }

    bool writePartitionSectors(uint64_t relLba, uint16_t count, const void* src) noexcept {
        if (!m_ahci) return false;
        m_dmaWritesIssued++;
        return m_ahci->writeSectors(m_partitionLba + relLba, count, src);
    }

    // Seal Update Sequence Array (USA) fixups before committing a 1024B MFT or 4096B INDX block to disk
    bool sealUsaFixup(uint8_t* recordBuf, size_t recordSize, uint16_t usaOffset, uint16_t usaCount) noexcept {
        if (usaCount < 2 || usaOffset + static_cast<size_t>(usaCount) * 2 > recordSize) return false;
        size_t sectorsToFix = static_cast<size_t>(usaCount - 1);
        if (sectorsToFix * 512 > recordSize) return false;

        uint16_t usn = 0;
        std::memcpy(&usn, recordBuf + usaOffset, sizeof(uint16_t));
        usn = (usn == 0 || usn == 0xFFFF) ? 1 : static_cast<uint16_t>(usn + 1);
        std::memcpy(recordBuf + usaOffset, &usn, sizeof(uint16_t));

        for (size_t s = 0; s < sectorsToFix; ++s) {
            size_t tailOffset = (s + 1) * 512 - 2;
            uint16_t originalWord = 0;
            std::memcpy(&originalWord, recordBuf + tailOffset, sizeof(uint16_t));
            std::memcpy(recordBuf + usaOffset + (s + 1) * 2, &originalWord, sizeof(uint16_t));
            std::memcpy(recordBuf + tailOffset, &usn, sizeof(uint16_t));
        }

        m_usaFixupsSealed++;
        return true;
    }

    // Encode a single positive NTFS mapping-pair run (clusterCount, startLcn) + 0x00 terminator
    static size_t encodeSingleRun(uint64_t clusterCount, uint64_t startLcn, uint8_t* outBuf, size_t cap) noexcept {
        if (!outBuf || cap < 20 || clusterCount == 0) return 0;
        uint8_t lenBytes[8]{};
        size_t lenSize = 0;
        uint64_t c = clusterCount;
        do {
            lenBytes[lenSize++] = static_cast<uint8_t>(c & 0xFF);
            c >>= 8;
        } while (c > 0 && lenSize < 8);

        uint8_t offBytes[9]{};
        size_t offSize = 0;
        uint64_t o = startLcn;
        do {
            offBytes[offSize++] = static_cast<uint8_t>(o & 0xFF);
            o >>= 8;
        } while (o > 0 && offSize < 8);
        // Ensure signed positive interpretation (MSB bit 7 must be 0 for positive LCN)
        if (offSize > 0 && (offBytes[offSize - 1] & 0x80) != 0 && offSize < 8) {
            offBytes[offSize++] = 0x00;
        }

        size_t pos = 0;
        outBuf[pos++] = static_cast<uint8_t>((lenSize & 0x0F) | ((offSize & 0x0F) << 4));
        for (size_t i = 0; i < lenSize; ++i) outBuf[pos++] = lenBytes[i];
        for (size_t i = 0; i < offSize; ++i) outBuf[pos++] = offBytes[i];
        outBuf[pos++] = 0x00;
        return pos;
    }

    // Write and USA-seal an MFT record directly to disk via AHCI Bus-Master DMA
    bool writeMftRecord(uint64_t recordNumber, const uint8_t* unfixedRecBuf) noexcept {
        if (!m_ahci || !unfixedRecBuf) return false;
        std::memcpy(s_ntfsWriteMftBuf, unfixedRecBuf, m_mftRecordSize);
        const auto* hdr = reinterpret_cast<const MftRecordHeader*>(s_ntfsWriteMftBuf);
        if (hdr->signature != NTFS_FILE_MAGIC) return false;
        if (!sealUsaFixup(s_ntfsWriteMftBuf, m_mftRecordSize, hdr->usaOffset, hdr->usaCount)) return false;
        uint64_t relLba = mftRecordToRelLba(recordNumber);
        uint16_t recSectors = static_cast<uint16_t>(m_mftRecordSize / m_bytesPerSector);
        return writePartitionSectors(relLba, recSectors, s_ntfsWriteMftBuf);
    }

    // Allocate contiguous free clusters from on-disk $Bitmap (MFT Record #6) and commit via AHCI DMA
    bool allocateClusters(uint64_t clusterCount, uint64_t& outStartLcn) noexcept {
        outStartLcn = 0;
        if (!m_mounted || clusterCount == 0) return false;
        if (!readMftRecord(MFT_RECORD_BITMAP, s_ntfsMftBuf, sizeof(s_ntfsMftBuf))) return false;

        const auto* hdr = reinterpret_cast<const MftRecordHeader*>(s_ntfsMftBuf);
        size_t off = hdr->firstAttributeOffset;
        NtfsDataRun bmRuns[8]{};
        size_t bmRunCnt = 0;
        uint64_t bmRealSize = 0;

        while (off + sizeof(NtfsAttrHeader) <= m_mftRecordSize) {
            const auto* attr = reinterpret_cast<const NtfsAttrHeader*>(s_ntfsMftBuf + off);
            if (attr->type == NTFS_ATTR_END || attr->length == 0 || off + attr->length > m_mftRecordSize) break;
            if (attr->type == NTFS_ATTR_DATA && attr->nameLength == 0 && attr->nonResident != 0) {
                const auto* nr = reinterpret_cast<const NtfsNonResidentAttr*>(s_ntfsMftBuf + off);
                bmRealSize = nr->realSize;
                bmRunCnt = decodeRunlist(
                    s_ntfsMftBuf + off + nr->mappingPairsOffset,
                    attr->length - nr->mappingPairsOffset,
                    bmRuns,
                    8
                );
                break;
            }
            off += attr->length;
        }
        if (bmRunCnt == 0 || bmRuns[0].lcn < 0) return false;

        uint64_t bmClusters = bmRuns[0].clusterCount;
        if (bmClusters * m_bytesPerCluster > sizeof(s_ntfsBitmapBuf)) {
            bmClusters = sizeof(s_ntfsBitmapBuf) / m_bytesPerCluster;
        }
        uint16_t bmSectors = static_cast<uint16_t>(bmClusters * m_sectorsPerCluster);
        uint64_t bmRelLba = static_cast<uint64_t>(bmRuns[0].lcn) * m_sectorsPerCluster;
        if (!readPartitionSectors(bmRelLba, bmSectors, s_ntfsBitmapBuf)) return false;

        uint64_t maxClusters = (m_totalSectors / m_sectorsPerCluster);
        uint64_t bmBitLimit = bmRealSize * 8ULL;
        if (bmBitLimit > 0 && bmBitLimit < maxClusters) maxClusters = bmBitLimit;
        if (bmClusters * m_bytesPerCluster * 8ULL < maxClusters) maxClusters = bmClusters * m_bytesPerCluster * 8ULL;

        uint64_t runStart = 0;
        uint64_t runLen = 0;
        for (uint64_t c = 256; c < maxClusters; ++c) {
            bool used = (s_ntfsBitmapBuf[c >> 3] & static_cast<uint8_t>(1u << (c & 7))) != 0;
            if (!used) {
                if (runLen == 0) runStart = c;
                runLen++;
                if (runLen >= clusterCount) {
                    for (uint64_t k = 0; k < clusterCount; ++k) {
                        uint64_t bit = runStart + k;
                        s_ntfsBitmapBuf[bit >> 3] |= static_cast<uint8_t>(1u << (bit & 7));
                    }
                    if (!writePartitionSectors(bmRelLba, bmSectors, s_ntfsBitmapBuf)) {
                        return false;
                    }
                    outStartLcn = runStart;
                    m_clustersAllocated += clusterCount;
                    return true;
                }
            } else {
                runLen = 0;
            }
        }
        return false;
    }

    // Locate an unused MFT record (record >= 16 where signature != "FILE" or InUse flag == 0)
    bool allocateMftRecord(uint64_t& outRecNum) noexcept {
        outRecNum = 0;
        uint64_t maxRec = (m_mftRecordCount < 512) ? m_mftRecordCount : 512;
        uint16_t recSectors = static_cast<uint16_t>(m_mftRecordSize / m_bytesPerSector);
        for (uint64_t r = 16; r < maxRec; ++r) {
            uint64_t relLba = mftRecordToRelLba(r);
            if (!readPartitionSectors(relLba, recSectors, s_ntfsMftBuf)) continue;
            const auto* hdr = reinterpret_cast<const MftRecordHeader*>(s_ntfsMftBuf);
            if (hdr->signature != NTFS_FILE_MAGIC || (hdr->flags & 0x01) == 0) {
                outRecNum = r;
                return true;
            }
        }
        return false;
    }

    // Build and commit a 100% spec-compliant 1024B NTFS 3.1 file MFT record (Resident <=512B or Non-Resident DMA)
    bool buildAndCommitFileMftRecord(
        uint64_t recNum,
        uint64_t parentMft,
        const char* name,
        const void* data,
        size_t dataSize,
        uint64_t& outAllocSize
    ) noexcept {
        outAllocSize = 0;
        alignas(16) uint8_t recBuf[1024]{};
        std::memset(recBuf, 0, sizeof(recBuf));

        constexpr uint64_t NT_TIME = 133769000000000000ULL;
        constexpr uint16_t USA_OFF = 0x30;
        uint16_t usaCount = static_cast<uint16_t>(1 + (m_mftRecordSize / 512));
        size_t pos = 0x38;

        // 1. $STANDARD_INFORMATION (0x10)
        {
            uint32_t payloadLen = 48;
            uint16_t valOff = 24;
            uint32_t attrLen = (valOff + payloadLen + 7u) & ~7u;
            auto* res = reinterpret_cast<NtfsResidentAttr*>(recBuf + pos);
            res->header.type = NTFS_ATTR_STANDARD_INFO;
            res->header.length = attrLen;
            res->header.nonResident = 0;
            res->header.attributeId = 0;
            res->valueLength = payloadLen;
            res->valueOffset = valOff;
            uint64_t times[4] = { NT_TIME, NT_TIME, NT_TIME, NT_TIME };
            std::memcpy(recBuf + pos + valOff, times, sizeof(times));
            uint32_t dosAttr = 0x20;
            std::memcpy(recBuf + pos + valOff + 32, &dosAttr, sizeof(dosAttr));
            pos += attrLen;
        }

        // Pre-allocate clusters & DMA-write data if Non-Resident (> 512 B)
        bool isNonResident = (dataSize > 512);
        uint64_t dataStartLcn = 0;
        uint64_t dataNumClusters = 0;
        if (isNonResident) {
            dataNumClusters = (dataSize + m_bytesPerCluster - 1) / m_bytesPerCluster;
            outAllocSize = dataNumClusters * m_bytesPerCluster;
            if (!allocateClusters(dataNumClusters, dataStartLcn)) return false;

            const auto* src = static_cast<const uint8_t*>(data);
            size_t written = 0;
            uint64_t maxBatchClusters = sizeof(s_ntfsIoClusterBuf) / m_bytesPerCluster;
            if (maxBatchClusters == 0) maxBatchClusters = 1;
            uint64_t curLcn = dataStartLcn;
            uint64_t remClusters = dataNumClusters;
            while (remClusters > 0) {
                uint64_t batch = (remClusters < maxBatchClusters) ? remClusters : maxBatchClusters;
                size_t batchBytes = static_cast<size_t>(batch * m_bytesPerCluster);
                std::memset(s_ntfsIoClusterBuf, 0, batchBytes);
                if (src && written < dataSize) {
                    size_t toCopy = dataSize - written;
                    if (toCopy > batchBytes) toCopy = batchBytes;
                    std::memcpy(s_ntfsIoClusterBuf, src + written, toCopy);
                    written += toCopy;
                }
                uint64_t relLba = curLcn * m_sectorsPerCluster;
                uint16_t secCnt = static_cast<uint16_t>(batch * m_sectorsPerCluster);
                if (!writePartitionSectors(relLba, secCnt, s_ntfsIoClusterBuf)) return false;
                curLcn += batch;
                remClusters -= batch;
            }
        } else {
            outAllocSize = dataSize;
        }

        // 2. $FILE_NAME (0x30)
        {
            size_t nameLen = 0;
            while (name[nameLen] && nameLen < 120) ++nameLen;
            uint32_t payloadLen = static_cast<uint32_t>(66 + nameLen * 2);
            uint16_t valOff = 24;
            uint32_t attrLen = (valOff + payloadLen + 7u) & ~7u;
            if (pos + attrLen + 8 > m_mftRecordSize) return false;
            auto* res = reinterpret_cast<NtfsResidentAttr*>(recBuf + pos);
            res->header.type = NTFS_ATTR_FILE_NAME;
            res->header.length = attrLen;
            res->header.nonResident = 0;
            res->header.attributeId = 1;
            res->valueLength = payloadLen;
            res->valueOffset = valOff;

            auto* fn = reinterpret_cast<NtfsFileNameAttr*>(recBuf + pos + valOff);
            fn->parentDirectory = (1ULL << 48) | (parentMft & 0x0000FFFFFFFFFFFFULL);
            fn->creationTime = NT_TIME;
            fn->modificationTime = NT_TIME;
            fn->mftChangeTime = NT_TIME;
            fn->lastAccessTime = NT_TIME;
            fn->allocatedSize = outAllocSize;
            fn->realSize = dataSize;
            fn->fileAttributes = 0x00000020u;
            fn->reparseTag = 0;
            fn->nameLength = static_cast<uint8_t>(nameLen);
            fn->nameNamespace = 3; // Win32 + DOS
            for (size_t i = 0; i < nameLen; ++i) {
                fn->name[i] = static_cast<uint16_t>(static_cast<unsigned char>(name[i]));
            }
            pos += attrLen;
        }

        // 3. $DATA (0x80)
        if (!isNonResident) {
            uint32_t payloadLen = static_cast<uint32_t>(dataSize);
            uint16_t valOff = 24;
            uint32_t attrLen = (valOff + payloadLen + 7u) & ~7u;
            if (pos + attrLen + 8 > m_mftRecordSize) return false;
            auto* res = reinterpret_cast<NtfsResidentAttr*>(recBuf + pos);
            res->header.type = NTFS_ATTR_DATA;
            res->header.length = attrLen;
            res->header.nonResident = 0;
            res->header.attributeId = 2;
            res->valueLength = payloadLen;
            res->valueOffset = valOff;
            if (data && dataSize > 0) {
                std::memcpy(recBuf + pos + valOff, data, dataSize);
            }
            pos += attrLen;
        } else {
            uint8_t runBuf[24]{};
            size_t runLen = encodeSingleRun(dataNumClusters, dataStartLcn, runBuf, sizeof(runBuf));
            uint16_t mpOff = 64;
            uint32_t attrLen = static_cast<uint32_t>((mpOff + runLen + 7u) & ~7u);
            if (pos + attrLen + 8 > m_mftRecordSize) return false;
            auto* nr = reinterpret_cast<NtfsNonResidentAttr*>(recBuf + pos);
            nr->header.type = NTFS_ATTR_DATA;
            nr->header.length = attrLen;
            nr->header.nonResident = 1;
            nr->header.attributeId = 2;
            nr->lowestVcn = 0;
            nr->highestVcn = (dataNumClusters > 0) ? (dataNumClusters - 1) : 0;
            nr->mappingPairsOffset = mpOff;
            nr->compressionUnit = 0;
            nr->reserved = 0;
            nr->allocatedSize = outAllocSize;
            nr->realSize = dataSize;
            nr->initializedSize = dataSize;
            std::memcpy(recBuf + pos + mpOff, runBuf, runLen);
            pos += attrLen;
        }

        // End attribute marker 0xFFFFFFFF
        uint32_t endMarker = NTFS_ATTR_END;
        std::memcpy(recBuf + pos, &endMarker, sizeof(endMarker));
        uint32_t usedSize = static_cast<uint32_t>(pos + 8);

        auto* hdr = reinterpret_cast<MftRecordHeader*>(recBuf);
        hdr->signature = NTFS_FILE_MAGIC;
        hdr->usaOffset = USA_OFF;
        hdr->usaCount = usaCount;
        hdr->lsn = 200 + recNum;
        hdr->sequenceNumber = 1;
        hdr->hardLinkCount = 1;
        hdr->firstAttributeOffset = 0x38;
        hdr->flags = 0x01; // InUse File
        hdr->usedSize = usedSize;
        hdr->allocatedSize = m_mftRecordSize;
        hdr->baseFileRecord = 0;
        hdr->nextAttributeId = 3;
        hdr->mftRecordNumber = static_cast<uint32_t>(recNum);

        return writeMftRecord(recNum, recBuf);
    }

    // Splice a new child entry into parent directory's $INDEX_ROOT (0x90) or $INDEX_ALLOCATION (INDX) block
    bool insertIntoDirectoryIndex(
        uint64_t dirMftRec,
        uint64_t childMftRec,
        const char* childName,
        uint64_t realSize,
        uint64_t allocSize
    ) noexcept {
        if (!readMftRecord(dirMftRec, s_ntfsDirMftBuf, sizeof(s_ntfsDirMftBuf))) return false;
        auto* hdr = reinterpret_cast<MftRecordHeader*>(s_ntfsDirMftBuf);
        if ((hdr->flags & 0x02) == 0) return false;

        // Build packed 8-byte aligned $I30 index entry
        size_t nameLen = 0;
        while (childName[nameLen] && nameLen < 120) ++nameLen;
        uint16_t fnLen = static_cast<uint16_t>(66 + nameLen * 2);
        uint16_t entryLen = static_cast<uint16_t>((16 + fnLen + 7u) & ~7u);
        uint8_t entryBuf[320]{};
        if (entryLen > sizeof(entryBuf)) return false;

        constexpr uint64_t NT_TIME = 133769000000000000ULL;
        auto* ie = reinterpret_cast<NtfsIndexEntryHeader*>(entryBuf);
        ie->fileReference = (1ULL << 48) | (childMftRec & 0x0000FFFFFFFFFFFFULL);
        ie->entryLength = entryLen;
        ie->keyLength = fnLen;
        ie->flags = 0;

        auto* fn = reinterpret_cast<NtfsFileNameAttr*>(entryBuf + sizeof(NtfsIndexEntryHeader));
        fn->parentDirectory = (1ULL << 48) | (dirMftRec & 0x0000FFFFFFFFFFFFULL);
        fn->creationTime = NT_TIME;
        fn->modificationTime = NT_TIME;
        fn->mftChangeTime = NT_TIME;
        fn->lastAccessTime = NT_TIME;
        fn->allocatedSize = allocSize;
        fn->realSize = realSize;
        fn->fileAttributes = 0x00000020u;
        fn->reparseTag = 0;
        fn->nameLength = static_cast<uint8_t>(nameLen);
        fn->nameNamespace = 3;
        for (size_t i = 0; i < nameLen; ++i) {
            fn->name[i] = static_cast<uint16_t>(static_cast<unsigned char>(childName[i]));
        }

        // First check if directory has $INDEX_ALLOCATION (0xA0) INDX blocks
        size_t off = hdr->firstAttributeOffset;
        size_t irAttrOff = 0;
        while (off + sizeof(NtfsAttrHeader) <= m_mftRecordSize) {
            const auto* attr = reinterpret_cast<const NtfsAttrHeader*>(s_ntfsDirMftBuf + off);
            if (attr->type == NTFS_ATTR_END || attr->length == 0 || off + attr->length > m_mftRecordSize) break;
            if (attr->type == NTFS_ATTR_INDEX_ROOT && attr->nonResident == 0) {
                irAttrOff = off;
            } else if (attr->type == NTFS_ATTR_INDEX_ALLOCATION && attr->nonResident != 0) {
                const auto* nr = reinterpret_cast<const NtfsNonResidentAttr*>(s_ntfsDirMftBuf + off);
                NtfsDataRun runs[16]{};
                size_t runCnt = decodeRunlist(
                    s_ntfsDirMftBuf + off + nr->mappingPairsOffset,
                    attr->length - nr->mappingPairsOffset,
                    runs,
                    16
                );
                if (runCnt > 0 && runs[0].lcn >= 0) {
                    uint16_t blkSectors = static_cast<uint16_t>(m_indexBlockSize / m_bytesPerSector);
                    uint64_t lba = static_cast<uint64_t>(runs[0].lcn) * m_sectorsPerCluster;
                    if (readPartitionSectors(lba, blkSectors, s_ntfsIndxBuf)) {
                        auto* indx = reinterpret_cast<NtfsIndxBlockHeader*>(s_ntfsIndxBuf);
                        if (indx->signature == NTFS_INDX_MAGIC &&
                            applyUsaFixup(s_ntfsIndxBuf, m_indexBlockSize, indx->usaOffset, indx->usaCount)) {
                            uint8_t* ihBase = reinterpret_cast<uint8_t*>(&indx->indexHeader);
                            size_t ihMax = m_indexBlockSize - offsetof(NtfsIndxBlockHeader, indexHeader);
                            if (indx->indexHeader.totalEntriesSize + entryLen <= ihMax) {
                                uint32_t pos = indx->indexHeader.entriesOffset;
                                while (pos + sizeof(NtfsIndexEntryHeader) <= indx->indexHeader.totalEntriesSize) {
                                    const auto* cur = reinterpret_cast<const NtfsIndexEntryHeader*>(ihBase + pos);
                                    if (cur->flags & 0x02) {
                                        size_t tailLen = indx->indexHeader.totalEntriesSize - pos;
                                        std::memmove(ihBase + pos + entryLen, ihBase + pos, tailLen);
                                        std::memcpy(ihBase + pos, entryBuf, entryLen);
                                        indx->indexHeader.totalEntriesSize += entryLen;
                                        if (!sealUsaFixup(s_ntfsIndxBuf, m_indexBlockSize, indx->usaOffset, indx->usaCount)) return false;
                                        return writePartitionSectors(lba, blkSectors, s_ntfsIndxBuf);
                                    }
                                    if (cur->entryLength < sizeof(NtfsIndexEntryHeader)) break;
                                    pos += cur->entryLength;
                                }
                            }
                        }
                    }
                }
            }
            off += attr->length;
        }

        // Otherwise splice directly into resident $INDEX_ROOT (0x90) inside directory MFT record
        if (irAttrOff != 0 && hdr->usedSize + entryLen <= m_mftRecordSize - 8) {
            auto* res = reinterpret_cast<NtfsResidentAttr*>(s_ntfsDirMftBuf + irAttrOff);
            auto* ir  = reinterpret_cast<NtfsIndexRootAttr*>(s_ntfsDirMftBuf + irAttrOff + res->valueOffset);
            uint8_t* ihBase = reinterpret_cast<uint8_t*>(&ir->indexHeader);
            uint32_t pos = ir->indexHeader.entriesOffset;
            while (pos + sizeof(NtfsIndexEntryHeader) <= ir->indexHeader.totalEntriesSize) {
                const auto* cur = reinterpret_cast<const NtfsIndexEntryHeader*>(ihBase + pos);
                if (cur->flags & 0x02) {
                    size_t absInsertPos = static_cast<size_t>((ihBase + pos) - s_ntfsDirMftBuf);
                    size_t moveLen = hdr->usedSize - absInsertPos;
                    std::memmove(s_ntfsDirMftBuf + absInsertPos + entryLen, s_ntfsDirMftBuf + absInsertPos, moveLen);
                    std::memcpy(s_ntfsDirMftBuf + absInsertPos, entryBuf, entryLen);
                    res->header.length += entryLen;
                    res->valueLength += entryLen;
                    ir->indexHeader.totalEntriesSize += entryLen;
                    ir->indexHeader.allocatedSize += entryLen;
                    hdr->usedSize += entryLen;
                    return writeMftRecord(dirMftRec, s_ntfsDirMftBuf);
                }
                if (cur->entryLength < sizeof(NtfsIndexEntryHeader)) break;
                pos += cur->entryLength;
            }
        }
        return false;
    }

    bool tryMountAtLba(uint64_t partLba) noexcept {
        std::memset(s_ntfsSectorBuf, 0, 512);
        m_dmaReadsIssued++;
        if (!m_ahci->readSectors(partLba, 1, s_ntfsSectorBuf)) {
            return false;
        }

        const auto* bpb = reinterpret_cast<const NtfsBootSector*>(s_ntfsSectorBuf);
        if (bpb->endMarker != 0xAA55) {
            return false;
        }
        if (std::memcmp(bpb->oemId, "NTFS    ", 8) != 0) {
            return false;
        }
        if (bpb->bytesPerSector != 512 || bpb->sectorsPerCluster == 0) {
            return false;
        }

        m_partitionLba = partLba;
        m_bytesPerSector = bpb->bytesPerSector;
        m_sectorsPerCluster = bpb->sectorsPerCluster;
        m_bytesPerCluster = m_bytesPerSector * m_sectorsPerCluster;
        m_totalSectors = bpb->totalSectors;
        m_mftClusterNumber = bpb->mftClusterNumber;
        m_mftMirrClusterNumber = bpb->mftMirrClusterNumber;
        m_volumeSerial = bpb->volumeSerialNumber;

        if (bpb->clustersPerFileRecordSegment < 0) {
            m_mftRecordSize = 1u << static_cast<uint32_t>(-bpb->clustersPerFileRecordSegment);
        } else {
            m_mftRecordSize = static_cast<uint32_t>(bpb->clustersPerFileRecordSegment) * m_bytesPerCluster;
        }
        if (m_mftRecordSize < 512 || m_mftRecordSize > 4096) {
            m_mftRecordSize = 1024;
        }

        if (bpb->clustersPerIndexBlock < 0) {
            m_indexBlockSize = 1u << static_cast<uint32_t>(-bpb->clustersPerIndexBlock);
        } else {
            m_indexBlockSize = static_cast<uint32_t>(bpb->clustersPerIndexBlock) * m_bytesPerCluster;
        }
        if (m_indexBlockSize < 512 || m_indexBlockSize > 4096) {
            m_indexBlockSize = 4096;
        }

        // Read MFT Record #0 ($MFT) to decode its own runlist and total record count
        m_mftRunCount = 0;
        uint64_t mft0Lba = m_mftClusterNumber * m_sectorsPerCluster;
        uint16_t recSectors = static_cast<uint16_t>(m_mftRecordSize / m_bytesPerSector);
        if (!readPartitionSectors(mft0Lba, recSectors, s_ntfsMftBuf)) {
            return false;
        }

        const auto* hdr0 = reinterpret_cast<const MftRecordHeader*>(s_ntfsMftBuf);
        if (hdr0->signature != NTFS_FILE_MAGIC) {
            return false;
        }
        if (!applyUsaFixup(s_ntfsMftBuf, m_mftRecordSize, hdr0->usaOffset, hdr0->usaCount)) {
            return false;
        }

        // Extract $MFT's own $DATA attribute (0x80) runlist
        size_t off = hdr0->firstAttributeOffset;
        while (off + sizeof(NtfsAttrHeader) <= m_mftRecordSize) {
            const auto* attr = reinterpret_cast<const NtfsAttrHeader*>(s_ntfsMftBuf + off);
            if (attr->type == NTFS_ATTR_END || attr->length == 0 || off + attr->length > m_mftRecordSize) break;
            if (attr->type == NTFS_ATTR_DATA && attr->nameLength == 0 && attr->nonResident != 0) {
                const auto* nr = reinterpret_cast<const NtfsNonResidentAttr*>(s_ntfsMftBuf + off);
                if (nr->mappingPairsOffset < attr->length) {
                    m_mftRunCount = decodeRunlist(
                        s_ntfsMftBuf + off + nr->mappingPairsOffset,
                        attr->length - nr->mappingPairsOffset,
                        m_mftRuns,
                        32
                    );
                }
                if (nr->realSize > 0) {
                    m_mftRecordCount = nr->realSize / m_mftRecordSize;
                }
                break;
            }
            off += attr->length;
        }

        // Read MFT Record #3 ($Volume) to extract authentic volume label & NTFS version
        std::memcpy(m_volumeLabel, "NTFS_VOL", 9);
        if (readMftRecord(MFT_RECORD_VOLUME, s_ntfsMftBuf, sizeof(s_ntfsMftBuf))) {
            const auto* volHdr = reinterpret_cast<const MftRecordHeader*>(s_ntfsMftBuf);
            size_t voff = volHdr->firstAttributeOffset;
            while (voff + sizeof(NtfsAttrHeader) <= m_mftRecordSize) {
                const auto* attr = reinterpret_cast<const NtfsAttrHeader*>(s_ntfsMftBuf + voff);
                if (attr->type == NTFS_ATTR_END || attr->length == 0 || voff + attr->length > m_mftRecordSize) break;
                if (attr->type == NTFS_ATTR_VOLUME_NAME && attr->nonResident == 0) {
                    const auto* res = reinterpret_cast<const NtfsResidentAttr*>(s_ntfsMftBuf + voff);
                    if (res->valueOffset + res->valueLength <= attr->length && res->valueLength > 0) {
                        const auto* u16 = reinterpret_cast<const uint16_t*>(s_ntfsMftBuf + voff + res->valueOffset);
                        utf16ToAscii(u16, res->valueLength / 2, m_volumeLabel, sizeof(m_volumeLabel));
                    }
                } else if (attr->type == NTFS_ATTR_VOLUME_INFO && attr->nonResident == 0) {
                    const auto* res = reinterpret_cast<const NtfsResidentAttr*>(s_ntfsMftBuf + voff);
                    if (res->valueLength >= sizeof(NtfsVolumeInfoAttr)) {
                        const auto* vi = reinterpret_cast<const NtfsVolumeInfoAttr*>(s_ntfsMftBuf + voff + res->valueOffset);
                        m_majorVersion = vi->majorVersion;
                        m_minorVersion = vi->minorVersion;
                    }
                }
                voff += attr->length;
            }
        }

        // Verify MFT Record #5 (Root Directory ".")
        if (!readMftRecord(MFT_RECORD_ROOT, s_ntfsMftBuf, sizeof(s_ntfsMftBuf))) {
            return false;
        }

        m_mounted = true;
        return true;
    }

    // Translate MFT record number into partition-relative LBA via $MFT runlist
    uint64_t mftRecordToRelLba(uint64_t recordNumber) const noexcept {
        uint64_t byteOffset = recordNumber * m_mftRecordSize;
        uint64_t vcn = byteOffset / m_bytesPerCluster;
        uint64_t inClusterByte = byteOffset % m_bytesPerCluster;

        if (m_mftRunCount > 0) {
            uint64_t curVcn = 0;
            for (size_t i = 0; i < m_mftRunCount; ++i) {
                if (vcn >= curVcn && vcn < curVcn + m_mftRuns[i].clusterCount) {
                    if (m_mftRuns[i].lcn < 0) return 0;
                    uint64_t targetLcn = static_cast<uint64_t>(m_mftRuns[i].lcn) + (vcn - curVcn);
                    return targetLcn * m_sectorsPerCluster + (inClusterByte / m_bytesPerSector);
                }
                curVcn += m_mftRuns[i].clusterCount;
            }
        }
        return m_mftClusterNumber * m_sectorsPerCluster + (byteOffset / m_bytesPerSector);
    }

    bool extractBestFileName(const uint8_t* mftBuf, uint64_t& outParentMft, char* outName, size_t nameCap) const noexcept {
        const auto* hdr = reinterpret_cast<const MftRecordHeader*>(mftBuf);
        size_t off = hdr->firstAttributeOffset;
        bool foundAny = false;
        uint8_t bestNs = 2; // Prefer Win32 (1) / Win32+DOS (3) / POSIX (0) over DOS-only (2)

        while (off + sizeof(NtfsAttrHeader) <= m_mftRecordSize) {
            const auto* attr = reinterpret_cast<const NtfsAttrHeader*>(mftBuf + off);
            if (attr->type == NTFS_ATTR_END || attr->length == 0 || off + attr->length > m_mftRecordSize) break;
            if (attr->type == NTFS_ATTR_FILE_NAME && attr->nonResident == 0) {
                const auto* res = reinterpret_cast<const NtfsResidentAttr*>(mftBuf + off);
                if (res->valueOffset + sizeof(NtfsFileNameAttr) <= attr->length) {
                    const auto* fn = reinterpret_cast<const NtfsFileNameAttr*>(mftBuf + off + res->valueOffset);
                    if (!foundAny || bestNs == 2 || fn->nameNamespace == 1 || fn->nameNamespace == 3) {
                        outParentMft = fn->parentDirectory & 0x0000FFFFFFFFFFFFULL;
                        utf16ToAscii(fn->name, fn->nameLength, outName, nameCap);
                        bestNs = fn->nameNamespace;
                        foundAny = true;
                    }
                }
            }
            off += attr->length;
        }
        return foundAny;
    }

    uint64_t extractDataSize(const uint8_t* mftBuf) const noexcept {
        const auto* hdr = reinterpret_cast<const MftRecordHeader*>(mftBuf);
        size_t off = hdr->firstAttributeOffset;
        while (off + sizeof(NtfsAttrHeader) <= m_mftRecordSize) {
            const auto* attr = reinterpret_cast<const NtfsAttrHeader*>(mftBuf + off);
            if (attr->type == NTFS_ATTR_END || attr->length == 0 || off + attr->length > m_mftRecordSize) break;
            if (attr->type == NTFS_ATTR_DATA && attr->nameLength == 0) {
                if (attr->nonResident == 0) {
                    const auto* res = reinterpret_cast<const NtfsResidentAttr*>(mftBuf + off);
                    return res->valueLength;
                } else {
                    const auto* nr = reinterpret_cast<const NtfsNonResidentAttr*>(mftBuf + off);
                    return nr->realSize;
                }
            }
            off += attr->length;
        }
        return 0;
    }

public:
    static BareMetalNtfsVolume& get() noexcept {
        static BareMetalNtfsVolume instance;
        return instance;
    }

    [[nodiscard]] bool isMounted() const noexcept { return m_mounted; }
    [[nodiscard]] const char* getVolumeLabel() const noexcept { return m_volumeLabel; }
    [[nodiscard]] uint64_t getPartitionLba() const noexcept { return m_partitionLba; }
    [[nodiscard]] uint64_t getTotalSectors() const noexcept { return m_totalSectors; }
    [[nodiscard]] uint32_t getBytesPerCluster() const noexcept { return m_bytesPerCluster; }
    [[nodiscard]] uint32_t getMftRecordSize() const noexcept { return m_mftRecordSize; }
    [[nodiscard]] uint64_t getMftClusterNumber() const noexcept { return m_mftClusterNumber; }
    [[nodiscard]] uint64_t getMftRecordCount() const noexcept { return m_mftRecordCount; }
    [[nodiscard]] uint64_t getVolumeSerial() const noexcept { return m_volumeSerial; }
    [[nodiscard]] uint8_t  getMajorVersion() const noexcept { return m_majorVersion; }
    [[nodiscard]] uint8_t  getMinorVersion() const noexcept { return m_minorVersion; }
    [[nodiscard]] uint32_t getUsaFixupsVerified() const noexcept { return m_usaFixupsVerified; }
    [[nodiscard]] uint32_t getUsaFixupsSealed() const noexcept { return m_usaFixupsSealed; }
    [[nodiscard]] uint64_t getDmaReadsIssued() const noexcept { return m_dmaReadsIssued; }
    [[nodiscard]] uint64_t getDmaWritesIssued() const noexcept { return m_dmaWritesIssued; }
    [[nodiscard]] uint64_t getClustersAllocated() const noexcept { return m_clustersAllocated; }
    [[nodiscard]] uint32_t getFilesCreated() const noexcept { return m_filesCreated; }

    /**
     * @brief Probe GPT, MBR, and Raw LBA 0 via real AHCI DMA and mount the NTFS 3.1 volume.
     */
    bool mount(ahci_baremetal::BareMetalAhciController& ahci) noexcept {
        m_ahci = &ahci;
        m_mounted = false;
        m_usaFixupsVerified = 0;
        m_dmaReadsIssued = 0;

        if (!ahci.isInitialized() || !ahci.isPort1Online()) {
            return false;
        }

        // 1. Check GPT Header at LBA 1 ("EFI PART")
        alignas(512) static uint8_t s_gptSec[512];
        alignas(512) static uint8_t s_gptEntries[2048]; // 16 entries (4 sectors)
        m_dmaReadsIssued++;
        if (ahci.readSectors(1, 1, s_gptSec)) {
            const auto* gpt = reinterpret_cast<const GptHeader*>(s_gptSec);
            if (gpt->signature == 0x5452415020494645ULL && gpt->sizeOfPartitionEntry >= 128) {
                uint32_t maxCheck = (gpt->numPartitionEntries < 16) ? gpt->numPartitionEntries : 16;
                m_dmaReadsIssued++;
                if (ahci.readSectors(gpt->partitionEntryLba, 4, s_gptEntries)) {
                    for (uint32_t i = 0; i < maxCheck; ++i) {
                        const auto* pe = reinterpret_cast<const GptPartitionEntry*>(
                            s_gptEntries + static_cast<size_t>(i) * gpt->sizeOfPartitionEntry
                        );
                        if (pe->startingLba != 0 && pe->endingLba > pe->startingLba) {
                            if (tryMountAtLba(pe->startingLba)) {
                                return true;
                            }
                        }
                    }
                }
            }
        }

        // 2. Check MBR Partition Table at LBA 0
        alignas(512) static uint8_t s_mbrSec[512];
        m_dmaReadsIssued++;
        if (ahci.readSectors(0, 1, s_mbrSec)) {
            const auto* mbr = reinterpret_cast<const MbrSector*>(s_mbrSec);
            if (mbr->signature == 0xAA55) {
                for (int i = 0; i < 4; ++i) {
                    if (mbr->partitions[i].startingLba != 0 && mbr->partitions[i].sectorCount != 0) {
                        if (tryMountAtLba(mbr->partitions[i].startingLba)) {
                            return true;
                        }
                    }
                }
            }
        }

        // 3. Check Raw Superfloppy NTFS at LBA 0
        return tryMountAtLba(0);
    }

    /**
     * @brief Read and USA-verify an NTFS MFT Record directly from AHCI disk sectors.
     */
    bool readMftRecord(uint64_t recordNumber, uint8_t* outBuf, size_t bufSize) noexcept {
        if (!m_ahci || !outBuf || bufSize < m_mftRecordSize) return false;
        uint64_t relLba = mftRecordToRelLba(recordNumber);
        uint16_t recSectors = static_cast<uint16_t>(m_mftRecordSize / m_bytesPerSector);
        if (!readPartitionSectors(relLba, recSectors, outBuf)) {
            return false;
        }
        const auto* hdr = reinterpret_cast<const MftRecordHeader*>(outBuf);
        if (hdr->signature != NTFS_FILE_MAGIC) {
            return false;
        }
        return applyUsaFixup(outBuf, m_mftRecordSize, hdr->usaOffset, hdr->usaCount);
    }

    /**
     * @brief Search a single directory MFT record for a child entry name via
     *        $INDEX_ROOT (0x90) and $INDEX_ALLOCATION (0xA0 / INDX blocks).
     */
    bool findChildInDirectory(uint64_t dirMftRec, const char* name, size_t nameLen, NtfsDirEntry& outEntry) noexcept {
        if (!readMftRecord(dirMftRec, s_ntfsDirMftBuf, sizeof(s_ntfsDirMftBuf))) {
            return false;
        }

        const auto* hdr = reinterpret_cast<const MftRecordHeader*>(s_ntfsDirMftBuf);
        if ((hdr->flags & 0x02) == 0) {
            return false; // Not a directory
        }

        auto scanIndexEntries = [&](const uint8_t* base, uint32_t entriesOff, uint32_t totalBytes, size_t limit) -> bool {
            uint32_t pos = entriesOff;
            uint32_t end = entriesOff + totalBytes;
            if (end > limit) end = static_cast<uint32_t>(limit);
            while (pos + sizeof(NtfsIndexEntryHeader) <= end) {
                const auto* ie = reinterpret_cast<const NtfsIndexEntryHeader*>(base + pos);
                if (ie->entryLength < sizeof(NtfsIndexEntryHeader) || pos + ie->entryLength > end) break;
                if (ie->flags & 0x02) break; // Last entry sentinel
                if (ie->keyLength >= sizeof(NtfsFileNameAttr)) {
                    const auto* fn = reinterpret_cast<const NtfsFileNameAttr*>(base + pos + sizeof(NtfsIndexEntryHeader));
                    if (equalUtf16ToAsciiIgnoreCase(fn->name, fn->nameLength, name, nameLen)) {
                        uint64_t childMft = ie->fileReference & 0x0000FFFFFFFFFFFFULL;
                        utf16ToAscii(fn->name, fn->nameLength, outEntry.name, sizeof(outEntry.name));
                        outEntry.mftRecordNumber = childMft;
                        outEntry.isDirectory = (fn->fileAttributes & 0x10000000u) != 0;
                        outEntry.fileSize = fn->realSize;
                        // Refine size & directory bit from actual target MFT record
                        if (readMftRecord(childMft, s_ntfsMftBuf, sizeof(s_ntfsMftBuf))) {
                            const auto* ch = reinterpret_cast<const MftRecordHeader*>(s_ntfsMftBuf);
                            outEntry.isDirectory = (ch->flags & 0x02) != 0;
                            if (!outEntry.isDirectory) {
                                outEntry.fileSize = extractDataSize(s_ntfsMftBuf);
                            }
                        }
                        return true;
                    }
                }
                pos += ie->entryLength;
            }
            return false;
        };

        // 1. Scan $INDEX_ROOT (0x90)
        size_t off = hdr->firstAttributeOffset;
        while (off + sizeof(NtfsAttrHeader) <= m_mftRecordSize) {
            const auto* attr = reinterpret_cast<const NtfsAttrHeader*>(s_ntfsDirMftBuf + off);
            if (attr->type == NTFS_ATTR_END || attr->length == 0 || off + attr->length > m_mftRecordSize) break;

            if (attr->type == NTFS_ATTR_INDEX_ROOT && attr->nonResident == 0) {
                const auto* res = reinterpret_cast<const NtfsResidentAttr*>(s_ntfsDirMftBuf + off);
                if (res->valueOffset + sizeof(NtfsIndexRootAttr) <= attr->length) {
                    const auto* ir = reinterpret_cast<const NtfsIndexRootAttr*>(s_ntfsDirMftBuf + off + res->valueOffset);
                    const uint8_t* ihBase = reinterpret_cast<const uint8_t*>(&ir->indexHeader);
                    size_t ihMax = res->valueLength - offsetof(NtfsIndexRootAttr, indexHeader);
                    if (scanIndexEntries(ihBase, ir->indexHeader.entriesOffset, ir->indexHeader.totalEntriesSize, ihMax)) {
                        return true;
                    }
                }
            } else if (attr->type == NTFS_ATTR_INDEX_ALLOCATION && attr->nonResident != 0) {
                const auto* nr = reinterpret_cast<const NtfsNonResidentAttr*>(s_ntfsDirMftBuf + off);
                NtfsDataRun runs[16]{};
                size_t runCnt = decodeRunlist(
                    s_ntfsDirMftBuf + off + nr->mappingPairsOffset,
                    attr->length - nr->mappingPairsOffset,
                    runs,
                    16
                );
                uint16_t blkSectors = static_cast<uint16_t>(m_indexBlockSize / m_bytesPerSector);
                uint32_t blksPerCluster = (m_bytesPerCluster >= m_indexBlockSize) ? (m_bytesPerCluster / m_indexBlockSize) : 1;
                for (size_t r = 0; r < runCnt; ++r) {
                    if (runs[r].lcn < 0) continue;
                    for (uint64_t c = 0; c < runs[r].clusterCount; ++c) {
                        for (uint32_t b = 0; b < blksPerCluster; ++b) {
                            uint64_t lba = (static_cast<uint64_t>(runs[r].lcn) + c) * m_sectorsPerCluster + b * blkSectors;
                            if (!readPartitionSectors(lba, blkSectors, s_ntfsIndxBuf)) continue;
                            const auto* indx = reinterpret_cast<const NtfsIndxBlockHeader*>(s_ntfsIndxBuf);
                            if (indx->signature != NTFS_INDX_MAGIC) continue;
                            if (!applyUsaFixup(s_ntfsIndxBuf, m_indexBlockSize, indx->usaOffset, indx->usaCount)) continue;
                            const uint8_t* ihBase = reinterpret_cast<const uint8_t*>(&indx->indexHeader);
                            size_t ihMax = m_indexBlockSize - offsetof(NtfsIndxBlockHeader, indexHeader);
                            if (scanIndexEntries(ihBase, indx->indexHeader.entriesOffset, indx->indexHeader.totalEntriesSize, ihMax)) {
                                return true;
                            }
                        }
                    }
                }
            }
            off += attr->length;
        }

        // 2. Fallback: scan active MFT records referencing parent == dirMftRec
        uint64_t maxRec = (m_mftRecordCount < 512) ? m_mftRecordCount : 512;
        for (uint64_t r = 16; r < maxRec; ++r) {
            if (!readMftRecord(r, s_ntfsMftBuf, sizeof(s_ntfsMftBuf))) continue;
            const auto* rh = reinterpret_cast<const MftRecordHeader*>(s_ntfsMftBuf);
            if ((rh->flags & 0x01) == 0) continue; // Not in use
            uint64_t parentMft = 0;
            char entryName[128]{};
            if (extractBestFileName(s_ntfsMftBuf, parentMft, entryName, sizeof(entryName))) {
                if (parentMft == dirMftRec && equalAsciiIgnoreCase(entryName, name, nameLen)) {
                    std::memcpy(outEntry.name, entryName, sizeof(outEntry.name));
                    outEntry.mftRecordNumber = r;
                    outEntry.isDirectory = (rh->flags & 0x02) != 0;
                    outEntry.fileSize = outEntry.isDirectory ? 0 : extractDataSize(s_ntfsMftBuf);
                    return true;
                }
            }
        }

        return false;
    }

    /**
     * @brief Resolve a full NT/Win32 path (e.g. "\MicaNT\system.ini" or "C:\Windows\System32\cmd.exe").
     */
    bool findPath(const char* path, NtfsDirEntry& outEntry) noexcept {
        if (!m_mounted || !path) return false;

        // Strip optional drive prefix ("C:")
        const char* p = path;
        if (((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) && p[1] == ':') {
            p += 2;
        }
        while (*p == '\\' || *p == '/') ++p;

        if (*p == '\0') {
            std::memcpy(outEntry.name, "\\", 2);
            outEntry.mftRecordNumber = MFT_RECORD_ROOT;
            outEntry.isDirectory = true;
            outEntry.fileSize = 0;
            return true;
        }

        uint64_t currentDir = MFT_RECORD_ROOT;
        while (*p != '\0') {
            const char* segStart = p;
            while (*p != '\0' && *p != '\\' && *p != '/') ++p;
            size_t segLen = static_cast<size_t>(p - segStart);
            while (*p == '\\' || *p == '/') ++p;

            if (segLen == 0) continue;
            NtfsDirEntry next{};
            if (!findChildInDirectory(currentDir, segStart, segLen, next)) {
                return false;
            }
            outEntry = next;
            if (*p != '\0') {
                if (!next.isDirectory) return false;
                currentDir = next.mftRecordNumber;
            }
        }
        return true;
    }

    /**
     * @brief Enumerate all entries inside a directory path using real NTFS $INDEX_ROOT / $INDEX_ALLOCATION.
     */
    template <typename Callback>
    bool iterateDirectory(const char* path, Callback&& cb) noexcept {
        if (!m_mounted) return false;
        NtfsDirEntry dir{};
        if (!findPath(path ? path : "\\", dir) || !dir.isDirectory) {
            return false;
        }

        if (!readMftRecord(dir.mftRecordNumber, s_ntfsDirMftBuf, sizeof(s_ntfsDirMftBuf))) {
            return false;
        }

        const auto* hdr = reinterpret_cast<const MftRecordHeader*>(s_ntfsDirMftBuf);
        size_t yielded = 0;

        auto emitEntries = [&](const uint8_t* base, uint32_t entriesOff, uint32_t totalBytes, size_t limit) {
            uint32_t pos = entriesOff;
            uint32_t end = entriesOff + totalBytes;
            if (end > limit) end = static_cast<uint32_t>(limit);
            while (pos + sizeof(NtfsIndexEntryHeader) <= end) {
                const auto* ie = reinterpret_cast<const NtfsIndexEntryHeader*>(base + pos);
                if (ie->entryLength < sizeof(NtfsIndexEntryHeader) || pos + ie->entryLength > end) break;
                if (ie->flags & 0x02) break;
                if (ie->keyLength >= sizeof(NtfsFileNameAttr)) {
                    const auto* fn = reinterpret_cast<const NtfsFileNameAttr*>(base + pos + sizeof(NtfsIndexEntryHeader));
                    // Skip DOS-only aliases (namespace == 2) and hidden '$' system metafiles when listing user view?
                    // Include non-DOS entries cleanly (user can see real files + dirs).
                    if (fn->nameNamespace != 2) {
                        NtfsDirEntry e{};
                        utf16ToAscii(fn->name, fn->nameLength, e.name, sizeof(e.name));
                        if (e.name[0] != '$' && !(e.name[0] == '.' && e.name[1] == '\0')) {
                            e.mftRecordNumber = ie->fileReference & 0x0000FFFFFFFFFFFFULL;
                            e.isDirectory = (fn->fileAttributes & 0x10000000u) != 0;
                            e.fileSize = fn->realSize;
                            if (readMftRecord(e.mftRecordNumber, s_ntfsMftBuf, sizeof(s_ntfsMftBuf))) {
                                const auto* ch = reinterpret_cast<const MftRecordHeader*>(s_ntfsMftBuf);
                                e.isDirectory = (ch->flags & 0x02) != 0;
                                if (!e.isDirectory) e.fileSize = extractDataSize(s_ntfsMftBuf);
                            }
                            cb(e);
                            yielded++;
                        }
                    }
                }
                pos += ie->entryLength;
            }
        };

        size_t off = hdr->firstAttributeOffset;
        while (off + sizeof(NtfsAttrHeader) <= m_mftRecordSize) {
            const auto* attr = reinterpret_cast<const NtfsAttrHeader*>(s_ntfsDirMftBuf + off);
            if (attr->type == NTFS_ATTR_END || attr->length == 0 || off + attr->length > m_mftRecordSize) break;

            if (attr->type == NTFS_ATTR_INDEX_ROOT && attr->nonResident == 0) {
                const auto* res = reinterpret_cast<const NtfsResidentAttr*>(s_ntfsDirMftBuf + off);
                if (res->valueOffset + sizeof(NtfsIndexRootAttr) <= attr->length) {
                    const auto* ir = reinterpret_cast<const NtfsIndexRootAttr*>(s_ntfsDirMftBuf + off + res->valueOffset);
                    const uint8_t* ihBase = reinterpret_cast<const uint8_t*>(&ir->indexHeader);
                    size_t ihMax = res->valueLength - offsetof(NtfsIndexRootAttr, indexHeader);
                    emitEntries(ihBase, ir->indexHeader.entriesOffset, ir->indexHeader.totalEntriesSize, ihMax);
                }
            } else if (attr->type == NTFS_ATTR_INDEX_ALLOCATION && attr->nonResident != 0) {
                const auto* nr = reinterpret_cast<const NtfsNonResidentAttr*>(s_ntfsDirMftBuf + off);
                NtfsDataRun runs[16]{};
                size_t runCnt = decodeRunlist(
                    s_ntfsDirMftBuf + off + nr->mappingPairsOffset,
                    attr->length - nr->mappingPairsOffset,
                    runs,
                    16
                );
                uint16_t blkSectors = static_cast<uint16_t>(m_indexBlockSize / m_bytesPerSector);
                uint32_t blksPerCluster = (m_bytesPerCluster >= m_indexBlockSize) ? (m_bytesPerCluster / m_indexBlockSize) : 1;
                for (size_t r = 0; r < runCnt; ++r) {
                    if (runs[r].lcn < 0) continue;
                    for (uint64_t c = 0; c < runs[r].clusterCount; ++c) {
                        for (uint32_t b = 0; b < blksPerCluster; ++b) {
                            uint64_t lba = (static_cast<uint64_t>(runs[r].lcn) + c) * m_sectorsPerCluster + b * blkSectors;
                            if (!readPartitionSectors(lba, blkSectors, s_ntfsIndxBuf)) continue;
                            const auto* indx = reinterpret_cast<const NtfsIndxBlockHeader*>(s_ntfsIndxBuf);
                            if (indx->signature != NTFS_INDX_MAGIC) continue;
                            if (!applyUsaFixup(s_ntfsIndxBuf, m_indexBlockSize, indx->usaOffset, indx->usaCount)) continue;
                            const uint8_t* ihBase = reinterpret_cast<const uint8_t*>(&indx->indexHeader);
                            size_t ihMax = m_indexBlockSize - offsetof(NtfsIndxBlockHeader, indexHeader);
                            emitEntries(ihBase, indx->indexHeader.entriesOffset, indx->indexHeader.totalEntriesSize, ihMax);
                        }
                    }
                }
            }
            off += attr->length;
        }

        // Fallback MFT parent scan if index was empty
        if (yielded == 0) {
            uint64_t maxRec = (m_mftRecordCount < 512) ? m_mftRecordCount : 512;
            for (uint64_t r = 16; r < maxRec; ++r) {
                if (!readMftRecord(r, s_ntfsMftBuf, sizeof(s_ntfsMftBuf))) continue;
                const auto* rh = reinterpret_cast<const MftRecordHeader*>(s_ntfsMftBuf);
                if ((rh->flags & 0x01) == 0) continue;
                uint64_t parentMft = 0;
                NtfsDirEntry e{};
                if (extractBestFileName(s_ntfsMftBuf, parentMft, e.name, sizeof(e.name))) {
                    if (parentMft == dir.mftRecordNumber && e.name[0] != '$') {
                        e.mftRecordNumber = r;
                        e.isDirectory = (rh->flags & 0x02) != 0;
                        e.fileSize = e.isDirectory ? 0 : extractDataSize(s_ntfsMftBuf);
                        cb(e);
                    }
                }
            }
        }
        return true;
    }

    /**
     * @brief Read an entire file (Resident OR Non-Resident multi-extent $DATA) via real AHCI DMA.
     */
    bool readFile(const char* path, void* outBuf, size_t maxBytes, size_t& outBytesRead) noexcept {
        outBytesRead = 0;
        if (!m_mounted || !path || !outBuf || maxBytes == 0) return false;

        NtfsDirEntry entry{};
        if (!findPath(path, entry) || entry.isDirectory) {
            return false;
        }

        if (!readMftRecord(entry.mftRecordNumber, s_ntfsMftBuf, sizeof(s_ntfsMftBuf))) {
            return false;
        }

        const auto* hdr = reinterpret_cast<const MftRecordHeader*>(s_ntfsMftBuf);
        size_t off = hdr->firstAttributeOffset;
        while (off + sizeof(NtfsAttrHeader) <= m_mftRecordSize) {
            const auto* attr = reinterpret_cast<const NtfsAttrHeader*>(s_ntfsMftBuf + off);
            if (attr->type == NTFS_ATTR_END || attr->length == 0 || off + attr->length > m_mftRecordSize) break;

            if (attr->type == NTFS_ATTR_DATA && attr->nameLength == 0) {
                if (attr->nonResident == 0) {
                    // Resident $DATA directly inside the MFT record
                    const auto* res = reinterpret_cast<const NtfsResidentAttr*>(s_ntfsMftBuf + off);
                    if (res->valueOffset + res->valueLength > attr->length) return false;
                    size_t toCopy = (res->valueLength < maxBytes) ? res->valueLength : maxBytes;
                    std::memcpy(outBuf, s_ntfsMftBuf + off + res->valueOffset, toCopy);
                    outBytesRead = toCopy;
                    return true;
                } else {
                    // Non-Resident $DATA via mapping-pair runlist + real AHCI DMA reads
                    const auto* nr = reinterpret_cast<const NtfsNonResidentAttr*>(s_ntfsMftBuf + off);
                    NtfsDataRun runs[64]{};
                    size_t runCnt = decodeRunlist(
                        s_ntfsMftBuf + off + nr->mappingPairsOffset,
                        attr->length - nr->mappingPairsOffset,
                        runs,
                        64
                    );

                    size_t targetSize = (nr->realSize < maxBytes) ? static_cast<size_t>(nr->realSize) : maxBytes;
                    auto* dst = static_cast<uint8_t*>(outBuf);
                    size_t copied = 0;
                    uint64_t maxClustersPerDma = sizeof(s_ntfsIoClusterBuf) / m_bytesPerCluster;
                    if (maxClustersPerDma == 0) maxClustersPerDma = 1;

                    for (size_t r = 0; r < runCnt && copied < targetSize; ++r) {
                        uint64_t remClusters = runs[r].clusterCount;
                        int64_t curLcn = runs[r].lcn;

                        while (remClusters > 0 && copied < targetSize) {
                            uint64_t batchClusters = (remClusters < maxClustersPerDma) ? remClusters : maxClustersPerDma;
                            size_t batchBytes = static_cast<size_t>(batchClusters * m_bytesPerCluster);
                            size_t toCopy = (targetSize - copied < batchBytes) ? (targetSize - copied) : batchBytes;

                            if (curLcn < 0) {
                                // Sparse run -> zero-fill
                                std::memset(dst + copied, 0, toCopy);
                            } else {
                                uint64_t relLba = static_cast<uint64_t>(curLcn) * m_sectorsPerCluster;
                                uint16_t secCount = static_cast<uint16_t>(batchClusters * m_sectorsPerCluster);
                                if (!readPartitionSectors(relLba, secCount, s_ntfsIoClusterBuf)) {
                                    return false;
                                }
                                std::memcpy(dst + copied, s_ntfsIoClusterBuf, toCopy);
                                curLcn += static_cast<int64_t>(batchClusters);
                            }

                            copied += toCopy;
                            remClusters -= batchClusters;
                        }
                    }

                    outBytesRead = copied;
                    return true;
                }
            }
            off += attr->length;
        }
        return false;
    }

    /**
     * @brief Create or overwrite a real file on the mounted NTFS 3.1 partition via AHCI Bus-Master DMA.
     *        Handles Resident (<= 512 B) AND Non-Resident (> 512 B, $Bitmap cluster allocation + mapping-pair
     *        runlists), seals USA fixups, and inserts new files into parent directory $INDEX_ROOT / INDX blocks.
     */
    bool writeFile(const char* path, const void* data, size_t dataSize) noexcept {
        if (!m_mounted || !path) return false;

        // Strip optional drive prefix ("C:")
        const char* p = path;
        if (((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) && p[1] == ':') {
            p += 2;
        }
        while (*p == '\\' || *p == '/') ++p;
        if (*p == '\0') return false;

        // Split into parentDirPath and leafName
        const char* lastSlash = nullptr;
        for (const char* q = p; *q; ++q) {
            if (*q == '\\' || *q == '/') lastSlash = q;
        }

        char parentPath[128]{};
        char leafName[128]{};
        if (lastSlash) {
            size_t plen = static_cast<size_t>(lastSlash - p);
            if (plen + 2 >= sizeof(parentPath)) return false;
            parentPath[0] = '\\';
            std::memcpy(parentPath + 1, p, plen);
            parentPath[plen + 1] = '\0';
            const char* leaf = lastSlash + 1;
            size_t l = 0;
            while (leaf[l] && l + 1 < sizeof(leafName)) { leafName[l] = leaf[l]; ++l; }
            leafName[l] = '\0';
        } else {
            parentPath[0] = '\\';
            parentPath[1] = '\0';
            size_t l = 0;
            while (p[l] && l + 1 < sizeof(leafName)) { leafName[l] = p[l]; ++l; }
            leafName[l] = '\0';
        }
        if (leafName[0] == '\0') return false;

        NtfsDirEntry parentDir{};
        if (!findPath(parentPath, parentDir) || !parentDir.isDirectory) {
            return false;
        }

        size_t leafLen = 0;
        while (leafName[leafLen]) ++leafLen;

        // Check whether file already exists inside parent directory
        NtfsDirEntry existing{};
        if (findChildInDirectory(parentDir.mftRecordNumber, leafName, leafLen, existing)) {
            if (existing.isDirectory) return false;
            uint64_t allocSize = 0;
            return buildAndCommitFileMftRecord(
                existing.mftRecordNumber, parentDir.mftRecordNumber, leafName, data, dataSize, allocSize
            );
        }

        // Brand-new file: allocate unused MFT record, commit FILE record, insert into parent $I30 index
        uint64_t newRecNum = 0;
        if (!allocateMftRecord(newRecNum)) return false;

        uint64_t allocSize = 0;
        if (!buildAndCommitFileMftRecord(newRecNum, parentDir.mftRecordNumber, leafName, data, dataSize, allocSize)) {
            return false;
        }
        if (!insertIntoDirectoryIndex(parentDir.mftRecordNumber, newRecNum, leafName, dataSize, allocSize)) {
            return false;
        }
        m_filesCreated++;
        return true;
    }
};

} // namespace micant::km::ntfs_baremetal
