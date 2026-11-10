#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "micant/km/drivers/input/hidclass.hpp"
#include "micant/km/drivers/input/kbdclass.hpp"

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
        flags = 0;
        if (usage >= 0xE0 && usage <= 0xE7) {
            static constexpr uint16_t modifierCodes[8] = {0x1D, 0x2A, 0x38, 0x5B, 0x1D, 0x36, 0x38, 0x5C};
            code = modifierCodes[usage - 0xE0];
            if (usage == 0xE3 || usage == 0xE4 || usage == 0xE6 || usage == 0xE7) flags |= KEY_E0;
            return true;
        }
        if (usage >= 0x04 && usage <= 0x1D) {
            static constexpr uint8_t letters[26] = {
                0x1E,0x30,0x2E,0x20,0x12,0x21,0x22,0x23,0x17,0x24,0x25,0x26,0x32,
                0x31,0x18,0x19,0x10,0x13,0x1F,0x14,0x16,0x2F,0x11,0x2D,0x15,0x2C};
            code = letters[usage - 0x04]; return true;
        }
        if (usage >= 0x1E && usage <= 0x27) {
            static constexpr uint8_t digits[10] = {0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B};
            code = digits[usage - 0x1E]; return true;
        }
        switch (usage) {
        case 0x28: code = 0x1C; break; case 0x29: code = 0x01; break;
        case 0x2A: code = 0x0E; break; case 0x2B: code = 0x0F; break;
        case 0x2C: code = 0x39; break; case 0x2D: code = 0x0C; break;
        case 0x2E: code = 0x0D; break; case 0x2F: code = 0x1A; break;
        case 0x30: code = 0x1B; break; case 0x31: code = 0x2B; break;
        case 0x33: code = 0x27; break; case 0x34: code = 0x28; break;
        case 0x35: code = 0x29; break; case 0x36: code = 0x33; break;
        case 0x37: code = 0x34; break; case 0x38: code = 0x35; break;
        case 0x39: code = 0x3A; break; case 0x3A: code = 0x3B; break;
        case 0x3B: code = 0x3C; break; case 0x3C: code = 0x3D; break;
        case 0x3D: code = 0x3E; break; case 0x3E: code = 0x3F; break;
        case 0x3F: code = 0x40; break; case 0x40: code = 0x41; break;
        case 0x41: code = 0x42; break; case 0x42: code = 0x43; break;
        case 0x43: code = 0x44; break; case 0x44: code = 0x57; break;
        case 0x45: code = 0x58; break; case 0x47: code = 0x46; break;
        case 0x46: case 0x48: return false; // Print Screen and Pause require multi-byte sequences.
        case 0x49: code = 0x52; flags |= KEY_E0; break; // Insert
        case 0x4A: code = 0x47; flags |= KEY_E0; break; // Home
        case 0x4B: code = 0x49; flags |= KEY_E0; break; // Page Up
        case 0x4C: code = 0x53; flags |= KEY_E0; break; // Delete
        case 0x4D: code = 0x4F; flags |= KEY_E0; break; // End
        case 0x4E: code = 0x51; flags |= KEY_E0; break; // Page Down
        case 0x4F: code = 0x4D; flags |= KEY_E0; break; // Right
        case 0x50: code = 0x4B; flags |= KEY_E0; break; // Left
        case 0x51: code = 0x50; flags |= KEY_E0; break; // Down
        case 0x52: code = 0x48; flags |= KEY_E0; break; // Up
        case 0x53: code = 0x45; break;
        case 0x54: code = 0x35; flags |= KEY_E0; break;
        case 0x55: code = 0x37; break; case 0x56: code = 0x4A; break;
        case 0x57: code = 0x4E; break; case 0x58: code = 0x1C; flags |= KEY_E0; break;
        case 0x59: code = 0x4F; break; case 0x5A: code = 0x50; break;
        case 0x5B: code = 0x51; break; case 0x5C: code = 0x4B; break;
        case 0x5D: code = 0x4C; break; case 0x5E: code = 0x4D; break;
        case 0x5F: code = 0x47; break; case 0x60: code = 0x48; break;
        case 0x61: code = 0x49; break; case 0x62: code = 0x52; break;
        case 0x63: code = 0x53; break;
        default: return false;
        }
        return true;
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
