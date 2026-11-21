// ============================================================================
// MicaNT: Sovereign Clean-Room Primary NTFS Storage Subsystem
// Horizon 362 / Suite 578 - Transactional NTFS (TxF), Kernel Transaction Manager
// (KTM) Integration, 2-Phase Commit (2PC) Protocol & Copy-On-Write Mini-Versions
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
#include "micant/km/executive/ktm.hpp"
#include "micant/km/drivers/fs/ntfs_primary_storage.hpp"

namespace micant::ntfs::txf {

// ============================================================================
// 1. TxF GUIDs & Resource Manager Identifiers
// ============================================================================

inline constexpr km::ktm::KTM_GUID TXF_RESOURCE_MANAGER_GUID = {
    0x54584631, 0x524D, 0x4B54, { 0x4D, 0x54, 0x78, 0x46, 0x5F, 0x53, 0x4F, 0x56 }
};

inline constexpr uint32_t TXF_MINIVERSION_COMMITTED_VIEW = 0x0000;
inline constexpr uint32_t TXF_MINIVERSION_DIRTY_VIEW     = 0xFFFF;

// ============================================================================
// 2. TxF Mini-Version (Copy-On-Write Isolated Buffer)
// ============================================================================

struct TxfMiniVersion {
    uint16_t miniVersionId{1};
    uint64_t transactionUow{0};
    uint64_t realSize{0};
    uint64_t allocatedSize{0};
    std::vector<uint8_t> data;
    std::vector<uint64_t> allocatedClusters; // Clusters allocated during transaction
    bool isMarkedForDeletion{false};
    bool isNewFile{false};
};

// ============================================================================
// 3. TxF Inode File Node
// ============================================================================

class TxfFileNode {
public:
    TxfFileNode(uint64_t fileRef, std::wstring_view path)
        : m_fileReference(fileRef), m_filePath(path) {}

    [[nodiscard]] uint64_t getFileReference() const noexcept {
        return m_fileReference;
    }

    [[nodiscard]] const std::wstring& getFilePath() const noexcept {
        return m_filePath;
    }

    // Set or update committed baseline (outside any transaction)
    void setCommittedData(std::span<const uint8_t> data, const std::vector<uint64_t>& clusters) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_committedData.assign(data.begin(), data.end());
        m_committedClusters = clusters;
        m_committedRealSize = data.size();
        m_committedAllocatedSize = (data.size() + 511) & ~511ULL;
        m_existsOnDisk = true;
    }

    [[nodiscard]] bool existsOnDisk() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_existsOnDisk;
    }

    // Transacted Read: isolated snapshot
    NtStatus readTransacted(uint64_t txUow, std::vector<uint8_t>& outData) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (txUow != 0) {
            auto it = m_miniVersions.find(txUow);
            if (it != m_miniVersions.end()) {
                if (it->second.isMarkedForDeletion) {
                    return NtStatus::NoSuchFile;
                }
                outData = it->second.data;
                return NtStatus::Success;
            }
        }
        // Non-transacted reader or transaction with no local write: read committed
        if (!m_existsOnDisk) {
            return NtStatus::NoSuchFile;
        }
        outData = m_committedData;
        return NtStatus::Success;
    }

    // Transacted Write: writes exclusively into isolated COW mini-version
    NtStatus writeTransacted(
        uint64_t txUow,
        std::span<const uint8_t> data,
        const std::vector<uint64_t>& newClusters) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& mv = m_miniVersions[txUow];
        mv.transactionUow = txUow;
        mv.data.assign(data.begin(), data.end());
        mv.realSize = data.size();
        mv.allocatedSize = (data.size() + 511) & ~511ULL;
        mv.allocatedClusters = newClusters;
        mv.isMarkedForDeletion = false;
        if (!m_existsOnDisk) {
            mv.isNewFile = true;
        }
        return NtStatus::Success;
    }

    // Transacted Delete: marks file deleted inside transaction
    NtStatus deleteTransacted(uint64_t txUow) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& mv = m_miniVersions[txUow];
        mv.transactionUow = txUow;
        mv.isMarkedForDeletion = true;
        return NtStatus::Success;
    }

    // Commit mini-version
    bool commitMiniVersion(uint64_t txUow, std::vector<uint64_t>& outClustersToFree) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_miniVersions.find(txUow);
        if (it == m_miniVersions.end()) return false;

        const auto& mv = it->second;
        if (mv.isMarkedForDeletion) {
            // Free previously committed clusters
            outClustersToFree = m_committedClusters;
            m_committedData.clear();
            m_committedClusters.clear();
            m_committedRealSize = 0;
            m_committedAllocatedSize = 0;
            m_existsOnDisk = false;
        } else {
            // Free old clusters replaced by new ones
            if (!mv.allocatedClusters.empty()) {
                outClustersToFree = m_committedClusters;
                m_committedClusters = mv.allocatedClusters;
            }
            m_committedData = mv.data;
            m_committedRealSize = mv.realSize;
            m_committedAllocatedSize = mv.allocatedSize;
            m_existsOnDisk = true;
        }

        m_miniVersions.erase(it);
        return true;
    }

    // Rollback mini-version (discards dirty changes, returns newly allocated clusters to be freed)
    bool rollbackMiniVersion(uint64_t txUow, std::vector<uint64_t>& outClustersToFree) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_miniVersions.find(txUow);
        if (it == m_miniVersions.end()) return false;

        outClustersToFree = it->second.allocatedClusters;
        m_miniVersions.erase(it);
        return true;
    }

    [[nodiscard]] bool hasActiveMiniVersion(uint64_t txUow) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_miniVersions.contains(txUow);
    }

private:
    uint64_t m_fileReference{0};
    std::wstring m_filePath;
    mutable std::mutex m_mutex;

    // Committed Baseline State
    bool m_existsOnDisk{false};
    uint64_t m_committedRealSize{0};
    uint64_t m_committedAllocatedSize{0};
    std::vector<uint8_t> m_committedData;
    std::vector<uint64_t> m_committedClusters;

    // Active Mini-Versions (isolated per transaction UOW)
    std::unordered_map<uint64_t, TxfMiniVersion> m_miniVersions;
};

// ============================================================================
// 4. Sovereign TxF Resource Manager Engine
// ============================================================================

enum class TxfTransactionState : uint32_t {
    Active = 1,
    Preparing = 2,
    Prepared = 3,
    Committed = 4,
    Aborted = 5
};

struct TxfTransactionContext {
    uint64_t transactionUow{0};
    TxfTransactionState state{TxfTransactionState::Active};
    std::vector<std::wstring> enrolledFiles;
    std::vector<uint64_t> pendingClusters;
};

class SovereignTxfResourceManager {
public:
    static SovereignTxfResourceManager& Instance() noexcept {
        static SovereignTxfResourceManager s_instance;
        return s_instance;
    }

    // ------------------------------------------------------------------------
    // Transaction Lifecycle & Enlistment
    // ------------------------------------------------------------------------
    NtStatus enlistTransaction(uint64_t txUow) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (txUow == 0) return NtStatus::InvalidParameter;

        if (m_transactions.contains(txUow)) {
            return NtStatus::Success; // Already enlisted
        }

        m_transactions[txUow] = TxfTransactionContext{
            .transactionUow = txUow,
            .state = TxfTransactionState::Active,
            .enrolledFiles = {},
            .pendingClusters = {}
        };
        m_totalTransactionsEnlisted++;
        return NtStatus::Success;
    }

    // ------------------------------------------------------------------------
    // Transacted File Operations
    // ------------------------------------------------------------------------
    NtStatus createFileTransacted(
        uint64_t txUow,
        std::wstring_view filePath,
        std::shared_ptr<TxfFileNode>& outFileNode) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto txIt = m_transactions.find(txUow);
        if (txIt == m_transactions.end()) return NtStatus::InvalidParameter;

        std::wstring key(filePath);
        auto fileIt = m_fileNodes.find(key);
        if (fileIt == m_fileNodes.end()) {
            uint64_t newRef = ++m_nextFileReference;
            auto node = std::make_shared<TxfFileNode>(newRef, filePath);
            m_fileNodes[key] = node;
            outFileNode = node;
        } else {
            outFileNode = fileIt->second;
        }

        if (std::find(txIt->second.enrolledFiles.begin(), txIt->second.enrolledFiles.end(), key) == txIt->second.enrolledFiles.end()) {
            txIt->second.enrolledFiles.push_back(key);
        }
        return NtStatus::Success;
    }

    NtStatus writeFileTransacted(
        uint64_t txUow,
        std::wstring_view filePath,
        std::span<const uint8_t> data,
        primary::NtfsVolumeBitmap& bitmap) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto txIt = m_transactions.find(txUow);
        if (txIt == m_transactions.end() || txIt->second.state != TxfTransactionState::Active) {
            return NtStatus::InvalidParameter;
        }

        std::wstring key(filePath);
        auto fileIt = m_fileNodes.find(key);
        if (fileIt == m_fileNodes.end()) {
            return NtStatus::NoSuchFile;
        }

        // Allocate clusters from primary NTFS volume bitmap
        uint64_t clusterCount = (data.size() + 4095) / 4096;
        if (clusterCount == 0) clusterCount = 1;

        uint64_t startClust = 0;
        if (!bitmap.allocateContiguousClusters(clusterCount, startClust)) {
            return NtStatus::DiskFull;
        }

        std::vector<uint64_t> clusters;
        clusters.reserve(clusterCount);
        for (uint64_t c = startClust; c < startClust + clusterCount; ++c) {
            clusters.push_back(c);
            txIt->second.pendingClusters.push_back(c);
        }

        fileIt->second->writeTransacted(txUow, data, clusters);
        if (std::find(txIt->second.enrolledFiles.begin(), txIt->second.enrolledFiles.end(), key) == txIt->second.enrolledFiles.end()) {
            txIt->second.enrolledFiles.push_back(key);
        }
        return NtStatus::Success;
    }

    NtStatus readFileTransacted(
        uint64_t txUow,
        std::wstring_view filePath,
        std::vector<uint8_t>& outData) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::wstring key(filePath);
        auto it = m_fileNodes.find(key);
        if (it == m_fileNodes.end()) {
            return NtStatus::NoSuchFile;
        }
        return it->second->readTransacted(txUow, outData);
    }

    NtStatus deleteFileTransacted(uint64_t txUow, std::wstring_view filePath) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto txIt = m_transactions.find(txUow);
        if (txIt == m_transactions.end() || txIt->second.state != TxfTransactionState::Active) {
            return NtStatus::InvalidParameter;
        }

        std::wstring key(filePath);
        auto fileIt = m_fileNodes.find(key);
        if (fileIt == m_fileNodes.end()) {
            return NtStatus::NoSuchFile;
        }

        fileIt->second->deleteTransacted(txUow);
        if (std::find(txIt->second.enrolledFiles.begin(), txIt->second.enrolledFiles.end(), key) == txIt->second.enrolledFiles.end()) {
            txIt->second.enrolledFiles.push_back(key);
        }
        return NtStatus::Success;
    }

    // ------------------------------------------------------------------------
    // 2-Phase Commit (2PC): Phase 1 (Prepare)
    // ------------------------------------------------------------------------
    NtStatus prepareTransaction(uint64_t txUow) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_transactions.find(txUow);
        if (it == m_transactions.end() || it->second.state != TxfTransactionState::Active) {
            return NtStatus::InvalidParameter;
        }

        it->second.state = TxfTransactionState::Prepared;
        m_totalPrepared++;
        return NtStatus::Success;
    }

    // ------------------------------------------------------------------------
    // 2-Phase Commit (2PC): Phase 2 (Commit)
    // ------------------------------------------------------------------------
    NtStatus commitTransaction(
        uint64_t txUow,
        primary::NtfsVolumeBitmap& bitmap,
        primary::BTreeDirectoryIndex* dirIndex = nullptr) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto txIt = m_transactions.find(txUow);
        if (txIt == m_transactions.end()) {
            return NtStatus::InvalidParameter;
        }

        // Commit all enlisted files
        for (const auto& path : txIt->second.enrolledFiles) {
            auto fileIt = m_fileNodes.find(path);
            if (fileIt != m_fileNodes.end()) {
                std::vector<uint64_t> oldClustersToFree;
                fileIt->second->commitMiniVersion(txUow, oldClustersToFree);

                for (uint64_t c : oldClustersToFree) {
                    bitmap.freeClusters(c, 1);
                }

                // If B-Tree index provided, update directory entry
                if (dirIndex) {
                    if (fileIt->second->existsOnDisk()) {
                        primary::BTreeIndexNodeEntry entry{
                            .fileReferenceNumber = fileIt->second->getFileReference(),
                            .allocatedSize = 4096,
                            .realSize = 1024,
                            .fileAttributes = primary::FILE_ATTRIBUTE_NORMAL,
                            .fileName = path
                        };
                        dirIndex->insertEntry(entry);
                    } else {
                        dirIndex->removeEntry(path);
                    }
                }
            }
        }

        txIt->second.state = TxfTransactionState::Committed;
        m_totalCommitted++;
        m_transactions.erase(txIt);
        return NtStatus::Success;
    }

    // ------------------------------------------------------------------------
    // 2-Phase Commit (2PC): Phase 2 (Rollback)
    // ------------------------------------------------------------------------
    NtStatus rollbackTransaction(
        uint64_t txUow,
        primary::NtfsVolumeBitmap& bitmap) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto txIt = m_transactions.find(txUow);
        if (txIt == m_transactions.end()) {
            return NtStatus::InvalidParameter;
        }

        // Rollback all enlisted files and free pending clusters
        for (const auto& path : txIt->second.enrolledFiles) {
            auto fileIt = m_fileNodes.find(path);
            if (fileIt != m_fileNodes.end()) {
                std::vector<uint64_t> clustersToFree;
                fileIt->second->rollbackMiniVersion(txUow, clustersToFree);
                for (uint64_t c : clustersToFree) {
                    bitmap.freeClusters(c, 1);
                }
            }
        }

        txIt->second.state = TxfTransactionState::Aborted;
        m_totalAborted++;
        m_transactions.erase(txIt);
        return NtStatus::Success;
    }

    [[nodiscard]] size_t getActiveTransactionCount() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_transactions.size();
    }

    [[nodiscard]] uint64_t getTotalCommitted() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_totalCommitted;
    }

    [[nodiscard]] uint64_t getTotalAborted() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_totalAborted;
    }

private:
    SovereignTxfResourceManager() = default;
    mutable std::mutex m_mutex;
    uint64_t m_nextFileReference{200};
    uint64_t m_totalTransactionsEnlisted{0};
    uint64_t m_totalPrepared{0};
    uint64_t m_totalCommitted{0};
    uint64_t m_totalAborted{0};

    std::unordered_map<std::wstring, std::shared_ptr<TxfFileNode>> m_fileNodes;
    std::unordered_map<uint64_t, TxfTransactionContext> m_transactions;
};

// Subsystem Verification Harness
bool VerifyHorizon362TransactionalNtfsSubsystem() noexcept;

} // namespace micant::ntfs::txf
