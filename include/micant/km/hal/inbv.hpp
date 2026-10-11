#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <mutex>
#include <functional>
#include <atomic>
#include <cstring>
#include "ntdef.hpp"
#include "ntstatus.hpp"
#include "bootvid.hpp"

namespace micant::inbv {

// ============================================================================
// 1. Display Ownership & States
// ============================================================================
enum class DisplayOwnership : uint32_t {
    FirmwareGop         = 0, // Owned by UEFI GOP firmware pre-OS
    InbvDriver          = 1, // Owned by Kernel Initialization Boot Video Driver
    DisplayDriver       = 2  // Handed off to WDDM / DXGKRNL Display Driver
};

// ============================================================================
// 2. Standard 16-Color NT Palette
// ============================================================================
inline constexpr uint32_t INBV_COLOR_BLACK         = 0;
inline constexpr uint32_t INBV_COLOR_BLUE          = 1;
inline constexpr uint32_t INBV_COLOR_GREEN         = 2;
inline constexpr uint32_t INBV_COLOR_CYAN          = 3;
inline constexpr uint32_t INBV_COLOR_RED           = 4;
inline constexpr uint32_t INBV_COLOR_MAGENTA       = 5;
inline constexpr uint32_t INBV_COLOR_BROWN         = 6;
inline constexpr uint32_t INBV_COLOR_LIGHT_GRAY    = 7;
inline constexpr uint32_t INBV_COLOR_DARK_GRAY     = 8;
inline constexpr uint32_t INBV_COLOR_LIGHT_BLUE    = 9;
inline constexpr uint32_t INBV_COLOR_LIGHT_GREEN   = 10;
inline constexpr uint32_t INBV_COLOR_LIGHT_CYAN    = 11;
inline constexpr uint32_t INBV_COLOR_LIGHT_RED     = 12;
inline constexpr uint32_t INBV_COLOR_LIGHT_MAGENTA = 13;
inline constexpr uint32_t INBV_COLOR_YELLOW        = 14;
inline constexpr uint32_t INBV_COLOR_WHITE         = 15;

inline bootvid::Color InbvGetPaletteColor(uint32_t index) noexcept {
    static constexpr bootvid::Color PALETTE[16] = {
        bootvid::Color(0, 0, 0),        // 0: Black
        bootvid::Color(0, 0, 128),      // 1: Blue
        bootvid::Color(0, 128, 0),      // 2: Green
        bootvid::Color(0, 128, 128),    // 3: Cyan
        bootvid::Color(128, 0, 0),      // 4: Red
        bootvid::Color(128, 0, 128),    // 5: Magenta
        bootvid::Color(128, 128, 0),    // 6: Brown
        bootvid::Color(192, 192, 192),  // 7: Light Gray
        bootvid::Color(128, 128, 128),  // 8: Dark Gray
        bootvid::Color(0, 0, 255),      // 9: Light Blue
        bootvid::Color(0, 255, 0),      // 10: Light Green
        bootvid::Color(0, 255, 255),    // 11: Light Cyan
        bootvid::Color(255, 0, 0),      // 12: Light Red
        bootvid::Color(255, 0, 255),    // 13: Light Magenta
        bootvid::Color(255, 255, 0),    // 14: Yellow
        bootvid::Color(255, 255, 255)   // 15: White
    };
    return PALETTE[index & 0x0F];
}

// ============================================================================
// 3. Pixel Formats & Linear Framebuffer Transcoder
// ============================================================================
enum class PixelFormat : uint32_t {
    Bgra32 = 0, // UEFI GOP standard (Blue, Green, Red, Alpha 8-bit)
    Rgba32 = 1, // Red, Green, Blue, Alpha 8-bit
    Rgb24  = 2, // 24-bpp Packed (R, G, B)
    Bgr24  = 3, // 24-bpp Packed (B, G, R)
    Rgb565 = 4, // 16-bpp (5-bit Red, 6-bit Green, 5-bit Blue)
    Rgb555 = 5  // 16-bpp (1-bit reserved, 5-bit R, 5-bit G, 5-bit B)
};

inline uint32_t ColorToPixel(bootvid::Color c, PixelFormat fmt) noexcept {
    switch (fmt) {
        case PixelFormat::Bgra32:
            return c.toBgra32();
        case PixelFormat::Rgba32:
            return c.toRgba32();
        case PixelFormat::Rgb24:
            return (static_cast<uint32_t>(c.r) << 16) | (static_cast<uint32_t>(c.g) << 8) | static_cast<uint32_t>(c.b);
        case PixelFormat::Bgr24:
            return (static_cast<uint32_t>(c.b) << 16) | (static_cast<uint32_t>(c.g) << 8) | static_cast<uint32_t>(c.r);
        case PixelFormat::Rgb565: {
            uint16_t r5 = (c.r >> 3) & 0x1F;
            uint16_t g6 = (c.g >> 2) & 0x3F;
            uint16_t b5 = (c.b >> 3) & 0x1F;
            return static_cast<uint32_t>((r5 << 11) | (g6 << 5) | b5);
        }
        case PixelFormat::Rgb555: {
            uint16_t r5 = (c.r >> 3) & 0x1F;
            uint16_t g5 = (c.g >> 3) & 0x1F;
            uint16_t b5 = (c.b >> 3) & 0x1F;
            return static_cast<uint32_t>((r5 << 10) | (g5 << 5) | b5);
        }
    }
    return c.toBgra32();
}

inline bootvid::Color PixelToColor(uint32_t pixel, PixelFormat fmt) noexcept {
    switch (fmt) {
        case PixelFormat::Bgra32:
            return bootvid::Color(
                static_cast<uint8_t>((pixel >> 16) & 0xFF),
                static_cast<uint8_t>((pixel >> 8) & 0xFF),
                static_cast<uint8_t>(pixel & 0xFF),
                static_cast<uint8_t>((pixel >> 24) & 0xFF)
            );
        case PixelFormat::Rgba32:
            return bootvid::Color(
                static_cast<uint8_t>(pixel & 0xFF),
                static_cast<uint8_t>((pixel >> 8) & 0xFF),
                static_cast<uint8_t>((pixel >> 16) & 0xFF),
                static_cast<uint8_t>((pixel >> 24) & 0xFF)
            );
        case PixelFormat::Rgb24:
            return bootvid::Color(
                static_cast<uint8_t>((pixel >> 16) & 0xFF),
                static_cast<uint8_t>((pixel >> 8) & 0xFF),
                static_cast<uint8_t>(pixel & 0xFF)
            );
        case PixelFormat::Bgr24:
            return bootvid::Color(
                static_cast<uint8_t>(pixel & 0xFF),
                static_cast<uint8_t>((pixel >> 8) & 0xFF),
                static_cast<uint8_t>((pixel >> 16) & 0xFF)
            );
        case PixelFormat::Rgb565: {
            uint8_t r = static_cast<uint8_t>(((pixel >> 11) & 0x1F) * 255 / 31);
            uint8_t g = static_cast<uint8_t>(((pixel >> 5) & 0x3F) * 255 / 63);
            uint8_t b = static_cast<uint8_t>((pixel & 0x1F) * 255 / 31);
            return bootvid::Color(r, g, b);
        }
        case PixelFormat::Rgb555: {
            uint8_t r = static_cast<uint8_t>(((pixel >> 10) & 0x1F) * 255 / 31);
            uint8_t g = static_cast<uint8_t>(((pixel >> 5) & 0x1F) * 255 / 31);
            uint8_t b = static_cast<uint8_t>((pixel & 0x1F) * 255 / 31);
            return bootvid::Color(r, g, b);
        }
    }
    return bootvid::Color::black();
}

/**
 * @brief Blits and converts pixel formats between framebuffers.
 */
inline bool BlitConvertFramebuffer(
    const void* src, PixelFormat srcFmt, uint32_t srcPitchPixels,
    void* dst, PixelFormat dstFmt, uint32_t dstPitchPixels,
    uint32_t width, uint32_t height
) {
    if (!src || !dst || width == 0 || height == 0) return false;

    const auto* pSrc32 = reinterpret_cast<const uint32_t*>(src);
    auto* pDst32 = reinterpret_cast<uint32_t*>(dst);

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            uint32_t srcVal = pSrc32[y * srcPitchPixels + x];
            bootvid::Color c = PixelToColor(srcVal, srcFmt);
            pDst32[y * dstPitchPixels + x] = ColorToPixel(c, dstFmt);
        }
    }
    return true;
}

// ============================================================================
// 4. Inbv (Initialization Boot Video) Subsystem Controller
// ============================================================================
using INBV_RESET_DISPLAY_PARAMETERS = BOOLEAN;
using INBV_DISPLAY_STRING_FILTER = void (*)(const char* str);

class InbvController {
public:
    static InbvController& get() noexcept {
        static InbvController instance;
        return instance;
    }

    bool initialize(uint32_t width = 1024, uint32_t height = 768) {
        std::lock_guard<std::mutex> guard(mutex_);
        width_ = width;
        height_ = height;
        ownership_ = DisplayOwnership::FirmwareGop;
        bootDriverEnabled_ = false;
        textColor_ = INBV_COLOR_WHITE;
        progressBarCoord_ = { (width - 300) / 2, height - 100, 300, 8 };
        progressPercentage_ = 0;
        cursorX_ = 0;
        cursorY_ = 0;

        // Initialize driver
        return driver_.initializeVirtual(width, height);
    }

    void enableBootDriver(bool enable) {
        std::lock_guard<std::mutex> guard(mutex_);
        bootDriverEnabled_ = enable;
    }

    [[nodiscard]] bool isBootDriverEnabled() const noexcept {
        return bootDriverEnabled_;
    }

    /**
     * @brief Acquires exclusive display ownership from UEFI GOP firmware.
     */
    void acquireDisplayOwnership() {
        std::lock_guard<std::mutex> guard(mutex_);
        ownership_ = DisplayOwnership::InbvDriver;
        bootDriverEnabled_ = true;
    }

    /**
     * @brief Relinquishes display ownership to WDDM / DXGKRNL display driver.
     */
    void notifyDisplayOwnershipLost(bool resetDisplay = true) {
        std::lock_guard<std::mutex> guard(mutex_);
        ownership_ = DisplayOwnership::DisplayDriver;
        if (resetDisplay) {
            driver_.clear(bootvid::Color::black());
        }
        if (ownershipLostCallback_) {
            ownershipLostCallback_();
        }
    }

    [[nodiscard]] bool checkDisplayOwnership() const noexcept {
        return ownership_ == DisplayOwnership::InbvDriver;
    }

    [[nodiscard]] DisplayOwnership getOwnershipState() const noexcept {
        return ownership_;
    }

    void resetDisplay() {
        std::lock_guard<std::mutex> guard(mutex_);
        if (ownership_ == DisplayOwnership::InbvDriver) {
            driver_.clear(bootvid::Color::black());
            cursorX_ = 0;
            cursorY_ = 0;
        }
    }

    void solidColorFill(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom, uint32_t colorIndex) {
        std::lock_guard<std::mutex> guard(mutex_);
        if (ownership_ != DisplayOwnership::InbvDriver) return;
        if (right < left || bottom < top) return;

        bootvid::Color c = InbvGetPaletteColor(colorIndex);
        driver_.fillRectangle(left, top, (right - left) + 1, (bottom - top) + 1, c);
    }

    void setTextColor(uint32_t colorIndex) {
        std::lock_guard<std::mutex> guard(mutex_);
        textColor_ = colorIndex;
    }

    void displayString(const char* str) {
        if (!str) return;
        std::lock_guard<std::mutex> guard(mutex_);
        if (ownership_ != DisplayOwnership::InbvDriver) return;

        if (stringFilter_) {
            stringFilter_(str);
        }

        bootvid::Color fg = InbvGetPaletteColor(textColor_);
        driver_.drawString(cursorX_, cursorY_, str, fg, bootvid::Color::transparent(), 1);
        cursorY_ += 12;
        if (cursorY_ + 12 >= height_) {
            cursorY_ = 0; // Wrap cursor
        }
    }

    void setProgressBarCoordinates(uint32_t left, uint32_t top, uint32_t width = 300, uint32_t height = 8) {
        std::lock_guard<std::mutex> guard(mutex_);
        progressBarCoord_ = { left, top, width, height };
    }

    void updateProgressBar(uint32_t percentage) {
        std::lock_guard<std::mutex> guard(mutex_);
        if (ownership_ != DisplayOwnership::InbvDriver) return;

        progressPercentage_ = std::min(percentage, 100u);
        float t = static_cast<float>(progressPercentage_) / 100.0f;
        driver_.drawProgressBar(
            progressBarCoord_.x,
            progressBarCoord_.y,
            progressBarCoord_.w,
            progressBarCoord_.h,
            t,
            bootvid::Color::micaCyan(),
            bootvid::Color::micaSurface(),
            bootvid::Color::micaBorder()
        );
    }

    void installDisplayStringFilter(INBV_DISPLAY_STRING_FILTER filter) {
        std::lock_guard<std::mutex> guard(mutex_);
        stringFilter_ = filter;
    }

    void registerOwnershipLostCallback(std::function<void()> cb) {
        std::lock_guard<std::mutex> guard(mutex_);
        ownershipLostCallback_ = std::move(cb);
    }

    [[nodiscard]] bootvid::BootVideoDriver& getDriver() noexcept { return driver_; }
    [[nodiscard]] uint32_t getProgressPercentage() const noexcept { return progressPercentage_; }
    [[nodiscard]] uint32_t getTextColor() const noexcept { return textColor_; }

private:
    InbvController() = default;

    struct Rect { uint32_t x{0}, y{0}, w{0}, h{0}; };

    mutable std::mutex mutex_;
    bootvid::BootVideoDriver driver_;
    DisplayOwnership ownership_{DisplayOwnership::FirmwareGop};
    bool bootDriverEnabled_{false};
    uint32_t width_{1024};
    uint32_t height_{768};
    uint32_t cursorX_{0};
    uint32_t cursorY_{0};
    uint32_t textColor_{INBV_COLOR_WHITE};
    Rect progressBarCoord_{};
    uint32_t progressPercentage_{0};
    INBV_DISPLAY_STRING_FILTER stringFilter_{nullptr};
    std::function<void()> ownershipLostCallback_;
};

// ============================================================================
// 5. Canonical NT Inbv Functions
// ============================================================================
inline void InbvEnableBootDriver(BOOLEAN enable) {
    InbvController::get().enableBootDriver(enable != FALSE);
}

inline void InbvAcquireDisplayOwnership() {
    InbvController::get().acquireDisplayOwnership();
}

inline void InbvResetDisplay() {
    InbvController::get().resetDisplay();
}

inline void InbvNotifyDisplayOwnershipLost(BOOLEAN resetDisplay) {
    InbvController::get().notifyDisplayOwnershipLost(resetDisplay != FALSE);
}

inline BOOLEAN InbvCheckDisplayOwnership() {
    return InbvController::get().checkDisplayOwnership() ? TRUE : FALSE;
}

inline void InbvSolidColorFill(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom, uint32_t colorIndex) {
    InbvController::get().solidColorFill(left, top, right, bottom, colorIndex);
}

inline void InbvDisplayString(const char* str) {
    InbvController::get().displayString(str);
}

inline void InbvSetTextColor(uint32_t colorIndex) {
    InbvController::get().setTextColor(colorIndex);
}

inline void InbvSetProgressBarCoordinates(uint32_t left, uint32_t top) {
    InbvController::get().setProgressBarCoordinates(left, top);
}

inline void InbvUpdateProgressBar(uint32_t percentage) {
    InbvController::get().updateProgressBar(percentage);
}

inline void InbvInstallDisplayStringFilter(INBV_DISPLAY_STRING_FILTER filter) {
    InbvController::get().installDisplayStringFilter(filter);
}

} // namespace micant::inbv

namespace micant {
    using inbv::InbvEnableBootDriver;
    using inbv::InbvAcquireDisplayOwnership;
    using inbv::InbvResetDisplay;
    using inbv::InbvNotifyDisplayOwnershipLost;
    using inbv::InbvCheckDisplayOwnership;
    using inbv::InbvSolidColorFill;
    using inbv::InbvDisplayString;
    using inbv::InbvSetTextColor;
    using inbv::InbvSetProgressBarCoordinates;
    using inbv::InbvUpdateProgressBar;
    using inbv::InbvInstallDisplayStringFilter;
}
