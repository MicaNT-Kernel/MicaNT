#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <shared_mutex>
#include <memory>
#include <span>
#include <chrono>
#include <algorithm>

#include "micant/shared/ntstatus.hpp"
#include "ntfs_primary_storage.hpp"

namespace micant::ntfs::selfheal {

// ============================================================================
// Sovereign NTFS Online Self-Healing, Hot-Fixing & Corruption Scrubbing Subsystem
// Horizon 376 / Suite 592
// ============================================================================

// FSCTL Definitions for Online Self-Healing
inline constexpr uint32_t FSCTL_SELF_HEALING_CONTROL        = 0x000900DCu;
inline constexpr uint32_t FSCTL_GET_NTFS_VOLUME_DATA_CODE   = 0x00090064u;
inline constexpr uint32_t FSCTL_MARK_VOLUME_DIRTY_CODE     = 0x0009007Cu;
inline constexpr uint32_t FSCTL_IS_VOLUME_DIRTY_CODE       = 0x00090078u;

// Self-Healing Control Flags
inline constexpr uint32_t NTFS_SELF_HEALING_ENABLE             = 0x00000001u;
inline constexpr uint32_t NTFS_SELF_HEALING_REPAIR_ON_ACCESS   = 0x00000002u;
inline constexpr uint32_t NTFS_SELF_HEALING_VERIFY_READS       = 0x00000004u;
inline constexpr uint32_t NTFS_SELF_HEALING_BACKGROUND_SCRUB   = 0x00000008u;
inline constexpr uint32_t NTFS_SELF_HEALING_DYNAMIC_SPARING    = 0x00000010u;

// Magic Signatures
inline constexpr uint32_t NTFS_FILE_MAGIC = 0x454C4946u; // 'FILE'
inline constexpr uint32_t NTFS_BAAD_MAGIC = 0x44414142u; // 'BAAD'
inline constexpr uint32_t NTFS_INDX_MAGIC = 0x58444E49u; // 'INDX'

// Spot-Fix Anomaly Categories
enum class SpotAnomalyCategory : uint32_t {
    None                      = 0,
    TornWriteFixupMismatch    = 1,
    CorruptedRecordMagic      = 2,
    AttributeHeaderCorrupt    = 3,
    IndexCycleDetected        = 4,
    OrphanFileReference       = 5,
    BitmapLeakedCluster       = 6,
    BitmapUntrackedCluster    = 7,
    BadSectorUncorrectable    = 8
};

// Spot-Fix Audit Action Record
struct SpotFixRecord {
    uint64_t            FileRecordNumber{0};
    SpotAnomalyCategory Anomaly{SpotAnomalyCategory::None};
    bool                Repaired{false};
    uint64_t            OldLcn{0};
    uint64_t            NewLcn{0};
    uint64_t            Timestamp{0};
    std::wstring        Description;
};

// FSCTL_SELF_HEALING_CONTROL Input/Output Data
struct NTFS_SELF_HEALING_CONTROL_DATA {
    uint32_t Flags{NTFS_SELF_HEALING_ENABLE |
                   NTFS_SELF_HEALING_REPAIR_ON_ACCESS |
                   NTFS_SELF_HEALING_DYNAMIC_SPARING};
    uint32_t MaxRepairsPerMinute{120};
    uint32_t MaxScrubRateClustersPerSec{10000};
    uint64_t Reserved[2]{0, 0};
};

// Self-Healing & Scrubbing Subsystem Telemetry
struct SelfHealTelemetry {
    uint64_t TotalAnomaliesDetected{0};
    uint64_t TotalSpotFixesRepaired{0};
    uint64_t TotalBadSectorsSpared{0};
    uint64_t TotalClustersScrubbed{0};
    uint64_t TotalLeakedClustersReclaimed{0};
    uint64_t TotalOrphansReconnected{0};
    bool     VolumeDirtyMarked{false};
};

// File extent mapping for hot-fixing and dynamic cluster sparing
struct FileExtentMapping {
    uint64_t Vcn{0};
    uint64_t Lcn{0};
    uint64_t Length{0};
};

// ============================================================================
// SovereignNtfsSelfHealEngine Class
// ============================================================================

class SovereignNtfsSelfHealEngine {
public:
    SovereignNtfsSelfHealEngine();
    ~SovereignNtfsSelfHealEngine();

    // Volume Initialization
    NtStatus initializeVolume(uint64_t totalClusters, uint32_t clusterSize = 4096);

    // Self-Healing Control Interface (FSCTL_SELF_HEALING_CONTROL)
    NtStatus setControl(const NTFS_SELF_HEALING_CONTROL_DATA& controlData);
    NtStatus getControl(NTFS_SELF_HEALING_CONTROL_DATA& outControlData) const;

    // Spot-Fixing: MFT Record Repair (USA fixup synchronization, magic restoration)
    NtStatus spotFixMftRecord(std::span<uint8_t> mftRecordBuffer, uint64_t frn, SpotFixRecord& outRecord);

    // Bad Sector Hot-Fixing & Dynamic Sparing ($BadClust stream updates)
    NtStatus hotFixBadCluster(
        uint64_t frn,
        uint64_t badLcn,
        const std::vector<uint8_t>& reconstructedPayload,
        uint64_t& outSparedLcn,
        SpotFixRecord& outRecord
    );

    // B-Tree Index Cycle Resolution & Orphan Reconnection
    NtStatus repairIndexCycle(
        uint64_t dirFrn,
        std::vector<uint64_t>& entryFrns,
        SpotFixRecord& outRecord
    );

    NtStatus reconnectOrphan(
        uint64_t orphanFrn,
        uint64_t targetParentFrn,
        SpotFixRecord& outRecord
    );

    // Volume Scrubbing & Bitmap Allocation Reconciliation
    NtStatus scrubVolumePass(
        const std::unordered_map<uint64_t, std::vector<FileExtentMapping>>& activeFileExtents,
        size_t& outReclaimedCount,
        size_t& outUntrackedAllocatedCount
    );

    // Dirty Bit Management (FSCTL_MARK_VOLUME_DIRTY / FSCTL_IS_VOLUME_DIRTY)
    void markVolumeDirty();
    void clearVolumeDirty();
    bool isVolumeDirty() const;

    // Bad Cluster Query ($BadClust)
    bool isClusterMarkedBad(uint64_t lcn) const;
    const std::unordered_set<uint64_t>& getBadClusters() const;

    // Telemetry & Audit History
    SelfHealTelemetry getTelemetry() const;
    std::vector<SpotFixRecord> getAuditLog() const;

    // Helper: Register simulated active file extent
    void registerFileExtent(uint64_t frn, uint64_t vcn, uint64_t lcn, uint64_t length);
    std::vector<FileExtentMapping> getFileExtents(uint64_t frn) const;

private:
    mutable std::shared_mutex                                         m_mutex;
    bool                                                              m_initialized{false};
    uint64_t                                                          m_totalClusters{0};
    uint32_t                                                          m_clusterSize{4096};
    NTFS_SELF_HEALING_CONTROL_DATA                                    m_control{};
    SelfHealTelemetry                                                 m_telemetry{};
    std::vector<bool>                                                 m_bitmap;
    std::unordered_set<uint64_t>                                      m_badClusters;
    std::unordered_map<uint64_t, std::vector<FileExtentMapping>>      m_fileExtents;
    std::vector<SpotFixRecord>                                        m_auditLog;

    uint64_t allocateSpareClusterLocked();
};

bool VerifyHorizon376NtfsSelfHealSubsystem();

} // namespace micant::ntfs::selfheal
