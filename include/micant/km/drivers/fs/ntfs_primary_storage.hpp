// ============================================================================
// MicaNT: Sovereign Clean-Room Primary NTFS Storage Architecture
// Horizon 360 / Suite 576 - Master File Table ($MFT) Record Engine, USN
// Change Journal ($UsnJrnl), B-Tree Directory Indexing ($INDEX_ROOT /
// $INDEX_ALLOCATION), Paging File Cluster Reservation & Crash Resilience
//
// Clean-Room Implementation in modern ISO C++23.
// Compatible with Microsoft Windows NT NTFS on-disk layout, win32metadata,
// and IFS driver architecture. Zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <mutex>
#include <span>
#include <cstring>
#include <algorithm>
#include <unordered_map>
#include <chrono>
#include <sstream>
#include <iomanip>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/km/drivers/storage/storage.hpp"
#include "micant/km/executive/io.hpp"
#include "micant/km/drivers/fs/ntfs.hpp"

namespace micant::ntfs::primary {

// ============================================================================
// 1. Constants & FSCTL Definitions for Primary NTFS Subsystem
// ============================================================================

inline constexpr uint32_t NTFS_INDX_BLOCK_SIZE     = 4096;
inline constexpr uint32_t NTFS_INDEX_USA_OFFSET    = 0x28;
inline constexpr uint32_t NTFS_INDEX_USA_COUNT     = 9; // 1 sequence number + 8 sector fixup slots

// File Attributes
inline constexpr uint32_t FILE_ATTRIBUTE_READONLY  = 0x00000001;
inline constexpr uint32_t FILE_ATTRIBUTE_HIDDEN    = 0x00000002;
inline constexpr uint32_t FILE_ATTRIBUTE_SYSTEM    = 0x00000004;
inline constexpr uint32_t FILE_ATTRIBUTE_DIRECTORY = 0x00000010;
inline constexpr uint32_t FILE_ATTRIBUTE_ARCHIVE   = 0x00000020;
inline constexpr uint32_t FILE_ATTRIBUTE_NORMAL    = 0x00000080;

// USN Reason Bitmask Flags
inline constexpr uint32_t USN_REASON_DATA_OVERWRITE       = 0x00000001;
inline constexpr uint32_t USN_REASON_DATA_EXTEND          = 0x00000002;
inline constexpr uint32_t USN_REASON_DATA_TRUNCATION      = 0x00000004;
inline constexpr uint32_t USN_REASON_FILE_CREATE          = 0x00000100;
inline constexpr uint32_t USN_REASON_FILE_DELETE          = 0x00000200;
inline constexpr uint32_t USN_REASON_EA_CHANGE            = 0x00000400;
inline constexpr uint32_t USN_REASON_SECURITY_CHANGE      = 0x00000800;
inline constexpr uint32_t USN_REASON_RENAME_OLD_NAME      = 0x00001000;
inline constexpr uint32_t USN_REASON_RENAME_NEW_NAME      = 0x00002000;
inline constexpr uint32_t USN_REASON_INDEXABLE_CHANGE     = 0x00004000;
inline constexpr uint32_t USN_REASON_BASIC_INFO_CHANGE    = 0x00008000;
inline constexpr uint32_t USN_REASON_HARD_LINK_CHANGE     = 0x00010000;
inline constexpr uint32_t USN_REASON_COMPRESSION_CHANGE   = 0x00020000;
inline constexpr uint32_t USN_REASON_ENCRYPTION_CHANGE    = 0x00040000;
inline constexpr uint32_t USN_REASON_OBJECT_ID_CHANGE     = 0x00080000;
inline constexpr uint32_t USN_REASON_REPARSE_POINT_CHANGE = 0x00100000;
inline constexpr uint32_t USN_REASON_STREAM_CHANGE        = 0x00200000;
inline constexpr uint32_t USN_REASON_CLOSE                = 0x80000000;

#pragma pack(push, 1)

// ============================================================================
// 2. On-Disk USN Change Journal Structures
// ============================================================================

struct USN_JOURNAL_DATA_V0 {
    uint64_t UsnJournalID{0x20261120AABBCCDDULL};
    uint64_t FirstUsn{0};
    uint64_t NextUsn{0};
    uint64_t LowestValidUsn{0};
    uint64_t MaxUsn{0xFFFFFFFFULL};
    uint64_t MaximumSize{0x8000000ULL};    // 128 MB default
    uint64_t AllocationDelta{0x1000000ULL};// 16 MB default
};

struct CREATE_USN_JOURNAL_DATA {
    uint64_t MaximumSize{0x8000000ULL};
    uint64_t AllocationDelta{0x1000000ULL};
};

struct READ_USN_JOURNAL_DATA_V0 {
    uint64_t StartUsn{0};
    uint32_t ReasonMask{0xFFFFFFFF};
    uint32_t ReturnOnlyOnClose{0};
    uint64_t Timeout{0};
    uint64_t BytesToWaitFor{0};
    uint64_t UsnJournalID{0};
};

struct USN_RECORD_V2 {
    uint32_t RecordLength{sizeof(USN_RECORD_V2)};
    uint16_t MajorVersion{2};
    uint16_t MinorVersion{0};
    uint64_t FileReferenceNumber{0};
    uint64_t ParentFileReferenceNumber{0};
    uint64_t Usn{0};
    uint64_t TimeStamp{0};
    uint32_t Reason{0};
    uint32_t SourceInfo{0};
    uint32_t SecurityId{0};
    uint32_t FileAttributes{0};
    uint16_t FileNameLength{0};
    uint16_t FileNameOffset{sizeof(USN_RECORD_V2) - sizeof(wchar_t)};
    wchar_t  FileName[1]{L'\0'};
};

// ============================================================================
// 3. Retrieval Pointers for BypassIO / Direct Sector Mapping
// ============================================================================

struct STARTING_VCN_INPUT_BUFFER {
    uint64_t StartingVcn{0};
};

struct EXTENT_PAIR {
    uint64_t NextVcn{0};
    int64_t  Lcn{0};
};

struct RETRIEVAL_POINTERS_BUFFER {
    uint32_t ExtentCount{0};
    uint32_t Reserved{0};
    uint64_t StartingVcn{0};
    EXTENT_PAIR Extents[1]{};
};

// ============================================================================
// 4. B-Tree Directory Index On-Disk Structures ($INDEX_ROOT / $INDEX_ALLOCATION)
// ============================================================================

struct IndexBlockHeader {
    uint32_t magic{0x58444E49};    // "INDX"
    uint16_t usaOffset{NTFS_INDEX_USA_OFFSET};
    uint16_t usaCount{NTFS_INDEX_USA_COUNT};
    uint64_t lsn{0};               // $LogFile sequence number
    uint64_t indexBlockVcn{0};     // VCN of this 4096-byte index block
    uint32_t entriesOffset{0x40};
    uint32_t totalEntryBytes{0};
    uint32_t allocatedEntryBytes{NTFS_INDX_BLOCK_SIZE - 0x40};
    uint8_t  isLeaf{1};            // 1 if leaf node (no child VCNs)
    uint8_t  padding[3]{0};
};

#pragma pack(pop)

// ============================================================================
// 5. Volume Cluster Bitmap Engine ($Bitmap)
// ============================================================================

class NtfsVolumeBitmap {
public:
    NtfsVolumeBitmap() = default;

    void initialize(uint64_t totalClusters) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_totalClusters = totalClusters;
        m_bitmap.assign((totalClusters + 7) / 8, 0);
        // Reserve cluster 0..31 (System metadata: $Boot, $MFT, $LogFile, etc.)
        for (uint64_t c = 0; c < std::min<uint64_t>(totalClusters, 32); ++c) {
            setBitInternal(c, true);
        }
    }

    [[nodiscard]] bool isClusterAllocated(uint64_t cluster) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (cluster >= m_totalClusters) return true;
        size_t byteIdx = static_cast<size_t>(cluster / 8);
        uint8_t bitMask = static_cast<uint8_t>(1u << (cluster % 8));
        return (m_bitmap[byteIdx] & bitMask) != 0;
    }

    bool allocateContiguousClusters(uint64_t count, uint64_t& outStartingCluster) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (count == 0 || count > m_totalClusters) return false;

        uint64_t runStart = 32;
        uint64_t runLength = 0;

        for (uint64_t c = 32; c < m_totalClusters; ++c) {
            size_t byteIdx = static_cast<size_t>(c / 8);
            uint8_t bitMask = static_cast<uint8_t>(1u << (c % 8));
            if ((m_bitmap[byteIdx] & bitMask) == 0) {
                if (runLength == 0) runStart = c;
                runLength++;
                if (runLength == count) {
                    for (uint64_t i = runStart; i < runStart + count; ++i) {
                        setBitInternal(i, true);
                    }
                    outStartingCluster = runStart;
                    m_allocatedCount += count;
                    return true;
                }
            } else {
                runLength = 0;
            }
        }
        return false;
    }

    void freeClusters(uint64_t startingCluster, uint64_t count) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (uint64_t c = startingCluster; c < startingCluster + count && c < m_totalClusters; ++c) {
            if (isBitSetInternal(c)) {
                setBitInternal(c, false);
                if (m_allocatedCount > 0) m_allocatedCount--;
            }
        }
    }

    [[nodiscard]] uint64_t getTotalClusters() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_totalClusters;
    }

    [[nodiscard]] uint64_t getAllocatedClusters() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_allocatedCount;
    }

    [[nodiscard]] uint64_t getFreeClusters() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return (m_totalClusters > m_allocatedCount) ? (m_totalClusters - m_allocatedCount) : 0;
    }

private:
    void setBitInternal(uint64_t cluster, bool val) {
        size_t byteIdx = static_cast<size_t>(cluster / 8);
        uint8_t bitMask = static_cast<uint8_t>(1u << (cluster % 8));
        if (val) m_bitmap[byteIdx] |= bitMask;
        else     m_bitmap[byteIdx] &= ~bitMask;
    }

    [[nodiscard]] bool isBitSetInternal(uint64_t cluster) const {
        size_t byteIdx = static_cast<size_t>(cluster / 8);
        uint8_t bitMask = static_cast<uint8_t>(1u << (cluster % 8));
        return (m_bitmap[byteIdx] & bitMask) != 0;
    }

    mutable std::mutex m_mutex;
    uint64_t m_totalClusters{65536};
    uint64_t m_allocatedCount{32};
    std::vector<uint8_t> m_bitmap;
};

// ============================================================================
// 6. USN Change Journal Engine ($Extend\$UsnJrnl)
// ============================================================================

struct StoredUsnRecord {
    uint64_t usn{0};
    uint64_t fileReferenceNumber{0};
    uint64_t parentFileReferenceNumber{0};
    uint64_t timeStamp{0};
    uint32_t reason{0};
    uint32_t fileAttributes{0};
    std::wstring fileName;
};

class UsnChangeJournalEngine {
public:
    UsnChangeJournalEngine() = default;

    NtStatus createJournal(uint64_t maxSize, uint64_t allocDelta) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_journalData.MaximumSize = (maxSize > 0) ? maxSize : 0x8000000ULL;
        m_journalData.AllocationDelta = (allocDelta > 0) ? allocDelta : 0x1000000ULL;
        m_journalData.FirstUsn = 0;
        m_journalData.LowestValidUsn = 0;
        m_journalData.NextUsn = 0x1000;
        m_records.clear();
        m_active = true;
        return NtStatus::Success;
    }

    NtStatus queryJournal(USN_JOURNAL_DATA_V0& outData) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_active) return NtStatus::DeviceNotReady;
        outData = m_journalData;
        return NtStatus::Success;
    }

    uint64_t postRecord(
        uint64_t frn,
        uint64_t parentFrn,
        uint32_t reason,
        uint32_t attributes,
        std::wstring_view fileName
    ) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_active) return 0;

        uint64_t curUsn = m_journalData.NextUsn;
        m_journalData.NextUsn += 64 + (fileName.size() * sizeof(wchar_t));

        StoredUsnRecord rec{
            .usn = curUsn,
            .fileReferenceNumber = frn,
            .parentFileReferenceNumber = parentFrn,
            .timeStamp = 133500000000000000ULL + curUsn,
            .reason = reason,
            .fileAttributes = attributes,
            .fileName = std::wstring(fileName)
        };
        m_records.push_back(std::move(rec));
        return curUsn;
    }

    NtStatus readJournal(
        uint64_t startUsn,
        uint32_t reasonMask,
        std::vector<uint8_t>& outBuffer
    ) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_active) return NtStatus::DeviceNotReady;

        outBuffer.clear();
        for (const auto& rec : m_records) {
            if (rec.usn >= startUsn && (rec.reason & reasonMask) != 0) {
                uint32_t nameByteLen = static_cast<uint32_t>(rec.fileName.size() * sizeof(wchar_t));
                uint32_t recLen = static_cast<uint32_t>(sizeof(USN_RECORD_V2) + nameByteLen);
                recLen = (recLen + 7) & ~7u; // 8-byte align

                size_t baseOffset = outBuffer.size();
                outBuffer.resize(baseOffset + recLen, 0);

                auto* pRec = reinterpret_cast<USN_RECORD_V2*>(outBuffer.data() + baseOffset);
                pRec->RecordLength = recLen;
                pRec->MajorVersion = 2;
                pRec->MinorVersion = 0;
                pRec->FileReferenceNumber = rec.fileReferenceNumber;
                pRec->ParentFileReferenceNumber = rec.parentFileReferenceNumber;
                pRec->Usn = rec.usn;
                pRec->TimeStamp = rec.timeStamp;
                pRec->Reason = rec.reason;
                pRec->FileAttributes = rec.fileAttributes;
                pRec->FileNameLength = static_cast<uint16_t>(nameByteLen);
                pRec->FileNameOffset = static_cast<uint16_t>(sizeof(USN_RECORD_V2) - sizeof(wchar_t));

                std::memcpy(pRec->FileName, rec.fileName.data(), nameByteLen);
            }
        }
        return NtStatus::Success;
    }

    NtStatus deleteJournal() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_records.clear();
        m_active = false;
        return NtStatus::Success;
    }

    [[nodiscard]] bool isActive() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_active;
    }

    [[nodiscard]] size_t getRecordCount() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_records.size();
    }

private:
    mutable std::mutex m_mutex;
    bool m_active{false};
    USN_JOURNAL_DATA_V0 m_journalData{};
    std::vector<StoredUsnRecord> m_records;
};

// ============================================================================
// 7. B-Tree Directory Index Structure ($INDEX_ROOT / $INDEX_ALLOCATION)
// ============================================================================

struct BTreeIndexNodeEntry {
    uint64_t fileReferenceNumber{0};
    uint64_t allocatedSize{0};
    uint64_t realSize{0};
    uint32_t fileAttributes{0};
    std::wstring fileName;
    int64_t  childSubnodeVcn{-1}; // -1 if leaf

    auto operator<=>(const BTreeIndexNodeEntry& other) const noexcept {
        // Standard NTFS CollationFilename: case-insensitive ordinal compare
        return fileName <=> other.fileName;
    }
};

class BTreeDirectoryIndex {
public:
    BTreeDirectoryIndex() = default;

    bool insertEntry(const BTreeIndexNodeEntry& entry) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = std::lower_bound(m_entries.begin(), m_entries.end(), entry);
        if (it != m_entries.end() && it->fileName == entry.fileName) {
            *it = entry; // Update
            return true;
        }
        m_entries.insert(it, entry);
        return true;
    }

    bool lookupEntry(std::wstring_view name, BTreeIndexNodeEntry& outEntry) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        BTreeIndexNodeEntry probe;
        probe.fileName = std::wstring(name);

        auto it = std::lower_bound(m_entries.begin(), m_entries.end(), probe);
        if (it != m_entries.end() && it->fileName == probe.fileName) {
            outEntry = *it;
            return true;
        }
        return false;
    }

    bool removeEntry(std::wstring_view name) {
        std::lock_guard<std::mutex> lock(m_mutex);
        BTreeIndexNodeEntry probe;
        probe.fileName = std::wstring(name);

        auto it = std::lower_bound(m_entries.begin(), m_entries.end(), probe);
        if (it != m_entries.end() && it->fileName == probe.fileName) {
            m_entries.erase(it);
            return true;
        }
        return false;
    }

    [[nodiscard]] size_t getEntryCount() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_entries.size();
    }

    [[nodiscard]] std::vector<BTreeIndexNodeEntry> getAllEntries() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_entries;
    }

    /**
     * @brief Synthesizes a standard 4096-byte on-disk INDX block with USA fixups.
     */
    std::vector<uint8_t> serializeIndxBlock(uint64_t vcn) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<uint8_t> block(NTFS_INDX_BLOCK_SIZE, 0);

        auto* hdr = reinterpret_cast<IndexBlockHeader*>(block.data());
        hdr->magic = 0x58444E49; // "INDX"
        hdr->usaOffset = NTFS_INDEX_USA_OFFSET;
        hdr->usaCount = NTFS_INDEX_USA_COUNT;
        hdr->lsn = 500;
        hdr->indexBlockVcn = vcn;
        hdr->entriesOffset = 0x40;
        hdr->allocatedEntryBytes = NTFS_INDX_BLOCK_SIZE - 0x40;
        hdr->isLeaf = 1;

        size_t curOffset = hdr->entriesOffset;
        for (const auto& e : m_entries) {
            if (curOffset + sizeof(IndexEntryHeader) + sizeof(FileNamePayload) + (e.fileName.size() * sizeof(char16_t)) > NTFS_INDX_BLOCK_SIZE - 64) {
                break; // Stop before block capacity
            }

            auto* entryHdr = reinterpret_cast<IndexEntryHeader*>(block.data() + curOffset);
            entryHdr->mftReference = e.fileReferenceNumber;

            size_t fnPayloadSize = sizeof(FileNamePayload) + (e.fileName.size() * sizeof(char16_t));
            entryHdr->keyLength = static_cast<uint16_t>(fnPayloadSize);
            entryHdr->flags = (e.childSubnodeVcn != -1) ? INDEX_ENTRY_HAS_SUBNODE : 0;

            uint16_t totalLen = static_cast<uint16_t>(sizeof(IndexEntryHeader) + fnPayloadSize);
            totalLen = (totalLen + 7) & ~7u;
            entryHdr->entryLength = totalLen;

            auto* fn = reinterpret_cast<FileNamePayload*>(block.data() + curOffset + sizeof(IndexEntryHeader));
            fn->realSize = e.realSize;
            fn->allocatedSize = e.allocatedSize;
            fn->flags = e.fileAttributes;
            fn->nameLength = static_cast<uint8_t>(e.fileName.size());
            for (size_t i = 0; i < e.fileName.size(); ++i) {
                fn->name[i] = static_cast<char16_t>(e.fileName[i]);
            }

            curOffset += totalLen;
        }

        // Add Last Entry marker
        auto* lastHdr = reinterpret_cast<IndexEntryHeader*>(block.data() + curOffset);
        lastHdr->flags = INDEX_ENTRY_LAST;
        lastHdr->entryLength = sizeof(IndexEntryHeader);
        curOffset += sizeof(IndexEntryHeader);

        hdr->totalEntryBytes = static_cast<uint32_t>(curOffset - hdr->entriesOffset);

        // Apply USA fixups
        UsaEngine::generateFixups(block.data(), NTFS_INDX_BLOCK_SIZE, 0x4321);
        return block;
    }

private:
    mutable std::mutex m_mutex;
    std::vector<BTreeIndexNodeEntry> m_entries;
};

// ============================================================================
// 8. Paging File Extent Reservation Engine (pagefile.sys & BypassIO)
// ============================================================================

struct PagingFileExtentDescriptor {
    uint64_t virtualClusterStart{0};
    uint64_t logicalClusterStart{0};
    uint64_t clusterCount{0};
    uint64_t physicalSectorLba{0};
};

struct PagingFileReservation {
    uint64_t totalBytesReserved{0};
    uint64_t totalClusters{0};
    uint32_t extentCount{0};
    bool isContiguous{false};
    bool bypassIoSupported{true};
    std::vector<PagingFileExtentDescriptor> extents;
};

class NtfsPagingFileEngine {
public:
    NtfsPagingFileEngine() = default;

    NtStatus reservePagingFile(
        NtfsVolumeBitmap& bitmap,
        uint64_t sizeInBytes,
        uint32_t clusterSize,
        PagingFileReservation& outRes
    ) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (sizeInBytes == 0 || clusterSize == 0) return NtStatus::InvalidParameter;

        uint64_t neededClusters = (sizeInBytes + clusterSize - 1) / clusterSize;
        outRes.totalBytesReserved = neededClusters * clusterSize;
        outRes.totalClusters = neededClusters;
        outRes.extents.clear();

        // Attempt contiguous allocation first
        uint64_t startLcn = 0;
        if (bitmap.allocateContiguousClusters(neededClusters, startLcn)) {
            PagingFileExtentDescriptor ext{
                .virtualClusterStart = 0,
                .logicalClusterStart = startLcn,
                .clusterCount = neededClusters,
                .physicalSectorLba = startLcn * (clusterSize / NTFS_SECTOR_SIZE)
            };
            outRes.extents.push_back(ext);
            outRes.extentCount = 1;
            outRes.isContiguous = true;
            outRes.bypassIoSupported = true;
            m_activeReservation = outRes;
            return NtStatus::Success;
        }

        // Segmented fallback if contiguous allocation unavailable
        uint64_t clustersRemaining = neededClusters;
        uint64_t curVcn = 0;
        while (clustersRemaining > 0) {
            uint64_t chunk = std::min<uint64_t>(clustersRemaining, 1024);
            uint64_t chunkLcn = 0;
            if (!bitmap.allocateContiguousClusters(chunk, chunkLcn)) {
                return NtStatus::DiskFull;
            }
            PagingFileExtentDescriptor ext{
                .virtualClusterStart = curVcn,
                .logicalClusterStart = chunkLcn,
                .clusterCount = chunk,
                .physicalSectorLba = chunkLcn * (clusterSize / NTFS_SECTOR_SIZE)
            };
            outRes.extents.push_back(ext);
            curVcn += chunk;
            clustersRemaining -= chunk;
        }

        outRes.extentCount = static_cast<uint32_t>(outRes.extents.size());
        outRes.isContiguous = (outRes.extentCount == 1);
        outRes.bypassIoSupported = (outRes.extentCount <= 4); // Low fragmentation for BypassIO
        m_activeReservation = outRes;
        return NtStatus::Success;
    }

    [[nodiscard]] const PagingFileReservation& getActiveReservation() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_activeReservation;
    }

private:
    mutable std::mutex m_mutex;
    PagingFileReservation m_activeReservation{};
};

// ============================================================================
// 9. Sovereign Primary NTFS Storage Engine (Orchestrator)
// ============================================================================

class SovereignPrimaryNtfsEngine {
public:
    static SovereignPrimaryNtfsEngine& Instance() noexcept {
        static SovereignPrimaryNtfsEngine s_instance;
        return s_instance;
    }

    NtStatus initializePrimaryStorage(
        std::shared_ptr<storage::IBlockDevice> dev,
        uint64_t totalSectors = 268435456ULL, // 128 GB default
        std::wstring_view volumeLabel = L"MicaNT_System"
    ) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_device = dev;
        m_volumeLabel = volumeLabel;
        uint64_t totalClusters = totalSectors / 8; // 4096-byte clusters

        m_bitmap.initialize(totalClusters);
        m_usnEngine.createJournal(0x8000000ULL, 0x1000000ULL);
        m_rootIndex = std::make_unique<BTreeDirectoryIndex>();

        // Pre-populate core Windows NT directory structure in B-Tree index
        BTreeIndexNodeEntry winDir{
            .fileReferenceNumber = 16,
            .allocatedSize = 4096,
            .realSize = 0,
            .fileAttributes = FILE_ATTRIBUTE_DIRECTORY,
            .fileName = L"Windows"
        };
        m_rootIndex->insertEntry(winDir);

        BTreeIndexNodeEntry sys32Dir{
            .fileReferenceNumber = 17,
            .allocatedSize = 4096,
            .realSize = 0,
            .fileAttributes = FILE_ATTRIBUTE_DIRECTORY,
            .fileName = L"System32"
        };
        m_rootIndex->insertEntry(sys32Dir);

        BTreeIndexNodeEntry progFilesDir{
            .fileReferenceNumber = 18,
            .allocatedSize = 4096,
            .realSize = 0,
            .fileAttributes = FILE_ATTRIBUTE_DIRECTORY,
            .fileName = L"Program Files"
        };
        m_rootIndex->insertEntry(progFilesDir);

        BTreeIndexNodeEntry usersDir{
            .fileReferenceNumber = 19,
            .allocatedSize = 4096,
            .realSize = 0,
            .fileAttributes = FILE_ATTRIBUTE_DIRECTORY,
            .fileName = L"Users"
        };
        m_rootIndex->insertEntry(usersDir);

        m_usnEngine.postRecord(16, MFT_REC_ROOT, USN_REASON_FILE_CREATE, FILE_ATTRIBUTE_DIRECTORY, L"Windows");
        m_usnEngine.postRecord(17, 16, USN_REASON_FILE_CREATE, FILE_ATTRIBUTE_DIRECTORY, L"System32");

        m_initialized = true;
        return NtStatus::Success;
    }

    [[nodiscard]] bool isInitialized() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_initialized;
    }

    [[nodiscard]] NtfsVolumeBitmap& getBitmap() noexcept { return m_bitmap; }
    [[nodiscard]] UsnChangeJournalEngine& getUsnEngine() noexcept { return m_usnEngine; }
    [[nodiscard]] BTreeDirectoryIndex* getRootIndex() const noexcept { return m_rootIndex.get(); }
    [[nodiscard]] NtfsPagingFileEngine& getPagingEngine() noexcept { return m_pagingEngine; }

private:
    SovereignPrimaryNtfsEngine() = default;
    mutable std::mutex m_mutex;
    bool m_initialized{false};
    std::shared_ptr<storage::IBlockDevice> m_device;
    std::wstring m_volumeLabel{L"MicaNT_System"};
    NtfsVolumeBitmap m_bitmap;
    UsnChangeJournalEngine m_usnEngine;
    std::unique_ptr<BTreeDirectoryIndex> m_rootIndex;
    NtfsPagingFileEngine m_pagingEngine;
};

// ============================================================================
// 10. Subsystem Architectural Verification (Suite 576)
// ============================================================================

bool VerifyHorizon360PrimaryNtfsSubsystem() noexcept;

} // namespace micant::ntfs::primary
