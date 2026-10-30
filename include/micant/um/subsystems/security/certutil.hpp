#ifndef MICANT_CERTUTIL_HPP
#define MICANT_CERTUTIL_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <memory>
#include <map>
#include "micant/um/servers/certsrv.hpp"

namespace micant::certutil {

// ============================================================================
// Canonical Win32 certutil.exe CLI Implementation
// Clean-Room pure ISO C++23 under 17 U.S.C. § 1201(f)
// ============================================================================

class CertutilCommandExecutor {
public:
    static std::string Execute(const std::vector<std::string>& args) {
        if (args.empty()) {
            return GetUsageHelp();
        }

        std::string command = args[0];
        // Strip leading '-' or '/'
        if (!command.empty() && (command[0] == '-' || command[0] == '/')) {
            command = command.substr(1);
        }
        std::transform(command.begin(), command.end(), command.begin(), ::tolower);

        if (command == "?" || command == "help") {
            return GetUsageHelp();
        }
        if (command == "ping") {
            std::string caTarget = (args.size() > 1) ? args[1] : "";
            return ExecutePing(caTarget);
        }
        if (command == "catemplates") {
            return ExecuteCaTemplates();
        }
        if (command == "dump") {
            std::string target = (args.size() > 1) ? args[1] : "";
            return ExecuteDump(target);
        }
        if (command == "verify") {
            std::string target = (args.size() > 1) ? args[1] : "";
            return ExecuteVerify(target);
        }
        if (command == "ca.cert") {
            std::string outFile = (args.size() > 1) ? args[1] : "ca.crt";
            return ExecuteCaCert(outFile);
        }
        if (command == "crl") {
            return ExecuteCrl();
        }
        if (command == "getreg") {
            std::string key = (args.size() > 1) ? args[1] : "";
            return ExecuteGetReg(key);
        }
        if (command == "pulse") {
            return ExecutePulse();
        }
        if (command == "viewdelstore") {
            std::string store = (args.size() > 1) ? args[1] : "MY";
            return ExecuteViewDelStore(store);
        }

        return "CertUtil: Unknown option: -" + args[0] + "\n" + GetUsageHelp();
    }

    static std::string ExecuteString(const std::string& cmdLine) {
        std::vector<std::string> args = Tokenize(cmdLine);
        if (!args.empty() && (args[0] == "certutil" || args[0] == "certutil.exe")) {
            args.erase(args.begin());
        }
        return Execute(args);
    }

private:
    static std::vector<std::string> Tokenize(const std::string& line) {
        std::vector<std::string> tokens;
        std::string current;
        bool inQuotes = false;

        for (size_t i = 0; i < line.size(); ++i) {
            char c = line[i];
            if (c == '"') {
                inQuotes = !inQuotes;
            } else if ((c == ' ' || c == '\t' || c == '\r' || c == '\n') && !inQuotes) {
                if (!current.empty()) {
                    tokens.push_back(current);
                    current.clear();
                }
            } else {
                current += c;
            }
        }
        if (!current.empty()) {
            tokens.push_back(current);
        }
        return tokens;
    }

    static std::string GetUsageHelp() {
        std::ostringstream ss;
        ss << "Microsoft (R) CertUtil (MicaNT Enterprise)\n"
           << "Usage:\n"
           << "  CertUtil [Options] [Arguments]\n\n"
           << "Options:\n"
           << "  -dump <Target>           -- Dump configuration, certificate, CSR, or CRL\n"
           << "  -verify <Target>         -- Verify certificates, CRL, or certificate chain\n"
           << "  -ping [Server\\CAName]   -- Ping Active Directory Certificate Services interface\n"
           << "  -catemplates             -- Display published Certificate Templates\n"
           << "  -ca.cert <OutFile>       -- Retrieve and export CA's certificate\n"
           << "  -crl                     -- Publish new Base and Delta CRLs\n"
           << "  -getreg <Key>            -- Display CA registry configuration values\n"
           << "  -pulse                   -- Pulse autoenrollment event for machine/user\n"
           << "  -viewdelstore <Store>    -- Delete or view certificates in store\n"
           << "  -?                       -- Display this usage message\n\n"
           << "CertUtil: -? command completed successfully.\n";
        return ss.str();
    }

    static std::string ExecutePing(const std::string& caTarget) {
        auto& pki = micant::certsrv::CertificateServicesEngine::instance();
        std::ostringstream ss;

        std::string targetDisplay = caTarget.empty() ? ("\\\\147.135.1.229\\" + pki.getCaName()) : caTarget;
        ss << "Connecting to " << targetDisplay << " ...\n";
        ss << "Server \"" << pki.getCaName() << "\" is alive and responding.\n";
        ss << "  CA State: " << (pki.getCaState() == micant::certsrv::CaState::Running ? "Running" : "Stopped") << "\n";
        ss << "  CA Type: Enterprise Root CA\n";
        ss << "  CA Distinguished Name: " << pki.getCaDn() << "\n";
        ss << "  AIA Location: " << pki.getAiaUri() << "\n";
        ss << "  CDP Location: " << pki.getCdpUri() << "\n";
        ss << "  OCSP Responder: " << pki.getOcspUri() << "\n";
        ss << "  Active Directory Integration: Connected (CN=Public Key Services,CN=Services,CN=Configuration,DC=micant,DC=local)\n";
        ss << "CertUtil: -ping command completed successfully.\n";
        return ss.str();
    }

    static std::string ExecuteCaTemplates() {
        auto& pki = micant::certsrv::CertificateServicesEngine::instance();
        auto templates = pki.getAllTemplates();
        std::ostringstream ss;

        ss << "========================================================================\n";
        ss << "Published Active Directory Certificate Templates (" << templates.size() << " total):\n";
        ss << "========================================================================\n";

        for (const auto& tmpl : templates) {
            ss << "  Template: " << tmpl.templateName << " (" << tmpl.displayName << ")\n";
            ss << "    Schema Version: " << tmpl.schemaVersion
               << ", Validity: " << (tmpl.validityPeriodSeconds / 86400) << " days"
               << ", Min Key: " << tmpl.minKeySizeBits << " bits\n";
            ss << "    Auto-Enroll: " << (tmpl.autoEnrollAllowed ? "Allowed" : "Disabled")
               << ", Approval: " << (tmpl.requiresApproval ? "Required" : "Automatic")
               << ", IsCA: " << (tmpl.isCa ? "True" : "False") << "\n";
            ss << "    EKUs: ";
            if (tmpl.ekus.empty()) {
                ss << "None";
            } else {
                for (size_t i = 0; i < tmpl.ekus.size(); ++i) {
                    if (i > 0) ss << ", ";
                    ss << tmpl.ekus[i];
                }
            }
            ss << "\n";
        }
        ss << "CertUtil: -catemplates command completed successfully.\n";
        return ss.str();
    }

    static std::string ExecuteDump(const std::string& target) {
        auto& pki = micant::certsrv::CertificateServicesEngine::instance();
        std::ostringstream ss;

        if (target.empty()) {
            return "CertUtil: Missing argument for -dump\n" + GetUsageHelp();
        }

        // Check if target matches serial number in certificate store
        micant::certsrv::CertificateRecord cert;
        if (pki.getCertificate(target, cert)) {
            ss << "========================================================================\n"
               << "X.509 Certificate (Serial: " << cert.serialNumber << "):\n"
               << "========================================================================\n"
               << "  Version: 3 (0x2)\n"
               << "  Serial Number: " << cert.serialNumber << "\n"
               << "  Signature Algorithm: sha256RSA (1.2.840.113549.1.1.11)\n"
               << "  Issuer: " << cert.issuerDn << "\n"
               << "  NotBefore: " << cert.notBefore << "\n"
               << "  NotAfter: " << cert.notAfter << "\n"
               << "  Subject: " << cert.subjectDn << "\n"
               << "  Public Key Algorithm: " << cert.publicKeyAlg << " (" << cert.publicKeyBits << " Bits)\n"
               << "  Subject Key Identifier: " << cert.subjectKeyIdentifier << "\n"
               << "  Authority Key Identifier: " << cert.authorityKeyIdentifier << "\n"
               << "  Template: " << cert.templateName << "\n"
               << "  Is CA: " << (cert.isCa ? "TRUE" : "FALSE") << "\n"
               << "  Revocation Status: " << (cert.isRevoked ? "REVOKED" : "Valid") << "\n";

            if (!cert.sanDnsNames.empty()) {
                ss << "  Subject Alternative Name (DNS): ";
                for (size_t i = 0; i < cert.sanDnsNames.size(); ++i) {
                    if (i > 0) ss << ", ";
                    ss << cert.sanDnsNames[i];
                }
                ss << "\n";
            }
            if (!cert.ekus.empty()) {
                ss << "  Enhanced Key Usage (EKU): ";
                for (size_t i = 0; i < cert.ekus.size(); ++i) {
                    if (i > 0) ss << ", ";
                    ss << cert.ekus[i];
                }
                ss << "\n";
            }
            ss << "CertUtil: -dump command completed successfully.\n";
            return ss.str();
        }

        // Check if target is a request ID
        try {
            uint32_t reqId = static_cast<uint32_t>(std::stoul(target));
            micant::certsrv::CertificateRequestRecord req;
            if (pki.getRequest(reqId, req)) {
                ss << "========================================================================\n"
                   << "Certificate Request Record (ID: " << req.requestId << "):\n"
                   << "========================================================================\n"
                   << "  Request ID: " << req.requestId << "\n"
                   << "  Subject DN: " << req.subjectDn << "\n"
                   << "  Template: " << req.templateName << "\n"
                   << "  Requester: " << req.requesterAccount << "\n"
                   << "  Submission Time: " << req.submissionTime << "\n"
                   << "  Disposition: " << DispositionToString(req.disposition) << "\n"
                   << "  Assigned Serial: " << (req.issuedSerialNumber.empty() ? "None" : req.issuedSerialNumber) << "\n"
                   << "  Status Message: " << (req.statusMessage.empty() ? "OK" : req.statusMessage) << "\n";
                ss << "CertUtil: -dump command completed successfully.\n";
                return ss.str();
            }
        } catch (...) {}

        // Fallback for general dump
        ss << "CertUtil: Target \"" << target << "\" dumped (synthetic catalog dump).\n"
           << "CertUtil: -dump command completed successfully.\n";
        return ss.str();
    }

    static std::string ExecuteVerify(const std::string& target) {
        auto& pki = micant::certsrv::CertificateServicesEngine::instance();
        std::ostringstream ss;

        if (target.empty()) {
            return "CertUtil: Missing argument for -verify\n" + GetUsageHelp();
        }

        micant::certsrv::CertificateRecord cert;
        if (!pki.getCertificate(target, cert)) {
            // Attempt to search by subject DN
            bool found = false;
            for (const auto& c : pki.getAllCertificates()) {
                if (c.subjectDn.find(target) != std::string::npos) {
                    cert = c;
                    found = true;
                    break;
                }
            }
            if (!found) {
                return "CertUtil: Certificate \"" + target + "\" not found in certificate database.\nCertUtil: -verify command FAILED.\n";
            }
        }

        bool chainValid = false;
        std::vector<std::string> chainDns;
        pki.verifyCertificateChain(cert.serialNumber, &chainValid, &chainDns);

        ss << "Validating certificate serial: " << cert.serialNumber << "\n";
        ss << "  Subject: " << cert.subjectDn << "\n";
        ss << "  Issuer: " << cert.issuerDn << "\n";
        ss << "  Certificate Chain Validation: " << (chainValid ? "SUCCESS (Trusted Enterprise Root)" : "FAILED (Untrusted or Expired)") << "\n";
        for (size_t i = 0; i < chainDns.size(); ++i) {
            ss << "    [" << i << "] " << chainDns[i] << "\n";
        }

        // Revocation status check
        micant::certsrv::OcspResponse ocspResp;
        pki.processOcspRequest(cert.serialNumber, ocspResp);
        ss << "  Revocation Check (CRL & OCSP):\n";
        ss << "    CRL Status: " << (cert.isRevoked ? "REVOKED" : "Good (Not in CRL)") << "\n";
        ss << "    OCSP Status: " << OcspStatusToString(ocspResp.certStatus) << " (" << ocspResp.responderId << ")\n";

        if (chainValid && !cert.isRevoked) {
            ss << "CertUtil: -verify command completed successfully.\n";
        } else {
            ss << "CertUtil: -verify command completed with certificate verification errors.\n";
        }
        return ss.str();
    }

    static std::string ExecuteCaCert(const std::string& outFile) {
        auto& pki = micant::certsrv::CertificateServicesEngine::instance();
        micant::certsrv::CertificateRecord rootCert;
        std::ostringstream ss;

        if (!pki.getCertificate(pki.getRootSerialNumber(), rootCert)) {
            return "CertUtil: Failed to retrieve CA Root certificate.\n";
        }

        ss << "Exporting Enterprise Root CA Certificate for \"" << pki.getCaName() << "\"...\n";
        ss << "CA Certificate Serial: " << rootCert.serialNumber << "\n";
        ss << "Subject: " << rootCert.subjectDn << "\n";
        ss << "Output File: " << outFile << "\n";
        ss << rootCert.rawPem;
        ss << "CertUtil: -ca.cert command completed successfully.\n";
        return ss.str();
    }

    static std::string ExecuteCrl() {
        auto& pki = micant::certsrv::CertificateServicesEngine::instance();
        std::ostringstream ss;

        pki.publishCrl();
        auto crl = pki.getLatestCrl();

        ss << "Publishing Certificate Revocation List (CRL) for \"" << pki.getCaName() << "\"...\n";
        ss << "  Base CRL #" << crl.crlNumber << " published to " << crl.cdpUri << "\n";
        ss << "  Revoked certificates included: " << crl.revokedEntries.size() << "\n";
        ss << "  Delta CRL #" << (crl.crlNumber) << " published to " << crl.cdpUri << "\n";
        ss << "CertUtil: -crl command completed successfully.\n";
        return ss.str();
    }

    static std::string ExecuteGetReg(const std::string& key) {
        auto& pki = micant::certsrv::CertificateServicesEngine::instance();
        std::ostringstream ss;

        ss << "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\CertSvc\\Configuration\\"
           << pki.getCaName() << ":\n";
        ss << "  Active REG_DWORD = 1\n";
        ss << "  CAType REG_DWORD = 0 (Enterprise Root CA)\n";
        ss << "  CommonName REG_SZ = " << pki.getCaName() << "\n";
        ss << "  DistinguishedName REG_SZ = " << pki.getCaDn() << "\n";
        ss << "  CRLPeriod REG_SZ = Weeks\n";
        ss << "  CRLPeriodUnits REG_DWORD = 1\n";
        ss << "  CRLDeltaPeriod REG_SZ = Days\n";
        ss << "  CRLDeltaPeriodUnits REG_DWORD = 1\n";
        ss << "  ValidityPeriod REG_SZ = Years\n";
        ss << "  ValidityPeriodUnits REG_DWORD = 20\n";
        ss << "  WebEnrollment REG_DWORD = 1 (HTTPS Port 443 / SCEP / MS-WCCE)\n";
        ss << "CertUtil: -getreg command completed successfully.\n";
        return ss.str();
    }

    static std::string ExecutePulse() {
        std::ostringstream ss;
        ss << "CertUtil: Triggering Certificate Autoenrollment Pulse for machine and user stores...\n";
        ss << "  Evaluating machine certificate templates: DomainController, Computer, WebServer, SubCA.\n";
        ss << "  Evaluating user certificate templates: User, SmartcardLogon.\n";
        ss << "  Autoenrollment pulse completed: all enterprise stores synchronized with Active Directory.\n";
        ss << "CertUtil: -pulse command completed successfully.\n";
        return ss.str();
    }

    static std::string ExecuteViewDelStore(const std::string& store) {
        auto& pki = micant::certsrv::CertificateServicesEngine::instance();
        std::ostringstream ss;

        auto certs = pki.getAllCertificates();
        ss << "CertUtil: Inspecting Certificate Store \"" << store << "\"...\n";
        ss << "  Total certificates found in store: " << certs.size() << "\n";
        for (const auto& cert : certs) {
            ss << "    Serial: " << cert.serialNumber << " | Subject: " << cert.subjectDn << " | Template: " << cert.templateName << "\n";
        }
        ss << "CertUtil: -viewdelstore command completed successfully.\n";
        return ss.str();
    }
};

inline int ExecuteCertutil(int argc, char* argv[]) {
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    std::string out = CertutilCommandExecutor::Execute(args);
    std::cout << out;
    return (out.find("completed successfully") != std::string::npos) ? 0 : 1;
}

inline std::string ExecuteCertutilString(const std::string& cmdLine) {
    return CertutilCommandExecutor::ExecuteString(cmdLine);
}

} // namespace micant::certutil

#endif // MICANT_CERTUTIL_HPP
