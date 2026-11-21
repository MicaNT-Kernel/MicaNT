// ============================================================================
// MicaNT: Sovereign Clean-Room Primary NTFS Storage Subsystem
// Horizon 363 / Suite 579 - Volume Shadow Copy (VSS) Provider, Volume Snapshot
// Driver (\Driver\VolSnap), Differential Block Copy-On-Write (COW) & DiffArea
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
#include <deque>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/km/drivers/fs/ntfs_primary_storage.hpp"

namespace micant::ntfs::vss {

// ============================================================================
// 1. VSS & VolSnap GUIDs, IOCTLs and Status Constants
// ============================================================================

struct VSS_ID {
    uint32_t Data1{0};
    uint16_t Data2{0};
    uint16_t Data3{0};
    uint8_t  Data4[8]{0};

    bool operator==(const VSS_ID& other) const noexcept {
        return Data1 == other.Data1 && Data2 == other.Data2 &&
               Data3 == other.Data3 && std::memcmp(Data4, other.Data4, 8) == 0;
    }
};

inline constexpr VSS_ID SOVEREIGN_VSS_PROVIDER_GUID = {
    0xA4B75204, 0x6258, 0x4444, { 0x93, 0x5C, 0x68, 0x15, 0x8F, 0x28, 0x3B, 0xEB }
};

// Standard NT VolSnap IOCTL definitions
inline constexpr uint32_t IOCTL_VOLSNAP_FLUSH_AND_HOLD_WRITES     = 0x00530008;
inline constexpr uint32_t IOCTL_VOLSNAP_RELEASE_WRITES            = 0x0053000C;
inline constexpr uint32_t IOCTL_VOLSNAP_PREPARE_FOR_SNAPSHOT      = 0x00530010;
inline constexpr uint32_t IOCTL_VOLSNAP_COMMIT_SNAPSHOT           = 0x00530014;
inline constexpr uint32_t IOCTL_VOLSNAP_QUERY_NAMES_OF_SNAPSHOTS  = 0x00530024;
inline constexpr uint32_t IOCTL_VOLSNAP_CLEAR_DIFF_AREA           = 0x00530038;

enum class VssSnapshotState : uint32_t {
    Uninitialized = 0,
    Preparing     = 1,
    Frozen        = 2,
    Committed     = 3,
    Mounted       = 4,
    Reverting     = 5,
    Deleted       = 6
};

// ============================================================================
// 2. Differential Block Storage (DiffArea) Entry
// ============================================================================

struct DiffBlock {
    uint64_t originalLcn{0};    // Logical cluster number on base volume
    uint64_t diffLcn{0};        // Allocation offset within diff area
    uint32_t clusterBytes{4096};
    std::vector<uint8_t> data;  // Preserved pre-mutation cluster data
    uint64_t timestampNs{0};    // Capture timestamp
    bool isZeroBlock{false};
};

// ============================================================================
// 3. Sovereign Volume Shadow Copy Instance (\Device\HarddiskVolumeShadowCopy{N})
// ============================================================================

class VolSnapDevice {
public:
    VolSnapDevice(uint32_t snapshotIndex, VSS_ID snapshotId, std::wstring_view baseVolumePath)
        : m_snapshotIndex(snapshotIndex), m_snapshotId(snapshotId), m_baseVolumePath(baseVolumePath) {
        std::wstringstream wss;
        wss << L"\\Device\\HarddiskVolumeShadowCopy" << snapshotIndex;
        m_devicePath = wss.str();

        std::wstringstream globalWss;
        globalWss << L"\\\\?\\GLOBALROOT\\Device\\HarddiskVolumeShadowCopy" << snapshotIndex;
        m_globalSymbolicLink = globalWss.str();
    }

    [[nodiscard]] uint32_t getSnapshotIndex() const noexcept { return m_snapshotIndex; }
    [[nodiscard]] const VSS_ID& getSnapshotId() const noexcept { return m_snapshotId; }
    [[nodiscard]] const std::wstring& getDevicePath() const noexcept { return m_devicePath; }
    [[nodiscard]] const std::wstring& getGlobalSymbolicLink() const noexcept { return m_globalSymbolicLink; }
    [[nodiscard]] const std::wstring& getBaseVolumePath() const noexcept { return m_baseVolumePath; }
    [[nodiscard]] VssSnapshotState getState() const noexcept { return m_state; }
    [[nodiscard]] uint64_t getCreationTimeNs() const noexcept { return m_creationTimeNs; }

    void setState(VssSnapshotState state) noexcept {
        m_state = state;
    }

    void setCreationTimeNs(uint64_t ts) noexcept {
        m_creationTimeNs = ts;
    }

    // Check if cluster is already captured in this snapshot's DiffArea
    [[nodiscard]] bool hasOriginalCluster(uint64_t lcn) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_diffMap.contains(lcn);
    }

    // Record pre-mutation cluster into DiffArea before base write proceeds
    void recordDiffBlock(uint64_t lcn, std::span<const uint8_t> originalData, uint64_t timestampNs) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_diffMap.contains(lcn)) {
            // Already captured at earlier point in time for this snapshot
            return;
        }

        DiffBlock block;
        block.originalLcn = lcn;
        block.diffLcn = m_nextDiffLcn++;
        block.clusterBytes = static_cast<uint32_t>(originalData.size());
        block.data.assign(originalData.begin(), originalData.end());
        block.timestampNs = timestampNs;
        block.isZeroBlock = std::all_of(originalData.begin(), originalData.end(), [](uint8_t b) { return b == 0; });

        m_diffBytes += block.clusterBytes;
        m_diffMap.emplace(lcn, std::move(block));
    }

    // Read cluster from snapshot: returns true if found in DiffArea, false if base volume pass-through
    bool readClusterFromSnapshot(uint64_t lcn, std::vector<uint8_t>& outData) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_diffMap.find(lcn);
        if (it != m_diffMap.end()) {
            outData = it->second.data;
            return true; // Read served from snapshot difference area
        }
        return false; // Read must pass through to base volume
    }

    [[nodiscard]] size_t getDiffBlockCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_diffMap.size();
    }

    [[nodiscard]] uint64_t getDiffBytes() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_diffBytes;
    }

    // Retrieve all recorded difference blocks (for rollback reversion)
    void getDiffBlocks(std::vector<DiffBlock>& outBlocks) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        outBlocks.clear();
        outBlocks.reserve(m_diffMap.size());
        for (const auto& [lcn, block] : m_diffMap) {
            outBlocks.push_back(block);
        }
    }

private:
    uint32_t m_snapshotIndex{0};
    VSS_ID m_snapshotId{};
    std::wstring m_baseVolumePath;
    std::wstring m_devicePath;
    std::wstring m_globalSymbolicLink;
    VssSnapshotState m_state{VssSnapshotState::Uninitialized};
    uint64_t m_creationTimeNs{0};
    uint64_t m_nextDiffLcn{0};
    uint64_t m_diffBytes{0};
    mutable std::mutex m_mutex;
    std::unordered_map<uint64_t, DiffBlock> m_diffMap;
};

// ============================================================================
// 4. Pending Write IRP (Held during Volume Freeze)
// ============================================================================

struct PendingWriteIrp {
    uint64_t targetLcn{0};
    std::vector<uint8_t> buffer;
    uint64_t sequenceNumber{0};
};

// ============================================================================
// 5. Simulated Base Volume Mock for Cluster Layering
// ============================================================================

class MockBaseVolume {
public:
    explicit MockBaseVolume(size_t totalClusters, size_t clusterSize = 4096)
        : m_clusterSize(clusterSize), m_clusters(totalClusters, std::vector<uint8_t>(clusterSize, 0)) {}

    [[nodiscard]] size_t getClusterSize() const noexcept { return m_clusterSize; }
    [[nodiscard]] size_t getTotalClusters() const noexcept { return m_clusters.size(); }

    void writeClusterDirect(uint64_t lcn, std::span<const uint8_t> data) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (lcn < m_clusters.size()) {
            size_t copyLen = std::min(data.size(), m_clusterSize);
            std::memcpy(m_clusters[lcn].data(), data.data(), copyLen);
        }
    }

    [[nodiscard]] std::vector<uint8_t> readClusterDirect(uint64_t lcn) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (lcn < m_clusters.size()) {
            return m_clusters[lcn];
        }
        return std::vector<uint8_t>(m_clusterSize, 0);
    }

private:
    size_t m_clusterSize{4096};
    mutable std::mutex m_mutex;
    std::vector<std::vector<uint8_t>> m_clusters;
};

// ============================================================================
// 6. VolSnap Filter Driver & Sovereign VSS Subsystem Engine
// ============================================================================

class SovereignVolSnapProvider {
public:
    explicit SovereignVolSnapProvider(std::shared_ptr<MockBaseVolume> baseVolume, std::wstring_view baseVolumePath = L"\\Device\\HarddiskVolume1")
        : m_baseVolume(std::move(baseVolume)), m_baseVolumePath(baseVolumePath) {}

    // 1. Prepare Snapshot
    NtStatus prepareSnapshot(const VSS_ID& snapshotId, std::shared_ptr<VolSnapDevice>& outDevice) {
        std::lock_guard<std::mutex> lock(m_engineMutex);

        uint32_t idx = ++m_snapshotCounter;
        auto device = std::make_shared<VolSnapDevice>(idx, snapshotId, m_baseVolumePath);
        device->setState(VssSnapshotState::Preparing);

        m_activeSnapshots.push_back(device);
        outDevice = device;
        return NtStatus::Success;
    }

    // 2. IOCTL_VOLSNAP_FLUSH_AND_HOLD_WRITES
    NtStatus flushAndHoldWrites() {
        std::lock_guard<std::mutex> lock(m_engineMutex);
        if (m_writesHeld) {
            return NtStatus::InvalidDeviceState;
        }

        m_writesHeld = true;
        // In real NT, file system cache is flushed to disk here.
        for (auto& snap : m_activeSnapshots) {
            if (snap->getState() == VssSnapshotState::Preparing) {
                snap->setState(VssSnapshotState::Frozen);
            }
        }
        return NtStatus::Success;
    }

    // 3. IOCTL_VOLSNAP_COMMIT_SNAPSHOT
    NtStatus commitSnapshot(const VSS_ID& snapshotId) {
        std::lock_guard<std::mutex> lock(m_engineMutex);

        auto it = std::find_if(m_activeSnapshots.begin(), m_activeSnapshots.end(),
            [&](const std::shared_ptr<VolSnapDevice>& s) { return s->getSnapshotId() == snapshotId; });

        if (it == m_activeSnapshots.end()) {
            return NtStatus::NoSuchFile;
        }

        auto now = std::chrono::high_resolution_clock::now().time_since_epoch();
        uint64_t tsNs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());

        (*it)->setCreationTimeNs(tsNs);
        (*it)->setState(VssSnapshotState::Committed);
        return NtStatus::Success;
    }

    // 4. IOCTL_VOLSNAP_RELEASE_WRITES
    NtStatus releaseWrites() {
        std::lock_guard<std::mutex> lock(m_engineMutex);
        if (!m_writesHeld) {
            return NtStatus::Success;
        }

        // Drain any held pending writes down through the COW filter to base volume
        while (!m_heldWriteQueue.empty()) {
            auto pending = std::move(m_heldWriteQueue.front());
            m_heldWriteQueue.pop_front();

            // Process COW for all active committed snapshots before writing
            performCopyOnWriteInternal(pending.targetLcn);
            m_baseVolume->writeClusterDirect(pending.targetLcn, pending.buffer);
        }

        m_writesHeld = false;

        for (auto& snap : m_activeSnapshots) {
            if (snap->getState() == VssSnapshotState::Committed) {
                snap->setState(VssSnapshotState::Mounted);
            }
        }

        return NtStatus::Success;
    }

    // 5. Intercepted Live Base Volume Write with COW Trapping
    NtStatus writeBaseCluster(uint64_t lcn, std::span<const uint8_t> data) {
        std::lock_guard<std::mutex> lock(m_engineMutex);

        if (m_writesHeld) {
            // Queue write IRP while volume is frozen
            PendingWriteIrp irp;
            irp.targetLcn = lcn;
            irp.buffer.assign(data.begin(), data.end());
            irp.sequenceNumber = ++m_writeSeqCounter;
            m_heldWriteQueue.push_back(std::move(irp));
            return NtStatus::Pending;
        }

        // Pre-write Copy-On-Write check across all active snapshots
        performCopyOnWriteInternal(lcn);

        // Perform write to base volume
        m_baseVolume->writeClusterDirect(lcn, data);
        return NtStatus::Success;
    }

    // 6. Read from Snapshot Virtual Device (\Device\HarddiskVolumeShadowCopy{N})
    NtStatus readSnapshotCluster(uint32_t snapshotIndex, uint64_t lcn, std::vector<uint8_t>& outBuffer) const {
        std::lock_guard<std::mutex> lock(m_engineMutex);

        auto it = std::find_if(m_activeSnapshots.begin(), m_activeSnapshots.end(),
            [&](const std::shared_ptr<VolSnapDevice>& s) { return s->getSnapshotIndex() == snapshotIndex; });

        if (it == m_activeSnapshots.end()) {
            return NtStatus::NoSuchFile;
        }

        const auto& snap = *it;
        if (snap->getState() != VssSnapshotState::Committed && snap->getState() != VssSnapshotState::Mounted) {
            return NtStatus::InvalidDeviceState;
        }

        // If block was modified since snapshot, read from DiffArea
        if (snap->readClusterFromSnapshot(lcn, outBuffer)) {
            return NtStatus::Success;
        }

        // Otherwise pass through to base volume unmodified block
        outBuffer = m_baseVolume->readClusterDirect(lcn);
        return NtStatus::Success;
    }

    // 7. Volume Rollback / Revert to Snapshot
    NtStatus revertToSnapshot(uint32_t snapshotIndex) {
        std::lock_guard<std::mutex> lock(m_engineMutex);

        auto it = std::find_if(m_activeSnapshots.begin(), m_activeSnapshots.end(),
            [&](const std::shared_ptr<VolSnapDevice>& s) { return s->getSnapshotIndex() == snapshotIndex; });

        if (it == m_activeSnapshots.end()) {
            return NtStatus::NoSuchFile;
        }

        auto& snap = *it;
        snap->setState(VssSnapshotState::Reverting);

        // Replay all pre-mutation DiffArea blocks back onto the base volume
        std::vector<DiffBlock> blocks;
        snap->getDiffBlocks(blocks);

        for (const auto& blk : blocks) {
            m_baseVolume->writeClusterDirect(blk.originalLcn, blk.data);
        }

        snap->setState(VssSnapshotState::Mounted);
        return NtStatus::Success;
    }

    // 8. Delete / Unmount Snapshot
    NtStatus deleteSnapshot(uint32_t snapshotIndex) {
        std::lock_guard<std::mutex> lock(m_engineMutex);

        auto it = std::find_if(m_activeSnapshots.begin(), m_activeSnapshots.end(),
            [&](const std::shared_ptr<VolSnapDevice>& s) { return s->getSnapshotIndex() == snapshotIndex; });

        if (it == m_activeSnapshots.end()) {
            return NtStatus::NoSuchFile;
        }

        (*it)->setState(VssSnapshotState::Deleted);
        m_activeSnapshots.erase(it);
        return NtStatus::Success;
    }

    [[nodiscard]] bool areWritesHeld() const noexcept { return m_writesHeld; }
    [[nodiscard]] size_t getHeldWriteCount() const noexcept { return m_heldWriteQueue.size(); }
    [[nodiscard]] size_t getActiveSnapshotCount() const noexcept { return m_activeSnapshots.size(); }

private:
    void performCopyOnWriteInternal(uint64_t lcn) {
        auto now = std::chrono::high_resolution_clock::now().time_since_epoch();
        uint64_t tsNs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());

        for (auto& snap : m_activeSnapshots) {
            if (snap->getState() == VssSnapshotState::Committed || snap->getState() == VssSnapshotState::Mounted) {
                if (!snap->hasOriginalCluster(lcn)) {
                    // Read original baseline from base volume before mutation
                    std::vector<uint8_t> originalData = m_baseVolume->readClusterDirect(lcn);
                    snap->recordDiffBlock(lcn, originalData, tsNs);
                }
            }
        }
    }

    std::shared_ptr<MockBaseVolume> m_baseVolume;
    std::wstring m_baseVolumePath;
    mutable std::mutex m_engineMutex;
    uint32_t m_snapshotCounter{0};
    uint64_t m_writeSeqCounter{0};
    bool m_writesHeld{false};
    std::vector<std::shared_ptr<VolSnapDevice>> m_activeSnapshots;
    std::deque<PendingWriteIrp> m_heldWriteQueue;
};

// ============================================================================
// 7. Subsystem Verification Harness (Suite 579 / Horizon 363)
// ============================================================================

bool VerifyHorizon363VolumeShadowCopySubsystem();

} // namespace micant::ntfs::vss
