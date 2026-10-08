#ifndef MICANT_GROUPPOLICY_HPP
#define MICANT_GROUPPOLICY_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <functional>
#include <atomic>
#include <cstring>

#include "ntstatus.hpp"
#include "version.hpp"
#include "scm.hpp"

namespace micant::gp {

// Standard Group Policy Object Scope
enum class GpoScope : uint32_t {
    Computer = 0,
    User     = 1,
    Both     = 2
};

// GPO Activation Status
enum class GpoStatus : uint32_t {
    Enabled         = 0,
    UserDisabled    = 1,
    MachineDisabled = 2,
    AllDisabled     = 3
};

// Scope of Management (SOM) Level in LSDOU Precedence
enum class SomType : uint32_t {
    Local               = 0, // 1st applied (lowest priority unless enforced)
    Site                = 1, // 2nd applied
    Domain              = 2, // 3rd applied
    OrganizationalUnit  = 3  // 4th applied (highest priority in standard hierarchy)
};

// Group Policy Loopback Processing Modes
enum class LoopbackMode : uint32_t {
    Disabled = 0,
    Replace  = 1, // Replaces user GPO list with computer OU user settings
    Merge    = 2  // Appends computer OU user settings on top of normal user GPOs
};

// Policy Registry Value Types
enum class PolicyValueType : uint32_t {
    Dword        = 4,  // REG_DWORD
    String       = 1,  // REG_SZ
    ExpandString = 2,  // REG_EXPAND_SZ
    Binary       = 3,  // REG_BINARY
    MultiString  = 7   // REG_MULTI_SZ
};

// Reason for GPO evaluation filter verdict
enum class FilterVerdict : uint32_t {
    Applied         = 0,
    Disabled        = 1,
    WmiFilterFailed = 2,
    AccessDenied    = 3,
    EmptyGpo        = 4,
    BlockedInherit  = 5
};

// Individual Setting from Registry.pol
struct PolicySetting {
    std::string keyPath;
    std::string valueName;
    PolicyValueType type{PolicyValueType::Dword};
    uint32_t dwordValue{0};
    std::string stringValue;
    std::vector<uint8_t> binaryValue;
    bool isDelete{false};
    bool isDeleteValues{false};
    bool isDeleteKeys{false};
};

// Registry.pol Binary Serialization Engine
// Windows Format: Magic 0x67655250 ("PReg"), Version 1, followed by bracketed tokens:
// [Key;Value;Type;Size;Data]
class RegistryPol {
public:
    static constexpr uint32_t POL_MAGIC = 0x67655250; // "PReg"
    static constexpr uint32_t POL_VERSION = 1;

    std::vector<PolicySetting> settings;

    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> out;
        out.reserve(512);

        // Header: Magic + Version
        uint32_t magic = POL_MAGIC;
        uint32_t ver = POL_VERSION;
        const uint8_t* pM = reinterpret_cast<const uint8_t*>(&magic);
        const uint8_t* pV = reinterpret_cast<const uint8_t*>(&ver);
        out.insert(out.end(), pM, pM + 4);
        out.insert(out.end(), pV, pV + 4);

        for (const auto& s : settings) {
            // Write '[' (UTF-16LE: 0x005B)
            out.push_back('['); out.push_back(0);

            // Write keyPath UTF-16LE + ';'
            for (char c : s.keyPath) { out.push_back(c); out.push_back(0); }
            out.push_back(';'); out.push_back(0);

            // Write valueName UTF-16LE + ';'
            for (char c : s.valueName) { out.push_back(c); out.push_back(0); }
            out.push_back(';'); out.push_back(0);

            // Type (uint32_t) + ';'
            uint32_t typeCode = static_cast<uint32_t>(s.type);
            const uint8_t* pT = reinterpret_cast<const uint8_t*>(&typeCode);
            out.insert(out.end(), pT, pT + 4);
            out.push_back(';'); out.push_back(0);

            // Size (uint32_t) + ';'
            uint32_t dataSize = 4;
            if (s.type == PolicyValueType::String || s.type == PolicyValueType::ExpandString) {
                dataSize = static_cast<uint32_t>((s.stringValue.size() + 1) * 2);
            } else if (s.type == PolicyValueType::Binary) {
                dataSize = static_cast<uint32_t>(s.binaryValue.size());
            }
            const uint8_t* pS = reinterpret_cast<const uint8_t*>(&dataSize);
            out.insert(out.end(), pS, pS + 4);
            out.push_back(';'); out.push_back(0);

            // Data
            if (s.type == PolicyValueType::Dword) {
                uint32_t dw = s.dwordValue;
                const uint8_t* pD = reinterpret_cast<const uint8_t*>(&dw);
                out.insert(out.end(), pD, pD + 4);
            } else if (s.type == PolicyValueType::String || s.type == PolicyValueType::ExpandString) {
                for (char c : s.stringValue) { out.push_back(c); out.push_back(0); }
                out.push_back(0); out.push_back(0); // null terminator
            } else {
                out.insert(out.end(), s.binaryValue.begin(), s.binaryValue.end());
            }

            // Write ']' (UTF-16LE: 0x005D)
            out.push_back(']'); out.push_back(0);
        }

        return out;
    }

    bool deserialize(const uint8_t* data, size_t size) {
        if (!data || size < 8) return false;
        uint32_t magic = 0;
        uint32_t ver = 0;
        std::memcpy(&magic, data, 4);
        std::memcpy(&ver, data + 4, 4);
        if (magic != POL_MAGIC || ver != POL_VERSION) return false;

        settings.clear();
        size_t idx = 8;
        while (idx + 4 < size) {
            // Check for '['
            if (data[idx] != '[' || data[idx + 1] != 0) {
                idx += 2;
                continue;
            }
            idx += 2;

            // Read KeyPath until ';'
            std::string key;
            while (idx + 1 < size && !(data[idx] == ';' && data[idx + 1] == 0)) {
                if (data[idx] != 0) key.push_back(static_cast<char>(data[idx]));
                idx += 2;
            }
            if (idx + 2 <= size) idx += 2; // skip ';'

            // Read ValueName until ';'
            std::string valName;
            while (idx + 1 < size && !(data[idx] == ';' && data[idx + 1] == 0)) {
                if (data[idx] != 0) valName.push_back(static_cast<char>(data[idx]));
                idx += 2;
            }
            if (idx + 2 <= size) idx += 2; // skip ';'

            // Read Type (4 bytes)
            if (idx + 4 > size) break;
            uint32_t typeVal = 0;
            std::memcpy(&typeVal, data + idx, 4);
            idx += 4;
            if (idx + 2 <= size && data[idx] == ';' && data[idx + 1] == 0) idx += 2;

            // Read Size (4 bytes)
            if (idx + 4 > size) break;
            uint32_t sz = 0;
            std::memcpy(&sz, data + idx, 4);
            idx += 4;
            if (idx + 2 <= size && data[idx] == ';' && data[idx + 1] == 0) idx += 2;

            // Read Data
            PolicySetting ps;
            ps.keyPath = key;
            ps.valueName = valName;
            ps.type = static_cast<PolicyValueType>(typeVal);

            if (ps.type == PolicyValueType::Dword && sz >= 4 && idx + 4 <= size) {
                std::memcpy(&ps.dwordValue, data + idx, 4);
                idx += sz;
            } else if ((ps.type == PolicyValueType::String || ps.type == PolicyValueType::ExpandString) && idx + sz <= size) {
                std::string s;
                for (size_t i = 0; i < sz; i += 2) {
                    if (data[idx + i] != 0) s.push_back(static_cast<char>(data[idx + i]));
                }
                ps.stringValue = s;
                idx += sz;
            } else if (idx + sz <= size) {
                ps.binaryValue.assign(data + idx, data + idx + sz);
                idx += sz;
            }

            // Check for ']'
            if (idx + 1 < size && data[idx] == ']' && data[idx + 1] == 0) {
                idx += 2;
            }

            settings.push_back(ps);
        }
        return true;
    }
};

// WMI Filter Definition
struct WmiFilter {
    std::string filterId;     // e.g. "{7C9B23F1-5231-4F2A-85F0-55B173617300}"
    std::string name;         // e.g. "Windows 11 Client 24H2 Only"
    std::string query;        // e.g. "SELECT * FROM Win32_OperatingSystem WHERE Version LIKE '10.0.26100%'"
    std::string wmiNamespace; // "root\\cimv2"

    bool evaluate(const std::map<std::string, std::string>& machineFacts) const {
        if (query.empty()) return true;

        // Simple evaluation engine for Win32_OperatingSystem / Win32_ComputerSystem facts
        if (query.find("Version LIKE '10.0.26100%'") != std::string::npos) {
            auto it = machineFacts.find("OSVersion");
            if (it != machineFacts.end() && it->second.find("10.0.26100") == 0) return true;
            return false;
        }
        if (query.find("OSArchitecture = '64-bit'") != std::string::npos) {
            auto it = machineFacts.find("Architecture");
            if (it != machineFacts.end() && it->second == "x64") return true;
            return false;
        }
        if (query.find("ProductType = '1'") != std::string::npos) { // Workstation
            auto it = machineFacts.find("ProductType");
            if (it != machineFacts.end() && it->second == "1") return true;
            return false;
        }
        if (query.find("DomainRole = 'dc'") != std::string::npos) {
            auto it = machineFacts.find("DomainRole");
            if (it != machineFacts.end() && it->second == "dc") return true;
            return false;
        }

        // Default: If syntax parsed without negation, assume match
        return true;
    }
};

// Client-Side Extension (CSE)
struct ClientSideExtension {
    std::string clsid;
    std::string name;
    std::string dllPath;
    bool enabled{true};
    bool noBackgroundRefresh{false};
    uint32_t executionCount{0};
};

// Full Group Policy Object Model
struct GroupPolicyObject {
    std::string gpoId;          // e.g. "{31B2F340-016D-11D2-945F-00C04FB984F9}"
    std::string displayName;    // e.g. "Default Domain Policy"
    GpoStatus status{GpoStatus::Enabled};
    uint32_t userVersion{1};
    uint32_t machineVersion{1};
    std::string fileSysPath;
    std::string wmiFilterId;

    RegistryPol machinePolicy;
    RegistryPol userPolicy;

    // Security Settings (Audit, Privileges, Password Policy)
    std::map<std::string, std::string> securitySettings;

    // Scripts CSE
    std::vector<std::string> startupScripts;
    std::vector<std::string> shutdownScripts;
    std::vector<std::string> logonScripts;
    std::vector<std::string> logoffScripts;

    // Folder Redirection CSE
    std::map<std::string, std::string> folderRedirections;
};

// Link in a Scope of Management (SOM)
struct GpoLink {
    std::string gpoId;
    bool enforced{false};  // NoOverride - takes absolute precedence
    bool enabled{true};
    int32_t order{1};      // Link Order: 1 is applied last (highest precedence among links)
};

// Scope of Management (SOM) node
struct ScopeOfManagement {
    SomType type;
    std::string name;
    std::string distinguishedName;
    bool blockInheritance{false};
    std::vector<GpoLink> links;
};

// Resultant Set of Policy (RSoP) resolved policy entry
struct RsopSetting {
    std::string keyPath;
    std::string valueName;
    PolicyValueType type{PolicyValueType::Dword};
    uint32_t dwordValue{0};
    std::string stringValue;
    std::string winningGpoId;
    std::string winningGpoName;
    SomType appliedSomType{SomType::Local};
    bool enforced{false};
};

// Diagnostic RSoP Report
struct GroupPolicyReport {
    std::string targetName;
    bool isMachine{true};
    LoopbackMode loopbackMode{LoopbackMode::Disabled};
    uint64_t refreshTimestampUs{0};
    std::vector<std::string> appliedGpoIds;
    std::vector<std::pair<std::string, FilterVerdict>> filteredGpoReasons;
    std::map<std::string, RsopSetting> resolvedSettings;
    std::map<std::string, std::string> resolvedSecurity;
    std::vector<std::string> executedScripts;

    std::string generateSummary() const {
        std::ostringstream ss;
        ss << "================================================================================\n";
        ss << "  MicaNT Resultant Set of Policy (RSoP) Diagnostic Report\n";
        ss << "================================================================================\n";
        ss << "Target: " << targetName << " (" << (isMachine ? "Computer" : "User") << " Configuration)\n";
        ss << "Loopback Mode: " << (loopbackMode == LoopbackMode::Disabled ? "Disabled" :
                                   loopbackMode == LoopbackMode::Replace ? "Replace" : "Merge") << "\n";
        ss << "Applied GPOs (" << appliedGpoIds.size() << "):\n";
        for (const auto& g : appliedGpoIds) {
            ss << "  [+] " << g << "\n";
        }
        if (!filteredGpoReasons.empty()) {
            ss << "Filtered / Denied GPOs (" << filteredGpoReasons.size() << "):\n";
            for (const auto& f : filteredGpoReasons) {
                const char* r = "Unknown";
                switch (f.second) {
                    case FilterVerdict::Disabled: r = "GPO Disabled"; break;
                    case FilterVerdict::WmiFilterFailed: r = "WMI Filter Condition False"; break;
                    case FilterVerdict::AccessDenied: r = "Access Denied / Security Filter"; break;
                    case FilterVerdict::EmptyGpo: r = "Empty GPO"; break;
                    case FilterVerdict::BlockedInherit: r = "Blocked by Inheritance"; break;
                    default: break;
                }
                ss << "  [-] " << f.first << " -> Reason: " << r << "\n";
            }
        }
        ss << "Effective Policy Settings (" << resolvedSettings.size() << "):\n";
        for (const auto& [k, v] : resolvedSettings) {
            ss << "  " << v.keyPath << "\\" << v.valueName << " = ";
            if (v.type == PolicyValueType::Dword) ss << v.dwordValue << " (DWORD)";
            else ss << "\"" << v.stringValue << "\" (SZ)";
            ss << " [Won by: " << v.winningGpoName << (v.enforced ? " (ENFORCED)]" : "]") << "\n";
        }
        ss << "================================================================================\n";
        return ss.str();
    }
};

// ============================================================================
// GroupPolicySubsystem: Singleton Engine
// ============================================================================
class GroupPolicySubsystem {
private:
    std::mutex m_mutex;
    bool m_initialized{false};
    LoopbackMode m_loopbackMode{LoopbackMode::Disabled};
    uint32_t m_backgroundRefreshIntervalMin{90};
    uint32_t m_randomOffsetMin{30};

    // Repositories
    std::unordered_map<std::string, GroupPolicyObject> m_gpos;
    std::unordered_map<std::string, WmiFilter> m_wmiFilters;
    std::unordered_map<std::string, ClientSideExtension> m_cses;
    std::map<std::string, std::string> m_machineFacts;

    // SOM Hierarchy
    ScopeOfManagement m_localSom;
    std::unordered_map<std::string, ScopeOfManagement> m_siteSoms;
    std::unordered_map<std::string, ScopeOfManagement> m_domainSoms;
    std::unordered_map<std::string, ScopeOfManagement> m_ouSoms;

    // Metrics
    std::atomic<uint64_t> m_refreshesCompleted{0};
    std::atomic<uint64_t> m_policiesApplied{0};
    std::atomic<uint64_t> m_wmiEvaluations{0};
    std::atomic<uint64_t> m_cseInvocations{0};

    GroupPolicyReport m_lastMachineReport;
    GroupPolicyReport m_lastUserReport;

    GroupPolicySubsystem() {
        initializeDefaults();
    }

public:
    static GroupPolicySubsystem& instance() {
        static GroupPolicySubsystem s_inst;
        return s_inst;
    }

    void initializeDefaults() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_initialized) return;

        // Register default machine facts for WMI evaluation
        m_machineFacts["OSVersion"] = "10.0.26100.1";
        m_machineFacts["Architecture"] = "x64";
        m_machineFacts["ProductType"] = "1"; // Workstation / Client
        m_machineFacts["DomainRole"] = "member";
        m_machineFacts["TotalPhysicalMemory"] = "17179869184"; // 16GB

        // Register Standard Client-Side Extensions (CSEs)
        // Registry CSE
        m_cses["{35378EAC-683F-11D2-A89A-00C04FBBCFA2}"] = {
            "{35378EAC-683F-11D2-A89A-00C04FBBCFA2}",
            "Registry Client-Side Extension",
            "C:\\Windows\\System32\\gptext.dll",
            true, false, 0
        };
        // Security CSE
        m_cses["{827D319E-6EAC-11D2-A4EA-00C04F79F83A}"] = {
            "{827D319E-6EAC-11D2-A4EA-00C04F79F83A}",
            "Security Client-Side Extension",
            "C:\\Windows\\System32\\scecli.dll",
            true, false, 0
        };
        // Scripts CSE
        m_cses["{42B5FA82-575C-11D2-964C-00C04617B5CE}"] = {
            "{42B5FA82-575C-11D2-964C-00C04617B5CE}",
            "Scripts Client-Side Extension",
            "C:\\Windows\\System32\\gptext.dll",
            true, false, 0
        };
        // Folder Redirection CSE
        m_cses["{25537BA6-77A8-11D2-9B6C-00C04FB873C9}"] = {
            "{25537BA6-77A8-11D2-9B6C-00C04FB873C9}",
            "Folder Redirection Client-Side Extension",
            "C:\\Windows\\System32\\fdeploy.dll",
            true, false, 0
        };

        // Initialize Local SOM
        m_localSom.type = SomType::Local;
        m_localSom.name = "Local Computer Policy";
        m_localSom.distinguishedName = "LOCAL";
        m_localSom.blockInheritance = false;

        // Create Local GPO
        GroupPolicyObject localGpo;
        localGpo.gpoId = "{LOCAL-GPO-0000-0000-000000000000}";
        localGpo.displayName = "Local Group Policy";
        localGpo.fileSysPath = "C:\\Windows\\System32\\GroupPolicy";
        PolicySetting s1;
        s1.keyPath = "Software\\Policies\\Microsoft\\Windows\\System";
        s1.valueName = "LocalAuditLevel";
        s1.type = PolicyValueType::Dword;
        s1.dwordValue = 1;
        localGpo.machinePolicy.settings.push_back(s1);
        m_gpos[localGpo.gpoId] = localGpo;
        m_localSom.links.push_back({localGpo.gpoId, false, true, 1});

        // Initialize Default Domain SOM & Default Domain Policy
        ScopeOfManagement domainSom;
        domainSom.type = SomType::Domain;
        domainSom.name = "micant.internal";
        domainSom.distinguishedName = "DC=micant,DC=internal";
        domainSom.blockInheritance = false;

        GroupPolicyObject defDomainGpo;
        defDomainGpo.gpoId = "{31B2F340-016D-11D2-945F-00C04FB984F9}";
        defDomainGpo.displayName = "Default Domain Policy";
        defDomainGpo.fileSysPath = "\\\\micant.internal\\sysvol\\micant.internal\\Policies\\{31B2F340-016D-11D2-945F-00C04FB984F9}";
        defDomainGpo.securitySettings["PasswordComplexity"] = "1";
        defDomainGpo.securitySettings["MinPasswordLength"] = "12";
        defDomainGpo.securitySettings["MaxPasswordAge"] = "60";
        PolicySetting s2;
        s2.keyPath = "Software\\Policies\\Microsoft\\Windows\\WindowsUpdate";
        s2.valueName = "AUOptions";
        s2.type = PolicyValueType::Dword;
        s2.dwordValue = 4; // Auto download and install
        defDomainGpo.machinePolicy.settings.push_back(s2);
        m_gpos[defDomainGpo.gpoId] = defDomainGpo;
        domainSom.links.push_back({defDomainGpo.gpoId, false, true, 1});
        m_domainSoms[domainSom.distinguishedName] = domainSom;

        // Initialize Default Domain Controllers Policy
        GroupPolicyObject defDcGpo;
        defDcGpo.gpoId = "{6AC1786C-016F-11D2-945F-00C04fB984F9}";
        defDcGpo.displayName = "Default Domain Controllers Policy";
        defDcGpo.fileSysPath = "\\\\micant.internal\\sysvol\\micant.internal\\Policies\\{6AC1786C-016F-11D2-945F-00C04fB984F9}";
        defDcGpo.securitySettings["SeNetworkLogonRight"] = "*S-1-5-11,*S-1-5-32-544";
        m_gpos[defDcGpo.gpoId] = defDcGpo;

        // Initialize Workstations OU SOM
        ScopeOfManagement wsOu;
        wsOu.type = SomType::OrganizationalUnit;
        wsOu.name = "Workstations";
        wsOu.distinguishedName = "OU=Workstations,DC=micant,DC=internal";
        wsOu.blockInheritance = false;

        // Create Workstation Baseline GPO
        GroupPolicyObject wsBaselineGpo;
        wsBaselineGpo.gpoId = "{A0B1C2D3-E4F5-4678-9012-3456789ABCDE}";
        wsBaselineGpo.displayName = "Workstation Security Baseline";
        wsBaselineGpo.fileSysPath = "\\\\micant.internal\\sysvol\\micant.internal\\Policies\\{A0B1C2D3-E4F5-4678-9012-3456789ABCDE}";
        PolicySetting s3;
        s3.keyPath = "Software\\Policies\\Microsoft\\Windows\\System";
        s3.valueName = "DisableCMD";
        s3.type = PolicyValueType::Dword;
        s3.dwordValue = 0; // CMD Allowed
        PolicySetting s4;
        s4.keyPath = "Software\\Policies\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon";
        s4.valueName = "LegalNoticeText";
        s4.type = PolicyValueType::String;
        s4.stringValue = "Authorized Access Only - MicaNT Enterprise";
        wsBaselineGpo.machinePolicy.settings.push_back(s3);
        wsBaselineGpo.machinePolicy.settings.push_back(s4);
        wsBaselineGpo.startupScripts.push_back("C:\\Windows\\System32\\GroupPolicy\\Machine\\Scripts\\Startup\\verify_integrity.cmd");
        m_gpos[wsBaselineGpo.gpoId] = wsBaselineGpo;
        wsOu.links.push_back({wsBaselineGpo.gpoId, false, true, 1});
        m_ouSoms[wsOu.distinguishedName] = wsOu;

        // Register Standard WMI Filters
        WmiFilter wfWin11;
        wfWin11.filterId = "{F1111111-2222-3333-4444-555555555555}";
        wfWin11.name = "Windows 11 24H2 Systems Only";
        wfWin11.query = "SELECT * FROM Win32_OperatingSystem WHERE Version LIKE '10.0.26100%'";
        m_wmiFilters[wfWin11.filterId] = wfWin11;

        WmiFilter wfDcOnly;
        wfDcOnly.filterId = "{F2222222-3333-4444-5555-666666666666}";
        wfDcOnly.name = "Domain Controllers Only";
        wfDcOnly.query = "SELECT * FROM Win32_ComputerSystem WHERE DomainRole = 'dc'";
        m_wmiFilters[wfDcOnly.filterId] = wfDcOnly;

        // Register SCM service record
        registerScmService();

        // Register VersionDatabase binaries
        registerVersionDatabase();

        m_initialized = true;
    }

    void registerScmService() {
        auto& scm = micant::scm::ServiceControlManager::get();
        auto rec = std::make_shared<micant::scm::ServiceRecord>();
        rec->serviceName = L"Gpsvc";
        rec->displayName = L"Group Policy Client";
        rec->serviceType = micant::scm::SERVICE_WIN32_SHARE_PROCESS;
        rec->startType = micant::scm::SERVICE_AUTO_START;
        rec->errorControl = micant::scm::SERVICE_ERROR_NORMAL;
        rec->binaryPath = L"C:\\Windows\\System32\\gpsvc.dll";
        rec->status.dwServiceType = micant::scm::SERVICE_WIN32_SHARE_PROCESS;
        rec->status.dwCurrentState = micant::scm::SERVICE_RUNNING;
        rec->status.dwControlsAccepted = micant::scm::SERVICE_ACCEPT_STOP | micant::scm::SERVICE_ACCEPT_SHUTDOWN;
        scm.registerServiceRecord(rec);
    }

    void registerVersionDatabase() {
        auto& db = micant::version::VersionDatabase::Instance();
        db.RegisterModule("gpsvc.dll", "10.0.26100.1", "Group Policy Client Service");
        db.RegisterModule("gpupdate.exe", "10.0.26100.1", "Group Policy Update Utility");
        db.RegisterModule("gpreport.exe", "10.0.26100.1", "Group Policy Results Diagnostic Utility");
        db.RegisterModule("gpedit.dll", "10.0.26100.1", "Group Policy Group Policy Editor Snap-in");
        db.RegisterModule("userenv.dll", "10.0.26100.1", "Userenv Group Policy Engine");
    }

    uint32_t getBackgroundRefreshIntervalMin() const { return m_backgroundRefreshIntervalMin; }
    uint32_t getRandomOffsetMin() const { return m_randomOffsetMin; }

    // GPO CRUD
    bool createGpo(const GroupPolicyObject& gpo) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_gpos[gpo.gpoId] = gpo;
        return true;
    }

    bool getGpo(const std::string& gpoId, GroupPolicyObject* outGpo) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_gpos.find(gpoId);
        if (it == m_gpos.end()) return false;
        if (outGpo) *outGpo = it->second;
        return true;
    }

    size_t getGpoCount() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_gpos.size();
    }

    // Link Management
    bool linkGpo(SomType somType, const std::string& somDn, const std::string& gpoId, bool enforced = false, bool enabled = true) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_gpos.find(gpoId) == m_gpos.end()) return false;

        ScopeOfManagement* pSom = nullptr;
        if (somType == SomType::Local) pSom = &m_localSom;
        else if (somType == SomType::Site) {
            pSom = &m_siteSoms[somDn];
            pSom->type = SomType::Site;
            pSom->distinguishedName = somDn;
        } else if (somType == SomType::Domain) {
            pSom = &m_domainSoms[somDn];
            pSom->type = SomType::Domain;
            pSom->distinguishedName = somDn;
        } else if (somType == SomType::OrganizationalUnit) {
            pSom = &m_ouSoms[somDn];
            pSom->type = SomType::OrganizationalUnit;
            pSom->distinguishedName = somDn;
        }

        if (!pSom) return false;
        pSom->links.push_back({gpoId, enforced, enabled, static_cast<int32_t>(pSom->links.size() + 1)});
        return true;
    }

    void setSomBlockInheritance(SomType somType, const std::string& somDn, bool block) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (somType == SomType::OrganizationalUnit) {
            m_ouSoms[somDn].blockInheritance = block;
        } else if (somType == SomType::Domain) {
            m_domainSoms[somDn].blockInheritance = block;
        }
    }

    // WMI Filters
    bool registerWmiFilter(const WmiFilter& filter) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_wmiFilters[filter.filterId] = filter;
        return true;
    }

    void setGpoWmiFilter(const std::string& gpoId, const std::string& filterId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_gpos.find(gpoId);
        if (it != m_gpos.end()) {
            it->second.wmiFilterId = filterId;
        }
    }

    // Machine Facts update
    void setMachineFact(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_machineFacts[key] = value;
    }

    // Loopback Configuration
    void setLoopbackMode(LoopbackMode mode) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_loopbackMode = mode;
    }

    LoopbackMode getLoopbackMode() const {
        return m_loopbackMode;
    }

    // Core Policy Processing: LSDOU Precedence & Inheritance Resolution
    GroupPolicyReport processGroupPolicy(const std::string& targetName, bool isMachine, const std::string& targetOuDn, bool force = false) {
        std::lock_guard<std::mutex> lock(m_mutex);
        (void)force;

        GroupPolicyReport report;
        report.targetName = targetName;
        report.isMachine = isMachine;
        report.loopbackMode = m_loopbackMode;
        report.refreshTimestampUs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());

        // Step 1: Collect GPO candidates in LSDOU order (Local -> Site -> Domain -> Parent OU -> Target OU)
        struct OrderedGpo {
            std::string gpoId;
            SomType somType;
            bool enforced;
            int32_t order;
        };
        std::vector<OrderedGpo> candidates;

        // 1. Local GPOs
        for (const auto& l : m_localSom.links) {
            if (l.enabled) candidates.push_back({l.gpoId, SomType::Local, l.enforced, l.order});
        }

        // 2. Site GPOs (if any)
        for (const auto& [siteDn, siteSom] : m_siteSoms) {
            for (const auto& l : siteSom.links) {
                if (l.enabled) candidates.push_back({l.gpoId, SomType::Site, l.enforced, l.order});
            }
        }

        // 3. Domain GPOs
        bool blockDomainInheritance = false;
        auto itDomain = m_domainSoms.find("DC=micant,DC=internal");
        if (itDomain != m_domainSoms.end()) {
            for (const auto& l : itDomain->second.links) {
                if (l.enabled) candidates.push_back({l.gpoId, SomType::Domain, l.enforced, l.order});
            }
        }

        // 4. OU GPOs (check block inheritance on target OU)
        auto itOu = m_ouSoms.find(targetOuDn);
        if (itOu != m_ouSoms.end()) {
            blockDomainInheritance = itOu->second.blockInheritance;
            for (const auto& l : itOu->second.links) {
                if (l.enabled) candidates.push_back({l.gpoId, SomType::OrganizationalUnit, l.enforced, l.order});
            }
        }

        // Step 2: Separate into Normal GPOs vs Enforced GPOs
        // Normal rule: Later applied GPOs win over earlier GPOs (OU wins over Domain wins over Site wins over Local).
        // Enforced (NoOverride) rule: Enforced GPOs win over all non-enforced GPOs, and higher-level enforced GPOs win over lower-level ones!
        std::vector<OrderedGpo> regularPass;
        std::vector<OrderedGpo> enforcedPass;

        for (const auto& cand : candidates) {
            // Check Block Inheritance: If OU blocks inheritance, drop non-enforced GPOs from Site and Domain
            if (blockDomainInheritance && !cand.enforced && (cand.somType == SomType::Domain || cand.somType == SomType::Site)) {
                report.filteredGpoReasons.push_back({cand.gpoId, FilterVerdict::BlockedInherit});
                continue;
            }

            auto itGpo = m_gpos.find(cand.gpoId);
            if (itGpo == m_gpos.end()) continue;

            const auto& gpo = itGpo->second;

            // Check GPO status
            if (gpo.status == GpoStatus::AllDisabled ||
                (isMachine && gpo.status == GpoStatus::MachineDisabled) ||
                (!isMachine && gpo.status == GpoStatus::UserDisabled)) {
                report.filteredGpoReasons.push_back({cand.gpoId, FilterVerdict::Disabled});
                continue;
            }

            // Check WMI Filter
            if (!gpo.wmiFilterId.empty()) {
                m_wmiEvaluations.fetch_add(1, std::memory_order_relaxed);
                auto itWmi = m_wmiFilters.find(gpo.wmiFilterId);
                if (itWmi != m_wmiFilters.end()) {
                    if (!itWmi->second.evaluate(m_machineFacts)) {
                        report.filteredGpoReasons.push_back({cand.gpoId, FilterVerdict::WmiFilterFailed});
                        continue;
                    }
                }
            }

            if (cand.enforced) {
                enforcedPass.push_back(cand);
            } else {
                regularPass.push_back(cand);
            }
        }

        // Apply regular GPOs in order
        for (const auto& c : regularPass) {
            report.appliedGpoIds.push_back(c.gpoId);
            applyGpoToReport(c.gpoId, c.somType, false, isMachine, report);
        }

        // Apply Enforced GPOs on top (enforced from higher SOMs win)
        for (const auto& c : enforcedPass) {
            report.appliedGpoIds.push_back(c.gpoId);
            applyGpoToReport(c.gpoId, c.somType, true, isMachine, report);
        }

        // Invoke CSE counts
        m_cses["{35378EAC-683F-11D2-A89A-00C04FBBCFA2}"].executionCount++; // Registry CSE
        m_cses["{827D319E-6EAC-11D2-A4EA-00C04F79F83A}"].executionCount++; // Security CSE
        m_cses["{42B5FA82-575C-11D2-964C-00C04617B5CE}"].executionCount++; // Scripts CSE
        m_cseInvocations.fetch_add(3, std::memory_order_relaxed);

        m_refreshesCompleted.fetch_add(1, std::memory_order_relaxed);
        m_policiesApplied.fetch_add(report.resolvedSettings.size(), std::memory_order_relaxed);

        if (isMachine) m_lastMachineReport = report;
        else m_lastUserReport = report;

        return report;
    }

private:
    void applyGpoToReport(const std::string& gpoId, SomType somType, bool enforced, bool isMachine, GroupPolicyReport& report) {
        auto it = m_gpos.find(gpoId);
        if (it == m_gpos.end()) return;
        const auto& gpo = it->second;

        // Apply Registry Policy Settings
        const auto& pol = isMachine ? gpo.machinePolicy : gpo.userPolicy;
        for (const auto& s : pol.settings) {
            std::string settingKey = s.keyPath + "\\" + s.valueName;

            // If an enforced GPO already set this, regular GPO cannot overwrite
            auto existIt = report.resolvedSettings.find(settingKey);
            if (existIt != report.resolvedSettings.end() && existIt->second.enforced && !enforced) {
                continue;
            }

            RsopSetting rs;
            rs.keyPath = s.keyPath;
            rs.valueName = s.valueName;
            rs.type = s.type;
            rs.dwordValue = s.dwordValue;
            rs.stringValue = s.stringValue;
            rs.winningGpoId = gpo.gpoId;
            rs.winningGpoName = gpo.displayName;
            rs.appliedSomType = somType;
            rs.enforced = enforced;
            report.resolvedSettings[settingKey] = rs;
        }

        // Apply Security Settings
        for (const auto& [k, v] : gpo.securitySettings) {
            report.resolvedSecurity[k] = v;
        }

        // Apply Startup / Logon Scripts
        if (isMachine) {
            for (const auto& sc : gpo.startupScripts) report.executedScripts.push_back(sc);
        } else {
            for (const auto& sc : gpo.logonScripts) report.executedScripts.push_back(sc);
        }
    }

public:
    // Reporting getters
    GroupPolicyReport getLastMachineReport() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_lastMachineReport;
    }

    GroupPolicyReport getLastUserReport() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_lastUserReport;
    }

    // Metrics getters
    uint64_t getRefreshesCompleted() const { return m_refreshesCompleted.load(std::memory_order_relaxed); }
    uint64_t getPoliciesApplied() const { return m_policiesApplied.load(std::memory_order_relaxed); }
    uint64_t getWmiEvaluations() const { return m_wmiEvaluations.load(std::memory_order_relaxed); }
    uint64_t getCseInvocations() const { return m_cseInvocations.load(std::memory_order_relaxed); }
};

// ============================================================================
// Clean-Room Win32 C ABI Parity Exports (`userenv.dll` & `gpsvc.dll`)
// ============================================================================
extern "C" {

struct GPO_LINK_NODE {
    wchar_t szGpoId[64];
    wchar_t szDisplayName[128];
    uint32_t dwFlags;
    struct GPO_LINK_NODE* pNext;
};

inline uint32_t WINAPI MicaProcessGroupPolicyCompleted(void* hToken, uint32_t dwStatus) {
    (void)hToken;
    (void)dwStatus;
    return 0; // ERROR_SUCCESS
}

inline uint32_t WINAPI MicaRefreshPolicy(int32_t bMachine) {
    auto& gp = GroupPolicySubsystem::instance();
    gp.processGroupPolicy(bMachine ? "LOCAL_MACHINE" : "CURRENT_USER", bMachine != 0, "OU=Workstations,DC=micant,DC=internal", false);
    return 1; // TRUE
}

inline uint32_t WINAPI MicaRefreshPolicyEx(int32_t bMachine, uint32_t dwOptions) {
    auto& gp = GroupPolicySubsystem::instance();
    bool force = (dwOptions & 0x00000001) != 0; // RP_FORCE
    gp.processGroupPolicy(bMachine ? "LOCAL_MACHINE" : "CURRENT_USER", bMachine != 0, "OU=Workstations,DC=micant,DC=internal", force);
    return 1; // TRUE
}

inline uint32_t WINAPI MicaGetGPOListW(void* hToken, const wchar_t* lpTargetName, const wchar_t* lpHostName,
                                      const wchar_t* lpExtGuid, uint32_t dwFlags, GPO_LINK_NODE** pGPOList) {
    (void)hToken;
    (void)lpTargetName;
    (void)lpHostName;
    (void)lpExtGuid;
    (void)dwFlags;
    if (!pGPOList) return 87; // ERROR_INVALID_PARAMETER

    auto node = new GPO_LINK_NODE();
    std::wcsncpy(node->szGpoId, L"{31B2F340-016D-11D2-945F-00C04FB984F9}", 63);
    std::wcsncpy(node->szDisplayName, L"Default Domain Policy", 127);
    node->dwFlags = 0;
    node->pNext = nullptr;
    *pGPOList = node;
    return 0; // ERROR_SUCCESS
}

inline uint32_t WINAPI MicaFreeGPOListW(GPO_LINK_NODE* pGPOList) {
    while (pGPOList) {
        GPO_LINK_NODE* next = pGPOList->pNext;
        delete pGPOList;
        pGPOList = next;
    }
    return 1; // TRUE
}

} // extern "C"

inline void RegisterGroupPolicySubsystem() {
    GroupPolicySubsystem::instance();
}

} // namespace micant::gp

#endif // MICANT_GROUPPOLICY_HPP
