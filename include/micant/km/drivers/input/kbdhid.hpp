#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "micant/km/drivers/input/hidclass.hpp"
#include "micant/km/drivers/input/kbdclass.hpp"
#include "micant/km/drivers/input/hid_scancode.hpp"

namespace micant::km::hid {

class KbdHidDriver {
public:
    static KbdHidDriver& Instance() { static KbdHidDriver instance; return instance; }

    HIDP_STATUS process(uintptr_t collectionId, PHIDP_PREPARSED_DATA preparsedData,
        const uint8_t* report, size_t reportLength) {
        if (!preparsedData || !report || reportLength > UINT32_MAX || collectionId == 0)
            return HIDP_STATUS_INVALID_REPORT_LENGTH;
        std::array<USAGE, 128> current{};
        ULONG currentCount = static_cast<ULONG>(current.size());
        auto status = HidP_GetUsages(HidP_Input, HID_USAGE_PAGE_KEYBOARD, 0, current.data(),
            &currentCount, preparsedData, reinterpret_cast<PCHAR>(const_cast<uint8_t*>(report)),
            static_cast<ULONG>(reportLength));
        if (status != HIDP_STATUS_SUCCESS) return status;

        std::array<KEYBOARD_INPUT_DATA, 256> packets{};
        size_t packetCount = 0;
        {
            ke::SpinLockGuard guard(m_lock);
            CollectionState* state = findCollection(collectionId, true);
            if (!state) return HIDP_STATUS_INTERNAL_ERROR;
            auto& previous = state->pressed;
            for (size_t index = 0; index < previous.count; ++index) {
                const USAGE usage = previous.usages[index];
                if (!contains(current.data(), currentCount, usage))
                    appendPacket(packets, packetCount, usage, KEY_BREAK);
            }
            for (ULONG index = 0; index < currentCount; ++index) {
                const USAGE usage = current[index];
                if (!contains(previous.usages.data(), previous.count, usage))
                    appendPacket(packets, packetCount, usage, KEY_MAKE);
            }
            previous.count = std::min<size_t>(currentCount, previous.usages.size());
            std::copy_n(current.begin(), previous.count, previous.usages.begin());
        }

        if (packetCount != 0) {
            ULONG consumed = 0;
            KeyboardClassServiceCallback(KbdClassDriver::Instance().classDevice(), packets.data(),
                packets.data() + packetCount, &consumed);
        }
        return HIDP_STATUS_SUCCESS;
    }

    void removeCollection(uintptr_t collectionId) {
        ke::SpinLockGuard guard(m_lock);
        if (auto* state = findCollection(collectionId, false)) *state = {};
    }

private:
    static constexpr size_t CollectionCapacity = 64;
    struct PressedKeys { std::array<USAGE, 128> usages{}; size_t count{0}; };
    struct CollectionState { uintptr_t id{0}; PressedKeys pressed{}; };
    CollectionState* findCollection(uintptr_t id, bool create) {
        CollectionState* freeSlot = nullptr;
        for (auto& state : m_pressed) {
            if (state.id == id && id != 0) return &state;
            if (state.id == 0 && !freeSlot) freeSlot = &state;
        }
        if (create && freeSlot) { freeSlot->id = id; return freeSlot; }
        return nullptr;
    }
    static bool contains(const USAGE* values, size_t count, USAGE value) {
        for (size_t index = 0; index < count; ++index) if (values[index] == value) return true;
        return false;
    }
    static bool scanCode(USAGE usage, uint16_t& code, uint16_t& flags) {
        return hidUsageToScanCode(usage, code, flags);
    }
    static void appendPacket(std::array<KEYBOARD_INPUT_DATA, 256>& packets, size_t& count,
        USAGE usage, uint16_t transition) {
        if (count == packets.size()) return;
        uint16_t code = 0, flags = 0;
        if (!scanCode(usage, code, flags)) return;
        packets[count++] = KEYBOARD_INPUT_DATA{0, code, static_cast<uint16_t>(flags | transition), 0, 0};
    }

    ke::SpinLock m_lock{};
    std::array<CollectionState, CollectionCapacity> m_pressed{};
};

inline void KbdHidProcessInputReport(uintptr_t collectionId, PHIDP_PREPARSED_DATA preparsedData,
    const uint8_t* report, size_t reportLength) {
    (void)KbdHidDriver::Instance().process(collectionId, preparsedData, report, reportLength);
}

inline void InitializeKbdHidDriver(io::DriverObject& driver) {
    driver.driverName = L"\\Driver\\Kbdhid";
}

} // namespace micant::km::hid
