#pragma once

#include <cstdint>
#include <cstddef>
#include "ntstatus.hpp"
#include "version.hpp"
#include "scm.hpp"

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>

namespace micant::activedirectory {

// ============================================================================
// 1. Constants & Enumerations
// ============================================================================
constexpr const char* DEFAULT_DOMAIN_DNS   = "micant.internal";
constexpr const char* DEFAULT_DOMAIN_NETBIOS = "MICANT";
constexpr const char* DEFAULT_DOMAIN_DN    = "DC=micant,DC=internal";
constexpr const char* DEFAULT_KRBTGT_NAME  = "krbtgt";
constexpr uint32_t DEFAULT_KDC_PORT        = 88;
constexpr uint32_t DEFAULT_LDAP_PORT       = 389;
constexpr uint32_t DEFAULT_LDAPS_PORT      = 636;
constexpr uint64_t TICKET_LIFETIME_HOURS   = 10;
constexpr uint64_t TICKET_RENEW_DAYS       = 7;

// Ticket Flags (RFC 4120)
constexpr uint32_t TKT_FLG_FORWARDABLE     = 0x40000000;
constexpr uint32_t TKT_FLG_PROXIABLE       = 0x20000000;
constexpr uint32_t TKT_FLG_RENEWABLE       = 0x00800000;
constexpr uint32_t TKT_FLG_INITIAL         = 0x00400000;
constexpr uint32_t TKT_FLG_PRE_AUTHENT     = 0x00200000;
constexpr uint32_t TKT_FLG_TRANSIT_POLICY  = 0x00040000;

// User Account Control (UAC) Flags
constexpr uint32_t UF_ACCOUNTDISABLE       = 0x0002;
constexpr uint32_t UF_NORMAL_ACCOUNT       = 0x0200;
constexpr uint32_t UF_WORKSTATION_TRUST    = 0x1000;
constexpr uint32_t UF_SERVER_TRUST         = 0x2000;
constexpr uint32_t UF_DONT_EXPIRE_PASSWD   = 0x10000;

enum class ObjectClass {
    Container,
    OrganizationalUnit,
    User,
    Computer,
    Group,
    DomainDNS
};

inline const char* ObjectClassToString(ObjectClass oc) noexcept {
    switch (oc) {
        case ObjectClass::Container:          return "Container";
        case ObjectClass::OrganizationalUnit: return "OrganizationalUnit";
        case ObjectClass::User:               return "User";
        case ObjectClass::Computer:           return "Computer";
        case ObjectClass::Group:              return "Group";
        case ObjectClass::DomainDNS:          return "DomainDNS";
        default:                              return "Unknown";
    }
}

enum class KerberosTicketType {
    TicketGrantingTicket, // TGT (AS-REP)
    ServiceTicket         // TGS (TGS-REP)
};

inline const char* KerberosTicketTypeToString(KerberosTicketType tt) noexcept {
    switch (tt) {
        case KerberosTicketType::TicketGrantingTicket: return "TGT (AS-REP)";
        case KerberosTicketType::ServiceTicket:        return "TGS (TGS-REP)";
        default:                                      return "Unknown";
    }
}

enum class EncryptionType {
    AES256_CTS_HMAC_SHA1_96 = 18,
    AES128_CTS_HMAC_SHA1_96 = 17,
    RC4_HMAC                = 23
};

inline const char* EncryptionTypeToString(EncryptionType et) noexcept {
    switch (et) {
        case EncryptionType::AES256_CTS_HMAC_SHA1_96: return "AES-256-CTS-HMAC-SHA1-96";
        case EncryptionType::AES128_CTS_HMAC_SHA1_96: return "AES-128-CTS-HMAC-SHA1-96";
        case EncryptionType::RC4_HMAC:                return "RC4-HMAC-NT";
        default:                                      return "Unknown";
    }
}

enum class TrustDirection {
    Disabled      = 0,
    Inbound       = 1,
    Outbound      = 2,
    Bidirectional = 3
};

inline const char* TrustDirectionToString(TrustDirection td) noexcept {
    switch (td) {
        case TrustDirection::Inbound:       return "Inbound";
        case TrustDirection::Outbound:      return "Outbound";
        case TrustDirection::Bidirectional: return "Bidirectional (Two-Way)";
        default:                            return "Disabled";
    }
}

enum class TrustType {
    Downlevel   = 1,
    Uplevel     = 2, // MIT Kerberos / Active Directory
    Forest      = 3,
    External    = 4
};

inline const char* TrustTypeToString(TrustType tt) noexcept {
    switch (tt) {
        case TrustType::Downlevel: return "NT 4.0 Downlevel";
        case TrustType::Uplevel:   return "Active Directory Realm";
        case TrustType::Forest:    return "Transitive Forest Trust";
        case TrustType::External:  return "Non-Transitive External";
        default:                   return "Unknown";
    }
}

// ============================================================================
// 2. Privilege Attribute Certificate (PAC)
// ============================================================================
struct PacGroupMembership {
    std::string groupSid;
    uint32_t attributes{0x7}; // Mandatory, Default, Enabled
};

struct PacLogonInfo {
    std::string userSid;
    std::string primaryGroupSid;
    std::string accountName;
    std::string domainName;
    std::string domainSid;
    uint32_t userRid{500};
    uint32_t userAccountControl{UF_NORMAL_ACCOUNT | UF_DONT_EXPIRE_PASSWD};
    std::vector<PacGroupMembership> groups;
    bool serverSignatureValid{true};
    bool kdcSignatureValid{true};
};

// ============================================================================
// 3. NTDS Directory Object (`ntds.dit` / ESE Hierarchical Database)
// ============================================================================
struct DirectoryObject {
    std::string distinguishedName;
    std::string samAccountName;
    ObjectClass objectClass{ObjectClass::User};
    std::string objectSid;
    std::string userPrincipalName;
    std::vector<std::string> servicePrincipalNames;
    std::vector<std::string> memberOfSids;
    uint32_t userAccountControl{UF_NORMAL_ACCOUNT};
    std::unordered_map<std::string, std::string> attributes;
    uint64_t createdTimeUs{0};
    uint64_t modifiedTimeUs{0};
    std::string passwordKeyHex; // AES-256 derived key
};

// ============================================================================
// 4. Kerberos v5 Ticket (`kdcsvc.dll`, `kdc.sys`)
// ============================================================================
struct KerberosTicket {
    std::string ticketId;
    KerberosTicketType ticketType{KerberosTicketType::TicketGrantingTicket};
    std::string clientPrincipal;
    std::string servicePrincipal;
    std::string realm{DEFAULT_DOMAIN_DNS};
    std::string sessionKey;
    EncryptionType encType{EncryptionType::AES256_CTS_HMAC_SHA1_96};
    uint32_t flags{TKT_FLG_FORWARDABLE | TKT_FLG_PROXIABLE | TKT_FLG_RENEWABLE | TKT_FLG_INITIAL | TKT_FLG_PRE_AUTHENT};
    uint64_t issueTimeUs{0};
    uint64_t startTimeUs{0};
    uint64_t endTimeUs{0};
    uint64_t renewTillUs{0};
    std::string clientAddress{"127.0.0.1"};
    PacLogonInfo pac{};
    bool isRevoked{false};

    bool isExpired(uint64_t nowUs) const noexcept {
        return nowUs >= endTimeUs;
    }
};

// ============================================================================
// 5. Cross-Realm Domain Trust (`netlogon.dll`)
// ============================================================================
struct DomainTrust {
    std::string partnerDomain;
    std::string netbiosName;
    TrustDirection direction{TrustDirection::Bidirectional};
    TrustType type{TrustType::Forest};
    bool isTransitive{true};
    std::string trustSecretKeyHex;
    uint64_t establishedTimeUs{0};
};

// ============================================================================
// 6. Active Directory Domain Services Subsystem (TitanDirectory / AegisKDC)
// ============================================================================
class ActiveDirectorySubsystem {
public:
    static ActiveDirectorySubsystem& get() noexcept {
        static ActiveDirectorySubsystem instance;
        return instance;
    }

    void initialize() {
        std::unique_lock lock(m_mutex);
        initializeLocked();
    }

    void reset() {
        std::unique_lock lock(m_mutex);
        m_objects.clear();
        m_samIndex.clear();
        m_spnIndex.clear();
        m_tickets.clear();
        m_trusts.clear();
        m_asReqCount->store(0);
        m_tgsReqCount->store(0);
        m_ldapQueryCount->store(0);
        m_authSuccessCount->store(0);
        m_authFailCount->store(0);
        m_initialized = false;
        initializeLocked();
    }

    bool isInitialized() const noexcept {
        std::shared_lock lock(m_mutex);
        return m_initialized;
    }

    // --- Directory Object Management (ntds.dit) ---
    bool addObject(const DirectoryObject& obj) {
        std::unique_lock lock(m_mutex);
        if (m_objects.find(obj.distinguishedName) != m_objects.end()) return false;

        m_objects[obj.distinguishedName] = obj;
        if (!obj.samAccountName.empty()) {
            std::string samLower = toLower(obj.samAccountName);
            m_samIndex[samLower] = obj.distinguishedName;
        }
        for (const auto& spn : obj.servicePrincipalNames) {
            std::string spnLower = toLower(spn);
            m_spnIndex[spnLower] = obj.distinguishedName;
        }
        return true;
    }

    const DirectoryObject* getObjectByDn(const std::string& dn) const {
        std::shared_lock lock(m_mutex);
        auto it = m_objects.find(dn);
        return (it != m_objects.end()) ? &it->second : nullptr;
    }

    const DirectoryObject* getObjectBySam(const std::string& sam) const {
        std::shared_lock lock(m_mutex);
        auto it = m_samIndex.find(toLower(sam));
        if (it == m_samIndex.end()) return nullptr;
        auto objIt = m_objects.find(it->second);
        return (objIt != m_objects.end()) ? &objIt->second : nullptr;
    }

    const DirectoryObject* getObjectBySpn(const std::string& spn) const {
        std::shared_lock lock(m_mutex);
        auto it = m_spnIndex.find(toLower(spn));
        if (it == m_spnIndex.end()) return nullptr;
        auto objIt = m_objects.find(it->second);
        return (objIt != m_objects.end()) ? &objIt->second : nullptr;
    }

    size_t getObjectCount() const noexcept {
        std::shared_lock lock(m_mutex);
        return m_objects.size();
    }

    std::vector<DirectoryObject> getAllObjects() const {
        std::shared_lock lock(m_mutex);
        std::vector<DirectoryObject> list;
        list.reserve(m_objects.size());
        for (const auto& [_, obj] : m_objects) list.push_back(obj);
        return list;
    }

    // --- LDAP Query Evaluation (wldap32.dll) ---
    std::vector<DirectoryObject> searchLdap(
        const std::string& baseDn,
        const std::string& filter,
        bool subtreeScope = true) const
    {
        std::shared_lock lock(m_mutex);
        m_ldapQueryCount->fetch_add(1);

        std::vector<DirectoryObject> results;
        std::string baseLower = toLower(baseDn);

        // Simple LDAP Filter Evaluator: e.g. "(objectClass=user)", "(sAMAccountName=Administrator)", "(cn=TITAN*)"
        std::string cleanFilter = filter;
        if (cleanFilter.front() == '(' && cleanFilter.back() == ')') {
            cleanFilter = cleanFilter.substr(1, cleanFilter.size() - 2);
        }

        std::string attrName;
        std::string attrVal;
        size_t eqPos = cleanFilter.find('=');
        if (eqPos != std::string::npos) {
            attrName = toLower(cleanFilter.substr(0, eqPos));
            attrVal = cleanFilter.substr(eqPos + 1);
        }

        for (const auto& [dn, obj] : m_objects) {
            std::string dnLower = toLower(dn);
            if (!baseLower.empty() && dnLower.find(baseLower) == std::string::npos) {
                continue;
            }

            if (!subtreeScope && baseLower != dnLower) {
                // One level check: count commas relative to base
                size_t baseCommas = std::count(baseLower.begin(), baseLower.end(), ',');
                size_t objCommas = std::count(dnLower.begin(), dnLower.end(), ',');
                if (objCommas != baseCommas + 1) continue;
            }

            if (attrName.empty() || attrVal == "*") {
                results.push_back(obj);
                continue;
            }

            bool match = false;
            if (attrName == "objectclass") {
                match = (toLower(ObjectClassToString(obj.objectClass)) == toLower(attrVal));
            } else if (attrName == "samaccountname") {
                match = matchWildcard(toLower(obj.samAccountName), toLower(attrVal));
            } else if (attrName == "serviceprincipalname") {
                for (const auto& spn : obj.servicePrincipalNames) {
                    if (matchWildcard(toLower(spn), toLower(attrVal))) { match = true; break; }
                }
            } else if (attrName == "distinguishedname") {
                match = matchWildcard(toLower(obj.distinguishedName), toLower(attrVal));
            } else {
                auto aIt = obj.attributes.find(attrName);
                if (aIt != obj.attributes.end()) {
                    match = matchWildcard(toLower(aIt->second), toLower(attrVal));
                }
            }

            if (match) results.push_back(obj);
        }

        return results;
    }

    // --- Kerberos Authentication Service (AS-REQ / AS-REP) ---
    bool authenticateAsReq(
        const std::string& clientPrincipal,
        const std::string& realm,
        KerberosTicket& outTgt)
    {
        std::unique_lock lock(m_mutex);
        m_asReqCount->fetch_add(1);

        uint64_t nowUs = getCurrentTimeUs();

        // 1. Look up client principal
        std::string sam = clientPrincipal;
        size_t atPos = sam.find('@');
        if (atPos != std::string::npos) sam = sam.substr(0, atPos);

        auto it = m_samIndex.find(toLower(sam));
        if (it == m_samIndex.end()) {
            m_authFailCount->fetch_add(1);
            return false;
        }

        const auto& clientObj = m_objects[it->second];
        if (clientObj.userAccountControl & UF_ACCOUNTDISABLE) {
            m_authFailCount->fetch_add(1);
            return false;
        }

        // 2. Synthesize PAC (Privilege Attribute Certificate)
        PacLogonInfo pac{};
        pac.userSid = clientObj.objectSid;
        pac.accountName = clientObj.samAccountName;
        pac.domainName = DEFAULT_DOMAIN_NETBIOS;
        pac.domainSid = "S-1-5-21-3948572019-2039482910-1029384756";
        pac.primaryGroupSid = "S-1-5-21-3948572019-2039482910-1029384756-513"; // Domain Users
        pac.userAccountControl = clientObj.userAccountControl;

        for (const auto& gSid : clientObj.memberOfSids) {
            PacGroupMembership gm{};
            gm.groupSid = gSid;
            gm.attributes = 0x7;
            pac.groups.push_back(gm);
        }
        pac.serverSignatureValid = true;
        pac.kdcSignatureValid = true;

        // 3. Generate TGT
        outTgt.ticketId = "TKT-AS-" + std::to_string(m_tickets.size() + 1);
        outTgt.ticketType = KerberosTicketType::TicketGrantingTicket;
        outTgt.clientPrincipal = clientPrincipal;
        outTgt.servicePrincipal = std::string(DEFAULT_KRBTGT_NAME) + "/" + realm;
        outTgt.realm = realm;
        outTgt.sessionKey = "A7F9C012B4E856D3" + std::to_string(nowUs);
        outTgt.encType = EncryptionType::AES256_CTS_HMAC_SHA1_96;
        outTgt.flags = TKT_FLG_FORWARDABLE | TKT_FLG_PROXIABLE | TKT_FLG_RENEWABLE | TKT_FLG_INITIAL | TKT_FLG_PRE_AUTHENT;
        outTgt.issueTimeUs = nowUs;
        outTgt.startTimeUs = nowUs;
        outTgt.endTimeUs = nowUs + (TICKET_LIFETIME_HOURS * 3600ULL * 1000000ULL);
        outTgt.renewTillUs = nowUs + (TICKET_RENEW_DAYS * 86400ULL * 1000000ULL);
        outTgt.clientAddress = "127.0.0.1";
        outTgt.pac = pac;
        outTgt.isRevoked = false;

        m_tickets[outTgt.ticketId] = outTgt;
        m_authSuccessCount->fetch_add(1);
        return true;
    }

    // --- Kerberos Ticket Granting Service (TGS-REQ / TGS-REP) ---
    bool grantServiceTicketTgsReq(
        const std::string& tgtTicketId,
        const std::string& targetSpn,
        KerberosTicket& outTgs)
    {
        std::unique_lock lock(m_mutex);
        m_tgsReqCount->fetch_add(1);

        uint64_t nowUs = getCurrentTimeUs();

        // 1. Verify TGT
        auto itTgt = m_tickets.find(tgtTicketId);
        if (itTgt == m_tickets.end()) return false;
        const auto& tgt = itTgt->second;

        if (tgt.ticketType != KerberosTicketType::TicketGrantingTicket || tgt.isRevoked || tgt.isExpired(nowUs)) {
            return false;
        }

        // 2. Resolve Service Principal in NTDS
        std::string spnLower = toLower(targetSpn);
        auto itSpn = m_spnIndex.find(spnLower);

        // Check cross-realm domain trust referral if not local
        if (itSpn == m_spnIndex.end()) {
            size_t slashPos = targetSpn.find('/');
            if (slashPos != std::string::npos) {
                std::string hostPart = targetSpn.substr(slashPos + 1);
                for (const auto& [domain, trust] : m_trusts) {
                    if (toLower(hostPart).find(toLower(domain)) != std::string::npos &&
                        trust.direction != TrustDirection::Disabled)
                    {
                        // Return Cross-Realm Referral Ticket
                        outTgs.ticketId = "TKT-REF-" + std::to_string(m_tickets.size() + 1);
                        outTgs.ticketType = KerberosTicketType::ServiceTicket;
                        outTgs.clientPrincipal = tgt.clientPrincipal;
                        outTgs.servicePrincipal = "krbtgt/" + trust.partnerDomain;
                        outTgs.realm = trust.partnerDomain;
                        outTgs.sessionKey = "REF-KEY-SEC-" + std::to_string(nowUs);
                        outTgs.encType = EncryptionType::AES256_CTS_HMAC_SHA1_96;
                        outTgs.flags = TKT_FLG_FORWARDABLE | TKT_FLG_TRANSIT_POLICY;
                        outTgs.issueTimeUs = nowUs;
                        outTgs.startTimeUs = nowUs;
                        outTgs.endTimeUs = tgt.endTimeUs;
                        outTgs.renewTillUs = tgt.renewTillUs;
                        outTgs.pac = tgt.pac;
                        m_tickets[outTgs.ticketId] = outTgs;
                        return true;
                    }
                }
            }
            return false;
        }

        // 3. Issue Target Service Ticket
        outTgs.ticketId = "TKT-TGS-" + std::to_string(m_tickets.size() + 1);
        outTgs.ticketType = KerberosTicketType::ServiceTicket;
        outTgs.clientPrincipal = tgt.clientPrincipal;
        outTgs.servicePrincipal = targetSpn;
        outTgs.realm = tgt.realm;
        outTgs.sessionKey = "SESS-SVC-" + std::to_string(nowUs);
        outTgs.encType = EncryptionType::AES256_CTS_HMAC_SHA1_96;
        outTgs.flags = TKT_FLG_FORWARDABLE | TKT_FLG_PROXIABLE;
        outTgs.issueTimeUs = nowUs;
        outTgs.startTimeUs = nowUs;
        outTgs.endTimeUs = tgt.endTimeUs;
        outTgs.renewTillUs = tgt.renewTillUs;
        outTgs.clientAddress = tgt.clientAddress;
        outTgs.pac = tgt.pac;
        outTgs.isRevoked = false;

        m_tickets[outTgs.ticketId] = outTgs;
        return true;
    }

    // --- Service Ticket Verification (AP-REQ / Application Server) ---
    bool verifyServiceTicket(
        const std::string& ticketId,
        const std::string& expectedSpn,
        PacLogonInfo* outPac = nullptr) const
    {
        std::shared_lock lock(m_mutex);
        uint64_t nowUs = getCurrentTimeUs();

        auto it = m_tickets.find(ticketId);
        if (it == m_tickets.end()) return false;

        const auto& tkt = it->second;
        if (tkt.ticketType != KerberosTicketType::ServiceTicket || tkt.isRevoked || tkt.isExpired(nowUs)) {
            return false;
        }

        if (!expectedSpn.empty() && toLower(tkt.servicePrincipal) != toLower(expectedSpn)) {
            return false;
        }

        if (!tkt.pac.serverSignatureValid || !tkt.pac.kdcSignatureValid) {
            return false;
        }

        if (outPac) *outPac = tkt.pac;
        return true;
    }

    // --- Domain Trust Management ---
    bool addTrust(const DomainTrust& trust) {
        std::unique_lock lock(m_mutex);
        m_trusts[trust.partnerDomain] = trust;
        return true;
    }

    const DomainTrust* getTrust(const std::string& partnerDomain) const {
        std::shared_lock lock(m_mutex);
        auto it = m_trusts.find(partnerDomain);
        return (it != m_trusts.end()) ? &it->second : nullptr;
    }

    std::vector<DomainTrust> getAllTrusts() const {
        std::shared_lock lock(m_mutex);
        std::vector<DomainTrust> list;
        list.reserve(m_trusts.size());
        for (const auto& [_, t] : m_trusts) list.push_back(t);
        return list;
    }

    // --- Statistics ---
    uint64_t getAsReqCount() const noexcept { return m_asReqCount->load(); }
    uint64_t getTgsReqCount() const noexcept { return m_tgsReqCount->load(); }
    uint64_t getLdapQueryCount() const noexcept { return m_ldapQueryCount->load(); }
    uint64_t getAuthSuccessCount() const noexcept { return m_authSuccessCount->load(); }
    uint64_t getAuthFailCount() const noexcept { return m_authFailCount->load(); }
    size_t getActiveTicketCount() const noexcept {
        std::shared_lock lock(m_mutex);
        return m_tickets.size();
    }

private:
    ActiveDirectorySubsystem() = default;

    void initializeLocked() {
        if (m_initialized) return;

        uint64_t now = getCurrentTimeUs();

        // 1. Root Domain Object
        DirectoryObject rootDns{};
        rootDns.distinguishedName = DEFAULT_DOMAIN_DN;
        rootDns.samAccountName = DEFAULT_DOMAIN_NETBIOS;
        rootDns.objectClass = ObjectClass::DomainDNS;
        rootDns.objectSid = "S-1-5-21-3948572019-2039482910-1029384756";
        rootDns.createdTimeUs = now;
        rootDns.modifiedTimeUs = now;
        m_objects[rootDns.distinguishedName] = rootDns;

        // 2. Pre-seed Builtin Containers
        DirectoryObject ouUsers{};
        ouUsers.distinguishedName = "CN=Users," + std::string(DEFAULT_DOMAIN_DN);
        ouUsers.samAccountName = "Users";
        ouUsers.objectClass = ObjectClass::Container;
        m_objects[ouUsers.distinguishedName] = ouUsers;

        DirectoryObject ouComputers{};
        ouComputers.distinguishedName = "CN=Computers," + std::string(DEFAULT_DOMAIN_DN);
        ouComputers.samAccountName = "Computers";
        ouComputers.objectClass = ObjectClass::Container;
        m_objects[ouComputers.distinguishedName] = ouComputers;

        DirectoryObject ouDomainControllers{};
        ouDomainControllers.distinguishedName = "OU=Domain Controllers," + std::string(DEFAULT_DOMAIN_DN);
        ouDomainControllers.samAccountName = "Domain Controllers";
        ouDomainControllers.objectClass = ObjectClass::OrganizationalUnit;
        m_objects[ouDomainControllers.distinguishedName] = ouDomainControllers;

        // 3. Pre-seed Security Groups
        DirectoryObject grpDomainAdmins{};
        grpDomainAdmins.distinguishedName = "CN=Domain Admins,CN=Users," + std::string(DEFAULT_DOMAIN_DN);
        grpDomainAdmins.samAccountName = "Domain Admins";
        grpDomainAdmins.objectClass = ObjectClass::Group;
        grpDomainAdmins.objectSid = "S-1-5-21-3948572019-2039482910-1029384756-512";
        m_objects[grpDomainAdmins.distinguishedName] = grpDomainAdmins;
        m_samIndex[toLower(grpDomainAdmins.samAccountName)] = grpDomainAdmins.distinguishedName;

        DirectoryObject grpDomainUsers{};
        grpDomainUsers.distinguishedName = "CN=Domain Users,CN=Users," + std::string(DEFAULT_DOMAIN_DN);
        grpDomainUsers.samAccountName = "Domain Users";
        grpDomainUsers.objectClass = ObjectClass::Group;
        grpDomainUsers.objectSid = "S-1-5-21-3948572019-2039482910-1029384756-513";
        m_objects[grpDomainUsers.distinguishedName] = grpDomainUsers;
        m_samIndex[toLower(grpDomainUsers.samAccountName)] = grpDomainUsers.distinguishedName;

        // 4. Pre-seed krbtgt Account (KDC Key)
        DirectoryObject krbtgt{};
        krbtgt.distinguishedName = "CN=krbtgt,CN=Users," + std::string(DEFAULT_DOMAIN_DN);
        krbtgt.samAccountName = DEFAULT_KRBTGT_NAME;
        krbtgt.objectClass = ObjectClass::User;
        krbtgt.objectSid = "S-1-5-21-3948572019-2039482910-1029384756-502";
        krbtgt.userAccountControl = UF_NORMAL_ACCOUNT | UF_ACCOUNTDISABLE;
        krbtgt.servicePrincipalNames = { "kadmin/changepw" };
        krbtgt.createdTimeUs = now;
        m_objects[krbtgt.distinguishedName] = krbtgt;
        m_samIndex[toLower(krbtgt.samAccountName)] = krbtgt.distinguishedName;

        // 5. Pre-seed Administrator Account
        DirectoryObject admin{};
        admin.distinguishedName = "CN=Administrator,CN=Users," + std::string(DEFAULT_DOMAIN_DN);
        admin.samAccountName = "Administrator";
        admin.objectClass = ObjectClass::User;
        admin.objectSid = "S-1-5-21-3948572019-2039482910-1029384756-500";
        admin.userPrincipalName = "Administrator@" + std::string(DEFAULT_DOMAIN_DNS);
        admin.userAccountControl = UF_NORMAL_ACCOUNT | UF_DONT_EXPIRE_PASSWD;
        admin.memberOfSids = { "S-1-5-21-3948572019-2039482910-1029384756-512" };
        admin.createdTimeUs = now;
        m_objects[admin.distinguishedName] = admin;
        m_samIndex[toLower(admin.samAccountName)] = admin.distinguishedName;

        // 6. Pre-seed Primary Domain Controller (TITAN-DC01$)
        DirectoryObject dc01{};
        dc01.distinguishedName = "CN=TITAN-DC01,OU=Domain Controllers," + std::string(DEFAULT_DOMAIN_DN);
        dc01.samAccountName = "TITAN-DC01$";
        dc01.objectClass = ObjectClass::Computer;
        dc01.objectSid = "S-1-5-21-3948572019-2039482910-1029384756-1001";
        dc01.userAccountControl = UF_SERVER_TRUST | UF_DONT_EXPIRE_PASSWD;
        dc01.servicePrincipalNames = {
            "HOST/TITAN-DC01",
            "HOST/titan-dc01.micant.internal",
            "cifs/titan-dc01.micant.internal",
            "ldap/titan-dc01.micant.internal",
            "GC/titan-dc01.micant.internal/micant.internal"
        };
        dc01.createdTimeUs = now;
        m_objects[dc01.distinguishedName] = dc01;
        m_samIndex[toLower(dc01.samAccountName)] = dc01.distinguishedName;
        for (const auto& spn : dc01.servicePrincipalNames) {
            m_spnIndex[toLower(spn)] = dc01.distinguishedName;
        }

        // 7. Pre-seed Member Application Server (TITAN-APP01$)
        DirectoryObject app01{};
        app01.distinguishedName = "CN=TITAN-APP01,CN=Computers," + std::string(DEFAULT_DOMAIN_DN);
        app01.samAccountName = "TITAN-APP01$";
        app01.objectClass = ObjectClass::Computer;
        app01.objectSid = "S-1-5-21-3948572019-2039482910-1029384756-1002";
        app01.userAccountControl = UF_WORKSTATION_TRUST | UF_DONT_EXPIRE_PASSWD;
        app01.servicePrincipalNames = {
            "HOST/TITAN-APP01",
            "HOST/titan-app01.micant.internal",
            "http/titan-app01.micant.internal",
            "cifs/titan-app01.micant.internal"
        };
        app01.createdTimeUs = now;
        m_objects[app01.distinguishedName] = app01;
        m_samIndex[toLower(app01.samAccountName)] = app01.distinguishedName;
        for (const auto& spn : app01.servicePrincipalNames) {
            m_spnIndex[toLower(spn)] = app01.distinguishedName;
        }

        // 8. Pre-seed Forest Domain Trust (PARTNER.CORP)
        DomainTrust forestTrust{};
        forestTrust.partnerDomain = "partner.corp";
        forestTrust.netbiosName = "PARTNER";
        forestTrust.direction = TrustDirection::Bidirectional;
        forestTrust.type = TrustType::Forest;
        forestTrust.isTransitive = true;
        forestTrust.trustSecretKeyHex = "9F8E7D6C5B4A30211029384756ABCDEF";
        forestTrust.establishedTimeUs = now;
        m_trusts[forestTrust.partnerDomain] = forestTrust;

        m_initialized = true;
    }

    static std::string toLower(const std::string& str) {
        std::string s = str;
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    static bool matchWildcard(const std::string& str, const std::string& pattern) {
        if (pattern == "*") return true;
        if (pattern.back() == '*') {
            std::string prefix = pattern.substr(0, pattern.size() - 1);
            return (str.rfind(prefix, 0) == 0);
        }
        return str == pattern;
    }

    static uint64_t getCurrentTimeUs() {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
    }

    mutable std::shared_mutex m_mutex;
    bool m_initialized{false};
    std::unordered_map<std::string, DirectoryObject> m_objects;  // DN -> Object
    std::unordered_map<std::string, std::string> m_samIndex;     // SAM (lower) -> DN
    std::unordered_map<std::string, std::string> m_spnIndex;     // SPN (lower) -> DN
    std::unordered_map<std::string, KerberosTicket> m_tickets;   // TicketID -> Ticket
    std::unordered_map<std::string, DomainTrust> m_trusts;       // Domain -> Trust

    std::shared_ptr<std::atomic<uint64_t>> m_asReqCount = std::make_shared<std::atomic<uint64_t>>(0);
    std::shared_ptr<std::atomic<uint64_t>> m_tgsReqCount = std::make_shared<std::atomic<uint64_t>>(0);
    std::shared_ptr<std::atomic<uint64_t>> m_ldapQueryCount = std::make_shared<std::atomic<uint64_t>>(0);
    std::shared_ptr<std::atomic<uint64_t>> m_authSuccessCount = std::make_shared<std::atomic<uint64_t>>(0);
    std::shared_ptr<std::atomic<uint64_t>> m_authFailCount = std::make_shared<std::atomic<uint64_t>>(0);
};

// ============================================================================
// 7. Win32 & NT Clean-Room C ABI Exports (kdcsvc.dll, kdc.sys, wldap32.dll)
// ============================================================================
extern "C" {

inline NTSTATUS KdcAuthenticateClient(
    const char* clientPrincipal,
    const char* realm,
    void** phTgt,
    char* outTicketId,
    size_t outTicketIdLen)
{
    if (!clientPrincipal || !realm) return micant::STATUS_INVALID_PARAMETER;
    auto& sys = ActiveDirectorySubsystem::get();
    sys.initialize();

    KerberosTicket tgt{};
    bool ok = sys.authenticateAsReq(clientPrincipal, realm, tgt);
    if (!ok) return micant::STATUS_LOGON_FAILURE;

    if (phTgt) *phTgt = reinterpret_cast<void*>(0xDEADAD01);
    if (outTicketId && outTicketIdLen > 0) {
        std::strncpy(outTicketId, tgt.ticketId.c_str(), outTicketIdLen - 1);
        outTicketId[outTicketIdLen - 1] = '\0';
    }
    return micant::STATUS_SUCCESS;
}

inline NTSTATUS KdcGrantServiceTicket(
    const char* tgtTicketId,
    const char* targetSpn,
    void** phTgs,
    char* outTicketId,
    size_t outTicketIdLen)
{
    if (!tgtTicketId || !targetSpn) return micant::STATUS_INVALID_PARAMETER;
    auto& sys = ActiveDirectorySubsystem::get();
    sys.initialize();

    KerberosTicket tgs{};
    bool ok = sys.grantServiceTicketTgsReq(tgtTicketId, targetSpn, tgs);
    if (!ok) return micant::STATUS_ACCESS_DENIED;

    if (phTgs) *phTgs = reinterpret_cast<void*>(0xDEADAD02);
    if (outTicketId && outTicketIdLen > 0) {
        std::strncpy(outTicketId, tgs.ticketId.c_str(), outTicketIdLen - 1);
        outTicketId[outTicketIdLen - 1] = '\0';
    }
    return micant::STATUS_SUCCESS;
}

inline NTSTATUS KdcVerifyServiceTicket(
    const char* serviceTicketId,
    const char* expectedSpn,
    bool* pValid)
{
    if (!serviceTicketId || !pValid) return micant::STATUS_INVALID_PARAMETER;
    auto& sys = ActiveDirectorySubsystem::get();
    sys.initialize();

    *pValid = sys.verifyServiceTicket(serviceTicketId, expectedSpn ? expectedSpn : "");
    return *pValid ? micant::STATUS_SUCCESS : micant::STATUS_LOGON_FAILURE;
}

inline NTSTATUS NtdsCreatePrincipal(
    const char* dn,
    const char* samName,
    uint32_t objectClass,
    const char* upn,
    void** phObj)
{
    if (!dn || !samName) return micant::STATUS_INVALID_PARAMETER;
    auto& sys = ActiveDirectorySubsystem::get();
    sys.initialize();

    DirectoryObject obj{};
    obj.distinguishedName = dn;
    obj.samAccountName = samName;
    obj.objectClass = static_cast<ObjectClass>(objectClass);
    obj.objectSid = "S-1-5-21-3948572019-2039482910-1029384756-" + std::to_string(sys.getObjectCount() + 1000);
    if (upn) obj.userPrincipalName = upn;
    obj.userAccountControl = UF_NORMAL_ACCOUNT | UF_DONT_EXPIRE_PASSWD;

    bool ok = sys.addObject(obj);
    if (!ok) return micant::STATUS_OBJECT_NAME_COLLISION;

    if (phObj) *phObj = reinterpret_cast<void*>(0xDEADAD03);
    return micant::STATUS_SUCCESS;
}

inline NTSTATUS NtdsQueryObject(
    const char* samName,
    char* outDnBuf,
    size_t outDnBufLen)
{
    if (!samName || !outDnBuf || outDnBufLen == 0) return micant::STATUS_INVALID_PARAMETER;
    auto& sys = ActiveDirectorySubsystem::get();
    sys.initialize();

    const auto* obj = sys.getObjectBySam(samName);
    if (!obj) return micant::STATUS_NOT_FOUND;

    std::strncpy(outDnBuf, obj->distinguishedName.c_str(), outDnBufLen - 1);
    outDnBuf[outDnBufLen - 1] = '\0';
    return micant::STATUS_SUCCESS;
}

inline NTSTATUS LdapSearchDirectory(
    const char* baseDn,
    const char* filter,
    uint32_t* pMatchCount)
{
    if (!filter || !pMatchCount) return micant::STATUS_INVALID_PARAMETER;
    auto& sys = ActiveDirectorySubsystem::get();
    sys.initialize();

    auto res = sys.searchLdap(baseDn ? baseDn : "", filter);
    *pMatchCount = static_cast<uint32_t>(res.size());
    return micant::STATUS_SUCCESS;
}

} // extern "C"

// ============================================================================
// 8. SCM Driver & VersionDatabase Registration Helper
// ============================================================================
inline void RegisterActiveDirectorySubsystem() {
    // 1. Version Database Registration
    auto& vdb = micant::version::VersionDatabase::Instance();
    vdb.RegisterModule("kdcsvc.dll",   "10.0.26100.1", "Kerberos Key Distribution Center Service");
    vdb.RegisterModule("kdc.sys",      "10.0.26100.1", "Kerberos KDC Kernel Dispatcher");
    vdb.RegisterModule("ntds.dit",     "10.0.26100.1", "Active Directory Database Engine");
    vdb.RegisterModule("wldap32.dll",  "10.0.26100.1", "Win32 LDAP Client API Subsystem");
    vdb.RegisterModule("netlogon.dll", "10.0.26100.1", "Netlogon Trust Authentication Service");

    // 2. SCM Registration
    auto& scm = micant::scm::ServiceControlManager::get();
    auto kdcRec = std::make_shared<micant::scm::ServiceRecord>();
    kdcRec->serviceName = L"Kdc";
    kdcRec->displayName = L"Kerberos Key Distribution Center";
    kdcRec->serviceType = micant::scm::SERVICE_WIN32_SHARE_PROCESS;
    kdcRec->startType = micant::scm::SERVICE_AUTO_START;
    kdcRec->binaryPath = L"C:\\Windows\\System32\\kdcsvc.dll";
    kdcRec->status.dwCurrentState = micant::scm::SERVICE_RUNNING;
    scm.registerServiceRecord(kdcRec);

    auto ntdsRec = std::make_shared<micant::scm::ServiceRecord>();
    ntdsRec->serviceName = L"NTDS";
    ntdsRec->displayName = L"Active Directory Domain Services";
    ntdsRec->serviceType = micant::scm::SERVICE_WIN32_OWN_PROCESS;
    ntdsRec->startType = micant::scm::SERVICE_AUTO_START;
    ntdsRec->binaryPath = L"C:\\Windows\\System32\\ntdsai.dll";
    ntdsRec->status.dwCurrentState = micant::scm::SERVICE_RUNNING;
    scm.registerServiceRecord(ntdsRec);

    auto netlogonRec = std::make_shared<micant::scm::ServiceRecord>();
    netlogonRec->serviceName = L"Netlogon";
    netlogonRec->displayName = L"Netlogon Trust Service";
    netlogonRec->serviceType = micant::scm::SERVICE_WIN32_SHARE_PROCESS;
    netlogonRec->startType = micant::scm::SERVICE_AUTO_START;
    netlogonRec->binaryPath = L"C:\\Windows\\System32\\lsass.exe";
    netlogonRec->status.dwCurrentState = micant::scm::SERVICE_RUNNING;
    scm.registerServiceRecord(netlogonRec);
}

} // namespace micant::activedirectory
