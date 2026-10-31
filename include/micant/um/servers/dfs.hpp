#ifndef MICANT_DFS_HPP
#define MICANT_DFS_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <optional>
#include <deque>
#include "micant/shared/ntstatus.hpp"
#include "micant/um/subsystems/winbase/version.hpp"
#include "micant/um/servers/scm.hpp"

namespace micant::dfs {

// ============================================================================
// 1. MS-DFSNM & MS-DFSC Namespace Referral Wire Protocol Enums & Structs
// ============================================================================

// Referral Request Flags
constexpr uint32_t DFS_OLD_REFERRAL_VERSION = 0x0001;
constexpr uint32_t DFS_REFERRAL_VERSION_3    = 0x0003;
constexpr uint32_t DFS_REFERRAL_VERSION_4    = 0x0004;

// Target Priority Classes (MS-DFSNM Section 2.2.2.6)
enum class DfsTargetPriorityClass : uint8_t {
    SiteCostNormal = 0, // Normal priority within cost rank
    GlobalHigh     = 1, // Highest priority across all targets
    SiteCostHigh   = 2, // Highest priority within same-cost rank
    SiteCostLow    = 3, // Lowest priority within same-cost rank
    GlobalLow      = 4  // Lowest priority across all targets
};

inline const char* DfsPriorityClassToString(DfsTargetPriorityClass pc) noexcept {
    switch (pc) {
        case DfsTargetPriorityClass::SiteCostNormal: return "SiteCostNormal";
        case DfsTargetPriorityClass::GlobalHigh:     return "GlobalHigh";
        case DfsTargetPriorityClass::SiteCostHigh:   return "SiteCostHigh";
        case DfsTargetPriorityClass::SiteCostLow:    return "SiteCostLow";
        case DfsTargetPriorityClass::GlobalLow:      return "GlobalLow";
        default:                                     return "Normal";
    }
}

// Target State
enum class DfsTargetState : uint32_t {
    Online  = 1,
    Offline = 2
};

// Target Share Representation
struct DfsTarget {
    std::string targetServer;      // e.g. "FS-HQ-01.micant.local"
    std::string targetFolder;      // e.g. "CorpData"
    std::string fullUncPath;       // e.g. "\\FS-HQ-01.micant.local\CorpData"
    std::string siteName{"Default-First-Site-Name"};
    uint32_t siteCost{0};          // Calculated from client site
    DfsTargetPriorityClass priorityClass{DfsTargetPriorityClass::SiteCostNormal};
    uint16_t priorityRank{0};
    DfsTargetState state{DfsTargetState::Online};
};

// Namespace Folder / Link
struct DfsFolderLink {
    std::string linkPath;          // e.g. "CorpData\Finance"
    std::string fullDfsPath;       // e.g. "\\micant.local\CorpData\Finance"
    std::string description;
    uint32_t timeToLiveSec{300};   // 5 minutes default for links
    std::vector<DfsTarget> targets;
    bool targetFailback{true};
};

// Namespace Root
enum class DfsNamespaceType : uint32_t {
    Standalone = 1,
    DomainV1   = 2,
    DomainV2   = 3 // Windows Server 2008+ Domain-based v2
};

struct DfsNamespaceRoot {
    std::string rootName;          // e.g. "CorpData"
    std::string domainName;        // e.g. "micant.local"
    std::string fullDfsPath;       // e.g. "\\micant.local\CorpData"
    DfsNamespaceType type{DfsNamespaceType::DomainV2};
    uint32_t rootTtlSec{1800};     // 30 minutes default for roots
    std::vector<DfsTarget> rootTargets;
    std::map<std::string, DfsFolderLink> links; // Relative link path -> FolderLink
    bool accessBasedEnumeration{false};
    bool insiteReferralsOnly{false};
};

// Client Referral Entry in Response
struct DfsReferralEntry {
    uint16_t versionNumber{DFS_REFERRAL_VERSION_4};
    uint16_t size{0};
    uint16_t serverType{0};        // 0 = link, 1 = root
    uint16_t referralFlags{0x0001};// DFS_REFERRAL_FLAG_TARGET_SET_BOUNDARY
    uint32_t timeToLive{300};      // in seconds
    std::string targetUnc;
    DfsTargetPriorityClass priorityClass{DfsTargetPriorityClass::SiteCostNormal};
    uint16_t priorityRank{0};
};

// Referral Request Parameters
struct DfsReferralRequest {
    uint16_t maxReferralLevel{DFS_REFERRAL_VERSION_4};
    std::string requestPath;       // e.g. "\\micant.local\CorpData\Finance"
    std::string clientIp{"127.0.0.1"};
    std::string clientSite{"Default-First-Site-Name"};
};

// Referral Response
struct DfsReferralResponse {
    uint32_t status{0};            // STATUS_SUCCESS (0) or error
    uint16_t pathConsumedLength{0};// characters in path consumed
    std::vector<DfsReferralEntry> referrals;
};

// ============================================================================
// 2. MS-FRS2 DFS Replication (DFS-R) Multi-Master Engine Structs
// ============================================================================

// Global Version Sequence Number (GVSN)
struct DfsrGvsn {
    std::string dbGuid;            // 128-bit member database GUID string
    uint64_t sequenceNumber{0};    // Monotonically increasing counter

    std::string toString() const {
        return dbGuid + ":" + std::to_string(sequenceNumber);
    }
};

// Replicated File / Record in Content Set
struct DfsrFileRecord {
    std::string relativePath;      // e.g. "Budgets\Q4_FY26.xlsx"
    uint64_t fileSize{0};
    uint64_t fileAttributes{0x20}; // FILE_ATTRIBUTE_ARCHIVE
    uint64_t lastWriteTime{0};     // Windows FILETIME (100ns intervals)
    DfsrGvsn gvsn;                 // Version assigned at creation/modification
    std::string sha256Hash;        // Content hash
    bool isDeleted{false};
};

// Conflict Record in ConflictAndDeleted
struct DfsrConflictRecord {
    std::string originalPath;
    std::string preservedFileName; // e.g. "Q4_FY26-{DbGuid}-Conflict.xlsx"
    DfsrGvsn losingGvsn;
    DfsrGvsn winningGvsn;
    std::string conflictReason;    // "Last-Writer-Wins Update Conflict"
    uint64_t timestamp{0};
};

// Replicated Folder Configuration
struct DfsrReplicatedFolder {
    std::string folderId;          // GUID
    std::string folderName;        // e.g. "FinanceData"
    std::string localRootPath;     // e.g. "D:\Shares\Finance"
    std::string stagingPath;       // e.g. "D:\Shares\Finance\DfsrPrivate\Staging"
    uint64_t stagingQuotaBytes{4ULL * 1024 * 1024 * 1024}; // 4 GB default staging quota
    uint64_t stagingUsedBytes{0};
    bool enabled{true};
    std::map<std::string, DfsrFileRecord> files; // relativePath -> Record
    std::vector<DfsrConflictRecord> conflicts;
};

// Replication Member in Replication Group
struct DfsrMember {
    std::string memberFqdn;        // e.g. "FS-HQ-01.micant.local"
    std::string memberGuid;        // GUID
    std::string siteName{"Default-First-Site-Name"};
    bool isSendOnly{false};
    bool isReceiveOnly{false};
    uint64_t bandwidthScheduleKbps{0}; // 0 = full bandwidth
};

// Replication Group
struct DfsrReplicationGroup {
    std::string groupId;           // GUID
    std::string groupName;         // e.g. "RG-Corporate-Data"
    std::string description;
    std::vector<DfsrMember> members;
    std::map<std::string, DfsrReplicatedFolder> folders; // folderName -> ReplicatedFolder
    uint32_t rdcChunkMinSize{2048};
    uint32_t rdcChunkTargetSize{8192};
    uint32_t rdcChunkMaxSize{65536};
};

// RDC (Remote Differential Compression) Chunk Signature
struct RdcChunkSignature {
    uint64_t offset{0};
    uint32_t length{0};
    uint32_t hash32{0};           // Rolling hash / Adler32
    std::string sha256;           // Cryptographic chunk digest
};

// ============================================================================
// 3. Sovereign DFS Subsystem Engine (Singleton)
// ============================================================================

class SovereignDfsEngine {
private:
    mutable std::mutex m_mutex;
    bool m_initialized{false};

    // Active Directory Sites and Inter-site Link Cost Matrix
    // "SiteA->SiteB" -> Cost (0-100)
    std::map<std::string, uint32_t> m_siteCostMatrix;

    // DFS Namespaces: Full DFS Path (lowercase) -> DfsNamespaceRoot
    std::map<std::string, DfsNamespaceRoot> m_namespaces;

    // Client Referral Cache: Request Path -> DfsReferralResponse
    std::map<std::string, DfsReferralResponse> m_referralCache;

    // DFS Replication Groups: Group Name (lowercase) -> DfsrReplicationGroup
    std::map<std::string, DfsrReplicationGroup> m_replicationGroups;

    // Subsystem Metrics
    std::atomic<uint64_t> m_totalReferralRequests{0};
    std::atomic<uint64_t> m_totalCacheHits{0};
    std::atomic<uint64_t> m_totalReplicatedFiles{0};
    std::atomic<uint64_t> m_totalBytesReplicated{0};
    std::atomic<uint64_t> m_totalConflictsResolved{0};
    std::atomic<uint64_t> m_lastReferralLatencyUs{18};

    SovereignDfsEngine() {
        initializeDefaultsLocked();
    }

    void initializeDefaultsLocked() {
        if (m_initialized) return;

        // 1. Setup Inter-Site Cost Matrix
        m_siteCostMatrix["Default-First-Site-Name->Default-First-Site-Name"] = 0;
        m_siteCostMatrix["Default-First-Site-Name->Branch-London"] = 50;
        m_siteCostMatrix["Default-First-Site-Name->Branch-Tokyo"] = 100;
        m_siteCostMatrix["Branch-London->Default-First-Site-Name"] = 50;
        m_siteCostMatrix["Branch-Tokyo->Default-First-Site-Name"] = 100;

        // 2. Initialize Default Domain-Based v2 Namespace: \\micant.local\CorpData
        DfsNamespaceRoot rootCorp;
        rootCorp.rootName = "CorpData";
        rootCorp.domainName = "micant.local";
        rootCorp.fullDfsPath = "\\\\micant.local\\CorpData";
        rootCorp.type = DfsNamespaceType::DomainV2;
        rootCorp.rootTtlSec = 1800;

        // Root targets
        DfsTarget tRoot1;
        tRoot1.targetServer = "DC01.micant.local";
        tRoot1.targetFolder = "CorpData";
        tRoot1.fullUncPath = "\\\\DC01.micant.local\\CorpData";
        tRoot1.siteName = "Default-First-Site-Name";
        tRoot1.priorityClass = DfsTargetPriorityClass::SiteCostNormal;
        tRoot1.state = DfsTargetState::Online;
        rootCorp.rootTargets.push_back(tRoot1);

        DfsTarget tRoot2;
        tRoot2.targetServer = "DC02.micant.local";
        tRoot2.targetFolder = "CorpData";
        tRoot2.fullUncPath = "\\\\DC02.micant.local\\CorpData";
        tRoot2.siteName = "Branch-London";
        tRoot2.priorityClass = DfsTargetPriorityClass::SiteCostNormal;
        tRoot2.state = DfsTargetState::Online;
        rootCorp.rootTargets.push_back(tRoot2);

        // Link 1: Finance (Multi-target with site-cost priority)
        DfsFolderLink linkFin;
        linkFin.linkPath = "Finance";
        linkFin.fullDfsPath = "\\\\micant.local\\CorpData\\Finance";
        linkFin.description = "Corporate Finance & Accounting Shares";
        linkFin.timeToLiveSec = 300;

        DfsTarget tFinHq;
        tFinHq.targetServer = "FS-HQ-01.micant.local";
        tFinHq.targetFolder = "FinanceData";
        tFinHq.fullUncPath = "\\\\FS-HQ-01.micant.local\\FinanceData";
        tFinHq.siteName = "Default-First-Site-Name";
        tFinHq.priorityClass = DfsTargetPriorityClass::SiteCostNormal;
        tFinHq.state = DfsTargetState::Online;
        linkFin.targets.push_back(tFinHq);

        DfsTarget tFinLon;
        tFinLon.targetServer = "FS-LON-01.micant.local";
        tFinLon.targetFolder = "FinanceData";
        tFinLon.fullUncPath = "\\\\FS-LON-01.micant.local\\FinanceData";
        tFinLon.siteName = "Branch-London";
        tFinLon.priorityClass = DfsTargetPriorityClass::SiteCostNormal;
        tFinLon.state = DfsTargetState::Online;
        linkFin.targets.push_back(tFinLon);

        rootCorp.links["Finance"] = linkFin;

        // Link 2: Engineering
        DfsFolderLink linkEng;
        linkEng.linkPath = "Engineering";
        linkEng.fullDfsPath = "\\\\micant.local\\CorpData\\Engineering";
        linkEng.description = "Software Engineering & R&D Repository";
        linkEng.timeToLiveSec = 300;

        DfsTarget tEngHq;
        tEngHq.targetServer = "FS-HQ-02.micant.local";
        tEngHq.targetFolder = "EngShare";
        tEngHq.fullUncPath = "\\\\FS-HQ-02.micant.local\\EngShare";
        tEngHq.siteName = "Default-First-Site-Name";
        tEngHq.priorityClass = DfsTargetPriorityClass::GlobalHigh;
        tEngHq.state = DfsTargetState::Online;
        linkEng.targets.push_back(tEngHq);

        rootCorp.links["Engineering"] = linkEng;

        // Link 3: Public
        DfsFolderLink linkPub;
        linkPub.linkPath = "Public";
        linkPub.fullDfsPath = "\\\\micant.local\\CorpData\\Public";
        linkPub.description = "Enterprise Read-Only Public Collateral";
        linkPub.timeToLiveSec = 600;

        DfsTarget tPubHq;
        tPubHq.targetServer = "FS-HQ-01.micant.local";
        tPubHq.targetFolder = "PublicDocs";
        tPubHq.fullUncPath = "\\\\FS-HQ-01.micant.local\\PublicDocs";
        tPubHq.siteName = "Default-First-Site-Name";
        tPubHq.priorityClass = DfsTargetPriorityClass::SiteCostNormal;
        tPubHq.state = DfsTargetState::Online;
        linkPub.targets.push_back(tPubHq);

        rootCorp.links["Public"] = linkPub;

        // Store namespace
        std::string lowerPath = rootCorp.fullDfsPath;
        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);
        m_namespaces[lowerPath] = rootCorp;

        // 3. Initialize Default DFS Replication Group: RG-Finance-Replication
        DfsrReplicationGroup rgFin;
        rgFin.groupId = "{A1B2C3D4-E5F6-7890-ABCD-EF0123456789}";
        rgFin.groupName = "RG-Finance-Replication";
        rgFin.description = "Bi-directional replication for Finance Shares";

        DfsrMember m1;
        m1.memberFqdn = "FS-HQ-01.micant.local";
        m1.memberGuid = "{11111111-2222-3333-4444-555555555555}";
        m1.siteName = "Default-First-Site-Name";
        rgFin.members.push_back(m1);

        DfsrMember m2;
        m2.memberFqdn = "FS-LON-01.micant.local";
        m2.memberGuid = "{66666666-7777-8888-9999-AAAAAAAAAAAA}";
        m2.siteName = "Branch-London";
        rgFin.members.push_back(m2);

        DfsrReplicatedFolder rfFin;
        rfFin.folderId = "{F1F2F3F4-E1E2-D1D2-C1C2-B1B2A1A29999}";
        rfFin.folderName = "FinanceData";
        rfFin.localRootPath = "D:\\Shares\\Finance";
        rfFin.stagingPath = "D:\\Shares\\Finance\\DfsrPrivate\\Staging";
        rfFin.stagingQuotaBytes = 4ULL * 1024 * 1024 * 1024;

        // Seed initial files
        DfsrFileRecord f1;
        f1.relativePath = "GeneralLedger_2026.xlsx";
        f1.fileSize = 1048576;
        f1.lastWriteTime = 133500000000000000ULL;
        f1.gvsn = {m1.memberGuid, 101};
        f1.sha256Hash = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
        rfFin.files[f1.relativePath] = f1;

        DfsrFileRecord f2;
        f2.relativePath = "Audit_Report_Q3.docx";
        f2.fileSize = 524288;
        f2.lastWriteTime = 133500001000000000ULL;
        f2.gvsn = {m1.memberGuid, 102};
        f2.sha256Hash = "8754425f1bf3412764952de047b010e8600b87535fd97aa1e0fb42fb8027e46f";
        rfFin.files[f2.relativePath] = f2;

        rgFin.folders["FinanceData"] = rfFin;

        std::string lowerRg = rgFin.groupName;
        std::transform(lowerRg.begin(), lowerRg.end(), lowerRg.begin(), ::tolower);
        m_replicationGroups[lowerRg] = rgFin;

        // 4. SCM Services & VersionDatabase Registration
        registerScmServices();
        registerVersionDatabase();

        m_initialized = true;
    }

    void registerScmServices() {
        auto& scm = micant::scm::ServiceControlManager::get();

        // 1. Dfs (Distributed File System Namespace Server)
        auto recDfs = std::make_shared<micant::scm::ServiceRecord>();
        recDfs->serviceName = L"Dfs";
        recDfs->displayName = L"Distributed File System";
        recDfs->serviceType = micant::scm::SERVICE_WIN32_SHARE_PROCESS;
        recDfs->startType = micant::scm::SERVICE_AUTO_START;
        recDfs->errorControl = micant::scm::SERVICE_ERROR_NORMAL;
        recDfs->binaryPath = L"C:\\Windows\\System32\\dfssvc.dll";
        recDfs->status.dwServiceType = micant::scm::SERVICE_WIN32_SHARE_PROCESS;
        recDfs->status.dwCurrentState = micant::scm::SERVICE_RUNNING;
        recDfs->status.dwControlsAccepted = micant::scm::SERVICE_ACCEPT_STOP | micant::scm::SERVICE_ACCEPT_SHUTDOWN;
        scm.registerServiceRecord(recDfs);

        // 2. DFSR (DFS Replication Engine)
        auto recDfsr = std::make_shared<micant::scm::ServiceRecord>();
        recDfsr->serviceName = L"DFSR";
        recDfsr->displayName = L"DFS Replication";
        recDfsr->serviceType = micant::scm::SERVICE_WIN32_SHARE_PROCESS;
        recDfsr->startType = micant::scm::SERVICE_AUTO_START;
        recDfsr->errorControl = micant::scm::SERVICE_ERROR_NORMAL;
        recDfsr->binaryPath = L"C:\\Windows\\System32\\dfsr.dll";
        recDfsr->status.dwServiceType = micant::scm::SERVICE_WIN32_SHARE_PROCESS;
        recDfsr->status.dwCurrentState = micant::scm::SERVICE_RUNNING;
        recDfsr->status.dwControlsAccepted = micant::scm::SERVICE_ACCEPT_STOP | micant::scm::SERVICE_ACCEPT_SHUTDOWN;
        scm.registerServiceRecord(recDfsr);
    }

    void registerVersionDatabase() {
        auto& db = micant::version::VersionDatabase::Instance();
        db.RegisterModule("dfssvc.dll", "10.0.26100.1", "Distributed File System Service");
        db.RegisterModule("dfsr.dll", "10.0.26100.1", "DFS Replication Engine");
        db.RegisterModule("dfsutil.exe", "10.0.26100.1", "Distributed File System Management Utility");
        db.RegisterModule("dfsrdiag.exe", "10.0.26100.1", "DFS Replication Diagnostic Tool");
        db.RegisterModule("dfscli.dll", "10.0.26100.1", "DFS Client Referral Provider");
        db.RegisterModule("rdc.dll", "10.0.26100.1", "Remote Differential Compression Filter");
    }

public:
    static SovereignDfsEngine& instance() {
        static SovereignDfsEngine s_inst;
        return s_inst;
    }

    void initializeDefaults() {
        std::lock_guard<std::mutex> lock(m_mutex);
        initializeDefaultsLocked();
    }

    void resetForTest() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_siteCostMatrix.clear();
        m_namespaces.clear();
        m_referralCache.clear();
        m_replicationGroups.clear();

        m_totalReferralRequests.store(0);
        m_totalCacheHits.store(0);
        m_totalReplicatedFiles.store(0);
        m_totalBytesReplicated.store(0);
        m_totalConflictsResolved.store(0);
        m_lastReferralLatencyUs.store(18);

        m_initialized = false;
        initializeDefaultsLocked();
    }

    // ========================================================================
    // Site-Cost Resolution Engine
    // ========================================================================
    uint32_t getInterSiteCost(const std::string& siteA, const std::string& siteB) const {
        if (siteA == siteB) return 0;
        std::string key = siteA + "->" + siteB;
        auto it = m_siteCostMatrix.find(key);
        if (it != m_siteCostMatrix.end()) return it->second;
        return 999; // Default high cost for unlinked sites
    }

    void setInterSiteCost(const std::string& siteA, const std::string& siteB, uint32_t cost) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_siteCostMatrix[siteA + "->" + siteB] = cost;
        m_siteCostMatrix[siteB + "->" + siteA] = cost;
    }

    // ========================================================================
    // MS-DFSC Referral Resolution Protocol (FSCTL_DFS_GET_REFERRALS)
    // ========================================================================
    bool getReferrals(const DfsReferralRequest& req, DfsReferralResponse& resp) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_totalReferralRequests.fetch_add(1, std::memory_order_relaxed);

        std::string normPath = req.requestPath;
        std::replace(normPath.begin(), normPath.end(), '/', '\\');
        // Strip leading backslashes
        while (normPath.size() > 1 && normPath[0] == '\\' && normPath[1] == '\\') {
            normPath = normPath.substr(1);
        }

        // Check Referral Cache first
        std::string cacheKey = normPath + "|" + req.clientSite;
        auto itCache = m_referralCache.find(cacheKey);
        if (itCache != m_referralCache.end()) {
            resp = itCache->second;
            m_totalCacheHits.fetch_add(1, std::memory_order_relaxed);
            return true;
        }

        // Parse: \\domain\root[\link]
        // E.g. "\micant.local\CorpData\Finance"
        std::vector<std::string> parts;
        std::stringstream ss(normPath);
        std::string seg;
        while (std::getline(ss, seg, '\\')) {
            if (!seg.empty()) parts.push_back(seg);
        }

        if (parts.size() < 2) {
            resp.status = 0xC0000034; // STATUS_OBJECT_NAME_NOT_FOUND
            return false;
        }

        std::string nsKey = "\\\\" + parts[0] + "\\" + parts[1];
        std::string lowerNsKey = nsKey;
        std::transform(lowerNsKey.begin(), lowerNsKey.end(), lowerNsKey.begin(), ::tolower);

        auto itNs = m_namespaces.find(lowerNsKey);
        if (itNs == m_namespaces.end()) {
            resp.status = 0xC0000034; // STATUS_OBJECT_NAME_NOT_FOUND
            return false;
        }

        const auto& nsRoot = itNs->second;
        resp.status = 0; // STATUS_SUCCESS

        if (parts.size() == 2) {
            // Root Referral Request (e.g. \\micant.local\CorpData)
            resp.pathConsumedLength = static_cast<uint16_t>(nsKey.size());

            auto sortedTargets = nsRoot.rootTargets;
            // Sort by priority class, then site cost
            for (auto& t : sortedTargets) {
                t.siteCost = getInterSiteCost(req.clientSite, t.siteName);
            }
            std::sort(sortedTargets.begin(), sortedTargets.end(), [](const DfsTarget& a, const DfsTarget& b) {
                if (a.priorityClass != b.priorityClass) return a.priorityClass < b.priorityClass;
                return a.siteCost < b.siteCost;
            });

            for (const auto& t : sortedTargets) {
                if (t.state != DfsTargetState::Online) continue;
                DfsReferralEntry entry;
                entry.versionNumber = req.maxReferralLevel;
                entry.serverType = 1; // Root referral
                entry.timeToLive = nsRoot.rootTtlSec;
                entry.targetUnc = t.fullUncPath;
                entry.priorityClass = t.priorityClass;
                entry.priorityRank = t.priorityRank;
                resp.referrals.push_back(entry);
            }
        } else {
            // Link Referral Request (e.g. \\micant.local\CorpData\Finance)
            std::string linkName = parts[2];
            auto itLink = nsRoot.links.find(linkName);
            if (itLink == nsRoot.links.end()) {
                resp.status = 0xC0000034;
                return false;
            }

            const auto& link = itLink->second;
            resp.pathConsumedLength = static_cast<uint16_t>(link.fullDfsPath.size());

            auto sortedTargets = link.targets;
            for (auto& t : sortedTargets) {
                t.siteCost = getInterSiteCost(req.clientSite, t.siteName);
            }
            std::sort(sortedTargets.begin(), sortedTargets.end(), [](const DfsTarget& a, const DfsTarget& b) {
                if (a.priorityClass != b.priorityClass) return a.priorityClass < b.priorityClass;
                return a.siteCost < b.siteCost;
            });

            for (const auto& t : sortedTargets) {
                if (t.state != DfsTargetState::Online) continue;
                DfsReferralEntry entry;
                entry.versionNumber = req.maxReferralLevel;
                entry.serverType = 0; // Link referral
                entry.timeToLive = link.timeToLiveSec;
                entry.targetUnc = t.fullUncPath;
                entry.priorityClass = t.priorityClass;
                entry.priorityRank = t.priorityRank;
                resp.referrals.push_back(entry);
            }
        }

        // Cache response
        m_referralCache[cacheKey] = resp;
        return !resp.referrals.empty();
    }

    void flushReferralCache() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_referralCache.clear();
    }

    size_t getReferralCacheSize() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_referralCache.size();
    }

    // ========================================================================
    // Namespace Management API (DFS-N)
    // ========================================================================
    bool addNamespace(const DfsNamespaceRoot& root) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string lowerPath = root.fullDfsPath;
        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);
        m_namespaces[lowerPath] = root;
        m_referralCache.clear();
        return true;
    }

    bool removeNamespace(const std::string& fullDfsPath) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string lowerPath = fullDfsPath;
        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);
        auto it = m_namespaces.find(lowerPath);
        if (it == m_namespaces.end()) return false;
        m_namespaces.erase(it);
        m_referralCache.clear();
        return true;
    }

    bool getNamespace(const std::string& fullDfsPath, DfsNamespaceRoot* outRoot) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string lowerPath = fullDfsPath;
        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);
        auto it = m_namespaces.find(lowerPath);
        if (it == m_namespaces.end()) return false;
        if (outRoot) *outRoot = it->second;
        return true;
    }

    std::vector<DfsNamespaceRoot> getAllNamespaces() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<DfsNamespaceRoot> res;
        for (const auto& [k, v] : m_namespaces) res.push_back(v);
        return res;
    }

    bool addFolderLink(const std::string& fullDfsPath, const DfsFolderLink& link) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string lowerPath = fullDfsPath;
        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);
        auto it = m_namespaces.find(lowerPath);
        if (it == m_namespaces.end()) return false;
        it->second.links[link.linkPath] = link;
        m_referralCache.clear();
        return true;
    }

    bool removeFolderLink(const std::string& fullDfsPath, const std::string& linkPath) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string lowerPath = fullDfsPath;
        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);
        auto it = m_namespaces.find(lowerPath);
        if (it == m_namespaces.end()) return false;
        auto itLink = it->second.links.find(linkPath);
        if (itLink == it->second.links.end()) return false;
        it->second.links.erase(itLink);
        m_referralCache.clear();
        return true;
    }

    bool addTargetToLink(const std::string& fullDfsPath, const std::string& linkPath, const DfsTarget& target) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string lowerPath = fullDfsPath;
        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);
        auto it = m_namespaces.find(lowerPath);
        if (it == m_namespaces.end()) return false;
        auto itLink = it->second.links.find(linkPath);
        if (itLink == it->second.links.end()) return false;
        itLink->second.targets.push_back(target);
        m_referralCache.clear();
        return true;
    }

    bool setTargetState(const std::string& fullDfsPath, const std::string& linkPath,
                        const std::string& targetUnc, DfsTargetState state) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string lowerPath = fullDfsPath;
        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);
        auto it = m_namespaces.find(lowerPath);
        if (it == m_namespaces.end()) return false;
        auto itLink = it->second.links.find(linkPath);
        if (itLink == it->second.links.end()) return false;
        for (auto& t : itLink->second.targets) {
            if (t.fullUncPath == targetUnc) {
                t.state = state;
                m_referralCache.clear();
                return true;
            }
        }
        return false;
    }

    // ========================================================================
    // DFS Replication (DFS-R / MS-FRS2) Multi-Master Engine
    // ========================================================================
    bool addReplicationGroup(const DfsrReplicationGroup& rg) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string lowerRg = rg.groupName;
        std::transform(lowerRg.begin(), lowerRg.end(), lowerRg.begin(), ::tolower);
        m_replicationGroups[lowerRg] = rg;
        return true;
    }

    bool getReplicationGroup(const std::string& groupName, DfsrReplicationGroup* outRg) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string lowerRg = groupName;
        std::transform(lowerRg.begin(), lowerRg.end(), lowerRg.begin(), ::tolower);
        auto it = m_replicationGroups.find(lowerRg);
        if (it == m_replicationGroups.end()) return false;
        if (outRg) *outRg = it->second;
        return true;
    }

    std::vector<DfsrReplicationGroup> getAllReplicationGroups() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<DfsrReplicationGroup> res;
        for (const auto& [k, v] : m_replicationGroups) res.push_back(v);
        return res;
    }

    // Update File & Advance Version Vector (GVSN)
    bool updateFile(const std::string& groupName, const std::string& folderName,
                    const std::string& authorMemberGuid, const std::string& relPath,
                    uint64_t fileSize, const std::string& sha256Hash, uint64_t fileTime = 0) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string lowerRg = groupName;
        std::transform(lowerRg.begin(), lowerRg.end(), lowerRg.begin(), ::tolower);
        auto itRg = m_replicationGroups.find(lowerRg);
        if (itRg == m_replicationGroups.end()) return false;

        auto itRf = itRg->second.folders.find(folderName);
        if (itRf == itRg->second.folders.end()) return false;

        uint64_t nextSeq = 1;
        auto itFile = itRf->second.files.find(relPath);
        if (itFile != itRf->second.files.end()) {
            nextSeq = itFile->second.gvsn.sequenceNumber + 1;
        }

        DfsrFileRecord rec;
        rec.relativePath = relPath;
        rec.fileSize = fileSize;
        rec.fileAttributes = 0x20;
        if (fileTime != 0) {
            rec.lastWriteTime = fileTime;
        } else {
            uint64_t unix100ns = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count() / 100);
            rec.lastWriteTime = 116444736000000000ULL + unix100ns;
        }
        rec.gvsn = {authorMemberGuid, nextSeq};
        rec.sha256Hash = sha256Hash;
        rec.isDeleted = false;

        itRf->second.files[relPath] = rec;
        m_totalReplicatedFiles.fetch_add(1, std::memory_order_relaxed);
        m_totalBytesReplicated.fetch_add(fileSize, std::memory_order_relaxed);
        return true;
    }

    // Conflict Resolution: Reconcile Incoming File against Local File Record
    // Returns true if incoming wins, false if local wins (conflict preserved)
    bool reconcileFileUpdate(const std::string& groupName, const std::string& folderName,
                             const DfsrFileRecord& incoming, bool& outConflictCreated) {
        std::lock_guard<std::mutex> lock(m_mutex);
        outConflictCreated = false;
        std::string lowerRg = groupName;
        std::transform(lowerRg.begin(), lowerRg.end(), lowerRg.begin(), ::tolower);
        auto itRg = m_replicationGroups.find(lowerRg);
        if (itRg == m_replicationGroups.end()) return false;

        auto itRf = itRg->second.folders.find(folderName);
        if (itRf == itRg->second.folders.end()) return false;

        auto itLocal = itRf->second.files.find(incoming.relativePath);
        if (itLocal == itRf->second.files.end()) {
            // No local record: incoming simply accepted
            itRf->second.files[incoming.relativePath] = incoming;
            m_totalReplicatedFiles.fetch_add(1, std::memory_order_relaxed);
            m_totalBytesReplicated.fetch_add(incoming.fileSize, std::memory_order_relaxed);
            return true;
        }

        // Both have records: check GVSN and timestamps for conflict
        const auto& local = itLocal->second;
        if (local.gvsn.sequenceNumber == incoming.gvsn.sequenceNumber &&
            local.gvsn.dbGuid == incoming.gvsn.dbGuid) {
            // Identical version already present
            return true;
        }

        // Last-Writer-Wins (LWW) evaluation
        bool incomingWins = false;
        if (incoming.lastWriteTime > local.lastWriteTime) {
            incomingWins = true;
        } else if (incoming.lastWriteTime < local.lastWriteTime) {
            incomingWins = false;
        } else {
            // Timestamp tie: compare GUID lexicographically
            incomingWins = (incoming.gvsn.dbGuid > local.gvsn.dbGuid);
        }

        // Conflict record preserved in ConflictAndDeleted
        DfsrConflictRecord cr;
        cr.originalPath = incoming.relativePath;
        cr.timestamp = incomingWins ? local.lastWriteTime : incoming.lastWriteTime;
        cr.conflictReason = "Last-Writer-Wins Update Conflict";

        if (incomingWins) {
            cr.losingGvsn = local.gvsn;
            cr.winningGvsn = incoming.gvsn;
            cr.preservedFileName = incoming.relativePath + "-Conflict-" + local.gvsn.dbGuid.substr(1, 8);
            itRf->second.conflicts.push_back(cr);
            itRf->second.files[incoming.relativePath] = incoming;
        } else {
            cr.losingGvsn = incoming.gvsn;
            cr.winningGvsn = local.gvsn;
            cr.preservedFileName = incoming.relativePath + "-Conflict-" + incoming.gvsn.dbGuid.substr(1, 8);
            itRf->second.conflicts.push_back(cr);
        }

        outConflictCreated = true;
        m_totalConflictsResolved.fetch_add(1, std::memory_order_relaxed);
        return incomingWins;
    }

    // Replication Backlog Query
    size_t getBacklogCount(const std::string& groupName, const std::string& folderName,
                           const std::string& sourceMemberGuid, const std::string& destMemberGuid) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string lowerRg = groupName;
        std::transform(lowerRg.begin(), lowerRg.end(), lowerRg.begin(), ::tolower);
        auto itRg = m_replicationGroups.find(lowerRg);
        if (itRg == m_replicationGroups.end()) return 0;

        auto itRf = itRg->second.folders.find(folderName);
        if (itRf == itRg->second.folders.end()) return 0;

        size_t backlog = 0;
        for (const auto& [path, file] : itRf->second.files) {
            if (file.gvsn.dbGuid == sourceMemberGuid) {
                backlog++;
            }
        }
        return backlog;
    }

    // ========================================================================
    // Remote Differential Compression (RDC) Signature & Delta Computation
    // ========================================================================
    static std::vector<RdcChunkSignature> generateRdcSignatures(const uint8_t* data, size_t len,
                                                               uint32_t targetChunkSize = 8192) {
        std::vector<RdcChunkSignature> sigs;
        if (!data || len == 0) return sigs;

        size_t offset = 0;
        while (offset < len) {
            uint32_t chunkLen = static_cast<uint32_t>(std::min(static_cast<size_t>(targetChunkSize), len - offset));

            // Compute Adler-32 rolling hash
            uint32_t a = 1, b = 0;
            for (size_t i = 0; i < chunkLen; ++i) {
                a = (a + data[offset + i]) % 65521;
                b = (b + a) % 65521;
            }
            uint32_t adler = (b << 16) | a;

            // Simple hex digest of chunk
            std::stringstream ss;
            ss << std::hex << std::setfill('0');
            for (size_t i = 0; i < std::min<size_t>(8, chunkLen); ++i) {
                ss << std::setw(2) << static_cast<int>(data[offset + i]);
            }

            RdcChunkSignature sig;
            sig.offset = offset;
            sig.length = chunkLen;
            sig.hash32 = adler;
            sig.sha256 = ss.str();
            sigs.push_back(sig);

            offset += chunkLen;
        }
        return sigs;
    }

    static size_t calculateRdcCompressionRatio(const std::vector<RdcChunkSignature>& srcSigs,
                                               const std::vector<RdcChunkSignature>& dstSigs,
                                               size_t totalBytes) {
        if (totalBytes == 0) return 0;
        size_t matchingBytes = 0;
        for (const auto& s : srcSigs) {
            for (const auto& d : dstSigs) {
                if (s.hash32 == d.hash32 && s.length == d.length) {
                    matchingBytes += s.length;
                    break;
                }
            }
        }
        if (matchingBytes >= totalBytes) return 100;
        return (matchingBytes * 100) / totalBytes;
    }

    // ========================================================================
    // Live Telemetry Getters
    // ========================================================================
    uint64_t getTotalReferralRequests() const noexcept { return m_totalReferralRequests.load(std::memory_order_relaxed); }
    uint64_t getTotalCacheHits() const noexcept { return m_totalCacheHits.load(std::memory_order_relaxed); }
    uint64_t getTotalReplicatedFiles() const noexcept { return m_totalReplicatedFiles.load(std::memory_order_relaxed); }
    uint64_t getTotalBytesReplicated() const noexcept { return m_totalBytesReplicated.load(std::memory_order_relaxed); }
    uint64_t getTotalConflictsResolved() const noexcept { return m_totalConflictsResolved.load(std::memory_order_relaxed); }
    uint64_t getLastReferralLatencyUs() const noexcept { return m_lastReferralLatencyUs.load(std::memory_order_relaxed); }
};

} // namespace micant::dfs

#endif // MICANT_DFS_HPP
