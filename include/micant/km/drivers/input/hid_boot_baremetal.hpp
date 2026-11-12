#pragma once

#include "micant/km/drivers/input/hid_scancode.hpp"
#include "micant/km/drivers/input/mouclass_contract.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace micant::km::hid {
// Per-interface boot protocol state; disconnect generates releases through the
// same class callbacks as normal reports. Class queues accept each batch whole.
class BootHidCollection {
    uint8_t previousKeys_[8]{};
    uint8_t previousButtons_{};
    uint16_t unitId_{};
    static bool contains(const uint8_t* report, uint8_t key) noexcept {
        for (unsigned i = 2; i < 8; ++i) if (report[i] == key) return true;
        return false;
    }
public:
    void setUnitId(uint16_t unit) noexcept { unitId_ = unit; }
    bool keyboard(const uint8_t* report, size_t length) noexcept {
        if (!report || length != 8 || report[1]) return false;
        for (unsigned i = 2; i < 8; ++i) {
            if (report[i] && report[i] < 4) return false; // rollover/error: keep prior state
            if (report[i]) for (unsigned j = 2; j < i; ++j) if (report[i] == report[j]) return false;
        }
        KEYBOARD_INPUT_DATA packets[28]{};
        size_t count{};
        const auto append = [&](uint8_t usage, bool release) noexcept {
            uint16_t code{}, flags{};
            if (!hidUsageToScanCode(usage, code, flags)) return;
            packets[count].UnitId = unitId_;
            packets[count].MakeCode = code;
            packets[count++].Flags = flags | (release ? KEY_BREAK : KEY_MAKE);
        };
        for (unsigned bit = 0; bit < 8; ++bit)
            if ((previousKeys_[0] & (1u << bit)) && !(report[0] & (1u << bit))) append(0xe0 + bit, true);
        for (unsigned i = 2; i < 8; ++i)
            if (previousKeys_[i] && !contains(report, previousKeys_[i])) append(previousKeys_[i], true);
        for (unsigned bit = 0; bit < 8; ++bit)
            if (!(previousKeys_[0] & (1u << bit)) && (report[0] & (1u << bit))) append(0xe0 + bit, false);
        for (unsigned i = 2; i < 8; ++i)
            if (report[i] && !contains(previousKeys_, report[i])) append(report[i], false);
        uint32_t consumed{};
        if (count) KeyboardClassServiceCallback(nullptr, packets, packets + count, &consumed);
        if (consumed != count) return false;
        std::memcpy(previousKeys_, report, 8);
        return true;
    }
    bool mouse(const uint8_t* report, size_t length) noexcept {
        if (!report || length != 3) return false;
        const uint8_t buttons = report[0] & 7u;
        MOUSE_INPUT_DATA packet{};
        packet.UnitId = unitId_;
        static constexpr uint16_t down[]{MOUSE_LEFT_BUTTON_DOWN, MOUSE_RIGHT_BUTTON_DOWN, MOUSE_MIDDLE_BUTTON_DOWN};
        static constexpr uint16_t up[]{MOUSE_LEFT_BUTTON_UP, MOUSE_RIGHT_BUTTON_UP, MOUSE_MIDDLE_BUTTON_UP};
        for (unsigned bit = 0; bit < 3; ++bit) {
            if ((buttons & (1u << bit)) && !(previousButtons_ & (1u << bit))) packet.ButtonFlags |= down[bit];
            if (!(buttons & (1u << bit)) && (previousButtons_ & (1u << bit))) packet.ButtonFlags |= up[bit];
        }
        packet.RawButtons = buttons;
        packet.LastX = static_cast<int8_t>(report[1]); packet.LastY = static_cast<int8_t>(report[2]);
        if (packet.LastX || packet.LastY || packet.ButtonFlags) {
            uint32_t consumed{};
            MouseClassServiceCallback(nullptr, &packet, &packet + 1, &consumed);
            if (consumed != 1) return false;
        }
        previousButtons_ = buttons;
        return true;
    }
    bool disconnect() noexcept {
        const uint8_t emptyKeys[8]{};
        const uint8_t emptyMouse[3]{};
        return keyboard(emptyKeys, sizeof(emptyKeys)) && mouse(emptyMouse, sizeof(emptyMouse));
    }
};
}
