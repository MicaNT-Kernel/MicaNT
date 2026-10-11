#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <unordered_map>
#include <optional>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <span>
#include "ntdef.hpp"
#include "ntstatus.hpp"
#include "ob.hpp"

namespace micant::se {

// ============================================================================
// Standard Access Rights & Masks (aligned with Win32/NT security model)
// ============================================================================
inline constexpr uint32_t DELETE                    = 0x00010000;
inline constexpr uint32_t READ_CONTROL              = 0x00020000;
inline constexpr uint32_t WRITE_DAC                 = 0x00040000;
inline constexpr uint32_t WRITE_OWNER               = 0x00080000;
inline constexpr uint32_t SYNCHRONIZE               = 0x00100000;

inline constexpr uint32_t STANDARD_RIGHTS_REQUIRED  = 0x000F0000;
inline constexpr uint32_t STANDARD_RIGHTS_READ      = READ_CONTROL;
inline constexpr uint32_t STANDARD_RIGHTS_WRITE     = READ_CONTROL;
inline constexpr uint32_t STANDARD_RIGHTS_EXECUTE   = READ_CONTROL;
inline constexpr uint32_t STANDARD_RIGHTS_ALL       = 0x001F0000;
inline constexpr uint32_t SPECIFIC_RIGHTS_ALL       = 0x0000FFFF;

inline constexpr uint32_t FILE_READ_DATA            = 0x00000001;
inline constexpr uint32_t FILE_WRITE_DATA           = 0x00000002;
inline constexpr uint32_t FILE_APPEND_DATA          = 0x00000004;
inline constexpr uint32_t FILE_EXECUTE              = 0x00000020;

inline constexpr uint32_t ACCESS_SYSTEM_SECURITY    = 0x01000000;
inline constexpr uint32_t MAXIMUM_ALLOWED           = 0x02000000;

inline constexpr uint32_t GENERIC_READ              = 0x80000000;
inline constexpr uint32_t GENERIC_WRITE             = 0x40000000;
inline constexpr uint32_t GENERIC_EXECUTE           = 0x20000000;
inline constexpr uint32_t GENERIC_ALL               = 0x10000000;

// Security Descriptor Control Flags
inline constexpr uint16_t SE_OWNER_DEFAULTED        = 0x0001;
inline constexpr uint16_t SE_GROUP_DEFAULTED        = 0x0002;
inline constexpr uint16_t SE_DACL_PRESENT           = 0x0004;
inline constexpr uint16_t SE_DACL_DEFAULTED         = 0x0008;
inline constexpr uint16_t SE_SACL_PRESENT           = 0x0010;
inline constexpr uint16_t SE_SACL_DEFAULTED         = 0x0020;
inline constexpr uint16_t SE_DACL_AUTO_INHERIT_REQ  = 0x0100;
inline constexpr uint16_t SE_SACL_AUTO_INHERIT_REQ  = 0x0200;
inline constexpr uint16_t SE_DACL_AUTO_INHERITED    = 0x0400;
inline constexpr uint16_t SE_SACL_AUTO_INHERITED    = 0x0800;
inline constexpr uint16_t SE_DACL_PROTECTED         = 0x1000;
inline constexpr uint16_t SE_SACL_PROTECTED         = 0x2000;
inline constexpr uint16_t SE_RM_CONTROL_VALID       = 0x4000;
inline constexpr uint16_t SE_SELF_RELATIVE          = 0x8000;

// Token Privilege Attribute Flags
inline constexpr uint32_t SE_PRIVILEGE_ENABLED_BY_DEFAULT = 0x00000001;
inline constexpr uint32_t SE_PRIVILEGE_ENABLED            = 0x00000002;
inline constexpr uint32_t SE_PRIVILEGE_REMOVED            = 0x00000004;
inline constexpr uint32_t SE_PRIVILEGE_USED_FOR_ACCESS    = 0x80000000;

// Token Group Attribute Flags
inline constexpr uint32_t SE_GROUP_MANDATORY              = 0x00000001;
inline constexpr uint32_t SE_GROUP_ENABLED_BY_DEFAULT     = 0x00000002;
inline constexpr uint32_t SE_GROUP_ENABLED                = 0x00000004;
inline constexpr uint32_t SE_GROUP_OWNER                  = 0x00000008;
inline constexpr uint32_t SE_GROUP_USE_FOR_DENY_ONLY      = 0x00000010;
inline constexpr uint32_t SE_GROUP_INTEGRITY              = 0x00000020;
inline constexpr uint32_t SE_GROUP_RESOURCE               = 0x20000000;
inline constexpr uint32_t SE_GROUP_LOGON_ID               = 0xC0000000;

// ACE Inheritance & Audit Flags
inline constexpr uint8_t OBJECT_INHERIT_ACE               = 0x01;
inline constexpr uint8_t CONTAINER_INHERIT_ACE            = 0x02;
inline constexpr uint8_t NO_PROPAGATE_INHERIT_ACE         = 0x04;
inline constexpr uint8_t INHERIT_ONLY_ACE                 = 0x08;
inline constexpr uint8_t INHERITED_ACE                    = 0x10;
inline constexpr uint8_t SUCCESSFUL_ACCESS_ACE_FLAG       = 0x40;
inline constexpr uint8_t FAILED_ACCESS_ACE_FLAG           = 0x80;

// Mandatory Integrity Control (MIC) Label Policy Flags
inline constexpr uint32_t SYSTEM_MANDATORY_LABEL_NO_WRITE_UP   = 0x00000001;
inline constexpr uint32_t SYSTEM_MANDATORY_LABEL_NO_READ_UP    = 0x00000002;
inline constexpr uint32_t SYSTEM_MANDATORY_LABEL_NO_EXECUTE_UP = 0x00000004;

// Mandatory Integrity RIDs (Authority 16: SECURITY_MANDATORY_LABEL_AUTHORITY)
inline constexpr uint32_t SECURITY_MANDATORY_UNTRUSTED_RID         = 0x00000000; // S-1-16-0
inline constexpr uint32_t SECURITY_MANDATORY_LOW_RID               = 0x00001000; // S-1-16-4096
inline constexpr uint32_t SECURITY_MANDATORY_MEDIUM_RID            = 0x00002000; // S-1-16-8192
inline constexpr uint32_t SECURITY_MANDATORY_MEDIUM_PLUS_RID       = 0x00002100; // S-1-16-8448
inline constexpr uint32_t SECURITY_MANDATORY_HIGH_RID              = 0x00003000; // S-1-16-12288
inline constexpr uint32_t SECURITY_MANDATORY_SYSTEM_RID            = 0x00004000; // S-1-16-16384
inline constexpr uint32_t SECURITY_MANDATORY_PROTECTED_PROCESS_RID = 0x00005000; // S-1-16-20480

// Standard NT Privilege Strings
inline constexpr std::wstring_view SE_DEBUG_NAME             = L"SeDebugPrivilege";
inline constexpr std::wstring_view SE_SHUTDOWN_NAME          = L"SeShutdownPrivilege";
inline constexpr std::wstring_view SE_BACKUP_NAME            = L"SeBackupPrivilege";
inline constexpr std::wstring_view SE_RESTORE_NAME           = L"SeRestorePrivilege";
inline constexpr std::wstring_view SE_SECURITY_NAME          = L"SeSecurityPrivilege";
inline constexpr std::wstring_view SE_TAKE_OWNERSHIP_NAME    = L"SeTakeOwnershipPrivilege";
inline constexpr std::wstring_view SE_TCB_NAME               = L"SeTcbPrivilege";
inline constexpr std::wstring_view SE_IMPERSONATE_NAME       = L"SeImpersonatePrivilege";
inline constexpr std::wstring_view SE_CHANGE_NOTIFY_NAME     = L"SeChangeNotifyPrivilege";
inline constexpr std::wstring_view SE_SYSTEMTIME_NAME        = L"SeSystemtimePrivilege";
inline constexpr std::wstring_view SE_SYSTEM_ENVIRONMENT_NAME= L"SeSystemEnvironmentPrivilege";
inline constexpr std::wstring_view SE_ASSIGNPRIMARYTOKEN_NAME= L"SeAssignPrimaryTokenPrivilege";
inline constexpr std::wstring_view SE_INCREASE_QUOTA_NAME    = L"SeIncreaseQuotaPrivilege";
inline constexpr std::wstring_view SE_LOAD_DRIVER_NAME       = L"SeLoadDriverPrivilege";
inline constexpr std::wstring_view SE_CREATE_TOKEN_NAME      = L"SeCreateTokenPrivilege";
inline constexpr std::wstring_view SE_LOCK_MEMORY_NAME       = L"SeLockMemoryPrivilege";

enum class ProcessorMode : uint8_t {
    KernelMode = 0,
    UserMode   = 1
};

/**
 * @brief Security Identifier (SID).
 * Variable-length numerical value used to uniquely identify security principals.
 * Canonical string format: S-1-<IdentifierAuthority>-<SubAuth1>-<SubAuth2>...
 */
class Sid {
public:
    Sid() : revision_(1), identifierAuthority_{0, 0, 0, 0, 0, 0} {}

    Sid(uint64_t authority, std::initializer_list<uint32_t> subAuthorities)
        : revision_(1) {
        for (int i = 5; i >= 0; --i) {
            identifierAuthority_[i] = static_cast<uint8_t>(authority & 0xFF);
            authority >>= 8;
        }
        subAuthorities_.assign(subAuthorities.begin(), subAuthorities.end());
    }

    [[nodiscard]] uint8_t getRevision() const noexcept { return revision_; }
    [[nodiscard]] uint64_t getIdentifierAuthority() const noexcept {
        uint64_t auth = 0;
        for (uint8_t b : identifierAuthority_) {
            auth = (auth << 8) | b;
        }
        return auth;
    }
    [[nodiscard]] const std::vector<uint32_t>& getSubAuthorities() const noexcept {
        return subAuthorities_;
    }

    [[nodiscard]] std::wstring toString() const {
        std::wostringstream wos;
        wos << L"S-" << static_cast<uint32_t>(revision_) << L"-" << getIdentifierAuthority();
        for (uint32_t sa : subAuthorities_) {
            wos << L"-" << sa;
        }
        return wos.str();
    }

    bool operator==(const Sid& other) const noexcept {
        return revision_ == other.revision_ &&
               std::equal(identifierAuthority_, identifierAuthority_ + 6, other.identifierAuthority_) &&
               subAuthorities_ == other.subAuthorities_;
    }

    // Well-Known SIDs
    [[nodiscard]] static Sid null() { return Sid(0, {0}); }                     // S-1-0-0
    [[nodiscard]] static Sid everyone() { return Sid(1, {0}); }                 // S-1-1-0 (World)
    [[nodiscard]] static Sid local() { return Sid(2, {0}); }                    // S-1-2-0
    [[nodiscard]] static Sid creatorOwner() { return Sid(3, {0}); }             // S-1-3-0
    [[nodiscard]] static Sid authenticatedUsers() { return Sid(5, {11}); }      // S-1-5-11
    [[nodiscard]] static Sid localSystem() { return Sid(5, {18}); }             // S-1-5-18
    [[nodiscard]] static Sid localService() { return Sid(5, {19}); }            // S-1-5-19
    [[nodiscard]] static Sid networkService() { return Sid(5, {20}); }          // S-1-5-20
    [[nodiscard]] static Sid administrators() { return Sid(5, {32, 544}); }     // S-1-5-32-544 (Builtin Administrators)
    [[nodiscard]] static Sid users() { return Sid(5, {32, 545}); }              // S-1-5-32-545 (Builtin Users)

    // Integrity Level SIDs (Authority 16 = SECURITY_MANDATORY_LABEL_AUTHORITY)
    [[nodiscard]] static Sid fromIntegrityRid(uint32_t rid) { return Sid(16, {rid}); }
    [[nodiscard]] static Sid integrityUntrusted() { return fromIntegrityRid(SECURITY_MANDATORY_UNTRUSTED_RID); }
    [[nodiscard]] static Sid integrityLow() { return fromIntegrityRid(SECURITY_MANDATORY_LOW_RID); }
    [[nodiscard]] static Sid integrityMedium() { return fromIntegrityRid(SECURITY_MANDATORY_MEDIUM_RID); }
    [[nodiscard]] static Sid integrityMediumPlus() { return fromIntegrityRid(SECURITY_MANDATORY_MEDIUM_PLUS_RID); }
    [[nodiscard]] static Sid integrityHigh() { return fromIntegrityRid(SECURITY_MANDATORY_HIGH_RID); }
    [[nodiscard]] static Sid integritySystem() { return fromIntegrityRid(SECURITY_MANDATORY_SYSTEM_RID); }
    [[nodiscard]] static Sid integrityProtectedProcess() { return fromIntegrityRid(SECURITY_MANDATORY_PROTECTED_PROCESS_RID); }

private:
    uint8_t revision_{1};
    uint8_t identifierAuthority_[6]{0};
    std::vector<uint32_t> subAuthorities_;
};

// ACE Types
enum class AceType : uint8_t {
    AccessAllowed        = 0x00,
    AccessDenied         = 0x01,
    SystemAudit          = 0x02,
    SystemAlarm          = 0x03,
    SystemMandatoryLabel = 0x11
};

/**
 * @brief Access Control Entry (ACE).
 */
struct Ace {
    AceType type{AceType::AccessAllowed};
    uint8_t flags{0};
    uint32_t mask{0};
    Sid sid;
};

/**
 * @brief Access Control List (ACL).
 * Manages an ordered collection of ACEs.
 */
class Acl {
public:
    Acl() = default;

    void addAllowedAce(const Sid& sid, uint32_t mask, uint8_t flags = 0) {
        aces_.push_back(Ace{ .type = AceType::AccessAllowed, .flags = flags, .mask = mask, .sid = sid });
    }

    void addDeniedAce(const Sid& sid, uint32_t mask, uint8_t flags = 0) {
        aces_.push_back(Ace{ .type = AceType::AccessDenied, .flags = flags, .mask = mask, .sid = sid });
    }

    void addAuditAce(const Sid& sid, uint32_t mask, uint8_t flags = SUCCESSFUL_ACCESS_ACE_FLAG | FAILED_ACCESS_ACE_FLAG) {
        aces_.push_back(Ace{ .type = AceType::SystemAudit, .flags = flags, .mask = mask, .sid = sid });
    }

    void addMandatoryLabelAce(const Sid& integritySid, uint32_t policyMask, uint8_t flags = 0) {
        aces_.push_back(Ace{ .type = AceType::SystemMandatoryLabel, .flags = flags, .mask = policyMask, .sid = integritySid });
    }

    void addAce(Ace ace) {
        aces_.push_back(std::move(ace));
    }

    [[nodiscard]] size_t getAceCount() const noexcept { return aces_.size(); }
    [[nodiscard]] const std::vector<Ace>& getAces() const noexcept { return aces_; }
    [[nodiscard]] std::vector<Ace>& getAces() noexcept { return aces_; }
    void clear() noexcept { aces_.clear(); }

private:
    std::vector<Ace> aces_;
};

/**
 * @brief Security Descriptor.
 * Contains security information associated with an object: Owner, Group, DACL, SACL, and Control Flags.
 */
class SecurityDescriptor {
public:
    SecurityDescriptor() = default;

    void setOwner(Sid owner) { 
        owner_ = std::move(owner); 
        control_ &= ~SE_OWNER_DEFAULTED;
    }
    [[nodiscard]] const std::optional<Sid>& getOwner() const noexcept { return owner_; }

    void setGroup(Sid group) { 
        group_ = std::move(group); 
        control_ &= ~SE_GROUP_DEFAULTED;
    }
    [[nodiscard]] const std::optional<Sid>& getGroup() const noexcept { return group_; }

    void setDacl(Acl dacl) { 
        dacl_ = std::move(dacl); 
        daclPresent_ = true; 
        control_ |= SE_DACL_PRESENT;
    }
    void removeDacl() noexcept {
        dacl_.reset();
        daclPresent_ = false;
        control_ &= ~SE_DACL_PRESENT;
    }
    [[nodiscard]] const std::optional<Acl>& getDacl() const noexcept { return dacl_; }
    [[nodiscard]] bool hasDacl() const noexcept { return daclPresent_; }

    void setSacl(Acl sacl) {
        sacl_ = std::move(sacl);
        saclPresent_ = true;
        control_ |= SE_SACL_PRESENT;
    }
    void removeSacl() noexcept {
        sacl_.reset();
        saclPresent_ = false;
        control_ &= ~SE_SACL_PRESENT;
    }
    [[nodiscard]] const std::optional<Acl>& getSacl() const noexcept { return sacl_; }
    [[nodiscard]] bool hasSacl() const noexcept { return saclPresent_; }

    [[nodiscard]] uint16_t getControl() const noexcept { return control_; }
    void setControl(uint16_t control) noexcept { control_ = control; }

    // Mandatory Integrity Control Helpers (encoded in SACL as SystemMandatoryLabel ACE)
    void setIntegrityLevel(uint32_t integrityRid, uint32_t policy = SYSTEM_MANDATORY_LABEL_NO_WRITE_UP) {
        if (!sacl_) {
            sacl_ = Acl();
            saclPresent_ = true;
            control_ |= SE_SACL_PRESENT;
        }
        for (auto& ace : sacl_->getAces()) {
            if (ace.type == AceType::SystemMandatoryLabel) {
                ace.sid = Sid::fromIntegrityRid(integrityRid);
                ace.mask = policy;
                return;
            }
        }
        sacl_->addMandatoryLabelAce(Sid::fromIntegrityRid(integrityRid), policy);
    }

    [[nodiscard]] uint32_t getIntegrityLevel() const noexcept {
        if (sacl_) {
            for (const auto& ace : sacl_->getAces()) {
                if (ace.type == AceType::SystemMandatoryLabel) {
                    const auto& sub = ace.sid.getSubAuthorities();
                    if (!sub.empty()) {
                        return sub[0];
                    }
                }
            }
        }
        return SECURITY_MANDATORY_MEDIUM_RID; // Standard NT object default
    }

    [[nodiscard]] uint32_t getIntegrityPolicy() const noexcept {
        if (sacl_) {
            for (const auto& ace : sacl_->getAces()) {
                if (ace.type == AceType::SystemMandatoryLabel) {
                    return ace.mask;
                }
            }
        }
        return SYSTEM_MANDATORY_LABEL_NO_WRITE_UP;
    }

    [[nodiscard]] bool hasMandatoryLabel() const noexcept {
        if (sacl_) {
            for (const auto& ace : sacl_->getAces()) {
                if (ace.type == AceType::SystemMandatoryLabel) {
                    return true;
                }
            }
        }
        return false;
    }

private:
    std::optional<Sid> owner_;
    std::optional<Sid> group_;
    std::optional<Acl> dacl_;
    std::optional<Acl> sacl_;
    bool daclPresent_{false};
    bool saclPresent_{false};
    uint16_t control_{0};
};

/**
 * @brief Token Type (Primary or Impersonation).
 */
enum class TokenType : uint32_t {
    Primary = 1,
    Impersonation = 2
};

/**
 * @brief Security Impersonation Level.
 */
enum class SecurityImpersonationLevel : uint32_t {
    Anonymous = 0,
    Identification = 1,
    Impersonation = 2,
    Delegation = 3
};

/**
 * @brief Token Group Entry with NT Attribute Flags.
 */
struct TokenGroup {
    Sid sid;
    uint32_t attributes{SE_GROUP_ENABLED | SE_GROUP_ENABLED_BY_DEFAULT};
};

/**
 * @brief Process / Thread Access Token.
 * Represents user identity, group memberships, assigned administrative privileges, and integrity level.
 */
class TokenObject {
public:
    explicit TokenObject(Sid userSid, TokenType type = TokenType::Primary)
        : userSid_(std::move(userSid)), tokenType_(type) {}

    [[nodiscard]] const Sid& getUserSid() const noexcept { return userSid_; }
    [[nodiscard]] TokenType getTokenType() const noexcept { return tokenType_; }
    void setTokenType(TokenType t) noexcept { tokenType_ = t; }

    [[nodiscard]] const std::vector<Sid>& getGroupSids() const noexcept { 
        return cachedGroupSids_; 
    }
    [[nodiscard]] const std::vector<TokenGroup>& getGroups() const noexcept {
        return groups_;
    }

    void addGroup(Sid group, uint32_t attributes = SE_GROUP_ENABLED | SE_GROUP_ENABLED_BY_DEFAULT) {
        cachedGroupSids_.push_back(group);
        groups_.push_back(TokenGroup{ .sid = std::move(group), .attributes = attributes });
    }

    [[nodiscard]] bool hasSid(const Sid& sid, bool allowDenyOnly = true) const {
        if (userSid_ == sid) return true;
        for (const auto& g : groups_) {
            if (g.sid == sid) {
                if ((g.attributes & SE_GROUP_USE_FOR_DENY_ONLY) != 0) {
                    if (allowDenyOnly) return true;
                    continue; // Cannot be used for access allow
                }
                if ((g.attributes & SE_GROUP_ENABLED) != 0) {
                    return true;
                }
            }
        }
        return false;
    }

    [[nodiscard]] uint32_t getIntegrityLevel() const noexcept { return integrityLevel_; }
    void setIntegrityLevel(uint32_t level) noexcept { integrityLevel_ = level; }
    [[nodiscard]] Sid getIntegritySid() const noexcept { return Sid::fromIntegrityRid(integrityLevel_); }

    void addPrivilege(std::wstring_view name, uint32_t attributes = SE_PRIVILEGE_ENABLED) {
        privileges_[std::wstring(name)] = attributes;
    }

    void removePrivilege(std::wstring_view name) {
        privileges_.erase(std::wstring(name));
    }

    [[nodiscard]] bool hasPrivilege(std::wstring_view name, bool mustBeEnabled = true) const {
        auto it = privileges_.find(std::wstring(name));
        if (it == privileges_.end()) return false;
        if (mustBeEnabled) {
            return (it->second & SE_PRIVILEGE_ENABLED) != 0;
        }
        return true;
    }

    bool enablePrivilege(std::wstring_view name, bool enable = true) {
        auto it = privileges_.find(std::wstring(name));
        if (it == privileges_.end()) return false;
        if (enable) {
            it->second |= SE_PRIVILEGE_ENABLED;
        } else {
            it->second &= ~SE_PRIVILEGE_ENABLED;
        }
        return true;
    }

    [[nodiscard]] const std::unordered_map<std::wstring, uint32_t>& getPrivileges() const noexcept {
        return privileges_;
    }

    [[nodiscard]] Luid getAuthenticationId() const noexcept { return authId_; }
    void setAuthenticationId(Luid id) noexcept { authId_ = id; }

    [[nodiscard]] uint32_t getSessionId() const noexcept { return sessionId_; }
    void setSessionId(uint32_t id) noexcept { sessionId_ = id; }

    [[nodiscard]] SecurityImpersonationLevel getImpersonationLevel() const noexcept { return impersonationLevel_; }
    void setImpersonationLevel(SecurityImpersonationLevel level) noexcept { impersonationLevel_ = level; }

    [[nodiscard]] const std::vector<Sid>& getRestrictedSids() const noexcept { return restrictedSids_; }
    [[nodiscard]] bool isRestricted() const noexcept { return isRestricted_; }

    /**
     * @brief Creates a restricted token (NtFilterToken).
     */
    [[nodiscard]] std::shared_ptr<TokenObject> createRestrictedToken(
        const std::vector<Sid>& sidsToDisable,
        const std::vector<std::wstring>& privilegesToRemove,
        const std::vector<Sid>& restrictingSids = {}
    ) const {
        auto clone = std::make_shared<TokenObject>(userSid_, tokenType_);
        clone->impersonationLevel_ = impersonationLevel_;
        clone->authId_ = authId_;
        clone->sessionId_ = sessionId_;
        clone->integrityLevel_ = integrityLevel_;

        // Groups: mark disabled SIDs as deny-only
        for (const auto& g : groups_) {
            uint32_t attr = g.attributes;
            bool shouldDisable = std::any_of(sidsToDisable.begin(), sidsToDisable.end(),
                [&g](const Sid& s) { return s == g.sid; });
            if (shouldDisable) {
                attr &= ~SE_GROUP_ENABLED;
                attr |= SE_GROUP_USE_FOR_DENY_ONLY;
            }
            clone->addGroup(g.sid, attr);
        }

        // Privileges: copy all except removed
        for (const auto& [name, attr] : privileges_) {
            bool shouldRemove = std::any_of(privilegesToRemove.begin(), privilegesToRemove.end(),
                [&name](const std::wstring& p) { return p == name; });
            if (!shouldRemove) {
                clone->addPrivilege(name, attr);
            }
        }

        if (!restrictingSids.empty()) {
            clone->isRestricted_ = true;
            clone->restrictedSids_ = restrictingSids;
        }

        return clone;
    }

    /**
     * @brief Simulates standard NT UAC token filtering: drops admin privileges,
     * marks administrators group as SE_GROUP_USE_FOR_DENY_ONLY, and sets Medium integrity.
     */
    [[nodiscard]] std::shared_ptr<TokenObject> filterTokenForUac() const {
        std::vector<Sid> sidsToDisable = { Sid::administrators() };
        std::vector<std::wstring> privsToRemove = {
            std::wstring(SE_DEBUG_NAME),
            std::wstring(SE_SHUTDOWN_NAME),
            std::wstring(SE_BACKUP_NAME),
            std::wstring(SE_RESTORE_NAME),
            std::wstring(SE_SECURITY_NAME),
            std::wstring(SE_TAKE_OWNERSHIP_NAME),
            std::wstring(SE_TCB_NAME),
            std::wstring(SE_SYSTEM_ENVIRONMENT_NAME),
            std::wstring(SE_ASSIGNPRIMARYTOKEN_NAME),
            std::wstring(SE_INCREASE_QUOTA_NAME),
            std::wstring(SE_LOAD_DRIVER_NAME),
            std::wstring(SE_CREATE_TOKEN_NAME),
            std::wstring(SE_LOCK_MEMORY_NAME)
        };

        auto filtered = createRestrictedToken(sidsToDisable, privsToRemove);
        filtered->setIntegrityLevel(SECURITY_MANDATORY_MEDIUM_RID);
        return filtered;
    }

    // Factory Helpers
    [[nodiscard]] static std::shared_ptr<TokenObject> createSystemToken() {
        auto token = std::make_shared<TokenObject>(Sid::localSystem());
        token->setIntegrityLevel(SECURITY_MANDATORY_SYSTEM_RID);
        token->addGroup(Sid::administrators());
        token->addGroup(Sid::authenticatedUsers());
        token->addGroup(Sid::everyone());
        token->addPrivilege(SE_DEBUG_NAME, SE_PRIVILEGE_ENABLED | SE_PRIVILEGE_ENABLED_BY_DEFAULT);
        token->addPrivilege(SE_SHUTDOWN_NAME, SE_PRIVILEGE_ENABLED | SE_PRIVILEGE_ENABLED_BY_DEFAULT);
        token->addPrivilege(SE_TCB_NAME, SE_PRIVILEGE_ENABLED | SE_PRIVILEGE_ENABLED_BY_DEFAULT);
        token->addPrivilege(SE_IMPERSONATE_NAME, SE_PRIVILEGE_ENABLED | SE_PRIVILEGE_ENABLED_BY_DEFAULT);
        token->addPrivilege(SE_CHANGE_NOTIFY_NAME, SE_PRIVILEGE_ENABLED | SE_PRIVILEGE_ENABLED_BY_DEFAULT);
        return token;
    }

    [[nodiscard]] static std::shared_ptr<TokenObject> createAdminToken(Sid userSid) {
        auto token = std::make_shared<TokenObject>(std::move(userSid));
        token->setIntegrityLevel(SECURITY_MANDATORY_HIGH_RID);
        token->addGroup(Sid::administrators());
        token->addGroup(Sid::authenticatedUsers());
        token->addGroup(Sid::everyone());
        token->addPrivilege(SE_DEBUG_NAME, SE_PRIVILEGE_ENABLED_BY_DEFAULT);
        token->addPrivilege(SE_SHUTDOWN_NAME, SE_PRIVILEGE_ENABLED_BY_DEFAULT);
        token->addPrivilege(SE_CHANGE_NOTIFY_NAME, SE_PRIVILEGE_ENABLED | SE_PRIVILEGE_ENABLED_BY_DEFAULT);
        return token;
    }

    [[nodiscard]] static std::shared_ptr<TokenObject> createUserToken(Sid userSid) {
        auto token = std::make_shared<TokenObject>(std::move(userSid));
        token->setIntegrityLevel(SECURITY_MANDATORY_MEDIUM_RID);
        token->addGroup(Sid::users());
        token->addGroup(Sid::authenticatedUsers());
        token->addGroup(Sid::everyone());
        token->addPrivilege(SE_CHANGE_NOTIFY_NAME, SE_PRIVILEGE_ENABLED | SE_PRIVILEGE_ENABLED_BY_DEFAULT);
        return token;
    }

private:
    Sid userSid_;
    std::vector<TokenGroup> groups_;
    std::vector<Sid> cachedGroupSids_;
    std::unordered_map<std::wstring, uint32_t> privileges_;
    TokenType tokenType_{TokenType::Primary};
    SecurityImpersonationLevel impersonationLevel_{SecurityImpersonationLevel::Impersonation};
    Luid authId_{0, 0};
    uint32_t sessionId_{0};
    uint32_t integrityLevel_{SECURITY_MANDATORY_MEDIUM_RID};
    bool isRestricted_{false};
    std::vector<Sid> restrictedSids_;
};

/**
 * @brief Security Audit Record.
 */
struct SecurityAuditRecord {
    Sid userSid;
    std::wstring objectName;
    uint32_t desiredAccess{0};
    uint32_t grantedAccess{0};
    bool accessGranted{false};
};

/**
 * @brief Security Reference Monitor (SRM).
 * Performs Dave Cutler's NT access validation algorithm against Security Descriptors,
 * Mandatory Integrity Control (MIC), and SACL Auditing.
 */
class SecurityReferenceMonitor {
public:
    inline static std::vector<SecurityAuditRecord> s_AuditLog;

    static const std::vector<SecurityAuditRecord>& getAuditLog() noexcept { return s_AuditLog; }
    static void clearAuditLog() noexcept { s_AuditLog.clear(); }

    [[nodiscard]] static bool SeSinglePrivilegeCheck(
        std::wstring_view privName,
        const TokenObject& token,
        ProcessorMode mode = ProcessorMode::UserMode
    ) noexcept {
        if (mode == ProcessorMode::KernelMode) {
            return true;
        }
        return token.hasPrivilege(privName, true);
    }

    [[nodiscard]] static bool SePrivilegeCheck(
        const std::vector<std::wstring>& privNames,
        const TokenObject& token,
        bool allRequired = true,
        ProcessorMode mode = ProcessorMode::UserMode
    ) noexcept {
        if (mode == ProcessorMode::KernelMode) {
            return true;
        }
        if (privNames.empty()) {
            return true;
        }
        if (allRequired) {
            for (const auto& priv : privNames) {
                if (!token.hasPrivilege(priv, true)) {
                    return false;
                }
            }
            return true;
        } else {
            for (const auto& priv : privNames) {
                if (token.hasPrivilege(priv, true)) {
                    return true;
                }
            }
            return false;
        }
    }

    /**
     * @brief Performs access validation according to standard NT security semantics.
     *
     * 1. If desiredAccess is 0, access is granted.
     * 2. Mandatory Integrity Control (MIC):
     *    - If object has mandatory label and subject integrity < object integrity:
     *      - SYSTEM_MANDATORY_LABEL_NO_WRITE_UP blocks any write rights.
     *      - SYSTEM_MANDATORY_LABEL_NO_READ_UP blocks any read rights.
     *      - SYSTEM_MANDATORY_LABEL_NO_EXECUTE_UP blocks execute rights.
     * 3. ACCESS_SYSTEM_SECURITY privilege check (requires SeSecurityPrivilege).
     * 4. WRITE_OWNER privilege check (SeTakeOwnershipPrivilege bypass).
     * 5. Owner rights: Owner is always granted READ_CONTROL and WRITE_DAC if requested.
     * 6. If no DACL is present (daclPresent == false), full access is granted to all callers.
     * 7. If DACL is present but empty (aceCount == 0), all access is denied.
     * 8. ACEs are traversed in order:
     *    - If matching SID has an AccessDenied ACE covering requested rights -> ACCESS_DENIED.
     *      (Deny-only SIDs still match AccessDenied ACEs).
     *    - If matching SID has an AccessAllowed ACE -> accumulate granted rights.
     *      (Deny-only SIDs CANNOT match AccessAllowed ACEs).
     *    - If all desired rights are granted -> STATUS_SUCCESS.
     * 9. If loop finishes and ungranted rights remain -> ACCESS_DENIED.
     * 10. SACL SystemAudit ACE evaluation and logging.
     */
    [[nodiscard]] static NtStatus accessCheck(
        const TokenObject& token,
        const SecurityDescriptor& secDesc,
        uint32_t desiredAccess,
        uint32_t& grantedAccess,
        std::wstring_view objectName = L""
    ) noexcept {
        grantedAccess = 0;
        if (desiredAccess == 0) {
            return NtStatus::Success;
        }

        // Map generic rights if present
        uint32_t remainingDesired = desiredAccess;
        if (remainingDesired & GENERIC_ALL) {
            remainingDesired = STANDARD_RIGHTS_ALL | SPECIFIC_RIGHTS_ALL;
        }
        if (remainingDesired & GENERIC_READ) {
            remainingDesired |= (STANDARD_RIGHTS_READ | FILE_READ_DATA);
            remainingDesired &= ~GENERIC_READ;
        }
        if (remainingDesired & GENERIC_WRITE) {
            remainingDesired |= (STANDARD_RIGHTS_WRITE | FILE_WRITE_DATA | FILE_APPEND_DATA);
            remainingDesired &= ~GENERIC_WRITE;
        }
        if (remainingDesired & GENERIC_EXECUTE) {
            remainingDesired |= (STANDARD_RIGHTS_EXECUTE | FILE_EXECUTE);
            remainingDesired &= ~GENERIC_EXECUTE;
        }

        // 1. Mandatory Integrity Control (MIC) Check
        if (secDesc.hasMandatoryLabel()) {
            uint32_t objIntegrity = secDesc.getIntegrityLevel();
            uint32_t subIntegrity = token.getIntegrityLevel();
            uint32_t policy = secDesc.getIntegrityPolicy();

            if (subIntegrity < objIntegrity) {
                // Write access check: generic write, write DAC, write owner, delete, or specific write masks
                constexpr uint32_t WRITE_RIGHTS = GENERIC_WRITE | WRITE_DAC | WRITE_OWNER | DELETE | FILE_WRITE_DATA | FILE_APPEND_DATA;
                if ((policy & SYSTEM_MANDATORY_LABEL_NO_WRITE_UP) &&
                    ((desiredAccess & (GENERIC_WRITE | GENERIC_ALL)) != 0 || (remainingDesired & WRITE_RIGHTS) != 0)) {
                    recordAudit(token.getUserSid(), objectName, desiredAccess, 0, false, secDesc);
                    return NtStatus::AccessDenied;
                }
                // Read access check
                constexpr uint32_t READ_RIGHTS = GENERIC_READ | FILE_READ_DATA;
                if ((policy & SYSTEM_MANDATORY_LABEL_NO_READ_UP) &&
                    ((desiredAccess & (GENERIC_READ | GENERIC_ALL)) != 0 || (remainingDesired & READ_RIGHTS) != 0)) {
                    recordAudit(token.getUserSid(), objectName, desiredAccess, 0, false, secDesc);
                    return NtStatus::AccessDenied;
                }
                // Execute access check
                constexpr uint32_t EXEC_RIGHTS = GENERIC_EXECUTE | FILE_EXECUTE;
                if ((policy & SYSTEM_MANDATORY_LABEL_NO_EXECUTE_UP) &&
                    ((desiredAccess & (GENERIC_EXECUTE | GENERIC_ALL)) != 0 || (remainingDesired & EXEC_RIGHTS) != 0)) {
                    recordAudit(token.getUserSid(), objectName, desiredAccess, 0, false, secDesc);
                    return NtStatus::AccessDenied;
                }
            }
        }

        // 2. ACCESS_SYSTEM_SECURITY privilege check
        if (remainingDesired & ACCESS_SYSTEM_SECURITY) {
            if (!token.hasPrivilege(SE_SECURITY_NAME, true)) {
                recordAudit(token.getUserSid(), objectName, desiredAccess, 0, false, secDesc);
                return NtStatus::PrivilegeNotHeld;
            }
            grantedAccess |= ACCESS_SYSTEM_SECURITY;
            remainingDesired &= ~ACCESS_SYSTEM_SECURITY;
            if (remainingDesired == 0) {
                recordAudit(token.getUserSid(), objectName, desiredAccess, grantedAccess, true, secDesc);
                return NtStatus::Success;
            }
        }

        // 3. WRITE_OWNER with SeTakeOwnershipPrivilege
        if ((remainingDesired & WRITE_OWNER) && token.hasPrivilege(SE_TAKE_OWNERSHIP_NAME, true)) {
            grantedAccess |= WRITE_OWNER;
            remainingDesired &= ~WRITE_OWNER;
            if (remainingDesired == 0) {
                recordAudit(token.getUserSid(), objectName, desiredAccess, grantedAccess, true, secDesc);
                return NtStatus::Success;
            }
        }

        // 4. Owner privilege: Owner always has READ_CONTROL and WRITE_DAC
        if (secDesc.getOwner() && token.hasSid(*secDesc.getOwner(), false)) {
            uint32_t ownerGrant = remainingDesired & (READ_CONTROL | WRITE_DAC);
            grantedAccess |= ownerGrant;
            remainingDesired &= ~ownerGrant;
            if (remainingDesired == 0) {
                recordAudit(token.getUserSid(), objectName, desiredAccess, grantedAccess, true, secDesc);
                return NtStatus::Success;
            }
        }

        // 5. Check DACL presence
        if (!secDesc.hasDacl()) {
            // Null DACL = Full access granted to everyone
            grantedAccess = desiredAccess;
            recordAudit(token.getUserSid(), objectName, desiredAccess, grantedAccess, true, secDesc);
            return NtStatus::Success;
        }

        const auto& daclOpt = secDesc.getDacl();
        if (!daclOpt || daclOpt->getAceCount() == 0) {
            // Empty DACL = Access denied to everyone
            recordAudit(token.getUserSid(), objectName, desiredAccess, 0, false, secDesc);
            return NtStatus::AccessDenied;
        }

        // 6. Traverse DACL ACEs in order
        for (const auto& ace : daclOpt->getAces()) {
            uint32_t aceMask = ace.mask;
            if (aceMask & GENERIC_ALL) {
                aceMask |= (STANDARD_RIGHTS_ALL | SPECIFIC_RIGHTS_ALL);
            }
            if (aceMask & GENERIC_READ) {
                aceMask |= (STANDARD_RIGHTS_READ | FILE_READ_DATA);
            }
            if (aceMask & GENERIC_WRITE) {
                aceMask |= (STANDARD_RIGHTS_WRITE | FILE_WRITE_DATA | FILE_APPEND_DATA);
            }
            if (aceMask & GENERIC_EXECUTE) {
                aceMask |= (STANDARD_RIGHTS_EXECUTE | FILE_EXECUTE);
            }

            if (ace.type == AceType::AccessDenied) {
                // Deny ACE: matches if token has SID (even if group is deny-only!)
                if (token.hasSid(ace.sid, true)) {
                    if ((remainingDesired & aceMask) != 0) {
                        grantedAccess = 0;
                        recordAudit(token.getUserSid(), objectName, desiredAccess, 0, false, secDesc);
                        return NtStatus::AccessDenied;
                    }
                }
            } else if (ace.type == AceType::AccessAllowed) {
                // Allow ACE: matches ONLY IF token has SID AND group is NOT deny-only!
                if (token.hasSid(ace.sid, false)) {
                    uint32_t matched = (remainingDesired & aceMask);
                    grantedAccess |= matched;
                    remainingDesired &= ~matched;

                    if (remainingDesired == 0) {
                        recordAudit(token.getUserSid(), objectName, desiredAccess, grantedAccess, true, secDesc);
                        return NtStatus::Success;
                    }
                }
            }
        }

        bool success = (remainingDesired == 0);
        recordAudit(token.getUserSid(), objectName, desiredAccess, grantedAccess, success, secDesc);
        return success ? NtStatus::Success : NtStatus::AccessDenied;
    }

private:
    static void recordAudit(
        const Sid& sid,
        std::wstring_view objName,
        uint32_t desired,
        uint32_t granted,
        bool success,
        const SecurityDescriptor& sd
    ) noexcept {
        if (!sd.hasSacl()) return;
        const auto& saclOpt = sd.getSacl();
        if (!saclOpt) return;

        for (const auto& ace : saclOpt->getAces()) {
            if (ace.type == AceType::SystemAudit) {
                bool logSuccess = (ace.flags & SUCCESSFUL_ACCESS_ACE_FLAG) && success;
                bool logFailure = (ace.flags & FAILED_ACCESS_ACE_FLAG) && !success;
                if (logSuccess || logFailure) {
                    s_AuditLog.push_back(SecurityAuditRecord{
                        .userSid = sid,
                        .objectName = std::wstring(objName),
                        .desiredAccess = desired,
                        .grantedAccess = granted,
                        .accessGranted = success
                    });
                    break;
                }
            }
        }
    }
};

} // namespace micant::se
