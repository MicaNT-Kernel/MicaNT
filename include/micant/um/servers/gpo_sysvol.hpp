#pragma once

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <functional>
#include <cstdint>
#include <cstring>
#include "micant/um/subsystems/mgmt/grouppolicy.hpp"
#include "micant/km/drivers/net/srv2.hpp"

namespace micant::gp {

// ============================================================================
// GPT.INI: General Purpose Template Configuration Codec (MS-GPOL)
// ============================================================================

struct GptIni {
    uint32_t version{65537}; // (MachineVersion << 16) | UserVersion (1 << 16 | 1 = 65537)
    std::string displayName;

    uint32_t getMachineVersion() const { return (version >> 16) & 0xFFFF; }
    uint32_t getUserVersion() const { return version & 0xFFFF; }
    void setVersions(uint16_t machineVer, uint16_t userVer) {
        version = (static_cast<uint32_t>(machineVer) << 16) | static_cast<uint32_t>(userVer);
    }

    std::string serialize() const {
        std::ostringstream ss;
        ss << "[General]\r\n";
        ss << "Version=" << version << "\r\n";
        if (!displayName.empty()) {
            ss << "displayName=" << displayName << "\r\n";
        }
        return ss.str();
    }

    bool deserialize(const std::string& text) {
        std::istringstream iss(text);
        std::string line;
        bool inGeneral = false;
        while (std::getline(iss, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
            if (line.empty() || line[0] == ';') continue;
            if (line == "[General]") { inGeneral = true; continue; }
            if (line[0] == '[') { inGeneral = false; continue; }
            if (!inGeneral) continue;

            auto eq = line.find('=');
            if (eq != std::string::npos) {
                std::string k = line.substr(0, eq);
                std::string v = line.substr(eq + 1);
                if (_stricmp(k.c_str(), "Version") == 0) {
                    try { version = static_cast<uint32_t>(std::stoul(v)); } catch (...) {}
                } else if (_stricmp(k.c_str(), "displayName") == 0) {
                    displayName = v;
                }
            }
        }
        return true;
    }
};

// ============================================================================
// GptTmpl.inf: Security Settings Template Codec (MS-GPSB)
// ============================================================================

struct GptTmplInf {
    std::map<std::string, std::string> systemAccess;
    std::map<std::string, std::string> kerberosPolicy;
    std::map<std::string, std::string> eventAudit;
    std::map<std::string, std::string> registryValues;
    std::map<std::string, std::string> privilegeRights;

    std::string serialize() const {
        std::ostringstream ss;
        ss << "[Unicode]\r\nUnicode=yes\r\n";

        if (!systemAccess.empty()) {
            ss << "[System Access]\r\n";
            for (const auto& [k, v] : systemAccess) {
                ss << k << " = " << v << "\r\n";
            }
        }
        if (!kerberosPolicy.empty()) {
            ss << "[Kerberos Policy]\r\n";
            for (const auto& [k, v] : kerberosPolicy) {
                ss << k << " = " << v << "\r\n";
            }
        }
        if (!eventAudit.empty()) {
            ss << "[Event Audit]\r\n";
            for (const auto& [k, v] : eventAudit) {
                ss << k << " = " << v << "\r\n";
            }
        }
        if (!privilegeRights.empty()) {
            ss << "[Privilege Rights]\r\n";
            for (const auto& [k, v] : privilegeRights) {
                ss << k << " = " << v << "\r\n";
            }
        }
        if (!registryValues.empty()) {
            ss << "[Registry Values]\r\n";
            for (const auto& [k, v] : registryValues) {
                ss << k << "=" << v << "\r\n";
            }
        }
        return ss.str();
    }

    bool deserialize(const std::string& text) {
        std::istringstream iss(text);
        std::string line;
        std::string curSection;
        while (std::getline(iss, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
            if (line.empty() || line[0] == ';') continue;
            if (line.front() == '[' && line.back() == ']') {
                curSection = line.substr(1, line.size() - 2);
                continue;
            }

            auto eq = line.find('=');
            if (eq != std::string::npos) {
                std::string k = line.substr(0, eq);
                std::string v = line.substr(eq + 1);
                // trim
                while (!k.empty() && k.back() == ' ') k.pop_back();
                while (!k.empty() && k.front() == ' ') k.erase(0, 1);
                while (!v.empty() && v.back() == ' ') v.pop_back();
                while (!v.empty() && v.front() == ' ') v.erase(0, 1);

                if (_stricmp(curSection.c_str(), "System Access") == 0) {
                    systemAccess[k] = v;
                } else if (_stricmp(curSection.c_str(), "Kerberos Policy") == 0) {
                    kerberosPolicy[k] = v;
                } else if (_stricmp(curSection.c_str(), "Event Audit") == 0) {
                    eventAudit[k] = v;
                } else if (_stricmp(curSection.c_str(), "Privilege Rights") == 0) {
                    privilegeRights[k] = v;
                } else if (_stricmp(curSection.c_str(), "Registry Values") == 0) {
                    registryValues[k] = v;
                }
            }
        }
        return true;
    }
};

// ============================================================================
// SysvolGpoDirectory: Full SYSVOL Policy Tree for a GPO
// ============================================================================

struct SysvolGpoDirectory {
    std::string gpoId;
    std::string displayName;
    GptIni gptIni;
    RegistryPol machineRegistryPol;
    RegistryPol userRegistryPol;
    GptTmplInf securityTemplate;
    std::vector<std::string> startupScripts;
    std::vector<std::string> shutdownScripts;
    std::vector<std::string> logonScripts;
    std::vector<std::string> logoffScripts;
};

// ============================================================================
// EnterpriseSysvolServer: Sovereign SYSVOL Policy Repository Engine
// ============================================================================

class EnterpriseSysvolServer {
private:
    std::mutex m_mutex;
    bool m_initialized{false};
    std::string m_domainFqdn{"micant.local"};
    std::string m_domainNetbios{"MICANT"};
    std::string m_sysvolSharePath{"\\\\micant.local\\SYSVOL"};
    std::string m_localSysvolPath{"C:\\Windows\\SYSVOL\\sysvol\\micant.local\\Policies"};

    std::unordered_map<std::string, SysvolGpoDirectory> m_sysvolGpos;

    EnterpriseSysvolServer() {
        initializeDefaults();
    }

public:
    static EnterpriseSysvolServer& instance() {
        static EnterpriseSysvolServer s_inst;
        return s_inst;
    }

    void resetForTest() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sysvolGpos.clear();
        m_initialized = false;
        initializeDefaultsLocked();
    }

    void initializeDefaults() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_initialized) return;
        initializeDefaultsLocked();
    }

private:
    void initializeDefaultsLocked() {
        // Pre-seed 1: Default Domain Policy {31B2F340-016D-11D2-945F-00C04FB984F9}
        SysvolGpoDirectory defDomain;
        defDomain.gpoId = "{31B2F340-016D-11D2-945F-00C04FB984F9}";
        defDomain.displayName = "Default Domain Policy";
        defDomain.gptIni.displayName = "Default Domain Policy";
        defDomain.gptIni.setVersions(1, 1);

        // Security Template: Password, Kerberos, Lockout
        defDomain.securityTemplate.systemAccess["MinimumPasswordLength"] = "12";
        defDomain.securityTemplate.systemAccess["PasswordComplexity"] = "1";
        defDomain.securityTemplate.systemAccess["PasswordHistorySize"] = "24";
        defDomain.securityTemplate.systemAccess["MaximumPasswordAge"] = "42";
        defDomain.securityTemplate.systemAccess["LockoutBadCount"] = "5";
        defDomain.securityTemplate.systemAccess["ResetLockoutCount"] = "30";
        defDomain.securityTemplate.systemAccess["LockoutDuration"] = "30";

        defDomain.securityTemplate.kerberosPolicy["MaxTicketAge"] = "10";
        defDomain.securityTemplate.kerberosPolicy["MaxRenewAge"] = "7";
        defDomain.securityTemplate.kerberosPolicy["MaxClockSkew"] = "5";

        defDomain.securityTemplate.eventAudit["AuditLogonEvents"] = "3"; // Success and Failure
        defDomain.securityTemplate.eventAudit["AuditAccountManage"] = "3";

        // Machine registry.pol: Windows Update AUOptions = 4
        PolicySetting sAu;
        sAu.keyPath = "Software\\Policies\\Microsoft\\Windows\\WindowsUpdate";
        sAu.valueName = "AUOptions";
        sAu.type = PolicyValueType::Dword;
        sAu.dwordValue = 4;
        defDomain.machineRegistryPol.settings.push_back(sAu);

        m_sysvolGpos[defDomain.gpoId] = defDomain;

        // Pre-seed 2: Default Domain Controllers Policy {6AC1786C-016F-11D2-945F-00C04fB984F9}
        SysvolGpoDirectory defDc;
        defDc.gpoId = "{6AC1786C-016F-11D2-945F-00C04fB984F9}";
        defDc.displayName = "Default Domain Controllers Policy";
        defDc.gptIni.displayName = "Default Domain Controllers Policy";
        defDc.gptIni.setVersions(1, 1);

        defDc.securityTemplate.privilegeRights["SeNetworkLogonRight"] = "*S-1-5-11,*S-1-5-32-544";
        defDc.securityTemplate.privilegeRights["SeSecurityPrivilege"] = "*S-1-5-32-544";
        defDc.securityTemplate.eventAudit["AuditDSAccess"] = "3";

        PolicySetting sRepl;
        sRepl.keyPath = "Software\\Policies\\Microsoft\\Windows\\DirectoryServices";
        sRepl.valueName = "StrictReplication";
        sRepl.type = PolicyValueType::Dword;
        sRepl.dwordValue = 1;
        defDc.machineRegistryPol.settings.push_back(sRepl);

        m_sysvolGpos[defDc.gpoId] = defDc;

        // Pre-seed 3: Enterprise Workstation Baseline Policy {B0E1F2A3-4C5D-6E7F-8A9B-0C1D2E3F4A5B}
        SysvolGpoDirectory wsGpo;
        wsGpo.gpoId = "{B0E1F2A3-4C5D-6E7F-8A9B-0C1D2E3F4A5B}";
        wsGpo.displayName = "Enterprise Workstation Baseline Policy";
        wsGpo.gptIni.displayName = "Enterprise Workstation Baseline Policy";
        wsGpo.gptIni.setVersions(1, 1);

        PolicySetting sFw;
        sFw.keyPath = "Software\\Policies\\Microsoft\\WindowsFirewall\\DomainProfile";
        sFw.valueName = "EnableFirewall";
        sFw.type = PolicyValueType::Dword;
        sFw.dwordValue = 1;
        wsGpo.machineRegistryPol.settings.push_back(sFw);

        PolicySetting sCmd;
        sCmd.keyPath = "Software\\Policies\\Microsoft\\Windows\\System";
        sCmd.valueName = "DisableCMD";
        sCmd.type = PolicyValueType::Dword;
        sCmd.dwordValue = 0;
        wsGpo.machineRegistryPol.settings.push_back(sCmd);

        PolicySetting sNotice;
        sNotice.keyPath = "Software\\Policies\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon";
        sNotice.valueName = "LegalNoticeText";
        sNotice.type = PolicyValueType::String;
        sNotice.stringValue = "Authorized Access Only - MicaNT Enterprise DC01";
        wsGpo.machineRegistryPol.settings.push_back(sNotice);

        wsGpo.startupScripts.push_back("C:\\Windows\\System32\\GroupPolicy\\Machine\\Scripts\\Startup\\verify_integrity.cmd");

        m_sysvolGpos[wsGpo.gpoId] = wsGpo;

        // Pre-seed 4: High Security Zero-Trust Enforced Policy {C1D2E3F4-5A6B-7C8D-9E0F-1A2B3C4D5E6F}
        SysvolGpoDirectory enfGpo;
        enfGpo.gpoId = "{C1D2E3F4-5A6B-7C8D-9E0F-1A2B3C4D5E6F}";
        enfGpo.displayName = "High Security Zero-Trust Enforced Policy";
        enfGpo.gptIni.displayName = "High Security Zero-Trust Enforced Policy";
        enfGpo.gptIni.setVersions(1, 1);

        PolicySetting sEnfNotice;
        sEnfNotice.keyPath = "Software\\Policies\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon";
        sEnfNotice.valueName = "LegalNoticeText";
        sEnfNotice.type = PolicyValueType::String;
        sEnfNotice.stringValue = "MANDATORY ROOT NOTICE: ZERO OVERRIDE";
        enfGpo.machineRegistryPol.settings.push_back(sEnfNotice);

        PolicySetting sUac;
        sUac.keyPath = "Software\\Policies\\Microsoft\\Windows\\System";
        sUac.valueName = "EnableLUA";
        sUac.type = PolicyValueType::Dword;
        sUac.dwordValue = 1;
        enfGpo.machineRegistryPol.settings.push_back(sUac);

        m_sysvolGpos[enfGpo.gpoId] = enfGpo;

        // Synchronize shares to SMB server
        syncToSmbShare();

        m_initialized = true;
    }

public:
    bool createSysvolGpo(const SysvolGpoDirectory& dir) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sysvolGpos[dir.gpoId] = dir;
        return true;
    }

    bool getSysvolGpo(const std::string& gpoId, SysvolGpoDirectory* outDir) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sysvolGpos.find(gpoId);
        if (it == m_sysvolGpos.end()) return false;
        if (outDir) *outDir = it->second;
        return true;
    }

    size_t getGpoCount() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_sysvolGpos.size();
    }

    std::vector<SysvolGpoDirectory> getAllGpos() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<SysvolGpoDirectory> list;
        list.reserve(m_sysvolGpos.size());
        for (const auto& [k, v] : m_sysvolGpos) list.push_back(v);
        return list;
    }

    std::string getSysvolPath(const std::string& gpoId, bool unc = true) const {
        if (unc) {
            return "\\\\" + m_domainFqdn + "\\SYSVOL\\" + m_domainFqdn + "\\Policies\\" + gpoId;
        } else {
            return "C:\\Windows\\SYSVOL\\sysvol\\" + m_domainFqdn + "\\Policies\\" + gpoId;
        }
    }

    std::string getDomainFqdn() const { return m_domainFqdn; }
    void setDomainFqdn(const std::string& fqdn) { m_domainFqdn = fqdn; }

    void syncToSmbShare() {
        auto& smbSrv = smb::km::SmbServerEngine::Instance();
        // Register SYSVOL share if not present
        smbSrv.addShare(L"SYSVOL", L"C:\\Windows\\SYSVOL\\sysvol", L"Logon server share", false);
        smbSrv.addShare(L"NETLOGON", L"C:\\Windows\\SYSVOL\\sysvol\\scripts", L"Logon scripts share", false);
    }
};

} // namespace micant::gp
