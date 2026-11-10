#pragma once

#include <cstddef>
#include <cstdint>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"

namespace micant::io { struct DeviceObject; }

namespace micant::km::hid {

inline constexpr uint32_t IOCTL_MOUSE_QUERY_ATTRIBUTES = 0x000F0000;

inline constexpr uint16_t MOUSE_MOVE_RELATIVE      = 0x0000;
inline constexpr uint16_t MOUSE_MOVE_ABSOLUTE      = 0x0001;
inline constexpr uint16_t MOUSE_VIRTUAL_DESKTOP    = 0x0002;
inline constexpr uint16_t MOUSE_ATTRIBUTES_CHANGED = 0x0004;

inline constexpr uint16_t MOUSE_LEFT_BUTTON_DOWN   = 0x0001;
inline constexpr uint16_t MOUSE_LEFT_BUTTON_UP     = 0x0002;
inline constexpr uint16_t MOUSE_RIGHT_BUTTON_DOWN  = 0x0004;
inline constexpr uint16_t MOUSE_RIGHT_BUTTON_UP    = 0x0008;
inline constexpr uint16_t MOUSE_MIDDLE_BUTTON_DOWN = 0x0010;
inline constexpr uint16_t MOUSE_MIDDLE_BUTTON_UP   = 0x0020;
inline constexpr uint16_t MOUSE_BUTTON_4_DOWN      = 0x0040;
inline constexpr uint16_t MOUSE_BUTTON_4_UP        = 0x0080;
inline constexpr uint16_t MOUSE_BUTTON_5_DOWN      = 0x0100;
inline constexpr uint16_t MOUSE_BUTTON_5_UP        = 0x0200;
inline constexpr uint16_t MOUSE_WHEEL              = 0x0400;
inline constexpr uint16_t MOUSE_HWHEEL             = 0x0800;

struct MOUSE_INPUT_DATA {
    uint16_t UnitId{0};
    uint16_t Flags{MOUSE_MOVE_RELATIVE};
    union {
        uint32_t Buttons{0};
        struct {
            uint16_t ButtonFlags;
            uint16_t ButtonData;
        };
    };
    uint32_t RawButtons{0};
    int32_t LastX{0};
    int32_t LastY{0};
    uint32_t ExtraInformation{0};
};

struct MOUSE_ATTRIBUTES {
    uint16_t MouseIdentifier{0x0100};
    uint16_t NumberOfButtons{5};
    uint16_t SampleRate{0};
    uint32_t InputDataQueueLength{256 * sizeof(MOUSE_INPUT_DATA)};
};

struct MOUSE_UNIT_ID_PARAMETER {
    uint16_t UnitId{0};
};

static_assert(sizeof(MOUSE_INPUT_DATA) == 24);
static_assert(sizeof(MOUSE_ATTRIBUTES) == 12);

void WINAPI MouseClassServiceCallback(
    io::DeviceObject* deviceObject,
    MOUSE_INPUT_DATA* inputDataStart,
    MOUSE_INPUT_DATA* inputDataEnd,
    uint32_t* inputDataConsumed);

bool MouClassPopInputData(MOUSE_INPUT_DATA& data);
size_t MouClassQueueDepth();
NtStatus MouClassQueryAttributes(void* outputBuffer, size_t outputLength);
void MouClassResetState();
uint64_t MouClassPacketsQueuedTotal();

} // namespace micant::km::hid
