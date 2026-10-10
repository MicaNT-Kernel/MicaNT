#pragma once

#include <cstdint>
#include "ntstatus.hpp"
#include "scm.hpp"
#include "version.hpp"

#include <string>
#include <vector>
#include <map>
#include <deque>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace micant::wsrm {

// ============================================================================
// Windows System Resource Manager (WSRM) Enums & Constants
// ============================================================================

enum class AllocationPolicyType : int32_t {
    EqualPerProcess = 0, // Divides CPU equally among all active processes
    EqualPerUser    = 1, // Divides CPU equally among all logged-on users
    EqualPerSession = 2, // Dynamic Fair Share Scheduling (DFSS) for Terminal Services
    CustomWeighted  = 3  // User-defined Process Matching Criteria & weight allocation
};

inline const char* AllocationPolicyTypeToString(AllocationPolicyType t) {
    switch (t) {
        case AllocationPolicyType::EqualPerProcess: return "Equal_Per_Process";
        case AllocationPolicyType::EqualPerUser:    return "Equal_Per_User";
        case AllocationPolicyType::EqualPerSession: return "Equal_Per_Session (DFSS)";
        case AllocationPolicyType::CustomWeighted:  return "Custom_Weighted";
        default: return "Unknown";
    }
}

// Job Object CPU Rate Control Information Flags (Windows Parity)
constexpr uint32_t JOBOBJECT_CPU_RATE_CONTROL_ENABLE         = 0x00000001;
constexpr uint32_t JOBOBJECT_CPU_RATE_CONTROL_WEIGHT_BASED     = 0x00000002;
constexpr uint32_t JOBOBJECT_CPU_RATE_CONTROL_HARD_CAP         = 0x00000004;
constexpr uint32_t JOBOBJECT_CPU_RATE_CONTROL_NOTIFY           = 0x00000008;
constexpr uint32_t JOBOBJECT_CPU_RATE_CONTROL_MIN_MAX_RATE     = 0x00000010;

struct JOBOBJECT_CPU_RATE_CONTROL_INFORMATION {
    uint32_t ControlFlags{0};
    uint32_t CpuRate{0}; // Rate in percentage * 100 (e.g. 5000 = 50.00%) or weight 1-9
};

// ============================================================================
// Process Matching Criteria (PMC)
// ============================================================================
struct ProcessMatchingCriteria {
    std::string criteriaName;
    std::string appPattern{"*"};      // Executable image name pattern (e.g. "sqlservr.exe", "w3wp.exe", "*")
    std::string userOrGroup{"*"};     // User or Group account name
    int32_t     sessionId{-1};        // -1 for all sessions, or specific Session ID (0=Services, 1=Console, 2+=RDP)
    uint32_t    targetCpuPercent{25}; // Target CPU rate percentage (1-100)
    bool        hardCap{true};        // Hard rate cap vs weight-based burstable
    uint64_t    memoryLimitBytes{0};  // 0 = unlimited
    uint64_t    minWorkingSetBytes{0};
    uint64_t    maxWorkingSetBytes{0};
    uint32_t    weight{5};            // Weight 1 to 9 for weight-based scheduling
};

// ============================================================================
// Managed Process Record & Telemetry
// ============================================================================
struct ManagedProcessRecord {
    uint32_t    processId{0};
    std::string processName;
    std::string userName;
    uint32_t    sessionId{0};
    std::string matchedCriteria;
    double      allocatedCpuPercent{0.0};
    uint32_t    rateControlFlags{0};
    uint64_t    memoryLimitBytes{0};
    uint64_t    userTimeUs{0};
    uint64_t    kernelTimeUs{0};
    uint64_t    peakWorkingSetBytes{0};
    uint64_t    readOperationCount{0};
    uint64_t    writeOperationCount{0};
    uint64_t    readTransferBytes{0};
    uint64_t    writeTransferBytes{0};
    uint64_t    throttlingEvents{0};
    bool        active{true};
};

// ============================================================================
// Historical Resource Accounting Record
// ============================================================================
struct AccountingRecord {
    uint64_t    timestampUs{0};
    std::string tenantName; // User, Session, or Application identifier
    std::string processName;
    uint32_t    processId{0};
    uint32_t    sessionId{0};
    uint64_t    cpuTimeUs{0};
    uint64_t    peakWorkingSetBytes{0};
    uint64_t    totalIoBytes{0};
    uint64_t    throttlingEvents{0};
};

// ============================================================================
// SystemResourceManager: Core Engine Singleton
// ============================================================================
class SystemResourceManager {
private:
    mutable std::mutex m_mutex;
    bool m_initialized{false};

    AllocationPolicyType m_activePolicy{AllocationPolicyType::EqualPerProcess};
    std::map<std::string, ProcessMatchingCriteria> m_criteria; // CriteriaName -> Criteria
    std::map<uint32_t, ManagedProcessRecord> m_managedProcesses; // PID -> Record
    std::deque<AccountingRecord> m_accountingLog;

    // Metrics
    std::atomic<uint64_t> m_totalThrottlingEvents{0};
    std::atomic<uint64_t> m_rebalanceCycles{0};
    std::atomic<uint64_t> m_totalRegisteredProcesses{0};

    SystemResourceManager() {
        initialize();
    }

public:
    static SystemResourceManager& instance() {
        static SystemResourceManager s_instance;
        return s_instance;
    }

    SystemResourceManager(const SystemResourceManager&) = delete;
    SystemResourceManager& operator=(const SystemResourceManager&) = delete;

    void initialize() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_initialized) return;

        // Register default built-in Process Matching Criteria
        ProcessMatchingCriteria pmcSql{};
        pmcSql.criteriaName = "SQLServer_Workload";
        pmcSql.appPattern = "sqlservr.exe";
        pmcSql.userOrGroup = "*";
        pmcSql.sessionId = -1;
        pmcSql.targetCpuPercent = 40;
        pmcSql.hardCap = true;
        pmcSql.memoryLimitBytes = 4ULL * 1024 * 1024 * 1024; // 4 GB cap
        pmcSql.weight = 8;
        m_criteria[pmcSql.criteriaName] = pmcSql;

        ProcessMatchingCriteria pmcIis{};
        pmcIis.criteriaName = "IIS_Worker_Pool";
        pmcIis.appPattern = "w3wp.exe";
        pmcIis.userOrGroup = "*";
        pmcIis.sessionId = -1;
        pmcIis.targetCpuPercent = 30;
        pmcIis.hardCap = true;
        pmcIis.memoryLimitBytes = 2ULL * 1024 * 1024 * 1024; // 2 GB cap
        pmcIis.weight = 6;
        m_criteria[pmcIis.criteriaName] = pmcIis;

        ProcessMatchingCriteria pmcDev{};
        pmcDev.criteriaName = "Developer_Build_Tasks";
        pmcDev.appPattern = "clang*.exe";
        pmcDev.userOrGroup = "Developers";
        pmcDev.sessionId = -1;
        pmcDev.targetCpuPercent = 20;
        pmcDev.hardCap = false;
        pmcDev.weight = 5;
        m_criteria[pmcDev.criteriaName] = pmcDev;

        // SCM & VersionDatabase registration
        registerScmServices();
        registerVersionDatabase();

        m_initialized = true;
    }

    void registerScmServices() {
        auto& scm = micant::scm::ServiceControlManager::get();

        // 1. WsrmService (Windows System Resource Manager)
        auto recWsrm = std::make_shared<micant::scm::ServiceRecord>();
        recWsrm->serviceName = L"WsrmService";
        recWsrm->displayName = L"Windows System Resource Manager";
        recWsrm->serviceType = micant::scm::SERVICE_WIN32_OWN_PROCESS;
        recWsrm->startType = micant::scm::SERVICE_AUTO_START;
        recWsrm->errorControl = micant::scm::SERVICE_ERROR_NORMAL;
        recWsrm->binaryPath = L"C:\\Windows\\System32\\wsrm.exe";
        recWsrm->status.dwServiceType = micant::scm::SERVICE_WIN32_OWN_PROCESS;
        recWsrm->status.dwCurrentState = micant::scm::SERVICE_RUNNING;
        recWsrm->status.dwControlsAccepted = micant::scm::SERVICE_ACCEPT_STOP | micant::scm::SERVICE_ACCEPT_SHUTDOWN;
        scm.registerServiceRecord(recWsrm);

        // 2. TitanQuota (Kernel Resource Quota Driver)
        auto recDrv = std::make_shared<micant::scm::ServiceRecord>();
        recDrv->serviceName = L"TitanQuota";
        recDrv->displayName = L"Titan Kernel Resource Quota Driver";
        recDrv->serviceType = micant::scm::SERVICE_KERNEL_DRIVER;
        recDrv->startType = micant::scm::SERVICE_DEMAND_START;
        recDrv->errorControl = micant::scm::SERVICE_ERROR_NORMAL;
        recDrv->binaryPath = L"C:\\Windows\\System32\\drivers\\wsrmcore.sys";
        recDrv->status.dwServiceType = micant::scm::SERVICE_KERNEL_DRIVER;
        recDrv->status.dwCurrentState = micant::scm::SERVICE_RUNNING;
        scm.registerServiceRecord(recDrv);
    }

    void registerVersionDatabase() {
        auto& db = micant::version::VersionDatabase::Instance();
        db.RegisterModule("wsrm.exe", "10.0.26100.1", "Windows System Resource Manager Service Executable");
        db.RegisterModule("wsrmcore.dll", "10.0.26100.1", "System Resource Manager Core Engine");
        db.RegisterModule("wsrm.msc", "10.0.26100.1", "Windows System Resource Manager Management Console");
        db.RegisterModule("wsrmcore.sys", "10.0.26100.1", "Kernel Resource Quota Driver");
    }

    // ========================================================================
    // Policy Management
    // ========================================================================

    void setAllocationPolicy(AllocationPolicyType type) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_activePolicy = type;
        rebalanceInternal();
    }

    AllocationPolicyType getAllocationPolicy() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_activePolicy;
    }

    bool addMatchingCriteria(const ProcessMatchingCriteria& criteria) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_criteria[criteria.criteriaName] = criteria;
        if (m_activePolicy == AllocationPolicyType::CustomWeighted) {
            rebalanceInternal();
        }
        return true;
    }

    bool getMatchingCriteria(const std::string& name, ProcessMatchingCriteria* outCriteria) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_criteria.find(name);
        if (it == m_criteria.end()) return false;
        if (outCriteria) *outCriteria = it->second;
        return true;
    }

    std::vector<ProcessMatchingCriteria> getAllCriteria() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<ProcessMatchingCriteria> list;
        for (const auto& [name, c] : m_criteria) {
            list.push_back(c);
        }
        return list;
    }

    // ========================================================================
    // Process Registration & Management
    // ========================================================================

    bool registerProcess(uint32_t pid, const std::string& processName,
                         const std::string& userName, uint32_t sessionId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        ManagedProcessRecord rec{};
        rec.processId = pid;
        rec.processName = processName;
        rec.userName = userName;
        rec.sessionId = sessionId;
        rec.active = true;

        m_managedProcesses[pid] = rec;
        m_totalRegisteredProcesses.fetch_add(1, std::memory_order_relaxed);

        rebalanceInternal();
        return true;
    }

    bool deregisterProcess(uint32_t pid) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_managedProcesses.find(pid);
        if (it == m_managedProcesses.end()) return false;

        // Commit final accounting log entry
        AccountingRecord acct{};
        acct.timestampUs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        acct.tenantName = it->second.userName;
        acct.processName = it->second.processName;
        acct.processId = it->second.processId;
        acct.sessionId = it->second.sessionId;
        acct.cpuTimeUs = it->second.userTimeUs + it->second.kernelTimeUs;
        acct.peakWorkingSetBytes = it->second.peakWorkingSetBytes;
        acct.totalIoBytes = it->second.readTransferBytes + it->second.writeTransferBytes;
        acct.throttlingEvents = it->second.throttlingEvents;

        m_accountingLog.push_back(acct);
        if (m_accountingLog.size() > 2000) {
            m_accountingLog.pop_front();
        }

        m_managedProcesses.erase(it);
        rebalanceInternal();
        return true;
    }

    bool updateProcessTelemetry(uint32_t pid, uint64_t userTimeUs, uint64_t kernelTimeUs,
                                uint64_t peakMemBytes, uint64_t ioBytes, bool throttled = false) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_managedProcesses.find(pid);
        if (it == m_managedProcesses.end()) return false;

        it->second.userTimeUs += userTimeUs;
        it->second.kernelTimeUs += kernelTimeUs;
        if (peakMemBytes > it->second.peakWorkingSetBytes) {
            it->second.peakWorkingSetBytes = peakMemBytes;
        }
        it->second.writeTransferBytes += ioBytes;
        if (throttled) {
            it->second.throttlingEvents++;
            m_totalThrottlingEvents.fetch_add(1, std::memory_order_relaxed);
        }
        return true;
    }

    bool getProcessAccounting(uint32_t pid, ManagedProcessRecord* outRecord) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_managedProcesses.find(pid);
        if (it == m_managedProcesses.end()) return false;
        if (outRecord) *outRecord = it->second;
        return true;
    }

    std::vector<ManagedProcessRecord> getAllManagedProcesses() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<ManagedProcessRecord> list;
        for (const auto& [pid, p] : m_managedProcesses) {
            list.push_back(p);
        }
        return list;
    }

    // ========================================================================
    // Dynamic Fair Share Scheduling (DFSS) & Resource Rebalancer
    // ========================================================================

    void applyFairShare(uint32_t activeSessionCount) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_activePolicy = AllocationPolicyType::EqualPerSession;

        if (activeSessionCount == 0) activeSessionCount = 1;
        double targetPerSession = 100.0 / static_cast<double>(activeSessionCount);

        for (auto& [pid, proc] : m_managedProcesses) {
            proc.allocatedCpuPercent = targetPerSession;
            proc.rateControlFlags = JOBOBJECT_CPU_RATE_CONTROL_ENABLE | JOBOBJECT_CPU_RATE_CONTROL_HARD_CAP;
            proc.matchedCriteria = "DFSS (Session " + std::to_string(proc.sessionId) + ")";
        }
        m_rebalanceCycles.fetch_add(1, std::memory_order_relaxed);
    }

    void rebalanceResources() {
        std::lock_guard<std::mutex> lock(m_mutex);
        rebalanceInternal();
    }

    // ========================================================================
    // Historical Accounting & Querying
    // ========================================================================

    bool getTenantAccounting(const std::string& tenantOrApp, uint64_t* pCpuUs,
                             uint64_t* pPeakMemBytes, uint64_t* pIoBytes) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        uint64_t totalCpu = 0;
        uint64_t maxMem = 0;
        uint64_t totalIo = 0;
        bool found = false;

        // Check active processes
        for (const auto& [pid, proc] : m_managedProcesses) {
            if (proc.userName == tenantOrApp || proc.processName == tenantOrApp) {
                totalCpu += (proc.userTimeUs + proc.kernelTimeUs);
                if (proc.peakWorkingSetBytes > maxMem) maxMem = proc.peakWorkingSetBytes;
                totalIo += (proc.readTransferBytes + proc.writeTransferBytes);
                found = true;
            }
        }

        // Check historical log
        for (const auto& entry : m_accountingLog) {
            if (entry.tenantName == tenantOrApp || entry.processName == tenantOrApp) {
                totalCpu += entry.cpuTimeUs;
                if (entry.peakWorkingSetBytes > maxMem) maxMem = entry.peakWorkingSetBytes;
                totalIo += entry.totalIoBytes;
                found = true;
            }
        }

        if (pCpuUs) *pCpuUs = totalCpu;
        if (pPeakMemBytes) *pPeakMemBytes = maxMem;
        if (pIoBytes) *pIoBytes = totalIo;
        return found;
    }

    std::vector<AccountingRecord> getAccountingHistory() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return std::vector<AccountingRecord>(m_accountingLog.begin(), m_accountingLog.end());
    }

    // Metrics getters
    uint64_t getTotalThrottlingEvents() const { return m_totalThrottlingEvents.load(std::memory_order_relaxed); }
    uint64_t getRebalanceCycles() const { return m_rebalanceCycles.load(std::memory_order_relaxed); }
    uint64_t getTotalRegisteredProcesses() const { return m_totalRegisteredProcesses.load(std::memory_order_relaxed); }

private:
    void rebalanceInternal() {
        m_rebalanceCycles.fetch_add(1, std::memory_order_relaxed);

        if (m_managedProcesses.empty()) return;

        if (m_activePolicy == AllocationPolicyType::EqualPerProcess) {
            double share = 100.0 / static_cast<double>(m_managedProcesses.size());
            for (auto& [pid, proc] : m_managedProcesses) {
                proc.allocatedCpuPercent = share;
                proc.rateControlFlags = JOBOBJECT_CPU_RATE_CONTROL_ENABLE | JOBOBJECT_CPU_RATE_CONTROL_HARD_CAP;
                proc.matchedCriteria = "Equal_Per_Process";
                proc.memoryLimitBytes = 0; // unlimited
            }
        } else if (m_activePolicy == AllocationPolicyType::EqualPerUser) {
            // Count distinct users
            std::map<std::string, std::vector<uint32_t>> users;
            for (const auto& [pid, proc] : m_managedProcesses) {
                users[proc.userName].push_back(pid);
            }
            if (!users.empty()) {
                double perUserShare = 100.0 / static_cast<double>(users.size());
                for (const auto& [user, pids] : users) {
                    double perProc = perUserShare / static_cast<double>(pids.size());
                    for (uint32_t pid : pids) {
                        auto& p = m_managedProcesses[pid];
                        p.allocatedCpuPercent = perProc;
                        p.rateControlFlags = JOBOBJECT_CPU_RATE_CONTROL_ENABLE | JOBOBJECT_CPU_RATE_CONTROL_HARD_CAP;
                        p.matchedCriteria = "Equal_Per_User (" + user + ")";
                    }
                }
            }
        } else if (m_activePolicy == AllocationPolicyType::EqualPerSession) {
            // Count distinct sessions
            std::map<uint32_t, std::vector<uint32_t>> sessions;
            for (const auto& [pid, proc] : m_managedProcesses) {
                sessions[proc.sessionId].push_back(pid);
            }
            if (!sessions.empty()) {
                double perSessionShare = 100.0 / static_cast<double>(sessions.size());
                for (const auto& [sid, pids] : sessions) {
                    double perProc = perSessionShare / static_cast<double>(pids.size());
                    for (uint32_t pid : pids) {
                        auto& p = m_managedProcesses[pid];
                        p.allocatedCpuPercent = perProc;
                        p.rateControlFlags = JOBOBJECT_CPU_RATE_CONTROL_ENABLE | JOBOBJECT_CPU_RATE_CONTROL_HARD_CAP;
                        p.matchedCriteria = "Equal_Per_Session (Session " + std::to_string(sid) + ")";
                    }
                }
            }
        } else if (m_activePolicy == AllocationPolicyType::CustomWeighted) {
            for (auto& [pid, proc] : m_managedProcesses) {
                bool matched = false;
                for (const auto& [cName, crit] : m_criteria) {
                    bool matchApp = (crit.appPattern == "*" || proc.processName == crit.appPattern);
                    bool matchUser = (crit.userOrGroup == "*" || proc.userName == crit.userOrGroup);
                    bool matchSession = (crit.sessionId == -1 || proc.sessionId == static_cast<uint32_t>(crit.sessionId));

                    if (matchApp && matchUser && matchSession) {
                        proc.allocatedCpuPercent = static_cast<double>(crit.targetCpuPercent);
                        proc.rateControlFlags = JOBOBJECT_CPU_RATE_CONTROL_ENABLE;
                        if (crit.hardCap) proc.rateControlFlags |= JOBOBJECT_CPU_RATE_CONTROL_HARD_CAP;
                        else proc.rateControlFlags |= JOBOBJECT_CPU_RATE_CONTROL_WEIGHT_BASED;
                        proc.matchedCriteria = crit.criteriaName;
                        proc.memoryLimitBytes = crit.memoryLimitBytes;
                        matched = true;
                        break;
                    }
                }
                if (!matched) {
                    proc.allocatedCpuPercent = 10.0; // Default baseline
                    proc.rateControlFlags = JOBOBJECT_CPU_RATE_CONTROL_ENABLE | JOBOBJECT_CPU_RATE_CONTROL_HARD_CAP;
                    proc.matchedCriteria = "Default_Baseline";
                    proc.memoryLimitBytes = 0;
                }
            }
        }
    }
};

// ============================================================================
// Clean-Room Win32 C ABI Exports (wsrmcore.dll / wsrm.exe)
// ============================================================================

extern "C" {

inline int32_t MicaWsrmInitialize(void** ppEngine) {
    if (!ppEngine) return 0;
    auto& eng = SystemResourceManager::instance();
    *ppEngine = &eng;
    return 1;
}

inline int32_t MicaWsrmShutdown(void* pEngine) {
    if (!pEngine) return 0;
    return 1;
}

inline int32_t MicaWsrmSetAllocationPolicy(void* pEngine, int32_t policyType) {
    if (!pEngine || policyType < 0 || policyType > 3) return 0;
    auto* eng = reinterpret_cast<SystemResourceManager*>(pEngine);
    eng->setAllocationPolicy(static_cast<AllocationPolicyType>(policyType));
    return 1;
}

inline int32_t MicaWsrmCreateProcessMatchingCriteria(void* pEngine, const char* name,
                                                     const char* pattern, uint32_t cpuPercentage) {
    if (!pEngine || !name || !pattern || cpuPercentage > 100) return 0;
    auto* eng = reinterpret_cast<SystemResourceManager*>(pEngine);
    ProcessMatchingCriteria pmc;
    pmc.criteriaName = name;
    pmc.appPattern = pattern;
    pmc.targetCpuPercent = cpuPercentage;
    pmc.hardCap = true;
    return eng->addMatchingCriteria(pmc) ? 1 : 0;
}

inline int32_t MicaWsrmApplyFairShare(void* pEngine, uint32_t activeSessionCount) {
    if (!pEngine) return 0;
    auto* eng = reinterpret_cast<SystemResourceManager*>(pEngine);
    eng->applyFairShare(activeSessionCount);
    return 1;
}

inline int32_t MicaWsrmGetTenantAccounting(void* pEngine, const char* userOrApp,
                                           uint64_t* pCpuTimeUs, uint64_t* pPeakMemoryBytes,
                                           uint64_t* pIoBytes) {
    if (!pEngine || !userOrApp) return 0;
    auto* eng = reinterpret_cast<SystemResourceManager*>(pEngine);
    return eng->getTenantAccounting(userOrApp, pCpuTimeUs, pPeakMemoryBytes, pIoBytes) ? 1 : 0;
}

} // extern "C"

} // namespace micant::wsrm
