#pragma once

#include <cstddef>
#include <cstdint>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"

namespace micant::io { struct DeviceObject; }

namespace micant::km::hid {

inline constexpr uint32_t IOCTL_KEYBOARD_QUERY_ATTRIBUTES = 0x000B0000;
inline constexpr uint32_t IOCTL_KEYBOARD_QUERY_INDICATORS = 0x000B0040;
inline constexpr uint32_t IOCTL_KEYBOARD_SET_INDICATORS = 0x000B0008;
inline constexpr uint32_t IOCTL_KEYBOARD_QUERY_TYPEMATIC = 0x000B0020;
inline constexpr uint32_t IOCTL_KEYBOARD_SET_TYPEMATIC = 0x000B0004;

inline constexpr uint16_t KEY_MAKE = 0x0000;
inline constexpr uint16_t KEY_BREAK = 0x0001;
inline constexpr uint16_t KEY_E0 = 0x0002;
inline constexpr uint16_t KEY_E1 = 0x0004;
inline constexpr uint16_t KEYBOARD_SCROLL_LOCK_ON = 0x0001;
inline constexpr uint16_t KEYBOARD_NUM_LOCK_ON = 0x0002;
inline constexpr uint16_t KEYBOARD_CAPS_LOCK_ON = 0x0004;

struct KEYBOARD_INPUT_DATA {
    uint16_t UnitId{0};
    uint16_t MakeCode{0};
    uint16_t Flags{KEY_MAKE};
    uint16_t Reserved{0};
    uint32_t ExtraInformation{0};
};

struct KEYBOARD_INDICATOR_PARAMETERS { uint16_t UnitId{0}; uint16_t LedFlags{0}; };
struct KEYBOARD_TYPEMATIC_PARAMETERS { uint16_t UnitId{0}; uint16_t Rate{30}; uint16_t Delay{250}; };
struct KEYBOARD_ATTRIBUTES {
    uint8_t KeyboardMode{1};
    uint8_t NumberOfFunctionKeys{12};
    uint8_t NumberOfIndicators{3};
    uint16_t NumberOfKeysTotal{104};
    uint32_t InputDataQueueLength{128 * sizeof(KEYBOARD_INPUT_DATA)};
    KEYBOARD_TYPEMATIC_PARAMETERS KeyRepeatMinimum{0, 2, 250};
    KEYBOARD_TYPEMATIC_PARAMETERS KeyRepeatMaximum{0, 30, 1000};
};

static_assert(sizeof(KEYBOARD_INPUT_DATA) == 12);
static_assert(sizeof(KEYBOARD_ATTRIBUTES) == 24);

void WINAPI KeyboardClassServiceCallback(io::DeviceObject* deviceObject,
    KEYBOARD_INPUT_DATA* inputDataStart, KEYBOARD_INPUT_DATA* inputDataEnd,
    uint32_t* inputDataConsumed);

bool KbdClassPopInputData(KEYBOARD_INPUT_DATA& data);
size_t KbdClassQueueDepth();
uint64_t KbdClassPacketsQueuedTotal();
void KbdClassResetState();
NtStatus KbdClassQueryAttributes(void* outputBuffer, size_t outputLength);
NtStatus KbdClassDeviceControl(uint32_t code, void* buffer, size_t inputLength, size_t outputLength);

} // namespace micant::km::hid
