// ============================================================================
// MicaNT: Sovereign Clean-Room Primary NTFS Tiered Storage Spaces Subsystem
// Horizon 371 / Suite 587 - \Device\StorageSpaces Implementation
// ============================================================================

#include "micant/km/drivers/fs/ntfs_tiered_storage.hpp"
#include <iostream>
#include <chrono>

namespace micant::ntfs::tiered {

// ============================================================================
// 1. HeatMapTracker Implementation
// ============================================================================

void HeatMapTracker::recordAccess(uint32_t virtualSlabIndex, bool isWrite, uint32_t byteCount) {
    std::lock_guard lock(mutex_);
    auto nowUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    events_.push_back(HeatMapEvent{
        .virtualSlabIndex = virtualSlabIndex,
        .isWrite = isWrite,
        .byteCount = byteCount,
        .timestampUs = nowUs
    });
}

void HeatMapTracker::updateTemperature(SlabMappingEntry& entry) {
    // Weighted scoring: writes count 1.5x more than reads due to write amplification
    entry.heatScore = (static_cast<double>(entry.readCount) * 1.0) +
                      (static_cast<double>(entry.writeCount) * 1.5);

    if (entry.heatScore < 10.0) {
        entry.temperature = SlabTemperature::Freezing;
    } else if (entry.heatScore < 50.0) {
        entry.temperature = SlabTemperature::Cold;
    } else if (entry.heatScore < 200.0) {
        entry.temperature = SlabTemperature::Warm;
    } else {
        entry.temperature = SlabTemperature::Hot;
    }
}

void HeatMapTracker::applyEpochDecay(double decayFactor) {
    std::lock_guard lock(mutex_);
    // In-memory decay hook
    (void)decayFactor;
}

// ============================================================================
// 2. WriteBackCache Implementation
// ============================================================================

WriteBackCache::WriteBackCache(uint32_t maxCapacity)
    : maxCapacity_(maxCapacity) {
    entries_.reserve(maxCapacity_);
}

bool WriteBackCache::stageWrite(uint32_t virtualSlabIndex, uint32_t offsetInSlab, std::span<const uint8_t> data) {
    std::lock_guard lock(mutex_);

    // Check if entry already exists in cache
    for (auto& entry : entries_) {
        if (entry.virtualSlabIndex == virtualSlabIndex && entry.offsetInSlab == offsetInSlab) {
            entry.payload.assign(data.begin(), data.end());
            entry.isDirty = true;
            entry.cachedTimestampUs = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count());
            return true;
        }
    }

    if (entries_.size() >= maxCapacity_) {
        return false; // Cache full, must flush
    }

    WriteBackCacheEntry newEntry;
    newEntry.virtualSlabIndex = virtualSlabIndex;
    newEntry.offsetInSlab = offsetInSlab;
    newEntry.payload.assign(data.begin(), data.end());
    newEntry.isDirty = true;
    newEntry.cachedTimestampUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    entries_.push_back(std::move(newEntry));
    return true;
}

bool WriteBackCache::tryRead(uint32_t virtualSlabIndex, uint32_t offsetInSlab, std::span<uint8_t> outBuffer) {
    std::lock_guard lock(mutex_);
    for (const auto& entry : entries_) {
        if (entry.virtualSlabIndex == virtualSlabIndex && entry.offsetInSlab == offsetInSlab) {
            size_t copyLen = std::min(outBuffer.size(), entry.payload.size());
            std::memcpy(outBuffer.data(), entry.payload.data(), copyLen);
            return true;
        }
    }
    return false;
}

std::vector<WriteBackCacheEntry> WriteBackCache::drainDirtyEntries() {
    std::lock_guard lock(mutex_);
    std::vector<WriteBackCacheEntry> dirty;
    for (auto& entry : entries_) {
        if (entry.isDirty) {
            dirty.push_back(entry);
            entry.isDirty = false;
        }
    }
    entries_.clear();
    return dirty;
}

size_t WriteBackCache::getDirtyCount() const noexcept {
    std::lock_guard lock(mutex_);
    size_t count = 0;
    for (const auto& entry : entries_) {
        if (entry.isDirty) count++;
    }
    return count;
}

void WriteBackCache::clear() {
    std::lock_guard lock(mutex_);
    entries_.clear();
}

// ============================================================================
// 3. SovereignNtfsTieredStorageEngine Implementation
// ============================================================================

SovereignNtfsTieredStorageEngine::SovereignNtfsTieredStorageEngine() {
    reset();
}

void SovereignNtfsTieredStorageEngine::reset() {
    std::unique_lock lock(engineLock_);
    disks_.clear();
    slabs_.clear();
    physicalSlabStorage_.clear();
    migrationHistory_.clear();
    writeBackCache_.clear();
    stats_ = TieredStorageStats{};
    isInitialized_ = false;
}

NtStatus SovereignNtfsTieredStorageEngine::registerPhysicalDisk(uint32_t diskId, const std::wstring& name,
                                                              StorageTierType tier, uint32_t totalSlabs,
                                                              uint32_t avgLatencyUs) {
    std::unique_lock lock(engineLock_);
    for (const auto& d : disks_) {
        if (d.diskId == diskId) {
            return NtStatus::ObjectNameCollision;
        }
    }

    PhysicalDiskSpec spec;
    spec.diskId = diskId;
    spec.diskName = name;
    spec.tier = tier;
    spec.totalSlabs = totalSlabs;
    spec.allocatedSlabs = 0;
    spec.avgLatencyUs = avgLatencyUs;
    spec.isOnline = true;
    disks_.push_back(spec);

    if (tier == StorageTierType::PerformanceTier) {
        stats_.fastTierTotalSlabs += totalSlabs;
    } else {
        stats_.slowTierTotalSlabs += totalSlabs;
    }

    return NtStatus::Success;
}

NtStatus SovereignNtfsTieredStorageEngine::initializePool(uint32_t virtualSlabCount) {
    std::unique_lock lock(engineLock_);
    if (disks_.empty()) {
        return NtStatus::DeviceNotReady;
    }

    slabs_.clear();
    slabs_.resize(virtualSlabCount);
    for (uint32_t i = 0; i < virtualSlabCount; ++i) {
        slabs_[i].virtualSlabIndex = i;
        slabs_[i].isAllocated = false;
        slabs_[i].pinning = PinningPolicy::DefaultDynamic;
        slabs_[i].temperature = SlabTemperature::Freezing;
        slabs_[i].migrationState = SlabMigrationState::Idle;
    }

    stats_.totalVirtualSlabs = virtualSlabCount;
    stats_.allocatedVirtualSlabs = 0;
    isInitialized_ = true;
    return NtStatus::Success;
}

NtStatus SovereignNtfsTieredStorageEngine::findFreePhysicalSlab(StorageTierType tier, uint32_t& outDiskId, uint32_t& outPhysicalSlab) {
    for (auto& disk : disks_) {
        if (disk.isOnline && disk.tier == tier && disk.allocatedSlabs < disk.totalSlabs) {
            outDiskId = disk.diskId;
            outPhysicalSlab = disk.allocatedSlabs;
            disk.allocatedSlabs++;
            return NtStatus::Success;
        }
    }
    return NtStatus::DiskFull;
}

void SovereignNtfsTieredStorageEngine::freePhysicalSlab(uint32_t diskId, uint32_t physicalSlab) {
    (void)physicalSlab;
    for (auto& disk : disks_) {
        if (disk.diskId == diskId && disk.allocatedSlabs > 0) {
            disk.allocatedSlabs--;
            break;
        }
    }
}

NtStatus SovereignNtfsTieredStorageEngine::allocateVirtualSlab(uint32_t virtualSlabIndex,
                                                             StorageTierType preferredTier,
                                                             PinningPolicy pinning) {
    std::unique_lock lock(engineLock_);
    if (!isInitialized_ || virtualSlabIndex >= slabs_.size()) {
        return NtStatus::InvalidParameter;
    }
    if (slabs_[virtualSlabIndex].isAllocated) {
        return NtStatus::ObjectNameCollision;
    }

    uint32_t diskId = 0;
    uint32_t physicalSlab = 0;

    // Try preferred tier first
    NtStatus status = findFreePhysicalSlab(preferredTier, diskId, physicalSlab);
    StorageTierType actualTier = preferredTier;

    if (!NT_SUCCESS(status)) {
        // Fall back to alternative tier
        StorageTierType altTier = (preferredTier == StorageTierType::PerformanceTier)
                                  ? StorageTierType::CapacityTier
                                  : StorageTierType::PerformanceTier;
        status = findFreePhysicalSlab(altTier, diskId, physicalSlab);
        if (!NT_SUCCESS(status)) {
            return NtStatus::DiskFull;
        }
        actualTier = altTier;
    }

    auto& entry = slabs_[virtualSlabIndex];
    entry.virtualSlabIndex = virtualSlabIndex;
    entry.currentTier = actualTier;
    entry.physicalDiskId = diskId;
    entry.physicalSlabIndex = physicalSlab;
    entry.readCount = 0;
    entry.writeCount = 0;
    entry.heatScore = 0.0;
    entry.temperature = SlabTemperature::Freezing;
    entry.pinning = pinning;
    entry.migrationState = SlabMigrationState::Idle;
    entry.isAllocated = true;

    // Allocate simulated backing buffer
    uint64_t key = (static_cast<uint64_t>(diskId) << 32) | physicalSlab;
    physicalSlabStorage_[key] = std::vector<uint8_t>(TIERED_STORAGE_DEFAULT_SLAB_SZ, 0x00);

    stats_.allocatedVirtualSlabs++;
    if (actualTier == StorageTierType::PerformanceTier) {
        stats_.fastTierAllocatedSlabs++;
    } else {
        stats_.slowTierAllocatedSlabs++;
    }

    return NtStatus::Success;
}

NtStatus SovereignNtfsTieredStorageEngine::freeVirtualSlab(uint32_t virtualSlabIndex) {
    std::unique_lock lock(engineLock_);
    if (!isInitialized_ || virtualSlabIndex >= slabs_.size()) {
        return NtStatus::InvalidParameter;
    }
    auto& entry = slabs_[virtualSlabIndex];
    if (!entry.isAllocated) {
        return NtStatus::NotFound;
    }

    freePhysicalSlab(entry.physicalDiskId, entry.physicalSlabIndex);
    uint64_t key = (static_cast<uint64_t>(entry.physicalDiskId) << 32) | entry.physicalSlabIndex;
    physicalSlabStorage_.erase(key);

    if (entry.currentTier == StorageTierType::PerformanceTier) {
        if (stats_.fastTierAllocatedSlabs > 0) stats_.fastTierAllocatedSlabs--;
    } else {
        if (stats_.slowTierAllocatedSlabs > 0) stats_.slowTierAllocatedSlabs--;
    }
    if (stats_.allocatedVirtualSlabs > 0) stats_.allocatedVirtualSlabs--;

    entry.isAllocated = false;
    entry.migrationState = SlabMigrationState::Idle;
    return NtStatus::Success;
}

NtStatus SovereignNtfsTieredStorageEngine::writeSlabData(uint32_t virtualSlabIndex, uint32_t offsetInSlab,
                                                       std::span<const uint8_t> buffer, bool useWbc) {
    std::unique_lock lock(engineLock_);
    if (!isInitialized_ || virtualSlabIndex >= slabs_.size()) {
        return NtStatus::InvalidParameter;
    }
    auto& entry = slabs_[virtualSlabIndex];
    if (!entry.isAllocated) {
        return NtStatus::NotFound;
    }
    if (offsetInSlab + buffer.size() > TIERED_STORAGE_DEFAULT_SLAB_SZ) {
        return NtStatus::BufferOverflow;
    }

    // Heat-Map update
    entry.writeCount++;
    heatMapTracker_.recordAccess(virtualSlabIndex, true, static_cast<uint32_t>(buffer.size()));
    heatMapTracker_.updateTemperature(entry);

    // Try Write-Back Cache (WBC) staging for capacity-tier slabs or general burst absorption
    if (useWbc && writeBackCache_.stageWrite(virtualSlabIndex, offsetInSlab, buffer)) {
        stats_.totalWbcHits++;
        return NtStatus::Success;
    }

    // Direct write to physical backing
    uint64_t key = (static_cast<uint64_t>(entry.physicalDiskId) << 32) | entry.physicalSlabIndex;
    auto it = physicalSlabStorage_.find(key);
    if (it == physicalSlabStorage_.end()) {
        physicalSlabStorage_[key] = std::vector<uint8_t>(TIERED_STORAGE_DEFAULT_SLAB_SZ, 0x00);
        it = physicalSlabStorage_.find(key);
    }

    std::memcpy(it->second.data() + offsetInSlab, buffer.data(), buffer.size());
    return NtStatus::Success;
}

NtStatus SovereignNtfsTieredStorageEngine::readSlabData(uint32_t virtualSlabIndex, uint32_t offsetInSlab,
                                                      std::span<uint8_t> buffer) {
    std::unique_lock lock(engineLock_);
    if (!isInitialized_ || virtualSlabIndex >= slabs_.size()) {
        return NtStatus::InvalidParameter;
    }
    auto& entry = slabs_[virtualSlabIndex];
    if (!entry.isAllocated) {
        return NtStatus::NotFound;
    }
    if (offsetInSlab + buffer.size() > TIERED_STORAGE_DEFAULT_SLAB_SZ) {
        return NtStatus::BufferOverflow;
    }

    // Heat-Map update
    entry.readCount++;
    heatMapTracker_.recordAccess(virtualSlabIndex, false, static_cast<uint32_t>(buffer.size()));
    heatMapTracker_.updateTemperature(entry);

    // Check WBC first
    if (writeBackCache_.tryRead(virtualSlabIndex, offsetInSlab, buffer)) {
        return NtStatus::Success;
    }

    // Read from physical backing
    uint64_t key = (static_cast<uint64_t>(entry.physicalDiskId) << 32) | entry.physicalSlabIndex;
    auto it = physicalSlabStorage_.find(key);
    if (it == physicalSlabStorage_.end()) {
        std::memset(buffer.data(), 0, buffer.size());
        return NtStatus::Success;
    }

    std::memcpy(buffer.data(), it->second.data() + offsetInSlab, buffer.size());
    return NtStatus::Success;
}

NtStatus SovereignNtfsTieredStorageEngine::flushWriteBackCache() {
    std::unique_lock lock(engineLock_);
    auto dirty = writeBackCache_.drainDirtyEntries();
    for (const auto& entry : dirty) {
        if (entry.virtualSlabIndex < slabs_.size() && slabs_[entry.virtualSlabIndex].isAllocated) {
            const auto& slab = slabs_[entry.virtualSlabIndex];
            uint64_t key = (static_cast<uint64_t>(slab.physicalDiskId) << 32) | slab.physicalSlabIndex;
            auto it = physicalSlabStorage_.find(key);
            if (it == physicalSlabStorage_.end()) {
                physicalSlabStorage_[key] = std::vector<uint8_t>(TIERED_STORAGE_DEFAULT_SLAB_SZ, 0x00);
                it = physicalSlabStorage_.find(key);
            }
            if (entry.offsetInSlab + entry.payload.size() <= TIERED_STORAGE_DEFAULT_SLAB_SZ) {
                std::memcpy(it->second.data() + entry.offsetInSlab, entry.payload.data(), entry.payload.size());
            }
            stats_.totalWbcFlushes++;
        }
    }
    return NtStatus::Success;
}

void SovereignNtfsTieredStorageEngine::setSlabPinning(uint32_t virtualSlabIndex, PinningPolicy policy) {
    std::unique_lock lock(engineLock_);
    if (virtualSlabIndex < slabs_.size()) {
        slabs_[virtualSlabIndex].pinning = policy;
    }
}

NtStatus SovereignNtfsTieredStorageEngine::executeMigrationCycle(double fastTierDemotionThreshold) {
    std::unique_lock lock(engineLock_);
    if (!isInitialized_) return NtStatus::DeviceNotReady;

    // Flush dirty cache before moving physical slabs
    auto dirty = writeBackCache_.drainDirtyEntries();
    for (const auto& d : dirty) {
        if (d.virtualSlabIndex < slabs_.size() && slabs_[d.virtualSlabIndex].isAllocated) {
            const auto& slab = slabs_[d.virtualSlabIndex];
            uint64_t key = (static_cast<uint64_t>(slab.physicalDiskId) << 32) | slab.physicalSlabIndex;
            auto it = physicalSlabStorage_.find(key);
            if (it != physicalSlabStorage_.end() && d.offsetInSlab + d.payload.size() <= TIERED_STORAGE_DEFAULT_SLAB_SZ) {
                std::memcpy(it->second.data() + d.offsetInSlab, d.payload.data(), d.payload.size());
            }
        }
    }

    // Step 1: Promotion Scan (Move Hot slabs or PinnedFast slabs from Capacity Tier to Fast NVMe Tier)
    for (auto& entry : slabs_) {
        if (!entry.isAllocated) continue;
        if (entry.currentTier == StorageTierType::CapacityTier && entry.pinning != PinningPolicy::PinnedSlow) {
            if (entry.temperature == SlabTemperature::Hot || entry.pinning == PinningPolicy::PinnedFast) {
                uint32_t newDiskId = 0;
                uint32_t newPhysicalSlab = 0;
                NtStatus allocStat = findFreePhysicalSlab(StorageTierType::PerformanceTier, newDiskId, newPhysicalSlab);
                if (NT_SUCCESS(allocStat)) {
                    // Copy extent data
                    uint64_t oldKey = (static_cast<uint64_t>(entry.physicalDiskId) << 32) | entry.physicalSlabIndex;
                    uint64_t newKey = (static_cast<uint64_t>(newDiskId) << 32) | newPhysicalSlab;
                    physicalSlabStorage_[newKey] = physicalSlabStorage_[oldKey];

                    // Free old physical slab
                    freePhysicalSlab(entry.physicalDiskId, entry.physicalSlabIndex);
                    physicalSlabStorage_.erase(oldKey);

                    // Record Migration Event
                    TierMigrationEvent evt{
                        .virtualSlabIndex = entry.virtualSlabIndex,
                        .sourceTier = StorageTierType::CapacityTier,
                        .destTier = StorageTierType::PerformanceTier,
                        .sourceDiskId = entry.physicalDiskId,
                        .sourcePhysicalSlab = entry.physicalSlabIndex,
                        .destDiskId = newDiskId,
                        .destPhysicalSlab = newPhysicalSlab,
                        .isPromotion = true,
                        .durationUs = 12
                    };
                    migrationHistory_.push_back(evt);

                    // Update mapping entry
                    entry.currentTier = StorageTierType::PerformanceTier;
                    entry.physicalDiskId = newDiskId;
                    entry.physicalSlabIndex = newPhysicalSlab;

                    stats_.slowTierAllocatedSlabs--;
                    stats_.fastTierAllocatedSlabs++;
                    stats_.totalPromotions++;
                }
            }
        }
    }

    // Step 2: Demotion Scan (Demote Cold/Freezing slabs from Fast NVMe Tier to Capacity Tier when Fast tier is full or over threshold)
    double fastTierUsage = (stats_.fastTierTotalSlabs > 0)
        ? (static_cast<double>(stats_.fastTierAllocatedSlabs) / static_cast<double>(stats_.fastTierTotalSlabs))
        : 0.0;

    if (fastTierUsage >= fastTierDemotionThreshold) {
        for (auto& entry : slabs_) {
            if (!entry.isAllocated) continue;
            if (entry.currentTier == StorageTierType::PerformanceTier && entry.pinning != PinningPolicy::PinnedFast) {
                if (entry.temperature == SlabTemperature::Freezing || entry.temperature == SlabTemperature::Cold) {
                    uint32_t newDiskId = 0;
                    uint32_t newPhysicalSlab = 0;
                    NtStatus allocStat = findFreePhysicalSlab(StorageTierType::CapacityTier, newDiskId, newPhysicalSlab);
                    if (NT_SUCCESS(allocStat)) {
                        uint64_t oldKey = (static_cast<uint64_t>(entry.physicalDiskId) << 32) | entry.physicalSlabIndex;
                        uint64_t newKey = (static_cast<uint64_t>(newDiskId) << 32) | newPhysicalSlab;
                        physicalSlabStorage_[newKey] = physicalSlabStorage_[oldKey];

                        freePhysicalSlab(entry.physicalDiskId, entry.physicalSlabIndex);
                        physicalSlabStorage_.erase(oldKey);

                        TierMigrationEvent evt{
                            .virtualSlabIndex = entry.virtualSlabIndex,
                            .sourceTier = StorageTierType::PerformanceTier,
                            .destTier = StorageTierType::CapacityTier,
                            .sourceDiskId = entry.physicalDiskId,
                            .sourcePhysicalSlab = entry.physicalSlabIndex,
                            .destDiskId = newDiskId,
                            .destPhysicalSlab = newPhysicalSlab,
                            .isPromotion = false,
                            .durationUs = 35
                        };
                        migrationHistory_.push_back(evt);

                        entry.currentTier = StorageTierType::CapacityTier;
                        entry.physicalDiskId = newDiskId;
                        entry.physicalSlabIndex = newPhysicalSlab;

                        stats_.fastTierAllocatedSlabs--;
                        stats_.slowTierAllocatedSlabs++;
                        stats_.totalDemotions++;

                        // Recalculate usage; if dropped below threshold, we can break
                        fastTierUsage = static_cast<double>(stats_.fastTierAllocatedSlabs) / static_cast<double>(stats_.fastTierTotalSlabs);
                        if (fastTierUsage < fastTierDemotionThreshold) {
                            break;
                        }
                    }
                }
            }
        }
    }

    return NtStatus::Success;
}

TieredStorageStats SovereignNtfsTieredStorageEngine::getStats() const {
    std::shared_lock lock(engineLock_);
    return stats_;
}

const SlabMappingEntry* SovereignNtfsTieredStorageEngine::getSlabInfo(uint32_t virtualSlabIndex) const {
    std::shared_lock lock(engineLock_);
    if (virtualSlabIndex < slabs_.size()) {
        return &slabs_[virtualSlabIndex];
    }
    return nullptr;
}

// ============================================================================
// 4. Horizon 371 / Suite 587 Architectural Verification Sentinel
// ============================================================================

bool VerifyHorizon371NtfsTieredStorageSubsystem() {
    SovereignNtfsTieredStorageEngine engine;

    // Pillar 1: Storage Pool Initialization, Dual-Tier Physical Disks & Slab Allocation Mapping Table
    {
        // 2 Fast NVMe disks (16 slabs each)
        NtStatus s1 = engine.registerPhysicalDisk(0, L"\\Device\\Harddisk0\\Partition1",
                                                StorageTierType::PerformanceTier, 16, 15);
        NtStatus s2 = engine.registerPhysicalDisk(1, L"\\Device\\Harddisk1\\Partition1",
                                                StorageTierType::PerformanceTier, 16, 15);
        // 2 Slow Capacity HDD disks (64 slabs each)
        NtStatus s3 = engine.registerPhysicalDisk(2, L"\\Device\\Harddisk2\\Partition1",
                                                StorageTierType::CapacityTier, 64, 5000);
        NtStatus s4 = engine.registerPhysicalDisk(3, L"\\Device\\Harddisk3\\Partition1",
                                                StorageTierType::CapacityTier, 64, 5000);

        if (!NT_SUCCESS(s1) || !NT_SUCCESS(s2) || !NT_SUCCESS(s3) || !NT_SUCCESS(s4)) {
            std::cerr << "[-] Step 1 Failed: Physical disk registration failed\n";
            return false;
        }

        NtStatus poolStat = engine.initializePool(32); // 32 virtual slabs
        if (!NT_SUCCESS(poolStat)) {
            std::cerr << "[-] Step 2 Failed: Pool initialization failed\n";
            return false;
        }

        // Allocate 10 virtual slabs initially on Capacity Tier
        for (uint32_t i = 0; i < 10; ++i) {
            NtStatus aStat = engine.allocateVirtualSlab(i, StorageTierType::CapacityTier);
            if (!NT_SUCCESS(aStat)) {
                std::cerr << "[-] Step 3 Failed: Virtual slab " << i << " allocation failed\n";
                return false;
            }
        }

        auto stats = engine.getStats();
        if (stats.allocatedVirtualSlabs != 10 || stats.slowTierAllocatedSlabs != 10) {
            std::cerr << "[-] Step 4 Failed: Stats mismatch on initial slab allocations\n";
            return false;
        }
    }

    // Pillar 2: Real-Time Heat-Map Access Tracking Subsystem & Thermal Classification
    {
        // Write payload to Slab 3 to drive up its temperature to Hot
        std::vector<uint8_t> testPattern(4096, 0xAA);
        for (int i = 0; i < 200; ++i) {
            NtStatus wStat = engine.writeSlabData(3, 0, testPattern, false);
            if (!NT_SUCCESS(wStat)) {
                std::cerr << "[-] Step 5 Failed: Write to slab 3 failed at iteration " << i << "\n";
                return false;
            }
        }

        // Slab 3 should now be classified as Hot
        const auto* info3 = engine.getSlabInfo(3);
        if (!info3 || info3->temperature != SlabTemperature::Hot || info3->heatScore < 200.0) {
            std::cerr << "[-] Step 6 Failed: Slab 3 temperature did not transition to Hot\n";
            return false;
        }

        // Slab 8 was untouched, should remain Freezing
        const auto* info8 = engine.getSlabInfo(8);
        if (!info8 || info8->temperature != SlabTemperature::Freezing) {
            std::cerr << "[-] Step 7 Failed: Slab 8 did not remain Freezing\n";
            return false;
        }
    }

    // Pillar 3: Autonomous Heat-Map Extent Migration: Hot Slab Promotion to Fast NVMe Tier
    {
        // Execute migration cycle
        NtStatus migStat = engine.executeMigrationCycle();
        if (!NT_SUCCESS(migStat)) {
            std::cerr << "[-] Step 8 Failed: Migration cycle execution failed\n";
            return false;
        }

        // Slab 3 should now reside on PerformanceTier (promoted)
        const auto* info3 = engine.getSlabInfo(3);
        if (!info3 || info3->currentTier != StorageTierType::PerformanceTier) {
            std::cerr << "[-] Step 9 Failed: Slab 3 was not promoted to PerformanceTier\n";
            return false;
        }

        // Verify data payload was preserved byte-for-byte across migration
        std::vector<uint8_t> readBack(4096, 0x00);
        NtStatus rStat = engine.readSlabData(3, 0, readBack);
        if (!NT_SUCCESS(rStat)) {
            std::cerr << "[-] Step 10 Failed: Read from promoted slab 3 failed\n";
            return false;
        }

        for (size_t b = 0; b < 4096; ++b) {
            if (readBack[b] != 0xAA) {
                std::cerr << "[-] Step 11 Failed: Promoted slab 3 payload corruption at byte " << b << "\n";
                return false;
            }
        }

        auto stats = engine.getStats();
        if (stats.totalPromotions == 0 || stats.fastTierAllocatedSlabs != 1) {
            std::cerr << "[-] Step 12 Failed: Promotion statistics mismatch\n";
            return false;
        }
    }

    // Pillar 4: Dynamic Fast-Tier Space Pressure & Cold Slab Demotion to Capacity Tier
    {
        // Allocate virtual slabs directly to Fast Tier until fast tier reaches threshold
        // Slabs 11 through 26 (16 slabs) directly on PerformanceTier
        for (uint32_t i = 11; i < 27; ++i) {
            NtStatus stat = engine.allocateVirtualSlab(i, StorageTierType::PerformanceTier);
            if (!NT_SUCCESS(stat)) {
                std::cerr << "[-] Step 13 Failed: Allocating slab " << i << " on Fast tier failed\n";
                return false;
            }
        }

        // Ensure Slab 11 remains Cold/Freezing (0 writes)
        const auto* info11 = engine.getSlabInfo(11);
        if (!info11 || info11->temperature != SlabTemperature::Freezing) {
            std::cerr << "[-] Step 14 Failed: Slab 11 temperature expected Freezing\n";
            return false;
        }

        // Trigger demotion with a 50% utilization threshold to force eviction of cold slabs
        NtStatus demoteStat = engine.executeMigrationCycle(0.50);
        if (!NT_SUCCESS(demoteStat)) {
            std::cerr << "[-] Step 15 Failed: Demotion migration cycle failed\n";
            return false;
        }

        auto stats = engine.getStats();
        if (stats.totalDemotions == 0) {
            std::cerr << "[-] Step 16 Failed: Expected at least one demotion under space pressure\n";
            return false;
        }

        // Slab 11 should have been demoted down to CapacityTier
        info11 = engine.getSlabInfo(11);
        if (!info11 || info11->currentTier != StorageTierType::CapacityTier) {
            std::cerr << "[-] Step 17 Failed: Cold slab 11 was not demoted to CapacityTier\n";
            return false;
        }
    }

    // Pillar 5: System Tier Pinning Policy & Write-Back Cache (WBC) Burst Absorption
    {
        // Pin Slab 12 permanently to PerformanceTier
        engine.setSlabPinning(12, PinningPolicy::PinnedFast);
        const auto* info12 = engine.getSlabInfo(12);
        if (!info12 || info12->pinning != PinningPolicy::PinnedFast) {
            std::cerr << "[-] Step 18 Failed: Slab 12 pinning policy not set to PinnedFast\n";
            return false;
        }

        // Trigger demotion with extreme threshold (0.10) to force eviction of any demotable slab
        engine.executeMigrationCycle(0.10);

        // Slab 12 must STILL reside on PerformanceTier because it is pinned
        info12 = engine.getSlabInfo(12);
        if (!info12 || info12->currentTier != StorageTierType::PerformanceTier) {
            std::cerr << "[-] Step 19 Failed: PinnedFast slab 12 was improperly demoted\n";
            return false;
        }

        // Test Write-Back Cache (WBC) Burst absorption
        std::vector<uint8_t> burstData = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
        NtStatus wbcStat = engine.writeSlabData(5, 1024, burstData, true);
        if (!NT_SUCCESS(wbcStat)) {
            std::cerr << "[-] Step 20 Failed: WBC burst write failed\n";
            return false;
        }

        // Read immediately from WBC before flush
        std::vector<uint8_t> wbcRead(8, 0x00);
        NtStatus wbcReadStat = engine.readSlabData(5, 1024, wbcRead);
        if (!NT_SUCCESS(wbcReadStat) || wbcRead != burstData) {
            std::cerr << "[-] Step 21 Failed: Immediate read from WBC failed\n";
            return false;
        }

        // Flush WBC to backing
        NtStatus flushStat = engine.flushWriteBackCache();
        if (!NT_SUCCESS(flushStat)) {
            std::cerr << "[-] Step 22 Failed: WBC flush failed\n";
            return false;
        }

        // Verify read back after flush
        std::vector<uint8_t> postFlushRead(8, 0x00);
        NtStatus postReadStat = engine.readSlabData(5, 1024, postFlushRead);
        if (!NT_SUCCESS(postReadStat) || postFlushRead != burstData) {
            std::cerr << "[-] Step 23 Failed: Read from flushed backing failed\n";
            return false;
        }
    }

    // Pillar 6: High-Throughput Tiered Storage & Migration Engine Benchmark
    {
        auto t0 = std::chrono::high_resolution_clock::now();
        constexpr int kBenchmarkOps = 5000;
        std::vector<uint8_t> benchBuf(512, 0x5A);

        for (int op = 0; op < kBenchmarkOps; ++op) {
            uint32_t targetSlab = static_cast<uint32_t>(op % 10);
            if (op % 2 == 0) {
                (void)engine.writeSlabData(targetSlab, 0, benchBuf, true);
            } else {
                (void)engine.readSlabData(targetSlab, 0, benchBuf);
            }
        }
        (void)engine.flushWriteBackCache();

        auto t1 = std::chrono::high_resolution_clock::now();
        double elapsedUs = std::chrono::duration<double, std::micro>(t1 - t0).count();
        double usPerOp = elapsedUs / static_cast<double>(kBenchmarkOps);

        std::cout << "    [BENCHMARK] Tiered Storage Slabs (5,000 Ops): " << elapsedUs
                  << " us total (" << usPerOp << " us/op)\n";

        if (usPerOp > 5.0) {
            std::cerr << "[-] Step 24 Failed: Benchmark latency too high (" << usPerOp << " us/op)\n";
            return false;
        }
    }

    return true;
}

} // namespace micant::ntfs::tiered
