#ifndef MICANT_CERTSRV_HPP
#define MICANT_CERTSRV_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <cstring>
#include <random>

#include "micant/scm.hpp"
#include "micant/version.hpp"

namespace micant::certsrv {

// ============================================================================
// Enums & Protocol Constants
// ============================================================================

enum class CaType {
    EnterpriseRootCA,
    EnterpriseSubordinateCA,
    StandaloneRootCA,
    StandaloneSubordinateCA
};

enum class CaState {
    Stopped,
    Running,
    Suspended
};

// Certificate Request Dispositions (matches Windows certcli.h CR_DISP_*)
enum class RequestDisposition : uint32_t {
    CR_DISP_INCOMPLETE          = 0,
    CR_DISP_ERROR               = 1,
    CR_DISP_DENIED              = 2,
    CR_DISP_ISSUED              = 3,
    CR_DISP_ISSUED_OUT_OF_BAND  = 4,
    CR_DISP_UNDER_SUBMISSION    = 5,
    CR_DISP_REVOKED             = 6
};

// Revocation Reasons (RFC 5280 Section 5.3.1 / Windows CRL_REASON_*)
constexpr uint32_t CRL_REASON_UNSPECIFIED             = 0;
constexpr uint32_t CRL_REASON_KEY_COMPROMISE          = 1;
constexpr uint32_t CRL_REASON_CA_COMPROMISE           = 2;
constexpr uint32_t CRL_REASON_AFFILIATION_CHANGED     = 3;
constexpr uint32_t CRL_REASON_SUPERSEDED              = 4;
constexpr uint32_t CRL_REASON_CESSATION_OF_OPERATION  = 5;
constexpr uint32_t CRL_REASON_CERTIFICATE_HOLD        = 6;
constexpr uint32_t CRL_REASON_REMOVE_FROM_CRL         = 8;

// OCSP Response Status (RFC 6960)
enum class OcspStatus : uint32_t {
    OCSP_STATUS_GOOD    = 0,
    OCSP_STATUS_REVOKED = 1,
    OCSP_STATUS_UNKNOWN = 2
};

// Standard X.509 Key Usages (bitmask)
constexpr uint32_t KU_DIGITAL_SIGNATURE = 0x0001;
constexpr uint32_t KU_NON_REPUDIATION   = 0x0002;
constexpr uint32_t KU_KEY_ENCIPHERMENT  = 0x0004;
constexpr uint32_t KU_DATA_ENCIPHERMENT = 0x0008;
constexpr uint32_t KU_KEY_AGREEMENT     = 0x0010;
constexpr uint32_t KU_KEY_CERT_SIGN     = 0x0020;
constexpr uint32_t KU_CRL_SIGN          = 0x0040;
constexpr uint32_t KU_ENCIPHER_ONLY     = 0x0080;
constexpr uint32_t KU_DECIPHER_ONLY     = 0x0100;

// Standard Extended Key Usage (EKU) OIDs
inline const char* const OID_SERVER_AUTH        = "1.3.6.1.5.5.7.3.1";
inline const char* const OID_CLIENT_AUTH        = "1.3.6.1.5.5.7.3.2";
inline const char* const OID_CODE_SIGNING       = "1.3.6.1.5.5.7.3.3";
inline const char* const OID_EMAIL_PROTECTION   = "1.3.6.1.5.5.7.3.4";
inline const char* const OID_SMARTCARD_LOGON    = "1.3.6.1.4.1.311.20.2.2";
inline const char* const OID_TIMESTAMPING       = "1.3.6.1.5.5.7.3.8";
inline const char* const OID_IPSEC_TUNNEL       = "1.3.6.1.5.5.7.3.6";
inline const char* const OID_IPSEC_USER         = "1.3.6.1.5.5.7.3.7";
inline const char* const OID_KDC_AUTH           = "1.3.6.1.5.2.3.5";
inline const char* const OID_ENROLLMENT_AGENT   = "1.3.6.1.4.1.311.20.2.1";

inline std::string DispositionToString(RequestDisposition d) {
    switch (d) {
        case RequestDisposition::CR_DISP_INCOMPLETE:         return "Incomplete";
        case RequestDisposition::CR_DISP_ERROR:              return "Error";
        case RequestDisposition::CR_DISP_DENIED:             return "Denied";
        case RequestDisposition::CR_DISP_ISSUED:             return "Issued";
        case RequestDisposition::CR_DISP_ISSUED_OUT_OF_BAND: return "Issued Out-of-Band";
        case RequestDisposition::CR_DISP_UNDER_SUBMISSION:   return "Pending Approval";
        case RequestDisposition::CR_DISP_REVOKED:            return "Revoked";
        default: return "Unknown";
    }
}

inline std::string RevocationReasonToString(uint32_t reason) {
    switch (reason) {
        case CRL_REASON_UNSPECIFIED:            return "Unspecified";
        case CRL_REASON_KEY_COMPROMISE:         return "Key Compromise";
        case CRL_REASON_CA_COMPROMISE:          return "CA Compromise";
        case CRL_REASON_AFFILIATION_CHANGED:    return "Affiliation Changed";
        case CRL_REASON_SUPERSEDED:             return "Superseded";
        case CRL_REASON_CESSATION_OF_OPERATION: return "Cessation of Operation";
        case CRL_REASON_CERTIFICATE_HOLD:       return "Certificate Hold";
        case CRL_REASON_REMOVE_FROM_CRL:        return "Remove from CRL";
        default: return "Unknown Reason (" + std::to_string(reason) + ")";
    }
}

inline std::string OcspStatusToString(OcspStatus s) {
    switch (s) {
        case OcspStatus::OCSP_STATUS_GOOD:    return "Good";
        case OcspStatus::OCSP_STATUS_REVOKED: return "Revoked";
        case OcspStatus::OCSP_STATUS_UNKNOWN: return "Unknown";
        default: return "Unknown Status";
    }
}

// ============================================================================
// Clean-Room SHA-1 Digest Engine (for AKI / SKI Key Identifiers)
// ============================================================================
inline std::string ComputeSha1Hex(const std::string& input) {
    // SHA-1 implementation compliant with FIPS PUB 180-1
    uint32_t h0 = 0x67452301;
    uint32_t h1 = 0xEFCDAB89;
    uint32_t h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476;
    uint32_t h4 = 0xC3D2E1F0;

    std::vector<uint8_t> msg(input.begin(), input.end());
    uint64_t bitLen = msg.size() * 8;

    msg.push_back(0x80);
    while ((msg.size() % 64) != 56) {
        msg.push_back(0x00);
    }

    for (int i = 7; i >= 0; --i) {
        msg.push_back(static_cast<uint8_t>((bitLen >> (i * 8)) & 0xFF));
    }

    auto leftRotate = [](uint32_t val, uint32_t bits) -> uint32_t {
        return (val << bits) | (val >> (32 - bits));
    };

    for (size_t chunk = 0; chunk < msg.size(); chunk += 64) {
        uint32_t w[80]{};
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<uint32_t>(msg[chunk + i * 4]) << 24) |
                   (static_cast<uint32_t>(msg[chunk + i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(msg[chunk + i * 4 + 2]) << 8) |
                   (static_cast<uint32_t>(msg[chunk + i * 4 + 3]));
        }
        for (int i = 16; i < 80; ++i) {
            w[i] = leftRotate(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }

        uint32_t a = h0;
        uint32_t b = h1;
        uint32_t c = h2;
        uint32_t d = h3;
        uint32_t e = h4;

        for (int i = 0; i < 80; ++i) {
            uint32_t f = 0;
            uint32_t k = 0;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }

            uint32_t temp = leftRotate(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = leftRotate(b, 30);
            b = a;
            a = temp;
        }

        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }

    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    ss << std::setw(8) << h0 << std::setw(8) << h1 << std::setw(8) << h2 << std::setw(8) << h3 << std::setw(8) << h4;
    return ss.str();
}

// ============================================================================
// Certificate Template Record
// ============================================================================
struct CertificateTemplate {
    std::string              templateName;
    std::string              displayName;
    uint32_t                 schemaVersion{2};
    uint64_t                 validityPeriodSeconds{31536000}; // Default: 1 Year (365 days)
    uint64_t                 renewalPeriodSeconds{5184000};   // Default: 60 Days
    uint32_t                 keyUsageMask{0};
    std::vector<std::string> ekus;
    bool                     isCa{false};
    int32_t                  pathLengthConstraint{-1}; // -1 = Unlimited
    bool                     autoEnrollAllowed{true};
    bool                     requiresApproval{false};  // true = manual administrative approval
    uint32_t                 minKeySizeBits{2048};
};

// ============================================================================
// Issued / Stored Certificate Record
// ============================================================================
struct CertificateRecord {
    std::string              serialNumber;
    std::string              subjectDn;
    std::string              issuerDn;
    uint64_t                 notBefore{0};
    uint64_t                 notAfter{0};
    std::string              templateName;
    std::string              publicKeyAlg{"RSA"};
    uint32_t                 publicKeyBits{2048};
    std::string              subjectKeyIdentifier;
    std::string              authorityKeyIdentifier;
    std::vector<std::string> sanDnsNames;
    std::vector<std::string> sanIpAddresses;
    std::string              sanUpn;
    uint32_t                 keyUsage{0};
    std::vector<std::string> ekus;
    bool                     isRevoked{false};
    uint64_t                 revocationDate{0};
    uint32_t                 revocationReason{0};
    std::vector<uint8_t>     rawDer;
    std::string              rawPem;
    bool                     isCa{false};
};

// ============================================================================
// Certificate Request Record (CSR Tracker)
// ============================================================================
struct CertificateRequestRecord {
    uint32_t                 requestId{0};
    std::string              subjectDn;
    std::string              templateName;
    std::string              requesterAccount;
    uint64_t                 submissionTime{0};
    RequestDisposition       disposition{RequestDisposition::CR_DISP_INCOMPLETE};
    std::string              statusMessage;
    std::string              issuedSerialNumber;
    std::vector<std::string> sanList;
    uint32_t                 keySizeBits{2048};
};

// ============================================================================
// Certificate Revocation List (CRL) Entry & Record (RFC 5280)
// ============================================================================
struct RevokedCertEntry {
    std::string serialNumber;
    uint64_t    revocationDate{0};
    uint32_t    revocationReason{0};
};

struct CrlRecord {
    std::string                   issuerDn;
    uint64_t                      thisUpdate{0};
    uint64_t                      nextUpdate{0};
    uint32_t                      crlNumber{1};
    std::vector<RevokedCertEntry> revokedEntries;
    std::string                   cdpUri;
    std::vector<uint8_t>          rawCrlDer;
};

// ============================================================================
// Online Certificate Status Protocol (OCSP) Response (RFC 6960)
// ============================================================================
struct OcspResponse {
    uint32_t             responseStatus{0}; // 0 = successful
    OcspStatus           certStatus{OcspStatus::OCSP_STATUS_GOOD};
    std::string          serialNumber;
    uint64_t             producedAt{0};
    uint64_t             thisUpdate{0};
    uint64_t             nextUpdate{0};
    uint64_t             revocationTime{0};
    uint32_t             revocationReason{0};
    std::string          responderId;
    std::vector<uint8_t> signature;
};

// ============================================================================
// CertificateServicesEngine: Core Singleton Engine
// ============================================================================
class CertificateServicesEngine {
private:
    mutable std::mutex m_mutex;
    bool m_initialized{false};

    // CA Properties
    std::string m_caName{"Titan Enterprise Root CA"};
    std::string m_caDn{"CN=Titan Enterprise Root CA,DC=titan,DC=local"};
    CaType      m_caType{CaType::EnterpriseRootCA};
    CaState     m_caState{CaState::Running};
    std::string m_aiaUri{"http://pki.titan.local/certdata/TitanRootCA.crt"};
    std::string m_cdpUri{"http://pki.titan.local/certdata/TitanRootCA.crl"};
    std::string m_ocspUri{"http://ocsp.titan.local/ocsp"};
    std::string m_rootSerialNumber{"01"};

    // Catalogs & Databases
    std::map<std::string, CertificateTemplate>      m_templates;
    std::map<uint32_t, CertificateRequestRecord>    m_requests;
    std::map<std::string, CertificateRecord>        m_certificates; // Keyed by serialNumber
    CrlRecord                                       m_latestCrl;

    // Generators
    uint32_t m_nextRequestId{1001};
    uint64_t m_nextSerialNumber{0x1000A0B0C0D00001ULL};

    // Telemetry Statistics
    mutable std::atomic<uint64_t> m_totalRequestsSubmitted{0};
    mutable std::atomic<uint64_t> m_totalCertificatesIssued{0};
    mutable std::atomic<uint64_t> m_totalCertificatesRevoked{0};
    mutable std::atomic<uint64_t> m_totalCrlPublished{0};
    mutable std::atomic<uint64_t> m_totalOcspQueries{0};

public:
    static CertificateServicesEngine& instance() {
        static CertificateServicesEngine inst;
        return inst;
    }

    CertificateServicesEngine() {
        initialize();
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_initialized) return;

        seedDefaultTemplates();
        seedRootCaCertificate();
        publishCrlInternal();

        registerScmServices();
        registerVersionDatabase();

        m_initialized = true;
    }

    void registerScmServices() {
        auto& scm = micant::scm::ServiceControlManager::get();

        // CertSvc (Active Directory Certificate Services)
        auto recCertSvc = std::make_shared<micant::scm::ServiceRecord>();
        recCertSvc->serviceName = L"CertSvc";
        recCertSvc->displayName = L"Active Directory Certificate Services";
        recCertSvc->serviceType = micant::scm::SERVICE_WIN32_OWN_PROCESS;
        recCertSvc->startType = micant::scm::SERVICE_AUTO_START;
        recCertSvc->errorControl = micant::scm::SERVICE_ERROR_NORMAL;
        recCertSvc->binaryPath = L"C:\\Windows\\System32\\certsrv.exe";
        recCertSvc->status.dwServiceType = micant::scm::SERVICE_WIN32_OWN_PROCESS;
        recCertSvc->status.dwCurrentState = micant::scm::SERVICE_RUNNING;
        recCertSvc->status.dwControlsAccepted = micant::scm::SERVICE_ACCEPT_STOP | micant::scm::SERVICE_ACCEPT_SHUTDOWN | micant::scm::SERVICE_ACCEPT_PAUSE_CONTINUE;
        scm.registerServiceRecord(recCertSvc);
    }

    void registerVersionDatabase() {
        auto& db = micant::version::VersionDatabase::Instance();
        db.RegisterModule("certsrv.exe", "10.0.26100.1", "Active Directory Certificate Services Engine");
        db.RegisterModule("certcli.dll", "10.0.26100.1", "Certificate Services Client Interface");
        db.RegisterModule("certenroll.dll", "10.0.26100.1", "Certificate Enrollment API");
        db.RegisterModule("certadm.dll", "10.0.26100.1", "Certificate Services Administration Engine");
    }

    void seedDefaultTemplates() {
        // 1. DomainController
        CertificateTemplate tDc;
        tDc.templateName = "DomainController";
        tDc.displayName = "Domain Controller Authentication";
        tDc.schemaVersion = 2;
        tDc.validityPeriodSeconds = 31536000; // 1 Year
        tDc.renewalPeriodSeconds = 5184000;   // 60 Days
        tDc.keyUsageMask = KU_DIGITAL_SIGNATURE | KU_KEY_ENCIPHERMENT;
        tDc.ekus = { OID_SERVER_AUTH, OID_CLIENT_AUTH, OID_SMARTCARD_LOGON, OID_KDC_AUTH };
        tDc.autoEnrollAllowed = true;
        tDc.requiresApproval = false;
        tDc.minKeySizeBits = 2048;
        m_templates[tDc.templateName] = tDc;

        // 2. Computer / Machine
        CertificateTemplate tComp;
        tComp.templateName = "Computer";
        tComp.displayName = "Client and Server Computer Authentication";
        tComp.schemaVersion = 2;
        tComp.validityPeriodSeconds = 31536000;
        tComp.renewalPeriodSeconds = 5184000;
        tComp.keyUsageMask = KU_DIGITAL_SIGNATURE | KU_KEY_ENCIPHERMENT;
        tComp.ekus = { OID_SERVER_AUTH, OID_CLIENT_AUTH };
        tComp.autoEnrollAllowed = true;
        tComp.requiresApproval = false;
        tComp.minKeySizeBits = 2048;
        m_templates[tComp.templateName] = tComp;

        // 3. User
        CertificateTemplate tUser;
        tUser.templateName = "User";
        tUser.displayName = "Enterprise Domain User Authentication";
        tUser.schemaVersion = 2;
        tUser.validityPeriodSeconds = 31536000;
        tUser.renewalPeriodSeconds = 5184000;
        tUser.keyUsageMask = KU_DIGITAL_SIGNATURE | KU_KEY_ENCIPHERMENT;
        tUser.ekus = { OID_CLIENT_AUTH, OID_EMAIL_PROTECTION, "1.3.6.1.4.1.311.10.3.4" /* EFS */ };
        tUser.autoEnrollAllowed = true;
        tUser.requiresApproval = false;
        tUser.minKeySizeBits = 2048;
        m_templates[tUser.templateName] = tUser;

        // 4. WebServer
        CertificateTemplate tWeb;
        tWeb.templateName = "WebServer";
        tWeb.displayName = "TLS Web Server Authentication";
        tWeb.schemaVersion = 2;
        tWeb.validityPeriodSeconds = 63072000; // 2 Years
        tWeb.renewalPeriodSeconds = 7776000;   // 90 Days
        tWeb.keyUsageMask = KU_DIGITAL_SIGNATURE | KU_KEY_ENCIPHERMENT;
        tWeb.ekus = { OID_SERVER_AUTH };
        tWeb.autoEnrollAllowed = false;
        tWeb.requiresApproval = false;
        tWeb.minKeySizeBits = 2048;
        m_templates[tWeb.templateName] = tWeb;

        // 5. CodeSigning
        CertificateTemplate tCode;
        tCode.templateName = "CodeSigning";
        tCode.displayName = "Authenticode Digital Code Signing";
        tCode.schemaVersion = 2;
        tCode.validityPeriodSeconds = 94608000; // 3 Years
        tCode.renewalPeriodSeconds = 7776000;
        tCode.keyUsageMask = KU_DIGITAL_SIGNATURE;
        tCode.ekus = { OID_CODE_SIGNING };
        tCode.autoEnrollAllowed = false;
        tCode.requiresApproval = true; // Requires manual CA manager approval
        tCode.minKeySizeBits = 3072;
        m_templates[tCode.templateName] = tCode;

        // 6. SubCA
        CertificateTemplate tSubCa;
        tSubCa.templateName = "SubCA";
        tSubCa.displayName = "Subordinate Certification Authority";
        tSubCa.schemaVersion = 2;
        tSubCa.validityPeriodSeconds = 157680000; // 5 Years
        tSubCa.renewalPeriodSeconds = 15768000;
        tSubCa.keyUsageMask = KU_DIGITAL_SIGNATURE | KU_KEY_CERT_SIGN | KU_CRL_SIGN;
        tSubCa.isCa = true;
        tSubCa.pathLengthConstraint = 0;
        tSubCa.autoEnrollAllowed = false;
        tSubCa.requiresApproval = true;
        tSubCa.minKeySizeBits = 4096;
        m_templates[tSubCa.templateName] = tSubCa;

        // 7. SmartcardLogon
        CertificateTemplate tSc;
        tSc.templateName = "SmartcardLogon";
        tSc.displayName = "Smart Card User Interactive Logon";
        tSc.schemaVersion = 2;
        tSc.validityPeriodSeconds = 31536000;
        tSc.renewalPeriodSeconds = 5184000;
        tSc.keyUsageMask = KU_DIGITAL_SIGNATURE | KU_KEY_ENCIPHERMENT;
        tSc.ekus = { OID_CLIENT_AUTH, OID_SMARTCARD_LOGON };
        tSc.autoEnrollAllowed = true;
        tSc.requiresApproval = false;
        tSc.minKeySizeBits = 2048;
        m_templates[tSc.templateName] = tSc;

        // 8. EnrollmentAgent
        CertificateTemplate tEa;
        tEa.templateName = "EnrollmentAgent";
        tEa.displayName = "Certificate Request Enrollment Agent";
        tEa.schemaVersion = 2;
        tEa.validityPeriodSeconds = 63072000;
        tEa.renewalPeriodSeconds = 7776000;
        tEa.keyUsageMask = KU_DIGITAL_SIGNATURE;
        tEa.ekus = { OID_ENROLLMENT_AGENT };
        tEa.autoEnrollAllowed = false;
        tEa.requiresApproval = true;
        tEa.minKeySizeBits = 2048;
        m_templates[tEa.templateName] = tEa;
    }

    void seedRootCaCertificate() {
        uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

        CertificateRecord rootCert;
        rootCert.serialNumber = m_rootSerialNumber;
        rootCert.subjectDn = m_caDn;
        rootCert.issuerDn = m_caDn; // Self-signed
        rootCert.notBefore = now - 3600; // 1 hour ago
        rootCert.notAfter = now + 630720000; // 20 Years
        rootCert.templateName = "RootCA";
        rootCert.publicKeyAlg = "RSA";
        rootCert.publicKeyBits = 4096;
        rootCert.subjectKeyIdentifier = ComputeSha1Hex(m_caDn + "_PUBKEY_ROOT");
        rootCert.authorityKeyIdentifier = rootCert.subjectKeyIdentifier;
        rootCert.keyUsage = KU_DIGITAL_SIGNATURE | KU_KEY_CERT_SIGN | KU_CRL_SIGN;
        rootCert.isCa = true;

        // Synthetic DER structure
        rootCert.rawDer = buildSyntheticDer(rootCert);
        rootCert.rawPem = buildPemString(rootCert.rawDer);

        m_certificates[rootCert.serialNumber] = rootCert;
    }

    std::vector<uint8_t> buildSyntheticDer(const CertificateRecord& cert) {
        // Generates an ASN.1 DER-tagged synthetic byte buffer
        std::vector<uint8_t> der;
        der.push_back(0x30); // SEQUENCE
        der.push_back(0x82); // Long form 2-byte length placeholder
        der.push_back(0x01);
        der.push_back(0x80);

        // TBSCertificate SEQUENCE
        der.push_back(0x30);
        der.push_back(0x82);
        der.push_back(0x01);
        der.push_back(0x20);

        // Version: [0] EXPLICIT INTEGER v3 (2)
        der.push_back(0xA0); der.push_back(0x03);
        der.push_back(0x02); der.push_back(0x01); der.push_back(0x02);

        // Serial Number
        der.push_back(0x02);
        der.push_back(static_cast<uint8_t>(cert.serialNumber.size()));
        for (char c : cert.serialNumber) der.push_back(static_cast<uint8_t>(c));

        // Signature Algorithm: sha256WithRSAEncryption
        der.push_back(0x30); der.push_back(0x0D);
        der.push_back(0x06); der.push_back(0x09);
        // OID 1.2.840.113549.1.1.11
        const uint8_t sha256RsaOid[] = {0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x0B};
        der.insert(der.end(), sha256RsaOid, sha256RsaOid + sizeof(sha256RsaOid));
        der.push_back(0x05); der.push_back(0x00); // NULL

        // Issuer & Subject payload markers
        for (char c : cert.issuerDn) der.push_back(static_cast<uint8_t>(c));
        for (char c : cert.subjectDn) der.push_back(static_cast<uint8_t>(c));

        // Pad to 384 bytes
        while (der.size() < 384) {
            der.push_back(0xAA);
        }
        return der;
    }

    std::string buildPemString(const std::vector<uint8_t>& der) {
        // Base64 encoding table
        static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string b64Str;
        size_t i = 0;
        while (i < der.size()) {
            uint32_t oct1 = (i < der.size()) ? der[i++] : 0;
            uint32_t oct2 = (i < der.size()) ? der[i++] : 0;
            uint32_t oct3 = (i < der.size()) ? der[i++] : 0;
            uint32_t triple = (oct1 << 16) | (oct2 << 8) | oct3;

            b64Str.push_back(b64[(triple >> 18) & 0x3F]);
            b64Str.push_back(b64[(triple >> 12) & 0x3F]);
            b64Str.push_back(b64[(triple >> 6) & 0x3F]);
            b64Str.push_back(b64[triple & 0x3F]);
        }

        std::string pem = "-----BEGIN CERTIFICATE-----\n";
        for (size_t p = 0; p < b64Str.size(); p += 64) {
            pem += b64Str.substr(p, 64) + "\n";
        }
        pem += "-----END CERTIFICATE-----\n";
        return pem;
    }

    std::string generateNextSerialNumber() {
        std::ostringstream ss;
        ss << std::hex << std::uppercase << std::setfill('0') << std::setw(16) << m_nextSerialNumber++;
        return ss.str();
    }

    // ========================================================================
    // Template Management
    // ========================================================================
    bool getTemplate(const std::string& name, CertificateTemplate& out) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_templates.find(name);
        if (it == m_templates.end()) return false;
        out = it->second;
        return true;
    }

    std::vector<CertificateTemplate> getAllTemplates() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<CertificateTemplate> result;
        result.reserve(m_templates.size());
        for (const auto& [_, t] : m_templates) {
            result.push_back(t);
        }
        return result;
    }

    bool addTemplate(const CertificateTemplate& tmpl) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_templates[tmpl.templateName] = tmpl;
        return true;
    }

    // ========================================================================
    // Certificate Request & Issuance Engine
    // ========================================================================
    bool submitRequest(const std::string& subjectDn,
                       const std::string& templateName,
                       const std::string& requester,
                       const std::vector<std::string>& sans,
                       uint32_t keyBits,
                       uint32_t* pOutRequestId,
                       RequestDisposition* pOutDisposition) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_totalRequestsSubmitted.fetch_add(1, std::memory_order_relaxed);

        auto it = m_templates.find(templateName);
        if (it == m_templates.end()) {
            if (pOutDisposition) *pOutDisposition = RequestDisposition::CR_DISP_ERROR;
            return false;
        }

        const auto& tmpl = it->second;
        if (keyBits < tmpl.minKeySizeBits) {
            if (pOutDisposition) *pOutDisposition = RequestDisposition::CR_DISP_DENIED;
            return false;
        }

        uint32_t reqId = m_nextRequestId++;
        uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

        CertificateRequestRecord req;
        req.requestId = reqId;
        req.subjectDn = subjectDn;
        req.templateName = templateName;
        req.requesterAccount = requester.empty() ? "TITAN\\SYSTEM" : requester;
        req.submissionTime = now;
        req.sanList = sans;
        req.keySizeBits = keyBits;

        if (tmpl.requiresApproval) {
            req.disposition = RequestDisposition::CR_DISP_UNDER_SUBMISSION;
            req.statusMessage = "Queued for administrative manager approval";
            m_requests[reqId] = req;

            if (pOutRequestId) *pOutRequestId = reqId;
            if (pOutDisposition) *pOutDisposition = req.disposition;
            return true;
        }

        // Auto-approve & Issue
        req.disposition = RequestDisposition::CR_DISP_ISSUED;
        req.statusMessage = "Certificate issued successfully via automated policy";

        CertificateRecord cert;
        cert.serialNumber = generateNextSerialNumber();
        cert.subjectDn = subjectDn;
        cert.issuerDn = m_caDn;
        cert.notBefore = now - 60; // 1 min buffer
        cert.notAfter = now + tmpl.validityPeriodSeconds;
        cert.templateName = templateName;
        cert.publicKeyAlg = "RSA";
        cert.publicKeyBits = keyBits;
        cert.subjectKeyIdentifier = ComputeSha1Hex(subjectDn + "_KEY_" + cert.serialNumber);
        cert.authorityKeyIdentifier = m_certificates[m_rootSerialNumber].subjectKeyIdentifier;
        cert.sanDnsNames = sans;
        cert.keyUsage = tmpl.keyUsageMask;
        cert.ekus = tmpl.ekus;
        cert.isCa = tmpl.isCa;
        cert.rawDer = buildSyntheticDer(cert);
        cert.rawPem = buildPemString(cert.rawDer);

        m_certificates[cert.serialNumber] = cert;
        req.issuedSerialNumber = cert.serialNumber;
        m_requests[reqId] = req;

        m_totalCertificatesIssued.fetch_add(1, std::memory_order_relaxed);

        if (pOutRequestId) *pOutRequestId = reqId;
        if (pOutDisposition) *pOutDisposition = req.disposition;
        return true;
    }

    bool approveRequest(uint32_t requestId, std::string* pOutSerial) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_requests.find(requestId);
        if (it == m_requests.end()) return false;
        if (it->second.disposition != RequestDisposition::CR_DISP_UNDER_SUBMISSION) return false;

        auto& req = it->second;
        auto tIt = m_templates.find(req.templateName);
        if (tIt == m_templates.end()) return false;
        const auto& tmpl = tIt->second;

        uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

        CertificateRecord cert;
        cert.serialNumber = generateNextSerialNumber();
        cert.subjectDn = req.subjectDn;
        cert.issuerDn = m_caDn;
        cert.notBefore = now - 60;
        cert.notAfter = now + tmpl.validityPeriodSeconds;
        cert.templateName = req.templateName;
        cert.publicKeyAlg = "RSA";
        cert.publicKeyBits = req.keySizeBits;
        cert.subjectKeyIdentifier = ComputeSha1Hex(req.subjectDn + "_KEY_" + cert.serialNumber);
        cert.authorityKeyIdentifier = m_certificates[m_rootSerialNumber].subjectKeyIdentifier;
        cert.sanDnsNames = req.sanList;
        cert.keyUsage = tmpl.keyUsageMask;
        cert.ekus = tmpl.ekus;
        cert.isCa = tmpl.isCa;
        cert.rawDer = buildSyntheticDer(cert);
        cert.rawPem = buildPemString(cert.rawDer);

        m_certificates[cert.serialNumber] = cert;
        req.disposition = RequestDisposition::CR_DISP_ISSUED;
        req.statusMessage = "Approved by CA Administrator";
        req.issuedSerialNumber = cert.serialNumber;

        m_totalCertificatesIssued.fetch_add(1, std::memory_order_relaxed);

        if (pOutSerial) *pOutSerial = cert.serialNumber;
        return true;
    }

    bool denyRequest(uint32_t requestId, const std::string& reason) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_requests.find(requestId);
        if (it == m_requests.end()) return false;
        if (it->second.disposition != RequestDisposition::CR_DISP_UNDER_SUBMISSION) return false;

        it->second.disposition = RequestDisposition::CR_DISP_DENIED;
        it->second.statusMessage = reason.empty() ? "Denied by CA Administrator" : reason;
        return true;
    }

    bool getRequest(uint32_t requestId, CertificateRequestRecord& out) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_requests.find(requestId);
        if (it == m_requests.end()) return false;
        out = it->second;
        return true;
    }

    std::vector<CertificateRequestRecord> getAllRequests() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<CertificateRequestRecord> result;
        result.reserve(m_requests.size());
        for (const auto& [_, r] : m_requests) {
            result.push_back(r);
        }
        return result;
    }

    bool getCertificate(const std::string& serial, CertificateRecord& out) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_certificates.find(serial);
        if (it == m_certificates.end()) return false;
        out = it->second;
        return true;
    }

    std::vector<CertificateRecord> getAllCertificates() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<CertificateRecord> result;
        result.reserve(m_certificates.size());
        for (const auto& [_, c] : m_certificates) {
            result.push_back(c);
        }
        return result;
    }

    // ========================================================================
    // Revocation & CRL Management (RFC 5280)
    // ========================================================================
    bool revokeCertificate(const std::string& serial, uint32_t reason) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_certificates.find(serial);
        if (it == m_certificates.end()) return false;
        if (it->second.isRevoked) return false; // Already revoked

        uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

        it->second.isRevoked = true;
        it->second.revocationDate = now;
        it->second.revocationReason = reason;

        m_totalCertificatesRevoked.fetch_add(1, std::memory_order_relaxed);

        publishCrlInternal();
        return true;
    }

    void publishCrlInternal() {
        uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

        m_latestCrl.issuerDn = m_caDn;
        m_latestCrl.thisUpdate = now;
        m_latestCrl.nextUpdate = now + 604800; // 7 Days
        m_latestCrl.crlNumber++;
        m_latestCrl.cdpUri = m_cdpUri;

        m_latestCrl.revokedEntries.clear();
        for (const auto& [_, cert] : m_certificates) {
            if (cert.isRevoked) {
                RevokedCertEntry entry;
                entry.serialNumber = cert.serialNumber;
                entry.revocationDate = cert.revocationDate;
                entry.revocationReason = cert.revocationReason;
                m_latestCrl.revokedEntries.push_back(entry);
            }
        }

        // Build synthetic raw CRL DER
        std::vector<uint8_t> crlDer;
        crlDer.push_back(0x30); // SEQUENCE
        crlDer.push_back(0x82);
        crlDer.push_back(0x01);
        crlDer.push_back(0x00);
        for (char c : m_caDn) crlDer.push_back(static_cast<uint8_t>(c));
        crlDer.push_back(static_cast<uint8_t>(m_latestCrl.crlNumber & 0xFF));
        while (crlDer.size() < 256) crlDer.push_back(0x55);
        m_latestCrl.rawCrlDer = crlDer;

        m_totalCrlPublished.fetch_add(1, std::memory_order_relaxed);
    }

    bool publishCrl() {
        std::lock_guard<std::mutex> lock(m_mutex);
        publishCrlInternal();
        return true;
    }

    CrlRecord getLatestCrl() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_latestCrl;
    }

    // ========================================================================
    // Online Certificate Status Protocol (OCSP / RFC 6960)
    // ========================================================================
    bool processOcspRequest(const std::string& serial, OcspResponse& outResponse) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_totalOcspQueries.fetch_add(1, std::memory_order_relaxed);

        uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

        outResponse.responseStatus = 0; // Successful
        outResponse.serialNumber = serial;
        outResponse.producedAt = now;
        outResponse.thisUpdate = now;
        outResponse.nextUpdate = now + 86400; // 24 Hours
        outResponse.responderId = "CN=Titan OCSP Responder,DC=titan,DC=local";

        auto it = m_certificates.find(serial);
        if (it == m_certificates.end()) {
            outResponse.certStatus = OcspStatus::OCSP_STATUS_UNKNOWN;
            return true;
        }

        if (it->second.isRevoked) {
            outResponse.certStatus = OcspStatus::OCSP_STATUS_REVOKED;
            outResponse.revocationTime = it->second.revocationDate;
            outResponse.revocationReason = it->second.revocationReason;
        } else {
            outResponse.certStatus = OcspStatus::OCSP_STATUS_GOOD;
        }

        // Synthetic signature
        outResponse.signature.assign(256, 0x4F); // 'O'
        return true;
    }

    // ========================================================================
    // Certificate Chain & Trust Path Validation
    // ========================================================================
    bool verifyCertificateChain(const std::string& leafSerial, bool* pValid, std::vector<std::string>* pChainDns) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (pValid) *pValid = false;

        auto it = m_certificates.find(leafSerial);
        if (it == m_certificates.end()) return false;

        uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

        std::string currentSerial = leafSerial;
        std::vector<std::string> chain;

        while (true) {
            auto certIt = m_certificates.find(currentSerial);
            if (certIt == m_certificates.end()) return false;

            const auto& cert = certIt->second;
            chain.push_back(cert.subjectDn);

            // Validity check
            if (now < cert.notBefore || now > cert.notAfter) {
                if (pValid) *pValid = false;
                if (pChainDns) *pChainDns = chain;
                return true;
            }

            // Revocation check
            if (cert.isRevoked) {
                if (pValid) *pValid = false;
                if (pChainDns) *pChainDns = chain;
                return true;
            }

            // Check if Root CA
            if (cert.subjectDn == m_caDn && cert.issuerDn == m_caDn) {
                // Reached trusted root anchor
                if (pValid) *pValid = true;
                if (pChainDns) *pChainDns = chain;
                return true;
            }

            // Find issuer certificate
            bool foundIssuer = false;
            for (const auto& [_, candidate] : m_certificates) {
                if (candidate.subjectDn == cert.issuerDn) {
                    currentSerial = candidate.serialNumber;
                    foundIssuer = true;
                    break;
                }
            }
            if (!foundIssuer) {
                if (pValid) *pValid = false;
                if (pChainDns) *pChainDns = chain;
                return true;
            }
        }
    }

    // ========================================================================
    // Getters & Telemetry
    // ========================================================================
    uint64_t getTotalRequestsSubmitted() const { return m_totalRequestsSubmitted.load(std::memory_order_relaxed); }
    uint64_t getTotalCertificatesIssued() const { return m_totalCertificatesIssued.load(std::memory_order_relaxed); }
    uint64_t getTotalCertificatesRevoked() const { return m_totalCertificatesRevoked.load(std::memory_order_relaxed); }
    uint64_t getTotalCrlPublished() const { return m_totalCrlPublished.load(std::memory_order_relaxed); }
    uint64_t getTotalOcspQueries() const { return m_totalOcspQueries.load(std::memory_order_relaxed); }

    CaState getCaState() const { std::lock_guard<std::mutex> lk(m_mutex); return m_caState; }
    CaType getCaType() const { std::lock_guard<std::mutex> lk(m_mutex); return m_caType; }
    std::string getCaName() const { std::lock_guard<std::mutex> lk(m_mutex); return m_caName; }
    std::string getCaDn() const { std::lock_guard<std::mutex> lk(m_mutex); return m_caDn; }
    std::string getAiaUri() const { std::lock_guard<std::mutex> lk(m_mutex); return m_aiaUri; }
    std::string getCdpUri() const { std::lock_guard<std::mutex> lk(m_mutex); return m_cdpUri; }
    std::string getOcspUri() const { std::lock_guard<std::mutex> lk(m_mutex); return m_ocspUri; }
    std::string getRootSerialNumber() const { std::lock_guard<std::mutex> lk(m_mutex); return m_rootSerialNumber; }
};

// ============================================================================
// Clean-Room Win32 C ABI Exports (certsrv.exe / certcli.dll Parity)
// ============================================================================
extern "C" {

inline int32_t MicaCertSrvInitialize(void** ppEngine) {
    if (!ppEngine) return 0;
    auto& eng = CertificateServicesEngine::instance();
    eng.initialize();
    *ppEngine = &eng;
    return 1;
}

inline int32_t MicaCertSrvSubmitRequest(void* pEngine,
                                       const char* szSubject,
                                       const char* szTemplate,
                                       const char* szRequester,
                                       uint32_t* pRequestId,
                                       uint32_t* pDisposition) {
    if (!pEngine || !szSubject || !szTemplate) return 0;
    auto* eng = static_cast<CertificateServicesEngine*>(pEngine);
    RequestDisposition disp = RequestDisposition::CR_DISP_INCOMPLETE;
    bool ok = eng->submitRequest(szSubject, szTemplate, szRequester ? szRequester : "", {}, 2048, pRequestId, &disp);
    if (pDisposition) *pDisposition = static_cast<uint32_t>(disp);
    return ok ? 1 : 0;
}

inline int32_t MicaCertSrvRetrieveCertificate(void* pEngine,
                                             uint32_t requestId,
                                             char* szSerialOut,
                                             uint32_t cchSerial,
                                             uint8_t* pDerOut,
                                             uint32_t cbDerMax,
                                             uint32_t* pcbDerActual) {
    if (!pEngine) return 0;
    auto* eng = static_cast<CertificateServicesEngine*>(pEngine);
    CertificateRequestRecord req;
    if (!eng->getRequest(requestId, req)) return 0;
    if (req.disposition != RequestDisposition::CR_DISP_ISSUED) return 0;

    CertificateRecord cert;
    if (!eng->getCertificate(req.issuedSerialNumber, cert)) return 0;

    if (szSerialOut && cchSerial > cert.serialNumber.size()) {
        std::memcpy(szSerialOut, cert.serialNumber.c_str(), cert.serialNumber.size() + 1);
    }
    if (pcbDerActual) *pcbDerActual = static_cast<uint32_t>(cert.rawDer.size());
    if (pDerOut && cbDerMax >= cert.rawDer.size()) {
        std::memcpy(pDerOut, cert.rawDer.data(), cert.rawDer.size());
    }
    return 1;
}

inline int32_t MicaCertSrvRevokeCertificate(void* pEngine, const char* szSerial, uint32_t dwReason) {
    if (!pEngine || !szSerial) return 0;
    auto* eng = static_cast<CertificateServicesEngine*>(pEngine);
    return eng->revokeCertificate(szSerial, dwReason) ? 1 : 0;
}

inline int32_t MicaCertSrvGetCrl(void* pEngine, uint32_t* pCrlNumber, uint32_t* pRevokedCount) {
    if (!pEngine) return 0;
    auto* eng = static_cast<CertificateServicesEngine*>(pEngine);
    auto crl = eng->getLatestCrl();
    if (pCrlNumber) *pCrlNumber = crl.crlNumber;
    if (pRevokedCount) *pRevokedCount = static_cast<uint32_t>(crl.revokedEntries.size());
    return 1;
}

inline int32_t MicaCertSrvOcspCheck(void* pEngine, const char* szSerial, uint32_t* pOcspStatus) {
    if (!pEngine || !szSerial || !pOcspStatus) return 0;
    auto* eng = static_cast<CertificateServicesEngine*>(pEngine);
    OcspResponse resp;
    if (!eng->processOcspRequest(szSerial, resp)) return 0;
    *pOcspStatus = static_cast<uint32_t>(resp.certStatus);
    return 1;
}

inline int32_t MicaCertSrvShutdown(void* pEngine) {
    if (!pEngine) return 0;
    return 1;
}

} // extern "C"

} // namespace micant::certsrv

#endif // MICANT_CERTSRV_HPP
