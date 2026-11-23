// ============================================================================
// MicaNT: Sovereign Clean-Room Primary NTFS Defragmentation & MFT Packer
// Horizon 374 / Suite 590 - FSCTL_GET_RETRIEVAL_POINTERS, FSCTL_MOVE_FILE,
// Free Space Consolidation, MFT Extent Packing & BypassIO Alignment
//
// Clean-Room Implementation in modern freestanding ISO C++23.
// Compatible with Dave Cutler's Windows NT architecture, win32metadata,
// and defrag.sys / FSCTL_MOVE_FILE specifications. Zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <shared_mutex>
#include <mutex>
#include <span>
#include <cstring>
#include <algorithm>
#include <unordered_map>
#include <chrono>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/km/drivers/fs/ntfs_primary_storage.hpp"

namespace micant::ntfs::defrag {

// ============================================================================
// 1. Constants & Enums for Defragmentation & Retrieval Pointers
// ============================================================================

inline constexpr uint32_t FSCTL_GET_RETRIEVAL_POINTERS_CODE   = 0x00090073;
inline constexpr uint32_t FSCTL_MOVE_FILE_CODE               = 0x00090074;
inline constexpr uint32_t NTFS_DEFRAG_CLUSTER_SIZE_BYTES     = 4096;

// ============================================================================
// 2. Data Structures: Retrieval Pointers, Move File & Extent Geometry
// ============================================================================

struct FileExtentMapping {
    uint64_t nextVcn{0};
    uint64_t lcn{0};
    uint64_t clusterCount{0};
};

struct RetrievalPointersBuffer {
    uint32_t                       extentCount{0};
    uint64_t                       startingVcn{0};
    std::vector<FileExtentMapping> extents;
};

struct MoveFileData {
    uint64_t fileRecordNumber{0};
    uint64_t startingVcn{0};
    uint64_t targetLcn{0};
    uint32_t clusterCount{0};
};

struct DefragTelemetry {
    uint64_t totalFilesAnalyzed{0};
    uint64_t totalFilesDefragmented{0};
    uint64_t totalExtentsConsolidated{0};
    uint64_t totalClustersMoved{0};
    uint64_t totalBytesMoved{0};
    uint64_t largestContiguousFreeBefore{0};
    uint64_t largestContiguousFreeAfter{0};
    double   averageFragmentationBefore{0.0};
    double   averageFragmentationAfter{0.0};
};

// ============================================================================
// 3. SovereignNtfsDefragEngine Main Class
// ============================================================================

class SovereignNtfsDefragEngine {
public:
    SovereignNtfsDefragEngine();
    ~SovereignNtfsDefragEngine() = default;

    NtStatus initializeVolume(uint64_t totalClusters, uint32_t clusterSizeBytes = NTFS_DEFRAG_CLUSTER_SIZE_BYTES);

    // Allocation & File Setup
    NtStatus allocateFragmentedFile(uint64_t fileRecordNumber, const std::vector<std::pair<uint64_t, uint64_t>>& lcnSpans);
    NtStatus allocateContiguousFile(uint64_t fileRecordNumber, uint64_t count, uint64_t& outStartLcn);
    NtStatus writeClusterPayload(uint64_t lcn, std::span<const uint8_t> buffer);
    NtStatus readClusterPayload(uint64_t lcn, std::span<uint8_t> buffer) const;

    // FSCTL APIs
    NtStatus getRetrievalPointers(uint64_t fileRecordNumber, RetrievalPointersBuffer& outBuffer) const;
    NtStatus moveFileClusters(const MoveFileData& moveData);

    // Defragmentation Algorithms
    NtStatus defragmentFile(uint64_t fileRecordNumber);
    NtStatus consolidateFreeSpace();
    NtStatus packMftExtents(const std::vector<uint64_t>& mftFragmentedLcons, uint64_t targetMftStartLcn);

    // Queries
    [[nodiscard]] uint64_t getLargestContiguousFreeBlock() const;
    [[nodiscard]] double getFileFragmentationRatio(uint64_t fileRecordNumber) const;
    [[nodiscard]] DefragTelemetry getTelemetry() const;
    [[nodiscard]] uint64_t getTotalClusters() const noexcept;

    void reset();

private:
    NtStatus findContiguousFree(uint64_t count, uint64_t& outStartLcn) const;
    void coalesceFileExtents(uint64_t fileRecordNumber);

    mutable std::shared_mutex                           mutex_;
    uint64_t                                           totalClusters_{0};
    uint32_t                                           clusterSizeBytes_{NTFS_DEFRAG_CLUSTER_SIZE_BYTES};
    std::vector<bool>                                  bitmap_; // true = occupied, false = free
    std::unordered_map<uint64_t, uint64_t>             clusterToFileMap_; // LCN -> FileRecordNumber
    std::unordered_map<uint64_t, std::vector<FileExtentMapping>> fileExtents_;
    std::unordered_map<uint64_t, std::vector<uint8_t>> clusterStorage_; // LCN -> Payload
    DefragTelemetry                                     telemetry_;
    bool                                               isInitialized_{false};
};

// ============================================================================
// 4. Horizon 374 / Suite 590 Architectural Verification Sentinel
// ============================================================================

bool VerifyHorizon374NtfsDefragSubsystem();

} // namespace micant::ntfs::defrag
