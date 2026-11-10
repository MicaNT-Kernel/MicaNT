#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "micant/km/drivers/input/mouclass.hpp"
#include "micant/km/core/ke.hpp"
#include "micant/um/servers/scm.hpp"
#include "micant/um/subsystems/winbase/version.hpp"

namespace micant::km::input {

enum class SerialMouseByteResult : uint8_t { Ignored, PacketStarted, PacketContinued, PacketDelivered };

class SerMouseDriver {
public:
    static SerMouseDriver& Instance() { static SerMouseDriver instance; return instance; }

    SerialMouseByteResult processByte(uintptr_t portId, uint8_t value) {
        if (portId == 0) return SerialMouseByteResult::Ignored;

        micant::km::hid::MOUSE_INPUT_DATA packet{};
        bool deliver = false;
        SerialMouseByteResult result = SerialMouseByteResult::Ignored;
        {
            ke::SpinLockGuard guard(m_lock);
            PortState* state = findPort(portId, true);
            if (!state) return SerialMouseByteResult::Ignored;

            // Microsoft serial mouse packets have one tagged header and two
            // six-bit motion bytes. A new tagged byte always re-synchronizes.
            if ((value & 0x40) != 0) {
                state->packet[0] = value;
                state->count = 1;
                return SerialMouseByteResult::PacketStarted;
            }
            if (state->count == 0) return SerialMouseByteResult::Ignored;

            state->packet[state->count++] = value & 0x3F;
            if (state->count < state->packet.size()) return SerialMouseByteResult::PacketContinued;
            state->count = 0;

            const uint8_t header = state->packet[0];
            const uint8_t xBits = static_cast<uint8_t>(((header & 0x03) << 6) | state->packet[1]);
            const uint8_t yBits = static_cast<uint8_t>(((header & 0x0C) << 4) | state->packet[2]);
            const uint8_t buttons = static_cast<uint8_t>(((header & 0x20) ? 0x01 : 0) |
                ((header & 0x10) ? 0x02 : 0));

            packet.LastX = static_cast<int8_t>(xBits);
            packet.LastY = static_cast<int8_t>(yBits);
            packet.RawButtons = buttons;
            packet.ButtonFlags = buttonTransitions(state->buttons, buttons);
            state->buttons = buttons;
            deliver = packet.LastX != 0 || packet.LastY != 0 || packet.ButtonFlags != 0;
            result = SerialMouseByteResult::PacketDelivered;
        }

        if (deliver) {
            uint32_t consumed = 0;
            micant::km::hid::MouseClassServiceCallback(nullptr, &packet, &packet + 1, &consumed);
        }
        return result;
    }

    void removePort(uintptr_t portId) {
        ke::SpinLockGuard guard(m_lock);
        if (auto* state = findPort(portId, false)) *state = {};
    }

private:
    static constexpr size_t PortCapacity = 16;
    struct PortState { uintptr_t id{0}; std::array<uint8_t, 3> packet{}; uint8_t count{0}; uint8_t buttons{0}; };

    static uint16_t buttonTransitions(uint8_t previous, uint8_t current) {
        uint16_t flags = 0;
        if ((previous & 0x01) != (current & 0x01))
            flags |= (current & 0x01) ? micant::km::hid::MOUSE_LEFT_BUTTON_DOWN
                                     : micant::km::hid::MOUSE_LEFT_BUTTON_UP;
        if ((previous & 0x02) != (current & 0x02))
            flags |= (current & 0x02) ? micant::km::hid::MOUSE_RIGHT_BUTTON_DOWN
                                     : micant::km::hid::MOUSE_RIGHT_BUTTON_UP;
        return flags;
    }

    PortState* findPort(uintptr_t id, bool create) {
        PortState* freeSlot = nullptr;
        for (auto& state : m_ports) {
            if (state.id == id && id != 0) return &state;
            if (state.id == 0 && !freeSlot) freeSlot = &state;
        }
        if (create && freeSlot) { freeSlot->id = id; return freeSlot; }
        return nullptr;
    }

    ke::SpinLock m_lock{};
    std::array<PortState, PortCapacity> m_ports{};
};

inline void InitializeSerMouseDriver(micant::io::DriverObject& driver) {
    driver.driverName = L"\\Driver\\Sermouse";

    auto service = std::make_shared<micant::scm::ServiceRecord>();
    service->serviceName = L"sermouse";
    service->displayName = L"Serial Mouse Port Driver";
    service->serviceType = micant::scm::SERVICE_KERNEL_DRIVER;
    service->startType = micant::scm::SERVICE_SYSTEM_START;
    service->errorControl = micant::scm::SERVICE_ERROR_NORMAL;
    service->binaryPath = L"\\SystemRoot\\System32\\drivers\\sermouse.sys";
    service->status.dwCurrentState = scm::SERVICE_RUNNING;
    micant::scm::ServiceControlManager::get().registerServiceRecord(service);
    micant::version::VersionDatabase::Instance().RegisterModule("sermouse.sys", "10.0.26100.1",
        "Serial Mouse Port Driver");
}

} // namespace micant::km::input
