#pragma once

#include <string>
#include <vector>
#include <string_view>
#include <span>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include "micant/um/subsystems/mgmt/grouppolicy.hpp"
#include "micant/um/servers/gpo_sysvol.hpp"
#include "micant/km/executive/cm.hpp"

namespace micant::gp_tools {

// ============================================================================
// Canonical gpupdate.exe CLI Implementation
// ============================================================================

class GpupdateCommandExecutor {
public:
    static GpupdateCommandExecutor& instance() {
        static GpupdateCommandExecutor s_inst;
        return s_inst;
    }

    int32_t execute(std::span<const std::string_view> args, std::string& outStr, std::string& errStr) {
        (void)errStr;
        bool updateComputer = true;
        bool updateUser = true;
        bool force = false;
        (void)force;

        for (auto arg : args) {
            std::string s(arg);
            if (_stricmp(s.c_str(), "/force") == 0 || _stricmp(s.c_str(), "-force") == 0) {
                force = true;
            } else if (_stricmp(s.c_str(), "/target:computer") == 0 || _stricmp(s.c_str(), "-target:computer") == 0) {
                updateComputer = true;
                updateUser = false;
            } else if (_stricmp(s.c_str(), "/target:user") == 0 || _stricmp(s.c_str(), "-target:user") == 0) {
                updateComputer = false;
                updateUser = true;
            } else if (_stricmp(s.c_str(), "/?") == 0 || _stricmp(s.c_str(), "-?") == 0 || _stricmp(s.c_str(), "/help") == 0) {
                outStr = "Description: Updates Group Policy settings.\r\n\r\n"
                         "Syntax: GPUPDATE [/Target:{Computer | User}] [/Force] [/Wait:<value>]\r\n"
                         "                 [/Logoff] [/Boot] [/Sync]\r\n\r\n"
                         "Parameters:\r\n"
                         "    /Target:{Computer | User}  Specifies that only User or Computer policy settings are updated.\r\n"
                         "    /Force                     Reapplies all policy settings.\r\n"
                         "    /Wait:<value>              Number of seconds to wait for policy processing to finish.\r\n"
                         "    /Sync                      Causes the next foreground policy application to be synchronous.\r\n";
                return 0;
            }
        }

        auto& gp = gp::GroupPolicySubsystem::instance();
        std::ostringstream ss;
        ss << "Updating policy...\r\n\r\n";

        if (updateComputer) {
            auto repComp = gp.processGroupPolicy("DC01", true, "OU=Domain Controllers,DC=micant,DC=local", force);
            // Apply settings to ConfigurationManager
            auto& cm = micant::cm::ConfigurationManager::get();
            auto sw = cm.resolvePath(L"\\Registry\\Machine\\SOFTWARE");
            if (sw) {
                auto polKey = sw->createSubkey(L"Policies");
                if (polKey) {
                    for (const auto& [k, v] : repComp.resolvedSettings) {
                        std::wstring valName(v.valueName.begin(), v.valueName.end());
                        if (v.type == gp::PolicyValueType::Dword) {
                            polKey->setValueDword(valName, v.dwordValue);
                        } else {
                            std::wstring strVal(v.stringValue.begin(), v.stringValue.end());
                            polKey->setValueString(valName, strVal);
                        }
                    }
                }
            }
            ss << "Computer Policy update has completed successfully.\r\n";
        }

        if (updateUser) {
            auto repUser = gp.processGroupPolicy("MicaAdmin", false, "CN=Users,DC=micant,DC=local", force);
            (void)repUser;
            ss << "User Policy update has completed successfully.\r\n";
        }

        outStr = ss.str();
        return 0;
    }
};

// ============================================================================
// Canonical gpresult.exe CLI Implementation
// ============================================================================

class GpresultCommandExecutor {
public:
    static GpresultCommandExecutor& instance() {
        static GpresultCommandExecutor s_inst;
        return s_inst;
    }

    int32_t execute(std::span<const std::string_view> args, std::string& outStr, std::string& errStr) {
        (void)errStr;
        bool verbose = false;
        bool summary = false;
        std::string scope = "both";
        (void)scope;

        for (auto arg : args) {
            std::string s(arg);
            if (_stricmp(s.c_str(), "/r") == 0 || _stricmp(s.c_str(), "-r") == 0) {
                summary = true;
            } else if (_stricmp(s.c_str(), "/v") == 0 || _stricmp(s.c_str(), "-v") == 0 ||
                       _stricmp(s.c_str(), "/z") == 0 || _stricmp(s.c_str(), "-z") == 0) {
                verbose = true;
            } else if (_stricmp(s.c_str(), "/scope:computer") == 0 || _stricmp(s.c_str(), "-scope:computer") == 0) {
                scope = "computer";
            } else if (_stricmp(s.c_str(), "/scope:user") == 0 || _stricmp(s.c_str(), "-scope:user") == 0) {
                scope = "user";
            } else if (_stricmp(s.c_str(), "/?") == 0 || _stricmp(s.c_str(), "-?") == 0 || _stricmp(s.c_str(), "/help") == 0) {
                outStr = "Microsoft (R) Windows (R) Operating System Group Policy Result tool v10.0.26100.1\r\n"
                         "Syntax: GPRESULT [/S system [/U username [/P [password]]]] [/SCOPE {user | computer}]\r\n"
                         "                 [/USER targetusername] {/R | /V | /Z}\r\n";
                return 0;
            }
        }

        if (!summary && !verbose) {
            summary = true; // default
        }

        auto& gp = gp::GroupPolicySubsystem::instance();
        auto repComp = gp.getLastMachineReport();
        if (repComp.appliedGpoIds.empty()) {
            repComp = gp.processGroupPolicy("DC01", true, "OU=Domain Controllers,DC=micant,DC=local", false);
        }

        std::ostringstream ss;
        ss << "\r\nMicrosoft (R) Windows (R) Operating System Group Policy Result tool v10.0.26100.1\r\n";
        ss << "Created on: 10/30/2026 at 3:15:00 PM\r\n\r\n";
        ss << "RSOP data for MICANT\\MicaAdmin on DC01 : Logging Mode\r\n";
        ss << "------------------------------------------------------\r\n\r\n";
        ss << "OS Configuration:            Primary Domain Controller\r\n";
        ss << "OS Version:                  10.0.26100\r\n";
        ss << "Site Name:                   Default-First-Site-Name\r\n";
        ss << "Roaming Profile:             N/A\r\n";
        ss << "Local Profile:               C:\\Users\\MicaAdmin\r\n";
        ss << "Connected over a slow link?: No\r\n\r\n";

        ss << "COMPUTER SETTINGS\r\n";
        ss << "------------------\r\n";
        ss << "    CN=DC01,OU=Domain Controllers,DC=micant,DC=local\r\n";
        ss << "    Last time Group Policy was applied: 10/30/2026 at 3:14:55 PM\r\n";
        ss << "    Group Policy was applied from:      DC01.micant.local\r\n";
        ss << "    Group Policy slow link threshold:   500 kbps\r\n";
        ss << "    Domain Name:                        micant.local\r\n";
        ss << "    Domain Type:                        Windows 2016 or higher\r\n\r\n";

        ss << "    Applied Group Policy Objects\r\n";
        ss << "    -----------------------------\r\n";
        for (const auto& gpoId : repComp.appliedGpoIds) {
            gp::GroupPolicyObject gpoObj;
            if (gp.getGpo(gpoId, &gpoObj)) {
                ss << "        " << gpoObj.displayName << "\r\n";
            } else {
                ss << "        " << gpoId << "\r\n";
            }
        }
        ss << "\r\n";

        if (!repComp.filteredGpoReasons.empty()) {
            ss << "    The following GPOs were not applied because they were filtered out\r\n";
            ss << "    -------------------------------------------------------------------\r\n";
            for (const auto& [gpoId, reason] : repComp.filteredGpoReasons) {
                gp::GroupPolicyObject gpoObj;
                std::string dName = gpoId;
                if (gp.getGpo(gpoId, &gpoObj)) dName = gpoObj.displayName;
                ss << "        " << dName << "\r\n";
                ss << "            Filtering:  Not Applied (";
                switch (reason) {
                    case gp::FilterVerdict::WmiFilterFailed: ss << "WMI Filter Failed"; break;
                    case gp::FilterVerdict::BlockedInherit: ss << "Blocked by Inheritance"; break;
                    case gp::FilterVerdict::Disabled: ss << "GPO Disabled"; break;
                    case gp::FilterVerdict::AccessDenied: ss << "Access Denied"; break;
                    default: ss << "Denied"; break;
                }
                ss << ")\r\n";
            }
            ss << "\r\n";
        }

        ss << "    The computer is a part of the following security groups\r\n";
        ss << "    -------------------------------------------------------\r\n";
        ss << "        BUILTIN\\Administrators\r\n";
        ss << "        Everyone\r\n";
        ss << "        BUILTIN\\Users\r\n";
        ss << "        NT AUTHORITY\\NETWORK\r\n";
        ss << "        NT AUTHORITY\\Authenticated Users\r\n";
        ss << "        NT AUTHORITY\\This Organization\r\n";
        ss << "        DC01$\r\n";
        ss << "        Domain Controllers\r\n";
        ss << "        Enterprise Admins\r\n";
        ss << "        Domain Admins\r\n\r\n";

        if (verbose) {
            ss << "    Resultant Set of Policies (Registry Settings)\r\n";
            ss << "    ----------------------------------------------\r\n";
            for (const auto& [k, v] : repComp.resolvedSettings) {
                ss << "        GPO: " << v.winningGpoName << (v.enforced ? " [ENFORCED]" : "") << "\r\n";
                ss << "            KeyName: " << v.keyPath << "\r\n";
                ss << "            Value:   " << v.valueName << "\r\n";
                ss << "            State:   Enabled\r\n\r\n";
            }
            if (!repComp.resolvedSecurity.empty()) {
                ss << "    Resultant Set of Security Policies\r\n";
                ss << "    ----------------------------------\r\n";
                for (const auto& [secKey, secVal] : repComp.resolvedSecurity) {
                    ss << "        " << secKey << " = " << secVal << "\r\n";
                }
                ss << "\r\n";
            }
        }

        outStr = ss.str();
        return 0;
    }
};

// ============================================================================
// Shell / CLI String Helpers
// ============================================================================

inline void ExecuteGpupdateString(const std::string& cmdLine, std::string& outStr, std::string& errStr, int32_t& exitCode) {
    std::vector<std::string_view> tokens;
    size_t pos = 0;
    while (pos < cmdLine.size()) {
        while (pos < cmdLine.size() && (cmdLine[pos] == ' ' || cmdLine[pos] == '\t')) pos++;
        if (pos >= cmdLine.size()) break;
        size_t start = pos;
        while (pos < cmdLine.size() && cmdLine[pos] != ' ' && cmdLine[pos] != '\t') pos++;
        tokens.push_back(std::string_view(cmdLine.data() + start, pos - start));
    }

    std::span<const std::string_view> args;
    if (tokens.size() > 1) {
        args = std::span<const std::string_view>(tokens.data() + 1, tokens.size() - 1);
    }
    exitCode = GpupdateCommandExecutor::instance().execute(args, outStr, errStr);
}

inline void ExecuteGpresultString(const std::string& cmdLine, std::string& outStr, std::string& errStr, int32_t& exitCode) {
    std::vector<std::string_view> tokens;
    size_t pos = 0;
    while (pos < cmdLine.size()) {
        while (pos < cmdLine.size() && (cmdLine[pos] == ' ' || cmdLine[pos] == '\t')) pos++;
        if (pos >= cmdLine.size()) break;
        size_t start = pos;
        while (pos < cmdLine.size() && cmdLine[pos] != ' ' && cmdLine[pos] != '\t') pos++;
        tokens.push_back(std::string_view(cmdLine.data() + start, pos - start));
    }

    std::span<const std::string_view> args;
    if (tokens.size() > 1) {
        args = std::span<const std::string_view>(tokens.data() + 1, tokens.size() - 1);
    }
    exitCode = GpresultCommandExecutor::instance().execute(args, outStr, errStr);
}

} // namespace micant::gp_tools
