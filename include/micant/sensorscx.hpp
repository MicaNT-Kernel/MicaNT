/**
 * @file sensorscx.hpp
 * @brief Windows Sensor Class Extension v2 (SensorsCx / sensorscx.sys) & 9-DoF Sensor Fusion Subsystem
 *
 * MicaNT Dave Cutler Clean-Room Architecture
 * Codename: TitanSensorFusion / AegisOrientation
 * Specification Reference: WDK Sensor Class Extension v2 (SensorsCx.sys),
 *                          Windows Sensor API, sensrsvc.dll, and Madgwick AHRS 9-DoF Fusion.
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <array>
#include <memory>
#include <mutex>
#include <atomic>
#include <cmath>
#include <algorithm>
#include <functional>
#include <chrono>
#include <sstream>
#include <iomanip>

#include "ntstatus.hpp"
#include "scm.hpp"
#include "version.hpp"

namespace micant::sensorscx {

// Sensor category / type definitions matching Windows 11 SensorsCx v2
enum class SensorType : uint32_t {
    Accelerometer3D = 0,
    Gyrometer3D     = 1,
    Magnetometer3D  = 2,
    Inclinometer3D  = 3,
    Barometer       = 4,
    AmbientLight    = 5,
    OrientationFusion = 6,
    Pedometer       = 7
};

inline const char* SensorTypeToString(SensorType type) {
    switch (type) {
        case SensorType::Accelerometer3D:  return "Accelerometer 3D (G)";
        case SensorType::Gyrometer3D:      return "Gyrometer 3D (deg/s)";
        case SensorType::Magnetometer3D:   return "Magnetometer 3D (uT)";
        case SensorType::Inclinometer3D:   return "Inclinometer 3D (deg)";
        case SensorType::Barometer:        return "Barometer / Altimeter (hPa)";
        case SensorType::AmbientLight:     return "Ambient Light Sensor (Lux)";
        case SensorType::OrientationFusion:return "9-DoF AHRS Fusion (Quaternion)";
        case SensorType::Pedometer:        return "Pedometer / Step Counter";
        default:                           return "Unknown Sensor";
    }
}

// Sensor lifecycle & power states
enum class SensorPowerState : uint32_t {
    PowerOff    = 0,
    Standby     = 1,
    LowPower    = 2,
    Active      = 3
};

// Display Screen Auto-Rotation Orientations
enum class DisplayOrientation : uint32_t {
    Landscape        = 0,  // 0 degrees normal
    Portrait         = 1,  // 90 degrees clockwise
    LandscapeFlipped = 2,  // 180 degrees inverted
    PortraitFlipped  = 3,  // 270 degrees counter-clockwise
    FaceUp           = 4,  // Lying flat on surface, display up
    FaceDown         = 5   // Lying flat on surface, display down
};

inline const char* DisplayOrientationToString(DisplayOrientation orient) {
    switch (orient) {
        case DisplayOrientation::Landscape:        return "Landscape (0 deg)";
        case DisplayOrientation::Portrait:         return "Portrait (90 deg)";
        case DisplayOrientation::LandscapeFlipped: return "Landscape Flipped (180 deg)";
        case DisplayOrientation::PortraitFlipped:  return "Portrait Flipped (270 deg)";
        case DisplayOrientation::FaceUp:           return "Face Up (Flat)";
        case DisplayOrientation::FaceDown:         return "Face Down (Flat)";
        default:                                   return "Unknown Orientation";
    }
}

// 3D Vector mathematical structure
struct Vector3f {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    Vector3f() = default;
    constexpr Vector3f(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    float lengthSquared() const { return x * x + y * y + z * z; }
    float length() const { return std::sqrt(lengthSquared()); }

    Vector3f normalized() const {
        float len = length();
        if (len > 1e-6f) {
            float inv = 1.0f / len;
            return { x * inv, y * inv, z * inv };
        }
        return { 0.0f, 0.0f, 0.0f };
    }

    float dot(const Vector3f& other) const {
        return x * other.x + y * other.y + z * other.z;
    }

    Vector3f cross(const Vector3f& other) const {
        return {
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        };
    }

    Vector3f operator+(const Vector3f& o) const { return { x + o.x, y + o.y, z + o.z }; }
    Vector3f operator-(const Vector3f& o) const { return { x - o.x, y - o.y, z - o.z }; }
    Vector3f operator*(float scalar) const { return { x * scalar, y * scalar, z * scalar }; }
};

// 4D Quaternion for 3D rotation representation (W, X, Y, Z)
struct Quaternion {
    float w{1.0f};
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    Quaternion() = default;
    constexpr Quaternion(float w_, float x_, float y_, float z_) : w(w_), x(x_), y(y_), z(z_) {}

    void normalize() {
        float norm = std::sqrt(w * w + x * x + y * y + z * z);
        if (norm > 1e-6f) {
            float inv = 1.0f / norm;
            w *= inv;
            x *= inv;
            y *= inv;
            z *= inv;
        } else {
            w = 1.0f; x = 0.0f; y = 0.0f; z = 0.0f;
        }
    }

    // Convert Quaternion to Euler angles (in degrees): Pitch (X), Roll (Y), Yaw (Z)
    void toEulerAngles(float& pitchDeg, float& rollDeg, float& yawDeg) const {
        // Roll (x-axis rotation)
        float sinr_cosp = 2.0f * (w * x + y * z);
        float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
        rollDeg = std::atan2(sinr_cosp, cosr_cosp) * (180.0f / 3.14159265358979323846f);

        // Pitch (y-axis rotation)
        float sinp = 2.0f * (w * y - z * x);
        if (std::abs(sinp) >= 1.0f) {
            pitchDeg = std::copysign(90.0f, sinp); // Use 90 degrees if out of range
        } else {
            pitchDeg = std::asin(sinp) * (180.0f / 3.14159265358979323846f);
        }

        // Yaw (z-axis rotation / Heading)
        float siny_cosp = 2.0f * (w * z + x * y);
        float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
        yawDeg = std::atan2(siny_cosp, cosy_cosp) * (180.0f / 3.14159265358979323846f);
        if (yawDeg < 0.0f) yawDeg += 360.0f;
    }
};

// Unified Sensor Sample Data Struct
struct SensorReading {
    SensorType sensorType{SensorType::Accelerometer3D};
    uint64_t timestampUs{0};
    uint32_t sequenceNumber{0};
    std::array<float, 6> values{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    // Helpers
    Vector3f asVector3() const {
        return { values[0], values[1], values[2] };
    }
    float asScalar() const {
        return values[0];
    }
};

// Gesture Event Types
enum class SensorGestureType : uint32_t {
    None        = 0,
    FreeFall    = 1,
    Shake       = 2,
    Tilt        = 3,
    Flip        = 4,
    Step        = 5
};

inline const char* SensorGestureToString(SensorGestureType gesture) {
    switch (gesture) {
        case SensorGestureType::FreeFall: return "Free-Fall Detected (Near 0g Weightlessness)";
        case SensorGestureType::Shake:    return "Shake Gesture Detected";
        case SensorGestureType::Tilt:     return "Significant Tilt Movement";
        case SensorGestureType::Flip:     return "Device Flipped (Face Down/Up)";
        case SensorGestureType::Step:     return "Pedometer Step Detected";
        default:                          return "No Gesture";
    }
}

// ----------------------------------------------------------------------------
// 9-DoF Madgwick AHRS Attitude Estimation Algorithm
// ----------------------------------------------------------------------------
class MadgwickAhrsFilter {
public:
    MadgwickAhrsFilter(float beta = 0.1f, float samplePeriodSec = 0.01f)
        : m_beta(beta), m_samplePeriod(samplePeriodSec) {}

    void setBeta(float beta) { m_beta = beta; }
    void setSamplePeriod(float dt) { m_samplePeriod = dt; }

    Quaternion getOrientation() const { return m_q; }

    // Update with 9-DoF: Accelerometer (G), Gyroscope (rad/s or deg/s), Magnetometer (uT)
    void update9DoF(float gx, float gy, float gz,
                    float ax, float ay, float az,
                    float mx, float my, float mz) {
        // Convert deg/s to rad/s if needed (assume input gyro is deg/s, convert to rad/s)
        constexpr float degToRad = 3.14159265358979323846f / 180.0f;
        gx *= degToRad;
        gy *= degToRad;
        gz *= degToRad;

        float q1 = m_q.w, q2 = m_q.x, q3 = m_q.y, q4 = m_q.z;
        float norm;
        float hx, hy, bx, bz;
        float s1, s2, s3, s4;
        float qDot1, qDot2, qDot3, qDot4;

        // Auxiliary variables to avoid repeated calculations
        float _2q1mx;
        float _2q1my;
        float _2q1mz;
        float _2q2mx;
        float _4bx;
        float _4bz;
        float _2q1 = 2.0f * q1;
        float _2q2 = 2.0f * q2;
        float _2q3 = 2.0f * q3;
        float _2q4 = 2.0f * q4;
        float _2q1q3 = 2.0f * q1 * q3;
        float _2q3q4 = 2.0f * q3 * q4;
        float q1q1 = q1 * q1;
        float q1q2 = q1 * q2;
        float q1q3 = q1 * q3;
        float q1q4 = q1 * q4;
        float q2q2 = q2 * q2;
        float q2q3 = q2 * q3;
        float q2q4 = q2 * q4;
        float q3q3 = q3 * q3;
        float q3q4 = q3 * q4;
        float q4q4 = q4 * q4;

        // Rate of change of quaternion from gyroscope
        qDot1 = 0.5f * (-q2 * gx - q3 * gy - q4 * gz);
        qDot2 = 0.5f * ( q1 * gx + q3 * gz - q4 * gy);
        qDot3 = 0.5f * ( q1 * gy - q2 * gz + q4 * gx);
        qDot4 = 0.5f * ( q1 * gz + q2 * gy - q3 * gx);

        // Normalize accelerometer measurement
        norm = std::sqrt(ax * ax + ay * ay + az * az);
        if (norm > 1e-4f) {
            norm = 1.0f / norm;
            ax *= norm;
            ay *= norm;
            az *= norm;

            // Normalize magnetometer measurement
            norm = std::sqrt(mx * mx + my * my + mz * mz);
            if (norm > 1e-4f) {
                norm = 1.0f / norm;
                mx *= norm;
                my *= norm;
                mz *= norm;

                // Reference direction of Earth's magnetic field
                _2q1mx = 2.0f * q1 * mx;
                _2q1my = 2.0f * q1 * my;
                _2q1mz = 2.0f * q1 * mz;
                _2q2mx = 2.0f * q2 * mx;
                hx = mx * q1q1 - _2q1my * q4 + _2q1mz * q3 + mx * q2q2 + _2q2 * my * q3 + _2q2 * mz * q4 - mx * q3q3 - mx * q4q4;
                hy = _2q1mx * q4 + my * q1q1 - _2q1mz * q2 + _2q2mx * q3 - my * q2q2 + my * q3q3 + _2q3 * mz * q4 - my * q4q4;
                _2q1mx = 2.0f * q1 * hx;
                _2q1my = 2.0f * q1 * hy;
                bx = std::sqrt(hx * hx + hy * hy);
                bz = -_2q1mx * q3 + _2q1my * q2 + mz * q1q1 + _2q2mx * q4 - mz * q2q2 + _2q3 * my * q4 - mz * q3q3 + mz * q4q4;
                _4bx = 2.0f * bx;
                _4bz = 2.0f * bz;

                // Gradient descent algorithm corrective step
                s1 = -_2q3 * (2.0f * q2q4 - _2q1q3 - ax) + _2q2 * (2.0f * q1q2 + _2q3q4 - ay) - bz * q3 * (bx * (0.5f - q3q3 - q4q4) + bz * (q2q4 - q1q3) - mx) + (-bx * q4 + bz * q2) * (bx * (q2q3 - q1q4) + bz * (q1q2 + q3q4) - my) + bx * q3 * (bx * (q1q3 + q2q4) + bz * (0.5f - q2q2 - q3q3) - mz);
                s2 = _2q4 * (2.0f * q2q4 - _2q1q3 - ax) + _2q1 * (2.0f * q1q2 + _2q3q4 - ay) - 4.0f * q2 * (1.0f - 2.0f * q2q2 - 2.0f * q3q3 - az) + bz * q4 * (bx * (0.5f - q3q3 - q4q4) + bz * (q2q4 - q1q3) - mx) + (bx * q3 + bz * q1) * (bx * (q2q3 - q1q4) + bz * (q1q2 + q3q4) - my) + (bx * q4 - _4bz * q2) * (bx * (q1q3 + q2q4) + bz * (0.5f - q2q2 - q3q3) - mz);
                s3 = -_2q1 * (2.0f * q2q4 - _2q1q3 - ax) + _2q4 * (2.0f * q1q2 + _2q3q4 - ay) - 4.0f * q3 * (1.0f - 2.0f * q2q2 - 2.0f * q3q3 - az) + (-_4bx * q3 - bz * q1) * (bx * (0.5f - q3q3 - q4q4) + bz * (q2q4 - q1q3) - mx) + (bx * q2 + bz * q4) * (bx * (q2q3 - q1q4) + bz * (q1q2 + q3q4) - my) + (bx * q1 - _4bz * q3) * (bx * (q1q3 + q2q4) + bz * (0.5f - q2q2 - q3q3) - mz);
                s4 = _2q2 * (2.0f * q2q4 - _2q1q3 - ax) + _2q3 * (2.0f * q1q2 + _2q3q4 - ay) + (-_4bx * q4 + bz * q2) * (bx * (0.5f - q3q3 - q4q4) + bz * (q2q4 - q1q3) - mx) + (-bx * q1 + bz * q3) * (bx * (q2q3 - q1q4) + bz * (q1q2 + q3q4) - my) + bx * q2 * (bx * (q1q3 + q2q4) + bz * (0.5f - q2q2 - q3q3) - mz);

                norm = std::sqrt(s1 * s1 + s2 * s2 + s3 * s3 + s4 * s4);
                if (norm > 1e-6f) {
                    norm = 1.0f / norm;
                    s1 *= norm;
                    s2 *= norm;
                    s3 *= norm;
                    s4 *= norm;

                    // Apply feedback step
                    qDot1 -= m_beta * s1;
                    qDot2 -= m_beta * s2;
                    qDot3 -= m_beta * s3;
                    qDot4 -= m_beta * s4;
                }
            } else {
                // 6-DoF fallback (IMU only: Accel + Gyro)
                s1 = _2q3 * (2.0f * (q2 * q4 - q1 * q3) - ax) - _2q2 * (2.0f * (q1 * q2 + q3 * q4) - ay);
                s2 = _2q4 * (2.0f * (q2 * q4 - q1 * q3) - ax) + _2q1 * (2.0f * (q1 * q2 + q3 * q4) - ay) - 4.0f * q2 * (1.0f - 2.0f * (q2 * q2 + q3 * q3) - az);
                s3 = -_2q1 * (2.0f * (q2 * q4 - q1 * q3) - ax) + _2q4 * (2.0f * (q1 * q2 + q3 * q4) - ay) - 4.0f * q3 * (1.0f - 2.0f * (q2 * q2 + q3 * q3) - az);
                s4 = _2q2 * (2.0f * (q2 * q4 - q1 * q3) - ax) + _2q3 * (2.0f * (q1 * q2 + q3 * q4) - ay);
                norm = std::sqrt(s1 * s1 + s2 * s2 + s3 * s3 + s4 * s4);
                if (norm > 1e-6f) {
                    norm = 1.0f / norm;
                    qDot1 -= m_beta * (s1 * norm);
                    qDot2 -= m_beta * (s2 * norm);
                    qDot3 -= m_beta * (s3 * norm);
                    qDot4 -= m_beta * (s4 * norm);
                }
            }
        }

        // Integrate rate of change of quaternion to yield quaternion
        q1 += qDot1 * m_samplePeriod;
        q2 += qDot2 * m_samplePeriod;
        q3 += qDot3 * m_samplePeriod;
        q4 += qDot4 * m_samplePeriod;

        // Normalise quaternion
        norm = std::sqrt(q1 * q1 + q2 * q2 + q3 * q3 + q4 * q4);
        if (norm > 1e-6f) {
            norm = 1.0f / norm;
            m_q.w = q1 * norm;
            m_q.x = q2 * norm;
            m_q.y = q3 * norm;
            m_q.z = q4 * norm;
        }
    }

    void reset() {
        m_q = Quaternion(1.0f, 0.0f, 0.0f, 0.0f);
    }

private:
    float m_beta{0.1f};
    float m_samplePeriod{0.01f}; // Default 100Hz = 10ms
    Quaternion m_q{1.0f, 0.0f, 0.0f, 0.0f};
};

// ----------------------------------------------------------------------------
// Individual Sensor Endpoint Representation
// ----------------------------------------------------------------------------
class SensorEndpoint {
public:
    SensorEndpoint(uint32_t sensorId, SensorType type, std::string friendlyName)
        : m_sensorId(sensorId), m_type(type), m_friendlyName(std::move(friendlyName)) {
        m_lastReading.sensorType = type;
    }

    uint32_t getId() const { return m_sensorId; }
    SensorType getType() const { return m_type; }
    const std::string& getName() const { return m_friendlyName; }

    SensorPowerState getPowerState() const { return m_powerState; }
    void setPowerState(SensorPowerState state) { m_powerState = state; }

    uint32_t getReportIntervalMs() const { return m_reportIntervalMs; }
    void setReportIntervalMs(uint32_t ms) { m_reportIntervalMs = std::max(1u, ms); }

    // Dynamic Hardware Batching FIFO
    void enableBatching(size_t maxCapacity = 64) {
        m_batchingEnabled = true;
        m_batchCapacity = maxCapacity;
    }
    void disableBatching() {
        m_batchingEnabled = false;
        m_batchFifo.clear();
    }

    // Ingest data reading
    void pushReading(const SensorReading& reading) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastReading = reading;
        m_sampleCount++;

        if (m_batchingEnabled) {
            if (m_batchFifo.size() >= m_batchCapacity) {
                m_batchFifo.erase(m_batchFifo.begin()); // FIFO eviction
            }
            m_batchFifo.push_back(reading);
        }
    }

    SensorReading getLastReading() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_lastReading;
    }

    std::vector<SensorReading> flushBatch() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<SensorReading> batch = std::move(m_batchFifo);
        m_batchFifo.clear();
        return batch;
    }

    uint64_t getSampleCount() const { return m_sampleCount; }

private:
    uint32_t m_sensorId{0};
    SensorType m_type{SensorType::Accelerometer3D};
    std::string m_friendlyName;
    SensorPowerState m_powerState{SensorPowerState::Active};
    uint32_t m_reportIntervalMs{10}; // 100 Hz default
    bool m_batchingEnabled{false};
    size_t m_batchCapacity{64};
    std::vector<SensorReading> m_batchFifo;
    SensorReading m_lastReading;
    uint64_t m_sampleCount{0};
    mutable std::mutex m_mutex;
};

// ----------------------------------------------------------------------------
// Display Auto-Rotation & Motion Gesture Recognition Engine
// ----------------------------------------------------------------------------
class SensorGestureDetector {
public:
    SensorGestureDetector() = default;

    // Evaluate gravity vector to compute display orientation with hysteresis
    DisplayOrientation updateOrientation(const Vector3f& accelG) {
        float ax = accelG.x;
        float ay = accelG.y;
        float az = accelG.z;

        // Check for face up / face down
        if (az > 0.85f) {
            m_currentOrientation = DisplayOrientation::FaceUp;
            return m_currentOrientation;
        } else if (az < -0.85f) {
            m_currentOrientation = DisplayOrientation::FaceDown;
            return m_currentOrientation;
        }

        // Hysteresis deadband in tilt angle:
        // Angle in XY plane
        float angleRad = std::atan2(ax, -ay); // 0 at normal portrait (ax=0, ay=-1)
        float angleDeg = angleRad * (180.0f / 3.14159265358979323846f);
        if (angleDeg < 0.0f) angleDeg += 360.0f;

        // Determine target orientation candidate:
        // Landscape (90 deg), Portrait (0 deg), PortraitFlipped (180 deg), LandscapeFlipped (270 deg)
        // With standard Windows 15-degree deadband guard
        constexpr float deadband = 15.0f;
        DisplayOrientation candidate = m_currentOrientation;

        if (angleDeg >= (45.0f + deadband) && angleDeg <= (135.0f - deadband)) {
            candidate = DisplayOrientation::Landscape; // Left tilt -> Landscape
        } else if (angleDeg >= (135.0f + deadband) && angleDeg <= (225.0f - deadband)) {
            candidate = DisplayOrientation::PortraitFlipped; // Inverted
        } else if (angleDeg >= (225.0f + deadband) && angleDeg <= (315.0f - deadband)) {
            candidate = DisplayOrientation::LandscapeFlipped; // Right tilt -> Landscape flipped
        } else if (angleDeg >= (315.0f + deadband) || angleDeg <= (45.0f - deadband)) {
            candidate = DisplayOrientation::Portrait; // Upright
        }

        if (candidate != m_currentOrientation) {
            m_currentOrientation = candidate;
            m_orientationChangeCount++;
        }

        return m_currentOrientation;
    }

    // Free-fall and Shake detection
    SensorGestureType detectGesture(const Vector3f& accelG) {
        float magnitude = accelG.length();

        // 1. Free-Fall: vector magnitude drops close to 0G (weightless drop)
        if (magnitude < 0.25f) {
            m_freeFallCount++;
            return SensorGestureType::FreeFall;
        }

        // 2. Shake: acceleration variance/magnitude exceeds 2.5G
        if (magnitude > 2.5f) {
            m_shakeCount++;
            return SensorGestureType::Shake;
        }

        return SensorGestureType::None;
    }

    // Step Pedometer counting using peak-valley zero-crossing detection
    bool processStep(float magnitude) {
        constexpr float stepThreshold = 1.15f; // Step acceleration threshold
        bool isStep = false;
        if (!m_inPeak && magnitude > stepThreshold) {
            m_inPeak = true;
            m_stepCount++;
            isStep = true;
        } else if (m_inPeak && magnitude < 0.95f) {
            m_inPeak = false;
        }
        return isStep;
    }

    DisplayOrientation getOrientation() const { return m_currentOrientation; }
    uint32_t getOrientationChangeCount() const { return m_orientationChangeCount; }
    uint32_t getFreeFallCount() const { return m_freeFallCount; }
    uint32_t getShakeCount() const { return m_shakeCount; }
    uint32_t getStepCount() const { return m_stepCount; }

    void reset() {
        m_currentOrientation = DisplayOrientation::Landscape;
        m_orientationChangeCount = 0;
        m_freeFallCount = 0;
        m_shakeCount = 0;
        m_stepCount = 0;
        m_inPeak = false;
    }

private:
    DisplayOrientation m_currentOrientation{DisplayOrientation::Landscape};
    uint32_t m_orientationChangeCount{0};
    uint32_t m_freeFallCount{0};
    uint32_t m_shakeCount{0};
    uint32_t m_stepCount{0};
    bool m_inPeak{false};
};

// ----------------------------------------------------------------------------
// SensorsCx Core Subsystem Singleton (TitanSensorFusion / AegisOrientation)
// ----------------------------------------------------------------------------
class SensorsCxSubsystem {
public:
    static SensorsCxSubsystem& get() {
        static SensorsCxSubsystem instance;
        return instance;
    }

    bool initialize() {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_initialized) return true;

        // Register default hardware platform sensors
        m_sensors.clear();
        m_sensors.push_back(std::make_shared<SensorEndpoint>(1, SensorType::Accelerometer3D, "STMicroelectronics LSM6DSOX 3-Axis Accelerometer"));
        m_sensors.push_back(std::make_shared<SensorEndpoint>(2, SensorType::Gyrometer3D, "STMicroelectronics LSM6DSOX 3-Axis Gyroscope"));
        m_sensors.push_back(std::make_shared<SensorEndpoint>(3, SensorType::Magnetometer3D, "Bosch BMM150 3-Axis Geomagnetic Compass"));
        m_sensors.push_back(std::make_shared<SensorEndpoint>(4, SensorType::Barometer, "Bosch BMP390 High-Precision Barometric Pressure Sensor"));
        m_sensors.push_back(std::make_shared<SensorEndpoint>(5, SensorType::AmbientLight, "Texas Instruments OPT3001 Ambient Light & Lux Sensor"));
        m_sensors.push_back(std::make_shared<SensorEndpoint>(6, SensorType::OrientationFusion, "MicaNT 9-DoF AHRS Fusion Synthetic Sensor"));
        m_sensors.push_back(std::make_shared<SensorEndpoint>(7, SensorType::Pedometer, "Hardware IMU Step Detector & Counter"));

        // Seed default resting state readings (1G down on Z)
        SensorReading accelInit{};
        accelInit.sensorType = SensorType::Accelerometer3D;
        accelInit.values = { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f }; // 1.0G on Z
        m_sensors[0]->pushReading(accelInit);

        SensorReading gyroInit{};
        gyroInit.sensorType = SensorType::Gyrometer3D;
        gyroInit.values = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        m_sensors[1]->pushReading(gyroInit);

        SensorReading magInit{};
        magInit.sensorType = SensorType::Magnetometer3D;
        magInit.values = { 20.0f, 0.0f, 40.0f, 0.0f, 0.0f, 0.0f }; // Typical terrestrial magnetic flux
        m_sensors[2]->pushReading(magInit);

        SensorReading baroInit{};
        baroInit.sensorType = SensorType::Barometer;
        baroInit.values = { 1013.25f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }; // 1 ATM = 1013.25 hPa
        m_sensors[3]->pushReading(baroInit);

        SensorReading alsInit{};
        alsInit.sensorType = SensorType::AmbientLight;
        alsInit.values = { 350.0f, 5000.0f, 0.0f, 0.0f, 0.0f, 0.0f }; // 350 Lux, 5000K daylight
        m_sensors[4]->pushReading(alsInit);

        m_ahrsFilter.reset();
        m_gestureDetector.reset();

        m_initialized = true;
        return true;
    }

    bool isInitialized() const { return m_initialized; }

    std::shared_ptr<SensorEndpoint> getSensor(uint32_t sensorId) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        for (auto& s : m_sensors) {
            if (s->getId() == sensorId) return s;
        }
        return nullptr;
    }

    std::shared_ptr<SensorEndpoint> getSensorByType(SensorType type) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        for (auto& s : m_sensors) {
            if (s->getType() == type) return s;
        }
        return nullptr;
    }

    std::vector<std::shared_ptr<SensorEndpoint>> getAllSensors() const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_sensors;
    }

    uint32_t registerSensor(SensorType type, const std::string& name) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        uint32_t newId = static_cast<uint32_t>(m_sensors.size()) + 1;
        auto ep = std::make_shared<SensorEndpoint>(newId, type, name);
        m_sensors.push_back(ep);
        return newId;
    }

    // Ingest sample into sensor endpoint and run fusion / gesture pipeline
    bool injectReading(uint32_t sensorId, const std::array<float, 6>& values) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        for (auto& s : m_sensors) {
            if (s->getId() == sensorId) {
                SensorReading r{};
                r.sensorType = s->getType();
                r.timestampUs = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();
                r.sequenceNumber = static_cast<uint32_t>(s->getSampleCount() + 1);
                r.values = values;
                s->pushReading(r);

                // Run real-time processing triggers
                if (s->getType() == SensorType::Accelerometer3D) {
                    Vector3f accel(values[0], values[1], values[2]);
                    m_gestureDetector.updateOrientation(accel);
                    m_gestureDetector.detectGesture(accel);
                    m_gestureDetector.processStep(accel.length());
                }

                m_totalReadingsProcessed++;
                return true;
            }
        }
        return false;
    }

    // Run a step of 9-DoF sensor fusion
    void processFusionStep() {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto acc = getSensorByType(SensorType::Accelerometer3D);
        auto gyr = getSensorByType(SensorType::Gyrometer3D);
        auto mag = getSensorByType(SensorType::Magnetometer3D);

        if (!acc || !gyr || !mag) return;

        auto accR = acc->getLastReading();
        auto gyrR = gyr->getLastReading();
        auto magR = mag->getLastReading();

        m_ahrsFilter.update9DoF(
            gyrR.values[0], gyrR.values[1], gyrR.values[2],
            accR.values[0], accR.values[1], accR.values[2],
            magR.values[0], magR.values[1], magR.values[2]
        );

        Quaternion q = m_ahrsFilter.getOrientation();
        float pitch = 0.0f, roll = 0.0f, yaw = 0.0f;
        q.toEulerAngles(pitch, roll, yaw);

        // Update synthetic fusion sensor endpoint
        auto fusion = getSensorByType(SensorType::OrientationFusion);
        if (fusion) {
            SensorReading fr{};
            fr.sensorType = SensorType::OrientationFusion;
            fr.values = { q.w, q.x, q.y, q.z, pitch, roll };
            fusion->pushReading(fr);
        }
    }

    Quaternion getCurrentOrientationQuaternion() const {
        return m_ahrsFilter.getOrientation();
    }

    void getCurrentEulerAngles(float& pitch, float& roll, float& yaw) const {
        Quaternion q = m_ahrsFilter.getOrientation();
        q.toEulerAngles(pitch, roll, yaw);
    }

    DisplayOrientation getDisplayOrientation() const {
        return m_gestureDetector.getOrientation();
    }

    bool isOrientationLocked() const { return m_orientationLocked; }
    void setOrientationLock(bool locked) { m_orientationLocked = locked; }

    const SensorGestureDetector& getGestureDetector() const { return m_gestureDetector; }
    uint64_t getTotalReadings() const { return m_totalReadingsProcessed; }

    void reset() {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_ahrsFilter.reset();
        m_gestureDetector.reset();
        m_totalReadingsProcessed = 0;
        m_orientationLocked = false;
    }

private:
    SensorsCxSubsystem() = default;

    bool m_initialized{false};
    bool m_orientationLocked{false};
    std::vector<std::shared_ptr<SensorEndpoint>> m_sensors;
    MadgwickAhrsFilter m_ahrsFilter;
    SensorGestureDetector m_gestureDetector;
    std::atomic<uint64_t> m_totalReadingsProcessed{0};
    mutable std::recursive_mutex m_mutex;
};

// ----------------------------------------------------------------------------
// Win32 & NT Clean-Room C ABI Parity Exports (sensorscx.sys, sensrsvc.dll)
// ----------------------------------------------------------------------------
extern "C" {

inline NTSTATUS SensorsCxDeviceInitialize(void* /*deviceContext*/) {
    if (SensorsCxSubsystem::get().initialize()) {
        return micant::STATUS_SUCCESS;
    }
    return micant::STATUS_UNSUCCESSFUL;
}

inline NTSTATUS SensorsCxSensorCreate(uint32_t sensorType, uint32_t* pSensorId) {
    if (!pSensorId || sensorType > 7) {
        return micant::STATUS_INVALID_PARAMETER;
    }
    uint32_t id = SensorsCxSubsystem::get().registerSensor(
        static_cast<SensorType>(sensorType), "Custom SensorsCx Peripheral");
    *pSensorId = id;
    return micant::STATUS_SUCCESS;
}

inline NTSTATUS SensorsCxSensorStart(uint32_t sensorId) {
    auto sensor = SensorsCxSubsystem::get().getSensor(sensorId);
    if (!sensor) return micant::STATUS_NOT_FOUND;
    sensor->setPowerState(SensorPowerState::Active);
    return micant::STATUS_SUCCESS;
}

inline NTSTATUS SensorsCxSensorStop(uint32_t sensorId) {
    auto sensor = SensorsCxSubsystem::get().getSensor(sensorId);
    if (!sensor) return micant::STATUS_NOT_FOUND;
    sensor->setPowerState(SensorPowerState::Standby);
    return micant::STATUS_SUCCESS;
}

inline NTSTATUS SensorsCxSensorDataReady(uint32_t sensorId, const float* data, uint32_t count) {
    if (!data || count == 0 || count > 6) {
        return micant::STATUS_INVALID_PARAMETER;
    }
    std::array<float, 6> vals{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    for (uint32_t i = 0; i < count; ++i) {
        vals[i] = data[i];
    }
    if (SensorsCxSubsystem::get().injectReading(sensorId, vals)) {
        return micant::STATUS_SUCCESS;
    }
    return micant::STATUS_NOT_FOUND;
}

inline NTSTATUS SensorsCxGetDeviceOrientation(uint32_t* pOrientation, float* pPitch, float* pRoll, float* pYaw) {
    if (!pOrientation) return micant::STATUS_INVALID_PARAMETER;
    *pOrientation = static_cast<uint32_t>(SensorsCxSubsystem::get().getDisplayOrientation());
    float p = 0.0f, r = 0.0f, y = 0.0f;
    SensorsCxSubsystem::get().getCurrentEulerAngles(p, r, y);
    if (pPitch) *pPitch = p;
    if (pRoll)  *pRoll  = r;
    if (pYaw)   *pYaw   = y;
    return micant::STATUS_SUCCESS;
}

} // extern "C"

// ----------------------------------------------------------------------------
// SCM & Version Registration Helper
// ----------------------------------------------------------------------------
inline void RegisterSensorsCxSubsystem() {
    // 1. Version Database Registration
    auto& vdb = micant::version::VersionDatabase::Instance();
    vdb.RegisterModule("sensorscx.sys", "10.0.26100.1", "SensorsCx Class Extension v2 Driver (TitanSensorFusion)");
    vdb.RegisterModule("sensrsvc.dll", "10.0.26100.1", "Sensor & Location Broker Userland Service (AegisOrientation)");

    // 2. SCM Registration
    auto& scm = micant::scm::ServiceControlManager::get();
    auto rec = std::make_shared<micant::scm::ServiceRecord>();
    rec->serviceName = L"SensorService";
    rec->displayName = L"Windows Sensor & 9-DoF Orientation Broker Service";
    rec->serviceType = micant::scm::SERVICE_WIN32_SHARE_PROCESS;
    rec->startType = micant::scm::SERVICE_AUTO_START;
    rec->binaryPath = L"C:\\Windows\\System32\\svchost.exe -k SensorGroup";
    rec->status.dwCurrentState = micant::scm::SERVICE_RUNNING;
    scm.registerServiceRecord(rec);

    // Initialize core subsystem singleton
    SensorsCxSubsystem::get().initialize();
}


} // namespace micant::sensorscx
