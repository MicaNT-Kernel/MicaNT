#pragma once

/**
 * @file wsus_server.hpp
 * @brief Windows Server Update Services (WSUS 10.0 / SUSDB / TitanWSUS) Subsystem.
 *
 * Clean-room implementation of the Microsoft Windows Server Update Services engine,
 * featuring SCM service registration (WsusService), SUSDB repository management,
 * computer target groups, update catalog synchronization, approval workflow engine,
 * ClientWebService synchronization protocol, compliance reporting, IIS web app integration,
 * and Win32 C ABI exports.
 */

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <memory>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdint>
#include <functional>

#include "ntstatus.hpp"
#include "ntdef.hpp"
#include "scm.hpp"
#include "version.hpp"
#include "iis_server.hpp"

namespace micant::wsus {

enum class UpdateClassification : uint32_t {
    CriticalUpdates     = 1,
    SecurityUpdates     = 2,
    DefinitionUpdates   = 3,
    UpdateRollups       = 4,
    ServicePacks        = 5,
    FeaturePacks        = 6,
    Tools               = 7,
    Updates             = 8
};

inline const char* UpdateClassificationToString(UpdateClassification c) {
    switch (c) {
        case UpdateClassification::CriticalUpdates:   return "Critical Updates";
        case UpdateClassification::SecurityUpdates:   return "Security Updates";
        case UpdateClassification::DefinitionUpdates: return "Definition Updates";
        case UpdateClassification::UpdateRollups:     return "Update Rollups";
        case UpdateClassification::ServicePacks:      return "Service Packs";
        case UpdateClassification::FeaturePacks:      return "Feature Packs";
        case UpdateClassification::Tools:             return "Tools";
        case UpdateClassification::Updates:           return "Updates";
        default:                                      return "Unknown";
    }
}

enum class UpdateApprovalAction : uint32_t {
    Install    = 1,
    Uninstall  = 2,
    DetectOnly = 3,
    Decline    = 4
};

inline const char* UpdateApprovalActionToString(UpdateApprovalAction a) {
    switch (a) {
        case UpdateApprovalAction::Install:    return "Install";
        case UpdateApprovalAction::Uninstall:  return "Uninstall";
        case UpdateApprovalAction::DetectOnly: return "Detect Only";
        case UpdateApprovalAction::Decline:    return "Decline";
        default:                               return "Unknown";
    }
}

enum class UpdateInstallationState : uint32_t {
    NotApplicable   = 0,
    NotInstalled    = 1,
    Installed       = 2,
    Downloaded      = 3,
    Failed          = 4,
    PendingReboot   = 5
};

inline const char* UpdateInstallationStateToString(UpdateInstallationState s) {
    switch (s) {
        case UpdateInstallationState::NotApplicable: return "Not Applicable";
        case UpdateInstallationState::NotInstalled:  return "Needed / Not Installed";
        case UpdateInstallationState::Installed:     return "Installed";
        case UpdateInstallationState::Downloaded:    return "Downloaded";
        case UpdateInstallationState::Failed:        return "Failed";
        case UpdateInstallationState::PendingReboot: return "Pending Reboot";
        default:                                     return "Unknown";
    }
}

struct WsusUpdate {
    std::string updateId;                // UUID string
    std::string kbArticle;               // e.g. "KB5034441"
    std::string title;                   // e.g. "2026-10 Security Update for Windows Server (KB5034441)"
    std::string description;
    UpdateClassification classification{UpdateClassification::SecurityUpdates};
    uint64_t payloadSizeBytes{104857600}; // e.g. 100 MB
    std::string sha256Hash;
    std::string downloadUrl;
    bool isSuperseded{false};
    std::string supersededByUpdateId;
    std::string msrcSeverity{"Critical"};
    std::string releaseDate{"2026-10-01"};
    bool isApproved{false};
};

struct ComputerTargetGroup {
    std::string groupId;                 // UUID or well-known ID
    std::string groupName;               // e.g. "All Computers", "Servers", "Pilot Ring"
    std::string parentGroupId;
    std::vector<std::string> memberIds;  // computer IDs
};

struct ClientComputerTarget {
    std::string computerId;              // UUID
    std::string fqdn;                    // e.g. "TITAN-DC01.micant.internal"
    std::string ipAddress{"10.0.0.10"};
    std::string osVersion{"10.0.26100.1"};
    std::string targetGroupId{"GROUP-ALL-COMPUTERS"};
    uint64_t lastSyncTimeUs{0};
    uint64_t lastReportTimeUs{0};
    std::map<std::string, UpdateInstallationState> updateStates; // updateId -> state
};

struct UpdateApproval {
    std::string approvalId;
    std::string updateId;
    std::string targetGroupId;
    UpdateApprovalAction action{UpdateApprovalAction::Install};
    std::string administrator{"Administrator"};
    std::string timestamp;
};

class EnterpriseWsusServer {
private:
    mutable std::recursive_mutex m_mutex;
    bool m_initialized{false};

    // SUSDB Storage
    std::unordered_map<std::string, WsusUpdate> m_updates;                // updateId -> update
    std::unordered_map<std::string, ComputerTargetGroup> m_targetGroups;   // groupId -> group
    std::unordered_map<std::string, ClientComputerTarget> m_clients;       // computerId -> client
    std::vector<UpdateApproval> m_approvals;                               // active approvals

    // Telemetry & Metrics
    mutable std::atomic<uint64_t> m_totalSyncAttempts{0};
    mutable std::atomic<uint64_t> m_totalSyncSuccesses{0};
    mutable std::atomic<uint64_t> m_totalClientSyncRequests{0};
    mutable std::atomic<uint64_t> m_totalStatusReportsReceived{0};
    mutable std::atomic<uint64_t> m_totalApprovalsExecuted{0};
    mutable std::atomic<uint64_t> m_totalPayloadBytesServed{0};

    // Upstream configuration
    std::string m_upstreamServer{"https://update.microsoft.com/v6"};
    uint16_t m_httpPort{8530};
    uint16_t m_httpsPort{8531};
    std::string m_contentDirectory{"C:\\WSUS\\WsusContent"};

public:
    static EnterpriseWsusServer& instance() {
        static EnterpriseWsusServer inst;
        return inst;
    }

    EnterpriseWsusServer() {
        initialize();
    }

    void initialize() {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_initialized) return;

        seedDefaultTargetGroups();
        seedDefaultCatalog();
        registerScmServices();
        registerVersionDatabase();
        integrateWithIis();

        m_initialized = true;
    }

    void seedDefaultTargetGroups() {
        ComputerTargetGroup gAll;
        gAll.groupId = "GROUP-ALL-COMPUTERS";
        gAll.groupName = "All Computers";
        m_targetGroups[gAll.groupId] = gAll;

        ComputerTargetGroup gUnassigned;
        gUnassigned.groupId = "GROUP-UNASSIGNED";
        gUnassigned.groupName = "Unassigned Computers";
        gUnassigned.parentGroupId = "GROUP-ALL-COMPUTERS";
        m_targetGroups[gUnassigned.groupId] = gUnassigned;

        ComputerTargetGroup gServers;
        gServers.groupId = "GROUP-SERVERS";
        gServers.groupName = "Production Servers";
        gServers.parentGroupId = "GROUP-ALL-COMPUTERS";
        m_targetGroups[gServers.groupId] = gServers;

        ComputerTargetGroup gPilot;
        gPilot.groupId = "GROUP-PILOT";
        gPilot.groupName = "Pilot Ring (Fast)";
        gPilot.parentGroupId = "GROUP-ALL-COMPUTERS";
        m_targetGroups[gPilot.groupId] = gPilot;
    }

    void seedDefaultCatalog() {
        // 1. Critical Security Update
        WsusUpdate u1;
        u1.updateId = "9F818C52-79F8-4D2A-98C0-745BD3BC6E21";
        u1.kbArticle = "KB5044284";
        u1.title = "2026-10 Cumulative Update for Windows Server 2025 x64-based Systems (KB5044284)";
        u1.description = "Resolves critical remote code execution vulnerabilities in kernel executive and network stack.";
        u1.classification = UpdateClassification::SecurityUpdates;
        u1.payloadSizeBytes = 524288000; // 500 MB
        u1.sha256Hash = "8d3e91f0a1c3b5d7e9f2a4c6e8b0d2f4a6c8e0b2d4f6a8c0e2b4d6f8a0c2e4b6";
        u1.downloadUrl = "http://titan-wsus01.micant.internal:8530/Content/8D/8D3E91F0A1C3B5D7.cab";
        u1.msrcSeverity = "Critical";
        u1.releaseDate = "2026-10-06";
        u1.isApproved = true;
        m_updates[u1.updateId] = u1;

        // Auto-approve u1 for All Computers
        UpdateApproval a1;
        a1.approvalId = "APPR-0001";
        a1.updateId = u1.updateId;
        a1.targetGroupId = "GROUP-ALL-COMPUTERS";
        a1.action = UpdateApprovalAction::Install;
        a1.administrator = "SYSTEM";
        a1.timestamp = "2026-10-06T10:00:00Z";
        m_approvals.push_back(a1);

        // 2. Definition Update (Antimalware / SentinelDefender)
        WsusUpdate u2;
        u2.updateId = "3C25D94B-11F2-438B-B5C4-E1D879F4B301";
        u2.kbArticle = "KB2267602";
        u2.title = "Security Intelligence Update for Microsoft Defender Antivirus (KB2267602)";
        u2.description = "Daily definition update version 1.419.82.0 for antimalware scanning engine.";
        u2.classification = UpdateClassification::DefinitionUpdates;
        u2.payloadSizeBytes = 83886080; // 80 MB
        u2.sha256Hash = "a1b2c3d4e5f60718293a4b5c6d7e8f9a0b1c2d3e4f5a6b7c8d9e0f1a2b3c4d5e";
        u2.downloadUrl = "http://titan-wsus01.micant.internal:8530/Content/A1/A1B2C3D4E5F60718.cab";
        u2.msrcSeverity = "Important";
        u2.releaseDate = "2026-10-08";
        u2.isApproved = true;
        m_updates[u2.updateId] = u2;

        UpdateApproval a2;
        a2.approvalId = "APPR-0002";
        a2.updateId = u2.updateId;
        a2.targetGroupId = "GROUP-ALL-COMPUTERS";
        a2.action = UpdateApprovalAction::Install;
        a2.administrator = "AutoApprovalRule";
        a2.timestamp = "2026-10-08T06:00:00Z";
        m_approvals.push_back(a2);

        // 3. Superseded Update
        WsusUpdate u3;
        u3.updateId = "1A09E5B7-50A4-48FE-98D7-564AC7C48D2A";
        u3.kbArticle = "KB5043080";
        u3.title = "2026-09 Cumulative Update for Windows Server 2025 (KB5043080)";
        u3.description = "September cumulative update (superseded by KB5044284).";
        u3.classification = UpdateClassification::SecurityUpdates;
        u3.payloadSizeBytes = 512000000;
        u3.sha256Hash = "c0d1e2f3a4b5c6d7e8f9a0b1c2d3e4f5a6b7c8d9e0f1a2b3c4d5e6f7a8b9c0d1";
        u3.downloadUrl = "http://titan-wsus01.micant.internal:8530/Content/C0/C0D1E2F3A4B5C6D7.cab";
        u3.msrcSeverity = "Critical";
        u3.releaseDate = "2026-09-08";
        u3.isSuperseded = true;
        u3.supersededByUpdateId = u1.updateId;
        u3.isApproved = false;
        m_updates[u3.updateId] = u3;
    }

    void registerScmServices() {
        auto& scm = micant::scm::ServiceControlManager::get();

        auto recWsus = std::make_shared<micant::scm::ServiceRecord>();
        recWsus->serviceName = L"WsusService";
        recWsus->displayName = L"WSUS Service";
        recWsus->serviceType = micant::scm::SERVICE_WIN32_OWN_PROCESS;
        recWsus->startType = micant::scm::SERVICE_AUTO_START;
        recWsus->errorControl = micant::scm::SERVICE_ERROR_NORMAL;
        recWsus->binaryPath = L"C:\\Program Files\\Update Services\\Services\\wsusservice.exe";
        recWsus->status.dwCurrentState = micant::scm::SERVICE_RUNNING;
        scm.registerServiceRecord(recWsus);
    }

    void registerVersionDatabase() {
        auto& db = micant::version::VersionDatabase::Instance();
        db.RegisterModule("wsusservice.exe", "10.0.26100.1", "WSUS Core Service");
        db.RegisterModule("wsusutil.exe", "10.0.26100.1", "WSUS Administrative Utility");
        db.RegisterModule("susdb.dll", "10.0.26100.1", "WSUS Database Access Provider");
        db.RegisterModule("wuaueng.dll", "10.0.26100.1", "Windows Update Agent Engine");
        db.RegisterModule("microsoft.updateservices.administration.dll", "10.0.26100.1", "WSUS Managed Management Assembly");
    }

    void integrateWithIis() {
        // Configure WsusPool application pool and WSUS administration site in IIS
        auto& iis = micant::iis::EnterpriseWebServer::instance();
        iis.createAppPool("WsusPool", micant::iis::ManagedPipelineMode::Integrated);
        iis.createSite(8530, "WSUS Administration", "WsusPool", "C:\\Program Files\\Update Services\\WebServices");
        iis.addBinding(8530, micant::iis::ProtocolType::Http, "*", m_httpPort, "");
        iis.addBinding(8530, micant::iis::ProtocolType::Https, "*", m_httpsPort, "titan-wsus01.micant.internal");
    }

    // --- Computer Target & Registration API ---

    bool registerClient(const std::string& fqdn, const std::string& ip, const std::string& osVer, std::string& outComputerId) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        
        // Find existing or assign new ID
        for (const auto& [id, client] : m_clients) {
            if (client.fqdn == fqdn) {
                outComputerId = id;
                auto& c = m_clients[id];
                c.ipAddress = ip;
                c.osVersion = osVer;
                c.lastSyncTimeUs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
                return true;
            }
        }

        std::stringstream ss;
        ss << "CLIENT-" << std::setw(4) << std::setfill('0') << (m_clients.size() + 1);
        outComputerId = ss.str();

        ClientComputerTarget c;
        c.computerId = outComputerId;
        c.fqdn = fqdn;
        c.ipAddress = ip;
        c.osVersion = osVer;
        c.targetGroupId = "GROUP-ALL-COMPUTERS";
        c.lastSyncTimeUs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        c.lastReportTimeUs = c.lastSyncTimeUs;

        // Populate initial update states from catalog
        for (const auto& [upId, up] : m_updates) {
            c.updateStates[upId] = up.isSuperseded ? UpdateInstallationState::NotApplicable : UpdateInstallationState::NotInstalled;
        }

        m_clients[outComputerId] = c;
        m_targetGroups["GROUP-ALL-COMPUTERS"].memberIds.push_back(outComputerId);
        return true;
    }

    bool assignComputerToGroup(const std::string& computerId, const std::string& groupId) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto itC = m_clients.find(computerId);
        auto itG = m_targetGroups.find(groupId);
        if (itC == m_clients.end() || itG == m_targetGroups.end()) {
            return false;
        }

        // Remove from old group members list
        std::string oldGrp = itC->second.targetGroupId;
        auto& oldMembers = m_targetGroups[oldGrp].memberIds;
        oldMembers.erase(std::remove(oldMembers.begin(), oldMembers.end(), computerId), oldMembers.end());

        // Assign to new group
        itC->second.targetGroupId = groupId;
        itG->second.memberIds.push_back(computerId);
        return true;
    }

    // --- Update Approval & Catalog Management ---

    bool approveUpdate(const std::string& updateId, const std::string& targetGroupId, UpdateApprovalAction action, const std::string& adminUser) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto itU = m_updates.find(updateId);
        auto itG = m_targetGroups.find(targetGroupId);
        if (itU == m_updates.end() || itG == m_targetGroups.end()) {
            return false;
        }

        itU->second.isApproved = (action != UpdateApprovalAction::Decline);

        UpdateApproval appr;
        appr.approvalId = "APPR-" + std::to_string(m_approvals.size() + 1);
        appr.updateId = updateId;
        appr.targetGroupId = targetGroupId;
        appr.action = action;
        appr.administrator = adminUser;
        appr.timestamp = "2026-10-08T17:00:00Z";
        m_approvals.push_back(appr);

        m_totalApprovalsExecuted.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    bool declineUpdate(const std::string& updateId, const std::string& adminUser) {
        return approveUpdate(updateId, "GROUP-ALL-COMPUTERS", UpdateApprovalAction::Decline, adminUser);
    }

    bool syncCatalog(uint32_t simulatedNewUpdatesCount, uint32_t* pImportedCount) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_totalSyncAttempts.fetch_add(1, std::memory_order_relaxed);

        uint32_t added = 0;
        for (uint32_t i = 0; i < simulatedNewUpdatesCount; ++i) {
            std::stringstream ssId, ssKb;
            ssId << "SYNC-" << std::setw(4) << std::setfill('0') << (m_updates.size() + 1);
            ssKb << "KB" << (5045000 + m_updates.size());

            WsusUpdate u;
            u.updateId = ssId.str();
            u.kbArticle = ssKb.str();
            u.title = "Security Patch " + u.kbArticle + " for Windows Server 2025";
            u.description = "Synchronized patch from upstream Microsoft Update catalog.";
            u.classification = (i % 2 == 0) ? UpdateClassification::SecurityUpdates : UpdateClassification::CriticalUpdates;
            u.payloadSizeBytes = 41943040 + i * 1048576;
            u.sha256Hash = "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";
            u.downloadUrl = "http://titan-wsus01.micant.internal:8530/Content/" + u.updateId + ".cab";
            u.isApproved = false;
            m_updates[u.updateId] = u;
            added++;
        }

        if (pImportedCount) *pImportedCount = added;
        m_totalSyncSuccesses.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    // --- Client Sync & Reporting Web Service ---

    struct ClientSyncResult {
        std::vector<WsusUpdate> applicableUpdates;
        std::vector<std::string> declinedUpdates;
        uint32_t totalApprovedCount{0};
    };

    ClientSyncResult syncUpdatesForClient(const std::string& computerId) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_totalClientSyncRequests.fetch_add(1, std::memory_order_relaxed);

        ClientSyncResult res;
        auto itC = m_clients.find(computerId);
        if (itC == m_clients.end()) return res;

        std::string grpId = itC->second.targetGroupId;

        for (const auto& [uId, update] : m_updates) {
            if (update.isSuperseded) continue;

            // Check if approved for client's group or All Computers
            bool approvedForClient = false;
            for (const auto& appr : m_approvals) {
                if (appr.updateId == uId && (appr.targetGroupId == grpId || appr.targetGroupId == "GROUP-ALL-COMPUTERS")) {
                    if (appr.action == UpdateApprovalAction::Install) {
                        approvedForClient = true;
                    } else if (appr.action == UpdateApprovalAction::Decline) {
                        res.declinedUpdates.push_back(uId);
                    }
                }
            }

            if (approvedForClient) {
                res.totalApprovedCount++;
                auto stateIt = itC->second.updateStates.find(uId);
                if (stateIt == itC->second.updateStates.end() || 
                    stateIt->second == UpdateInstallationState::NotInstalled || 
                    stateIt->second == UpdateInstallationState::Failed) {
                    res.applicableUpdates.push_back(update);
                }
            }
        }

        itC->second.lastSyncTimeUs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        return res;
    }

    bool reportClientStatus(const std::string& computerId, const std::string& updateId, UpdateInstallationState state) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto itC = m_clients.find(computerId);
        if (itC == m_clients.end()) return false;

        itC->second.updateStates[updateId] = state;
        itC->second.lastReportTimeUs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        m_totalStatusReportsReceived.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    // --- Metrics & Status Inspection ---

    double calculateOverallComplianceRate() const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_clients.empty()) return 100.0;

        uint64_t totalEvaluations = 0;
        uint64_t compliantEvaluations = 0;

        for (const auto& [cId, client] : m_clients) {
            for (const auto& [uId, up] : m_updates) {
                if (up.isSuperseded || !up.isApproved) continue;

                totalEvaluations++;
                auto itSt = client.updateStates.find(uId);
                if (itSt != client.updateStates.end() && itSt->second == UpdateInstallationState::Installed) {
                    compliantEvaluations++;
                }
            }
        }

        if (totalEvaluations == 0) return 100.0;
        return (static_cast<double>(compliantEvaluations) / static_cast<double>(totalEvaluations)) * 100.0;
    }

    size_t getUpdateCount() const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_updates.size();
    }

    size_t getClientCount() const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_clients.size();
    }

    size_t getTargetGroupCount() const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_targetGroups.size();
    }

    size_t getApprovalCount() const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_approvals.size();
    }

    std::vector<WsusUpdate> getAllUpdates() const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        std::vector<WsusUpdate> list;
        list.reserve(m_updates.size());
        for (const auto& [_, u] : m_updates) list.push_back(u);
        return list;
    }

    std::vector<ClientComputerTarget> getAllClients() const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        std::vector<ClientComputerTarget> list;
        list.reserve(m_clients.size());
        for (const auto& [_, c] : m_clients) list.push_back(c);
        return list;
    }

    std::vector<ComputerTargetGroup> getAllGroups() const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        std::vector<ComputerTargetGroup> list;
        list.reserve(m_targetGroups.size());
        for (const auto& [_, g] : m_targetGroups) list.push_back(g);
        return list;
    }

    uint64_t getTotalSyncAttempts() const { return m_totalSyncAttempts.load(); }
    uint64_t getTotalClientSyncRequests() const { return m_totalClientSyncRequests.load(); }
    uint64_t getTotalStatusReportsReceived() const { return m_totalStatusReportsReceived.load(); }
    uint64_t getTotalApprovalsExecuted() const { return m_totalApprovalsExecuted.load(); }
    uint16_t getHttpPort() const { return m_httpPort; }
    uint16_t getHttpsPort() const { return m_httpsPort; }
    const std::string& getUpstreamServer() const { return m_upstreamServer; }
};

} // namespace micant::wsus

// ----------------------------------------------------------------------------
// Win32 C ABI Exports (MicaWsus*)
// ----------------------------------------------------------------------------
extern "C" {

inline micant::NTSTATUS MicaWsusInitialize() {
    micant::wsus::EnterpriseWsusServer::instance().initialize();
    return micant::STATUS_SUCCESS;
}

inline micant::NTSTATUS MicaWsusRegisterComputer(const char* fqdn, const char* ip, const char* osVer, char* outComputerId, uint32_t bufSize) {
    if (!fqdn || !outComputerId || bufSize == 0) return micant::STATUS_INVALID_PARAMETER;
    std::string cid;
    if (micant::wsus::EnterpriseWsusServer::instance().registerClient(fqdn, ip ? ip : "10.0.0.1", osVer ? osVer : "10.0.26100.1", cid)) {
        if (cid.size() + 1 > bufSize) return micant::STATUS_BUFFER_TOO_SMALL;
        std::memcpy(outComputerId, cid.c_str(), cid.size() + 1);
        return micant::STATUS_SUCCESS;
    }
    return micant::STATUS_UNSUCCESSFUL;
}

inline micant::NTSTATUS MicaWsusApproveUpdate(const char* updateId, const char* targetGroupId, uint32_t action, const char* admin) {
    if (!updateId || !targetGroupId) return micant::STATUS_INVALID_PARAMETER;
    bool ok = micant::wsus::EnterpriseWsusServer::instance().approveUpdate(
        updateId, targetGroupId, static_cast<micant::wsus::UpdateApprovalAction>(action), admin ? admin : "Administrator");
    return ok ? micant::STATUS_SUCCESS : micant::STATUS_NOT_FOUND;
}

inline micant::NTSTATUS MicaWsusSyncCatalog(uint32_t newUpdatesToSimulate, uint32_t* pImportedCount) {
    bool ok = micant::wsus::EnterpriseWsusServer::instance().syncCatalog(newUpdatesToSimulate, pImportedCount);
    return ok ? micant::STATUS_SUCCESS : micant::STATUS_UNSUCCESSFUL;
}

inline double MicaWsusGetComplianceRate() {
    return micant::wsus::EnterpriseWsusServer::instance().calculateOverallComplianceRate();
}

inline micant::NTSTATUS MicaWsusShutdown() {
    return micant::STATUS_SUCCESS;
}

} // extern "C"
