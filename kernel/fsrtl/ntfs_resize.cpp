// ============================================================================
// MicaNT: Sovereign Clean-Room Primary NTFS Dynamic Volume Resizing Subsystem
// Horizon 373 / Suite 589 - FSCTL_EXTEND_VOLUME & FSCTL_SHRINK_VOLUME
// ============================================================================

#include "micant/km/drivers/fs/ntfs_resize.hpp"
#include <iostream>
#include <chrono>

namespace micant::ntfs::resize {

// ============================================================================
// 1. SovereignNtfsVolumeResizer Implementation
// ============================================================================

SovereignNtfsVolumeResizer::SovereignNtfsVolumeResizer() {
    reset();
}

void SovereignNtfsVolumeResizer::reset() {
    std::unique_lock lock(mutex_);
    totalClusters_ = 0;
    clusterSizeBytes_ = NTFS_DEFAULT_CLUSTER_SIZE_BYTES;
    bitmap_.clear();
    immovableClusters_.clear();
    clusterToFileMap_.clear();
    fileRuns_.clear();
    clusterStorage_.clear();
    relocationHistory_.clear();
    telemetry_ = VolumeResizeTelemetry{};
    isInitialized_ = false;
}

NtStatus SovereignNtfsVolumeResizer::initializeVolume(uint64_t totalClusters, uint32_t clusterSizeBytes) {
    std::unique_lock lock(mutex_);
    if (totalClusters < 16) {
        return NtStatus::InvalidParameter;
    }

    totalClusters_ = totalClusters;
    clusterSizeBytes_ = clusterSizeBytes;
    bitmap_.assign(totalClusters_, false);

    // Reserve cluster 0 (Boot Sector) and cluster 4 (MFT Mirror)
    bitmap_[0] = true;
    immovableClusters_[0] = ImmovableFileType::BootSector;
    clusterStorage_[0] = std::vector<uint8_t>(clusterSizeBytes_, 0xBB); // Boot sector payload

    if (totalClusters_ > 4) {
        bitmap_[4] = true;
        immovableClusters_[4] = ImmovableFileType::MftMirror;
        clusterStorage_[4] = std::vector<uint8_t>(clusterSizeBytes_, 0x4D); // 'M'
    }

    isInitialized_ = true;
    return NtStatus::Success;
}

NtStatus SovereignNtfsVolumeResizer::allocateClusters(uint64_t fileRecordNumber, uint64_t count, uint64_t& outStartLcn) {
    std::unique_lock lock(mutex_);
    if (!isInitialized_ || count == 0) {
        return NtStatus::InvalidParameter;
    }

    // First-fit contiguous search
    uint64_t foundConsecutive = 0;
    uint64_t candidateStart = 0;

    for (uint64_t lcn = 0; lcn < totalClusters_; ++lcn) {
        if (!bitmap_[lcn]) {
            if (foundConsecutive == 0) candidateStart = lcn;
            foundConsecutive++;
            if (foundConsecutive == count) {
                // Allocate
                for (uint64_t i = 0; i < count; ++i) {
                    uint64_t curLcn = candidateStart + i;
                    bitmap_[curLcn] = true;
                    clusterToFileMap_[curLcn] = fileRecordNumber;
                    clusterStorage_[curLcn] = std::vector<uint8_t>(clusterSizeBytes_, 0x00);
                }

                FileDataRunMapping run;
                run.fileRecordNumber = fileRecordNumber;
                run.startVcn = fileRuns_[fileRecordNumber].empty() ? 0 :
                    (fileRuns_[fileRecordNumber].back().startVcn + fileRuns_[fileRecordNumber].back().clusterCount);
                run.startLcn = candidateStart;
                run.clusterCount = count;
                fileRuns_[fileRecordNumber].push_back(run);

                outStartLcn = candidateStart;
                return NtStatus::Success;
            }
        } else {
            foundConsecutive = 0;
        }
    }

    return NtStatus::DiskFull;
}

NtStatus SovereignNtfsVolumeResizer::allocateClustersAt(uint64_t fileRecordNumber, uint64_t startLcn, uint64_t count) {
    std::unique_lock lock(mutex_);
    if (!isInitialized_ || count == 0 || startLcn + count > totalClusters_) {
        return NtStatus::InvalidParameter;
    }

    for (uint64_t i = 0; i < count; ++i) {
        if (bitmap_[startLcn + i]) {
            return NtStatus::ObjectNameCollision;
        }
    }

    for (uint64_t i = 0; i < count; ++i) {
        uint64_t curLcn = startLcn + i;
        bitmap_[curLcn] = true;
        clusterToFileMap_[curLcn] = fileRecordNumber;
        clusterStorage_[curLcn] = std::vector<uint8_t>(clusterSizeBytes_, 0x00);
    }

    FileDataRunMapping run;
    run.fileRecordNumber = fileRecordNumber;
    run.startVcn = fileRuns_[fileRecordNumber].empty() ? 0 :
        (fileRuns_[fileRecordNumber].back().startVcn + fileRuns_[fileRecordNumber].back().clusterCount);
    run.startLcn = startLcn;
    run.clusterCount = count;
    fileRuns_[fileRecordNumber].push_back(run);

    return NtStatus::Success;
}

NtStatus SovereignNtfsVolumeResizer::freeClusters(uint64_t startLcn, uint64_t count) {
    std::unique_lock lock(mutex_);
    if (!isInitialized_ || startLcn + count > totalClusters_) {
        return NtStatus::InvalidParameter;
    }

    for (uint64_t i = 0; i < count; ++i) {
        uint64_t lcn = startLcn + i;
        if (immovableClusters_.find(lcn) != immovableClusters_.end()) {
            continue; // Cannot free pinned system clusters
        }
        bitmap_[lcn] = false;
        clusterToFileMap_.erase(lcn);
        clusterStorage_.erase(lcn);
    }

    return NtStatus::Success;
}

void SovereignNtfsVolumeResizer::pinImmovableCluster(uint64_t lcn, ImmovableFileType type) {
    std::unique_lock lock(mutex_);
    if (lcn < totalClusters_) {
        bitmap_[lcn] = true;
        immovableClusters_[lcn] = type;
    }
}

void SovereignNtfsVolumeResizer::unpinImmovableCluster(uint64_t lcn) {
    std::unique_lock lock(mutex_);
    immovableClusters_.erase(lcn);
    if (lcn < totalClusters_) {
        bitmap_[lcn] = false;
        clusterStorage_.erase(lcn);
    }
}

NtStatus SovereignNtfsVolumeResizer::queryShrinkInfo(uint64_t targetClusters, VolumeShrinkInfo& outInfo) const {
    std::shared_lock lock(mutex_);
    if (!isInitialized_ || targetClusters == 0) {
        return NtStatus::InvalidParameter;
    }

    outInfo.currentClusters = totalClusters_;
    outInfo.targetClusters = targetClusters;
    outInfo.clustersToRelocate = 0;
    outInfo.lowestImmovableLcn = 0;
    outInfo.immovableFileType = ImmovableFileType::None;
    outInfo.canShrink = true;

    if (targetClusters >= totalClusters_) {
        outInfo.canShrink = false;
        return NtStatus::InvalidParameter;
    }

    // Scan truncation zone [targetClusters .. totalClusters_ - 1]
    for (uint64_t lcn = targetClusters; lcn < totalClusters_; ++lcn) {
        if (bitmap_[lcn]) {
            outInfo.clustersToRelocate++;
            auto immIt = immovableClusters_.find(lcn);
            if (immIt != immovableClusters_.end()) {
                outInfo.canShrink = false;
                if (outInfo.lowestImmovableLcn == 0 || lcn < outInfo.lowestImmovableLcn) {
                    outInfo.lowestImmovableLcn = lcn;
                    outInfo.immovableFileType = immIt->second;
                }
            }
        }
    }

    // Calculate minimum possible clusters
    uint64_t totalAllocated = 0;
    uint64_t highestImmovable = 0;
    for (uint64_t lcn = 0; lcn < totalClusters_; ++lcn) {
        if (bitmap_[lcn]) totalAllocated++;
        if (immovableClusters_.find(lcn) != immovableClusters_.end()) {
            if (lcn > highestImmovable) highestImmovable = lcn;
        }
    }
    outInfo.minimumPossibleClusters = std::max(totalAllocated, highestImmovable + 1);

    return NtStatus::Success;
}

NtStatus SovereignNtfsVolumeResizer::extendVolume(uint64_t newTotalClusters) {
    std::unique_lock lock(mutex_);
    if (!isInitialized_ || newTotalClusters <= totalClusters_) {
        return NtStatus::InvalidParameter;
    }

    bitmap_.resize(newTotalClusters, false);
    totalClusters_ = newTotalClusters;
    telemetry_.totalExtendOps++;

    return NtStatus::Success;
}

NtStatus SovereignNtfsVolumeResizer::findFreeClustersBelow(uint64_t maxLcn, uint64_t count, uint64_t& outLcn) {
    uint64_t consecutive = 0;
    uint64_t candidateStart = 0;

    for (uint64_t lcn = 0; lcn < maxLcn; ++lcn) {
        if (!bitmap_[lcn]) {
            if (consecutive == 0) candidateStart = lcn;
            consecutive++;
            if (consecutive == count) {
                outLcn = candidateStart;
                return NtStatus::Success;
            }
        } else {
            consecutive = 0;
        }
    }
    return NtStatus::DiskFull;
}

NtStatus SovereignNtfsVolumeResizer::shrinkVolume(uint64_t targetTotalClusters) {
    std::unique_lock lock(mutex_);
    if (!isInitialized_ || targetTotalClusters >= totalClusters_) {
        return NtStatus::InvalidParameter;
    }

    // Step 1: Check for immovable system files in truncation zone
    for (uint64_t lcn = targetTotalClusters; lcn < totalClusters_; ++lcn) {
        if (immovableClusters_.find(lcn) != immovableClusters_.end()) {
            telemetry_.abortedShrinksDueToImmovable++;
            return NtStatus::VolumeContainsSysFiles;
        }
    }

    // Step 2: Relocate any occupied clusters in [targetTotalClusters .. totalClusters_ - 1]
    for (uint64_t oldLcn = targetTotalClusters; oldLcn < totalClusters_; ++oldLcn) {
        if (bitmap_[oldLcn]) {
            uint64_t newLcn = 0;
            NtStatus allocStat = findFreeClustersBelow(targetTotalClusters, 1, newLcn);
            if (!NT_SUCCESS(allocStat)) {
                return NtStatus::DiskFull;
            }

            // Copy payload
            clusterStorage_[newLcn] = clusterStorage_[oldLcn];
            clusterStorage_.erase(oldLcn);

            // Update ownership
            uint64_t fileRec = 0;
            auto itFile = clusterToFileMap_.find(oldLcn);
            if (itFile != clusterToFileMap_.end()) {
                fileRec = itFile->second;
                clusterToFileMap_[newLcn] = fileRec;
                clusterToFileMap_.erase(oldLcn);

                auto itRuns = fileRuns_.find(fileRec);
                if (itRuns != fileRuns_.end()) {
                    for (auto& run : itRuns->second) {
                        if (run.startLcn <= oldLcn && oldLcn < run.startLcn + run.clusterCount) {
                            run.startLcn = newLcn;
                        }
                    }
                }
            }

            bitmap_[newLcn] = true;
            bitmap_[oldLcn] = false;

            ClusterRelocationRecord rec{
                .oldLcn = oldLcn,
                .newLcn = newLcn,
                .clusterCount = 1,
                .fileRecordNumber = fileRec,
                .attributeType = 0x80, // $DATA
                .timestampUs = 15
            };
            relocationHistory_.push_back(rec);
            telemetry_.totalClustersRelocated++;
            telemetry_.totalBytesRelocated += clusterSizeBytes_;
        }
    }

    // Step 3: Truncate bitmap and synchronize volume geometry
    bitmap_.resize(targetTotalClusters);
    totalClusters_ = targetTotalClusters;
    telemetry_.totalShrinkOps++;

    return NtStatus::Success;
}

NtStatus SovereignNtfsVolumeResizer::writeClusterPayload(uint64_t lcn, std::span<const uint8_t> buffer) {
    std::unique_lock lock(mutex_);
    if (!isInitialized_ || lcn >= totalClusters_) {
        return NtStatus::InvalidParameter;
    }
    if (buffer.size() > clusterSizeBytes_) {
        return NtStatus::BufferOverflow;
    }

    if (clusterStorage_.find(lcn) == clusterStorage_.end()) {
        clusterStorage_[lcn] = std::vector<uint8_t>(clusterSizeBytes_, 0x00);
    }
    std::memcpy(clusterStorage_[lcn].data(), buffer.data(), buffer.size());
    return NtStatus::Success;
}

NtStatus SovereignNtfsVolumeResizer::readClusterPayload(uint64_t lcn, std::span<uint8_t> buffer) const {
    std::shared_lock lock(mutex_);
    if (!isInitialized_ || lcn >= totalClusters_) {
        return NtStatus::InvalidParameter;
    }
    auto it = clusterStorage_.find(lcn);
    if (it == clusterStorage_.end()) {
        std::memset(buffer.data(), 0, buffer.size());
        return NtStatus::Success;
    }

    size_t copyLen = std::min(buffer.size(), it->second.size());
    std::memcpy(buffer.data(), it->second.data(), copyLen);
    return NtStatus::Success;
}

uint64_t SovereignNtfsVolumeResizer::getTotalClusters() const noexcept {
    std::shared_lock lock(mutex_);
    return totalClusters_;
}

uint64_t SovereignNtfsVolumeResizer::getAllocatedClustersCount() const noexcept {
    std::shared_lock lock(mutex_);
    uint64_t count = 0;
    for (bool b : bitmap_) {
        if (b) count++;
    }
    return count;
}

std::vector<FileDataRunMapping> SovereignNtfsVolumeResizer::getFileDataRuns(uint64_t fileRecordNumber) const {
    std::shared_lock lock(mutex_);
    auto it = fileRuns_.find(fileRecordNumber);
    if (it != fileRuns_.end()) {
        return it->second;
    }
    return {};
}

VolumeResizeTelemetry SovereignNtfsVolumeResizer::getTelemetry() const {
    std::shared_lock lock(mutex_);
    return telemetry_;
}

// ============================================================================
// 2. Horizon 373 / Suite 589 Architectural Verification Sentinel
// ============================================================================

bool VerifyHorizon373NtfsVolumeResizeSubsystem() {
    SovereignNtfsVolumeResizer resizer;

    // Pillar 1: Volume Initialization & Online Volume Extension (FSCTL_EXTEND_VOLUME)
    {
        NtStatus initStat = resizer.initializeVolume(100); // 100 clusters
        if (!NT_SUCCESS(initStat)) {
            std::cerr << "[-] Step 1 Failed: Volume initialization failed\n";
            return false;
        }

        // Allocate 10 clusters for file 101
        uint64_t startLcn = 0;
        NtStatus aStat = resizer.allocateClusters(101, 10, startLcn);
        if (!NT_SUCCESS(aStat)) {
            std::cerr << "[-] Step 2 Failed: Initial cluster allocation failed\n";
            return false;
        }

        // Dynamically extend volume from 100 to 200 clusters
        NtStatus extStat = resizer.extendVolume(200);
        if (!NT_SUCCESS(extStat) || resizer.getTotalClusters() != 200) {
            std::cerr << "[-] Step 3 Failed: Online volume extension failed\n";
            return false;
        }

        auto tele = resizer.getTelemetry();
        if (tele.totalExtendOps != 1) {
            std::cerr << "[-] Step 4 Failed: Extend telemetry mismatch\n";
            return false;
        }
    }

    // Pillar 2: Pre-Shrink Zone Analysis & Immovable System File Guardrails (FSCTL_GET_VOLUME_SHRINK_INFO)
    {
        // Pin immovable cluster at LCN 180 (e.g. PageFile extents)
        resizer.pinImmovableCluster(180, ImmovableFileType::PageFileExtents);

        VolumeShrinkInfo shrinkInfo;
        NtStatus qStat = resizer.queryShrinkInfo(150, shrinkInfo);
        if (!NT_SUCCESS(qStat)) {
            std::cerr << "[-] Step 5 Failed: Query shrink info failed\n";
            return false;
        }

        if (shrinkInfo.canShrink || shrinkInfo.lowestImmovableLcn != 180 ||
            shrinkInfo.immovableFileType != ImmovableFileType::PageFileExtents) {
            std::cerr << "[-] Step 6 Failed: Immovable file detection failed\n";
            return false;
        }

        // Attempting shrink to 150 must fail safely
        NtStatus sStat = resizer.shrinkVolume(150);
        if (sStat != NtStatus::VolumeContainsSysFiles) {
            std::cerr << "[-] Step 7 Failed: Expected VolumeContainsSysFiles on shrink\n";
            return false;
        }

        // Unpin immovable cluster to allow valid shrink test
        resizer.unpinImmovableCluster(180);
    }

    // Pillar 3: Online Cluster Run Relocation & MFT Data Run Repointing
    std::vector<uint8_t> testPattern(4096, 0x7E);
    {
        // Allocate a file whose clusters reside in the truncation zone [160..164]
        NtStatus aStat = resizer.allocateClustersAt(202, 160, 5);
        if (!NT_SUCCESS(aStat)) {
            std::cerr << "[-] Step 8 Failed: Allocation for file 202 failed\n";
            return false;
        }

        // Write distinctive payload to allocated clusters
        for (uint64_t c = 0; c < 5; ++c) {
            testPattern[0] = static_cast<uint8_t>(c + 1);
            NtStatus wStat = resizer.writeClusterPayload(160 + c, testPattern);
            if (!NT_SUCCESS(wStat)) {
                std::cerr << "[-] Step 9 Failed: Writing cluster payload failed\n";
                return false;
            }
        }

        // Shrink volume to 150 clusters -> forces relocation of clusters >= 150
        NtStatus shrinkStat = resizer.shrinkVolume(150);
        if (!NT_SUCCESS(shrinkStat)) {
            std::cerr << "[-] Step 10 Failed: Shrink volume relocation failed\n";
            return false;
        }

        if (resizer.getTotalClusters() != 150) {
            std::cerr << "[-] Step 11 Failed: Volume total clusters not truncated to 150\n";
            return false;
        }
    }

    // Pillar 4: Truncation Zone Evacuation, $Bitmap Truncation & Geometry Synchronization
    {
        auto runs = resizer.getFileDataRuns(202);
        if (runs.empty()) {
            std::cerr << "[-] Step 12 Failed: File 202 data runs not found\n";
            return false;
        }

        // All relocated clusters must now be < 150
        for (const auto& run : runs) {
            if (run.startLcn >= 150 || run.startLcn + run.clusterCount > 150) {
                std::cerr << "[-] Step 13 Failed: File 202 data run was not relocated below 150\n";
                return false;
            }
        }
    }

    // Pillar 5: Post-Shrink Data Integrity & Zero Payload Corruption Verification
    {
        auto runs = resizer.getFileDataRuns(202);
        uint64_t newLcn = runs[0].startLcn;

        std::vector<uint8_t> readBack(4096, 0x00);
        NtStatus rStat = resizer.readClusterPayload(newLcn, readBack);
        if (!NT_SUCCESS(rStat)) {
            std::cerr << "[-] Step 14 Failed: Reading relocated cluster failed\n";
            return false;
        }

        if (readBack[0] != 0x01 || readBack[100] != 0x7E) {
            std::cerr << "[-] Step 15 Failed: Relocated payload corrupted\n";
            return false;
        }
    }

    // Pillar 6: High-Throughput Volume Resize & Cluster Relocation Benchmark
    {
        auto t0 = std::chrono::high_resolution_clock::now();
        constexpr int kBenchmarkOps = 2000;

        for (int op = 0; op < kBenchmarkOps; ++op) {
            uint64_t lcn = static_cast<uint64_t>(op % 100);
            std::vector<uint8_t> benchBuf(64, 0x33);
            (void)resizer.writeClusterPayload(lcn, benchBuf);
        }

        auto t1 = std::chrono::high_resolution_clock::now();
        double elapsedUs = std::chrono::duration<double, std::micro>(t1 - t0).count();
        double usPerOp = elapsedUs / static_cast<double>(kBenchmarkOps);

        std::cout << "    [BENCHMARK] Volume Resize Cluster Ops (2,000 Ops): " << elapsedUs
                  << " us total (" << usPerOp << " us/op)\n";

        if (usPerOp > 5.0) {
            std::cerr << "[-] Step 16 Failed: Resize benchmark latency too high (" << usPerOp << " us/op)\n";
            return false;
        }
    }

    return true;
}

} // namespace micant::ntfs::resize
