// ============================================================================
// MicaNT: Sovereign Clean-Room Primary NTFS Clustered Shared Volumes (CSV)
// Horizon 370 / Suite 586 - \Device\HarddiskVolumeCsv, Distributed Lock
// Manager (DLM), Oplocks (RWH Leases), Direct/Redirected I/O & Failover
//
// Clean-Room Implementation in modern freestanding ISO C++23.
// Compatible with Dave Cutler's Windows NT architecture, win32metadata,
// and IFS CSVFS driver architecture. Zero external dependencies.
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

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/km/drivers/fs/ntfs_primary_storage.hpp"

namespace micant::ntfs::csv {

// ============================================================================
// 1. Constants & Enums for Clustered Shared Volumes & DLM
// ============================================================================

inline constexpr uint32_t CSV_DEFAULT_CLUSTER_PORT = 3343;
inline constexpr uint32_t CSV_VOLUME_MAX_NODES     = 64;

// Oplock Lease Levels (Windows 7+ RWH Lease Model)
inline constexpr uint32_t OPLOCK_LEVEL_NONE         = 0x00;
inline constexpr uint32_t OPLOCK_LEVEL_CACHE_READ   = 0x01; // 'R'
inline constexpr uint32_t OPLOCK_LEVEL_CACHE_HANDLE = 0x02; // 'H'
inline constexpr uint32_t OPLOCK_LEVEL_CACHE_WRITE  = 0x04; // 'W'
inline constexpr uint32_t OPLOCK_LEVEL_RWH          = (OPLOCK_LEVEL_CACHE_READ | OPLOCK_LEVEL_CACHE_HANDLE | OPLOCK_LEVEL_CACHE_WRITE);
inline constexpr uint32_t OPLOCK_LEVEL_RH           = (OPLOCK_LEVEL_CACHE_READ | OPLOCK_LEVEL_CACHE_HANDLE);

// Distributed Lock Manager (DLM) Modes (VAX / NT DLM standard)
inline constexpr uint32_t DLM_LOCK_NL = 0; // Null
inline constexpr uint32_t DLM_LOCK_CR = 1; // Concurrent Read
inline constexpr uint32_t DLM_LOCK_CW = 2; // Concurrent Write
inline constexpr uint32_t DLM_LOCK_PR = 3; // Protected Read
inline constexpr uint32_t DLM_LOCK_PW = 4; // Protected Write
inline constexpr uint32_t DLM_LOCK_EX = 5; // Exclusive

/**
 * @brief DLM Compatibility Matrix (Standard VAX / Windows NT DLM specification)
 * Row: Current Granted Mode, Col: Requested Mode
 */
inline bool IsDlmModeCompatible(uint32_t granted, uint32_t requested) noexcept {
    if (granted > DLM_LOCK_EX || requested > DLM_LOCK_EX) return false;
    if (granted == DLM_LOCK_NL || requested == DLM_LOCK_NL) return true;

    // Compatibility matrix:
    //      NL  CR  CW  PR  PW  EX
    //  NL   T   T   T   T   T   T
    //  CR   T   T   T   T   T   F
    //  CW   T   T   T   F   F   F
    //  PR   T   T   F   T   F   F
    //  PW   T   T   F   F   F   F
    //  EX   T   F   F   F   F   F
    static constexpr bool kMatrix[6][6] = {
        { true,  true,  true,  true,  true,  true  }, // NL
        { true,  true,  true,  true,  true,  false }, // CR
        { true,  true,  true,  false, false, false }, // CW
        { true,  true,  false, true,  false, false }, // PR
        { true,  true,  false, false, false, false }, // PW
        { true,  false, false, false, false, false }  // EX
    };
    return kMatrix[granted][requested];
}

enum class NodeConnectionState {
    OnlineDirect,     // Direct SAN/NVMe-oF I/O path to underlying clusters
    OnlineRedirected, // Storage path degraded; I/O redirected to coordinator over cluster network
    Paused,
    Offline
};

// ============================================================================
// 2. Data Structures: DLM Requests, Oplock Leases, and Cluster Nodes
// ============================================================================

struct DlmLockRequest {
    uint32_t     nodeId{0};
    std::wstring resourceName;
    uint32_t     mode{DLM_LOCK_NL};
    bool         isGranted{false};
    uint64_t     timestampUs{0};
};

struct DlmResourceLock {
    std::wstring resourceName;
    uint32_t     currentMode{DLM_LOCK_NL};
    std::vector<DlmLockRequest> grantedHolders;
    std::vector<DlmLockRequest> waitingQueue;
};

struct CsvOplockLease {
    uint64_t fileReferenceNumber{0};
    uint32_t holdingNodeId{0};
    uint32_t grantedLeaseLevel{OPLOCK_LEVEL_NONE};
    uint32_t breakRequestedLevel{OPLOCK_LEVEL_NONE};
    bool     isBreaking{false};
};

struct CsvClusterNode {
    uint32_t            nodeId{0};
    std::wstring        nodeName;
    bool                isCoordinator{false};
    NodeConnectionState connectionState{NodeConnectionState::OnlineDirect};
    uint64_t            directIoCount{0};
    uint64_t            redirectedIoCount{0};
    uint64_t            cachedEpoch{1};
};

struct CsvAuditRecord {
    std::wstring description;
    uint32_t     nodeId{0};
    uint64_t     timestampUs{0};
};

// ============================================================================
// 3. Distributed Lock Manager (DLM) Class
// ============================================================================

class DistributedLockManager {
public:
    NtStatus acquireLock(uint32_t nodeId, std::wstring_view resourceName, uint32_t mode, bool wait = true) noexcept;
    NtStatus releaseLock(uint32_t nodeId, std::wstring_view resourceName) noexcept;
    NtStatus convertLock(uint32_t nodeId, std::wstring_view resourceName, uint32_t newMode) noexcept;
    [[nodiscard]] uint32_t getLockMode(std::wstring_view resourceName) const noexcept;
    [[nodiscard]] size_t getActiveHolderCount(std::wstring_view resourceName) const noexcept;
    [[nodiscard]] size_t getWaitingCount(std::wstring_view resourceName) const noexcept;
    void clear() noexcept;

private:
    mutable std::recursive_mutex m_mutex;
    std::unordered_map<std::wstring, DlmResourceLock> m_locks;
};

// ============================================================================
// 4. CSV Oplock & Cache Coherency Manager
// ============================================================================

class CsvOplockManager {
public:
    NtStatus requestLease(uint32_t nodeId, uint64_t fileRef, uint32_t requestedLevel, uint32_t& grantedLevel) noexcept;
    bool checkOplockBreak(uint64_t fileRef, uint32_t requestingNodeId, uint32_t desiredAccess,
                          bool& outBreakNeeded, uint32_t& outBreakTargetNode, uint32_t& outNewLevel) noexcept;
    NtStatus acknowledgeBreak(uint32_t nodeId, uint64_t fileRef, uint32_t acknowledgedLevel) noexcept;
    [[nodiscard]] uint32_t getLeaseLevel(uint64_t fileRef) const noexcept;
    [[nodiscard]] bool isOplockBreaking(uint64_t fileRef) const noexcept;
    void clear() noexcept;

private:
    mutable std::recursive_mutex m_mutex;
    std::unordered_map<uint64_t, CsvOplockLease> m_leases;
};

// ============================================================================
// 5. Sovereign Clustered Shared Volumes Engine (SovereignCsvEngine)
// ============================================================================

class SovereignCsvEngine {
public:
    static SovereignCsvEngine& Instance() noexcept {
        static SovereignCsvEngine s_instance;
        return s_instance;
    }

    void initializeVolume(std::wstring_view volumePath = L"\\Device\\HarddiskVolumeCsv") noexcept;
    void resetVolume() noexcept;

    // Cluster Node Registration & State
    NtStatus registerNode(uint32_t nodeId, std::wstring_view nodeName, bool isCoordinator = false) noexcept;
    NtStatus unregisterNode(uint32_t nodeId) noexcept;
    NtStatus setNodeConnectionState(uint32_t nodeId, NodeConnectionState state) noexcept;
    [[nodiscard]] NodeConnectionState getNodeConnectionState(uint32_t nodeId) const noexcept;
    [[nodiscard]] uint32_t getCoordinatorNodeId() const noexcept;

    // Coordinator Failover Protocol
    NtStatus failoverCoordinator(uint32_t newCoordinatorNodeId) noexcept;

    // Clustered I/O Operations (Direct vs. Redirected)
    NtStatus createClusterFile(uint32_t nodeId, std::wstring_view fileName, uint64_t& outFileRef) noexcept;
    NtStatus readClusterFile(uint32_t nodeId, uint64_t fileRef, uint64_t offset, std::span<uint8_t> buffer, size_t& bytesRead) noexcept;
    NtStatus writeClusterFile(uint32_t nodeId, uint64_t fileRef, uint64_t offset, std::span<const uint8_t> buffer, size_t& bytesWritten) noexcept;

    // Subsystem Components
    [[nodiscard]] DistributedLockManager& getDlm() noexcept { return m_dlm; }
    [[nodiscard]] CsvOplockManager& getOplockMgr() noexcept { return m_oplockMgr; }

    // Audit Logging
    void recordAudit(uint32_t nodeId, std::wstring_view description) noexcept;
    [[nodiscard]] std::vector<CsvAuditRecord> getAuditHistory() const noexcept;
    void clearAuditHistory() noexcept;

    // Node Telemetry
    [[nodiscard]] uint64_t getNodeDirectIoCount(uint32_t nodeId) const noexcept;
    [[nodiscard]] uint64_t getNodeRedirectedIoCount(uint32_t nodeId) const noexcept;

private:
    SovereignCsvEngine() = default;
    mutable std::recursive_mutex m_mutex;
    bool m_initialized{false};
    std::wstring m_volumePath{L"\\Device\\HarddiskVolumeCsv"};
    uint32_t m_coordinatorNodeId{1};
    uint64_t m_clusterEpoch{1};
    uint64_t m_nextFileRef{1000};

    DistributedLockManager m_dlm;
    CsvOplockManager m_oplockMgr;
    std::unordered_map<uint32_t, CsvClusterNode> m_nodes;
    std::unordered_map<uint64_t, std::vector<uint8_t>> m_fileBuffers;
    std::unordered_map<uint64_t, std::wstring> m_fileNames;
    std::vector<CsvAuditRecord> m_auditLog;
};

// ============================================================================
// 6. Verification Suite (Suite 586)
// ============================================================================

bool VerifyHorizon370NtfsCsvSubsystem() noexcept;

} // namespace micant::ntfs::csv
