// ============================================================================
// MicaNT: Sovereign Clean-Room Primary NTFS Dynamic Volume Resizing Subsystem
// Horizon 373 / Suite 589 - FSCTL_EXTEND_VOLUME, FSCTL_SHRINK_VOLUME,
// Online Cluster Relocation, MFT Data Run Updates & Geometry Sync
//
// Clean-Room Implementation in modern freestanding ISO C++23.
// Compatible with Dave Cutler's Windows NT architecture, win32metadata,
// and NTFS volume resize specifications. Zero external dependencies.
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

namespace micant::ntfs::resize {

// ============================================================================
// 1. Constants & Enums for Dynamic Volume Resizing
// ============================================================================

inline constexpr uint32_t FSCTL_EXTEND_VOLUME_CODE            = 0x000900F0;
inline constexpr uint32_t FSCTL_SHRINK_VOLUME_CODE            = 0x00090280;
inline constexpr uint32_t FSCTL_GET_VOLUME_SHRINK_INFO_CODE   = 0x00090284;
inline constexpr uint32_t NTFS_DEFAULT_CLUSTER_SIZE_BYTES     = 4096;

enum class ImmovableFileType : uint32_t {
    None = 0,
    BootSector,
    MftMirror,
    PageFileExtents,
    SystemReserved
};

enum class ResizeOperationType : uint32_t {
    None = 0,
    Extend,
    Shrink
};

// ============================================================================
// 2. Data Structures: Shrink Info, Relocation Records & Geometry
// ============================================================================

struct VolumeShrinkInfo {
    uint64_t          currentClusters{0};
    uint64_t          targetClusters{0};
    uint64_t          minimumPossibleClusters{0};
    uint64_t          clustersToRelocate{0};
    uint64_t          lowestImmovableLcn{0};
    ImmovableFileType immovableFileType{ImmovableFileType::None};
    bool              canShrink{true};
};

struct ClusterRelocationRecord {
    uint64_t oldLcn{0};
    uint64_t newLcn{0};
    uint64_t clusterCount{0};
    uint64_t fileRecordNumber{0};
    uint32_t attributeType{0};
    uint64_t timestampUs{0};
};

struct FileDataRunMapping {
    uint64_t fileRecordNumber{0};
    uint64_t startVcn{0};
    uint64_t startLcn{0};
    uint64_t clusterCount{0};
};

struct VolumeResizeTelemetry {
    uint64_t totalExtendOps{0};
    uint64_t totalShrinkOps{0};
    uint64_t totalClustersRelocated{0};
    uint64_t totalBytesRelocated{0};
    uint64_t abortedShrinksDueToImmovable{0};
    uint64_t averageRelocationTimeUs{0};
};

// ============================================================================
// 3. SovereignNtfsVolumeResizer Main Class
// ============================================================================

class SovereignNtfsVolumeResizer {
public:
    SovereignNtfsVolumeResizer();
    ~SovereignNtfsVolumeResizer() = default;

    NtStatus initializeVolume(uint64_t totalClusters, uint32_t clusterSizeBytes = NTFS_DEFAULT_CLUSTER_SIZE_BYTES);

    // Allocation & File Tracking
    NtStatus allocateClusters(uint64_t fileRecordNumber, uint64_t count, uint64_t& outStartLcn);
    NtStatus allocateClustersAt(uint64_t fileRecordNumber, uint64_t startLcn, uint64_t count);
    NtStatus freeClusters(uint64_t startLcn, uint64_t count);
    void pinImmovableCluster(uint64_t lcn, ImmovableFileType type);
    void unpinImmovableCluster(uint64_t lcn);

    // Dynamic Resizing Path
    NtStatus queryShrinkInfo(uint64_t targetClusters, VolumeShrinkInfo& outInfo) const;
    NtStatus extendVolume(uint64_t newTotalClusters);
    NtStatus shrinkVolume(uint64_t targetTotalClusters);

    // Block I/O Access
    NtStatus writeClusterPayload(uint64_t lcn, std::span<const uint8_t> buffer);
    NtStatus readClusterPayload(uint64_t lcn, std::span<uint8_t> buffer) const;

    // Queries & Diagnostics
    [[nodiscard]] uint64_t getTotalClusters() const noexcept;
    [[nodiscard]] uint64_t getAllocatedClustersCount() const noexcept;
    [[nodiscard]] const std::vector<ClusterRelocationRecord>& getRelocationHistory() const noexcept {
        return relocationHistory_;
    }
    [[nodiscard]] std::vector<FileDataRunMapping> getFileDataRuns(uint64_t fileRecordNumber) const;
    [[nodiscard]] VolumeResizeTelemetry getTelemetry() const;

    void reset();

private:
    NtStatus findFreeClustersBelow(uint64_t maxLcn, uint64_t count, uint64_t& outLcn);

    mutable std::shared_mutex                         mutex_;
    uint64_t                                         totalClusters_{0};
    uint32_t                                         clusterSizeBytes_{NTFS_DEFAULT_CLUSTER_SIZE_BYTES};
    std::vector<bool>                                bitmap_; // true = occupied, false = free
    std::unordered_map<uint64_t, ImmovableFileType>  immovableClusters_;
    std::unordered_map<uint64_t, uint64_t>           clusterToFileMap_; // LCN -> FileRecordNumber
    std::unordered_map<uint64_t, std::vector<FileDataRunMapping>> fileRuns_;
    std::unordered_map<uint64_t, std::vector<uint8_t>>            clusterStorage_; // LCN -> Payload
    std::vector<ClusterRelocationRecord>             relocationHistory_;
    VolumeResizeTelemetry                            telemetry_;
    bool                                             isInitialized_{false};
};

// ============================================================================
// 4. Horizon 373 / Suite 589 Architectural Verification Sentinel
// ============================================================================

bool VerifyHorizon373NtfsVolumeResizeSubsystem();

} // namespace micant::ntfs::resize
