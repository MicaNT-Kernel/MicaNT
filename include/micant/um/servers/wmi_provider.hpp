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
#include "micant/um/servers/scm.hpp"

namespace micant::wmi {

// ============================================================================
// Sovereign WMI / CIM Data Structures
// ============================================================================

enum class WmiPropertyType {
    String,
    Uint32,
    Uint64,
    Boolean,
    DateTime
};

struct WmiProperty {
    std::string name;
    WmiPropertyType type{WmiPropertyType::String};
    std::string value;
};

struct WmiInstance {
    std::string className;
    std::map<std::string, WmiProperty> properties;

    std::string get(const std::string& propName) const {
        for (const auto& [k, v] : properties) {
            if (_stricmp(k.c_str(), propName.c_str()) == 0) {
                return v.value;
            }
        }
        return "";
    }

    void set(const std::string& name, const std::string& val, WmiPropertyType type = WmiPropertyType::String) {
        properties[name] = WmiProperty{name, type, val};
    }
};

// ============================================================================
// Sovereign WMI / CIM Repository Engine (root\cimv2)
// ============================================================================

class WmiRepository {
public:
    static WmiRepository& get() {
        static WmiRepository s_repo;
        return s_repo;
    }

    WmiRepository() {
        initializeStaticClasses();
    }

    void initializeStaticClasses() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queryCount = 0;
    }

    // Dynamic providers query execution
    std::vector<WmiInstance> query(const std::string& wql) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queryCount++;

        std::string raw = wql;
        // Normalize whitespace
        std::string targetClass;
        std::vector<std::string> targetProps;
        std::string whereClause;

        parseWql(raw, targetClass, targetProps, whereClause);

        std::vector<WmiInstance> instances = fetchClassInstances(targetClass);

        // Filter instances based on WHERE clause if present
        std::vector<WmiInstance> filtered;
        if (!whereClause.empty()) {
            for (const auto& inst : instances) {
                if (evaluateWhere(inst, whereClause)) {
                    filtered.push_back(inst);
                }
            }
        } else {
            filtered = instances;
        }

        // Apply property projection
        if (targetProps.empty() || (targetProps.size() == 1 && targetProps[0] == "*")) {
            return filtered;
        }

        std::vector<WmiInstance> projected;
        for (const auto& inst : filtered) {
            WmiInstance pInst;
            pInst.className = inst.className;
            for (const auto& reqProp : targetProps) {
                for (const auto& [k, v] : inst.properties) {
                    if (_stricmp(k.c_str(), reqProp.c_str()) == 0) {
                        pInst.properties[k] = v;
                    }
                }
            }
            projected.push_back(pInst);
        }

        return projected;
    }

    uint64_t getQueryCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queryCount;
    }

private:
    mutable std::mutex m_mutex;
    uint64_t m_queryCount{0};

    static int _stricmp(const char* a, const char* b) {
        while (*a && *b) {
            char ca = (*a >= 'A' && *a <= 'Z') ? (*a + 32) : *a;
            char cb = (*b >= 'A' && *b <= 'Z') ? (*b + 32) : *b;
            if (ca != cb) return ca - cb;
            a++;
            b++;
        }
        return (*a) - (*b);
    }

    void parseWql(const std::string& wql, std::string& outClass,
                  std::vector<std::string>& outProps, std::string& outWhere) {
        std::string s = wql;
        auto toLower = [](std::string str) {
            for (auto& c : str) if (c >= 'A' && c <= 'Z') c += 32;
            return str;
        };

        std::string sLower = toLower(s);
        size_t selectPos = sLower.find("select ");
        size_t fromPos = sLower.find(" from ");
        size_t wherePos = sLower.find(" where ");

        if (selectPos != std::string::npos && fromPos != std::string::npos) {
            std::string propsStr = s.substr(selectPos + 7, fromPos - (selectPos + 7));
            // Split props by comma
            std::stringstream ss(propsStr);
            std::string item;
            while (std::getline(ss, item, ',')) {
                // Trim item
                while (!item.empty() && (item.front() == ' ' || item.front() == '\t')) item.erase(0, 1);
                while (!item.empty() && (item.back() == ' ' || item.back() == '\t')) item.pop_back();
                if (!item.empty()) outProps.push_back(item);
            }

            if (wherePos != std::string::npos) {
                outClass = s.substr(fromPos + 6, wherePos - (fromPos + 6));
                outWhere = s.substr(wherePos + 7);
            } else {
                outClass = s.substr(fromPos + 6);
            }

            // Trim outClass
            while (!outClass.empty() && (outClass.front() == ' ' || outClass.front() == '\t')) outClass.erase(0, 1);
            while (!outClass.empty() && (outClass.back() == ' ' || outClass.back() == '\t')) outClass.pop_back();
        } else {
            outClass = s;
        }
    }

    bool evaluateWhere(const WmiInstance& inst, const std::string& whereClause) {
        // Simple evaluator: matches <attr> = '<val>' or <attr> = <val>
        size_t eqPos = whereClause.find('=');
        if (eqPos == std::string::npos) return true;

        std::string prop = whereClause.substr(0, eqPos);
        std::string val = whereClause.substr(eqPos + 1);

        auto trim = [](std::string& str) {
            while (!str.empty() && (str.front() == ' ' || str.front() == '\t' || str.front() == '\'' || str.front() == '\"')) str.erase(0, 1);
            while (!str.empty() && (str.back() == ' ' || str.back() == '\t' || str.back() == '\'' || str.back() == '\"')) str.pop_back();
        };

        trim(prop);
        trim(val);

        std::string actual = inst.get(prop);
        return (_stricmp(actual.c_str(), val.c_str()) == 0);
    }

    std::vector<WmiInstance> fetchClassInstances(const std::string& className) {
        std::vector<WmiInstance> list;

        if (_stricmp(className.c_str(), "Win32_OperatingSystem") == 0) {
            WmiInstance os;
            os.className = "Win32_OperatingSystem";
            os.set("Caption", "MicaNT Enterprise Operating System");
            os.set("Version", "10.0.26100");
            os.set("BuildNumber", "26100");
            os.set("OSArchitecture", "64-bit");
            os.set("CSName", "DC01");
            os.set("WindowsDirectory", "C:\\Windows");
            os.set("SystemDirectory", "C:\\Windows\\System32");
            os.set("TotalVisibleMemorySize", "33554432"); // 32 GB in KB
            os.set("FreePhysicalMemory", "25165824");
            os.set("NumberOfProcesses", "42");
            os.set("Status", "OK");
            os.set("Manufacturer", "Barrer Software");
            list.push_back(os);
        } else if (_stricmp(className.c_str(), "Win32_ComputerSystem") == 0) {
            WmiInstance cs;
            cs.className = "Win32_ComputerSystem";
            cs.set("Name", "DC01");
            cs.set("Domain", "micant.local");
            cs.set("Manufacturer", "Barrer Software");
            cs.set("Model", "MicaNT Sovereign Workstation");
            cs.set("TotalPhysicalMemory", "34359738368"); // 32 GB
            cs.set("PartOfDomain", "TRUE");
            cs.set("DomainRole", "5"); // Primary Domain Controller
            cs.set("PrimaryOwnerName", "Administrator");
            list.push_back(cs);
        } else if (_stricmp(className.c_str(), "Win32_Service") == 0) {
            // Interrogate live ServiceControlManager
            static const std::wstring s_knownSvcs[] = {
                L"WinRM", L"CertSvc", L"DHCPServer", L"DNS", L"NTDS", L"Kdc",
                L"HTTP", L"W3SVC", L"TermService", L"LanmanServer", L"LanmanWorkstation",
                L"RpcSs", L"EventLog", L"PlugPlay"
            };
            auto& scm = micant::scm::ServiceControlManager::get();
            for (const auto& name : s_knownSvcs) {
                auto s = scm.getServiceRecord(name);
                if (!s) continue;
                WmiInstance inst;
                inst.className = "Win32_Service";
                std::string sName(s->serviceName.begin(), s->serviceName.end());
                std::string dName(s->displayName.begin(), s->displayName.end());
                std::string bPath(s->binaryPath.begin(), s->binaryPath.end());

                inst.set("Name", sName);
                inst.set("DisplayName", dName);
                inst.set("State", (s->status.dwCurrentState == micant::scm::SERVICE_RUNNING) ? "Running" : "Stopped");
                inst.set("StartMode", (s->startType == micant::scm::SERVICE_AUTO_START) ? "Auto" : "Manual");
                inst.set("PathName", bPath);
                inst.set("ProcessId", std::to_string(s->status.dwProcessId ? s->status.dwProcessId : 840));
                inst.set("AcceptPause", "FALSE");
                inst.set("AcceptStop", "TRUE");
                list.push_back(inst);
            }
        } else if (_stricmp(className.c_str(), "Win32_Process") == 0) {
            struct ProcEntry {
                uint32_t pid;
                const char* name;
                const char* path;
                const char* cmd;
                uint64_t ws;
            };
            static const ProcEntry s_procs[] = {
                {4, "System", "ntoskrnl.exe", "ntoskrnl.exe", 12582912},
                {280, "smss.exe", "C:\\Windows\\System32\\smss.exe", "C:\\Windows\\System32\\smss.exe", 1048576},
                {368, "csrss.exe", "C:\\Windows\\System32\\csrss.exe", "C:\\Windows\\System32\\csrss.exe", 4194304},
                {440, "wininit.exe", "C:\\Windows\\System32\\wininit.exe", "C:\\Windows\\System32\\wininit.exe", 3145728},
                {512, "services.exe", "C:\\Windows\\System32\\services.exe", "C:\\Windows\\System32\\services.exe", 8388608},
                {520, "lsass.exe", "C:\\Windows\\System32\\lsass.exe", "C:\\Windows\\System32\\lsass.exe", 16777216},
                {840, "svchost.exe", "C:\\Windows\\System32\\svchost.exe", "C:\\Windows\\System32\\svchost.exe -k NetworkService", 25165824},
                {1024, "certsrv.exe", "C:\\Windows\\System32\\certsrv.exe", "C:\\Windows\\System32\\certsrv.exe", 18874368},
                {1080, "dns.exe", "C:\\Windows\\System32\\dns.exe", "C:\\Windows\\System32\\dns.exe", 14680064},
                {1120, "tcpsvcs.exe", "C:\\Windows\\System32\\tcpsvcs.exe", "C:\\Windows\\System32\\tcpsvcs.exe", 12582912},
                {1200, "w3wp.exe", "C:\\Windows\\System32\\inetsrv\\w3wp.exe", "C:\\Windows\\System32\\inetsrv\\w3wp.exe -ap DefaultAppPool", 33554432}
            };
            for (const auto& p : s_procs) {
                WmiInstance inst;
                inst.className = "Win32_Process";
                inst.set("ProcessId", std::to_string(p.pid));
                inst.set("Name", p.name);
                inst.set("ExecutablePath", p.path);
                inst.set("CommandLine", p.cmd);
                inst.set("WorkingSetSize", std::to_string(p.ws));
                inst.set("ThreadCount", "4");
                inst.set("HandleCount", "128");
                list.push_back(inst);
            }
        } else if (_stricmp(className.c_str(), "Win32_NetworkAdapterConfiguration") == 0) {
            WmiInstance nic;
            nic.className = "Win32_NetworkAdapterConfiguration";
            nic.set("Index", "1");
            nic.set("Description", "Intel(R) I210 Gigabit Network Connection");
            nic.set("IPAddress", "147.135.1.229");
            nic.set("IPSubnet", "255.255.255.0");
            nic.set("DefaultIPGateway", "147.135.1.1");
            nic.set("DNSDomain", "micant.local");
            nic.set("DNSServerSearchOrder", "147.135.1.229");
            nic.set("MACAddress", "00:15:5D:01:00:01");
            nic.set("IPEnabled", "TRUE");
            nic.set("DHCPEnabled", "FALSE");
            list.push_back(nic);
        } else if (_stricmp(className.c_str(), "Win32_LogicalDisk") == 0) {
            WmiInstance d;
            d.className = "Win32_LogicalDisk";
            d.set("DeviceID", "C:");
            d.set("DriveType", "3"); // Fixed disk
            d.set("FileSystem", "NTFS");
            d.set("VolumeName", "MicaSystem");
            d.set("Size", "512110190592");
            d.set("FreeSpace", "386547056640");
            list.push_back(d);
        } else if (_stricmp(className.c_str(), "Win32_Processor") == 0) {
            WmiInstance cpu;
            cpu.className = "Win32_Processor";
            cpu.set("DeviceID", "CPU0");
            cpu.set("Name", "Intel(R) Xeon(R) E-2288G CPU @ 3.70GHz");
            cpu.set("NumberOfCores", "8");
            cpu.set("NumberOfLogicalProcessors", "16");
            cpu.set("MaxClockSpeed", "3700");
            cpu.set("Architecture", "9"); // x64
            cpu.set("Manufacturer", "GenuineIntel");
            list.push_back(cpu);
        } else if (_stricmp(className.c_str(), "Win32_BIOS") == 0) {
            WmiInstance bios;
            bios.className = "Win32_BIOS";
            bios.set("Manufacturer", "MicaNT Sovereign UEFI");
            bios.set("SMBIOSBIOSVersion", "1.0.26100");
            bios.set("SerialNumber", "MICA-2026-XERJ-001");
            bios.set("Version", "MicaNT - 26100");
            list.push_back(bios);
        }

        return list;
    }
};

// ============================================================================
// Canonical wmic.exe CLI Utility
// ============================================================================

class WmicCommandExecutor {
public:
    static bool execute(const std::vector<std::string>& tokens, std::string& stdoutStr, std::string& stderrStr) {
        if (tokens.empty()) {
            stdoutStr = "wmic:root\\cli> \r\n";
            return true;
        }

        std::string sub = tokens[0];
        // Normalize lowercase
        std::string subLower = sub;
        for (auto& c : subLower) if (c >= 'A' && c <= 'Z') c += 32;

        if (subLower == "/?" || subLower == "-?" || subLower == "help") {
            stdoutStr = 
                "[WMIC - Windows Management Instrumentation Command-line]\r\n\r\n"
                "Available Class Aliases:\r\n"
                "  OS           - Operating system management\r\n"
                "  COMPUTERSYSTEM - Computer system management\r\n"
                "  SERVICE      - Service management\r\n"
                "  PROCESS      - Process management\r\n"
                "  NICCONFIG    - Network adapter configuration\r\n"
                "  LOGICALDISK  - Logical storage device management\r\n"
                "  CPU          - Processor management\r\n"
                "  BIOS         - BIOS and firmware management\r\n\r\n"
                "Verbs:\r\n"
                "  GET [<prop-list>] [/FORMAT:<format>]\r\n"
                "  LIST [BRIEF|FULL] [/FORMAT:<format>]\r\n";
            return true;
        }

        // Map alias to WMI Class Name
        std::string className;
        if (subLower == "os") className = "Win32_OperatingSystem";
        else if (subLower == "computersystem") className = "Win32_ComputerSystem";
        else if (subLower == "service") className = "Win32_Service";
        else if (subLower == "process") className = "Win32_Process";
        else if (subLower == "nicconfig") className = "Win32_NetworkAdapterConfiguration";
        else if (subLower == "logicaldisk" || subLower == "diskdrive") className = "Win32_LogicalDisk";
        else if (subLower == "cpu") className = "Win32_Processor";
        else if (subLower == "bios") className = "Win32_BIOS";
        else className = sub; // Raw class name

        std::string whereClause;
        std::string verb = "get";
        std::vector<std::string> requestedProps;
        std::string format = "list"; // default format

        size_t idx = 1;
        while (idx < tokens.size()) {
            std::string t = tokens[idx];
            std::string tLower = t;
            for (auto& c : tLower) if (c >= 'A' && c <= 'Z') c += 32;

            if (tLower == "where" && idx + 1 < tokens.size()) {
                whereClause = tokens[idx + 1];
                idx += 2;
                continue;
            } else if (tLower == "get") {
                verb = "get";
                idx++;
                if (idx < tokens.size() && tokens[idx].rfind("/", 0) != 0 && tokens[idx].rfind("-", 0) != 0) {
                    std::string pList = tokens[idx];
                    std::stringstream ss(pList);
                    std::string item;
                    while (std::getline(ss, item, ',')) {
                        while (!item.empty() && (item.front() == ' ' || item.front() == '\t')) item.erase(0, 1);
                        while (!item.empty() && (item.back() == ' ' || item.back() == '\t')) item.pop_back();
                        if (!item.empty()) requestedProps.push_back(item);
                    }
                    idx++;
                }
                continue;
            } else if (tLower == "list") {
                verb = "list";
                idx++;
                if (idx < tokens.size() && (tokens[idx] == "brief" || tokens[idx] == "BRIEF" ||
                                            tokens[idx] == "full" || tokens[idx] == "FULL")) {
                    format = "brief";
                    idx++;
                }
                continue;
            } else if (tLower.rfind("/format:", 0) == 0 || tLower.rfind("-format:", 0) == 0) {
                format = tLower.substr(8);
                idx++;
                continue;
            }
            idx++;
        }

        std::string wql = "SELECT ";
        if (requestedProps.empty()) {
            wql += "*";
        } else {
            for (size_t i = 0; i < requestedProps.size(); ++i) {
                if (i > 0) wql += ", ";
                wql += requestedProps[i];
            }
        }
        wql += " FROM " + className;
        if (!whereClause.empty()) {
            wql += " WHERE " + whereClause;
        }

        auto results = WmiRepository::get().query(wql);

        if (results.empty()) {
            stdoutStr = "No Instance(s) Available.\r\n";
            return true;
        }

        std::stringstream ss;
        if (format == "list") {
            for (size_t i = 0; i < results.size(); ++i) {
                const auto& inst = results[i];
                for (const auto& [k, v] : inst.properties) {
                    ss << k << "=" << v.value << "\r\n";
                }
                ss << "\r\n";
            }
        } else { // Table / Brief format
            // Collect all unique column names
            std::vector<std::string> cols;
            if (!requestedProps.empty()) {
                cols = requestedProps;
            } else {
                for (const auto& [k, _] : results[0].properties) {
                    cols.push_back(k);
                }
            }

            // Print header
            for (size_t c = 0; c < cols.size(); ++c) {
                if (c > 0) ss << "  ";
                ss << cols[c];
            }
            ss << "\r\n";

            // Print rows
            for (const auto& inst : results) {
                for (size_t c = 0; c < cols.size(); ++c) {
                    if (c > 0) ss << "  ";
                    ss << inst.get(cols[c]);
                }
                ss << "\r\n";
            }
        }

        stdoutStr = ss.str();
        return true;
    }
};

inline bool ExecuteWmic(const std::vector<std::string>& tokens, std::string& stdoutStr, std::string& stderrStr) {
    return WmicCommandExecutor::execute(tokens, stdoutStr, stderrStr);
}

inline bool ExecuteWmicString(const std::string& cmdLine, std::string& stdoutStr, std::string& stderrStr) {
    std::vector<std::string> tokens;
    std::string cur;
    bool inQuotes = false;
    for (char c : cmdLine) {
        if (c == '\"') {
            inQuotes = !inQuotes;
        } else if ((c == ' ' || c == '\t') && !inQuotes) {
            if (!cur.empty()) {
                tokens.push_back(cur);
                cur.clear();
            }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) tokens.push_back(cur);

    // Strip leading "wmic" or "wmic.exe" if present
    if (!tokens.empty()) {
        std::string first = tokens[0];
        std::string firstLower = first;
        for (auto& ch : firstLower) if (ch >= 'A' && ch <= 'Z') ch += 32;
        if (firstLower == "wmic" || firstLower == "wmic.exe") {
            tokens.erase(tokens.begin());
        }
    }

    return ExecuteWmic(tokens, stdoutStr, stderrStr);
}

} // namespace micant::wmi
