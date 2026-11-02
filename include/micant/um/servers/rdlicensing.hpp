#pragma once

/**
 * @file rdlicensing.hpp
 * @brief Clean-room Remote Desktop Licensing Subsystem (MS-RDPELE).
 *
 * Implements the Win32 Remote Desktop Protocol: Licensing Extension (MS-RDPELE)
 * architecture, Terminal Server Licensing service (lserver.exe, TermServLicensing),
 * Client Access License (CAL) key packs (Per-Device CALs and Per-User CALs),
 * license server discovery scopes (Workgroup, Domain, Forest/Enterprise),
 * 120-day grace period tracking, 90-day temporary license token issuance,
 * hardware ID (HWID) hashing, license revocation, and upgrade mechanics.
 *
 * Strict clean-room implementation referencing Microsoft's MIT-licensed win32metadata
 * under 17 U.S.C. § 1201(f) & Google LLC v. Oracle America, Inc. (141 S. Ct. 1183, 2021).
 * Zero proprietary, leaked, or decompiled code.
 *
 * Trademark Notice: Windows, Windows Server, and Remote Desktop are trademarks
 * or registered trademarks of Microsoft Corporation in the United States and other
 * countries. Referenced solely for interoperability and compatibility purposes.
 */

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <algorithm>
#include <mutex>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <chrono>

#include "micant/shared/ntdef.hpp"
#include "micant/um/native/ldr.hpp"
#include "micant/um/servers/scm.hpp"
#include "micant/um/subsystems/winbase/version.hpp"
#include "micant/shared/ntstatus.hpp"

namespace micant::rdlic {

// ============================================================================
// 1. Win32 RD Licensing Constants & Enums (win32metadata / MS-RDPELE)
// ============================================================================

inline constexpr uintptr_t TLS_CURRENT_SERVER_HANDLE = 0;
inline constexpr uint16_t  TLS_RPC_DEFAULT_PORT      = 135;
inline constexpr uint32_t  RDS_GRACE_PERIOD_DAYS     = 120;
inline constexpr uint32_t  RDS_TEMP_CAL_DAYS         = 90;

// Licensing Modes (matching GPO / WMI Win32_TerminalServiceSetting)
enum LicensingMode : uint32_t {
    LicModeNotConfigured = 0,
    LicModePerDevice     = 2,
    LicModePerUser       = 4
};

// Discovery Scopes
enum DiscoveryScope : uint32_t {
    LicScopeWorkgroup  = 0,
    LicScopeDomain     = 1,
    LicScopeEnterprise = 2 // Forest-wide
};

// Key Pack Types
enum KeyPackType : uint32_t {
    KeyPackUnknown   = 0,
    KeyPackTemporary = 1,
    KeyPackPerDevice = 2,
    KeyPackPerUser   = 3,
    KeyPackBuiltIn   = 4
};

// License Status
enum LicenseStatus : uint32_t {
    LicenseActive    = 1,
    LicenseTemporary = 2,
    LicenseRevoked   = 3,
    LicenseExpired   = 4,
    LicenseUpgraded  = 5
};

// Server Activation Status
enum ServerActivationStatus : uint32_t {
    ServerNotActivated = 0,
    ServerActivated    = 1,
    ServerPending      = 2
};

// Revocation Reasons
enum RevokeReason : uint32_t {
    RevokeReasonUnspecified           = 0,
    RevokeReasonHardwareReplaced      = 1,
    RevokeReasonComputerDecommissioned = 2,
    RevokeReasonAdminAction           = 3
};

// ============================================================================
// 2. Data Structures
// ============================================================================

struct LicenseKeyPack {
    uint32_t    packId = 0;
    std::string keyPackId;            // e.g. "KP-2026-DEV-001"
    std::string productName;          // e.g. "Windows Server 2025 Remote Desktop Services"
    uint32_t    productVersion = 0x000A0000; // 10.0
    KeyPackType keyPackType = KeyPackPerDevice;
    uint32_t    totalLicenses = 0;
    uint32_t    availableLicenses = 0;
    uint32_t    issuedLicenses = 0;
    uint32_t    revokedLicenses = 0;
    uint64_t    installTimestamp = 0;
    uint64_t    expirationTimestamp = 0; // 0 = Perpetual
};

struct IssuedLicense {
    uint32_t      licenseId = 0;
    uint32_t      packId = 0;
    KeyPackType   licenseType = KeyPackPerDevice;
    std::string   clientHardwareId;    // e.g. "HWID-A1B2-C3D4-E5F6"
    std::string   clientMachineName;   // e.g. "WS-FINANCE-01"
    std::string   userName;            // e.g. "john.doe"
    std::string   domainName;          // e.g. "CORP"
    uint64_t      issueDate = 0;
    uint64_t      expireDate = 0;
    LicenseStatus status = LicenseActive;
    uint32_t      version = 0x000A0000;
    std::string   licenseToken;        // MS-RDPELE token payload
    RevokeReason  revokeReason = RevokeReasonUnspecified;
};

struct LicenseServerInfo {
    std::string            serverName = "MICANT-LIC-01";
    std::string            serverFqdn = "lic01.corp.micant.local";
    DiscoveryScope         scope = LicScopeDomain;
    ServerActivationStatus activationStatus = ServerActivated;
    std::string            activationMethod = "Automatic";
    uint32_t               version = 0x000A0000;
    bool                   isOnline = true;
};

struct GracePeriodStatus {
    bool     inGracePeriod = false;
    uint32_t totalGraceDays = RDS_GRACE_PERIOD_DAYS;
    uint32_t remainingGraceDays = RDS_GRACE_PERIOD_DAYS;
    uint64_t gracePeriodStartDate = 0;
};

struct LicenseRequest {
    std::string hwid;
    std::string machineName;
    std::string userName;
    std::string domainName;
    KeyPackType requestedType = KeyPackPerDevice;
    std::string existingToken;
};

// ============================================================================
// 3. RdLicensingManager Subsystem Singleton
// ============================================================================

class RdLicensingManager {
public:
    static RdLicensingManager& get() {
        static RdLicensingManager instance;
        return instance;
    }

    // Subsystem lifecycle
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        resetInternal();
    }

    void resetToDefault() {
        reset();
    }

    // Server Configuration
    LicenseServerInfo getServerInfo() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return serverInfo_;
    }

    void setServerInfo(const LicenseServerInfo& info) {
        std::lock_guard<std::mutex> lock(mutex_);
        serverInfo_ = info;
    }

    LicensingMode getLicensingMode() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return licensingMode_;
    }

    void setLicensingMode(LicensingMode mode) {
        std::lock_guard<std::mutex> lock(mutex_);
        licensingMode_ = mode;
    }

    DiscoveryScope getDiscoveryScope() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return discoveryScope_;
    }

    void setDiscoveryScope(DiscoveryScope scope) {
        std::lock_guard<std::mutex> lock(mutex_);
        discoveryScope_ = scope;
        serverInfo_.scope = scope;
    }

    bool isActivated() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return serverInfo_.activationStatus == ServerActivated;
    }

    void setActivated(bool activated) {
        std::lock_guard<std::mutex> lock(mutex_);
        serverInfo_.activationStatus = activated ? ServerActivated : ServerNotActivated;
    }

    // Grace Period Management
    GracePeriodStatus getGracePeriodStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return gracePeriod_;
    }

    void setGracePeriod(bool inGrace, uint32_t remainingDays) {
        std::lock_guard<std::mutex> lock(mutex_);
        gracePeriod_.inGracePeriod = inGrace;
        gracePeriod_.remainingGraceDays = remainingDays;
    }

    // Key Pack Management
    uint32_t installKeyPack(LicenseKeyPack pack) {
        std::lock_guard<std::mutex> lock(mutex_);
        pack.packId = nextPackId_++;
        if (pack.keyPackId.empty()) {
            std::ostringstream ss;
            ss << "KP-2026-" << (pack.keyPackType == KeyPackPerDevice ? "DEV" : "USR")
               << "-" << std::setfill('0') << std::setw(3) << pack.packId;
            pack.keyPackId = ss.str();
        }
        if (pack.installTimestamp == 0) {
            pack.installTimestamp = getCurrentTimestamp();
        }
        pack.availableLicenses = pack.totalLicenses;
        pack.issuedLicenses = 0;
        pack.revokedLicenses = 0;

        keyPacks_.push_back(pack);
        return pack.packId;
    }

    bool removeKeyPack(uint32_t packId) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::find_if(keyPacks_.begin(), keyPacks_.end(),
                               [packId](const LicenseKeyPack& p) { return p.packId == packId; });
        if (it != keyPacks_.end()) {
            keyPacks_.erase(it);
            return true;
        }
        return false;
    }

    bool getKeyPack(uint32_t packId, LicenseKeyPack& outPack) const {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& p : keyPacks_) {
            if (p.packId == packId) {
                outPack = p;
                return true;
            }
        }
        return false;
    }

    std::vector<LicenseKeyPack> getKeyPacks() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return keyPacks_;
    }

    // MS-RDPELE License Issuance & Evaluation Engine
    int requestLicense(const LicenseRequest& req, IssuedLicense& outLicense) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (req.hwid.empty() && req.userName.empty()) {
            return -1; // Invalid request parameters
        }

        uint64_t now = getCurrentTimestamp();
        std::string normalizedHwid = normalizeHwid(req.hwid);

        // 1. Existing Token Validation (Idempotent client reconnect or renewal)
        if (!req.existingToken.empty()) {
            for (auto& lic : issuedLicenses_) {
                if (lic.licenseToken == req.existingToken &&
                    (lic.status == LicenseActive || lic.status == LicenseTemporary)) {
                    if (lic.expireDate == 0 || lic.expireDate > now) {
                        outLicense = lic;
                        return 0; // Existing valid token confirmed
                    }
                }
            }
        }

        // 2. Per-Device CAL Workflow
        if (req.requestedType == KeyPackPerDevice) {
            // Check if device already holds an active license
            for (const auto& lic : issuedLicenses_) {
                if (lic.clientHardwareId == normalizedHwid &&
                    (lic.status == LicenseActive || lic.status == LicenseTemporary)) {
                    if (lic.expireDate == 0 || lic.expireDate > now) {
                        outLicense = lic;
                        return 0; // Idempotent return of existing active license
                    }
                }
            }

            // Search for available Per-Device Key Pack
            LicenseKeyPack* targetPack = nullptr;
            for (auto& pack : keyPacks_) {
                if (pack.keyPackType == KeyPackPerDevice && pack.availableLicenses > 0) {
                    targetPack = &pack;
                    break;
                }
            }

            if (targetPack) {
                // Issue Permanent Per-Device CAL
                targetPack->availableLicenses--;
                targetPack->issuedLicenses++;

                IssuedLicense newLic{};
                newLic.licenseId = nextLicenseId_++;
                newLic.packId = targetPack->packId;
                newLic.licenseType = KeyPackPerDevice;
                newLic.clientHardwareId = normalizedHwid;
                newLic.clientMachineName = req.machineName.empty() ? "UNKNOWN-CLIENT" : req.machineName;
                newLic.userName = req.userName.empty() ? "DEVICE-ACCOUNT" : req.userName;
                newLic.domainName = req.domainName.empty() ? "WORKGROUP" : req.domainName;
                newLic.issueDate = now;
                newLic.expireDate = now + (90ULL * 86400ULL); // Standard 90-day renewal cycle
                newLic.status = LicenseActive;
                newLic.version = 0x000A0000;
                newLic.licenseToken = generateLicenseToken(newLic);

                issuedLicenses_.push_back(newLic);
                outLicense = newLic;
                return 0;
            } else {
                // No permanent Per-Device CALs available: Issue Temporary CAL (90 days)
                IssuedLicense tempLic{};
                tempLic.licenseId = nextLicenseId_++;
                tempLic.packId = 0; // Pack 0 indicates temporary token
                tempLic.licenseType = KeyPackTemporary;
                tempLic.clientHardwareId = normalizedHwid;
                tempLic.clientMachineName = req.machineName.empty() ? "UNKNOWN-CLIENT" : req.machineName;
                tempLic.userName = req.userName.empty() ? "DEVICE-ACCOUNT" : req.userName;
                tempLic.domainName = req.domainName.empty() ? "WORKGROUP" : req.domainName;
                tempLic.issueDate = now;
                tempLic.expireDate = now + (static_cast<uint64_t>(RDS_TEMP_CAL_DAYS) * 86400ULL);
                tempLic.status = LicenseTemporary;
                tempLic.version = 0x000A0000;
                tempLic.licenseToken = generateLicenseToken(tempLic);

                issuedLicenses_.push_back(tempLic);
                outLicense = tempLic;
                return 0;
            }
        }

        // 3. Per-User CAL Workflow
        if (req.requestedType == KeyPackPerUser) {
            std::string userKey = req.userName;
            if (!req.domainName.empty()) {
                userKey = req.domainName + "\\" + req.userName;
            }

            // Check if user already holds an active license
            for (const auto& lic : issuedLicenses_) {
                if (lic.userName == req.userName && lic.domainName == req.domainName &&
                    lic.status == LicenseActive) {
                    outLicense = lic;
                    return 0;
                }
            }

            // Search for available Per-User Key Pack
            LicenseKeyPack* targetPack = nullptr;
            for (auto& pack : keyPacks_) {
                if (pack.keyPackType == KeyPackPerUser && pack.availableLicenses > 0) {
                    targetPack = &pack;
                    break;
                }
            }

            if (targetPack) {
                targetPack->availableLicenses--;
                targetPack->issuedLicenses++;

                IssuedLicense newLic{};
                newLic.licenseId = nextLicenseId_++;
                newLic.packId = targetPack->packId;
                newLic.licenseType = KeyPackPerUser;
                newLic.clientHardwareId = normalizedHwid;
                newLic.clientMachineName = req.machineName;
                newLic.userName = req.userName;
                newLic.domainName = req.domainName;
                newLic.issueDate = now;
                newLic.expireDate = 0; // Perpetual per-user entitlement
                newLic.status = LicenseActive;
                newLic.version = 0x000A0000;
                newLic.licenseToken = generateLicenseToken(newLic);

                issuedLicenses_.push_back(newLic);
                outLicense = newLic;
                return 0;
            } else {
                // Issue Temporary Token
                IssuedLicense tempLic{};
                tempLic.licenseId = nextLicenseId_++;
                tempLic.packId = 0;
                tempLic.licenseType = KeyPackTemporary;
                tempLic.clientHardwareId = normalizedHwid;
                tempLic.clientMachineName = req.machineName;
                tempLic.userName = req.userName;
                tempLic.domainName = req.domainName;
                tempLic.issueDate = now;
                tempLic.expireDate = now + (static_cast<uint64_t>(RDS_TEMP_CAL_DAYS) * 86400ULL);
                tempLic.status = LicenseTemporary;
                tempLic.version = 0x000A0000;
                tempLic.licenseToken = generateLicenseToken(tempLic);

                issuedLicenses_.push_back(tempLic);
                outLicense = tempLic;
                return 0;
            }
        }

        return -2; // Unsupported request type
    }

    // Token Verification
    bool verifyLicenseToken(const std::string& token, const std::string& hwid) const {
        std::lock_guard<std::mutex> lock(mutex_);
        uint64_t now = getCurrentTimestamp();
        std::string normHwid = normalizeHwid(hwid);

        for (const auto& lic : issuedLicenses_) {
            if (lic.licenseToken == token && (lic.status == LicenseActive || lic.status == LicenseTemporary)) {
                if (lic.clientHardwareId == normHwid) {
                    if (lic.expireDate == 0 || lic.expireDate > now) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    // CAL Revocation
    bool revokeLicense(uint32_t licenseId, RevokeReason reason = RevokeReasonAdminAction) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& lic : issuedLicenses_) {
            if (lic.licenseId == licenseId && lic.status != LicenseRevoked) {
                lic.status = LicenseRevoked;
                lic.revokeReason = reason;

                // If permanent license attached to a pack, return license to key pack
                if (lic.packId != 0) {
                    for (auto& pack : keyPacks_) {
                        if (pack.packId == lic.packId) {
                            if (pack.issuedLicenses > 0) pack.issuedLicenses--;
                            pack.availableLicenses++;
                            pack.revokedLicenses++;
                            break;
                        }
                    }
                }
                return true;
            }
        }
        return false;
    }

    // Upgrade Temporary License to Permanent CAL
    bool upgradeTemporaryLicense(uint32_t licenseId) {
        std::lock_guard<std::mutex> lock(mutex_);
        IssuedLicense* targetLic = nullptr;
        for (auto& lic : issuedLicenses_) {
            if (lic.licenseId == licenseId && lic.status == LicenseTemporary) {
                targetLic = &lic;
                break;
            }
        }

        if (!targetLic) return false;

        // Search for available Per-Device Key Pack
        for (auto& pack : keyPacks_) {
            if (pack.keyPackType == KeyPackPerDevice && pack.availableLicenses > 0) {
                pack.availableLicenses--;
                pack.issuedLicenses++;

                uint64_t now = getCurrentTimestamp();
                targetLic->packId = pack.packId;
                targetLic->licenseType = KeyPackPerDevice;
                targetLic->status = LicenseActive;
                targetLic->issueDate = now;
                targetLic->expireDate = now + (90ULL * 86400ULL);
                targetLic->licenseToken = generateLicenseToken(*targetLic);
                return true;
            }
        }
        return false; // No available seats
    }

    // Reissue CAL for Hardware Replacement
    bool reissueLicense(uint32_t licenseId, const std::string& newHwid) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& lic : issuedLicenses_) {
            if (lic.licenseId == licenseId && lic.status == LicenseActive) {
                lic.clientHardwareId = normalizeHwid(newHwid);
                lic.issueDate = getCurrentTimestamp();
                lic.licenseToken = generateLicenseToken(lic);
                return true;
            }
        }
        return false;
    }

    // Introspection Queries
    std::vector<IssuedLicense> getIssuedLicenses() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return issuedLicenses_;
    }

    bool findLicenseById(uint32_t licenseId, IssuedLicense& outLic) const {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& lic : issuedLicenses_) {
            if (lic.licenseId == licenseId) {
                outLic = lic;
                return true;
            }
        }
        return false;
    }

    bool findLicenseByHwid(const std::string& hwid, IssuedLicense& outLic) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string normHwid = normalizeHwid(hwid);
        for (const auto& lic : issuedLicenses_) {
            if (lic.clientHardwareId == normHwid && lic.status != LicenseRevoked) {
                outLic = lic;
                return true;
            }
        }
        return false;
    }

    std::vector<IssuedLicense> findLicensesByUser(const std::string& userName) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<IssuedLicense> results;
        for (const auto& lic : issuedLicenses_) {
            if (lic.userName == userName && lic.status != LicenseRevoked) {
                results.push_back(lic);
            }
        }
        return results;
    }

    // Telemetry & Metrics
    uint32_t getInstalledCalCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        uint32_t total = 0;
        for (const auto& p : keyPacks_) {
            total += p.totalLicenses;
        }
        return total;
    }

    uint32_t getIssuedCalCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        uint32_t count = 0;
        for (const auto& l : issuedLicenses_) {
            if (l.status == LicenseActive) {
                count++;
            }
        }
        return count;
    }

    uint32_t getTempCalCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        uint32_t count = 0;
        for (const auto& l : issuedLicenses_) {
            if (l.status == LicenseTemporary) {
                count++;
            }
        }
        return count;
    }

    uint32_t getRevokedCalCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        uint32_t count = 0;
        for (const auto& l : issuedLicenses_) {
            if (l.status == LicenseRevoked) {
                count++;
            }
        }
        return count;
    }

    uint32_t getAvailableCalCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        uint32_t avail = 0;
        for (const auto& p : keyPacks_) {
            avail += p.availableLicenses;
        }
        return avail;
    }

    // Discovery Service
    std::vector<LicenseServerInfo> discoverLicenseServers(DiscoveryScope scope) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<LicenseServerInfo> servers;
        servers.push_back(serverInfo_);

        for (const auto& s : discoveredServers_) {
            if (s.scope <= scope) {
                servers.push_back(s);
            }
        }
        return servers;
    }

    void registerDiscoveredServer(const LicenseServerInfo& srv) {
        std::lock_guard<std::mutex> lock(mutex_);
        discoveredServers_.push_back(srv);
    }

    // Hardware ID Hashing
    static std::string computeHardwareIdHash(std::string_view raw) {
        // Clean-room FNV-1a 64-bit hashing formatted as canonical HWID-XXXX-XXXX-XXXX-XXXX
        uint64_t hash = 14695981039346656037ULL;
        for (char c : raw) {
            hash ^= static_cast<uint8_t>(c);
            hash *= 1099511628211ULL;
        }
        std::ostringstream ss;
        ss << "HWID-"
           << std::hex << std::uppercase << std::setfill('0')
           << std::setw(4) << ((hash >> 48) & 0xFFFF) << "-"
           << std::setw(4) << ((hash >> 32) & 0xFFFF) << "-"
           << std::setw(4) << ((hash >> 16) & 0xFFFF) << "-"
           << std::setw(4) << (hash & 0xFFFF);
        return ss.str();
    }

private:
    RdLicensingManager() {
        resetInternal();
    }

    void resetInternal() {
        serverInfo_.serverName = "MICANT-LIC-01";
        serverInfo_.serverFqdn = "lic01.corp.micant.local";
        serverInfo_.scope = LicScopeDomain;
        serverInfo_.activationStatus = ServerActivated;
        serverInfo_.activationMethod = "Automatic";
        serverInfo_.version = 0x000A0000;
        serverInfo_.isOnline = true;

        licensingMode_ = LicModePerDevice;
        discoveryScope_ = LicScopeDomain;

        gracePeriod_.inGracePeriod = false;
        gracePeriod_.totalGraceDays = RDS_GRACE_PERIOD_DAYS;
        gracePeriod_.remainingGraceDays = RDS_GRACE_PERIOD_DAYS;
        gracePeriod_.gracePeriodStartDate = getCurrentTimestamp();

        keyPacks_.clear();
        issuedLicenses_.clear();
        discoveredServers_.clear();

        nextPackId_ = 1;
        nextLicenseId_ = 1000;

        // Provision Default Key Pack 1: Windows Server 2025 RDS Per-Device (50 CALs)
        LicenseKeyPack pack1{};
        pack1.packId = nextPackId_++;
        pack1.keyPackId = "KP-2026-DEV-001";
        pack1.productName = "Windows Server 2025 Remote Desktop Services";
        pack1.productVersion = 0x000A0000;
        pack1.keyPackType = KeyPackPerDevice;
        pack1.totalLicenses = 50;
        pack1.availableLicenses = 50;
        pack1.issuedLicenses = 0;
        pack1.revokedLicenses = 0;
        pack1.installTimestamp = getCurrentTimestamp();
        pack1.expirationTimestamp = 0;
        keyPacks_.push_back(pack1);

        // Provision Default Key Pack 2: Windows Server 2025 RDS Per-User (100 CALs)
        LicenseKeyPack pack2{};
        pack2.packId = nextPackId_++;
        pack2.keyPackId = "KP-2026-USR-002";
        pack2.productName = "Windows Server 2025 Remote Desktop Services";
        pack2.productVersion = 0x000A0000;
        pack2.keyPackType = KeyPackPerUser;
        pack2.totalLicenses = 100;
        pack2.availableLicenses = 100;
        pack2.issuedLicenses = 0;
        pack2.revokedLicenses = 0;
        pack2.installTimestamp = getCurrentTimestamp();
        pack2.expirationTimestamp = 0;
        keyPacks_.push_back(pack2);
    }

    static uint64_t getCurrentTimestamp() {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    }

    static std::string normalizeHwid(std::string_view hwid) {
        if (hwid.starts_with("HWID-")) {
            return std::string(hwid);
        }
        return computeHardwareIdHash(hwid);
    }

    static std::string generateLicenseToken(const IssuedLicense& lic) {
        std::ostringstream ss;
        ss << "MS-RDPELE-TOK-"
           << std::hex << std::uppercase << std::setfill('0')
           << std::setw(4) << lic.licenseId << "-"
           << std::setw(4) << lic.packId << "-"
           << std::setw(2) << static_cast<uint32_t>(lic.licenseType) << "-"
           << std::setw(8) << lic.issueDate;
        return ss.str();
    }

    mutable std::mutex          mutex_;
    LicenseServerInfo           serverInfo_;
    LicensingMode               licensingMode_ = LicModePerDevice;
    DiscoveryScope              discoveryScope_ = LicScopeDomain;
    GracePeriodStatus           gracePeriod_;
    uint32_t                    nextPackId_ = 1;
    uint32_t                    nextLicenseId_ = 1000;
    std::vector<LicenseKeyPack> keyPacks_;
    std::vector<IssuedLicense>  issuedLicenses_;
    std::vector<LicenseServerInfo> discoveredServers_;
};

// ============================================================================
// 4. Win32 C ABI Exports for tlsapi.dll & lserver.exe
// ============================================================================

extern "C" {

inline int32_t TLSConnectToLsServer(const wchar_t* pszServer, void** phServer) {
    if (!phServer) return -1;
    *phServer = reinterpret_cast<void*>(TLS_CURRENT_SERVER_HANDLE + 1);
    return 0; // Success
}

inline int32_t TLSDisconnectFromServer(void* hServer) {
    (void)hServer;
    return 0;
}

inline int32_t TLSRequestNewLicense(void* hServer, const wchar_t* pszHwid, const wchar_t* pszMachine,
                                    const wchar_t* pszUser, uint32_t* pLicId) {
    (void)hServer;
    if (!pszHwid || !pLicId) return -1;

    std::string hwid;
    while (*pszHwid) hwid.push_back(static_cast<char>(*pszHwid++));
    std::string machine;
    if (pszMachine) {
        while (*pszMachine) machine.push_back(static_cast<char>(*pszMachine++));
    }
    std::string user;
    if (pszUser) {
        while (*pszUser) user.push_back(static_cast<char>(*pszUser++));
    }

    LicenseRequest req{};
    req.hwid = hwid;
    req.machineName = machine;
    req.userName = user;
    req.requestedType = KeyPackPerDevice;

    IssuedLicense outLic{};
    int res = RdLicensingManager::get().requestLicense(req, outLic);
    if (res == 0) {
        *pLicId = outLic.licenseId;
        return 0;
    }
    return res;
}

inline int32_t TLSIssuePlatformChallenge(void* hServer, uint32_t licId, uint8_t* pChallenge, uint32_t len) {
    (void)hServer;
    (void)licId;
    if (!pChallenge || len < 16) return -1;
    for (uint32_t i = 0; i < len; ++i) {
        pChallenge[i] = static_cast<uint8_t>((licId * 37 + i * 17) & 0xFF);
    }
    return 0;
}

inline int32_t TLSRevokeLicense(void* hServer, uint32_t licId) {
    (void)hServer;
    return RdLicensingManager::get().revokeLicense(licId, RevokeReasonAdminAction) ? 0 : -1;
}

} // extern "C"

// ============================================================================
// 5. Registration & Subsystem Initialization
// ============================================================================

inline void InitializeRdLicensingSubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();

    // 1. tlsapi.dll exports (Terminal Server Licensing Client API)
    ldr.registerExport("tlsapi.dll", "TLSConnectToLsServer", reinterpret_cast<void*>(TLSConnectToLsServer));
    ldr.registerExport("tlsapi.dll", "TLSDisconnectFromServer", reinterpret_cast<void*>(TLSDisconnectFromServer));
    ldr.registerExport("tlsapi.dll", "TLSRequestNewLicense", reinterpret_cast<void*>(TLSRequestNewLicense));
    ldr.registerExport("tlsapi.dll", "TLSIssuePlatformChallenge", reinterpret_cast<void*>(TLSIssuePlatformChallenge));
    ldr.registerExport("tlsapi.dll", "TLSRevokeLicense", reinterpret_cast<void*>(TLSRevokeLicense));

    // 2. lserver.exe exports (Terminal Server Licensing Service Engine)
    ldr.registerExport("lserver.exe", "ServiceMain", reinterpret_cast<void*>(TLSConnectToLsServer));

    // 3. SCM Service: TermServLicensing ("Remote Desktop Licensing")
    auto& scm = scm::ServiceControlManager::get();
    auto licRecord = std::make_shared<scm::ServiceRecord>();
    licRecord->serviceName = L"TermServLicensing";
    licRecord->displayName = L"Remote Desktop Licensing";
    licRecord->serviceType = scm::SERVICE_WIN32_OWN_PROCESS;
    licRecord->startType = scm::SERVICE_AUTO_START;
    licRecord->errorControl = scm::SERVICE_ERROR_NORMAL;
    licRecord->binaryPath = L"C:\\Windows\\System32\\lserver.exe";
    licRecord->status.dwServiceType = licRecord->serviceType;
    licRecord->status.dwCurrentState = scm::SERVICE_RUNNING;
    licRecord->status.dwProcessId = 1204;
    scm.registerServiceRecord(licRecord);
}

inline void RegisterRdLicensingSubsystem() {
    // 1. SCM Services & Exports Initialization
    InitializeRdLicensingSubsystemExports();

    // 2. Version Database PE Module Registration (6 modules)
    auto& vdb = micant::version::VersionDatabase::Instance();
    vdb.RegisterModule("lserver.exe", "10.0.26100.1", "Remote Desktop Licensing Service Engine");
    vdb.RegisterModule("licmgr.exe", "10.0.26100.1", "Remote Desktop Licensing Manager");
    vdb.RegisterModule("tlsapi.dll", "10.0.26100.1", "Terminal Server Licensing Client API");
    vdb.RegisterModule("tls236.dll", "10.0.26100.1", "Terminal Server Licensing Security Provider");
    vdb.RegisterModule("mstlsapi.dll", "10.0.26100.1", "Microsoft Terminal Server Licensing Protocol Library");
    vdb.RegisterModule("rdlic.exe", "10.0.26100.1", "Remote Desktop Licensing Diagnostic & Management CLI");

    // 3. Subsystem Singleton Default State
    RdLicensingManager::get().resetToDefault();
}

} // namespace micant::rdlic
