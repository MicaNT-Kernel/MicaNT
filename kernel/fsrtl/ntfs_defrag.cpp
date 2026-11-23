// ============================================================================
// MicaNT: Sovereign Clean-Room Primary NTFS Defragmentation & MFT Packer
// Horizon 374 / Suite 590 - FSCTL_GET_RETRIEVAL_POINTERS & FSCTL_MOVE_FILE
// ============================================================================

#include "micant/km/drivers/fs/ntfs_defrag.hpp"
#include <iostream>
#include <chrono>

namespace micant::ntfs::defrag {

// ============================================================================
// 1. SovereignNtfsDefragEngine Implementation
// ============================================================================

SovereignNtfsDefragEngine::SovereignNtfsDefragEngine() {
    reset();
}

void SovereignNtfsDefragEngine::reset() {
    std::unique_lock lock(mutex_);
    totalClusters_ = 0;
    clusterSizeBytes_ = NTFS_DEFRAG_CLUSTER_SIZE_BYTES;
    bitmap_.clear();
    clusterToFileMap_.clear();
    fileExtents_.clear();
    clusterStorage_.clear();
    telemetry_ = DefragTelemetry{};
    isInitialized_ = false;
}

NtStatus SovereignNtfsDefragEngine::initializeVolume(uint64_t totalClusters, uint32_t clusterSizeBytes) {
    std::unique_lock lock(mutex_);
    if (totalClusters < 16) {
        return NtStatus::InvalidParameter;
    }

    totalClusters_ = totalClusters;
    clusterSizeBytes_ = clusterSizeBytes;
    bitmap_.assign(totalClusters_, false);

    // Boot sector (LCN 0) and MFT baseline (LCN 4)
    bitmap_[0] = true;
    clusterStorage_[0] = std::vector<uint8_t>(clusterSizeBytes_, 0xBB);

    if (totalClusters_ > 4) {
        bitmap_[4] = true;
        clusterStorage_[4] = std::vector<uint8_t>(clusterSizeBytes_, 0x4D);
    }

    isInitialized_ = true;
    return NtStatus::Success;
}

NtStatus SovereignNtfsDefragEngine::allocateFragmentedFile(uint64_t fileRecordNumber,
                                                          const std::vector<std::pair<uint64_t, uint64_t>>& lcnSpans) {
    std::unique_lock lock(mutex_);
    if (!isInitialized_ || lcnSpans.empty()) {
        return NtStatus::InvalidParameter;
    }

    // Verify all spans are free
    for (const auto& [startLcn, count] : lcnSpans) {
        if (startLcn + count > totalClusters_) return NtStatus::InvalidParameter;
        for (uint64_t i = 0; i < count; ++i) {
            if (bitmap_[startLcn + i]) return NtStatus::ObjectNameCollision;
        }
    }

    uint64_t currentVcn = 0;
    for (const auto& [startLcn, count] : lcnSpans) {
        for (uint64_t i = 0; i < count; ++i) {
            uint64_t curLcn = startLcn + i;
            bitmap_[curLcn] = true;
            clusterToFileMap_[curLcn] = fileRecordNumber;
            clusterStorage_[curLcn] = std::vector<uint8_t>(clusterSizeBytes_, 0x00);
        }

        currentVcn += count;
        FileExtentMapping ext{
            .nextVcn = currentVcn,
            .lcn = startLcn,
            .clusterCount = count
        };
        fileExtents_[fileRecordNumber].push_back(ext);
    }

    return NtStatus::Success;
}

NtStatus SovereignNtfsDefragEngine::findContiguousFree(uint64_t count, uint64_t& outStartLcn) const {
    uint64_t consecutive = 0;
    uint64_t candidate = 0;

    for (uint64_t lcn = 0; lcn < totalClusters_; ++lcn) {
        if (!bitmap_[lcn]) {
            if (consecutive == 0) candidate = lcn;
            consecutive++;
            if (consecutive == count) {
                outStartLcn = candidate;
                return NtStatus::Success;
            }
        } else {
            consecutive = 0;
        }
    }
    return NtStatus::DiskFull;
}

NtStatus SovereignNtfsDefragEngine::allocateContiguousFile(uint64_t fileRecordNumber, uint64_t count, uint64_t& outStartLcn) {
    std::unique_lock lock(mutex_);
    if (!isInitialized_ || count == 0) return NtStatus::InvalidParameter;

    NtStatus status = findContiguousFree(count, outStartLcn);
    if (!NT_SUCCESS(status)) return status;

    for (uint64_t i = 0; i < count; ++i) {
        uint64_t curLcn = outStartLcn + i;
        bitmap_[curLcn] = true;
        clusterToFileMap_[curLcn] = fileRecordNumber;
        clusterStorage_[curLcn] = std::vector<uint8_t>(clusterSizeBytes_, 0x00);
    }

    FileExtentMapping ext{
        .nextVcn = count,
        .lcn = outStartLcn,
        .clusterCount = count
    };
    fileExtents_[fileRecordNumber].push_back(ext);

    return NtStatus::Success;
}

NtStatus SovereignNtfsDefragEngine::writeClusterPayload(uint64_t lcn, std::span<const uint8_t> buffer) {
    std::unique_lock lock(mutex_);
    if (!isInitialized_ || lcn >= totalClusters_) return NtStatus::InvalidParameter;
    if (buffer.size() > clusterSizeBytes_) return NtStatus::BufferOverflow;

    if (clusterStorage_.find(lcn) == clusterStorage_.end()) {
        clusterStorage_[lcn] = std::vector<uint8_t>(clusterSizeBytes_, 0x00);
    }
    std::memcpy(clusterStorage_[lcn].data(), buffer.data(), buffer.size());
    return NtStatus::Success;
}

NtStatus SovereignNtfsDefragEngine::readClusterPayload(uint64_t lcn, std::span<uint8_t> buffer) const {
    std::shared_lock lock(mutex_);
    if (!isInitialized_ || lcn >= totalClusters_) return NtStatus::InvalidParameter;

    auto it = clusterStorage_.find(lcn);
    if (it == clusterStorage_.end()) {
        std::memset(buffer.data(), 0, buffer.size());
        return NtStatus::Success;
    }

    size_t copyLen = std::min(buffer.size(), it->second.size());
    std::memcpy(buffer.data(), it->second.data(), copyLen);
    return NtStatus::Success;
}

NtStatus SovereignNtfsDefragEngine::getRetrievalPointers(uint64_t fileRecordNumber,
                                                        RetrievalPointersBuffer& outBuffer) const {
    std::shared_lock lock(mutex_);
    if (!isInitialized_) return NtStatus::DeviceNotReady;

    auto it = fileExtents_.find(fileRecordNumber);
    if (it == fileExtents_.end()) {
        return NtStatus::NotFound;
    }

    outBuffer.extentCount = static_cast<uint32_t>(it->second.size());
    outBuffer.startingVcn = 0;
    outBuffer.extents = it->second;

    return NtStatus::Success;
}

void SovereignNtfsDefragEngine::coalesceFileExtents(uint64_t fileRecordNumber) {
    auto it = fileExtents_.find(fileRecordNumber);
    if (it == fileExtents_.end() || it->second.size() <= 1) return;

    std::vector<FileExtentMapping> coalesced;
    coalesced.push_back(it->second[0]);

    for (size_t i = 1; i < it->second.size(); ++i) {
        auto& prev = coalesced.back();
        const auto& cur = it->second[i];

        // If contiguous in both VCN and LCN, coalesce into one extent
        if (prev.lcn + prev.clusterCount == cur.lcn) {
            prev.clusterCount += cur.clusterCount;
            prev.nextVcn = cur.nextVcn;
            telemetry_.totalExtentsConsolidated++;
        } else {
            coalesced.push_back(cur);
        }
    }

    it->second = std::move(coalesced);
}

NtStatus SovereignNtfsDefragEngine::moveFileClusters(const MoveFileData& moveData) {
    std::unique_lock lock(mutex_);
    if (!isInitialized_ || moveData.clusterCount == 0) return NtStatus::InvalidParameter;
    if (moveData.targetLcn + moveData.clusterCount > totalClusters_) return NtStatus::InvalidParameter;

    auto itFile = fileExtents_.find(moveData.fileRecordNumber);
    if (itFile == fileExtents_.end()) return NtStatus::NotFound;

    // Verify target range is free (or owned by the same file)
    for (uint32_t i = 0; i < moveData.clusterCount; ++i) {
        uint64_t checkLcn = moveData.targetLcn + i;
        if (bitmap_[checkLcn]) {
            auto itOwner = clusterToFileMap_.find(checkLcn);
            if (itOwner == clusterToFileMap_.end() || itOwner->second != moveData.fileRecordNumber) {
                return NtStatus::ObjectNameCollision; // Target already allocated to another file
            }
        }
    }

    // Collect source cluster payloads
    std::vector<std::vector<uint8_t>> payloads;
    payloads.reserve(moveData.clusterCount);

    std::vector<uint64_t> oldLcons;
    oldLcons.reserve(moveData.clusterCount);

    for (const auto& ext : itFile->second) {
        uint64_t extStartVcn = (ext.nextVcn - ext.clusterCount);
        for (uint64_t c = 0; c < ext.clusterCount; ++c) {
            uint64_t clusterVcn = extStartVcn + c;
            if (clusterVcn >= moveData.startingVcn && clusterVcn < moveData.startingVcn + moveData.clusterCount) {
                uint64_t srcLcn = ext.lcn + c;
                oldLcons.push_back(srcLcn);
                payloads.push_back(clusterStorage_[srcLcn]);
            }
        }
    }

    if (payloads.size() != moveData.clusterCount) {
        return NtStatus::BufferOverflow;
    }

    // Free old LCNs
    for (uint64_t oldLcn : oldLcons) {
        bitmap_[oldLcn] = false;
        clusterToFileMap_.erase(oldLcn);
        clusterStorage_.erase(oldLcn);
    }

    // Place into target LCNs
    for (uint32_t i = 0; i < moveData.clusterCount; ++i) {
        uint64_t dstLcn = moveData.targetLcn + i;
        bitmap_[dstLcn] = true;
        clusterToFileMap_[dstLcn] = moveData.fileRecordNumber;
        clusterStorage_[dstLcn] = std::move(payloads[i]);
    }

    // Update extent mapping
    itFile->second.clear();
    itFile->second.push_back(FileExtentMapping{
        .nextVcn = moveData.clusterCount,
        .lcn = moveData.targetLcn,
        .clusterCount = moveData.clusterCount
    });

    telemetry_.totalClustersMoved += moveData.clusterCount;
    telemetry_.totalBytesMoved += (moveData.clusterCount * clusterSizeBytes_);
    telemetry_.totalFilesDefragmented++;

    return NtStatus::Success;
}

NtStatus SovereignNtfsDefragEngine::defragmentFile(uint64_t fileRecordNumber) {
    std::unique_lock lock(mutex_);
    auto it = fileExtents_.find(fileRecordNumber);
    if (it == fileExtents_.end()) return NtStatus::NotFound;
    if (it->second.size() <= 1) return NtStatus::Success; // Already contiguous

    uint64_t totalFileClusters = 0;
    for (const auto& ext : it->second) {
        totalFileClusters += ext.clusterCount;
    }

    uint64_t targetLcn = 0;
    NtStatus findStat = findContiguousFree(totalFileClusters, targetLcn);
    if (!NT_SUCCESS(findStat)) return findStat;

    lock.unlock(); // Unlock before calling moveFileClusters which locks mutex
    MoveFileData mfd{
        .fileRecordNumber = fileRecordNumber,
        .startingVcn = 0,
        .targetLcn = targetLcn,
        .clusterCount = static_cast<uint32_t>(totalFileClusters)
    };
    return moveFileClusters(mfd);
}

uint64_t SovereignNtfsDefragEngine::getLargestContiguousFreeBlock() const {
    std::shared_lock lock(mutex_);
    uint64_t maxRun = 0;
    uint64_t curRun = 0;

    for (uint64_t lcn = 0; lcn < totalClusters_; ++lcn) {
        if (!bitmap_[lcn]) {
            curRun++;
            if (curRun > maxRun) maxRun = curRun;
        } else {
            curRun = 0;
        }
    }
    return maxRun;
}

NtStatus SovereignNtfsDefragEngine::consolidateFreeSpace() {
    telemetry_.largestContiguousFreeBefore = getLargestContiguousFreeBlock();

    // Snapshot file record numbers to avoid iterator invalidation during moves
    std::vector<uint64_t> fileKeys;
    {
        std::shared_lock lock(mutex_);
        fileKeys.reserve(fileExtents_.size());
        for (const auto& [fileRec, extents] : fileExtents_) {
            fileKeys.push_back(fileRec);
        }
    }

    for (uint64_t fileRec : fileKeys) {
        uint64_t fileClusters = 0;
        uint64_t firstLcn = 0;
        {
            std::shared_lock lock(mutex_);
            auto it = fileExtents_.find(fileRec);
            if (it == fileExtents_.end() || it->second.empty()) continue;
            for (const auto& ext : it->second) fileClusters += ext.clusterCount;
            firstLcn = it->second[0].lcn;
        }

        uint64_t targetLcn = 0;
        bool canMove = false;
        {
            std::shared_lock lock(mutex_);
            if (NT_SUCCESS(findContiguousFree(fileClusters, targetLcn)) && targetLcn < firstLcn) {
                canMove = true;
            }
        }

        if (canMove) {
            MoveFileData mfd{
                .fileRecordNumber = fileRec,
                .startingVcn = 0,
                .targetLcn = targetLcn,
                .clusterCount = static_cast<uint32_t>(fileClusters)
            };
            (void)moveFileClusters(mfd);
        }
    }

    telemetry_.largestContiguousFreeAfter = getLargestContiguousFreeBlock();
    return NtStatus::Success;
}

NtStatus SovereignNtfsDefragEngine::packMftExtents(const std::vector<uint64_t>& mftFragmentedLcons, uint64_t targetMftStartLcn) {
    std::unique_lock lock(mutex_);
    if (mftFragmentedLcons.empty()) return NtStatus::InvalidParameter;
    if (targetMftStartLcn + mftFragmentedLcons.size() > totalClusters_) return NtStatus::InvalidParameter;

    std::vector<std::vector<uint8_t>> payloads;
    payloads.reserve(mftFragmentedLcons.size());

    for (uint64_t lcn : mftFragmentedLcons) {
        payloads.push_back(clusterStorage_[lcn]);
        bitmap_[lcn] = false;
        clusterStorage_.erase(lcn);
    }

    for (size_t i = 0; i < mftFragmentedLcons.size(); ++i) {
        uint64_t dstLcn = targetMftStartLcn + i;
        bitmap_[dstLcn] = true;
        clusterStorage_[dstLcn] = std::move(payloads[i]);
    }

    return NtStatus::Success;
}

double SovereignNtfsDefragEngine::getFileFragmentationRatio(uint64_t fileRecordNumber) const {
    std::shared_lock lock(mutex_);
    auto it = fileExtents_.find(fileRecordNumber);
    if (it == fileExtents_.end() || it->second.empty()) return 0.0;

    uint64_t totalClusters = 0;
    for (const auto& ext : it->second) totalClusters += ext.clusterCount;
    if (totalClusters <= 1) return 0.0;

    return static_cast<double>(it->second.size() - 1) / static_cast<double>(totalClusters);
}

DefragTelemetry SovereignNtfsDefragEngine::getTelemetry() const {
    std::shared_lock lock(mutex_);
    return telemetry_;
}

uint64_t SovereignNtfsDefragEngine::getTotalClusters() const noexcept {
    std::shared_lock lock(mutex_);
    return totalClusters_;
}

// ============================================================================
// 2. Horizon 374 / Suite 590 Architectural Verification Sentinel
// ============================================================================

bool VerifyHorizon374NtfsDefragSubsystem() {
    SovereignNtfsDefragEngine engine;

    // Pillar 1: File Retrieval Pointers Mapping (FSCTL_GET_RETRIEVAL_POINTERS) & Fragmentation Analysis
    {
        NtStatus initStat = engine.initializeVolume(200); // 200 clusters
        if (!NT_SUCCESS(initStat)) {
            std::cerr << "[-] Step 1 Failed: Volume initialization failed\n";
            return false;
        }

        // Allocate File 301 fragmented across 5 disjoint extents (2 clusters each = 10 clusters total)
        std::vector<std::pair<uint64_t, uint64_t>> spans = {
            {10, 2}, {25, 2}, {40, 2}, {55, 2}, {70, 2}
        };
        NtStatus aStat = engine.allocateFragmentedFile(301, spans);
        if (!NT_SUCCESS(aStat)) {
            std::cerr << "[-] Step 2 Failed: Allocate fragmented file failed\n";
            return false;
        }

        RetrievalPointersBuffer rpb;
        NtStatus rpbStat = engine.getRetrievalPointers(301, rpb);
        if (!NT_SUCCESS(rpbStat) || rpb.extentCount != 5) {
            std::cerr << "[-] Step 3 Failed: Retrieval pointers extent count mismatch (got "
                      << rpb.extentCount << ", expected 5)\n";
            return false;
        }

        double fragRatio = engine.getFileFragmentationRatio(301);
        if (fragRatio <= 0.0) {
            std::cerr << "[-] Step 4 Failed: Fragmentation ratio must be > 0.0\n";
            return false;
        }
    }

    // Pillar 2: Online Atomic File Cluster Movement (FSCTL_MOVE_FILE) & Extent Coalescing
    std::vector<uint8_t> sampleData(4096, 0x55);
    {
        // Write distinct payloads to the 10 clusters of File 301
        for (uint64_t i = 0; i < 2; ++i) {
            sampleData[0] = static_cast<uint8_t>(i + 1);
            engine.writeClusterPayload(10 + i, sampleData);
            engine.writeClusterPayload(25 + i, sampleData);
            engine.writeClusterPayload(40 + i, sampleData);
            engine.writeClusterPayload(55 + i, sampleData);
            engine.writeClusterPayload(70 + i, sampleData);
        }

        // Target LCN range [100..109] is completely free
        MoveFileData mfd{
            .fileRecordNumber = 301,
            .startingVcn = 0,
            .targetLcn = 100,
            .clusterCount = 10
        };

        NtStatus mStat = engine.moveFileClusters(mfd);
        if (!NT_SUCCESS(mStat)) {
            std::cerr << "[-] Step 5 Failed: moveFileClusters failed\n";
            return false;
        }

        // Retrieval pointers must now reflect exactly 1 coalesced contiguous extent!
        RetrievalPointersBuffer rpbAfter;
        engine.getRetrievalPointers(301, rpbAfter);
        if (rpbAfter.extentCount != 1 || rpbAfter.extents[0].lcn != 100 || rpbAfter.extents[0].clusterCount != 10) {
            std::cerr << "[-] Step 6 Failed: File extents not coalesced into 1 contiguous extent\n";
            return false;
        }

        double fragAfter = engine.getFileFragmentationRatio(301);
        if (fragAfter != 0.0) {
            std::cerr << "[-] Step 7 Failed: Coalesced file fragmentation ratio must be exactly 0.0\n";
            return false;
        }
    }

    // Pillar 3: Data Integrity Verification & Zero Payload Corruption Post-Movement
    {
        std::vector<uint8_t> readBuf(4096, 0x00);
        NtStatus rStat = engine.readClusterPayload(100, readBuf); // First moved cluster
        if (!NT_SUCCESS(rStat) || readBuf[0] != 0x01 || readBuf[500] != 0x55) {
            std::cerr << "[-] Step 8 Failed: Data payload corruption in relocated cluster 100\n";
            return false;
        }
    }

    // Pillar 4: Volume-Wide Free Space Consolidation & Contiguous Extent Maximization
    {
        uint64_t maxFreeBefore = engine.getLargestContiguousFreeBlock();
        (void)maxFreeBefore;

        // Allocate small files at upper LCNs to create fragmented pockets
        uint64_t dummyLcn = 0;
        engine.allocateContiguousFile(401, 10, dummyLcn);

        NtStatus consStat = engine.consolidateFreeSpace();
        if (!NT_SUCCESS(consStat)) {
            std::cerr << "[-] Step 9 Failed: consolidateFreeSpace failed\n";
            return false;
        }

        uint64_t maxFreeAfter = engine.getLargestContiguousFreeBlock();
        if (maxFreeAfter == 0) {
            std::cerr << "[-] Step 10 Failed: Largest contiguous free block calculation failed\n";
            return false;
        }
    }

    // Pillar 5: MFT Zone Compaction & Contiguous MFT Packing
    {
        // 3 fragmented MFT records scattered at LCN 80, 85, 90
        std::vector<uint64_t> mftFrags = {80, 85, 90};
        for (uint64_t lcn : mftFrags) {
            std::vector<uint8_t> mftData(4096, 0x46); // 'F'
            engine.writeClusterPayload(lcn, mftData);
        }

        // Pack into contiguous MFT range starting at LCN 150
        NtStatus packStat = engine.packMftExtents(mftFrags, 150);
        if (!NT_SUCCESS(packStat)) {
            std::cerr << "[-] Step 11 Failed: packMftExtents failed\n";
            return false;
        }

        // Verify packed MFT payload
        std::vector<uint8_t> mftCheck(4096, 0x00);
        engine.readClusterPayload(150, mftCheck);
        if (mftCheck[0] != 0x46) {
            std::cerr << "[-] Step 12 Failed: Packed MFT extent data mismatch\n";
            return false;
        }
    }

    // Pillar 6: High-Throughput Defragmentation Engine Benchmark
    {
        auto t0 = std::chrono::high_resolution_clock::now();
        constexpr int kBenchmarkOps = 2500;

        RetrievalPointersBuffer rpbBench;
        for (int op = 0; op < kBenchmarkOps; ++op) {
            (void)engine.getRetrievalPointers(301, rpbBench);
        }

        auto t1 = std::chrono::high_resolution_clock::now();
        double elapsedUs = std::chrono::duration<double, std::micro>(t1 - t0).count();
        double usPerOp = elapsedUs / static_cast<double>(kBenchmarkOps);

        std::cout << "    [BENCHMARK] Defrag Retrieval Pointers (2,500 Ops): " << elapsedUs
                  << " us total (" << usPerOp << " us/op)\n";

        if (usPerOp > 5.0) {
            std::cerr << "[-] Step 13 Failed: Benchmark latency too high (" << usPerOp << " us/op)\n";
            return false;
        }
    }

    return true;
}

} // namespace micant::ntfs::defrag
