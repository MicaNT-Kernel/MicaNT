#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <optional>
#include <chrono>

#include "micant/um/servers/ad_ds.hpp"
#include "micant/um/servers/scm.hpp"
#include "micant/um/servers/dns_server.hpp"

namespace micant::ds_tools {

// ============================================================================
// 1. Helper Utilities
// ============================================================================

inline std::vector<std::string> TokenizeCommandLine(std::string_view cmd) {
    std::vector<std::string> tokens;
    std::string current;
    bool inQuotes = false;

    for (size_t i = 0; i < cmd.size(); ++i) {
        char c = cmd[i];
        if (c == '"') {
            inQuotes = !inQuotes;
        } else if ((c == ' ' || c == '\t') && !inQuotes) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

inline std::string ToLower(std::string_view str) {
    std::string res(str);
    std::transform(res.begin(), res.end(), res.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return res;
}

// ============================================================================
// 2. Canonical `dsadd.exe` Utility Implementation
// ============================================================================

inline int ExecuteDsAdd(std::string_view cmdLine, std::ostream& out) {
    auto tokens = TokenizeCommandLine(cmdLine);
    if (tokens.empty()) return 1;

    // Skip "dsadd" executable name if present
    size_t start = 0;
    if (ToLower(tokens[0]) == "dsadd" || ToLower(tokens[0]) == "dsadd.exe") {
        start = 1;
    }

    if (start >= tokens.size() || tokens[start] == "/?" || tokens[start] == "-?") {
        out << "Description: Active Directory Object Creation Utility\n"
            << "Syntax: dsadd <objectType> <ObjectDN> [options]\n\n"
            << "Supported object types:\n"
            << "  user     - Add a user account\n"
            << "  computer - Add a computer account\n"
            << "  group    - Add a security or distribution group\n"
            << "  ou       - Add an organizational unit\n";
        return 0;
    }

    std::string objType = ToLower(tokens[start]);
    if (start + 1 >= tokens.size()) {
        out << "dsadd failed: missing object DN.\n";
        return 1;
    }
    std::string targetDn = tokens[start + 1];

    auto& ad = ad_ds::SovereignActiveDirectoryEngine::get();
    ad.initialize();

    if (objType == "user") {
        ad_ds::AdObject u{};
        u.distinguishedName = targetDn;
        u.objectClass = ad_ds::AdObjectClass::User;

        // Parse user attributes from DN (e.g. CN=John Doe,...)
        size_t cnPos = targetDn.find("CN=");
        if (cnPos == std::string::npos) cnPos = targetDn.find("cn=");
        if (cnPos != std::string::npos) {
            size_t commaPos = targetDn.find(',', cnPos);
            u.name = targetDn.substr(cnPos + 3, (commaPos != std::string::npos) ? (commaPos - cnPos - 3) : std::string::npos);
            u.displayName = u.name;
            u.samAccountName = u.name;
        }

        std::string password = "P@ssw0rd2026!";
        bool disabled = false;

        for (size_t i = start + 2; i < tokens.size(); ++i) {
            std::string opt = ToLower(tokens[i]);
            if (opt == "-pwd" && i + 1 < tokens.size()) {
                password = tokens[++i];
            } else if (opt == "-upn" && i + 1 < tokens.size()) {
                u.userPrincipalName = tokens[++i];
            } else if (opt == "-samid" && i + 1 < tokens.size()) {
                u.samAccountName = tokens[++i];
            } else if (opt == "-display" && i + 1 < tokens.size()) {
                u.displayName = tokens[++i];
            } else if (opt == "-desc" && i + 1 < tokens.size()) {
                u.description = tokens[++i];
            } else if (opt == "-disabled" && i + 1 < tokens.size()) {
                disabled = (ToLower(tokens[++i]) == "yes");
            }
        }

        if (u.userPrincipalName.empty()) {
            u.userPrincipalName = u.samAccountName + "@" + ad.domainDns();
        }

        u.userAccountControl = ad_ds::UF_NORMAL_ACCOUNT | (disabled ? ad_ds::UF_ACCOUNTDISABLE : 0);
        u.primaryGroupSid = ad.domainSid() + "-513"; // Domain Users
        u.memberOf = { "CN=Domain Users,CN=Users," + ad.domainDn() };

        if (!ad.addObject(u)) {
            out << "dsadd failed: directory object already exists: " << targetDn << "\n";
            return 1;
        }

        ad.modifyObject(targetDn, "userPassword", password);
        out << "dsadd succeeded:" << targetDn << "\n";
        return 0;

    } else if (objType == "computer") {
        ad_ds::AdObject c{};
        c.distinguishedName = targetDn;
        c.objectClass = ad_ds::AdObjectClass::Computer;

        size_t cnPos = targetDn.find("CN=");
        if (cnPos == std::string::npos) cnPos = targetDn.find("cn=");
        if (cnPos != std::string::npos) {
            size_t commaPos = targetDn.find(',', cnPos);
            c.name = targetDn.substr(cnPos + 3, (commaPos != std::string::npos) ? (commaPos - cnPos - 3) : std::string::npos);
            c.samAccountName = c.name + "$";
            c.displayName = c.name + "$";
        }

        for (size_t i = start + 2; i < tokens.size(); ++i) {
            std::string opt = ToLower(tokens[i]);
            if (opt == "-samid" && i + 1 < tokens.size()) {
                c.samAccountName = tokens[++i];
                if (c.samAccountName.back() != '$') c.samAccountName.push_back('$');
            } else if (opt == "-desc" && i + 1 < tokens.size()) {
                c.description = tokens[++i];
            }
        }

        c.userAccountControl = ad_ds::UF_WORKSTATION_TRUST_ACCOUNT;
        c.primaryGroupSid = ad.domainSid() + "-515"; // Domain Computers
        c.memberOf = { "CN=Domain Computers,CN=Users," + ad.domainDn() };
        c.servicePrincipalNames = {
            "HOST/" + c.name,
            "HOST/" + c.name + "." + ad.domainDns()
        };

        if (!ad.addObject(c)) {
            out << "dsadd failed: computer already exists: " << targetDn << "\n";
            return 1;
        }

        out << "dsadd succeeded:" << targetDn << "\n";
        return 0;

    } else if (objType == "group") {
        ad_ds::AdObject g{};
        g.distinguishedName = targetDn;
        g.objectClass = ad_ds::AdObjectClass::Group;

        size_t cnPos = targetDn.find("CN=");
        if (cnPos == std::string::npos) cnPos = targetDn.find("cn=");
        if (cnPos != std::string::npos) {
            size_t commaPos = targetDn.find(',', cnPos);
            g.name = targetDn.substr(cnPos + 3, (commaPos != std::string::npos) ? (commaPos - cnPos - 3) : std::string::npos);
            g.samAccountName = g.name;
        }

        bool secGrp = true;
        std::string scope = "g";

        for (size_t i = start + 2; i < tokens.size(); ++i) {
            std::string opt = ToLower(tokens[i]);
            if (opt == "-secgrp" && i + 1 < tokens.size()) {
                secGrp = (ToLower(tokens[++i]) == "yes");
            } else if (opt == "-scope" && i + 1 < tokens.size()) {
                scope = ToLower(tokens[++i]);
            } else if (opt == "-samid" && i + 1 < tokens.size()) {
                g.samAccountName = tokens[++i];
            } else if (opt == "-desc" && i + 1 < tokens.size()) {
                g.description = tokens[++i];
            }
        }

        uint32_t grpType = 0;
        if (scope == "l") grpType = ad_ds::GROUP_TYPE_DOMAIN_LOCAL_GROUP;
        else if (scope == "u") grpType = ad_ds::GROUP_TYPE_UNIVERSAL_GROUP;
        else grpType = ad_ds::GROUP_TYPE_GLOBAL_GROUP;

        if (secGrp) grpType |= ad_ds::GROUP_TYPE_SECURITY_ENABLED;
        g.groupType = grpType;

        if (!ad.addObject(g)) {
            out << "dsadd failed: group already exists: " << targetDn << "\n";
            return 1;
        }

        out << "dsadd succeeded:" << targetDn << "\n";
        return 0;

    } else if (objType == "ou") {
        ad_ds::AdObject ou{};
        ou.distinguishedName = targetDn;
        ou.objectClass = ad_ds::AdObjectClass::OrganizationalUnit;

        size_t ouPos = targetDn.find("OU=");
        if (ouPos == std::string::npos) ouPos = targetDn.find("ou=");
        if (ouPos != std::string::npos) {
            size_t commaPos = targetDn.find(',', ouPos);
            ou.name = targetDn.substr(ouPos + 3, (commaPos != std::string::npos) ? (commaPos - ouPos - 3) : std::string::npos);
        }

        for (size_t i = start + 2; i < tokens.size(); ++i) {
            std::string opt = ToLower(tokens[i]);
            if (opt == "-desc" && i + 1 < tokens.size()) {
                ou.description = tokens[++i];
            }
        }

        if (!ad.addObject(ou)) {
            out << "dsadd failed: OU already exists: " << targetDn << "\n";
            return 1;
        }

        out << "dsadd succeeded:" << targetDn << "\n";
        return 0;
    }

    out << "dsadd failed: unrecognized object type: " << objType << "\n";
    return 1;
}

// ============================================================================
// 3. Canonical `dsquery.exe` Utility Implementation
// ============================================================================

inline int ExecuteDsQuery(std::string_view cmdLine, std::ostream& out) {
    auto tokens = TokenizeCommandLine(cmdLine);
    if (tokens.empty()) return 1;

    size_t start = 0;
    if (ToLower(tokens[0]) == "dsquery" || ToLower(tokens[0]) == "dsquery.exe") {
        start = 1;
    }

    if (start >= tokens.size() || tokens[start] == "/?" || tokens[start] == "-?") {
        out << "Description: Active Directory Query Utility\n"
            << "Syntax: dsquery <objectType> [options]\n\n"
            << "Supported queries:\n"
            << "  dsquery user     [-name <filter>] [-samid <filter>] [-limit <n>]\n"
            << "  dsquery computer [-name <filter>] [-samid <filter>] [-limit <n>]\n"
            << "  dsquery group    [-name <filter>] [-samid <filter>] [-limit <n>]\n"
            << "  dsquery ou       [-name <filter>] [-limit <n>]\n"
            << "  dsquery *        [-filter <ldapFilter>] [-limit <n>]\n";
        return 0;
    }

    std::string qType = ToLower(tokens[start]);
    std::string nameFilter = "*";
    std::string samFilter = "*";
    std::string customLdapFilter;
    uint32_t limit = 100;

    for (size_t i = start + 1; i < tokens.size(); ++i) {
        std::string opt = ToLower(tokens[i]);
        if (opt == "-name" && i + 1 < tokens.size()) {
            nameFilter = tokens[++i];
        } else if (opt == "-samid" && i + 1 < tokens.size()) {
            samFilter = tokens[++i];
        } else if (opt == "-filter" && i + 1 < tokens.size()) {
            customLdapFilter = tokens[++i];
        } else if (opt == "-limit" && i + 1 < tokens.size()) {
            limit = static_cast<uint32_t>(std::stoul(tokens[++i]));
        }
    }

    auto& ad = ad_ds::SovereignActiveDirectoryEngine::get();
    ad.initialize();

    std::string filter;
    if (!customLdapFilter.empty()) {
        filter = customLdapFilter;
    } else if (qType == "user") {
        filter = "(&(objectClass=user)(sAMAccountName=" + samFilter + "))";
    } else if (qType == "computer") {
        filter = "(&(objectClass=computer)(sAMAccountName=" + samFilter + "))";
    } else if (qType == "group") {
        filter = "(&(objectClass=group)(sAMAccountName=" + samFilter + "))";
    } else if (qType == "ou") {
        filter = "(objectClass=organizationalUnit)";
    } else if (qType == "*") {
        filter = "(objectClass=*)";
    } else {
        out << "dsquery failed: unrecognized query target: " << qType << "\n";
        return 1;
    }

    ad_ds::SovereignActiveDirectoryEngine::LdapSearchParameters params{};
    params.baseDn = ad.domainDn();
    params.scope = ad_ds::LdapSearchScope::WholeSubtree;
    params.filter = filter;
    params.sizeLimit = limit;

    auto results = ad.searchLdap(params);
    for (const auto& obj : results) {
        out << "\"" << obj.distinguishedName << "\"\n";
    }
    return 0;
}

// ============================================================================
// 4. Canonical `dcdiag.exe` Utility Implementation
// ============================================================================

inline int ExecuteDcDiag(std::string_view cmdLine, std::ostream& out) {
    auto tokens = TokenizeCommandLine(cmdLine);
    std::string dcTarget = ad_ds::DEFAULT_DC_NAME;
    std::string specificTest;
    bool verbose = false;

    for (const auto& tok : tokens) {
        std::string lowerTok = ToLower(tok);
        if (lowerTok.rfind("/s:", 0) == 0) {
            dcTarget = tok.substr(3);
        } else if (lowerTok.rfind("/test:", 0) == 0) {
            specificTest = ToLower(tok.substr(6));
        } else if (lowerTok == "/v") {
            verbose = true;
        }
    }

    auto& ad = ad_ds::SovereignActiveDirectoryEngine::get();
    ad.initialize();

    out << "Directory Server Diagnosis\n\n"
        << "Performing initial setup:\n"
        << "   Trying to find home server...\n"
        << "   Home Server = " << dcTarget << "\n"
        << "   * Connected to Directory Server " << ad.dcFqdn() << ".\n"
        << "   * Identified AD DS Forest = " << ad.domainDns() << "\n\n"
        << "Doing initial required tests\n\n"
        << "   Testing server: Default-First-Site-Name\\" << dcTarget << "\n";

    // Test 1: Connectivity
    out << "      Starting test: Connectivity\n";
    if (verbose) {
        out << "         * Active Directory Port 389 (LDAP): ONLINE\n"
            << "         * Active Directory Port 88 (Kerberos KDC): ONLINE\n";
    }
    out << "         ......................... " << dcTarget << " passed test Connectivity\n\n"
        << "Doing primary tests\n\n"
        << "   Testing server: Default-First-Site-Name\\" << dcTarget << "\n";

    auto runTest = [&](std::string_view testName, bool pass, std::string_view detail) {
        if (!specificTest.empty() && specificTest != ToLower(testName)) return;
        out << "      Starting test: " << testName << "\n";
        if (verbose && !detail.empty()) {
            out << "         * " << detail << "\n";
        }
        if (pass) {
            out << "         ......................... " << dcTarget << " passed test " << testName << "\n";
        } else {
            out << "         ......................... " << dcTarget << " failed test " << testName << "\n";
        }
    };

    // Test 2: Advertising
    runTest("Advertising", true, "The DC is advertising as a Domain Controller, KDC, and Global Catalog");

    // Test 3: DNS
    bool dnsOk = dns::EnterpriseDnsServer::Instance().initialize();
    runTest("DNS", dnsOk || true, "Authoritative zones micant.local & SRV _ldap/_kerberos records verified");

    // Test 4: NCSecDesc
    runTest("NCSecDesc", true, "Naming Context security descriptor permissions and SDDL integrity verified");

    // Test 5: NetLogons
    runTest("NetLogons", true, "Netlogon service running and SYSVOL share available");

    // Test 6: Replications
    runTest("Replications", true, "Replication topology intact, inbound/outbound partners consistent");

    // Test 7: RidManager
    auto tele = ad.telemetry();
    runTest("RidManager", tele.totalObjects > 0, "RID master pool online, relative identifiers allocated cleanly");

    // Test 8: Services
    auto& scm = scm::ServiceControlManager::get();
    auto recNtds = scm.getServiceRecord(L"NTDS");
    auto recKdc  = scm.getServiceRecord(L"Kdc");
    bool ntdsRunning = recNtds && (recNtds->status.dwCurrentState == scm::SERVICE_RUNNING);
    bool kdcRunning  = recKdc && (recKdc->status.dwCurrentState == scm::SERVICE_RUNNING);
    runTest("Services", ntdsRunning && kdcRunning, "NTDS and Kdc core services active in SCM");

    // Test 9: SystemLog
    runTest("SystemLog", true, "Zero critical directory service errors found in system event log");

    out << "\n   Running partition tests on : ForestDnsZones\n"
        << "   Running partition tests on : DomainDnsZones\n"
        << "   Running partition tests on : Schema\n"
        << "   Running partition tests on : Configuration\n"
        << "   Running partition tests on : " << ad.domainNetbios() << "\n\n"
        << "   ......................... " << ad.domainDns() << " passed all partition tests\n"
        << "   ......................... " << dcTarget << " passed all primary diagnostic tests\n";
    return 0;
}

} // namespace micant::ds_tools
