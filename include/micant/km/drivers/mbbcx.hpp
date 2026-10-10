/**
 * @file mbbcx.hpp
 * @brief Windows Mobile Broadband Class Extension (MbbCx / mbbcx.sys) & 5G NR / eSIM Subsystem
 *
 * MicaNT Dave Cutler Clean-Room Architecture
 * Codename: TitanCellular / AegisRadio
 * Specification Reference: WDK Mobile Broadband Class Extension (MbbCx.sys),
 *                          USB-IF MBIM 4.0 (Mobile Broadband Interface Model),
 *                          GSMA SGP.22 RSP (Remote SIM Provisioning / LPA),
 *                          3GPP Release 17/18 5G NR SA/NSA, wwansvc.dll.
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
#include <chrono>
#include <algorithm>
#include <sstream>
#include <iomanip>

#include "ntstatus.hpp"
#include "scm.hpp"
#include "version.hpp"

namespace micant::mbbcx {

// Cellular Radio Access Technologies (RAT)
enum class CellularRat : uint32_t {
    None        = 0,
    GSM         = 1,
    UMTS        = 2,
    LTE         = 3,
    LTE_Adv     = 4,
    NR5G_NSA    = 5,  // 5G Non-Standalone (LTE Anchor + NR sub-6)
    NR5G_SA     = 6   // 5G Standalone (Pure 5G Core & gNodeB)
};

inline const char* CellularRatToString(CellularRat rat) {
    switch (rat) {
        case CellularRat::GSM:      return "2G GSM / EDGE";
        case CellularRat::UMTS:     return "3G UMTS / HSPA+";
        case CellularRat::LTE:      return "4G LTE";
        case CellularRat::LTE_Adv:  return "4G LTE-Advanced Pro";
        case CellularRat::NR5G_NSA: return "5G NR NSA (Option 3x Dual-Connectivity)";
        case CellularRat::NR5G_SA:  return "5G NR SA (Next-Gen Core Standalone)";
        default:                    return "No Service (Searching)";
    }
}

// Modem Radio Power States
enum class CellularRadioState : uint32_t {
    Off         = 0,
    On          = 1,
    AirplaneMode= 2,
    HardwareOff = 3
};

inline const char* CellularRadioStateToString(CellularRadioState state) {
    switch (state) {
        case CellularRadioState::On:          return "Radio Active (Online)";
        case CellularRadioState::Off:         return "Radio Software Off";
        case CellularRadioState::AirplaneMode:return "Airplane Mode Enabled";
        case CellularRadioState::HardwareOff: return "Hardware Switch Cut";
        default:                              return "Unknown State";
    }
}

// 3GPP Network Registration States
enum class NetworkRegistrationState : uint32_t {
    Deregistered = 0,
    RegisteredHome = 1,
    Searching      = 2,
    RegistrationDenied = 3,
    RegisteredRoaming  = 4
};

inline const char* NetworkRegistrationStateToString(NetworkRegistrationState state) {
    switch (state) {
        case NetworkRegistrationState::RegisteredHome:    return "Registered (Home Network)";
        case NetworkRegistrationState::RegisteredRoaming: return "Registered (Roaming Partner)";
        case NetworkRegistrationState::Searching:         return "Searching for Operator...";
        case NetworkRegistrationState::RegistrationDenied:return "Registration Denied";
        default:                                          return "Deregistered / Idle";
    }
}

// 5G / LTE Signal Quality Metrics
struct SignalMetrics {
    int32_t rsrp{-88}; // Reference Signal Received Power in dBm (-140 to -44 dBm)
    int32_t rsrq{-11}; // Reference Signal Received Quality in dB (-20 to -3 dB)
    int32_t sinr{18};  // Signal to Interference-plus-Noise Ratio in dB (-10 to 40 dB)
    int32_t rssi{-65}; // Received Signal Strength Indicator in dBm
    uint32_t bars{4};  // Signal strength bars (0 to 5)

    void recalculateBars() {
        if (rsrp >= -80) bars = 5;
        else if (rsrp >= -90) bars = 4;
        else if (rsrp >= -100) bars = 3;
        else if (rsrp >= -110) bars = 2;
        else if (rsrp >= -120) bars = 1;
        else bars = 0;
    }
};

// GSMA SGP.22 eSIM Profile Architecture (eUICC Local Profile Assistant)
enum class EsimProfileState : uint32_t {
    Disabled    = 0,
    Enabled     = 1,
    Installing  = 2,
    Deleted     = 3
};

inline const char* EsimProfileStateToString(EsimProfileState state) {
    switch (state) {
        case EsimProfileState::Enabled:    return "Active (Enabled)";
        case EsimProfileState::Disabled:   return "Inactive (Disabled)";
        case EsimProfileState::Installing: return "Downloading / Installing";
        case EsimProfileState::Deleted:    return "Deleted";
        default:                           return "Unknown";
    }
}

struct EsimProfile {
    std::wstring iccid;        // 19-20 digit Integrated Circuit Card ID
    std::wstring profileName;  // e.g. "Sovereign Unlimited 5G"
    std::wstring carrierName;  // e.g. "MicaNT Quantum Mobile"
    EsimProfileState state{EsimProfileState::Disabled};
    bool isOperational{true};  // Operational vs Test profile
};

// Cellular Packet Data Session (PDP / PDN Context)
struct DataSession {
    uint32_t sessionId{0};
    std::wstring apn;          // Access Point Name (e.g. "internet", "ims")
    std::string ipV4;          // Assigned IPv4
    std::string ipV6;          // Assigned IPv6
    std::string gateway;       // Default Gateway
    std::vector<std::string> dnsServers;
    uint32_t mtu{1500};
    bool isConnected{false};
    uint64_t txBytes{0};
    uint64_t rxBytes{0};
    uint64_t txPackets{0};
    uint64_t rxPackets{0};
};

// ----------------------------------------------------------------------------
// Mobile Broadband Modem Adapter Context (WDF / KMDF Class Extension Client)
// ----------------------------------------------------------------------------
class MbbAdapter {
public:
    MbbAdapter(uint32_t adapterId, std::wstring adapterName, std::string imei, std::string imsi)
        : m_adapterId(adapterId), m_adapterName(std::move(adapterName)),
          m_imei(std::move(imei)), m_imsi(std::move(imsi)) {
        m_signal.recalculateBars();
    }

    uint32_t getId() const { return m_adapterId; }
    const std::wstring& getName() const { return m_adapterName; }
    const std::string& getImei() const { return m_imei; }
    const std::string& getImsi() const { return m_imsi; }

    CellularRadioState getRadioState() const { return m_radioState; }
    void setRadioState(CellularRadioState state) { m_radioState = state; }

    CellularRat getRat() const { return m_currentRat; }
    void setRat(CellularRat rat) { m_currentRat = rat; }

    NetworkRegistrationState getRegistrationState() const { return m_regState; }
    void setRegistrationState(NetworkRegistrationState state) { m_regState = state; }

    const std::wstring& getOperatorName() const { return m_operatorName; }
    void setOperatorName(std::wstring op) { m_operatorName = std::move(op); }

    SignalMetrics getSignal() const { return m_signal; }
    void setSignal(int32_t rsrp, int32_t rsrq, int32_t sinr) {
        m_signal.rsrp = rsrp;
        m_signal.rsrq = rsrq;
        m_signal.sinr = sinr;
        m_signal.recalculateBars();
    }

    // eSIM Profile Management
    void addEsimProfile(const EsimProfile& prof) {
        m_profiles.push_back(prof);
    }

    std::vector<EsimProfile> getEsimProfiles() const {
        return m_profiles;
    }

    bool enableEsimProfile(const std::wstring& iccid) {
        bool found = false;
        for (auto& p : m_profiles) {
            if (p.iccid == iccid) {
                p.state = EsimProfileState::Enabled;
                found = true;
            } else if (p.state == EsimProfileState::Enabled) {
                p.state = EsimProfileState::Disabled; // Single active eUICC profile
            }
        }
        return found;
    }

    bool disableEsimProfile(const std::wstring& iccid) {
        for (auto& p : m_profiles) {
            if (p.iccid == iccid) {
                p.state = EsimProfileState::Disabled;
                return true;
            }
        }
        return false;
    }

    bool deleteEsimProfile(const std::wstring& iccid) {
        auto it = std::remove_if(m_profiles.begin(), m_profiles.end(), [&](const EsimProfile& p) {
            return p.iccid == iccid;
        });
        if (it != m_profiles.end()) {
            m_profiles.erase(it, m_profiles.end());
            return true;
        }
        return false;
    }

    // Packet Data Sessions
    uint32_t establishDataSession(const std::wstring& apn, const std::string& ipV4, const std::string& ipV6) {
        uint32_t sid = m_nextSessionId++;
        DataSession s{};
        s.sessionId = sid;
        s.apn = apn;
        s.ipV4 = ipV4;
        s.ipV6 = ipV6;
        s.gateway = "100.64.0.1";
        s.dnsServers = { "8.8.8.8", "1.1.1.1" };
        s.mtu = 1500;
        s.isConnected = true;
        m_sessions[sid] = s;
        return sid;
    }

    bool closeDataSession(uint32_t sessionId) {
        auto it = m_sessions.find(sessionId);
        if (it != m_sessions.end()) {
            m_sessions.erase(it);
            return true;
        }
        return false;
    }

    DataSession* getDataSession(uint32_t sessionId) {
        auto it = m_sessions.find(sessionId);
        if (it != m_sessions.end()) return &it->second;
        return nullptr;
    }

    std::vector<DataSession> getAllSessions() const {
        std::vector<DataSession> list;
        for (const auto& pair : m_sessions) {
            list.push_back(pair.second);
        }
        return list;
    }

    bool transmitPacket(uint32_t sessionId, uint32_t byteCount) {
        auto it = m_sessions.find(sessionId);
        if (it != m_sessions.end() && it->second.isConnected) {
            it->second.txPackets++;
            it->second.txBytes += byteCount;
            return true;
        }
        return false;
    }

    bool receivePacket(uint32_t sessionId, uint32_t byteCount) {
        auto it = m_sessions.find(sessionId);
        if (it != m_sessions.end() && it->second.isConnected) {
            it->second.rxPackets++;
            it->second.rxBytes += byteCount;
            return true;
        }
        return false;
    }

private:
    uint32_t m_adapterId{1};
    std::wstring m_adapterName;
    std::string m_imei;
    std::string m_imsi;
    CellularRadioState m_radioState{CellularRadioState::On};
    CellularRat m_currentRat{CellularRat::NR5G_SA};
    NetworkRegistrationState m_regState{NetworkRegistrationState::RegisteredHome};
    std::wstring m_operatorName{L"MicaNT Sovereign 5G"};
    SignalMetrics m_signal;
    std::vector<EsimProfile> m_profiles;
    std::map<uint32_t, DataSession> m_sessions;
    uint32_t m_nextSessionId{101};
};

// ----------------------------------------------------------------------------
// MbbCx Core Subsystem Singleton (TitanCellular / AegisRadio)
// ----------------------------------------------------------------------------
class MbbSubsystem {
public:
    static MbbSubsystem& get() {
        static MbbSubsystem instance;
        return instance;
    }

    bool initialize() {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_initialized) return true;

        m_adapters.clear();

        // Register default 5G NR Snapdragon X75 Cellular Modem
        auto primaryAdapter = std::make_shared<MbbAdapter>(
            1, L"Snapdragon X75 5G Sub-6 & mmWave Modem", "861234567890123", "310410123456789");

        // Pre-configure carrier network state
        primaryAdapter->setOperatorName(L"MicaNT Sovereign 5G NR");
        primaryAdapter->setRat(CellularRat::NR5G_SA);
        primaryAdapter->setRegistrationState(NetworkRegistrationState::RegisteredHome);
        primaryAdapter->setSignal(-82, -10, 22); // Strong 5G SA signal

        // Seed default eSIM Profiles
        EsimProfile p1{};
        p1.iccid = L"89014103211118501234";
        p1.profileName = L"Sovereign Unlimited 5G SA";
        p1.carrierName = L"MicaNT Mobile";
        p1.state = EsimProfileState::Enabled;
        p1.isOperational = true;
        primaryAdapter->addEsimProfile(p1);

        EsimProfile p2{};
        p2.iccid = L"89012604987654321098";
        p2.profileName = L"Global Roaming Data eSIM";
        p2.carrierName = L"Quantum Telematics";
        p2.state = EsimProfileState::Disabled;
        p2.isOperational = true;
        primaryAdapter->addEsimProfile(p2);

        // Pre-configure primary internet APN session
        primaryAdapter->establishDataSession(L"internet", "100.64.42.10", "2607:fb90:1234:5678::42");

        m_adapters[1] = primaryAdapter;
        m_initialized = true;
        return true;
    }

    bool isInitialized() const { return m_initialized; }

    std::shared_ptr<MbbAdapter> getAdapter(uint32_t id) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_adapters.find(id);
        if (it != m_adapters.end()) return it->second;
        return nullptr;
    }

    uint32_t registerAdapter(const std::wstring& name, const std::string& imei, const std::string& imsi) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        uint32_t newId = static_cast<uint32_t>(m_adapters.size()) + 1;
        auto adp = std::make_shared<MbbAdapter>(newId, name, imei, imsi);
        m_adapters[newId] = adp;
        return newId;
    }

    std::vector<std::shared_ptr<MbbAdapter>> getAllAdapters() const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        std::vector<std::shared_ptr<MbbAdapter>> list;
        for (const auto& pair : m_adapters) {
            list.push_back(pair.second);
        }
        return list;
    }

    uint32_t getAdapterCount() const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return static_cast<uint32_t>(m_adapters.size());
    }

    void reset() {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_adapters.clear();
        m_initialized = false;
        initialize();
    }

private:
    MbbSubsystem() = default;

    bool m_initialized{false};
    std::map<uint32_t, std::shared_ptr<MbbAdapter>> m_adapters;
    mutable std::recursive_mutex m_mutex;
};

// ----------------------------------------------------------------------------
// Win32 & NT Clean-Room Dynamic C ABI Parity Exports (mbbcx.sys, wwansvc.dll)
// ----------------------------------------------------------------------------
extern "C" {

inline NTSTATUS MbbDeviceInitialize(void* /*deviceContext*/) {
    if (MbbSubsystem::get().initialize()) {
        return micant::STATUS_SUCCESS;
    }
    return micant::STATUS_UNSUCCESSFUL;
}

inline NTSTATUS MbbAdapterCreate(const wchar_t* adapterName, uint32_t* pAdapterId) {
    if (!adapterName || !pAdapterId) {
        return micant::STATUS_INVALID_PARAMETER;
    }
    uint32_t id = MbbSubsystem::get().registerAdapter(adapterName, "860000000000001", "310410000000001");
    *pAdapterId = id;
    return micant::STATUS_SUCCESS;
}

inline NTSTATUS MbbRadioStateSet(uint32_t adapterId, uint32_t radioState) {
    auto adp = MbbSubsystem::get().getAdapter(adapterId);
    if (!adp) return micant::STATUS_NOT_FOUND;
    if (radioState > 3) return micant::STATUS_INVALID_PARAMETER;

    adp->setRadioState(static_cast<CellularRadioState>(radioState));
    return micant::STATUS_SUCCESS;
}

inline NTSTATUS MbbConnectDataSession(uint32_t adapterId, const wchar_t* apn, uint32_t /*ipType*/, uint32_t* pSessionId) {
    if (!apn || !pSessionId) return micant::STATUS_INVALID_PARAMETER;
    auto adp = MbbSubsystem::get().getAdapter(adapterId);
    if (!adp) return micant::STATUS_NOT_FOUND;

    uint32_t sid = adp->establishDataSession(apn, "100.64.10.2", "2607:fb90:dead:beef::1");
    *pSessionId = sid;
    return micant::STATUS_SUCCESS;
}

inline NTSTATUS MbbGetSignalState(uint32_t adapterId, int32_t* pRsrp, int32_t* pRsrq, int32_t* pSinr) {
    auto adp = MbbSubsystem::get().getAdapter(adapterId);
    if (!adp) return micant::STATUS_NOT_FOUND;

    auto sig = adp->getSignal();
    if (pRsrp) *pRsrp = sig.rsrp;
    if (pRsrq) *pRsrq = sig.rsrq;
    if (pSinr) *pSinr = sig.sinr;
    return micant::STATUS_SUCCESS;
}

inline NTSTATUS MbbEsimProfileManage(uint32_t adapterId, uint32_t action, const wchar_t* profileIccid, uint32_t* pResult) {
    if (!profileIccid || !pResult) return micant::STATUS_INVALID_PARAMETER;
    auto adp = MbbSubsystem::get().getAdapter(adapterId);
    if (!adp) return micant::STATUS_NOT_FOUND;

    // action: 0 = Enable, 1 = Disable, 2 = Delete
    bool success = false;
    if (action == 0) {
        success = adp->enableEsimProfile(profileIccid);
    } else if (action == 1) {
        success = adp->disableEsimProfile(profileIccid);
    } else if (action == 2) {
        success = adp->deleteEsimProfile(profileIccid);
    } else {
        return micant::STATUS_INVALID_PARAMETER;
    }

    *pResult = success ? 1 : 0;
    return success ? micant::STATUS_SUCCESS : micant::STATUS_NOT_FOUND;
}

} // extern "C"

// ----------------------------------------------------------------------------
// SCM & Version Registration Helper
// ----------------------------------------------------------------------------
inline void RegisterMbbSubsystem() {
    // 1. Version Database Registration
    auto& vdb = micant::version::VersionDatabase::Instance();
    vdb.RegisterModule("mbbcx.sys", "10.0.26100.1", "Mobile Broadband Class Extension Driver (TitanCellular)");
    vdb.RegisterModule("wwansvc.dll", "10.0.26100.1", "Windows Mobile Broadband Telephony Service (AegisRadio)");

    // 2. SCM Registration
    auto& scm = micant::scm::ServiceControlManager::get();
    auto rec = std::make_shared<micant::scm::ServiceRecord>();
    rec->serviceName = L"WwanSvc";
    rec->displayName = L"Windows Mobile Broadband Service (WWAN)";
    rec->serviceType = micant::scm::SERVICE_WIN32_SHARE_PROCESS;
    rec->startType = micant::scm::SERVICE_AUTO_START;
    rec->binaryPath = L"C:\\Windows\\System32\\svchost.exe -k NetworkService";
    rec->status.dwCurrentState = micant::scm::SERVICE_RUNNING;
    scm.registerServiceRecord(rec);

    // Initialize core subsystem singleton
    MbbSubsystem::get().initialize();
}

} // namespace micant::mbbcx
