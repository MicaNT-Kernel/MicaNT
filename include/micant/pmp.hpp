/**
 * @file pmp.hpp
 * @brief Windows Hardware Protected Media Path (PMP), Protected Audio Video Path (PAVP) & HDCP 2.3 Subsystem
 *
 * MicaNT Dave Cutler Clean-Room Architecture
 * Codename: TitanPMP / AegisContent
 * Specification Reference: Windows Media Foundation Protected Media Path (mfpmp.exe),
 *                          Output Protection Manager (OPM / dxva2.dll),
 *                          High-bandwidth Digital Content Protection (HDCP 2.3),
 *                          Intel PAVP / AMD Secure Display / ARM TrustZone DRM.
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <array>

#include "ntstatus.hpp"
#include "scm.hpp"
#include "version.hpp"

namespace micant::pmp {

// ----------------------------------------------------------------------------
// OPM & HDCP Definitions
// ----------------------------------------------------------------------------
enum class OpmConnectorType : uint32_t {
    Unknown               = 0,
    VGA                   = 1,
    DVI                   = 2,
    HDMI                  = 3,
    DisplayPort_External  = 4,
    DisplayPort_Embedded  = 5,
    MiracastWireless      = 6
};

inline const char* OpmConnectorTypeToString(OpmConnectorType type) {
    switch (type) {
        case OpmConnectorType::VGA:                  return "VGA (Analog RGB)";
        case OpmConnectorType::DVI:                  return "DVI (Digital Single/Dual Link)";
        case OpmConnectorType::HDMI:                 return "HDMI 2.1 FRL (Fixed Rate Link)";
        case OpmConnectorType::DisplayPort_External: return "DisplayPort 2.1 (UHBR20 External)";
        case OpmConnectorType::DisplayPort_Embedded: return "eDP 1.5 (Embedded DisplayPort)";
        case OpmConnectorType::MiracastWireless:     return "Miracast HDCP-WFD (Wi-Fi Direct)";
        default:                                     return "Unknown / Virtual Endpoint";
    }
}

enum class HdcpProtectionLevel : uint32_t {
    Off          = 0, // No copy protection active
    Hdcp14       = 1, // Legacy HDCP 1.4 (56-bit KSV validation)
    Hdcp22_Type0 = 2, // HDCP 2.2 Type 0 (128-bit AES, standard 4K)
    Hdcp23_Type1 = 3  // HDCP 2.3 Type 1 (128-bit AES, hardware root of trust, no legacy repeaters)
};

inline const char* HdcpProtectionLevelToString(HdcpProtectionLevel level) {
    switch (level) {
        case HdcpProtectionLevel::Hdcp23_Type1: return "HDCP 2.3 Type 1 (Ultra Secure Hardware Enclave)";
        case HdcpProtectionLevel::Hdcp22_Type0: return "HDCP 2.2 Type 0 (4K UHD Content Protection)";
        case HdcpProtectionLevel::Hdcp14:       return "HDCP 1.4 (Legacy Digital Link Encryption)";
        default:                                return "Unprotected / HDCP Disabled";
    }
}

enum class OpmProtectionType : uint32_t {
    None         = 0,
    HDCP         = 1,
    DPCP         = 2,
    AnalogCGMSA  = 3
};

// ----------------------------------------------------------------------------
// Protected Video Output (Display / Monitor Endpoint)
// ----------------------------------------------------------------------------
class ProtectedVideoOutput {
public:
    ProtectedVideoOutput(uint32_t id, uint64_t hMon, std::wstring name,
                         OpmConnectorType conn, HdcpProtectionLevel maxLevel)
        : m_outputId(id), m_hMonitor(hMon), m_name(std::move(name)),
          m_connector(conn), m_maxHdcp(maxLevel) {
        // Generate simulated 256-byte X.509 certificate for OPM key exchange
        m_certificate.resize(256);
        for (size_t i = 0; i < m_certificate.size(); ++i) {
            m_certificate[i] = static_cast<uint8_t>((i * 31 + id) & 0xFF);
        }
    }

    uint32_t getId() const { return m_outputId; }
    uint64_t getHMonitor() const { return m_hMonitor; }
    const std::wstring& getName() const { return m_name; }
    OpmConnectorType getConnector() const { return m_connector; }
    HdcpProtectionLevel getCurrentHdcp() const { return m_currentHdcp; }
    HdcpProtectionLevel getMaxHdcp() const { return m_maxHdcp; }

    bool setProtection(HdcpProtectionLevel level) {
        if (static_cast<uint32_t>(level) > static_cast<uint32_t>(m_maxHdcp)) {
            return false; // Hardware capability exceeded
        }
        m_currentHdcp = level;
        return true;
    }

    bool isRepeater() const { return m_isRepeater; }
    void setRepeater(bool rep, uint32_t count = 0, uint32_t depth = 0) {
        m_isRepeater = rep;
        m_repeaterCount = count;
        m_repeaterDepth = depth;
    }

    uint32_t getRepeaterCount() const { return m_repeaterCount; }
    uint32_t getRepeaterDepth() const { return m_repeaterDepth; }

    const std::vector<uint8_t>& getCertificate() const { return m_certificate; }

    // Evaluates whether a protected stream (e.g. 4K HDR) is authorized on this output
    bool isStreamAuthorized(uint32_t height, bool requiresHdr) const {
        if (m_currentHdcp == HdcpProtectionLevel::Off) return false;

        // 8K or High-Assurance Studio Content requires HDCP 2.3 Type 1
        if (height >= 4320) {
            return (m_currentHdcp >= HdcpProtectionLevel::Hdcp23_Type1 && (!m_isRepeater || m_repeaterCount == 0));
        }

        // 4K UHD or HDR requires at least HDCP 2.2
        if (height >= 2160 || requiresHdr) {
            return (m_currentHdcp >= HdcpProtectionLevel::Hdcp22_Type0);
        }

        // Standard 1080p requires at least HDCP 1.4
        return (m_currentHdcp >= HdcpProtectionLevel::Hdcp14);
    }

private:
    uint32_t m_outputId{1};
    uint64_t m_hMonitor{0x10001};
    std::wstring m_name;
    OpmConnectorType m_connector{OpmConnectorType::DisplayPort_External};
    HdcpProtectionLevel m_currentHdcp{HdcpProtectionLevel::Hdcp23_Type1};
    HdcpProtectionLevel m_maxHdcp{HdcpProtectionLevel::Hdcp23_Type1};
    bool m_isRepeater{false};
    uint32_t m_repeaterCount{0};
    uint32_t m_repeaterDepth{0};
    std::vector<uint8_t> m_certificate;
};

// ----------------------------------------------------------------------------
// DRM Cryptographic Session & Key Material
// ----------------------------------------------------------------------------
struct DrmSessionKey {
    uint32_t keyId{1};
    std::array<uint8_t, 16> aesKey{
        0x10, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
        0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x01
    };
    std::array<uint8_t, 16> iv{
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
    };
    bool isRevoked{false};
};

// ----------------------------------------------------------------------------
// Protected Audio Video Path (PAVP) Stream Context
// ----------------------------------------------------------------------------
class PavpSession {
public:
    PavpSession(uint32_t sid, std::wstring name, uint32_t width, uint32_t height, bool hdr)
        : m_sessionId(sid), m_name(std::move(name)), m_width(width), m_height(height), m_isHdr(hdr) {}

    uint32_t getId() const { return m_sessionId; }
    const std::wstring& getName() const { return m_name; }
    uint32_t getWidth() const { return m_width; }
    uint32_t getHeight() const { return m_height; }
    bool isHdr() const { return m_isHdr; }

    uint64_t getFramesProcessed() const { return m_framesProcessed; }
    uint64_t getBytesDecrypted() const { return m_bytesDecrypted; }

    const DrmSessionKey& getKey() const { return m_key; }
    void setKeyRevoked(bool rev) { m_key.isRevoked = rev; }

    // Hardware decryption operation (AES-128 CTR keystream emulation)
    bool decryptFrame(const uint8_t* in, uint32_t inLen, uint8_t* out, uint32_t& outLen) {
        if (!in || !out || inLen == 0 || m_key.isRevoked) {
            return false;
        }

        // Apply reversible stream keystream transform
        for (uint32_t i = 0; i < inLen; ++i) {
            uint8_t k = m_key.aesKey[i % 16] ^ m_key.iv[(i + 3) % 16];
            out[i] = in[i] ^ k;
        }
        outLen = inLen;
        m_framesProcessed++;
        m_bytesDecrypted += inLen;
        return true;
    }

private:
    uint32_t m_sessionId{1};
    std::wstring m_name;
    uint32_t m_width{3840};
    uint32_t m_height{2160};
    bool m_isHdr{true};
    uint64_t m_framesProcessed{0};
    uint64_t m_bytesDecrypted{0};
    DrmSessionKey m_key;
};

// ----------------------------------------------------------------------------
// Media Foundation Protected Process Light (mfpmp.exe Host)
// ----------------------------------------------------------------------------
struct MfpmpProcessHost {
    uint32_t processId{2048};
    bool isPplActive{true};
    bool isCodeIntegrityPassed{true};
    std::wstring binaryPath{L"C:\\Windows\\System32\\mfpmp.exe"};
};

// ----------------------------------------------------------------------------
// Certificate Revocation List (CRL) Manager
// ----------------------------------------------------------------------------
class RevocationManager {
public:
    void revokeCertificateHash(uint32_t hash) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_revokedHashes.push_back(hash);
    }

    bool isRevoked(const std::vector<uint8_t>& cert) const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (cert.empty()) return true;
        uint32_t hash = computeHash(cert);
        return std::find(m_revokedHashes.begin(), m_revokedHashes.end(), hash) != m_revokedHashes.end();
    }

    size_t getRevokedCount() const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_revokedHashes.size();
    }

    void clear() {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_revokedHashes.clear();
    }

private:
    static uint32_t computeHash(const std::vector<uint8_t>& data) {
        uint32_t h = 2166136261u;
        for (auto b : data) {
            h ^= b;
            h *= 16777619u;
        }
        return h;
    }

    std::vector<uint32_t> m_revokedHashes;
    mutable std::recursive_mutex m_mutex;
};

// ----------------------------------------------------------------------------
// PMP Core Subsystem Singleton (TitanPMP / AegisContent)
// ----------------------------------------------------------------------------
class PmpSubsystem {
public:
    static PmpSubsystem& get() {
        static PmpSubsystem instance;
        return instance;
    }

    bool initialize() {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_initialized) return true;

        m_outputs.clear();
        m_sessions.clear();
        m_revocationMgr.clear();

        // Register default Monitor Output 1: DisplayPort 2.1 4K 144Hz Gaming OLED
        auto out1 = std::make_shared<ProtectedVideoOutput>(
            1, 0x10001, L"DELL Alienware AW3225QF (DP 2.1 UHBR20)",
            OpmConnectorType::DisplayPort_External, HdcpProtectionLevel::Hdcp23_Type1);
        m_outputs[1] = out1;

        // Register default Monitor Output 2: HDMI 2.1 8K Reference Display
        auto out2 = std::make_shared<ProtectedVideoOutput>(
            2, 0x10002, L"Sony BRAVIA XR Master 8K (HDMI 2.1 FRL)",
            OpmConnectorType::HDMI, HdcpProtectionLevel::Hdcp23_Type1);
        m_outputs[2] = out2;

        // Seed default 4K HDR PAVP Media Session
        auto sess1 = std::make_shared<PavpSession>(
            101, L"Studio4K_HEVC_Main10_HDR_ProtectedStream", 3840, 2160, true);
        m_sessions[101] = sess1;

        m_initialized = true;
        return true;
    }

    bool isInitialized() const { return m_initialized; }

    std::shared_ptr<ProtectedVideoOutput> getOutput(uint32_t id) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_outputs.find(id);
        if (it != m_outputs.end()) return it->second;
        return nullptr;
    }

    std::vector<std::shared_ptr<ProtectedVideoOutput>> getAllOutputs() const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        std::vector<std::shared_ptr<ProtectedVideoOutput>> list;
        for (const auto& pair : m_outputs) {
            list.push_back(pair.second);
        }
        return list;
    }

    uint32_t getOutputCount() const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return static_cast<uint32_t>(m_outputs.size());
    }

    uint32_t registerOutput(uint64_t hMonitor, std::wstring name,
                            OpmConnectorType conn, HdcpProtectionLevel maxLevel) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        uint32_t newId = static_cast<uint32_t>(m_outputs.size()) + 1;
        auto out = std::make_shared<ProtectedVideoOutput>(newId, hMonitor, std::move(name), conn, maxLevel);
        m_outputs[newId] = out;
        return newId;
    }

    // PAVP Session Management
    uint32_t createSecureSession(std::wstring name, uint32_t width, uint32_t height, bool hdr) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        uint32_t newSid = m_nextSessionId++;
        auto sess = std::make_shared<PavpSession>(newSid, std::move(name), width, height, hdr);
        m_sessions[newSid] = sess;
        return newSid;
    }

    std::shared_ptr<PavpSession> getSession(uint32_t sid) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_sessions.find(sid);
        if (it != m_sessions.end()) return it->second;
        return nullptr;
    }

    bool closeSession(uint32_t sid) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_sessions.find(sid);
        if (it != m_sessions.end()) {
            m_sessions.erase(it);
            return true;
        }
        return false;
    }

    std::vector<std::shared_ptr<PavpSession>> getAllSessions() const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        std::vector<std::shared_ptr<PavpSession>> list;
        for (const auto& pair : m_sessions) {
            list.push_back(pair.second);
        }
        return list;
    }

    uint32_t getSessionCount() const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return static_cast<uint32_t>(m_sessions.size());
    }

    MfpmpProcessHost& getProcessHost() { return m_host; }
    RevocationManager& getRevocationManager() { return m_revocationMgr; }

    void reset() {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_outputs.clear();
        m_sessions.clear();
        m_revocationMgr.clear();
        m_initialized = false;
        initialize();
    }

private:
    PmpSubsystem() = default;

    bool m_initialized{false};
    std::map<uint32_t, std::shared_ptr<ProtectedVideoOutput>> m_outputs;
    std::map<uint32_t, std::shared_ptr<PavpSession>> m_sessions;
    uint32_t m_nextSessionId{102};
    MfpmpProcessHost m_host;
    RevocationManager m_revocationMgr;
    mutable std::recursive_mutex m_mutex;
};

// ----------------------------------------------------------------------------
// Win32 & NT Clean-Room Dynamic C ABI Parity Exports (mfpmp.exe, dxva2.dll)
// ----------------------------------------------------------------------------
extern "C" {

inline NTSTATUS PmpInitializeSubsystem() {
    if (PmpSubsystem::get().initialize()) {
        return micant::STATUS_SUCCESS;
    }
    return micant::STATUS_UNSUCCESSFUL;
}

inline NTSTATUS OPMGetVideoOutputsFromHMONITOR(uint64_t hMonitor, uint32_t* pOutputCount, uint32_t* pOutputArray) {
    if (!pOutputCount) return micant::STATUS_INVALID_PARAMETER;
    auto& sys = PmpSubsystem::get();
    sys.initialize();

    auto allOuts = sys.getAllOutputs();
    std::vector<uint32_t> matched;
    for (const auto& o : allOuts) {
        if (hMonitor == 0 || o->getHMonitor() == hMonitor) {
            matched.push_back(o->getId());
        }
    }

    if (pOutputArray) {
        uint32_t copyCount = std::min(*pOutputCount, static_cast<uint32_t>(matched.size()));
        for (uint32_t i = 0; i < copyCount; ++i) {
            pOutputArray[i] = matched[i];
        }
        *pOutputCount = copyCount;
    } else {
        *pOutputCount = static_cast<uint32_t>(matched.size());
    }

    return micant::STATUS_SUCCESS;
}

inline NTSTATUS OPMCreateProtectedOutput(uint32_t outputId, uint32_t* pProtectedOutputHandle) {
    if (!pProtectedOutputHandle) return micant::STATUS_INVALID_PARAMETER;
    auto& sys = PmpSubsystem::get();
    sys.initialize();

    auto out = sys.getOutput(outputId);
    if (!out) return micant::STATUS_NOT_FOUND;

    *pProtectedOutputHandle = outputId;
    return micant::STATUS_SUCCESS;
}

inline NTSTATUS OPMGetCertificateSize(uint32_t outputHandle, uint32_t* pCertSize) {
    if (!pCertSize) return micant::STATUS_INVALID_PARAMETER;
    auto& sys = PmpSubsystem::get();
    sys.initialize();

    auto out = sys.getOutput(outputHandle);
    if (!out) return micant::STATUS_NOT_FOUND;

    *pCertSize = static_cast<uint32_t>(out->getCertificate().size());
    return micant::STATUS_SUCCESS;
}

inline NTSTATUS OPMGetCertificate(uint32_t outputHandle, uint8_t* pCertBuffer, uint32_t certSize) {
    if (!pCertBuffer || certSize == 0) return micant::STATUS_INVALID_PARAMETER;
    auto& sys = PmpSubsystem::get();
    sys.initialize();

    auto out = sys.getOutput(outputHandle);
    if (!out) return micant::STATUS_NOT_FOUND;

    const auto& cert = out->getCertificate();
    if (certSize < cert.size()) return micant::STATUS_BUFFER_TOO_SMALL;

    std::copy(cert.begin(), cert.end(), pCertBuffer);
    return micant::STATUS_SUCCESS;
}

inline NTSTATUS OPMSetProtectionLevel(uint32_t outputHandle, uint32_t protectionType, uint32_t protectionLevel) {
    auto& sys = PmpSubsystem::get();
    sys.initialize();

    auto out = sys.getOutput(outputHandle);
    if (!out) return micant::STATUS_NOT_FOUND;

    if (protectionType != static_cast<uint32_t>(OpmProtectionType::HDCP)) {
        return micant::STATUS_NOT_SUPPORTED;
    }

    if (protectionLevel > 3) return micant::STATUS_INVALID_PARAMETER;

    bool ok = out->setProtection(static_cast<HdcpProtectionLevel>(protectionLevel));
    return ok ? micant::STATUS_SUCCESS : micant::STATUS_INVALID_PARAMETER;
}

inline NTSTATUS PmpCreateSecureSession(const wchar_t* sessionName, uint32_t width, uint32_t height, uint32_t isHdr, uint32_t* pSessionId) {
    if (!sessionName || !pSessionId) return micant::STATUS_INVALID_PARAMETER;
    auto& sys = PmpSubsystem::get();
    sys.initialize();

    uint32_t sid = sys.createSecureSession(sessionName, width, height, isHdr != 0);
    *pSessionId = sid;
    return micant::STATUS_SUCCESS;
}

inline NTSTATUS PmpDecryptSample(uint32_t sessionId, const uint8_t* encData, uint32_t dataSize, uint8_t* decData, uint32_t* pDecSize) {
    if (!encData || !decData || !pDecSize || dataSize == 0) return micant::STATUS_INVALID_PARAMETER;
    auto& sys = PmpSubsystem::get();
    sys.initialize();

    auto sess = sys.getSession(sessionId);
    if (!sess) return micant::STATUS_NOT_FOUND;

    uint32_t outLen = 0;
    if (sess->decryptFrame(encData, dataSize, decData, outLen)) {
        *pDecSize = outLen;
        return micant::STATUS_SUCCESS;
    }
    return micant::STATUS_UNSUCCESSFUL;
}

} // extern "C"

// ----------------------------------------------------------------------------
// SCM & Version Registration Helper
// ----------------------------------------------------------------------------
inline void RegisterPmpSubsystem() {
    // 1. Version Database Registration
    auto& vdb = micant::version::VersionDatabase::Instance();
    vdb.RegisterModule("mfpmp.exe", "10.0.26100.1", "Media Foundation Protected Media Path Engine (TitanPMP)");
    vdb.RegisterModule("dxva2.dll", "10.0.26100.1", "DirectX Video Acceleration 2.0 / PAVP Driver (AegisContent)");

    // 2. SCM Registration
    auto& scm = micant::scm::ServiceControlManager::get();
    auto rec = std::make_shared<micant::scm::ServiceRecord>();
    rec->serviceName = L"PmpService";
    rec->displayName = L"Protected Media Path Driver Service (Media Foundation)";
    rec->serviceType = micant::scm::SERVICE_WIN32_SHARE_PROCESS;
    rec->startType = micant::scm::SERVICE_AUTO_START;
    rec->binaryPath = L"C:\\Windows\\System32\\svchost.exe -k MediaGroup";
    rec->status.dwCurrentState = micant::scm::SERVICE_RUNNING;
    scm.registerServiceRecord(rec);

    // Initialize core subsystem singleton
    PmpSubsystem::get().initialize();
}

} // namespace micant::pmp
