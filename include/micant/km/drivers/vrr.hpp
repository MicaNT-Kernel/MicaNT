// ============================================================================
// MicaNT Clean-Room Kernel - Windows Display Variable Refresh Rate (VRR),
// Adaptive-Sync, Dynamic Refresh Rate (DRR) & Advanced Color Management (ACM)
// File: include/micant/vrr.hpp
//
// Provenance & Clean-Room Statement:
// Authored strictly from public Microsoft Windows Driver Kit (WDK) Display
// Miniport Driver (Dxgkrnl / Dxgk) specifications, VESA Adaptive-Sync standard,
// DirectX DXGI SwapChain present timing, and Windows 11 Advanced Color
// Management (ACM) / Auto HDR architecture:
//   - Variable Refresh Rate (VRR / VESA Adaptive-Sync / G-Sync / FreeSync):
//     * Seamless VBlank duration modulation per frame based on GPU render time.
//     * Minimum and Maximum refresh frequency boundaries (e.g. 48Hz - 240Hz).
//     * Low-latency tearing elimination and PresentDuration flip queue sync.
//   - Dynamic Refresh Rate (DRR):
//     * Dynamic 60Hz <-> 120Hz/240Hz rate boosting during user interaction (inking,
//       touch gestures, kinetic scrolling) and power-saving fallback when idle.
//   - Auto HDR & Advanced Color Management (ACM):
//     * Color gamut spaces: sRGB (Rec.709), DCI-P3 (Display P3), BT.2020 (Wide Color).
//     * SMPTE ST 2084 Perceptual Quantizer (PQ) electro-optical transfer function.
//     * scRGB linear 16-bit floating point (FP16) high-dynamic-range buffers.
//     * SDR-to-HDR tone expansion based on paper white nits and peak luminance.
//
// Sovereign Codename: TitanDisplay / AegisRefresh
// Strict ISO C++23, zero external dependencies, 100% offline, zero telemetry.
// ============================================================================

#ifndef MICANT_VRR_HPP
#define MICANT_VRR_HPP

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
#include <chrono>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

#include "ntdef.hpp"
#include "ntstatus.hpp"
#include "version.hpp"
#include "scm.hpp"

namespace micant::vrr {

// ============================================================================
// 1. Color Gamuts, HDR Transfer Functions & Formats
// ============================================================================

enum class DisplayColorGamut : uint32_t {
    sRGB   = 0, // Rec.709 Standard Gamut (SDR standard)
    DCIP3  = 1, // DCI-P3 Digital Cinema / Display P3 Wide Gamut
    BT2020 = 2  // ITU-R BT.2020 Ultra-Wide HDR Gamut
};

inline const char* DisplayColorGamutToString(DisplayColorGamut gamut) noexcept {
    switch (gamut) {
        case DisplayColorGamut::sRGB:   return "sRGB (BT.709 / SDR)";
        case DisplayColorGamut::DCIP3:  return "DCI-P3 (Display P3 Wide Gamut)";
        case DisplayColorGamut::BT2020: return "BT.2020 (Ultra-Wide HDR Gamut)";
        default:                        return "Unknown Gamut";
    }
}

enum class DisplayTransferFunction : uint32_t {
    Gamma22 = 0, // Traditional Gamma 2.2 EOTF
    ST2084  = 1, // SMPTE ST 2084 Perceptual Quantizer (PQ)
    Linear  = 2  // Linear scRGB (1.0 = 80 nits standard SDR reference white)
};

inline const char* DisplayTransferFunctionToString(DisplayTransferFunction tf) noexcept {
    switch (tf) {
        case DisplayTransferFunction::Gamma22: return "Gamma 2.2 (SDR Power Curve)";
        case DisplayTransferFunction::ST2084:  return "SMPTE ST 2084 (Perceptual Quantizer / PQ)";
        case DisplayTransferFunction::Linear:  return "scRGB Linear Floating Point";
        default:                               return "Unknown EOTF";
    }
}

struct DisplayHdrCapabilities {
    bool     hdrSupported{true};
    bool     autoHdrEnabled{false};
    float    paperWhiteNits{200.0f};  // Reference white level (typically 80 - 300 nits)
    float    maxPeakLuminanceNits{1000.0f}; // Maximum panel peak burst brightness
    float    minBlackLevelNits{0.001f};     // Deep black floor (OLED / Mini-LED)
    DisplayColorGamut nativeGamut{DisplayColorGamut::DCIP3};
};

// ============================================================================
// 2. Variable Refresh Rate (VRR) & Display Timing Structures
// ============================================================================

struct DisplayVrrCapabilities {
    bool     vrrSupported{true};
    float    minRefreshHz{48.0f};    // Lower Adaptive-Sync limit
    float    maxRefreshHz{240.0f};   // Upper native panel limit
    float    currentRefreshHz{120.0f};
    bool     lowFramerateCompensation{true}; // LFC below minRefreshHz (frame doubling)
    bool     dynamicRefreshRateEnabled{true}; // DRR automatic boost/drop
};

struct DisplayPacingInfo {
    uint32_t renderTimeUs{0};          // GPU render time for the frame
    uint32_t calculatedVBlankUs{0};    // Paced VBlank duration
    float    effectiveRefreshHz{0.0f}; // Instantaneous effective refresh rate
    bool     frameDoubled{false};      // LFC active
};

// ============================================================================
// 3. Sovereign Display Endpoint
// ============================================================================

class DisplayEndpoint {
private:
    uint32_t                m_id{1};
    std::wstring            m_name;
    uint32_t                m_width{3840};
    uint32_t                m_height{2160};
    DisplayVrrCapabilities  m_vrrCaps{};
    DisplayHdrCapabilities  m_hdrCaps{};
    DisplayColorGamut       m_currentGamut{DisplayColorGamut::DCIP3};

    // DRR activity state
    bool                    m_inHighMotionState{false};
    float                   m_idleBaseHz{60.0f};
    float                   m_motionBoostHz{120.0f};

public:
    DisplayEndpoint(uint32_t id, std::wstring_view name, uint32_t width = 3840, uint32_t height = 2160)
        : m_id(id), m_name(name), m_width(width), m_height(height) {}

    [[nodiscard]] uint32_t getId() const noexcept { return m_id; }
    [[nodiscard]] const std::wstring& getName() const noexcept { return m_name; }
    [[nodiscard]] uint32_t getWidth() const noexcept { return m_width; }
    [[nodiscard]] uint32_t getHeight() const noexcept { return m_height; }

    [[nodiscard]] const DisplayVrrCapabilities& getVrrCaps() const noexcept { return m_vrrCaps; }
    void setVrrCaps(const DisplayVrrCapabilities& caps) noexcept { m_vrrCaps = caps; }

    [[nodiscard]] const DisplayHdrCapabilities& getHdrCaps() const noexcept { return m_hdrCaps; }
    void setHdrCaps(const DisplayHdrCapabilities& caps) noexcept { m_hdrCaps = caps; }

    [[nodiscard]] DisplayColorGamut getColorGamut() const noexcept { return m_currentGamut; }
    void setColorGamut(DisplayColorGamut gamut) noexcept { m_currentGamut = gamut; }

    void setRefreshRate(float hz) noexcept {
        if (!m_vrrCaps.vrrSupported) return;
        m_vrrCaps.currentRefreshHz = std::clamp(hz, m_vrrCaps.minRefreshHz, m_vrrCaps.maxRefreshHz);
    }

    void setDynamicRefreshRate(bool enabled, float idleHz = 60.0f, float boostHz = 120.0f) noexcept {
        m_vrrCaps.dynamicRefreshRateEnabled = enabled;
        m_idleBaseHz = idleHz;
        m_motionBoostHz = boostHz;
        if (!enabled) {
            m_vrrCaps.currentRefreshHz = m_motionBoostHz;
        }
    }

    void notifyUserInteraction(bool activeMotion) noexcept {
        if (!m_vrrCaps.dynamicRefreshRateEnabled) return;
        m_inHighMotionState = activeMotion;
        m_vrrCaps.currentRefreshHz = activeMotion ? m_motionBoostHz : m_idleBaseHz;
    }

    [[nodiscard]] bool isInHighMotion() const noexcept { return m_inHighMotionState; }

    // VESA Adaptive-Sync frame pacing calculation
    DisplayPacingInfo calculatePacing(uint32_t renderTimeUs) const noexcept {
        DisplayPacingInfo info{};
        info.renderTimeUs = renderTimeUs;

        // Microseconds per frame at max and min refresh rate
        double minFrameUs = 1000000.0 / static_cast<double>(m_vrrCaps.maxRefreshHz); // e.g. 4166.6 us @ 240Hz
        double maxFrameUs = 1000000.0 / static_cast<double>(m_vrrCaps.minRefreshHz); // e.g. 20833.3 us @ 48Hz

        double targetFrameUs = static_cast<double>(renderTimeUs);

        if (targetFrameUs < minFrameUs) {
            // Rendered faster than max display refresh -> pace to max refresh rate
            targetFrameUs = minFrameUs;
            info.frameDoubled = false;
        } else if (targetFrameUs > maxFrameUs) {
            // Rendered slower than min refresh rate (LFC needed)
            if (m_vrrCaps.lowFramerateCompensation) {
                // Double refresh cycles to avoid panel flicker
                targetFrameUs = targetFrameUs / 2.0;
                info.frameDoubled = true;
            } else {
                targetFrameUs = maxFrameUs;
                info.frameDoubled = false;
            }
        }

        info.calculatedVBlankUs = static_cast<uint32_t>(targetFrameUs);
        info.effectiveRefreshHz = (info.calculatedVBlankUs > 0)
            ? static_cast<float>(1000000.0 / info.calculatedVBlankUs)
            : m_vrrCaps.currentRefreshHz;

        return info;
    }

    // Auto HDR SDR-to-HDR highlight tone expansion
    void transformSdrToHdr(float sdrR, float sdrG, float sdrB, float& outR, float& outG, float& outB) const noexcept {
        if (!m_hdrCaps.autoHdrEnabled) {
            outR = sdrR;
            outG = sdrG;
            outB = sdrB;
            return;
        }

        // Paper white reference factor (80 nits standard normalized SDR)
        float paperWhiteScale = m_hdrCaps.paperWhiteNits / 80.0f;
        float peakHeadroomScale = m_hdrCaps.maxPeakLuminanceNits / m_hdrCaps.paperWhiteNits;

        // Calculate luminance (Rec.709 coefficients)
        float lum = 0.2126f * sdrR + 0.7152f * sdrG + 0.0722f * sdrB;

        // Non-linear highlight expansion shoulder (preserve midtones, boost bright spots)
        float boostFactor = 1.0f;
        if (lum > 0.5f) {
            float highlight = (lum - 0.5f) * 2.0f; // 0..1 range
            boostFactor += highlight * (peakHeadroomScale - 1.0f) * 0.65f;
        }

        outR = sdrR * paperWhiteScale * boostFactor;
        outG = sdrG * paperWhiteScale * boostFactor;
        outB = sdrB * paperWhiteScale * boostFactor;
    }
};

// ============================================================================
// 4. Sovereign VRR & Display Subsystem Manager
// ============================================================================

class VrrSubsystem {
private:
    std::mutex                                               m_mutex;
    bool                                                     m_enabled{true};
    std::unordered_map<uint32_t, std::shared_ptr<DisplayEndpoint>> m_displays;
    uint32_t                                                 m_nextDisplayId{1};

    // Telemetry counters
    uint64_t                                                 m_totalPacedFrames{0};
    uint64_t                                                 m_totalHdrConversions{0};

    VrrSubsystem() {
        // Pre-register Primary Sovereign OLED Display
        createDisplayInternal(L"MicaNT Sovereign 4K HDR Quantum Display", 3840, 2160);
    }

    uint32_t createDisplayInternal(std::wstring_view name, uint32_t width, uint32_t height) {
        uint32_t id = m_nextDisplayId++;
        m_displays[id] = std::make_shared<DisplayEndpoint>(id, name, width, height);
        return id;
    }

public:
    static VrrSubsystem& get() {
        static VrrSubsystem instance;
        return instance;
    }

    void setSubsystemEnabled(bool enable) noexcept { m_enabled = enable; }
    [[nodiscard]] bool isSubsystemEnabled() const noexcept { return m_enabled; }

    uint32_t registerDisplay(std::wstring_view name, uint32_t width = 3840, uint32_t height = 2160) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return createDisplayInternal(name, width, height);
    }

    std::shared_ptr<DisplayEndpoint> getDisplay(uint32_t displayId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_displays.find(displayId);
        return (it != m_displays.end()) ? it->second : nullptr;
    }

    [[nodiscard]] size_t getDisplayCount() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_mutex));
        return m_displays.size();
    }

    bool paceFrame(uint32_t displayId, uint32_t renderTimeUs, DisplayPacingInfo& outInfo) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_enabled) return false;

        auto it = m_displays.find(displayId);
        if (it == m_displays.end()) return false;

        outInfo = it->second->calculatePacing(renderTimeUs);
        m_totalPacedFrames++;
        return true;
    }

    bool convertSdrToHdr(uint32_t displayId, float sdrR, float sdrG, float sdrB,
                         float& outR, float& outG, float& outB) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_enabled) return false;

        auto it = m_displays.find(displayId);
        if (it == m_displays.end()) return false;

        it->second->transformSdrToHdr(sdrR, sdrG, sdrB, outR, outG, outB);
        m_totalHdrConversions++;
        return true;
    }

    [[nodiscard]] uint64_t getTotalPacedFrames() const noexcept { return m_totalPacedFrames; }
    [[nodiscard]] uint64_t getTotalHdrConversions() const noexcept { return m_totalHdrConversions; }
};

// ============================================================================
// 5. Win32 & NT Clean-Room Dynamic C ABI Parity Exports
// ============================================================================

extern "C" {

inline NTSTATUS DxgkGetDisplayVrrCapabilities(uint32_t displayId, DisplayVrrCapabilities* pCaps) {
    if (!pCaps) return micant::STATUS_INVALID_PARAMETER;
    auto disp = VrrSubsystem::get().getDisplay(displayId);
    if (!disp) return micant::STATUS_NOT_FOUND;

    *pCaps = disp->getVrrCaps();
    return micant::STATUS_SUCCESS;
}

inline NTSTATUS DxgkSetDisplayRefreshRate(uint32_t displayId, float targetHz) {
    auto disp = VrrSubsystem::get().getDisplay(displayId);
    if (!disp) return micant::STATUS_NOT_FOUND;

    disp->setRefreshRate(targetHz);
    return micant::STATUS_SUCCESS;
}

inline NTSTATUS DxgkConfigureAutoHdr(uint32_t displayId, uint32_t enabled, float paperWhiteNits, float peakNits) {
    auto disp = VrrSubsystem::get().getDisplay(displayId);
    if (!disp) return micant::STATUS_NOT_FOUND;

    auto hdr = disp->getHdrCaps();
    hdr.autoHdrEnabled = (enabled != 0);
    if (paperWhiteNits > 0.0f) hdr.paperWhiteNits = paperWhiteNits;
    if (peakNits > 0.0f) hdr.maxPeakLuminanceNits = peakNits;
    disp->setHdrCaps(hdr);
    return micant::STATUS_SUCCESS;
}

inline NTSTATUS DxgkApplyMonitorColorProfile(uint32_t displayId, uint32_t gamutType) {
    auto disp = VrrSubsystem::get().getDisplay(displayId);
    if (!disp) return micant::STATUS_NOT_FOUND;

    if (gamutType > static_cast<uint32_t>(DisplayColorGamut::BT2020)) {
        return micant::STATUS_INVALID_PARAMETER;
    }

    disp->setColorGamut(static_cast<DisplayColorGamut>(gamutType));
    return micant::STATUS_SUCCESS;
}

inline NTSTATUS DxgkCalculatePresentPacing(uint32_t displayId, uint32_t renderTimeUs, DisplayPacingInfo* pOutInfo) {
    if (!pOutInfo) return micant::STATUS_INVALID_PARAMETER;
    bool ok = VrrSubsystem::get().paceFrame(displayId, renderTimeUs, *pOutInfo);
    return ok ? micant::STATUS_SUCCESS : micant::STATUS_NOT_FOUND;
}

inline NTSTATUS DxgkTransformSdrToHdr(uint32_t displayId, float r, float g, float b,
                                     float* outR, float* outG, float* outB) {
    if (!outR || !outG || !outB) return micant::STATUS_INVALID_PARAMETER;
    bool ok = VrrSubsystem::get().convertSdrToHdr(displayId, r, g, b, *outR, *outG, *outB);
    return ok ? micant::STATUS_SUCCESS : micant::STATUS_NOT_FOUND;
}

} // extern "C"

// ============================================================================
// 6. Subsystem Registration Helper
// ============================================================================

inline void RegisterVrrSubsystem() {
    auto& vdb = micant::version::VersionDatabase::Instance();
    vdb.RegisterModule("dxgkrnl.sys", "10.0.26100.1", "DirectX Graphics Kernel Display Engine (TitanDisplay)");
    vdb.RegisterModule("display.sys", "10.0.26100.1", "Windows Display Miniport & VRR Driver (AegisRefresh)");

    auto& scm = micant::scm::ServiceControlManager::get();
    auto rec = std::make_shared<micant::scm::ServiceRecord>();
    rec->serviceName = L"DisplayEnhancementService";
    rec->displayName = L"Windows Display Enhancement Service";
    rec->serviceType = micant::scm::SERVICE_WIN32_SHARE_PROCESS;
    rec->startType = micant::scm::SERVICE_AUTO_START;
    rec->binaryPath = L"C:\\Windows\\System32\\svchost.exe -k DisplayGroup";
    rec->status.dwCurrentState = micant::scm::SERVICE_RUNNING;
    scm.registerServiceRecord(rec);
}

} // namespace micant::vrr

#endif // MICANT_VRR_HPP
