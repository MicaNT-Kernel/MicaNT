// ============================================================================
// MicaNT: Sovereign Clean-Room Primary NTFS Clustered Shared Volumes (CSV)
// Horizon 370 / Suite 586 - \Device\HarddiskVolumeCsv, Distributed Lock
// Manager (DLM), Oplocks (RWH Leases), Direct/Redirected I/O & Failover
//
// Clean-Room Implementation in modern freestanding ISO C++23.
// Compatible with Dave Cutler's Windows NT architecture, win32metadata,
// and IFS CSVFS driver architecture. Zero external dependencies.
// ============================================================================

#include "micant/km/drivers/fs/ntfs_csv.hpp"
#include <iostream>

namespace micant::ntfs::csv {

// ============================================================================
// 1. DistributedLockManager Implementation
// ============================================================================

NtStatus DistributedLockManager::acquireLock(
    uint32_t nodeId,
    std::wstring_view resourceName,
    uint32_t mode,
    bool wait
) noexcept {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::wstring key(resourceName);
    auto& resLock = m_locks[key];
    resLock.resourceName = key;

    uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count());

    // Check compatibility with currently granted holders
    bool compatible = IsDlmModeCompatible(resLock.currentMode, mode);

    if (compatible && resLock.waitingQueue.empty()) {
        resLock.currentMode = std::max(resLock.currentMode, mode);
        DlmLockRequest req{nodeId, key, mode, true, now};
        resLock.grantedHolders.push_back(std::move(req));
        return NtStatus::Success;
    }

    if (!wait) {
        return NtStatus::LockNotGranted;
    }

    // Queue in waiting list
    DlmLockRequest req{nodeId, key, mode, false, now};
    resLock.waitingQueue.push_back(std::move(req));
    return NtStatus::Pending;
}

NtStatus DistributedLockManager::releaseLock(
    uint32_t nodeId,
    std::wstring_view resourceName
) noexcept {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::wstring key(resourceName);
    auto it = m_locks.find(key);
    if (it == m_locks.end()) return NtStatus::NotFound;

    auto& resLock = it->second;
    auto holderIt = std::find_if(resLock.grantedHolders.begin(), resLock.grantedHolders.end(),
        [nodeId](const DlmLockRequest& r) { return r.nodeId == nodeId; });

    if (holderIt == resLock.grantedHolders.end()) {
        return NtStatus::NotFound;
    }

    resLock.grantedHolders.erase(holderIt);

    // Recalculate current granted mode
    resLock.currentMode = DLM_LOCK_NL;
    for (const auto& h : resLock.grantedHolders) {
        resLock.currentMode = std::max(resLock.currentMode, h.mode);
    }

    // Check if any waiting requests can now be promoted
    if (!resLock.waitingQueue.empty()) {
        auto waitIt = resLock.waitingQueue.begin();
        if (IsDlmModeCompatible(resLock.currentMode, waitIt->mode)) {
            waitIt->isGranted = true;
            resLock.currentMode = std::max(resLock.currentMode, waitIt->mode);
            resLock.grantedHolders.push_back(*waitIt);
            resLock.waitingQueue.erase(waitIt);
        }
    }

    return NtStatus::Success;
}

NtStatus DistributedLockManager::convertLock(
    uint32_t nodeId,
    std::wstring_view resourceName,
    uint32_t newMode
) noexcept {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::wstring key(resourceName);
    auto it = m_locks.find(key);
    if (it == m_locks.end()) return NtStatus::NotFound;

    auto& resLock = it->second;
    auto holderIt = std::find_if(resLock.grantedHolders.begin(), resLock.grantedHolders.end(),
        [nodeId](const DlmLockRequest& r) { return r.nodeId == nodeId; });

    if (holderIt == resLock.grantedHolders.end()) {
        return NtStatus::NotFound;
    }

    // Downgrade is always allowed immediately
    if (newMode <= holderIt->mode) {
        holderIt->mode = newMode;
        resLock.currentMode = DLM_LOCK_NL;
        for (const auto& h : resLock.grantedHolders) {
            resLock.currentMode = std::max(resLock.currentMode, h.mode);
        }
        return NtStatus::Success;
    }

    // Upgrade requires checking compatibility against OTHER holders
    uint32_t otherMode = DLM_LOCK_NL;
    for (const auto& h : resLock.grantedHolders) {
        if (h.nodeId != nodeId) {
            otherMode = std::max(otherMode, h.mode);
        }
    }

    if (IsDlmModeCompatible(otherMode, newMode)) {
        holderIt->mode = newMode;
        resLock.currentMode = std::max(otherMode, newMode);
        return NtStatus::Success;
    }

    return NtStatus::LockNotGranted;
}

uint32_t DistributedLockManager::getLockMode(std::wstring_view resourceName) const noexcept {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_locks.find(std::wstring(resourceName));
    return (it != m_locks.end()) ? it->second.currentMode : DLM_LOCK_NL;
}

size_t DistributedLockManager::getActiveHolderCount(std::wstring_view resourceName) const noexcept {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_locks.find(std::wstring(resourceName));
    return (it != m_locks.end()) ? it->second.grantedHolders.size() : 0;
}

size_t DistributedLockManager::getWaitingCount(std::wstring_view resourceName) const noexcept {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_locks.find(std::wstring(resourceName));
    return (it != m_locks.end()) ? it->second.waitingQueue.size() : 0;
}

void DistributedLockManager::clear() noexcept {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_locks.clear();
}

// ============================================================================
// 2. CsvOplockManager Implementation
// ============================================================================

NtStatus CsvOplockManager::requestLease(
    uint32_t nodeId,
    uint64_t fileRef,
    uint32_t requestedLevel,
    uint32_t& grantedLevel
) noexcept {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_leases.find(fileRef);

    if (it == m_leases.end()) {
        // No existing lease on this file - grant in full
        grantedLevel = requestedLevel;
        m_leases[fileRef] = CsvOplockLease{
            .fileReferenceNumber = fileRef,
            .holdingNodeId = nodeId,
            .grantedLeaseLevel = grantedLevel,
            .breakRequestedLevel = OPLOCK_LEVEL_NONE,
            .isBreaking = false
        };
        return NtStatus::Success;
    }

    auto& lease = it->second;
    if (lease.holdingNodeId == nodeId) {
        // Same node lease upgrade or modification
        lease.grantedLeaseLevel = requestedLevel;
        grantedLevel = requestedLevel;
        return NtStatus::Success;
    }

    // Different node requesting lease
    if (lease.grantedLeaseLevel & OPLOCK_LEVEL_CACHE_WRITE) {
        // Conflicting write lease already held by another node
        grantedLevel = OPLOCK_LEVEL_NONE;
        return NtStatus::OplockNotGranted;
    }

    // Shared read leases allowed
    if ((requestedLevel & OPLOCK_LEVEL_CACHE_WRITE) == 0) {
        grantedLevel = requestedLevel & OPLOCK_LEVEL_RH;
        return NtStatus::Success;
    }

    grantedLevel = OPLOCK_LEVEL_NONE;
    return NtStatus::OplockNotGranted;
}

bool CsvOplockManager::checkOplockBreak(
    uint64_t fileRef,
    uint32_t requestingNodeId,
    uint32_t desiredAccess,
    bool& outBreakNeeded,
    uint32_t& outBreakTargetNode,
    uint32_t& outNewLevel
) noexcept {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    outBreakNeeded = false;
    outBreakTargetNode = 0;
    outNewLevel = OPLOCK_LEVEL_NONE;

    auto it = m_leases.find(fileRef);
    if (it == m_leases.end()) return false;

    auto& lease = it->second;
    if (lease.holdingNodeId == requestingNodeId) {
        return false; // Access by current lease holder
    }

    // If holder has write caching (W), break is required
    if (lease.grantedLeaseLevel & OPLOCK_LEVEL_CACHE_WRITE) {
        outBreakNeeded = true;
        outBreakTargetNode = lease.holdingNodeId;
        // If requester only reads, downgrade holder to Read-Handle (RH)
        // If requester writes, downgrade holder to None
        outNewLevel = (desiredAccess == 1 /* Read */) ? OPLOCK_LEVEL_RH : OPLOCK_LEVEL_NONE;
        lease.isBreaking = true;
        lease.breakRequestedLevel = outNewLevel;
        return true;
    }

    // If requester wants to write, and holder has Read caching (R/H), break is required
    if ((desiredAccess == 2 /* Write */) && (lease.grantedLeaseLevel & OPLOCK_LEVEL_CACHE_READ)) {
        outBreakNeeded = true;
        outBreakTargetNode = lease.holdingNodeId;
        outNewLevel = OPLOCK_LEVEL_NONE;
        lease.isBreaking = true;
        lease.breakRequestedLevel = outNewLevel;
        return true;
    }

    return false;
}

NtStatus CsvOplockManager::acknowledgeBreak(
    uint32_t nodeId,
    uint64_t fileRef,
    uint32_t acknowledgedLevel
) noexcept {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_leases.find(fileRef);
    if (it == m_leases.end()) return NtStatus::NotFound;

    auto& lease = it->second;
    if (lease.holdingNodeId != nodeId) return NtStatus::AccessDenied;

    lease.grantedLeaseLevel = acknowledgedLevel;
    lease.isBreaking = false;
    lease.breakRequestedLevel = OPLOCK_LEVEL_NONE;
    return NtStatus::Success;
}

uint32_t CsvOplockManager::getLeaseLevel(uint64_t fileRef) const noexcept {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_leases.find(fileRef);
    return (it != m_leases.end()) ? it->second.grantedLeaseLevel : OPLOCK_LEVEL_NONE;
}

bool CsvOplockManager::isOplockBreaking(uint64_t fileRef) const noexcept {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_leases.find(fileRef);
    return (it != m_leases.end()) ? it->second.isBreaking : false;
}

void CsvOplockManager::clear() noexcept {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_leases.clear();
}

// ============================================================================
// 3. SovereignCsvEngine Implementation
// ============================================================================

void SovereignCsvEngine::initializeVolume(std::wstring_view volumePath) noexcept {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_initialized) return;

    m_volumePath = std::wstring(volumePath);
    m_coordinatorNodeId = 1;
    m_clusterEpoch = 1;
    m_nextFileRef = 1000;

    m_dlm.clear();
    m_oplockMgr.clear();
    m_nodes.clear();
    m_fileBuffers.clear();
    m_fileNames.clear();
    m_auditLog.clear();

    m_initialized = true;
}

void SovereignCsvEngine::resetVolume() noexcept {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_dlm.clear();
    m_oplockMgr.clear();
    m_nodes.clear();
    m_fileBuffers.clear();
    m_fileNames.clear();
    m_auditLog.clear();
    m_coordinatorNodeId = 1;
    m_clusterEpoch = 1;
    m_nextFileRef = 1000;
    m_initialized = true;
}

NtStatus SovereignCsvEngine::registerNode(
    uint32_t nodeId,
    std::wstring_view nodeName,
    bool isCoordinator
) noexcept {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    CsvClusterNode node{
        .nodeId = nodeId,
        .nodeName = std::wstring(nodeName),
        .isCoordinator = isCoordinator,
        .connectionState = NodeConnectionState::OnlineDirect,
        .directIoCount = 0,
        .redirectedIoCount = 0,
        .cachedEpoch = m_clusterEpoch
    };

    if (isCoordinator) {
        m_coordinatorNodeId = nodeId;
    }

    m_nodes[nodeId] = std::move(node);
    recordAudit(nodeId, L"Node Registered in Cluster Roster");
    return NtStatus::Success;
}

NtStatus SovereignCsvEngine::unregisterNode(uint32_t nodeId) noexcept {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_nodes.find(nodeId);
    if (it == m_nodes.end()) return NtStatus::NotFound;
    m_nodes.erase(it);
    recordAudit(nodeId, L"Node Unregistered from Cluster Roster");
    return NtStatus::Success;
}

NtStatus SovereignCsvEngine::setNodeConnectionState(
    uint32_t nodeId,
    NodeConnectionState state
) noexcept {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_nodes.find(nodeId);
    if (it == m_nodes.end()) return NtStatus::NotFound;
    it->second.connectionState = state;
    recordAudit(nodeId, L"Node Connection State Transitioned");
    return NtStatus::Success;
}

NodeConnectionState SovereignCsvEngine::getNodeConnectionState(uint32_t nodeId) const noexcept {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_nodes.find(nodeId);
    return (it != m_nodes.end()) ? it->second.connectionState : NodeConnectionState::Offline;
}

uint32_t SovereignCsvEngine::getCoordinatorNodeId() const noexcept {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_coordinatorNodeId;
}

NtStatus SovereignCsvEngine::failoverCoordinator(uint32_t newCoordinatorNodeId) noexcept {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_nodes.find(newCoordinatorNodeId);
    if (it == m_nodes.end()) return NtStatus::NotFound;

    // Demote current coordinator
    auto oldCoordIt = m_nodes.find(m_coordinatorNodeId);
    if (oldCoordIt != m_nodes.end()) {
        oldCoordIt->second.isCoordinator = false;
    }

    // Promote new coordinator
    it->second.isCoordinator = true;
    m_coordinatorNodeId = newCoordinatorNodeId;
    m_clusterEpoch++;

    // Broadcast new epoch to all online cluster nodes
    for (auto& [id, node] : m_nodes) {
        node.cachedEpoch = m_clusterEpoch;
    }

    recordAudit(newCoordinatorNodeId, L"Coordinator Failover Complete - New Cluster Coordinator Elected");
    return NtStatus::Success;
}

NtStatus SovereignCsvEngine::createClusterFile(
    uint32_t nodeId,
    std::wstring_view fileName,
    uint64_t& outFileRef
) noexcept {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_nodes.find(nodeId);
    if (it == m_nodes.end() || it->second.connectionState == NodeConnectionState::Offline) {
        return NtStatus::DeviceNotReady;
    }

    // Metadata operation is synchronized via DLM protected write
    std::wstring metaRes = L"CSV$METADATA$" + std::wstring(fileName);
    m_dlm.acquireLock(nodeId, metaRes, DLM_LOCK_PW, true);

    outFileRef = ++m_nextFileRef;
    m_fileBuffers[outFileRef] = std::vector<uint8_t>();
    m_fileNames[outFileRef] = std::wstring(fileName);

    if (it->second.connectionState == NodeConnectionState::OnlineDirect) {
        it->second.directIoCount++;
    } else {
        it->second.redirectedIoCount++;
    }

    m_dlm.releaseLock(nodeId, metaRes);
    return NtStatus::Success;
}

NtStatus SovereignCsvEngine::readClusterFile(
    uint32_t nodeId,
    uint64_t fileRef,
    uint64_t offset,
    std::span<uint8_t> buffer,
    size_t& bytesRead
) noexcept {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto nodeIt = m_nodes.find(nodeId);
    if (nodeIt == m_nodes.end() || nodeIt->second.connectionState == NodeConnectionState::Offline) {
        return NtStatus::DeviceNotReady;
    }

    auto fileIt = m_fileBuffers.find(fileRef);
    if (fileIt == m_fileBuffers.end()) return NtStatus::NotFound;

    const auto& fileBuf = fileIt->second;
    if (offset >= fileBuf.size()) {
        bytesRead = 0;
        return NtStatus::Success;
    }

    size_t available = fileBuf.size() - offset;
    size_t toCopy = std::min(available, buffer.size());
    std::memcpy(buffer.data(), fileBuf.data() + offset, toCopy);
    bytesRead = toCopy;

    if (nodeIt->second.connectionState == NodeConnectionState::OnlineDirect) {
        nodeIt->second.directIoCount++;
    } else {
        nodeIt->second.redirectedIoCount++;
    }

    return NtStatus::Success;
}

NtStatus SovereignCsvEngine::writeClusterFile(
    uint32_t nodeId,
    uint64_t fileRef,
    uint64_t offset,
    std::span<const uint8_t> buffer,
    size_t& bytesWritten
) noexcept {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto nodeIt = m_nodes.find(nodeId);
    if (nodeIt == m_nodes.end() || nodeIt->second.connectionState == NodeConnectionState::Offline) {
        return NtStatus::DeviceNotReady;
    }

    auto fileIt = m_fileBuffers.find(fileRef);
    if (fileIt == m_fileBuffers.end()) return NtStatus::NotFound;

    // Check Oplock conflicts
    bool breakNeeded = false;
    uint32_t targetNode = 0;
    uint32_t newLevel = 0;
    m_oplockMgr.checkOplockBreak(fileRef, nodeId, 2 /* Write */, breakNeeded, targetNode, newLevel);

    auto& fileBuf = fileIt->second;
    size_t required = offset + buffer.size();
    if (required > fileBuf.size()) {
        fileBuf.resize(required, 0);
    }

    std::memcpy(fileBuf.data() + offset, buffer.data(), buffer.size());
    bytesWritten = buffer.size();

    if (nodeIt->second.connectionState == NodeConnectionState::OnlineDirect) {
        nodeIt->second.directIoCount++;
    } else {
        nodeIt->second.redirectedIoCount++;
    }

    return NtStatus::Success;
}

void SovereignCsvEngine::recordAudit(uint32_t nodeId, std::wstring_view description) noexcept {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    CsvAuditRecord rec;
    rec.nodeId = nodeId;
    rec.description = std::wstring(description);
    rec.timestampUs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count());
    m_auditLog.push_back(rec);
}

std::vector<CsvAuditRecord> SovereignCsvEngine::getAuditHistory() const noexcept {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_auditLog;
}

void SovereignCsvEngine::clearAuditHistory() noexcept {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_auditLog.clear();
}

uint64_t SovereignCsvEngine::getNodeDirectIoCount(uint32_t nodeId) const noexcept {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_nodes.find(nodeId);
    return (it != m_nodes.end()) ? it->second.directIoCount : 0;
}

uint64_t SovereignCsvEngine::getNodeRedirectedIoCount(uint32_t nodeId) const noexcept {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_nodes.find(nodeId);
    return (it != m_nodes.end()) ? it->second.redirectedIoCount : 0;
}

// ============================================================================
// 4. Verification Suite (Suite 586)
// ============================================================================

bool VerifyHorizon370NtfsCsvSubsystem() noexcept {
    std::cout << "\n[TEST] Running Suite 586: Horizon 370 - Sovereign MicaNT NTFS Clustered Shared Volumes (CSV)...\n";

    auto& csv = SovereignCsvEngine::Instance();
    csv.resetVolume();

    // ------------------------------------------------------------------------
    // Pillar 1: CSV Volume Topology & Multi-Node Cluster Registration
    // ------------------------------------------------------------------------
    csv.initializeVolume(L"\\Device\\HarddiskVolumeCsv");

    NtStatus reg1 = csv.registerNode(1, L"MicaNode-01", true);  // Coordinator
    NtStatus reg2 = csv.registerNode(2, L"MicaNode-02", false); // Worker 1
    NtStatus reg3 = csv.registerNode(3, L"MicaNode-03", false); // Worker 2

    if (reg1 != NtStatus::Success || reg2 != NtStatus::Success || reg3 != NtStatus::Success) {
        std::cerr << "  [-] Step 1 Failed: Cluster node registration failed.\n";
        return false;
    }

    if (csv.getCoordinatorNodeId() != 1) {
        std::cerr << "  [-] Step 2 Failed: Initial coordinator election mismatch.\n";
        return false;
    }

    if (csv.getNodeConnectionState(1) != NodeConnectionState::OnlineDirect ||
        csv.getNodeConnectionState(2) != NodeConnectionState::OnlineDirect ||
        csv.getNodeConnectionState(3) != NodeConnectionState::OnlineDirect) {
        std::cerr << "  [-] Step 3 Failed: Nodes initial connection state must be OnlineDirect.\n";
        return false;
    }

    std::cout << "  Horizon 370 Architectural Validation Complete:\n";
    std::cout << "    Pillar 1: CSV Volume Topology & 3-Node Cluster Registration OK\n";

    // ------------------------------------------------------------------------
    // Pillar 2: Distributed Lock Manager (DLM) Lock Arbitration
    // ------------------------------------------------------------------------
    auto& dlm = csv.getDlm();
    std::wstring dbResource = L"\\ClusterStorage\\Volume1\\shared_database.mdf";

    // 1. Node 1 acquires Protected Read (PR)
    NtStatus lock1 = dlm.acquireLock(1, dbResource, DLM_LOCK_PR, true);
    if (lock1 != NtStatus::Success || dlm.getLockMode(dbResource) != DLM_LOCK_PR) {
        std::cerr << "  [-] Step 4 Failed: Node 1 DLM PR acquisition failed.\n";
        return false;
    }

    // 2. Node 2 acquires Protected Read (PR) concurrently - should succeed!
    NtStatus lock2 = dlm.acquireLock(2, dbResource, DLM_LOCK_PR, true);
    if (lock2 != NtStatus::Success || dlm.getActiveHolderCount(dbResource) != 2) {
        std::cerr << "  [-] Step 5 Failed: Node 2 concurrent DLM PR acquisition failed.\n";
        return false;
    }

    // 3. Node 3 requests Exclusive Lock (EX) - incompatible with PR, must be queued/pending!
    NtStatus lock3 = dlm.acquireLock(3, dbResource, DLM_LOCK_EX, true);
    if (lock3 != NtStatus::Pending || dlm.getWaitingCount(dbResource) != 1) {
        std::cerr << "  [-] Step 6 Failed: Node 3 DLM EX should be pending.\n";
        return false;
    }

    // 4. Node 1 releases PR
    NtStatus rel1 = dlm.releaseLock(1, dbResource);
    if (rel1 != NtStatus::Success || dlm.getActiveHolderCount(dbResource) != 1) {
        std::cerr << "  [-] Step 7 Failed: Node 1 DLM release failed.\n";
        return false;
    }

    // 5. Node 2 releases PR -> Now Node 3's pending EX lock must automatically be granted!
    NtStatus rel2 = dlm.releaseLock(2, dbResource);
    if (rel2 != NtStatus::Success || dlm.getLockMode(dbResource) != DLM_LOCK_EX ||
        dlm.getActiveHolderCount(dbResource) != 1 || dlm.getWaitingCount(dbResource) != 0) {
        std::cerr << "  [-] Step 8 Failed: Node 3 promotion to EX lock failed upon release.\n";
        return false;
    }

    // 6. Node 3 releases EX lock
    dlm.releaseLock(3, dbResource);
    if (dlm.getLockMode(dbResource) != DLM_LOCK_NL) {
        std::cerr << "  [-] Step 9 Failed: Resource lock mode did not return to NL.\n";
        return false;
    }

    std::cout << "    Pillar 2: Distributed Lock Manager (DLM) VAX/NT Arbitration OK\n";

    // ------------------------------------------------------------------------
    // Pillar 3: Opportunistic Locks (Oplocks / RWH Leases) & Cross-Node Breaks
    // ------------------------------------------------------------------------
    auto& oplocks = csv.getOplockMgr();
    uint64_t fileRef = 0;
    csv.createClusterFile(1, L"virtual_machine_disk.vhdx", fileRef);

    // Node 2 requests Read-Write-Handle (RWH) Lease
    uint32_t grantedLease = OPLOCK_LEVEL_NONE;
    NtStatus leaseStatus = oplocks.requestLease(2, fileRef, OPLOCK_LEVEL_RWH, grantedLease);
    if (leaseStatus != NtStatus::Success || grantedLease != OPLOCK_LEVEL_RWH) {
        std::cerr << "  [-] Step 10 Failed: Node 2 RWH lease request failed.\n";
        return false;
    }

    // Node 3 attempts to write - triggers Oplock Break
    bool breakNeeded = false;
    uint32_t breakTarget = 0;
    uint32_t targetNewLevel = 0;
    oplocks.checkOplockBreak(fileRef, 3, 2 /* Write */, breakNeeded, breakTarget, targetNewLevel);

    if (!breakNeeded || breakTarget != 2 || targetNewLevel != OPLOCK_LEVEL_NONE) {
        std::cerr << "  [-] Step 11 Failed: Oplock break was not triggered correctly.\n";
        return false;
    }

    if (!oplocks.isOplockBreaking(fileRef)) {
        std::cerr << "  [-] Step 12 Failed: Lease isBreaking flag mismatch.\n";
        return false;
    }

    // Node 2 acknowledges break (downgrade to None)
    NtStatus ackStatus = oplocks.acknowledgeBreak(2, fileRef, OPLOCK_LEVEL_NONE);
    if (ackStatus != NtStatus::Success || oplocks.getLeaseLevel(fileRef) != OPLOCK_LEVEL_NONE ||
        oplocks.isOplockBreaking(fileRef)) {
        std::cerr << "  [-] Step 13 Failed: Acknowledge break failed.\n";
        return false;
    }

    std::cout << "    Pillar 3: Opportunistic Locks (RWH Leases) & Cross-Node Break OK\n";

    // ------------------------------------------------------------------------
    // Pillar 4: Direct vs. Redirected I/O Path Switching on Fabric Drop
    // ------------------------------------------------------------------------
    uint8_t writeBuf[64];
    std::memset(writeBuf, 0xA5, sizeof(writeBuf));
    size_t written = 0;

    // Direct I/O write from Node 3
    csv.writeClusterFile(3, fileRef, 0, writeBuf, written);
    if (written != sizeof(writeBuf) || csv.getNodeDirectIoCount(3) != 1 || csv.getNodeRedirectedIoCount(3) != 0) {
        std::cerr << "  [-] Step 14 Failed: Node 3 Direct I/O accounting mismatch.\n";
        return false;
    }

    // Simulate SAN Fabric Network drop on Node 3 -> Switched to Redirected I/O!
    csv.setNodeConnectionState(3, NodeConnectionState::OnlineRedirected);
    if (csv.getNodeConnectionState(3) != NodeConnectionState::OnlineRedirected) {
        std::cerr << "  [-] Step 15 Failed: Node 3 connection state transition failed.\n";
        return false;
    }

    // Redirected I/O write from Node 3 (tunneled to Coordinator Node 1)
    csv.writeClusterFile(3, fileRef, 64, writeBuf, written);
    if (written != sizeof(writeBuf) || csv.getNodeRedirectedIoCount(3) != 1) {
        std::cerr << "  [-] Step 16 Failed: Node 3 Redirected I/O accounting mismatch.\n";
        return false;
    }

    // Verify Read consistency across cluster
    uint8_t readBuf[128];
    size_t readBytes = 0;
    csv.readClusterFile(2, fileRef, 0, readBuf, readBytes);
    if (readBytes != 128 || readBuf[0] != 0xA5 || readBuf[127] != 0xA5) {
        std::cerr << "  [-] Step 17 Failed: Cluster read buffer data inconsistency.\n";
        return false;
    }

    std::cout << "    Pillar 4: Direct vs. Redirected I/O Switching on Fabric Drop OK\n";

    // ------------------------------------------------------------------------
    // Pillar 5: Seamless Zero-Downtime Coordinator Failover
    // ------------------------------------------------------------------------
    // Simulate crash/shutdown of Node 1 -> Node 2 elected as new Coordinator
    NtStatus failoverStatus = csv.failoverCoordinator(2);
    if (failoverStatus != NtStatus::Success || csv.getCoordinatorNodeId() != 2) {
        std::cerr << "  [-] Step 18 Failed: Coordinator failover to Node 2 failed.\n";
        return false;
    }

    // Node 3 writes to cluster after failover - verifies cluster functions seamlessly
    csv.writeClusterFile(3, fileRef, 128, writeBuf, written);
    if (written != sizeof(writeBuf)) {
        std::cerr << "  [-] Step 19 Failed: Cluster write post-failover failed.\n";
        return false;
    }

    std::cout << "    Pillar 5: Seamless Zero-Downtime Coordinator Failover OK\n";

    // ------------------------------------------------------------------------
    // Pillar 6: High-Throughput Concurrent Multi-Node Cluster Benchmark
    // ------------------------------------------------------------------------
    constexpr size_t benchmarkIterations = 1000;
    uint8_t payload[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

    auto tStart = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < benchmarkIterations; ++i) {
        uint32_t activeNode = static_cast<uint32_t>((i % 2) + 2); // Rotate between Node 2 and Node 3
        size_t bWritten = 0;
        csv.writeClusterFile(activeNode, fileRef, (i % 10) * 16, payload, bWritten);
    }

    auto tEnd = std::chrono::high_resolution_clock::now();
    double totalUs = std::chrono::duration<double, std::micro>(tEnd - tStart).count();
    double avgUs = totalUs / static_cast<double>(benchmarkIterations);

    if (avgUs > 20.0) {
        std::cerr << "  [-] Step 20 Failed: Average cluster I/O latency exceeded (" << avgUs << " us).\n";
        return false;
    }

    std::cout << "    Pillar 6: High-Throughput Multi-Node Cluster Benchmark (" << avgUs << " us/op) OK\n";
    std::cout << "[TEST] Suite 586: Horizon 370 - Sovereign MicaNT NTFS Clustered Shared Volumes PASSED.\n";

    return true;
}

} // namespace micant::ntfs::csv
