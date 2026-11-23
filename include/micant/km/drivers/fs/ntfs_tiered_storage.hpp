// ============================================================================
// MicaNT: Sovereign Clean-Room Primary NTFS Tiered Storage Spaces Subsystem
// Horizon 371 / Suite 587 - \Device\StorageSpaces, Heat-Map Access Tracking,
// Hot/Cold Tier Extent Migration, Real-Time Slab Allocation & Write-Back Cache
//
// Clean-Room Implementation in modern freestanding ISO C++23.
// Compatible with Dave Cutler's Windows NT architecture, win32metadata,
// and spaceport.sys / s2d.sys tiered storage engine. Zero external dependencies.
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
#include "micant/km/drivers/storage/storage.hpp"
#include "micant/km/drivers/fs/ntfs_primary_storage.hpp"

namespace micant::ntfs::tiered {

// ============================================================================
// 1. Constants & Enums for Tiered Storage Spaces & Heat-Map Tracking
// ============================================================================

inline constexpr uint32_t TIERED_STORAGE_SECTOR_SIZE     = 512;
inline constexpr uint32_t TIERED_STORAGE_DEFAULT_SLAB_SZ = 64 * 1024; // 64 KB per slab
inline constexpr uint32_t TIERED_STORAGE_SECTORS_PER_SLAB = TIERED_STORAGE_DEFAULT_SLAB_SZ / TIERED_STORAGE_SECTOR_SIZE; // 128 sectors
inline constexpr uint32_t TIERED_STORAGE_MAX_DISKS       = 32;
inline constexpr uint32_t TIERED_STORAGE_MAX_SLABS       = 1024;
inline constexpr uint32_t TIERED_WBC_CAPACITY_SLABS      = 8; // Write-Back Cache capacity

// Storage Tier Media Types
enum class StorageTierType : uint32_t {
    PerformanceTier = 0, // Fast NVMe / Optane / SLC SSD (Ultra-low latency, high IOPS)
    CapacityTier    = 1  // Slow HDD / QLC SSD (Large capacity, cost-effective)
};

// Heat-Map Temperature Categories
enum class SlabTemperature : uint32_t {
    Freezing = 0, // Heat < 10 (Coldest, candidate for demotion)
    Cold     = 1, // 10 <= Heat < 50
    Warm     = 2, // 50 <= Heat < 200
    Hot      = 3  // Heat >= 200 (Hottest, immediate promotion candidate)
};

// Migration State for Slabs
enum class SlabMigrationState : uint32_t {
    Idle = 0,
    PendingPromotion,
    Promoting,
    PendingDemotion,
    Demoting
};

// Tier Pinning Policy (allows pinning critical system metadata like $MFT, ntoskrnl, pagefile)
enum class PinningPolicy : uint32_t {
    DefaultDynamic = 0, // Migration engine moves based on temperature
    PinnedFast     = 1, // Permanently pinned to Performance Tier (never demote)
    PinnedSlow     = 2  // Permanently pinned to Capacity Tier (never promote)
};

// ============================================================================
// 2. Data Structures: Physical Disks, Slabs, and Heat-Map Metrics
// ============================================================================

struct PhysicalDiskSpec {
    uint32_t        diskId{0};
    std::wstring    diskName;
    StorageTierType tier{StorageTierType::CapacityTier};
    uint32_t        totalSlabs{0};
    uint32_t        allocatedSlabs{0};
    uint64_t        readIops{0};
    uint64_t        writeIops{0};
    uint32_t        avgLatencyUs{1000}; // e.g. 15us for NVMe, 5000us for HDD
    bool            isOnline{true};
};

struct SlabMappingEntry {
    uint32_t           virtualSlabIndex{0};
    StorageTierType    currentTier{StorageTierType::CapacityTier};
    uint32_t           physicalDiskId{0};
    uint32_t           physicalSlabIndex{0};
    uint64_t           readCount{0};
    uint64_t           writeCount{0};
    uint64_t           lastAccessTickUs{0};
    double             heatScore{0.0};
    SlabTemperature    temperature{SlabTemperature::Freezing};
    PinningPolicy      pinning{PinningPolicy::DefaultDynamic};
    SlabMigrationState migrationState{SlabMigrationState::Idle};
    bool               isAllocated{false};
};

struct HeatMapEvent {
    uint32_t virtualSlabIndex{0};
    bool     isWrite{false};
    uint32_t byteCount{0};
    uint64_t timestampUs{0};
};

struct TierMigrationEvent {
    uint32_t        virtualSlabIndex{0};
    StorageTierType sourceTier{StorageTierType::CapacityTier};
    StorageTierType destTier{StorageTierType::PerformanceTier};
    uint32_t        sourceDiskId{0};
    uint32_t        sourcePhysicalSlab{0};
    uint32_t        destDiskId{0};
    uint32_t        destPhysicalSlab{0};
    bool            isPromotion{true};
    uint64_t        durationUs{0};
};

struct WriteBackCacheEntry {
    uint32_t            virtualSlabIndex{0};
    uint32_t            offsetInSlab{0};
    std::vector<uint8_t> payload;
    bool                isDirty{false};
    uint64_t            cachedTimestampUs{0};
};

struct TieredStorageStats {
    uint32_t totalVirtualSlabs{0};
    uint32_t allocatedVirtualSlabs{0};
    uint32_t fastTierTotalSlabs{0};
    uint32_t fastTierAllocatedSlabs{0};
    uint32_t slowTierTotalSlabs{0};
    uint32_t slowTierAllocatedSlabs{0};
    uint64_t totalPromotions{0};
    uint64_t totalDemotions{0};
    uint64_t totalWbcHits{0};
    uint64_t totalWbcFlushes{0};
};

// ============================================================================
// 3. HeatMapTracker Class
// ============================================================================

class HeatMapTracker {
public:
    HeatMapTracker() = default;

    void recordAccess(uint32_t virtualSlabIndex, bool isWrite, uint32_t byteCount);
    void updateTemperature(SlabMappingEntry& entry);
    void applyEpochDecay(double decayFactor = 0.90);

    [[nodiscard]] size_t getEventCount() const noexcept {
        std::lock_guard lock(mutex_);
        return events_.size();
    }

private:
    mutable std::mutex       mutex_;
    std::vector<HeatMapEvent> events_;
};

// ============================================================================
// 4. WriteBackCache Subsystem
// ============================================================================

class WriteBackCache {
public:
    explicit WriteBackCache(uint32_t maxEntries = TIERED_WBC_CAPACITY_SLABS);

    bool stageWrite(uint32_t virtualSlabIndex, uint32_t offsetInSlab, std::span<const uint8_t> data);
    bool tryRead(uint32_t virtualSlabIndex, uint32_t offsetInSlab, std::span<uint8_t> outBuffer);
    std::vector<WriteBackCacheEntry> drainDirtyEntries();
    [[nodiscard]] size_t getDirtyCount() const noexcept;
    void clear();

private:
    mutable std::mutex                     mutex_;
    uint32_t                               maxCapacity_{TIERED_WBC_CAPACITY_SLABS};
    std::vector<WriteBackCacheEntry>       entries_;
};

// ============================================================================
// 5. SovereignNtfsTieredStorageEngine Main Class
// ============================================================================

class SovereignNtfsTieredStorageEngine {
public:
    SovereignNtfsTieredStorageEngine();
    ~SovereignNtfsTieredStorageEngine() = default;

    // Pool Configuration
    NtStatus registerPhysicalDisk(uint32_t diskId, const std::wstring& name,
                                  StorageTierType tier, uint32_t totalSlabs,
                                  uint32_t avgLatencyUs = 100);
    NtStatus initializePool(uint32_t virtualSlabCount = 64);

    // Slab Allocations
    NtStatus allocateVirtualSlab(uint32_t virtualSlabIndex, StorageTierType preferredTier,
                                 PinningPolicy pinning = PinningPolicy::DefaultDynamic);
    NtStatus freeVirtualSlab(uint32_t virtualSlabIndex);

    // Block I/O Path with Heat Tracking & Write-Back Cache
    NtStatus writeSlabData(uint32_t virtualSlabIndex, uint32_t offsetInSlab,
                           std::span<const uint8_t> buffer, bool useWbc = true);
    NtStatus readSlabData(uint32_t virtualSlabIndex, uint32_t offsetInSlab,
                          std::span<uint8_t> buffer);
    NtStatus flushWriteBackCache();

    // Heat-Map Management & Autonomous Migration
    void setSlabPinning(uint32_t virtualSlabIndex, PinningPolicy policy);
    NtStatus executeMigrationCycle(double fastTierDemotionThreshold = 0.80);

    // Query Methods
    [[nodiscard]] TieredStorageStats getStats() const;
    [[nodiscard]] const SlabMappingEntry* getSlabInfo(uint32_t virtualSlabIndex) const;
    [[nodiscard]] const std::vector<TierMigrationEvent>& getMigrationHistory() const noexcept {
        return migrationHistory_;
    }

    // Reset for Clean Testing
    void reset();

private:
    NtStatus findFreePhysicalSlab(StorageTierType tier, uint32_t& outDiskId, uint32_t& outPhysicalSlab);
    void freePhysicalSlab(uint32_t diskId, uint32_t physicalSlab);

    mutable std::shared_mutex               engineLock_;
    std::vector<PhysicalDiskSpec>           disks_;
    std::vector<SlabMappingEntry>           slabs_;
    HeatMapTracker                          heatMapTracker_;
    WriteBackCache                          writeBackCache_;

    // Simulated Physical Storage Backing: DiskId -> (PhysicalSlab -> DataBuffer)
    std::unordered_map<uint64_t, std::vector<uint8_t>> physicalSlabStorage_;

    // Telemetry & Statistics
    TieredStorageStats                      stats_;
    std::vector<TierMigrationEvent>         migrationHistory_;
    bool                                    isInitialized_{false};
};

// ============================================================================
// 6. Horizon 371 / Suite 587 Architectural Verification Sentinel
// ============================================================================

bool VerifyHorizon371NtfsTieredStorageSubsystem();

} // namespace micant::ntfs::tiered
