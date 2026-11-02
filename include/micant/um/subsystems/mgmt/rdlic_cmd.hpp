#pragma once

/**
 * @file rdlic_cmd.hpp
 * @brief Canonical Win32 licmgr.exe / rdlic.exe CLI Management Utilities for RD Licensing.
 *
 * Clean-room implementation of Remote Desktop Licensing (RD Licensing)
 * CLI management tools under 17 U.S.C. § 1201(f) & Google LLC v. Oracle America, Inc.
 * (141 S. Ct. 1183, 2021).
 *
 * Provides complete CLI management for:
 *   - licmgr status / rdlic status: Introspect license server health, activation, CAL totals
 *   - licmgr pack-list / rdlic pack list: Tabular inventory of installed CAL key packs
 *   - licmgr pack-add <type> <qty> [product]: Install a new CAL key pack
 *   - licmgr lic-list / rdlic list: Tabular inventory of issued device and user licenses
 *   - licmgr issue <hwid> <machine> <user> [type]: Request/issue a CAL (permanent or temporary)
 *   - licmgr revoke <id> [reason]: Revoke an issued CAL and reclaim capacity
 *   - licmgr upgrade <id>: Upgrade a temporary CAL to a permanent CAL
 *   - licmgr discover [scope]: Discover license servers in Workgroup, Domain, or Forest
 *   - licmgr set-mode <perdevice|peruser>: Configure active licensing mode
 *   - licmgr dump: Export declarative PowerShell RDS licensing configuration
 *   - licmgr help: Command usage and syntax
 */

#include <string>
#include <string_view>
#include <vector>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <cstdint>
#include <span>

#include "micant/um/servers/rdlicensing.hpp"

namespace micant::rdlic_cmd::um {

inline std::vector<std::string_view> Tokenize(std::string_view input) {
    std::vector<std::string_view> tokens;
    size_t pos = 0;
    while (pos < input.size()) {
        while (pos < input.size() && (input[pos] == ' ' || input[pos] == '\t' || input[pos] == '\r' || input[pos] == '\n')) {
            pos++;
        }
        if (pos >= input.size()) break;
        size_t start = pos;
        if (input[pos] == '\"') {
            start = ++pos;
            while (pos < input.size() && input[pos] != '\"') pos++;
            tokens.push_back(input.substr(start, pos - start));
            if (pos < input.size()) pos++;
        } else {
            while (pos < input.size() && input[pos] != ' ' && input[pos] != '\t' && input[pos] != '\r' && input[pos] != '\n') {
                pos++;
            }
            tokens.push_back(input.substr(start, pos - start));
        }
    }
    return tokens;
}

inline std::string ModeToString(micant::rdlic::LicensingMode mode) {
    switch (mode) {
        case micant::rdlic::LicModePerDevice: return "Per-Device";
        case micant::rdlic::LicModePerUser:   return "Per-User";
        default:                              return "Not Configured";
    }
}

inline std::string ScopeToString(micant::rdlic::DiscoveryScope scope) {
    switch (scope) {
        case micant::rdlic::LicScopeDomain:     return "Domain";
        case micant::rdlic::LicScopeEnterprise: return "Enterprise/Forest";
        default:                                return "Workgroup";
    }
}

inline std::string KeyPackTypeToString(micant::rdlic::KeyPackType type) {
    switch (type) {
        case micant::rdlic::KeyPackPerDevice: return "Per-Device";
        case micant::rdlic::KeyPackPerUser:   return "Per-User";
        case micant::rdlic::KeyPackTemporary: return "Temporary";
        case micant::rdlic::KeyPackBuiltIn:   return "Built-In";
        default:                              return "Unknown";
    }
}

inline std::string LicenseStatusToString(micant::rdlic::LicenseStatus st) {
    switch (st) {
        case micant::rdlic::LicenseActive:    return "Active";
        case micant::rdlic::LicenseTemporary: return "Temporary";
        case micant::rdlic::LicenseRevoked:   return "Revoked";
        case micant::rdlic::LicenseExpired:   return "Expired";
        case micant::rdlic::LicenseUpgraded:  return "Upgraded";
        default:                              return "Unknown";
    }
}

inline int ExecuteRdlic(std::span<std::string_view> args, std::string& output) {
    std::ostringstream out;
    auto& mgr = micant::rdlic::RdLicensingManager::get();

    if (args.empty() || args[0] == "help" || args[0] == "/?" || args[0] == "-h" || args[0] == "--help") {
        out << "MicaNT Remote Desktop Licensing (RD Licensing) Subsystem CLI [Windows Server Compatible]\n"
            << "Copyright (C) 2026 MicaNT Authors. Windows and Windows Server are trademarks of Microsoft Corporation.\n\n"
            << "Available Utilities:\n"
            << "  licmgr status / rdlic status                  Introspect license server status & CAL totals\n"
            << "  licmgr pack list                              Tabular inventory of installed CAL key packs\n"
            << "  licmgr pack add <type> <qty> [product]        Install a new CAL key pack\n"
            << "  licmgr lic list / rdlic list                  Tabular inventory of issued licenses\n"
            << "  licmgr issue <hwid> <machine> <user> [type]   Request/issue a CAL token\n"
            << "  licmgr revoke <id> [reason]                   Revoke an issued CAL\n"
            << "  licmgr upgrade <id>                           Upgrade temporary CAL to permanent CAL\n"
            << "  licmgr discover [scope]                       Discover license servers in Workgroup/Domain/Forest\n"
            << "  licmgr set-mode <perdevice|peruser>           Set active licensing mode\n"
            << "  licmgr dump                                   Export declarative PowerShell RDS licensing config\n"
            << "  licmgr help                                   Display command syntax\n";
        output = out.str();
        return 0;
    }

    std::string verb(args[0]);

    // Handle "licmgr" or "rdlic" as first token if passed directly
    if ((verb == "licmgr" || verb == "rdlic") && args.size() > 1) {
        return ExecuteRdlic(args.subspan(1), output);
    }

    // ------------------------------------------------------------------------
    // status
    // ------------------------------------------------------------------------
    if (verb == "status") {
        auto info = mgr.getServerInfo();
        auto gp = mgr.getGracePeriodStatus();

        out << "Remote Desktop Licensing Subsystem Status:\n"
            << "  Service Name:        TermServLicensing (Running)\n"
            << "  Server Name:         " << info.serverName << "\n"
            << "  Server FQDN:         " << info.serverFqdn << "\n"
            << "  Activation Status:   " << (info.activationStatus == micant::rdlic::ServerActivated ? "Activated" : "Not Activated") << "\n"
            << "  Activation Method:   " << info.activationMethod << "\n"
            << "  Licensing Mode:      " << ModeToString(mgr.getLicensingMode()) << "\n"
            << "  Discovery Scope:     " << ScopeToString(mgr.getDiscoveryScope()) << "\n"
            << "  Grace Period:        " << (gp.inGracePeriod ? "Active (" + std::to_string(gp.remainingGraceDays) + " days left)" : "Inactive (License Server Active)") << "\n"
            << "  Installed CALs:      " << mgr.getInstalledCalCount() << "\n"
            << "  Issued CALs:         " << mgr.getIssuedCalCount() << "\n"
            << "  Temporary CALs:      " << mgr.getTempCalCount() << "\n"
            << "  Revoked CALs:        " << mgr.getRevokedCalCount() << "\n"
            << "  Available CALs:      " << mgr.getAvailableCalCount() << "\n";
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // pack list / pack-list
    // ------------------------------------------------------------------------
    if (verb == "pack-list" || (verb == "pack" && args.size() > 1 && args[1] == "list")) {
        auto packs = mgr.getKeyPacks();
        out << std::left
            << std::setw(8)  << "PACKID"
            << std::setw(18) << "KEYPACKID"
            << std::setw(14) << "TYPE"
            << std::setw(10) << "TOTAL"
            << std::setw(10) << "ISSUED"
            << std::setw(10) << "AVAIL"
            << std::setw(10) << "REVOKED"
            << "PRODUCT\n";
        out << std::string(80, '-') << "\n";

        for (const auto& p : packs) {
            out << std::left
                << std::setw(8)  << p.packId
                << std::setw(18) << p.keyPackId
                << std::setw(14) << KeyPackTypeToString(p.keyPackType)
                << std::setw(10) << p.totalLicenses
                << std::setw(10) << p.issuedLicenses
                << std::setw(10) << p.availableLicenses
                << std::setw(10) << p.revokedLicenses
                << p.productName << "\n";
        }
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // pack add / pack-add <perdevice|peruser> <quantity> [product]
    // ------------------------------------------------------------------------
    if (verb == "pack-add" || (verb == "pack" && args.size() > 1 && args[1] == "add")) {
        size_t offset = (verb == "pack-add") ? 1 : 2;
        if (args.size() <= offset + 1) {
            out << "Error: pack add requires <type: perdevice|peruser> and <quantity>.\n";
            output = out.str();
            return 1;
        }

        std::string typeStr(args[offset]);
        uint32_t qty = 0;
        try {
            qty = static_cast<uint32_t>(std::stoul(std::string(args[offset + 1])));
        } catch (...) {
            out << "Error: Invalid quantity specified.\n";
            output = out.str();
            return 1;
        }

        micant::rdlic::KeyPackType kpType = micant::rdlic::KeyPackPerDevice;
        if (typeStr == "peruser" || typeStr == "user") {
            kpType = micant::rdlic::KeyPackPerUser;
        }

        std::string prod = "Windows Server 2025 Remote Desktop Services";
        if (args.size() > offset + 2) {
            prod = std::string(args[offset + 2]);
        }

        micant::rdlic::LicenseKeyPack pack{};
        pack.keyPackType = kpType;
        pack.totalLicenses = qty;
        pack.productName = prod;

        uint32_t packId = mgr.installKeyPack(pack);
        out << "SUCCESS: Installed CAL Key Pack ID " << packId
            << " (" << KeyPackTypeToString(kpType) << ", " << qty << " licenses)\n";
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // lic list / lic-list
    // ------------------------------------------------------------------------
    if (verb == "lic-list" || (verb == "lic" && args.size() > 1 && args[1] == "list") || verb == "list") {
        auto lics = mgr.getIssuedLicenses();
        out << std::left
            << std::setw(8)  << "LICID"
            << std::setw(8)  << "PACKID"
            << std::setw(14) << "TYPE"
            << std::setw(12) << "STATUS"
            << std::setw(26) << "HWID"
            << std::setw(20) << "MACHINE"
            << "USER\n";
        out << std::string(96, '-') << "\n";

        for (const auto& l : lics) {
            out << std::left
                << std::setw(8)  << l.licenseId
                << std::setw(8)  << l.packId
                << std::setw(14) << KeyPackTypeToString(l.licenseType)
                << std::setw(12) << LicenseStatusToString(l.status)
                << std::setw(26) << l.clientHardwareId
                << std::setw(20) << l.clientMachineName
                << (l.domainName.empty() ? "" : l.domainName + "\\") << l.userName << "\n";
        }
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // issue <hwid> <machine> <user> [perdevice|peruser]
    // ------------------------------------------------------------------------
    if (verb == "issue") {
        if (args.size() < 4) {
            out << "Error: issue requires <hwid> <machine> <user> [type: perdevice|peruser].\n";
            output = out.str();
            return 1;
        }

        std::string hwid(args[1]);
        std::string machine(args[2]);
        std::string user(args[3]);
        micant::rdlic::KeyPackType kpType = micant::rdlic::KeyPackPerDevice;
        if (args.size() > 4) {
            std::string t(args[4]);
            if (t == "peruser" || t == "user") {
                kpType = micant::rdlic::KeyPackPerUser;
            }
        }

        micant::rdlic::LicenseRequest req{};
        req.hwid = hwid;
        req.machineName = machine;
        req.userName = user;
        req.requestedType = kpType;

        micant::rdlic::IssuedLicense lic{};
        int res = mgr.requestLicense(req, lic);
        if (res == 0) {
            out << "SUCCESS: Issued " << LicenseStatusToString(lic.status) << " CAL ID " << lic.licenseId
                << " (Type=" << KeyPackTypeToString(lic.licenseType)
                << ", Token=" << lic.licenseToken << ")\n";
            output = out.str();
            return 0;
        } else {
            out << "Error: Failed to issue license (error code " << res << ").\n";
            output = out.str();
            return 1;
        }
    }

    // ------------------------------------------------------------------------
    // revoke <licenseId> [reason]
    // ------------------------------------------------------------------------
    if (verb == "revoke") {
        if (args.size() < 2) {
            out << "Error: revoke requires <licenseId>.\n";
            output = out.str();
            return 1;
        }

        uint32_t licId = 0;
        try {
            licId = static_cast<uint32_t>(std::stoul(std::string(args[1])));
        } catch (...) {
            out << "Error: Invalid license ID.\n";
            output = out.str();
            return 1;
        }

        micant::rdlic::RevokeReason reason = micant::rdlic::RevokeReasonAdminAction;
        if (args.size() > 2) {
            std::string r(args[2]);
            if (r == "hardware") reason = micant::rdlic::RevokeReasonHardwareReplaced;
            else if (r == "decommission") reason = micant::rdlic::RevokeReasonComputerDecommissioned;
        }

        if (mgr.revokeLicense(licId, reason)) {
            out << "SUCCESS: Revoked license ID " << licId << ".\n";
            output = out.str();
            return 0;
        } else {
            out << "Error: License ID " << licId << " not found or already revoked.\n";
            output = out.str();
            return 1;
        }
    }

    // ------------------------------------------------------------------------
    // upgrade <licenseId>
    // ------------------------------------------------------------------------
    if (verb == "upgrade") {
        if (args.size() < 2) {
            out << "Error: upgrade requires <licenseId>.\n";
            output = out.str();
            return 1;
        }

        uint32_t licId = 0;
        try {
            licId = static_cast<uint32_t>(std::stoul(std::string(args[1])));
        } catch (...) {
            out << "Error: Invalid license ID.\n";
            output = out.str();
            return 1;
        }

        if (mgr.upgradeTemporaryLicense(licId)) {
            out << "SUCCESS: Upgraded temporary license ID " << licId << " to permanent CAL.\n";
            output = out.str();
            return 0;
        } else {
            out << "Error: Cannot upgrade license ID " << licId << " (not temporary or no seats available).\n";
            output = out.str();
            return 1;
        }
    }

    // ------------------------------------------------------------------------
    // discover [workgroup|domain|forest]
    // ------------------------------------------------------------------------
    if (verb == "discover") {
        micant::rdlic::DiscoveryScope scope = micant::rdlic::LicScopeDomain;
        if (args.size() > 1) {
            std::string s(args[1]);
            if (s == "workgroup") scope = micant::rdlic::LicScopeWorkgroup;
            else if (s == "forest" || s == "enterprise") scope = micant::rdlic::LicScopeEnterprise;
        }

        auto servers = mgr.discoverLicenseServers(scope);
        out << "Remote Desktop License Server Discovery (" << ScopeToString(scope) << "):\n";
        for (const auto& s : servers) {
            out << "  - " << s.serverName << " (" << s.serverFqdn << ") [Scope: "
                << ScopeToString(s.scope) << ", Status: "
                << (s.activationStatus == micant::rdlic::ServerActivated ? "Activated" : "Not Activated")
                << "]\n";
        }
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // set-mode <perdevice|peruser>
    // ------------------------------------------------------------------------
    if (verb == "set-mode") {
        if (args.size() < 2) {
            out << "Error: set-mode requires <perdevice|peruser>.\n";
            output = out.str();
            return 1;
        }
        std::string modeStr(args[1]);
        if (modeStr == "perdevice" || modeStr == "device") {
            mgr.setLicensingMode(micant::rdlic::LicModePerDevice);
            out << "SUCCESS: Licensing mode set to Per-Device.\n";
        } else if (modeStr == "peruser" || modeStr == "user") {
            mgr.setLicensingMode(micant::rdlic::LicModePerUser);
            out << "SUCCESS: Licensing mode set to Per-User.\n";
        } else {
            out << "Error: Invalid mode '" << modeStr << "'. Use 'perdevice' or 'peruser'.\n";
            output = out.str();
            return 1;
        }
        output = out.str();
        return 0;
    }

    // ------------------------------------------------------------------------
    // dump
    // ------------------------------------------------------------------------
    if (verb == "dump") {
        auto info = mgr.getServerInfo();
        out << "# MicaNT Sovereign Remote Desktop Licensing Configuration Export\n"
            << "# Generated: 2026-11-01 [MS-RDPELE Architecture / lserver.exe]\n\n"
            << "# SCM Service Configuration\n"
            << "sc.exe config TermServLicensing start= auto\n"
            << "net start TermServLicensing\n\n"
            << "# Licensing Server Mode\n"
            << "# LicensingType: " << (mgr.getLicensingMode() == micant::rdlic::LicModePerDevice ? "2 (Per-Device)" : "4 (Per-User)") << "\n"
            << "reg add \"HKLM\\SYSTEM\\CurrentControlSet\\Control\\Terminal Server\\RCM\\Licensing Core\" /v LicensingMode /t REG_DWORD /d "
            << static_cast<uint32_t>(mgr.getLicensingMode()) << " /f\n\n"
            << "# Installed CAL Key Packs\n";

        for (const auto& p : mgr.getKeyPacks()) {
            out << "# KeyPack " << p.packId << ": " << p.keyPackId
                << " (" << KeyPackTypeToString(p.keyPackType) << ", Total=" << p.totalLicenses
                << ", Avail=" << p.availableLicenses << ")\n";
        }
        output = out.str();
        return 0;
    }

    out << "Error: Unknown RD Licensing command '" << verb << "'. Type 'licmgr help' for usage.\n";
    output = out.str();
    return 1;
}

inline int ExecuteRdlicString(std::string_view commandLine, std::string& output) {
    auto tokens = Tokenize(commandLine);
    if (tokens.empty()) {
        output.clear();
        return 0;
    }
    std::string firstTok(tokens[0]);
    if (firstTok.ends_with(".exe")) {
        firstTok = firstTok.substr(0, firstTok.length() - 4);
    }
    if (firstTok == "licmgr" || firstTok == "rdlic") {
        tokens[0] = tokens[0].substr(0, tokens[0].find('.'));
    }
    return ExecuteRdlic(tokens, output);
}

} // namespace micant::rdlic_cmd::um
