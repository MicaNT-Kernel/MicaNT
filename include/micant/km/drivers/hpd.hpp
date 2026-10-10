// ============================================================================
// MicaNT Clean-Room Kernel - Windows Human Presence Detection (HPD / Presence Sensing)
// File: include/micant/hpd.hpp
//
// Provenance & Clean-Room Statement:
// Authored strictly from public Microsoft Windows 11 Human Presence Detection (HPD)
// specifications, USB-IF HID Sensor Usage Tables (0x20: Biometric / Human Presence),
// and PC OEM platform guidelines (Wake on Approach / Walk Away Lock):
//   - Microsoft Human Presence Detection Architecture:
//     * Sensor modalities: Time-of-Flight (ToF), mmWave Radar, Ultrasonic, Computer Vision IR.
//     * Presence States: Present, NotPresent, Engaged (gaze on screen), Unengaged (looked away).
//     * Dynamic Zone Transitions: Approaching, Engaged, Leaving, Absent.
//     * Adaptive Dimming & Look Away Dim: Smooth display power reduction when unengaged.
//     * Wake on Approach: Instant Modern Standby S0ix display wake upon user arrival.
//     * Walk-Away Lock: Automated secure workstation lock upon user departure.
//   - Windows Sensor Platform Integration:
//     * SensorService daemon (`sensrsvc.dll`) and hardware bus driver (`hpd.sys`).
//     * Clean-room C ABI parity exports and SCM service coordinator.
//
// Sovereign Codename: TitanPresence / AegisPresence
// Strict ISO C++23, zero external dependencies, 100% offline, zero telemetry.
// ============================================================================

#ifndef MICANT_HPD_HPP
#define MICANT_HPD_HPP

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <deque>
#include <chrono>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <functional>

#include "ntdef.hpp"
#include "ntstatus.hpp"
#include "version.hpp"
#include "scm.hpp"
#include "ldr.hpp"

namespace micant::hpd {

// ============================================================================
// 1. Human Presence Constants, Types & Sensor Reports
// ============================================================================

enum class HumanPresenceState : uint32_t {
    Unknown      = 0,
    Present      = 1, // User detected within field of view
    NotPresent   = 2, // No user detected
    Approaching  = 3, // Distance decreasing towards engagement zone
    Leaving      = 4, // Distance increasing away from workstation
    Engaged      = 5, // User present and attention/gaze directed at screen
    Unengaged    = 6  // User present but looked away (triggers look-away dim)
};

inline const char* PresenceStateToString(HumanPresenceState state) noexcept {
    switch (state) {
        case HumanPresenceState::Present:     return "Present";
        case HumanPresenceState::NotPresent:  return "NotPresent";
        case HumanPresenceState::Approaching: return "Approaching";
        case HumanPresenceState::Leaving:     return "Leaving";
        case HumanPresenceState::Engaged:     return "Engaged";
        case HumanPresenceState::Unengaged:   return "Unengaged (Look Away)";
        case HumanPresenceState::Unknown:
        default:                              return "Unknown";
    }
}

enum class PresenceSensorType : uint32_t {
    TimeOfFlight     = 0, // Optical IR Time-of-Flight
    RadarMmWave      = 1, // 60 GHz / 77 GHz mmWave FMCW Radar
    Ultrasonic       = 2, // Acoustic distance sensor
    ComputerVisionIR = 3  // Windows Hello IR camera presence pipeline
};

inline const char* SensorTypeToString(PresenceSensorType type) noexcept {
    switch (type) {
        case PresenceSensorType::TimeOfFlight:     return "Time-of-Flight (ToF)";
        case PresenceSensorType::RadarMmWave:      return "mmWave Radar (60GHz)";
        case PresenceSensorType::Ultrasonic:       return "Ultrasonic";
        case PresenceSensorType::ComputerVisionIR: return "Computer Vision IR";
        default:                                   return "Generic";
    }
}

struct HpdSensorReport {
    uint32_t           sensorId{1};
    PresenceSensorType sensorType{PresenceSensorType::RadarMmWave};
    float              distanceMeters{1.0f};  // Target distance in meters
    float              azimuthDegrees{0.0f};  // Horizontal angle [-90..+90]
    float              elevationDegrees{0.0f};// Vertical angle [-45..+45]
    bool               userEngaged{true};     // Gaze / attention directed at display
    float              confidence{0.95f};     // Detection confidence [0.0..1.0]
    uint64_t           timestampUs{0};        // Sample timestamp
};

struct HumanPresencePolicy {
    bool     wakeOnApproachEnabled{true};
    bool     walkAwayLockEnabled{true};
    bool     adaptiveDimmingEnabled{true};
    bool     lookAwayDimEnabled{true};
    float    approachThresholdM{1.2f};    // Distance to trigger Wake on Approach (< 1.2m)
    float    leaveThresholdM{2.0f};       // Distance to trigger Walk Away Lock (> 2.0m)
    float    dimBrightnessFloor{0.20f};   // Lowest dim level (20% brightness)
    uint32_t walkAwayLockTimeoutSec{10};  // Seconds absent before workstation locks
    uint32_t dimTimeoutSec{3};            // Seconds unengaged before screen dims
};

// ============================================================================
// 2. Human Presence State Machine & Hysteresis Filter
// ============================================================================

class PresenceTracker {
private:
    HumanPresencePolicy m_policy;
    HumanPresenceState  m_currentState{HumanPresenceState::Unknown};
    float               m_currentDistance{0.0f};
    bool                m_isEngaged{false};
    float               m_lastDistance{0.0f};
    float               m_currentBrightness{1.0f}; // 1.0 = 100% normal brightness

    // Timers (in seconds)
    float               m_absentTimer{0.0f};
    float               m_unengagedTimer{0.0f};
    bool                m_isWorkstationLocked{false};
    bool                m_isSystemAwake{true};

public:
    PresenceTracker(const HumanPresencePolicy& policy = HumanPresencePolicy{})
        : m_policy(policy) {}

    void setPolicy(const HumanPresencePolicy& policy) noexcept { m_policy = policy; }
    [[nodiscard]] const HumanPresencePolicy& getPolicy() const noexcept { return m_policy; }

    [[nodiscard]] HumanPresenceState getState() const noexcept { return m_currentState; }
    [[nodiscard]] float getDistance() const noexcept { return m_currentDistance; }
    [[nodiscard]] bool isEngaged() const noexcept { return m_isEngaged; }
    [[nodiscard]] float getBrightnessFactor() const noexcept { return m_currentBrightness; }
    [[nodiscard]] bool isWorkstationLocked() const noexcept { return m_isWorkstationLocked; }
    [[nodiscard]] bool isSystemAwake() const noexcept { return m_isSystemAwake; }

    void unlockWorkstation() noexcept { m_isWorkstationLocked = false; }
    void lockWorkstation() noexcept { m_isWorkstationLocked = true; }

    // Ingest sensor report and advance state machine
    void processReading(const HpdSensorReport& report, float deltaSec = 0.1f) {
        m_currentDistance = report.distanceMeters;
        m_isEngaged = report.userEngaged;

        bool targetPresent = (report.confidence >= 0.5f && report.distanceMeters > 0.05f && report.distanceMeters < 5.0f);

        if (!targetPresent) {
            // Absent
            m_absentTimer += deltaSec;
            m_unengagedTimer += deltaSec;
            m_currentState = HumanPresenceState::NotPresent;

            // Adaptive Dimming
            if (m_policy.adaptiveDimmingEnabled) {
                m_currentBrightness = std::max(m_policy.dimBrightnessFloor, m_currentBrightness - (deltaSec * 0.4f));
            }

            // Walk-Away Lock Trigger
            if (m_policy.walkAwayLockEnabled && !m_isWorkstationLocked && m_absentTimer >= static_cast<float>(m_policy.walkAwayLockTimeoutSec)) {
                m_isWorkstationLocked = true;
                m_isSystemAwake = false;
            }
        } else {
            // Target detected
            m_absentTimer = 0.0f;

            // Distance velocity tracking
            float deltaDist = report.distanceMeters - m_lastDistance;

            if (report.distanceMeters <= m_policy.approachThresholdM) {
                // Inside Engaged Zone
                if (m_lastDistance > m_policy.approachThresholdM && m_policy.wakeOnApproachEnabled) {
                    // Wake on Approach Trigger!
                    m_isSystemAwake = true;
                }

                if (report.userEngaged) {
                    m_currentState = HumanPresenceState::Engaged;
                    m_unengagedTimer = 0.0f;
                    // Restore full brightness immediately
                    m_currentBrightness = 1.0f;
                } else {
                    m_currentState = HumanPresenceState::Unengaged;
                    m_unengagedTimer += deltaSec;
                    if (m_policy.lookAwayDimEnabled && m_unengagedTimer >= static_cast<float>(m_policy.dimTimeoutSec)) {
                        m_currentBrightness = std::max(m_policy.dimBrightnessFloor, m_currentBrightness - (deltaSec * 0.5f));
                    }
                }
            } else if (report.distanceMeters >= m_policy.leaveThresholdM) {
                m_currentState = (deltaDist > 0.05f) ? HumanPresenceState::Leaving : HumanPresenceState::Present;
                if (m_policy.adaptiveDimmingEnabled) {
                    m_currentBrightness = std::max(m_policy.dimBrightnessFloor, m_currentBrightness - (deltaSec * 0.3f));
                }
            } else {
                // Intermediate Zone (1.2m .. 2.0m)
                if (deltaDist < -0.05f) {
                    m_currentState = HumanPresenceState::Approaching;
                } else if (deltaDist > 0.05f) {
                    m_currentState = HumanPresenceState::Leaving;
                } else {
                    m_currentState = HumanPresenceState::Present;
                }
                m_currentBrightness = 1.0f;
            }
        }

        m_lastDistance = report.distanceMeters;
    }
};

// ============================================================================
// 3. Sovereign Windows Human Presence Subsystem Coordinator
// ============================================================================

class HumanPresenceSubsystem {
private:
    std::mutex                                      m_mutex;
    bool                                            m_subsystemEnabled{true};
    PresenceTracker                                 m_tracker;
    std::unordered_map<uint32_t, HpdSensorReport>   m_registeredSensors;
    uint32_t                                        m_nextSensorId{1};

    // Telemetry counters
    uint64_t                                        m_reportsProcessed{0};
    uint64_t                                        m_wakeOnApproachEvents{0};
    uint64_t                                        m_walkAwayLockEvents{0};
    uint64_t                                        m_adaptiveDimEvents{0};

    HumanPresenceSubsystem() = default;

public:
    static HumanPresenceSubsystem& get() {
        static HumanPresenceSubsystem instance;
        return instance;
    }

    void setSubsystemEnabled(bool enable) noexcept { m_subsystemEnabled = enable; }
    [[nodiscard]] bool isSubsystemEnabled() const noexcept { return m_subsystemEnabled; }

    uint32_t registerSensor(PresenceSensorType type) {
        std::lock_guard<std::mutex> lock(m_mutex);
        uint32_t id = m_nextSensorId++;
        HpdSensorReport initialReport{};
        initialReport.sensorId = id;
        initialReport.sensorType = type;
        m_registeredSensors[id] = initialReport;
        return id;
    }

    void setPolicy(const HumanPresencePolicy& policy) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_tracker.setPolicy(policy);
    }

    [[nodiscard]] HumanPresencePolicy getPolicy() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_tracker.getPolicy();
    }

    void processSensorReport(const HpdSensorReport& report, float deltaSec = 0.1f) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_reportsProcessed++;
        if (!m_subsystemEnabled) return;

        bool prevLocked = m_tracker.isWorkstationLocked();
        bool prevAwake  = m_tracker.isSystemAwake();
        float prevBright = m_tracker.getBrightnessFactor();

        m_tracker.processReading(report, deltaSec);

        if (!prevLocked && m_tracker.isWorkstationLocked()) {
            m_walkAwayLockEvents++;
        }
        if (!prevAwake && m_tracker.isSystemAwake()) {
            m_wakeOnApproachEvents++;
        }
        if (m_tracker.getBrightnessFactor() < prevBright && m_tracker.getBrightnessFactor() <= 0.5f) {
            m_adaptiveDimEvents++;
        }
    }

    [[nodiscard]] HumanPresenceState getCurrentState() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_tracker.getState();
    }

    [[nodiscard]] float getCurrentDistance() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_tracker.getDistance();
    }

    [[nodiscard]] bool isUserEngaged() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_tracker.isEngaged();
    }

    [[nodiscard]] float getBrightnessFactor() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_tracker.getBrightnessFactor();
    }

    [[nodiscard]] bool isWorkstationLocked() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_tracker.isWorkstationLocked();
    }

    [[nodiscard]] bool isSystemAwake() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_tracker.isSystemAwake();
    }

    void unlockWorkstation() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_tracker.unlockWorkstation();
    }

    [[nodiscard]] size_t getSensorCount() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_mutex));
        return m_registeredSensors.size();
    }

    [[nodiscard]] uint64_t getReportsProcessed() const noexcept { return m_reportsProcessed; }
    [[nodiscard]] uint64_t getWakeOnApproachEvents() const noexcept { return m_wakeOnApproachEvents; }
    [[nodiscard]] uint64_t getWalkAwayLockEvents() const noexcept { return m_walkAwayLockEvents; }
    [[nodiscard]] uint64_t getAdaptiveDimEvents() const noexcept { return m_adaptiveDimEvents; }
};

// ============================================================================
// 4. Win32 & NT Clean-Room Dynamic C ABI Parity Exports
// ============================================================================

extern "C" {

inline NTSTATUS RegisterHumanPresenceSensor(uint32_t* pSensorId) {
    if (!pSensorId) return micant::STATUS_INVALID_PARAMETER;
    *pSensorId = HumanPresenceSubsystem::get().registerSensor(PresenceSensorType::RadarMmWave);
    return micant::STATUS_SUCCESS;
}

inline NTSTATUS HpdProcessSensorReading(uint32_t sensorId, const HpdSensorReport* pReport) {
    if (!pReport) return micant::STATUS_INVALID_PARAMETER;
    HpdSensorReport r = *pReport;
    r.sensorId = sensorId;
    HumanPresenceSubsystem::get().processSensorReport(r);
    return micant::STATUS_SUCCESS;
}

inline NTSTATUS HpdGetPresenceState(uint32_t* pState, float* pDistanceMeters, uint32_t* pEngaged) {
    if (!pState || !pDistanceMeters || !pEngaged) return micant::STATUS_INVALID_PARAMETER;
    *pState = static_cast<uint32_t>(HumanPresenceSubsystem::get().getCurrentState());
    *pDistanceMeters = HumanPresenceSubsystem::get().getCurrentDistance();
    *pEngaged = HumanPresenceSubsystem::get().isUserEngaged() ? 1 : 0;
    return micant::STATUS_SUCCESS;
}

inline NTSTATUS HpdConfigurePolicy(const HumanPresencePolicy* pPolicy) {
    if (!pPolicy) return micant::STATUS_INVALID_PARAMETER;
    HumanPresenceSubsystem::get().setPolicy(*pPolicy);
    return micant::STATUS_SUCCESS;
}

inline NTSTATUS HpdGetPolicy(HumanPresencePolicy* pPolicy) {
    if (!pPolicy) return micant::STATUS_INVALID_PARAMETER;
    *pPolicy = HumanPresenceSubsystem::get().getPolicy();
    return micant::STATUS_SUCCESS;
}

inline NTSTATUS HpdGetDisplayBrightnessFactor(float* pFactor) {
    if (!pFactor) return micant::STATUS_INVALID_PARAMETER;
    *pFactor = HumanPresenceSubsystem::get().getBrightnessFactor();
    return micant::STATUS_SUCCESS;
}

} // extern "C"

// ============================================================================
// 5. Subsystem Registration Helper
// ============================================================================

inline void RegisterHpdSubsystem() {
    auto& vdb = micant::version::VersionDatabase::Instance();
    vdb.RegisterModule("sensrsvc.dll", "10.0.26100.1", "Windows Sensor & Human Presence Detection Service (TitanPresence)");
    vdb.RegisterModule("hpd.sys", "10.0.26100.1", "Windows Human Presence Detection Sensor Port Driver");

    auto& scm = micant::scm::ServiceControlManager::get();
    auto rec = std::make_shared<micant::scm::ServiceRecord>();
    rec->serviceName = L"SensorService";
    rec->displayName = L"Sensor Service & Human Presence Sensing";
    rec->serviceType = micant::scm::SERVICE_WIN32_SHARE_PROCESS;
    rec->startType = micant::scm::SERVICE_AUTO_START;
    rec->binaryPath = L"C:\\Windows\\System32\\svchost.exe -k SensorGroup";
    rec->status.dwCurrentState = micant::scm::SERVICE_RUNNING;
    scm.registerServiceRecord(rec);
}

} // namespace micant::hpd

#endif // MICANT_HPD_HPP
