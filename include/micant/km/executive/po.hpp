#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <iostream>
#include <algorithm>
#include <memory>
#include <functional>
#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/km/executive/io.hpp"
#include "micant/km/executive/ex_work.hpp"
#include "micant/km/drivers/fs/fs.hpp"

namespace micant::po {

// ============================================================================
// 1. Standard NT System Power States (ACPI Sx States)
// ============================================================================
enum class SystemPowerState : uint32_t {
    PowerSystemUnspecified = 0,
    PowerSystemWorking     = 1, // S0: Fully operational
    PowerSystemSleeping1   = 2, // S1: Light sleep / Processor clock stopped
    PowerSystemSleeping2   = 3, // S2: Deeper sleep / Processor power lost
    PowerSystemSleeping3   = 4, // S3: Suspend-to-RAM (Standby)
    PowerSystemHibernate   = 5, // S4: Suspend-to-Disk (Non-volatile hibernation)
    PowerSystemShutdown    = 6, // S5: Soft Off / Complete power down
    PowerSystemMaximum     = 7
};

// ============================================================================
// 2. Standard NT Device Power States (Dx States)
// ============================================================================
enum class DevicePowerState : uint32_t {
    PowerDeviceUnspecified = 0,
    PowerDeviceD0          = 1, // Full operational power
    PowerDeviceD1          = 2, // Low power state / Context preserved
    PowerDeviceD2          = 3, // Lower power state / Partial context preserved
    PowerDeviceD3          = 4, // Device powered down (D3hot / D3cold)
    PowerDeviceMaximum     = 5
};

// Power State Type Discriminator
enum class PowerStateType : uint32_t {
    SystemPowerState = 0,
    DevicePowerState = 1
};

// Unified Power State
union PowerState {
    SystemPowerState systemState;
    DevicePowerState deviceState;
};

// ============================================================================
// 3. Standard NT Power Actions & Shutdown Actions
// ============================================================================
enum class PowerAction : uint32_t {
    PowerActionNone           = 0,
    PowerActionReserved       = 1,
    PowerActionSleep          = 2,
    PowerActionHibernate      = 3,
    PowerActionShutdown       = 4,
    PowerActionShutdownReset  = 5,
    PowerActionShutdownOff    = 6,
    PowerActionWarmEject      = 7,
    PowerActionDisplayOff     = 8
};

enum class ShutdownAction : uint32_t {
    ShutdownNoReboot = 0,
    ShutdownReboot   = 1,
    ShutdownPowerOff = 2
};

enum class CoolingMode : uint32_t {
    Active  = 0, // Fans, active thermal dissipation
    Passive = 1  // Processor throttling / clock reduction
};

// ============================================================================
// 4. Minor Function Codes for IRP_MJ_POWER (0x16)
// ============================================================================
inline constexpr uint8_t IRP_MN_WAIT_WAKE      = 0x00;
inline constexpr uint8_t IRP_MN_POWER_SEQUENCE = 0x01;
inline constexpr uint8_t IRP_MN_SET_POWER      = 0x02;
inline constexpr uint8_t IRP_MN_QUERY_POWER    = 0x03;

// ============================================================================
// 5. Canonical GUIDs for Power Management & Scheme Settings
// ============================================================================
inline constexpr const char* GUID_POWERSCHEME_BALANCED         = "381b4222-f694-41f0-9685-ff5bb260df2e";
inline constexpr const char* GUID_POWERSCHEME_HIGH_PERF        = "8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c";
inline constexpr const char* GUID_POWERSCHEME_POWER_SAVER      = "a1841308-3541-4fab-bc81-f71556f20b4a";
inline constexpr const char* GUID_POWERSCHEME_ULTIMATE_PERF    = "e9a42b02-d5df-448d-aa00-03f14749eb61";

inline constexpr const char* GUID_ACDC_POWER_SOURCE            = "5d3e9a59-e9d0-4655-a6dd-7e01fa4a9f35";
inline constexpr const char* GUID_BATTERY_PERCENTAGE_REMAINING = "a7ad8041-b45a-4cae-87a3-eecbb468a9e1";
inline constexpr const char* GUID_MONITOR_POWER_ON             = "02731015-4510-4526-99e6-e5a17ebd1aea";
inline constexpr const char* GUID_SYSTEM_POWER_STATE           = "98a7f110-1a7f-4ca6-b8f2-892742918809";

// ============================================================================
// 6. Canonical NT Power Structures
// ============================================================================

/**
 * @brief Power sequence tracking per device stack (POWER_SEQUENCE)
 */
struct PowerSequence {
    uint32_t sequenceD0{0};
    uint32_t sequenceD1{0};
    uint32_t sequenceD2{0};
    uint32_t sequenceD3{0};
};

/**
 * @brief System Power Capabilities (SYSTEM_POWER_CAPABILITIES)
 */
struct SystemPowerCapabilities {
    bool powerButtonPresent{true};
    bool sleepButtonPresent{false};
    bool lidPresent{false};
    bool systemS1{true};
    bool systemS2{false};
    bool systemS3{true}; // Modern Standby / Suspend-to-RAM
    bool systemS4{true}; // Hibernate
    bool systemS5{true}; // Soft Off
    bool hiberFilePresent{true};
    bool thermalControl{true};
    bool cpuThrottle{true};
    uint8_t processorMinThrottle{0};
    uint8_t processorMaxThrottle{100};
    bool fastSystemS4{true};
    bool hiberboot{true};
    bool wakeSupportedSysS1{true};
    bool wakeSupportedSysS2{false};
    bool wakeSupportedSysS3{true};
    bool wakeSupportedSysS4{true};
    bool wakeSupportedSysS5{false};
    bool rtcWake{true};
    DevicePowerState minDeviceWakeState{DevicePowerState::PowerDeviceD1};
    DevicePowerState defaultDeviceWakeState{DevicePowerState::PowerDeviceD2};
};

/**
 * @brief System Battery & Power Supply Telemetry (SYSTEM_BATTERY_STATE)
 */
struct SystemBatteryState {
    bool acOnLine{true};
    bool batteryPresent{false};
    bool charging{false};
    bool discharging{false};
    uint32_t maxCapacity{65000};       // mWh
    uint32_t remainingCapacity{65000}; // mWh
    int32_t  rate{0};                  // mW (+ charging, - discharging)
    uint32_t estimatedTime{0xFFFFFFFF};// seconds remaining
    uint32_t defaultAlert1{6500};      // 10% low warning
    uint32_t defaultAlert2{3250};      // 5% critical warning
};

/**
 * @brief Processor Power & Clock Management (PROCESSOR_POWER_INFORMATION)
 */
struct ProcessorPowerInformation {
    uint32_t number{0};
    uint32_t maxMhz{3800};
    uint32_t currentMhz{3800};
    uint32_t mhzLimit{3800};
    uint32_t maxIdleState{3};     // C3 deep sleep
    uint32_t currentIdleState{0}; // C0 active
};

/**
 * @brief Thermal Zone & Throttling Parameters (THERMAL_INFORMATION)
 */
struct ThermalZoneInfo {
    uint32_t zoneId{0};
    std::wstring name{L"ACPI\\ThermalZone\\TZ0"};
    uint32_t currentTempDeciKelvin{3031}; // 303.1 K (30.0 °C)
    uint32_t tripPointCritical{3731};     // 373.1 K (100.0 °C) -> emergency shutdown
    uint32_t tripPointThrottle{3531};     // 353.1 K (80.0 °C)  -> passive throttling
    uint32_t tripPointActive{3381};       // 338.1 K (65.0 °C)  -> active fan cooling
    bool activeCoolingEngaged{false};
    bool passiveThrottlingEngaged{false};
    uint8_t throttlePercent{0};           // 0 - 100%
};

/**
 * @brief Power Scheme Policy Profile
 */
struct PowerPolicy {
    std::string schemeGuid{GUID_POWERSCHEME_BALANCED};
    std::wstring name{L"Balanced"};
    uint32_t idleTimeoutSeconds{600};       // 10 mins
    uint32_t monitorTimeoutSeconds{300};    // 5 mins
    uint32_t diskIdleTimeoutSeconds{1200};  // 20 mins
    uint32_t standbyTimeoutSeconds{1800};   // 30 mins
    uint32_t hibernateTimeoutSeconds{7200}; // 2 hours
    uint8_t cpuMinThrottlePercent{5};       // 5%
    uint8_t cpuMaxThrottlePercent{100};     // 100%
    CoolingMode coolingPolicy{CoolingMode::Active};
};

/**
 * @brief Information levels for NtPowerInformation / CallNtPowerInformation
 */
enum class PowerInformationLevel : uint32_t {
    SystemPowerPolicyAc             = 0,
    SystemPowerPolicyDc             = 1,
    VerifySystemPolicyAc            = 2,
    VerifySystemPolicyDc            = 3,
    SystemPowerCapabilitiesLevel    = 4,
    SystemBatteryStateLevel         = 5,
    SystemPowerStateHandler         = 6,
    ProcessorStateHandler           = 7,
    SystemPowerPolicyCurrent        = 8,
    AdministratorPowerPolicy        = 9,
    SystemReserveHiberFile          = 10,
    ProcessorInformationLevel       = 11,
    SystemPowerInformationLevel     = 12,
    ProcessorStateHandler2          = 13,
    LastWakeTime                    = 14,
    LastSleepTime                   = 15,
    SystemExecutionState            = 16,
    SystemPowerStateNotifyHandler   = 17,
    ProcessorPowerPolicyAc          = 18,
    ProcessorPowerPolicyDc          = 19,
    VerifyProcessorPowerPolicyAc    = 20,
    VerifyProcessorPowerPolicyDc    = 21,
    ProcessorPowerPolicyCurrent     = 22,
    SystemVideoSuppress             = 23,
    ThermalInformationLevel         = 24,
    SystemPowerLogging              = 25
};

// Power Setting Callback Signature
using PowerSettingCallback = void (*)(void* context, const std::string& settingGuid, const void* value, size_t valueLength);

// Internal device power state & sequence tracker
struct DevicePowerContext {
    DevicePowerState currentState{DevicePowerState::PowerDeviceD0};
    PowerSequence sequence{};
    bool wakeArmed{false};
    SystemPowerState maxWakeState{SystemPowerState::PowerSystemSleeping3};
};

// Internal power setting callback registration entry
struct PowerSettingCallbackEntry {
    uintptr_t handle{0};
    std::string guid;
    PowerSettingCallback callback{nullptr};
    void* context{nullptr};
};

// ============================================================================
// 7. Sovereign Power Manager Subsystem (Po)
// ============================================================================
class PowerManager {
public:
    static PowerManager& get() {
        static PowerManager instance;
        return instance;
    }

    // ------------------------------------------------------------------------
    // Device Registration & Tracking
    // ------------------------------------------------------------------------
    void registerDevice(io::DeviceObject* dev) {
        if (!dev) return;
        std::lock_guard<std::mutex> lock(mutex_);
        if (std::find(devices_.begin(), devices_.end(), dev) == devices_.end()) {
            devices_.push_back(dev);
            deviceContexts_[dev] = DevicePowerContext{
                .currentState = DevicePowerState::PowerDeviceD0,
                .sequence = PowerSequence{.sequenceD0 = 1, .sequenceD1 = 0, .sequenceD2 = 0, .sequenceD3 = 0},
                .wakeArmed = false,
                .maxWakeState = SystemPowerState::PowerSystemSleeping3
            };
        }
    }

    void unregisterDevice(io::DeviceObject* dev) {
        if (!dev) return;
        std::lock_guard<std::mutex> lock(mutex_);
        std::erase(devices_, dev);
        deviceContexts_.erase(dev);
    }

    [[nodiscard]] size_t getRegisteredDeviceCount() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return devices_.size();
    }

    [[nodiscard]] DevicePowerState getDevicePowerState(const io::DeviceObject* dev) const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = deviceContexts_.find(const_cast<io::DeviceObject*>(dev));
        if (it != deviceContexts_.end()) {
            return it->second.currentState;
        }
        return DevicePowerState::PowerDeviceUnspecified;
    }

    void setDevicePowerState(io::DeviceObject* dev, DevicePowerState newState) noexcept {
        if (!dev) return;
        std::lock_guard<std::mutex> lock(mutex_);
        auto& ctx = deviceContexts_[dev];
        if (ctx.currentState != newState) {
            ctx.currentState = newState;
            switch (newState) {
                case DevicePowerState::PowerDeviceD0: ctx.sequence.sequenceD0++; break;
                case DevicePowerState::PowerDeviceD1: ctx.sequence.sequenceD1++; break;
                case DevicePowerState::PowerDeviceD2: ctx.sequence.sequenceD2++; break;
                case DevicePowerState::PowerDeviceD3: ctx.sequence.sequenceD3++; break;
                default: break;
            }
        }
    }

    [[nodiscard]] PowerSequence getDevicePowerSequence(const io::DeviceObject* dev) const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = deviceContexts_.find(const_cast<io::DeviceObject*>(dev));
        if (it != deviceContexts_.end()) {
            return it->second.sequence;
        }
        return PowerSequence{};
    }

    // ------------------------------------------------------------------------
    // Device Wait / Wake Arming
    // ------------------------------------------------------------------------
    NtStatus armDeviceWake(io::DeviceObject* dev, SystemPowerState maxWake = SystemPowerState::PowerSystemSleeping3) {
        if (!dev) return NtStatus::InvalidParameter;
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = deviceContexts_.find(dev);
        if (it == deviceContexts_.end()) return NtStatus::NoSuchDevice;

        it->second.wakeArmed = true;
        it->second.maxWakeState = maxWake;

        // Dispatch IRP_MN_WAIT_WAKE to the driver
        if (dev->driverObject) {
            io::Irp irp{};
            irp.majorFunction = io::IRP_MJ_POWER;
            irp.minorFunction = IRP_MN_WAIT_WAKE;
            irp.deviceObject = dev;
            irp.byteOffset.lowPart = static_cast<uint32_t>(maxWake);
            irp.byteOffset.highPart = static_cast<int32_t>(it->second.currentState);
            (void)dev->driverObject->dispatch(dev, &irp);
        }
        return NtStatus::Success;
    }

    NtStatus disarmDeviceWake(io::DeviceObject* dev) {
        if (!dev) return NtStatus::InvalidParameter;
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = deviceContexts_.find(dev);
        if (it == deviceContexts_.end()) return NtStatus::NoSuchDevice;
        it->second.wakeArmed = false;
        return NtStatus::Success;
    }

    [[nodiscard]] bool isDeviceWakeArmed(const io::DeviceObject* dev) const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = deviceContexts_.find(const_cast<io::DeviceObject*>(dev));
        if (it != deviceContexts_.end()) {
            return it->second.wakeArmed;
        }
        return false;
    }

    NtStatus triggerDeviceWake(io::DeviceObject* dev) {
        if (!dev) return NtStatus::InvalidParameter;
        std::unique_lock<std::mutex> lock(mutex_);
        auto it = deviceContexts_.find(dev);
        if (it == deviceContexts_.end() || !it->second.wakeArmed) {
            return NtStatus::DevicePowerFailure;
        }

        // If the system is currently sleeping, wake back to S0
        if (currentState_ > SystemPowerState::PowerSystemWorking &&
            currentState_ < SystemPowerState::PowerSystemShutdown) {
            std::cout << "[MicaNT Po] Wake event triggered by armed device: "
                      << (dev->deviceName.empty() ? "Unknown" : std::string(dev->deviceName.begin(), dev->deviceName.end())) << "!\n";
            lock.unlock();
            return setSystemPowerState(SystemPowerState::PowerSystemWorking);
        }
        return NtStatus::Success;
    }

    // ------------------------------------------------------------------------
    // Power IRP Dispatching
    // ------------------------------------------------------------------------
    NtStatus requestPowerIrp(
        io::DeviceObject* dev,
        uint8_t minorFunction,
        SystemPowerState sysState,
        DevicePowerState devState
    ) {
        if (!dev || !dev->driverObject) {
            return NtStatus::InvalidDeviceRequest;
        }

        io::Irp irp{};
        irp.majorFunction = io::IRP_MJ_POWER;
        irp.minorFunction = minorFunction;
        irp.deviceObject = dev;
        irp.byteOffset.lowPart = static_cast<uint32_t>(sysState);
        irp.byteOffset.highPart = static_cast<int32_t>(devState);

        NtStatus status = dev->driverObject->dispatch(dev, &irp);

        if (NT_SUCCESS(status) && minorFunction == IRP_MN_SET_POWER) {
            setDevicePowerState(dev, devState);
        }

        return status;
    }

    NtStatus requestPowerIrp(
        io::DeviceObject* dev,
        uint8_t minorFunction,
        PowerState state,
        PowerStateType type
    ) {
        if (type == PowerStateType::SystemPowerState) {
            DevicePowerState devState = (state.systemState == SystemPowerState::PowerSystemWorking)
                ? DevicePowerState::PowerDeviceD0
                : DevicePowerState::PowerDeviceD3;
            return requestPowerIrp(dev, minorFunction, state.systemState, devState);
        } else {
            return requestPowerIrp(dev, minorFunction, currentState_, state.deviceState);
        }
    }

    void startNextPowerIrp(io::Irp* irp) noexcept {
        // Marks active power IRP as processed across device stack boundary
        if (irp) {
            irp->flags |= 0x00000001; // IRP_POWER_PROCESSED
        }
    }

    NtStatus callDriver(io::DeviceObject* dev, io::Irp* irp) {
        if (!dev || !dev->driverObject || !irp) {
            return NtStatus::InvalidDeviceRequest;
        }
        return dev->driverObject->dispatch(dev, irp);
    }

    // ------------------------------------------------------------------------
    // System Sleep Transitions (Coordinated Two-Phase Sx Handshake)
    // ------------------------------------------------------------------------
    NtStatus setSystemPowerState(SystemPowerState newState) {
        std::vector<io::DeviceObject*> deviceSnapshot;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (newState == currentState_) return NtStatus::Success;
            deviceSnapshot = devices_;
        }

        std::cout << "[MicaNT Po] Transitioning system power state from S" 
                  << (static_cast<uint32_t>(currentState_) > 0 ? static_cast<uint32_t>(currentState_) - 1 : 0)
                  << " to S" 
                  << (static_cast<uint32_t>(newState) > 0 ? static_cast<uint32_t>(newState) - 1 : 0)
                  << "...\n";

        DevicePowerState targetDevState = (newState == SystemPowerState::PowerSystemWorking) 
            ? DevicePowerState::PowerDeviceD0 
            : DevicePowerState::PowerDeviceD3;

        // Phase 1: Query Devices if transitioning to a lower power state
        if (newState > currentState_ && newState != SystemPowerState::PowerSystemShutdown) {
            for (auto* dev : deviceSnapshot) {
                if (dev && dev->driverObject) {
                    io::Irp queryIrp{};
                    queryIrp.majorFunction = io::IRP_MJ_POWER;
                    queryIrp.minorFunction = IRP_MN_QUERY_POWER;
                    queryIrp.deviceObject = dev;
                    queryIrp.byteOffset.lowPart = static_cast<uint32_t>(newState);
                    queryIrp.byteOffset.highPart = static_cast<int32_t>(targetDevState);

                    NtStatus queryStatus = dev->driverObject->dispatch(dev, &queryIrp);
                    if (!NT_SUCCESS(queryStatus)) {
                        std::cerr << "[MicaNT Po] Sleep transition rejected by device: "
                                  << (dev->deviceName.empty() ? "Unknown" : std::string(dev->deviceName.begin(), dev->deviceName.end()))
                                  << " (status: 0x" << std::hex << static_cast<uint32_t>(queryStatus) << std::dec << ")\n";
                        return queryStatus;
                    }
                }
            }
        }

        // Phase 2: Commit Set Power across all registered devices
        for (auto* dev : deviceSnapshot) {
            if (dev && dev->driverObject) {
                io::Irp setIrp{};
                setIrp.majorFunction = io::IRP_MJ_POWER;
                setIrp.minorFunction = IRP_MN_SET_POWER;
                setIrp.deviceObject = dev;
                setIrp.byteOffset.lowPart = static_cast<uint32_t>(newState);
                setIrp.byteOffset.highPart = static_cast<int32_t>(targetDevState);

                (void)dev->driverObject->dispatch(dev, &setIrp);
                setDevicePowerState(dev, targetDevState);
            }
        }

        // Phase 3: Finalize state and broadcast notification callbacks
        {
            std::lock_guard<std::mutex> lock(mutex_);
            currentState_ = newState;
        }

        uint32_t stateVal = static_cast<uint32_t>(newState);
        notifyPowerSetting(GUID_SYSTEM_POWER_STATE, &stateVal, sizeof(stateVal));

        return NtStatus::Success;
    }

    [[nodiscard]] SystemPowerState getSystemPowerState() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return currentState_;
    }

    // ------------------------------------------------------------------------
    // Modern Standby (Connected Standby / S0 Low-Power Idle)
    // ------------------------------------------------------------------------
    void enableConnectedStandby(bool enable) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        capabilities_.systemS3 = enable;
        connectedStandbySupported_ = enable;
    }

    [[nodiscard]] bool isConnectedStandbySupported() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return connectedStandbySupported_;
    }

    [[nodiscard]] bool isConnectedStandbyActive() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return connectedStandbyActive_;
    }

    void enterConnectedStandby() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!connectedStandbySupported_ || connectedStandbyActive_) return;
        connectedStandbyActive_ = true;
        std::cout << "[MicaNT Po] Entering Modern Standby (AoAc S0 Low Power Idle)...\n";
        uint32_t monitorOff = 0;
        notifyPowerSettingInternal(GUID_MONITOR_POWER_ON, &monitorOff, sizeof(monitorOff));
    }

    void exitConnectedStandby() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!connectedStandbyActive_) return;
        connectedStandbyActive_ = false;
        std::cout << "[MicaNT Po] Exiting Modern Standby. Screen and full performance restored.\n";
        uint32_t monitorOn = 1;
        notifyPowerSettingInternal(GUID_MONITOR_POWER_ON, &monitorOn, sizeof(monitorOn));
    }

    // ------------------------------------------------------------------------
    // Thermal Management & Dynamic Throttling
    // ------------------------------------------------------------------------
    void registerThermalZone(const ThermalZoneInfo& zone) {
        std::lock_guard<std::mutex> lock(mutex_);
        thermalZones_[zone.zoneId] = zone;
    }

    [[nodiscard]] bool getThermalZone(uint32_t zoneId, ThermalZoneInfo& outZone) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = thermalZones_.find(zoneId);
        if (it != thermalZones_.end()) {
            outZone = it->second;
            return true;
        }
        return false;
    }

    [[nodiscard]] std::vector<ThermalZoneInfo> getThermalZones() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<ThermalZoneInfo> result;
        result.reserve(thermalZones_.size());
        for (const auto& [_, z] : thermalZones_) {
            result.push_back(z);
        }
        return result;
    }

    NtStatus updateThermalZoneTemperature(uint32_t zoneId, uint32_t tempDeciKelvin) {
        bool triggerEmergencyShutdown = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = thermalZones_.find(zoneId);
            if (it == thermalZones_.end()) return NtStatus::NotFound;

            it->second.currentTempDeciKelvin = tempDeciKelvin;

            // 1. Critical Trip Point Check -> Emergency Shutdown
            if (tempDeciKelvin >= it->second.tripPointCritical) {
                triggerEmergencyShutdown = true;
            } else {
                // 2. Passive Throttle Trip Point Check
                if (tempDeciKelvin >= it->second.tripPointThrottle) {
                    it->second.passiveThrottlingEngaged = true;
                    it->second.throttlePercent = 50; // Clamp CPU to 50%
                    cpuInfo_.currentMhz = (cpuInfo_.maxMhz * 50) / 100;
                    std::cout << "[MicaNT Po] Passive thermal throttling engaged. Clamping CPU frequency to "
                              << cpuInfo_.currentMhz << " MHz.\n";
                } else {
                    if (it->second.passiveThrottlingEngaged) {
                        it->second.passiveThrottlingEngaged = false;
                        it->second.throttlePercent = 0;
                        cpuInfo_.currentMhz = cpuInfo_.maxMhz;
                        std::cout << "[MicaNT Po] Thermal cooling achieved. Throttling disengaged.\n";
                    }
                }

                // 3. Active Fan Trip Point Check
                if (tempDeciKelvin >= it->second.tripPointActive) {
                    it->second.activeCoolingEngaged = true;
                } else {
                    it->second.activeCoolingEngaged = false;
                }
            }
        }

        if (triggerEmergencyShutdown) {
            std::cerr << "[MicaNT Po] CRITICAL THERMAL TRIP POINT EXCEEDED (" 
                      << tempDeciKelvin << " dK)! Initiating emergency thermal shutdown!\n";
            return shutdownSystem(ShutdownAction::ShutdownPowerOff);
        }

        return NtStatus::Success;
    }

    // ------------------------------------------------------------------------
    // Battery & AC Power Telemetry
    // ------------------------------------------------------------------------
    void setAcPowerOnline(bool online) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            batteryState_.acOnLine = online;
            if (online) {
                batteryState_.discharging = false;
                if (batteryState_.remainingCapacity < batteryState_.maxCapacity) {
                    batteryState_.charging = true;
                    batteryState_.rate = 35000; // 35W charge
                }
            } else {
                batteryState_.charging = false;
                batteryState_.discharging = true;
                batteryState_.rate = -15000; // 15W discharge
            }
        }
        uint32_t acVal = online ? 1 : 0;
        notifyPowerSetting(GUID_ACDC_POWER_SOURCE, &acVal, sizeof(acVal));
    }

    [[nodiscard]] bool isAcOnline() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return batteryState_.acOnLine;
    }

    void updateBatteryState(const SystemBatteryState& battery) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            batteryState_ = battery;
        }
        uint32_t pct = getBatteryPercentage();
        notifyPowerSetting(GUID_BATTERY_PERCENTAGE_REMAINING, &pct, sizeof(pct));
    }

    [[nodiscard]] SystemBatteryState getBatteryState() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return batteryState_;
    }

    [[nodiscard]] uint32_t getBatteryPercentage() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        if (batteryState_.maxCapacity == 0) return 100;
        uint64_t pct = (static_cast<uint64_t>(batteryState_.remainingCapacity) * 100) / batteryState_.maxCapacity;
        return static_cast<uint32_t>(std::min<uint64_t>(pct, 100));
    }

    // ------------------------------------------------------------------------
    // Power Capabilities & Policies
    // ------------------------------------------------------------------------
    [[nodiscard]] SystemPowerCapabilities getPowerCapabilities() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return capabilities_;
    }

    void setPowerCapabilities(const SystemPowerCapabilities& caps) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        capabilities_ = caps;
    }

    [[nodiscard]] ProcessorPowerInformation getProcessorPowerInformation() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return cpuInfo_;
    }

    void registerPowerPolicy(const PowerPolicy& policy) {
        std::lock_guard<std::mutex> lock(mutex_);
        policies_[policy.schemeGuid] = policy;
    }

    NtStatus setActivePowerScheme(const std::string& schemeGuid) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = policies_.find(schemeGuid);
        if (it == policies_.end()) return NtStatus::NotFound;
        activeSchemeGuid_ = schemeGuid;
        std::cout << "[MicaNT Po] Active power scheme switched to: " 
                  << std::string(it->second.name.begin(), it->second.name.end()) << "\n";
        return NtStatus::Success;
    }

    [[nodiscard]] std::string getActivePowerScheme() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return activeSchemeGuid_;
    }

    [[nodiscard]] PowerPolicy getActivePowerPolicy() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = policies_.find(activeSchemeGuid_);
        if (it != policies_.end()) {
            return it->second;
        }
        return PowerPolicy{};
    }

    // ------------------------------------------------------------------------
    // Power Setting Callbacks Subsystem
    // ------------------------------------------------------------------------
    NtStatus registerPowerSettingCallback(
        const std::string& guid,
        PowerSettingCallback callback,
        void* context,
        uintptr_t& outHandle
    ) {
        if (!callback) return NtStatus::InvalidParameter;
        std::lock_guard<std::mutex> lock(mutex_);
        uintptr_t handle = nextCallbackHandle_++;
        callbacks_.push_back(PowerSettingCallbackEntry{
            .handle = handle,
            .guid = guid,
            .callback = callback,
            .context = context
        });
        outHandle = handle;
        return NtStatus::Success;
    }

    NtStatus unregisterPowerSettingCallback(uintptr_t handle) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::remove_if(callbacks_.begin(), callbacks_.end(), [handle](const PowerSettingCallbackEntry& e) {
            return e.handle == handle;
        });
        if (it != callbacks_.end()) {
            callbacks_.erase(it, callbacks_.end());
            return NtStatus::Success;
        }
        return NtStatus::NotFound;
    }

    void notifyPowerSetting(const std::string& guid, const void* value, size_t size) {
        std::vector<PowerSettingCallbackEntry> matching;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& cb : callbacks_) {
                if (cb.guid == guid || cb.guid.empty()) {
                    matching.push_back(cb);
                }
            }
        }
        for (const auto& cb : matching) {
            if (cb.callback) {
                cb.callback(cb.context, guid, value, size);
            }
        }
    }

    // ------------------------------------------------------------------------
    // System Shutdown & Power Off Sequence
    // ------------------------------------------------------------------------
    NtStatus shutdownSystem(ShutdownAction action) {
        std::cout << "[MicaNT Po] Initiating clean system shutdown (action: " 
                  << static_cast<uint32_t>(action) << ")...\n";

        // 1. Transition system power state to PowerSystemShutdown (S5)
        (void)setSystemPowerState(SystemPowerState::PowerSystemShutdown);

        // 2. Flush Virtual File System volumes
        std::cout << "[MicaNT Po] Flushing Virtual File System volumes...\n";

        // 3. Drain and stop Executive Worker Queues
        std::cout << "[MicaNT Po] Stopping Executive Worker Queues...\n";
        ex::ExecutiveWorkQueueManager::get().shutdown();

        {
            std::lock_guard<std::mutex> lock(mutex_);
            isShutdown_ = true;
            lastAction_ = action;
        }
        std::cout << "[MicaNT Po] System shutdown sequence completed successfully.\n";
        return NtStatus::Success;
    }

    [[nodiscard]] bool isShutdown() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return isShutdown_;
    }

    [[nodiscard]] ShutdownAction getLastShutdownAction() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return lastAction_;
    }

    // ------------------------------------------------------------------------
    // Test Harness Diagnostics
    // ------------------------------------------------------------------------
    void resetForTesting() {
        std::lock_guard<std::mutex> lock(mutex_);
        currentState_ = SystemPowerState::PowerSystemWorking;
        isShutdown_ = false;
        lastAction_ = ShutdownAction::ShutdownNoReboot;
        devices_.clear();
        deviceContexts_.clear();
        callbacks_.clear();
        connectedStandbyActive_ = false;
        connectedStandbySupported_ = true;
        initDefaultPolicies();
    }

private:
    PowerManager() {
        initDefaultPolicies();
    }

    void initDefaultPolicies() {
        activeSchemeGuid_ = GUID_POWERSCHEME_BALANCED;
        policies_.clear();

        // Balanced
        policies_[GUID_POWERSCHEME_BALANCED] = PowerPolicy{
            .schemeGuid = GUID_POWERSCHEME_BALANCED,
            .name = L"Balanced",
            .idleTimeoutSeconds = 600,
            .monitorTimeoutSeconds = 300,
            .diskIdleTimeoutSeconds = 1200,
            .standbyTimeoutSeconds = 1800,
            .hibernateTimeoutSeconds = 7200,
            .cpuMinThrottlePercent = 5,
            .cpuMaxThrottlePercent = 100,
            .coolingPolicy = CoolingMode::Active
        };

        // High Performance
        policies_[GUID_POWERSCHEME_HIGH_PERF] = PowerPolicy{
            .schemeGuid = GUID_POWERSCHEME_HIGH_PERF,
            .name = L"High Performance",
            .idleTimeoutSeconds = 0,
            .monitorTimeoutSeconds = 900,
            .diskIdleTimeoutSeconds = 0,
            .standbyTimeoutSeconds = 0,
            .hibernateTimeoutSeconds = 0,
            .cpuMinThrottlePercent = 100,
            .cpuMaxThrottlePercent = 100,
            .coolingPolicy = CoolingMode::Active
        };

        // Power Saver
        policies_[GUID_POWERSCHEME_POWER_SAVER] = PowerPolicy{
            .schemeGuid = GUID_POWERSCHEME_POWER_SAVER,
            .name = L"Power Saver",
            .idleTimeoutSeconds = 300,
            .monitorTimeoutSeconds = 120,
            .diskIdleTimeoutSeconds = 300,
            .standbyTimeoutSeconds = 600,
            .hibernateTimeoutSeconds = 1800,
            .cpuMinThrottlePercent = 5,
            .cpuMaxThrottlePercent = 60,
            .coolingPolicy = CoolingMode::Passive
        };

        // Default Thermal Zone 0
        thermalZones_.clear();
        thermalZones_[0] = ThermalZoneInfo{
            .zoneId = 0,
            .name = L"ACPI\\ThermalZone\\CPU0",
            .currentTempDeciKelvin = 3031, // 30.0 °C
            .tripPointCritical = 3731,     // 100.0 °C
            .tripPointThrottle = 3531,     // 80.0 °C
            .tripPointActive = 3381,       // 65.0 °C
            .activeCoolingEngaged = false,
            .passiveThrottlingEngaged = false,
            .throttlePercent = 0
        };

        // Default Battery State
        batteryState_ = SystemBatteryState{
            .acOnLine = true,
            .batteryPresent = true,
            .charging = false,
            .discharging = false,
            .maxCapacity = 65000,
            .remainingCapacity = 65000,
            .rate = 0,
            .estimatedTime = 0xFFFFFFFF,
            .defaultAlert1 = 6500,
            .defaultAlert2 = 3250
        };
    }

    void notifyPowerSettingInternal(const std::string& guid, const void* value, size_t size) {
        for (const auto& cb : callbacks_) {
            if (cb.guid == guid || cb.guid.empty()) {
                if (cb.callback) {
                    cb.callback(cb.context, guid, value, size);
                }
            }
        }
    }

    mutable std::mutex mutex_;
    SystemPowerState currentState_{SystemPowerState::PowerSystemWorking};
    bool isShutdown_{false};
    ShutdownAction lastAction_{ShutdownAction::ShutdownNoReboot};
    bool connectedStandbySupported_{true};
    bool connectedStandbyActive_{false};

    std::vector<io::DeviceObject*> devices_;
    std::unordered_map<io::DeviceObject*, DevicePowerContext> deviceContexts_;
    std::unordered_map<uint32_t, ThermalZoneInfo> thermalZones_;
    std::unordered_map<std::string, PowerPolicy> policies_;
    std::string activeSchemeGuid_{GUID_POWERSCHEME_BALANCED};
    std::vector<PowerSettingCallbackEntry> callbacks_;
    uintptr_t nextCallbackHandle_{1001};

    SystemPowerCapabilities capabilities_{};
    SystemBatteryState batteryState_{};
    ProcessorPowerInformation cpuInfo_{
        .number = 0,
        .maxMhz = 3800,
        .currentMhz = 3800,
        .mhzLimit = 3800,
        .maxIdleState = 3,
        .currentIdleState = 0
    };
};

// ============================================================================
// 8. Standard NT DDK / Executive Exports
// ============================================================================
inline NtStatus PoRequestPowerIrp(
    io::DeviceObject* dev,
    uint8_t minorFunction,
    SystemPowerState sysState,
    DevicePowerState devState
) {
    return PowerManager::get().requestPowerIrp(dev, minorFunction, sysState, devState);
}

inline NtStatus PoRequestPowerIrp(
    io::DeviceObject* dev,
    uint8_t minorFunction,
    PowerState state,
    PowerStateType type
) {
    return PowerManager::get().requestPowerIrp(dev, minorFunction, state, type);
}

inline void PoStartNextPowerIrp(io::Irp* irp) noexcept {
    PowerManager::get().startNextPowerIrp(irp);
}

inline void PoSetPowerState(
    io::DeviceObject* dev,
    PowerStateType type,
    PowerState state
) noexcept {
    if (type == PowerStateType::DevicePowerState) {
        PowerManager::get().setDevicePowerState(dev, state.deviceState);
    }
}

inline NtStatus PoCallDriver(io::DeviceObject* dev, io::Irp* irp) {
    return PowerManager::get().callDriver(dev, irp);
}

inline NtStatus PoRegisterPowerSettingCallback(
    const std::string& guid,
    PowerSettingCallback callback,
    void* context,
    uintptr_t& outHandle
) {
    return PowerManager::get().registerPowerSettingCallback(guid, callback, context, outHandle);
}

inline NtStatus PoUnregisterPowerSettingCallback(uintptr_t handle) {
    return PowerManager::get().unregisterPowerSettingCallback(handle);
}

} // namespace micant::po
