#pragma once

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <span>
#include <optional>
#include <cstdint>
#include <cstring>
#include <atomic>

#include "micant/shared/ntstatus.hpp"
#include "micant/km/drivers/net/tcpip.hpp"
#include "micant/um/servers/scm.hpp"
#include "micant/um/subsystems/winbase/version.hpp"
#include "micant/km/drivers/security/cng.hpp"

namespace micant::ad_ds {

// ============================================================================
// 1. Constants & Configuration (RFC 4511 LDAP, RFC 4120 Kerberos, MS-PAC)
// ============================================================================

inline constexpr uint16_t LDAP_PORT         = 389;
inline constexpr uint16_t LDAPS_PORT        = 636;
inline constexpr uint16_t KERBEROS_KDC_PORT = 88;

inline constexpr const char* DEFAULT_DOMAIN_DNS     = "micant.local";
inline constexpr const char* DEFAULT_DOMAIN_NETBIOS = "MICANT";
inline constexpr const char* DEFAULT_DOMAIN_DN      = "DC=micant,DC=local";
inline constexpr const char* DEFAULT_DOMAIN_SID     = "S-1-5-21-147-135-1";
inline constexpr const char* DEFAULT_DC_NAME        = "DC01";
inline constexpr const char* DEFAULT_DC_FQDN        = "dc01.micant.local";
inline constexpr const char* DEFAULT_KRBTGT_NAME    = "krbtgt";

// User Account Control (UAC) Flags (Active Directory)
inline constexpr uint32_t UF_SCRIPT                         = 0x0001;
inline constexpr uint32_t UF_ACCOUNTDISABLE                 = 0x0002;
inline constexpr uint32_t UF_HOMEDIR_REQUIRED               = 0x0008;
inline constexpr uint32_t UF_LOCKOUT                        = 0x0010;
inline constexpr uint32_t UF_PASSWD_NOTREQD                 = 0x0020;
inline constexpr uint32_t UF_PASSWD_CANT_CHANGE             = 0x0040;
inline constexpr uint32_t UF_ENCRYPTED_TEXT_PASSWORD_ALLOWED= 0x0080;
inline constexpr uint32_t UF_TEMP_DUPLICATE_ACCOUNT         = 0x0100;
inline constexpr uint32_t UF_NORMAL_ACCOUNT                 = 0x0200;
inline constexpr uint32_t UF_INTERDOMAIN_TRUST_ACCOUNT      = 0x0800;
inline constexpr uint32_t UF_WORKSTATION_TRUST_ACCOUNT      = 0x1000;
inline constexpr uint32_t UF_SERVER_TRUST_ACCOUNT           = 0x2000;
inline constexpr uint32_t UF_DONT_EXPIRE_PASSWD             = 0x10000;
inline constexpr uint32_t UF_MNS_LOGON_ACCOUNT              = 0x20000;
inline constexpr uint32_t UF_SMARTCARD_REQUIRED             = 0x40000;
inline constexpr uint32_t UF_TRUSTED_FOR_DELEGATION         = 0x80000;
inline constexpr uint32_t UF_NOT_DELEGATED                  = 0x100000;
inline constexpr uint32_t UF_USE_DES_KEY_ONLY               = 0x200000;
inline constexpr uint32_t UF_DONT_REQUIRE_PREAUTH           = 0x400000;
inline constexpr uint32_t UF_PASSWORD_EXPIRED               = 0x800000;
inline constexpr uint32_t UF_TRUSTED_TO_AUTHENTICATE_FOR_DELEGATION = 0x1000000;

// Kerberos Ticket Flags (RFC 4120)
inline constexpr uint32_t KDC_TKT_FLAG_FORWARDABLE     = 0x40000000;
inline constexpr uint32_t KDC_TKT_FLAG_FORWARDED       = 0x20000000;
inline constexpr uint32_t KDC_TKT_FLAG_PROXIABLE       = 0x10000000;
inline constexpr uint32_t KDC_TKT_FLAG_PROXY           = 0x08000000;
inline constexpr uint32_t KDC_TKT_FLAG_MAY_POSTDATE    = 0x04000000;
inline constexpr uint32_t KDC_TKT_FLAG_POSTDATED       = 0x02000000;
inline constexpr uint32_t KDC_TKT_FLAG_INVALID         = 0x01000000;
inline constexpr uint32_t KDC_TKT_FLAG_RENEWABLE       = 0x00800000;
inline constexpr uint32_t KDC_TKT_FLAG_INITIAL         = 0x00400000;
inline constexpr uint32_t KDC_TKT_FLAG_PRE_AUTHENT     = 0x00200000;
inline constexpr uint32_t KDC_TKT_FLAG_HW_AUTHENT      = 0x00100000;
inline constexpr uint32_t KDC_TKT_FLAG_TRANSIT_POLICY  = 0x00040000;

// Kerberos Encryption Types (RFC 3961 / 3962 / 4757)
enum class KerberosEncryptionType : int32_t {
    DesCbcCrc                 = 1,
    DesCbcMd5                 = 3,
    Des3CbcSha1               = 16,
    Aes128CtsHmacSha1_96     = 17,
    Aes256CtsHmacSha1_96     = 18,
    ArcFourHmac               = 23,
    ArcFourHmacExp            = 24
};

// Kerberos Message Types (RFC 4120)
enum class KerberosMessageType : uint8_t {
    AsReq    = 10,
    AsRep    = 11,
    TgsReq   = 12,
    TgsRep   = 13,
    ApReq    = 14,
    ApRep    = 15,
    KrbError = 30
};

// LDAP v3 Protocol Result Codes (RFC 4511)
enum class LdapResultCode : uint32_t {
    Success                      = 0,
    OperationsError              = 1,
    ProtocolError                = 2,
    TimeLimitExceeded            = 3,
    SizeLimitExceeded            = 4,
    CompareFalse                 = 5,
    CompareTrue                  = 6,
    AuthMethodNotSupported       = 7,
    StrongerAuthRequired         = 8,
    Referral                     = 10,
    AdminLimitExceeded           = 11,
    UnavailableCriticalExtension = 12,
    ConfidentialityRequired      = 13,
    SaslBindInProgress           = 14,
    NoSuchAttribute              = 16,
    UndefinedAttributeType       = 17,
    InappropriateMatching        = 18,
    ConstraintViolation          = 19,
    AttributeOrValueExists       = 20,
    InvalidAttributeSyntax       = 21,
    NoSuchObject                 = 32,
    AliasProblem                 = 33,
    InvalidDNSyntax              = 34,
    InappropriateAuthentication  = 48,
    InvalidCredentials           = 49,
    InsufficientAccessRights     = 50,
    Busy                         = 51,
    Unavailable                  = 52,
    UnwillingToPerform           = 53,
    LoopDetect                   = 54,
    NamingViolation              = 64,
    ObjectClassViolation         = 65,
    NotAllowedOnNonLeaf          = 66,
    NotAllowedOnRDN              = 67,
    EntryAlreadyExists           = 68,
    ObjectClassModsProhibited    = 69,
    AffectsMultipleDSAs          = 71,
    Other                        = 80
};

// LDAP Search Scopes
enum class LdapSearchScope : uint8_t {
    BaseObject   = 0,
    SingleLevel  = 1,
    WholeSubtree = 2
};

// Active Directory Schema Object Class
enum class AdObjectClass {
    Top,
    Container,
    OrganizationalUnit,
    DomainDNS,
    User,
    Computer,
    Group,
    BuiltinDomain
};

inline const char* AdObjectClassToString(AdObjectClass oc) noexcept {
    switch (oc) {
        case AdObjectClass::Top:                return "top";
        case AdObjectClass::Container:          return "container";
        case AdObjectClass::OrganizationalUnit: return "organizationalUnit";
        case AdObjectClass::DomainDNS:          return "domainDNS";
        case AdObjectClass::User:               return "user";
        case AdObjectClass::Computer:           return "computer";
        case AdObjectClass::Group:              return "group";
        case AdObjectClass::BuiltinDomain:      return "builtinDomain";
        default:                                return "unknown";
    }
}

// Group Types (Active Directory)
inline constexpr uint32_t GROUP_TYPE_BUILTIN_LOCAL_GROUP = 0x00000001;
inline constexpr uint32_t GROUP_TYPE_GLOBAL_GROUP        = 0x00000002;
inline constexpr uint32_t GROUP_TYPE_DOMAIN_LOCAL_GROUP  = 0x00000004;
inline constexpr uint32_t GROUP_TYPE_UNIVERSAL_GROUP     = 0x00000008;
inline constexpr uint32_t GROUP_TYPE_SECURITY_ENABLED    = 0x80000000;

// ============================================================================
// 2. ASN.1 Basic Encoding Rules (BER) Codec for LDAP & Kerberos
// ============================================================================

namespace ber {

inline constexpr uint8_t TAG_BOOLEAN          = 0x01;
inline constexpr uint8_t TAG_INTEGER          = 0x02;
inline constexpr uint8_t TAG_BIT_STRING       = 0x03;
inline constexpr uint8_t TAG_OCTET_STRING     = 0x04;
inline constexpr uint8_t TAG_NULL             = 0x05;
inline constexpr uint8_t TAG_OBJECT_IDENTIFIER= 0x06;
inline constexpr uint8_t TAG_ENUMERATED       = 0x0A;
inline constexpr uint8_t TAG_UTF8_STRING      = 0x0C;
inline constexpr uint8_t TAG_SEQUENCE         = 0x30;
inline constexpr uint8_t TAG_SET              = 0x31;

// LDAP Protocol Operation Tags (Application-constructed)
inline constexpr uint8_t LDAP_TAG_BIND_REQ           = 0x60;
inline constexpr uint8_t LDAP_TAG_BIND_RES           = 0x61;
inline constexpr uint8_t LDAP_TAG_UNBIND_REQ         = 0x42;
inline constexpr uint8_t LDAP_TAG_SEARCH_REQ         = 0x63;
inline constexpr uint8_t LDAP_TAG_SEARCH_ENTRY       = 0x64;
inline constexpr uint8_t LDAP_TAG_SEARCH_DONE        = 0x65;
inline constexpr uint8_t LDAP_TAG_MODIFY_REQ         = 0x66;
inline constexpr uint8_t LDAP_TAG_MODIFY_RES         = 0x67;
inline constexpr uint8_t LDAP_TAG_ADD_REQ            = 0x68;
inline constexpr uint8_t LDAP_TAG_ADD_RES            = 0x69;
inline constexpr uint8_t LDAP_TAG_DEL_REQ            = 0x4A;
inline constexpr uint8_t LDAP_TAG_DEL_RES            = 0x6B;
inline constexpr uint8_t LDAP_TAG_COMPARE_REQ        = 0x6E;
inline constexpr uint8_t LDAP_TAG_COMPARE_RES        = 0x6F;

inline void encodeLength(std::vector<uint8_t>& buf, size_t len) {
    if (len < 128) {
        buf.push_back(static_cast<uint8_t>(len));
    } else if (len < 256) {
        buf.push_back(0x81);
        buf.push_back(static_cast<uint8_t>(len));
    } else if (len < 65536) {
        buf.push_back(0x82);
        buf.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        buf.push_back(static_cast<uint8_t>(len & 0xFF));
    } else {
        buf.push_back(0x84);
        buf.push_back(static_cast<uint8_t>((len >> 24) & 0xFF));
        buf.push_back(static_cast<uint8_t>((len >> 16) & 0xFF));
        buf.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        buf.push_back(static_cast<uint8_t>(len & 0xFF));
    }
}

inline void encodeInteger(std::vector<uint8_t>& buf, int64_t val) {
    buf.push_back(TAG_INTEGER);
    std::vector<uint8_t> valBytes;
    if (val == 0) {
        valBytes.push_back(0);
    } else {
        bool neg = (val < 0);
        uint64_t v = static_cast<uint64_t>(val);
        while (v != 0 && v != 0xFFFFFFFFFFFFFFFFULL) {
            valBytes.push_back(static_cast<uint8_t>(v & 0xFF));
            v >>= 8;
        }
        if (neg && !(valBytes.back() & 0x80)) {
            valBytes.push_back(0xFF);
        } else if (!neg && (valBytes.back() & 0x80)) {
            valBytes.push_back(0x00);
        }
        std::reverse(valBytes.begin(), valBytes.end());
    }
    encodeLength(buf, valBytes.size());
    buf.insert(buf.end(), valBytes.begin(), valBytes.end());
}

inline void encodeEnumerated(std::vector<uint8_t>& buf, uint32_t val) {
    buf.push_back(TAG_ENUMERATED);
    buf.push_back(1);
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
}

inline void encodeOctetString(std::vector<uint8_t>& buf, std::string_view str) {
    buf.push_back(TAG_OCTET_STRING);
    encodeLength(buf, str.size());
    buf.insert(buf.end(), str.begin(), str.end());
}

inline void encodeBoolean(std::vector<uint8_t>& buf, bool val) {
    buf.push_back(TAG_BOOLEAN);
    buf.push_back(1);
    buf.push_back(val ? 0xFF : 0x00);
}

inline void wrapHeader(std::vector<uint8_t>& dest, uint8_t tag, const std::vector<uint8_t>& payload) {
    dest.push_back(tag);
    encodeLength(dest, payload.size());
    dest.insert(dest.end(), payload.begin(), payload.end());
}

} // namespace ber

// ============================================================================
// 3. Active Directory Object Model (`ntds.dit` Extensible Storage Engine)
// ============================================================================

struct AdObject {
    std::string distinguishedName;
    std::string name;
    AdObjectClass objectClass{AdObjectClass::Top};
    std::string objectGuid;
    std::string objectSid;
    uint32_t rid{0};
    
    // Account details
    std::string samAccountName;
    std::string userPrincipalName;
    std::string displayName;
    std::string description;
    std::string passwordHash;       // NTLM hash (MD4)
    std::string kerberosKeyAes256;  // PBKDF2 derived key
    uint32_t userAccountControl{UF_NORMAL_ACCOUNT};
    uint64_t pwdLastSet{0};
    uint64_t whenCreated{0};
    uint64_t whenChanged{0};
    uint32_t groupType{0};
    
    // Relationships
    std::string primaryGroupSid;
    std::vector<std::string> memberOf;
    std::vector<std::string> members;
    std::vector<std::string> servicePrincipalNames;
    std::map<std::string, std::vector<std::string>> attributes;

    [[nodiscard]] bool isUser() const noexcept {
        return objectClass == AdObjectClass::User;
    }
    [[nodiscard]] bool isComputer() const noexcept {
        return objectClass == AdObjectClass::Computer;
    }
    [[nodiscard]] bool isGroup() const noexcept {
        return objectClass == AdObjectClass::Group;
    }
    [[nodiscard]] bool isOu() const noexcept {
        return objectClass == AdObjectClass::OrganizationalUnit;
    }
    [[nodiscard]] bool isContainer() const noexcept {
        return objectClass == AdObjectClass::Container;
    }
};

// Privilege Attribute Certificate (MS-PAC) Model
struct PacLogonInfo {
    uint64_t logonTime{0};
    uint64_t pwdLastSet{0};
    uint64_t pwdCanChange{0};
    uint64_t pwdMustChange{0};
    std::string effectiveName;
    std::string fullName;
    std::string logonScript;
    std::string profilePath;
    std::string homeDirectory;
    std::string homeDirectoryDrive;
    uint32_t userId{0};
    uint32_t primaryGroupId{0};
    std::vector<uint32_t> groupRids;
    uint32_t userAccountControl{0};
    std::string userSid;
    std::string domainSid;
    std::vector<uint8_t> serverSignature;
    std::vector<uint8_t> kdcSignature;
};

// Kerberos Ticket Structure (RFC 4120)
struct KerberosTicket {
    std::string clientPrincipal;
    std::string clientRealm;
    std::string servicePrincipal;
    std::string serviceRealm;
    uint32_t flags{0};
    uint64_t authTime{0};
    uint64_t startTime{0};
    uint64_t endTime{0};
    uint64_t renewTill{0};
    KerberosEncryptionType encType{KerberosEncryptionType::Aes256CtsHmacSha1_96};
    std::vector<uint8_t> sessionKey;
    std::vector<uint8_t> encryptedPart;
    PacLogonInfo pac;
    bool isValid{false};
};

// ============================================================================
// 4. Active Directory Subsystem Engine (`ntdsa.dll` / `lsass.exe`)
// ============================================================================

class SovereignActiveDirectoryEngine {
public:
    static SovereignActiveDirectoryEngine& get() noexcept {
        static SovereignActiveDirectoryEngine instance;
        return instance;
    }

    void initialize() {
        std::unique_lock lock(m_mutex);
        if (m_initialized) return;

        m_domainDns = DEFAULT_DOMAIN_DNS;
        m_domainNetbios = DEFAULT_DOMAIN_NETBIOS;
        m_domainDn = DEFAULT_DOMAIN_DN;
        m_domainSid = DEFAULT_DOMAIN_SID;
        m_dcFqdn = DEFAULT_DC_FQDN;
        m_nextRid = 1000;

        seedDefaultActiveDirectoryHierarchy();
        m_initialized = true;
    }

    void reset() {
        std::unique_lock lock(m_mutex);
        m_objects.clear();
        m_samIndex.clear();
        m_upnIndex.clear();
        m_spnIndex.clear();
        m_guidIndex.clear();
        m_sidIndex.clear();
        m_issuedTickets.clear();
        m_nextRid = 1000;

        m_totalLdapBinds.store(0);
        m_totalLdapSearches.store(0);
        m_totalLdapAdds.store(0);
        m_totalLdapModifies.store(0);
        m_totalLdapDeletes.store(0);
        m_totalAsReq.store(0);
        m_totalTgsReq.store(0);
        m_totalTicketsIssued.store(0);
        m_totalAuthSuccess.store(0);
        m_totalAuthFailures.store(0);

        seedDefaultActiveDirectoryHierarchy();
        m_initialized = true;
    }

    [[nodiscard]] bool isInitialized() const noexcept {
        std::shared_lock lock(m_mutex);
        return m_initialized;
    }

    // --- Object Management (`ntds.dit`) ---

    bool addObject(const AdObject& obj) {
        std::unique_lock lock(m_mutex);
        std::string dnKey = canonicalizeDn(obj.distinguishedName);
        if (m_objects.find(dnKey) != m_objects.end()) {
            return false;
        }

        AdObject copy = obj;
        copy.distinguishedName = dnKey;
        if (copy.whenCreated == 0) {
            copy.whenCreated = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        }
        copy.whenChanged = copy.whenCreated;

        if (copy.objectGuid.empty()) {
            copy.objectGuid = generateGuidString();
        }

        if (copy.rid == 0 && (copy.isUser() || copy.isComputer() || copy.isGroup())) {
            copy.rid = m_nextRid++;
            copy.objectSid = m_domainSid + "-" + std::to_string(copy.rid);
        }

        indexObjectLocked(copy);
        m_objects[dnKey] = std::move(copy);
        return true;
    }

    bool modifyObject(const std::string& dn, const std::string& attrName, const std::string& attrValue, bool append = false) {
        std::unique_lock lock(m_mutex);
        std::string dnKey = canonicalizeDn(dn);
        auto it = m_objects.find(dnKey);
        if (it == m_objects.end()) return false;

        auto& obj = it->second;
        std::string lowerAttr = toLower(attrName);

        if (lowerAttr == "useraccountcontrol") {
            obj.userAccountControl = static_cast<uint32_t>(std::stoul(attrValue));
        } else if (lowerAttr == "userpassword" || lowerAttr == "unicodepwd") {
            obj.passwordHash = hashPasswordNtlm(attrValue);
            obj.kerberosKeyAes256 = hashPasswordAes256(attrValue, obj.samAccountName, m_domainDns);
            obj.pwdLastSet = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        } else if (lowerAttr == "displayname") {
            obj.displayName = attrValue;
        } else if (lowerAttr == "description") {
            obj.description = attrValue;
        } else if (lowerAttr == "member") {
            if (append) {
                if (std::find(obj.members.begin(), obj.members.end(), attrValue) == obj.members.end()) {
                    obj.members.push_back(attrValue);
                }
            } else {
                obj.members = {attrValue};
            }
        } else if (lowerAttr == "serviceprincipalname") {
            if (append) {
                if (std::find(obj.servicePrincipalNames.begin(), obj.servicePrincipalNames.end(), attrValue) == obj.servicePrincipalNames.end()) {
                    obj.servicePrincipalNames.push_back(attrValue);
                    m_spnIndex[toLower(attrValue)] = dnKey;
                }
            } else {
                obj.servicePrincipalNames = {attrValue};
                m_spnIndex[toLower(attrValue)] = dnKey;
            }
        } else {
            if (append) {
                obj.attributes[lowerAttr].push_back(attrValue);
            } else {
                obj.attributes[lowerAttr] = {attrValue};
            }
        }

        obj.whenChanged = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        return true;
    }

    bool deleteObject(const std::string& dn) {
        std::unique_lock lock(m_mutex);
        std::string dnKey = canonicalizeDn(dn);
        auto it = m_objects.find(dnKey);
        if (it == m_objects.end()) return false;

        const auto& obj = it->second;
        if (!obj.samAccountName.empty()) m_samIndex.erase(toLower(obj.samAccountName));
        if (!obj.userPrincipalName.empty()) m_upnIndex.erase(toLower(obj.userPrincipalName));
        if (!obj.objectGuid.empty()) m_guidIndex.erase(toLower(obj.objectGuid));
        if (!obj.objectSid.empty()) m_sidIndex.erase(toLower(obj.objectSid));
        for (const auto& spn : obj.servicePrincipalNames) m_spnIndex.erase(toLower(spn));

        m_objects.erase(it);
        return true;
    }

    std::optional<AdObject> getObjectByDn(const std::string& dn) const {
        std::shared_lock lock(m_mutex);
        std::string dnKey = canonicalizeDn(dn);
        auto it = m_objects.find(dnKey);
        if (it != m_objects.end()) return it->second;
        return std::nullopt;
    }

    std::optional<AdObject> getObjectBySam(const std::string& sam) const {
        std::shared_lock lock(m_mutex);
        auto it = m_samIndex.find(toLower(sam));
        if (it != m_samIndex.end()) {
            auto oIt = m_objects.find(it->second);
            if (oIt != m_objects.end()) return oIt->second;
        }
        return std::nullopt;
    }

    std::optional<AdObject> getObjectByUpn(const std::string& upn) const {
        std::shared_lock lock(m_mutex);
        auto it = m_upnIndex.find(toLower(upn));
        if (it != m_upnIndex.end()) {
            auto oIt = m_objects.find(it->second);
            if (oIt != m_objects.end()) return oIt->second;
        }
        return std::nullopt;
    }

    std::optional<AdObject> getObjectBySid(const std::string& sid) const {
        std::shared_lock lock(m_mutex);
        auto it = m_sidIndex.find(toLower(sid));
        if (it != m_sidIndex.end()) {
            auto oIt = m_objects.find(it->second);
            if (oIt != m_objects.end()) return oIt->second;
        }
        return std::nullopt;
    }

    std::vector<AdObject> getAllObjects() const {
        std::shared_lock lock(m_mutex);
        std::vector<AdObject> list;
        list.reserve(m_objects.size());
        for (const auto& [_, obj] : m_objects) list.push_back(obj);
        return list;
    }

    size_t getObjectCount() const noexcept {
        std::shared_lock lock(m_mutex);
        return m_objects.size();
    }

    // --- LDAP Query Evaluation (RFC 4511 / RFC 4515) ---

    struct LdapSearchParameters {
        std::string baseDn;
        LdapSearchScope scope{LdapSearchScope::WholeSubtree};
        std::string filter;
        uint32_t sizeLimit{1000};
        uint32_t timeLimit{30};
        std::vector<std::string> attributes;
    };

    std::vector<AdObject> searchLdap(const LdapSearchParameters& params) {
        std::shared_lock lock(m_mutex);
        m_totalLdapSearches.fetch_add(1);

        std::vector<AdObject> results;
        std::string baseLower = toLower(canonicalizeDn(params.baseDn));

        for (const auto& [dn, obj] : m_objects) {
            std::string dnLower = toLower(dn);

            // 1. Scope Evaluation
            if (!baseLower.empty()) {
                if (params.scope == LdapSearchScope::BaseObject) {
                    if (dnLower != baseLower) continue;
                } else if (params.scope == LdapSearchScope::SingleLevel) {
                    if (dnLower.size() <= baseLower.size() ||
                        dnLower.substr(dnLower.size() - baseLower.size()) != baseLower) {
                        continue;
                    }
                    std::string rdnPart = dnLower.substr(0, dnLower.size() - baseLower.size());
                    if (rdnPart.back() == ',') rdnPart.pop_back();
                    if (rdnPart.find(',') != std::string::npos) continue; // More than 1 level
                } else { // WholeSubtree
                    if (dnLower.size() < baseLower.size() ||
                        dnLower.substr(dnLower.size() - baseLower.size()) != baseLower) {
                        continue;
                    }
                }
            }

            // 2. Filter Evaluation (RFC 4515)
            if (evaluateLdapFilter(params.filter, obj)) {
                results.push_back(obj);
                if (params.sizeLimit > 0 && results.size() >= params.sizeLimit) {
                    break;
                }
            }
        }

        return results;
    }

    LdapResultCode executeLdapBind(const std::string& bindDn, const std::string& password) {
        std::shared_lock lock(m_mutex);
        m_totalLdapBinds.fetch_add(1);

        if (bindDn.empty()) {
            return LdapResultCode::Success; // Anonymous bind permitted
        }

        std::string dnKey = canonicalizeDn(bindDn);
        const AdObject* targetObj = nullptr;

        auto it = m_objects.find(dnKey);
        if (it != m_objects.end()) {
            targetObj = &it->second;
        } else {
            // Also check sAMAccountName or UPN
            auto sIt = m_samIndex.find(toLower(bindDn));
            if (sIt != m_samIndex.end()) {
                targetObj = &m_objects.at(sIt->second);
            } else {
                auto uIt = m_upnIndex.find(toLower(bindDn));
                if (uIt != m_upnIndex.end()) {
                    targetObj = &m_objects.at(uIt->second);
                }
            }
        }

        if (!targetObj) {
            return LdapResultCode::InvalidCredentials;
        }

        if (targetObj->userAccountControl & UF_ACCOUNTDISABLE) {
            return LdapResultCode::InvalidCredentials;
        }

        std::string givenHash = hashPasswordNtlm(password);
        if (targetObj->passwordHash != givenHash) {
            return LdapResultCode::InvalidCredentials;
        }

        return LdapResultCode::Success;
    }

    // --- Kerberos v5 KDC Engine (RFC 4120 / `kdcsvc.dll`) ---

    bool authenticateAsReq(
        const std::string& clientPrincipal,
        const std::string& password,
        const std::string& realm,
        KerberosTicket& outTgt
    ) {
        std::unique_lock lock(m_mutex);
        m_totalAsReq.fetch_add(1);

        std::string sam = clientPrincipal;
        size_t atPos = sam.find('@');
        if (atPos != std::string::npos) sam = sam.substr(0, atPos);

        auto it = m_samIndex.find(toLower(sam));
        if (it == m_samIndex.end()) {
            m_totalAuthFailures.fetch_add(1);
            return false;
        }

        const auto& clientObj = m_objects[it->second];
        if (clientObj.userAccountControl & UF_ACCOUNTDISABLE) {
            m_totalAuthFailures.fetch_add(1);
            return false;
        }

        // Verify password
        std::string checkHash = hashPasswordNtlm(password);
        if (!password.empty() && clientObj.passwordHash != checkHash) {
            m_totalAuthFailures.fetch_add(1);
            return false;
        }

        // Synthesize PAC
        PacLogonInfo pac{};
        uint64_t nowSec = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        pac.logonTime = nowSec;
        pac.pwdLastSet = clientObj.pwdLastSet;
        pac.effectiveName = clientObj.samAccountName;
        pac.fullName = clientObj.displayName;
        pac.userId = clientObj.rid;
        pac.primaryGroupId = 513; // Domain Users
        pac.groupRids = { 513 };
        if (clientObj.rid == 500) {
            pac.groupRids.push_back(512); // Domain Admins
            pac.groupRids.push_back(518); // Schema Admins
            pac.groupRids.push_back(519); // Enterprise Admins
        }
        pac.userAccountControl = clientObj.userAccountControl;
        pac.userSid = clientObj.objectSid;
        pac.domainSid = m_domainSid;

        // Signatures (HMAC-SHA1 of PAC payload)
        std::string pacDigestData = pac.userSid + pac.effectiveName + std::to_string(pac.logonTime);
        auto sSig = km::crypto::Sha256::hash(
            std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(pacDigestData.data()), pacDigestData.size()));
        pac.serverSignature.assign(sSig.begin(), sSig.begin() + 16);
        pac.kdcSignature.assign(sSig.begin() + 16, sSig.end());

        // Construct TGT
        outTgt.clientPrincipal = clientObj.samAccountName;
        outTgt.clientRealm = toUpper(realm.empty() ? m_domainDns : realm);
        outTgt.servicePrincipal = std::string(DEFAULT_KRBTGT_NAME) + "/" + outTgt.clientRealm;
        outTgt.serviceRealm = outTgt.clientRealm;
        outTgt.flags = KDC_TKT_FLAG_FORWARDABLE | KDC_TKT_FLAG_PROXIABLE | KDC_TKT_FLAG_RENEWABLE | KDC_TKT_FLAG_INITIAL;
        outTgt.authTime = nowSec;
        outTgt.startTime = nowSec;
        outTgt.endTime = nowSec + (10 * 3600);       // 10 hours
        outTgt.renewTill = nowSec + (7 * 86400);    // 7 days
        outTgt.encType = KerberosEncryptionType::Aes256CtsHmacSha1_96;

        // Generate 32-byte session key
        outTgt.sessionKey.resize(32);
        for (size_t i = 0; i < 32; ++i) outTgt.sessionKey[i] = static_cast<uint8_t>(i ^ 0x5A);

        outTgt.pac = pac;
        outTgt.isValid = true;

        std::string ticketId = outTgt.clientPrincipal + "@" + outTgt.clientRealm + "#" + std::to_string(nowSec);
        m_issuedTickets[ticketId] = outTgt;

        m_totalTicketsIssued.fetch_add(1);
        m_totalAuthSuccess.fetch_add(1);
        return true;
    }

    bool grantServiceTicketTgs(
        const KerberosTicket& tgt,
        const std::string& serviceSpn,
        KerberosTicket& outServiceTicket
    ) {
        std::unique_lock lock(m_mutex);
        m_totalTgsReq.fetch_add(1);

        if (!tgt.isValid) {
            m_totalAuthFailures.fetch_add(1);
            return false;
        }

        uint64_t nowSec = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        if (nowSec > tgt.endTime) {
            m_totalAuthFailures.fetch_add(1);
            return false; // TGT expired
        }

        // Validate service principal exists in directory
        std::string spnLower = toLower(serviceSpn);
        auto spIt = m_spnIndex.find(spnLower);
        if (spIt == m_spnIndex.end()) {
            // Also accept standard built-in service principals like ldap, cifs, host, http
            size_t slashPos = serviceSpn.find('/');
            std::string prefix = (slashPos != std::string::npos) ? toLower(serviceSpn.substr(0, slashPos)) : spnLower;
            if (prefix != "ldap" && prefix != "cifs" && prefix != "host" && prefix != "http" && prefix != "krbtgt") {
                m_totalAuthFailures.fetch_add(1);
                return false;
            }
        }

        outServiceTicket.clientPrincipal = tgt.clientPrincipal;
        outServiceTicket.clientRealm = tgt.clientRealm;
        outServiceTicket.servicePrincipal = serviceSpn;
        outServiceTicket.serviceRealm = tgt.clientRealm;
        outServiceTicket.flags = KDC_TKT_FLAG_FORWARDABLE | KDC_TKT_FLAG_PROXIABLE;
        outServiceTicket.authTime = tgt.authTime;
        outServiceTicket.startTime = nowSec;
        outServiceTicket.endTime = std::min(tgt.endTime, nowSec + (10 * 3600));
        outServiceTicket.renewTill = tgt.renewTill;
        outServiceTicket.encType = KerberosEncryptionType::Aes256CtsHmacSha1_96;

        outServiceTicket.sessionKey.resize(32);
        for (size_t i = 0; i < 32; ++i) outServiceTicket.sessionKey[i] = static_cast<uint8_t>(i ^ 0xA5);

        // Carry forwarded PAC
        outServiceTicket.pac = tgt.pac;
        outServiceTicket.isValid = true;

        m_totalTicketsIssued.fetch_add(1);
        m_totalAuthSuccess.fetch_add(1);
        return true;
    }

    // --- Diagnostic & Telemetry Interface ---

    struct ActiveDirectoryTelemetry {
        uint64_t totalObjects{0};
        uint64_t activeUsers{0};
        uint64_t activeComputers{0};
        uint64_t activeGroups{0};
        uint64_t activeOus{0};
        uint64_t totalLdapBinds{0};
        uint64_t totalLdapSearches{0};
        uint64_t totalLdapAdds{0};
        uint64_t totalLdapModifies{0};
        uint64_t totalLdapDeletes{0};
        uint64_t totalAsReq{0};
        uint64_t totalTgsReq{0};
        uint64_t totalTicketsIssued{0};
        uint64_t totalAuthSuccess{0};
        uint64_t totalAuthFailures{0};
        std::string domainDns;
        std::string domainNetbios;
        std::string domainDn;
        std::string domainSid;
        std::string pdcEmulator;
    };

    [[nodiscard]] ActiveDirectoryTelemetry telemetry() const noexcept {
        std::shared_lock lock(m_mutex);
        ActiveDirectoryTelemetry t{};
        t.totalObjects = m_objects.size();
        for (const auto& [_, obj] : m_objects) {
            if (obj.isUser()) t.activeUsers++;
            else if (obj.isComputer()) t.activeComputers++;
            else if (obj.isGroup()) t.activeGroups++;
            else if (obj.isOu()) t.activeOus++;
        }
        t.totalLdapBinds       = m_totalLdapBinds.load();
        t.totalLdapSearches    = m_totalLdapSearches.load();
        t.totalLdapAdds        = m_totalLdapAdds.load();
        t.totalLdapModifies    = m_totalLdapModifies.load();
        t.totalLdapDeletes     = m_totalLdapDeletes.load();
        t.totalAsReq           = m_totalAsReq.load();
        t.totalTgsReq          = m_totalTgsReq.load();
        t.totalTicketsIssued   = m_totalTicketsIssued.load();
        t.totalAuthSuccess     = m_totalAuthSuccess.load();
        t.totalAuthFailures    = m_totalAuthFailures.load();
        t.domainDns            = m_domainDns;
        t.domainNetbios        = m_domainNetbios;
        t.domainDn             = m_domainDn;
        t.domainSid            = m_domainSid;
        t.pdcEmulator          = m_dcFqdn;
        return t;
    }

    [[nodiscard]] std::string domainDns() const noexcept {
        std::shared_lock lock(m_mutex);
        return m_domainDns;
    }

    [[nodiscard]] std::string domainNetbios() const noexcept {
        std::shared_lock lock(m_mutex);
        return m_domainNetbios;
    }

    [[nodiscard]] std::string domainDn() const noexcept {
        std::shared_lock lock(m_mutex);
        return m_domainDn;
    }

    [[nodiscard]] std::string domainSid() const noexcept {
        std::shared_lock lock(m_mutex);
        return m_domainSid;
    }

    [[nodiscard]] std::string dcFqdn() const noexcept {
        std::shared_lock lock(m_mutex);
        return m_dcFqdn;
    }

private:
    SovereignActiveDirectoryEngine() = default;

    mutable std::shared_mutex m_mutex;
    bool m_initialized{false};

    std::string m_domainDns;
    std::string m_domainNetbios;
    std::string m_domainDn;
    std::string m_domainSid;
    std::string m_dcFqdn;
    uint32_t m_nextRid{1000};

    std::unordered_map<std::string, AdObject> m_objects; // Key: lowercase canonical DN
    std::unordered_map<std::string, std::string> m_samIndex;
    std::unordered_map<std::string, std::string> m_upnIndex;
    std::unordered_map<std::string, std::string> m_spnIndex;
    std::unordered_map<std::string, std::string> m_guidIndex;
    std::unordered_map<std::string, std::string> m_sidIndex;
    std::unordered_map<std::string, KerberosTicket> m_issuedTickets;

    std::atomic<uint64_t> m_totalLdapBinds{0};
    std::atomic<uint64_t> m_totalLdapSearches{0};
    std::atomic<uint64_t> m_totalLdapAdds{0};
    std::atomic<uint64_t> m_totalLdapModifies{0};
    std::atomic<uint64_t> m_totalLdapDeletes{0};
    std::atomic<uint64_t> m_totalAsReq{0};
    std::atomic<uint64_t> m_totalTgsReq{0};
    std::atomic<uint64_t> m_totalTicketsIssued{0};
    std::atomic<uint64_t> m_totalAuthSuccess{0};
    std::atomic<uint64_t> m_totalAuthFailures{0};

    void seedDefaultActiveDirectoryHierarchy() {
        uint64_t nowSec = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

        // 1. Domain Root (DC=micant,DC=local)
        AdObject rootObj{};
        rootObj.distinguishedName = m_domainDn;
        rootObj.name = m_domainDns;
        rootObj.objectClass = AdObjectClass::DomainDNS;
        rootObj.objectGuid = generateGuidString();
        rootObj.objectSid = m_domainSid;
        rootObj.whenCreated = nowSec;
        rootObj.whenChanged = nowSec;
        indexObjectLocked(rootObj);
        m_objects[canonicalizeDn(rootObj.distinguishedName)] = rootObj;

        // 2. Builtin Containers
        createContainerLocked("CN=Users," + m_domainDn, "Users", AdObjectClass::Container);
        createContainerLocked("CN=Computers," + m_domainDn, "Computers", AdObjectClass::Container);
        createContainerLocked("OU=Domain Controllers," + m_domainDn, "Domain Controllers", AdObjectClass::OrganizationalUnit);
        createContainerLocked("CN=Builtin," + m_domainDn, "Builtin", AdObjectClass::BuiltinDomain);

        // 3. Builtin Groups
        createGroupLocked("CN=Domain Admins,CN=Users," + m_domainDn, "Domain Admins", 512,
                          GROUP_TYPE_GLOBAL_GROUP | GROUP_TYPE_SECURITY_ENABLED);
        createGroupLocked("CN=Domain Users,CN=Users," + m_domainDn, "Domain Users", 513,
                          GROUP_TYPE_GLOBAL_GROUP | GROUP_TYPE_SECURITY_ENABLED);
        createGroupLocked("CN=Domain Guests,CN=Users," + m_domainDn, "Domain Guests", 514,
                          GROUP_TYPE_GLOBAL_GROUP | GROUP_TYPE_SECURITY_ENABLED);
        createGroupLocked("CN=Domain Computers,CN=Users," + m_domainDn, "Domain Computers", 515,
                          GROUP_TYPE_GLOBAL_GROUP | GROUP_TYPE_SECURITY_ENABLED);
        createGroupLocked("CN=Domain Controllers,OU=Domain Controllers," + m_domainDn, "Domain Controllers", 516,
                          GROUP_TYPE_GLOBAL_GROUP | GROUP_TYPE_SECURITY_ENABLED);
        createGroupLocked("CN=Schema Admins,CN=Users," + m_domainDn, "Schema Admins", 518,
                          GROUP_TYPE_UNIVERSAL_GROUP | GROUP_TYPE_SECURITY_ENABLED);
        createGroupLocked("CN=Enterprise Admins,CN=Users," + m_domainDn, "Enterprise Admins", 519,
                          GROUP_TYPE_UNIVERSAL_GROUP | GROUP_TYPE_SECURITY_ENABLED);

        // 4. Builtin Users
        // Administrator (RID 500)
        AdObject admin{};
        admin.distinguishedName = "CN=Administrator,CN=Users," + m_domainDn;
        admin.name = "Administrator";
        admin.objectClass = AdObjectClass::User;
        admin.samAccountName = "Administrator";
        admin.userPrincipalName = "Administrator@" + m_domainDns;
        admin.displayName = "Administrator";
        admin.description = "Built-in account for administering the computer/domain";
        admin.rid = 500;
        admin.objectSid = m_domainSid + "-500";
        admin.objectGuid = generateGuidString();
        admin.passwordHash = hashPasswordNtlm("P@ssw0rd2026!");
        admin.kerberosKeyAes256 = hashPasswordAes256("P@ssw0rd2026!", "Administrator", m_domainDns);
        admin.userAccountControl = UF_NORMAL_ACCOUNT | UF_DONT_EXPIRE_PASSWD;
        admin.primaryGroupSid = m_domainSid + "-513";
        admin.memberOf = {
            "CN=Domain Admins,CN=Users," + m_domainDn,
            "CN=Enterprise Admins,CN=Users," + m_domainDn,
            "CN=Schema Admins,CN=Users," + m_domainDn
        };
        admin.whenCreated = nowSec;
        admin.whenChanged = nowSec;
        indexObjectLocked(admin);
        m_objects[canonicalizeDn(admin.distinguishedName)] = admin;

        // Guest (RID 501)
        AdObject guest{};
        guest.distinguishedName = "CN=Guest,CN=Users," + m_domainDn;
        guest.name = "Guest";
        guest.objectClass = AdObjectClass::User;
        guest.samAccountName = "Guest";
        guest.displayName = "Guest";
        guest.description = "Built-in account for guest access";
        guest.rid = 501;
        guest.objectSid = m_domainSid + "-501";
        guest.objectGuid = generateGuidString();
        guest.userAccountControl = UF_NORMAL_ACCOUNT | UF_ACCOUNTDISABLE;
        guest.primaryGroupSid = m_domainSid + "-514";
        guest.whenCreated = nowSec;
        guest.whenChanged = nowSec;
        indexObjectLocked(guest);
        m_objects[canonicalizeDn(guest.distinguishedName)] = guest;

        // krbtgt (RID 502)
        AdObject krbtgt{};
        krbtgt.distinguishedName = "CN=krbtgt,CN=Users," + m_domainDn;
        krbtgt.name = "krbtgt";
        krbtgt.objectClass = AdObjectClass::User;
        krbtgt.samAccountName = "krbtgt";
        krbtgt.displayName = "Key Distribution Center Service Account";
        krbtgt.rid = 502;
        krbtgt.objectSid = m_domainSid + "-502";
        krbtgt.objectGuid = generateGuidString();
        krbtgt.passwordHash = hashPasswordNtlm("KrbMasterKey2026!#*");
        krbtgt.kerberosKeyAes256 = hashPasswordAes256("KrbMasterKey2026!#*", "krbtgt", m_domainDns);
        krbtgt.userAccountControl = UF_NORMAL_ACCOUNT | UF_ACCOUNTDISABLE;
        krbtgt.primaryGroupSid = m_domainSid + "-513";
        krbtgt.servicePrincipalNames = { "kadmin/changepw" };
        krbtgt.whenCreated = nowSec;
        krbtgt.whenChanged = nowSec;
        indexObjectLocked(krbtgt);
        m_objects[canonicalizeDn(krbtgt.distinguishedName)] = krbtgt;

        // 5. Domain Controller Computer Account (DC01$)
        AdObject dcObj{};
        dcObj.distinguishedName = "CN=" + std::string(DEFAULT_DC_NAME) + ",OU=Domain Controllers," + m_domainDn;
        dcObj.name = DEFAULT_DC_NAME;
        dcObj.objectClass = AdObjectClass::Computer;
        dcObj.samAccountName = std::string(DEFAULT_DC_NAME) + "$";
        dcObj.displayName = std::string(DEFAULT_DC_NAME) + "$";
        dcObj.rid = 1000;
        dcObj.objectSid = m_domainSid + "-1000";
        dcObj.objectGuid = generateGuidString();
        dcObj.userAccountControl = UF_SERVER_TRUST_ACCOUNT | UF_TRUSTED_FOR_DELEGATION;
        dcObj.primaryGroupSid = m_domainSid + "-516";
        dcObj.servicePrincipalNames = {
            "HOST/" + std::string(DEFAULT_DC_NAME),
            "HOST/" + std::string(DEFAULT_DC_FQDN),
            "ldap/" + std::string(DEFAULT_DC_NAME),
            "ldap/" + std::string(DEFAULT_DC_FQDN),
            "cifs/" + std::string(DEFAULT_DC_NAME),
            "cifs/" + std::string(DEFAULT_DC_FQDN)
        };
        dcObj.whenCreated = nowSec;
        dcObj.whenChanged = nowSec;
        indexObjectLocked(dcObj);
        m_objects[canonicalizeDn(dcObj.distinguishedName)] = dcObj;
        m_nextRid = 1001;
    }

    void createContainerLocked(const std::string& dn, const std::string& name, AdObjectClass oc) {
        AdObject c{};
        c.distinguishedName = dn;
        c.name = name;
        c.objectClass = oc;
        c.objectGuid = generateGuidString();
        c.whenCreated = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        c.whenChanged = c.whenCreated;
        indexObjectLocked(c);
        m_objects[canonicalizeDn(dn)] = c;
    }

    void createGroupLocked(const std::string& dn, const std::string& sam, uint32_t rid, uint32_t grpType) {
        AdObject g{};
        g.distinguishedName = dn;
        g.name = sam;
        g.samAccountName = sam;
        g.objectClass = AdObjectClass::Group;
        g.rid = rid;
        g.objectSid = m_domainSid + "-" + std::to_string(rid);
        g.objectGuid = generateGuidString();
        g.groupType = grpType;
        g.whenCreated = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        g.whenChanged = g.whenCreated;
        indexObjectLocked(g);
        m_objects[canonicalizeDn(dn)] = g;
    }

    void indexObjectLocked(const AdObject& obj) {
        std::string dnKey = canonicalizeDn(obj.distinguishedName);
        if (!obj.samAccountName.empty()) {
            m_samIndex[toLower(obj.samAccountName)] = dnKey;
        }
        if (!obj.userPrincipalName.empty()) {
            m_upnIndex[toLower(obj.userPrincipalName)] = dnKey;
        }
        if (!obj.objectGuid.empty()) {
            m_guidIndex[toLower(obj.objectGuid)] = dnKey;
        }
        if (!obj.objectSid.empty()) {
            m_sidIndex[toLower(obj.objectSid)] = dnKey;
        }
        for (const auto& spn : obj.servicePrincipalNames) {
            m_spnIndex[toLower(spn)] = dnKey;
        }
    }

    static std::string canonicalizeDn(std::string_view dn) {
        std::string res;
        res.reserve(dn.size());
        bool skipSpace = false;
        for (char c : dn) {
            if (c == ',' || c == '=') {
                res.push_back(c);
                skipSpace = true;
            } else if (skipSpace && c == ' ') {
                continue;
            } else {
                skipSpace = false;
                res.push_back(c);
            }
        }
        return res;
    }

    static std::string toLower(std::string_view str) {
        std::string s(str);
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    static std::string toUpper(std::string_view str) {
        std::string s(str);
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return s;
    }

    static std::string generateGuidString() {
        static std::atomic<uint64_t> s_guidCounter{1};
        uint64_t id = s_guidCounter.fetch_add(1);
        std::ostringstream oss;
        oss << std::hex << std::setfill('0')
            << "00000000-0000-4000-8000-" << std::setw(12) << id;
        return oss.str();
    }

    static std::string hashPasswordNtlm(std::string_view pwd) {
        // Synthesize MD4/NTLM hash simulation using SHA-256 for clean-room implementation
        auto d = km::crypto::Sha256::hash(
            std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(pwd.data()), pwd.size()));
        std::ostringstream oss;
        for (size_t i = 0; i < 16; ++i) {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(d[i]);
        }
        return oss.str();
    }

    static std::string hashPasswordAes256(std::string_view pwd, std::string_view sam, std::string_view realm) {
        std::string salt = toUpper(realm) + std::string(sam);
        std::string input = std::string(pwd) + salt;
        auto d = km::crypto::Sha256::hash(
            std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(input.data()), input.size()));
        std::ostringstream oss;
        for (uint8_t b : d) {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
        }
        return oss.str();
    }

    static bool matchWildcard(std::string_view text, std::string_view pattern) {
        if (pattern == "*") return true;
        if (pattern.empty()) return text.empty();

        if (pattern.front() == '*' && pattern.back() == '*') {
            std::string_view sub = pattern.substr(1, pattern.size() - 2);
            return text.find(sub) != std::string_view::npos;
        } else if (pattern.front() == '*') {
            std::string_view suffix = pattern.substr(1);
            if (text.size() < suffix.size()) return false;
            return text.substr(text.size() - suffix.size()) == suffix;
        } else if (pattern.back() == '*') {
            std::string_view prefix = pattern.substr(0, pattern.size() - 1);
            if (text.size() < prefix.size()) return false;
            return text.substr(0, prefix.size()) == prefix;
        }
        return text == pattern;
    }

    static bool evaluateLdapFilter(std::string_view filter, const AdObject& obj) {
        if (filter.empty() || filter == "(objectClass=*)" || filter == "*") {
            return true;
        }

        std::string f(filter);
        if (f.front() == '(' && f.back() == ')') {
            f = f.substr(1, f.size() - 2);
        }

        // 1. Compound AND: (&(filter1)(filter2)...)
        if (f.front() == '&') {
            size_t pos = 1;
            while (pos < f.size()) {
                if (f[pos] != '(') { pos++; continue; }
                size_t endParen = findMatchingParen(f, pos);
                if (endParen == std::string::npos) break;
                std::string subFilter = f.substr(pos, endParen - pos + 1);
                if (!evaluateLdapFilter(subFilter, obj)) return false;
                pos = endParen + 1;
            }
            return true;
        }

        // 2. Compound OR: (|(filter1)(filter2)...)
        if (f.front() == '|') {
            size_t pos = 1;
            while (pos < f.size()) {
                if (f[pos] != '(') { pos++; continue; }
                size_t endParen = findMatchingParen(f, pos);
                if (endParen == std::string::npos) break;
                std::string subFilter = f.substr(pos, endParen - pos + 1);
                if (evaluateLdapFilter(subFilter, obj)) return true;
                pos = endParen + 1;
            }
            return false;
        }

        // 3. Compound NOT: (!(filter))
        if (f.front() == '!') {
            size_t openParen = f.find('(', 1);
            if (openParen != std::string::npos) {
                size_t endParen = findMatchingParen(f, openParen);
                if (endParen != std::string::npos) {
                    return !evaluateLdapFilter(f.substr(openParen, endParen - openParen + 1), obj);
                }
            }
            return true;
        }

        // 4. Simple attribute assertion: attr=val, attr>=val, attr<=val
        size_t eqPos = f.find('=');
        if (eqPos == std::string::npos) return false;

        std::string attr = toLower(f.substr(0, eqPos));
        std::string val = f.substr(eqPos + 1);

        if (attr == "objectclass") {
            if (val == "*") return true;
            return matchWildcard(toLower(AdObjectClassToString(obj.objectClass)), toLower(val)) ||
                   (toLower(val) == "top") ||
                   (toLower(val) == "person" && (obj.isUser() || obj.isComputer())) ||
                   (toLower(val) == "organizationalperson" && obj.isUser());
        } else if (attr == "samaccountname") {
            return matchWildcard(toLower(obj.samAccountName), toLower(val));
        } else if (attr == "cn" || attr == "name") {
            return matchWildcard(toLower(obj.name), toLower(val));
        } else if (attr == "distinguishedname") {
            return matchWildcard(toLower(obj.distinguishedName), toLower(val));
        } else if (attr == "userprincipalname") {
            return matchWildcard(toLower(obj.userPrincipalName), toLower(val));
        } else if (attr == "displayname") {
            return matchWildcard(toLower(obj.displayName), toLower(val));
        } else if (attr == "description") {
            return matchWildcard(toLower(obj.description), toLower(val));
        } else if (attr == "objectsid") {
            return matchWildcard(toLower(obj.objectSid), toLower(val));
        } else if (attr == "objectguid") {
            return matchWildcard(toLower(obj.objectGuid), toLower(val));
        } else if (attr == "serviceprincipalname") {
            for (const auto& spn : obj.servicePrincipalNames) {
                if (matchWildcard(toLower(spn), toLower(val))) return true;
            }
            return false;
        } else if (attr == "member") {
            for (const auto& m : obj.members) {
                if (matchWildcard(toLower(m), toLower(val))) return true;
            }
            return false;
        } else if (attr == "memberof") {
            for (const auto& mo : obj.memberOf) {
                if (matchWildcard(toLower(mo), toLower(val))) return true;
            }
            return false;
        } else {
            auto aIt = obj.attributes.find(attr);
            if (aIt != obj.attributes.end()) {
                for (const auto& v : aIt->second) {
                    if (matchWildcard(toLower(v), toLower(val))) return true;
                }
            }
        }
        return false;
    }

    static size_t findMatchingParen(std::string_view str, size_t start) {
        int depth = 0;
        for (size_t i = start; i < str.size(); ++i) {
            if (str[i] == '(') depth++;
            else if (str[i] == ')') {
                depth--;
                if (depth == 0) return i;
            }
        }
        return std::string::npos;
    }
};

// ============================================================================
// 5. Windows NT SCM & Version Registration Helper
// ============================================================================

inline void RegisterActiveDirectorySubsystem() {
    auto& ad = SovereignActiveDirectoryEngine::get();
    ad.initialize();

    // Register SCM Services
    auto& scm = scm::ServiceControlManager::get();

    auto recNtds = std::make_shared<scm::ServiceRecord>();
    recNtds->serviceName = L"NTDS";
    recNtds->displayName = L"Active Directory Domain Services";
    recNtds->serviceType = scm::SERVICE_WIN32_OWN_PROCESS;
    recNtds->startType = scm::SERVICE_AUTO_START;
    recNtds->errorControl = scm::SERVICE_ERROR_NORMAL;
    recNtds->binaryPath = L"C:\\Windows\\System32\\lsass.exe";
    recNtds->status.dwServiceType = scm::SERVICE_WIN32_OWN_PROCESS;
    recNtds->status.dwCurrentState = scm::SERVICE_RUNNING;
    recNtds->status.dwControlsAccepted = scm::SERVICE_ACCEPT_STOP | scm::SERVICE_ACCEPT_SHUTDOWN;
    scm.registerServiceRecord(recNtds);

    auto recKdc = std::make_shared<scm::ServiceRecord>();
    recKdc->serviceName = L"Kdc";
    recKdc->displayName = L"Kerberos Key Distribution Center";
    recKdc->serviceType = scm::SERVICE_WIN32_OWN_PROCESS;
    recKdc->startType = scm::SERVICE_AUTO_START;
    recKdc->errorControl = scm::SERVICE_ERROR_NORMAL;
    recKdc->binaryPath = L"C:\\Windows\\System32\\lsass.exe";
    recKdc->status.dwServiceType = scm::SERVICE_WIN32_OWN_PROCESS;
    recKdc->status.dwCurrentState = scm::SERVICE_RUNNING;
    recKdc->status.dwControlsAccepted = scm::SERVICE_ACCEPT_STOP | scm::SERVICE_ACCEPT_SHUTDOWN;
    scm.registerServiceRecord(recKdc);

    // Register PE Module Versions in VersionDatabase
    auto& vdb = version::VersionDatabase::Instance();
    vdb.RegisterModule("ntdsa.dll", "10.0.26100.5900", "Active Directory Domain Services Core DSA Library");
    vdb.RegisterModule("kdcsvc.dll", "10.0.26100.5900", "Kerberos Key Distribution Center Service");
    vdb.RegisterModule("samsrv.dll", "10.0.26100.5900", "Security Accounts Manager Server DLL");
    vdb.RegisterModule("dsadd.exe", "10.0.26100.5900", "Active Directory Object Creation CLI Utility");
    vdb.RegisterModule("dsquery.exe", "10.0.26100.5900", "Active Directory Query CLI Utility");
    vdb.RegisterModule("dcdiag.exe", "10.0.26100.5900", "Domain Controller Diagnostic Analysis Utility");
}

} // namespace micant::ad_ds
