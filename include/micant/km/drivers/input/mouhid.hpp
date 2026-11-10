#pragma once

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "micant/km/drivers/input/hidclass.hpp"
#include "micant/km/drivers/input/mouclass.hpp"

namespace micant::km::hid {

class MouHidDriver {
public:
    static MouHidDriver& Instance() { static MouHidDriver instance; return instance; }

    HIDP_STATUS process(uintptr_t collectionId, PHIDP_PREPARSED_DATA preparsedData,
        const uint8_t* report, size_t reportLength) {
        if (!preparsedData || !report || reportLength > UINT32_MAX || collectionId == 0)
            return HIDP_STATUS_INVALID_REPORT_LENGTH;

        std::array<USAGE, 64> activeUsages{};
        ULONG usageCount = static_cast<ULONG>(activeUsages.size());
        const HIDP_STATUS usageStatus = HidP_GetUsages(HidP_Input, HID_USAGE_PAGE_BUTTON, 0,
            activeUsages.data(), &usageCount, preparsedData,
            reinterpret_cast<PCHAR>(const_cast<uint8_t*>(report)), static_cast<ULONG>(reportLength));
        if (usageStatus != HIDP_STATUS_SUCCESS) return usageStatus;

        uint32_t buttons = 0;
        for (ULONG index = 0; index < usageCount; ++index) {
            if (activeUsages[index] >= 1 && activeUsages[index] <= 5)
                buttons |= 1u << (activeUsages[index] - 1);
        }

        int32_t x = 0, y = 0, wheel = 0, horizontalWheel = 0;
        bool absoluteX = false, absoluteY = false;
        const HIDP_STATUS xStatus = value(preparsedData, report, reportLength,
            HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_X, x, &absoluteX);
        const HIDP_STATUS yStatus = value(preparsedData, report, reportLength,
            HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_Y, y, &absoluteY);
        const HIDP_STATUS wheelStatus = value(preparsedData, report, reportLength,
            HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_WHEEL, wheel);
        constexpr USAGE HID_USAGE_CONSUMER_AC_PAN = 0x0238;
        const HIDP_STATUS horizontalWheelStatus = value(preparsedData, report, reportLength,
            HID_USAGE_PAGE_CONSUMER, HID_USAGE_CONSUMER_AC_PAN, horizontalWheel);
        if (xStatus != HIDP_STATUS_SUCCESS && xStatus != HIDP_STATUS_USAGE_NOT_FOUND) return xStatus;
        if (yStatus != HIDP_STATUS_SUCCESS && yStatus != HIDP_STATUS_USAGE_NOT_FOUND) return yStatus;
        if (wheelStatus != HIDP_STATUS_SUCCESS && wheelStatus != HIDP_STATUS_USAGE_NOT_FOUND) return wheelStatus;
        if (horizontalWheelStatus != HIDP_STATUS_SUCCESS &&
            horizontalWheelStatus != HIDP_STATUS_USAGE_NOT_FOUND) return horizontalWheelStatus;

        MOUSE_INPUT_DATA packet{};
        packet.Flags = (absoluteX || absoluteY) ? MOUSE_MOVE_ABSOLUTE : MOUSE_MOVE_RELATIVE;
        packet.LastX = xStatus == HIDP_STATUS_SUCCESS ? x : 0;
        packet.LastY = yStatus == HIDP_STATUS_SUCCESS ? y : 0;
        packet.RawButtons = buttons;
        if (!buttonTransitions(collectionId, buttons, packet.ButtonFlags))
            return HIDP_STATUS_INTERNAL_ERROR;
        if (wheelStatus == HIDP_STATUS_SUCCESS && wheel != 0) {
            packet.ButtonFlags |= MOUSE_WHEEL;
            packet.ButtonData = wheelDelta(wheel);
        } else if (horizontalWheelStatus == HIDP_STATUS_SUCCESS && horizontalWheel != 0) {
            packet.ButtonFlags |= MOUSE_HWHEEL;
            packet.ButtonData = wheelDelta(horizontalWheel);
        }

        if (packet.LastX != 0 || packet.LastY != 0 || packet.ButtonFlags != 0) {
            ULONG consumed = 0;
            MouseClassServiceCallback(nullptr, &packet, &packet + 1, &consumed);
        }
        return HIDP_STATUS_SUCCESS;
    }

    void removeCollection(uintptr_t collectionId) {
        ke::SpinLockGuard guard(m_lock);
        if (auto* state = findCollection(collectionId, false)) *state = {};
    }

private:
    static constexpr size_t CollectionCapacity = 64;
    struct CollectionState { uintptr_t id{0}; uint32_t buttons{0}; };

    static uint16_t wheelDelta(int32_t wheel) {
        const int64_t scaled = static_cast<int64_t>(wheel) * 120;
        return static_cast<uint16_t>(std::clamp<int64_t>(scaled, INT16_MIN, INT16_MAX));
    }

    static HIDP_STATUS value(PHIDP_PREPARSED_DATA preparsedData, const uint8_t* report,
        size_t reportLength, USAGE usagePage, USAGE usage, int32_t& result,
        bool* isAbsolute = nullptr) {
        if (isAbsolute) *isAbsolute = false;
        for (const auto& cap : preparsedData->InputValueCaps) {
            if (cap.UsagePage != usagePage) continue;
            const bool matches = cap.IsRange == TRUE_B
                ? usage >= cap.Range.UsageMin && usage <= cap.Range.UsageMax
                : usage == cap.NotRange.Usage;
            if (!matches) continue;
            ULONG raw = 0;
            const auto status = HidP_GetUsageValue(HidP_Input, usagePage, cap.LinkCollection,
                usage, &raw, preparsedData, reinterpret_cast<PCHAR>(const_cast<uint8_t*>(report)),
                static_cast<ULONG>(reportLength));
            if (status != HIDP_STATUS_SUCCESS) return status;
            result = static_cast<int32_t>(raw);
            if (isAbsolute && cap.IsAbsolute == TRUE_B) {
                *isAbsolute = true;
                const int64_t range = static_cast<int64_t>(cap.LogicalMax) - cap.LogicalMin;
                if (range > 0) {
                    const int64_t offset = std::clamp<int64_t>(result, cap.LogicalMin, cap.LogicalMax) - cap.LogicalMin;
                    result = static_cast<int32_t>((offset * 65535) / range);
                }
            }
            return status;
        }
        ULONG raw = 0;
        const auto status = HidP_GetUsageValue(HidP_Input, usagePage, 0, usage,
            &raw, preparsedData, reinterpret_cast<PCHAR>(const_cast<uint8_t*>(report)),
            static_cast<ULONG>(reportLength));
        if (status == HIDP_STATUS_SUCCESS) result = static_cast<int32_t>(raw);
        return status;
    }

    CollectionState* findCollection(uintptr_t id, bool create) {
        CollectionState* freeSlot = nullptr;
        for (auto& state : m_collections) {
            if (state.id == id && id != 0) return &state;
            if (state.id == 0 && !freeSlot) freeSlot = &state;
        }
        if (create && freeSlot) { freeSlot->id = id; return freeSlot; }
        return nullptr;
    }

    bool buttonTransitions(uintptr_t collectionId, uint32_t buttons, uint16_t& flags) {
        static constexpr uint16_t downFlags[5] = {MOUSE_LEFT_BUTTON_DOWN, MOUSE_RIGHT_BUTTON_DOWN,
            MOUSE_MIDDLE_BUTTON_DOWN, MOUSE_BUTTON_4_DOWN, MOUSE_BUTTON_5_DOWN};
        static constexpr uint16_t upFlags[5] = {MOUSE_LEFT_BUTTON_UP, MOUSE_RIGHT_BUTTON_UP,
            MOUSE_MIDDLE_BUTTON_UP, MOUSE_BUTTON_4_UP, MOUSE_BUTTON_5_UP};
        ke::SpinLockGuard guard(m_lock);
        auto* state = findCollection(collectionId, true);
        if (!state) return false;
        const uint32_t changed = state->buttons ^ buttons;
        for (size_t index = 0; index < 5; ++index) {
            if ((changed & (1u << index)) == 0) continue;
            flags |= (buttons & (1u << index)) ? downFlags[index] : upFlags[index];
        }
        state->buttons = buttons;
        return true;
    }

    ke::SpinLock m_lock{};
    std::array<CollectionState, CollectionCapacity> m_collections{};
};

inline void MouHidProcessInputReport(uintptr_t collectionId, PHIDP_PREPARSED_DATA preparsedData,
    const uint8_t* report, size_t reportLength) {
    (void)MouHidDriver::Instance().process(collectionId, preparsedData, report, reportLength);
}

inline void InitializeMouHidDriver(io::DriverObject& driver) {
    driver.driverName = L"\\Driver\\Mouhid";
}

} // namespace micant::km::hid
