/**
 * @file prismx_xhci_usb32_gamepad_wddm32.cpp
 * @brief Horizon 315 (Kernel Unit #91): Sovereign MicaNT USB 3.2 / xHCI & HID 8000Hz
 *        Hardware Gamepad Driver Stack with DirectInput 8 & XInput 1.4 Hardware Synchronization
 *        - xHCI SuperSpeed Transfer Request Block (TRB) Ring Buffer Management
 *        - USB 3.2 8000Hz Microframe Interrupt Endpoint Polling (125us Interval)
 *        - 64-Byte Raw HID Gamepad Report Parsing with Dual 16-Bit Analog Thumbsticks
 *        - Official DirectInput 8 & XInput 1.4 Radial Deadzone Filtering Engine
 *        - Low-Latency Haptic Force Feedback Actuator Output Routing.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_xhci_usb32_gamepad_wddm32.hpp"

namespace micant::um::prismx_hid {

bool VerifyHorizon315XhciGamepadSubsystem() noexcept {
    // ------------------------------------------------------------------------
    // Pillar 1: xHCI USB 3.2 TRB Ring Buffer Management & Cycle Bit Wrapping
    // ------------------------------------------------------------------------
    MicaNtXhciGamepadEngine engine;

    if (engine.GetRingCapacity() != 64u) return false;
    if (!engine.GetTelemetry().XhciTransferRingValid) return false;

    // Submit 64 TRBs to verify Link TRB traversal and cycle state toggling
    for (uint32_t i = 0; i < 64; ++i) {
        NTSTATUS status = engine.SubmitTransferTrb(0x0000000020000000ULL + i * 64, 64, true);
        if (!NT_SUCCESS(status)) return false;
    }

    if (engine.GetTelemetry().TotalTrbsEnqueued != 64u) return false;
    if (engine.GetTelemetry().CycleBitToggles < 1u) return false;

    // ------------------------------------------------------------------------
    // Pillar 2: 8000Hz Microframe Report Parsing & Axis Extraction
    // ------------------------------------------------------------------------
    MicaNtRawGamepadReport report{};
    report.Buttons = XINPUT_BTN_A | XINPUT_BTN_DPAD_UP | XINPUT_BTN_RIGHT_SHOULDER;
    report.LeftTrigger = 200;
    report.RightTrigger = 20; // Below threshold 30
    report.ThumbLX = 20000;
    report.ThumbLY = -15000; // Well outside deadzone 7849
    report.ThumbRX = 3000;
    report.ThumbRY = -4000;  // Inside radial deadzone 8689 (magnitude 5000)
    report.QpcTimestamp = 200000;

    NTSTATUS pushStatus = engine.PushRawReport(report);
    if (!NT_SUCCESS(pushStatus)) return false;

    // ------------------------------------------------------------------------
    // Pillar 3: Official XInput 1.4 API & Radial Deadzone Parity
    // ------------------------------------------------------------------------
    MicaNtXInputState state{};
    NTSTATUS getStateStatus = engine.GetXInputState(&state);
    if (!NT_SUCCESS(getStateStatus)) return false;

    if (state.Buttons != (XINPUT_BTN_A | XINPUT_BTN_DPAD_UP | XINPUT_BTN_RIGHT_SHOULDER)) return false;
    if (state.LeftTrigger != 200) return false;
    if (state.RightTrigger != 0) return false; // Threshold filtered
    if (state.ThumbLX == 0 || state.ThumbLY == 0) return false;
    if (state.ThumbRX != 0 || state.ThumbRY != 0) return false; // Deadzone zeroed

    // ------------------------------------------------------------------------
    // Pillar 4: Haptic Vibration Feedback Dispatch
    // ------------------------------------------------------------------------
    MicaNtXInputVibration vib{};
    vib.LeftMotorSpeed = 32768;
    vib.RightMotorSpeed = 65535;

    NTSTATUS vibStatus = engine.SetVibration(vib);
    if (!NT_SUCCESS(vibStatus)) return false;
    if (engine.GetCurrentVibration().LeftMotorSpeed != 32768) return false;
    if (engine.GetCurrentVibration().RightMotorSpeed != 65535) return false;

    // ------------------------------------------------------------------------
    // Pillar 5: Telemetry Validation
    // ------------------------------------------------------------------------
    const auto& tel = engine.GetTelemetry();
    if (tel.TotalTrbsEnqueued != 64u) return false;
    if (tel.TotalReportsReceived != 1u) return false;
    if (tel.PollingRateHz != 8000.0f) return false;
    if (tel.PollPeriodUs != 125.0f) return false;
    if (tel.DeadzoneFilteredReports < 1u) return false;
    if (tel.HapticFeedbackDispatched != 1u) return false;
    if (!tel.ReportParsingValid) return false;
    if (!tel.DeadzoneFilterValid) return false;
    if (!tel.HapticOutEndpointValid) return false;

    return true;
}

} // namespace micant::um::prismx_hid
