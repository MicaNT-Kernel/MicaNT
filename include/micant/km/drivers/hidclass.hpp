// ============================================================================
// MicaNT: Sovereign Human Interface Device (HID) Class Driver Architecture,
//         Report Descriptor Bytecode Parser & Kernel Input Stack (hidclass.hpp)
// Sovereign System Domain: TitanHID / SovereignInput
//
// Description:
//   Clean-room implementation of the Microsoft Windows Human Interface Device
//   (HID) Class Driver (hidclass.sys), HID Report Descriptor Bytecode Parser
//   (hidparse.sys), USB HID Miniport Driver (hidusb.sys), Keyboard Class Driver
//   (kbdclass.sys), Mouse Class Driver (mouclass.sys), PS/2 i8042 Controller
//   Port Driver (i8042prt.sys), and User-Mode HID Client Library (hid.dll).
//
//   Provides full binary HID Report Descriptor parsing (Main, Global, and Local
//   items, nested Collections, Usage Pages, Logical/Physical bounds, Report IDs,
//   Button & Value capability extraction), arbitrary bitfield unpacking/packing
//   (HidP_GetUsages, HidP_SetUsages, HidP_GetUsageValue, HidP_SetUsageValue),
//   Ping-Pong Interrupt URB pipelines over usbxhci.sys, Keyboard/Mouse Class
//   ring buffers (KEYBOARD_INPUT_DATA / MOUSE_INPUT_DATA), i8042 PS/2 port
//   0x60/0x64 ISR/DPC simulation, and Raw Input Thread (RIT) delivery into
//   the Win32k Window Manager.
//
// Clean-Room Engineering Reference & Standards:
//   - USB Implementers Forum (USB-IF): Device Class Definition for Human
//     Interface Devices (HID) Version 1.11
//   - USB-IF: HID Usage Tables (HUT) Version 1.4
//   - Microsoft Open win32metadata repository: Windows.Win32.Devices.HumanInterfaceDevice
//   - ISO/IEC 14882:2023 C++ Standard (Pure ISO C++23)
//   - Zero proprietary, decompiled, or leaked Microsoft source code.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <array>
#include <unordered_map>
#include <map>
#include <deque>
#include <mutex>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <optional>
#include <span>
#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/um/native/janusldr.hpp"
#include "micant/um/servers/scm.hpp"
#include "micant/um/subsystems/version.hpp"
#include "micant/km/executive/io.hpp"
#include "micant/km/drivers/xhci.hpp"
#include "micant/km/win32k/win32k.hpp"

namespace micant::km::hid {

// ============================================================================
// 1. Standard HID, Keyboard & Mouse Types, Status Codes & IOCTLs
// ============================================================================

using BOOL    = int32_t;
using BOOLEAN = uint8_t;
using USHORT  = uint16_t;
using SHORT   = int16_t;
using ULONG   = uint32_t;
using LONG    = int32_t;
using UCHAR   = uint8_t;
using PUCHAR  = uint8_t*;
using PCHAR   = char*;
using PWCHAR  = wchar_t*;
using PVOID   = void*;
using HANDLE  = win32::HANDLE;
using USAGE   = uint16_t;
using PUSAGE  = uint16_t*;

inline constexpr BOOL    TRUE_VAL  = 1;
inline constexpr BOOL    FALSE_VAL = 0;
inline constexpr BOOLEAN TRUE_B    = 1;
inline constexpr BOOLEAN FALSE_B   = 0;

// HIDP NTSTATUS Facility (0x11 = FACILITY_HID_ERROR_CODE)
using HIDP_STATUS = int32_t;
inline constexpr HIDP_STATUS HIDP_STATUS_SUCCESS                    = 0x00110000;
inline constexpr HIDP_STATUS HIDP_STATUS_NULL                       = static_cast<HIDP_STATUS>(0x80110001);
inline constexpr HIDP_STATUS HIDP_STATUS_INVALID_PREPARSED_DATA     = static_cast<HIDP_STATUS>(0xC0110001);
inline constexpr HIDP_STATUS HIDP_STATUS_INVALID_REPORT_TYPE        = static_cast<HIDP_STATUS>(0xC0110002);
inline constexpr HIDP_STATUS HIDP_STATUS_INVALID_REPORT_LENGTH      = static_cast<HIDP_STATUS>(0xC0110003);
inline constexpr HIDP_STATUS HIDP_STATUS_USAGE_NOT_FOUND            = static_cast<HIDP_STATUS>(0xC0110004);
inline constexpr HIDP_STATUS HIDP_STATUS_VALUE_OUT_OF_RANGE         = static_cast<HIDP_STATUS>(0xC0110005);
inline constexpr HIDP_STATUS HIDP_STATUS_BAD_LOG_PHY_VALUES         = static_cast<HIDP_STATUS>(0xC0110006);
inline constexpr HIDP_STATUS HIDP_STATUS_BUFFER_TOO_SMALL           = static_cast<HIDP_STATUS>(0xC0110007);
inline constexpr HIDP_STATUS HIDP_STATUS_INTERNAL_ERROR             = static_cast<HIDP_STATUS>(0xC0110008);
inline constexpr HIDP_STATUS HIDP_STATUS_I8042_TRANS_UNKNOWN        = static_cast<HIDP_STATUS>(0xC0110009);
inline constexpr HIDP_STATUS HIDP_STATUS_INCOMPATIBLE_REPORT_ID     = static_cast<HIDP_STATUS>(0xC011000A);
inline constexpr HIDP_STATUS HIDP_STATUS_NOT_VALUE_ARRAY            = static_cast<HIDP_STATUS>(0xC011000B);
inline constexpr HIDP_STATUS HIDP_STATUS_IS_VALUE_ARRAY             = static_cast<HIDP_STATUS>(0xC011000C);
inline constexpr HIDP_STATUS HIDP_STATUS_DATA_INDEX_NOT_FOUND       = static_cast<HIDP_STATUS>(0xC011000D);
inline constexpr HIDP_STATUS HIDP_STATUS_DATA_INDEX_OUT_OF_RANGE    = static_cast<HIDP_STATUS>(0xC011000E);
inline constexpr HIDP_STATUS HIDP_STATUS_BUTTON_NOT_PRESSED         = static_cast<HIDP_STATUS>(0xC011000F);
inline constexpr HIDP_STATUS HIDP_STATUS_REPORT_DOES_NOT_EXIST      = static_cast<HIDP_STATUS>(0xC0110010);

// Standard HID Usage Pages (USB-IF HUT 1.4)
inline constexpr USAGE HID_USAGE_PAGE_UNDEFINED       = 0x0000;
inline constexpr USAGE HID_USAGE_PAGE_GENERIC         = 0x0001; // Generic Desktop Controls
inline constexpr USAGE HID_USAGE_PAGE_SIMULATION      = 0x0002;
inline constexpr USAGE HID_USAGE_PAGE_VR              = 0x0003;
inline constexpr USAGE HID_USAGE_PAGE_SPORT           = 0x0004;
inline constexpr USAGE HID_USAGE_PAGE_GAME            = 0x0005;
inline constexpr USAGE HID_USAGE_PAGE_KEYBOARD        = 0x0007; // Keyboard / Keypad
inline constexpr USAGE HID_USAGE_PAGE_LED             = 0x0008; // LEDs
inline constexpr USAGE HID_USAGE_PAGE_BUTTON          = 0x0009; // Button
inline constexpr USAGE HID_USAGE_PAGE_ORDINAL         = 0x000A;
inline constexpr USAGE HID_USAGE_PAGE_TELEPHONY       = 0x000B;
inline constexpr USAGE HID_USAGE_PAGE_CONSUMER        = 0x000C; // Consumer Control
inline constexpr USAGE HID_USAGE_PAGE_DIGITIZER       = 0x000D; // Digitizer / Touch
inline constexpr USAGE HID_USAGE_PAGE_VENDOR_DEFINED  = 0xFF00;

// Generic Desktop Usages (Page 0x01)
inline constexpr USAGE HID_USAGE_GENERIC_POINTER      = 0x0001;
inline constexpr USAGE HID_USAGE_GENERIC_MOUSE        = 0x0002;
inline constexpr USAGE HID_USAGE_GENERIC_JOYSTICK     = 0x0004;
inline constexpr USAGE HID_USAGE_GENERIC_GAMEPAD      = 0x0005;
inline constexpr USAGE HID_USAGE_GENERIC_KEYBOARD     = 0x0006;
inline constexpr USAGE HID_USAGE_GENERIC_KEYPAD       = 0x0007;
inline constexpr USAGE HID_USAGE_GENERIC_X            = 0x0030;
inline constexpr USAGE HID_USAGE_GENERIC_Y            = 0x0031;
inline constexpr USAGE HID_USAGE_GENERIC_Z            = 0x0032;
inline constexpr USAGE HID_USAGE_GENERIC_RX           = 0x0033;
inline constexpr USAGE HID_USAGE_GENERIC_RY           = 0x0034;
inline constexpr USAGE HID_USAGE_GENERIC_RZ           = 0x0035;
inline constexpr USAGE HID_USAGE_GENERIC_WHEEL        = 0x0038;
inline constexpr USAGE HID_USAGE_GENERIC_HATSWITCH    = 0x0039;

// HID Class IOCTLs (FILE_DEVICE_KEYBOARD = 0x000B, FILE_DEVICE_MOUSE = 0x000F)
inline constexpr uint32_t IOCTL_HID_GET_DRIVER_CONFIG           = 0x000B0190;
inline constexpr uint32_t IOCTL_HID_SET_DRIVER_CONFIG           = 0x000B0194;
inline constexpr uint32_t IOCTL_HID_GET_POLL_FREQUENCY_MSEC     = 0x000B0198;
inline constexpr uint32_t IOCTL_HID_SET_POLL_FREQUENCY_MSEC     = 0x000B019C;
inline constexpr uint32_t IOCTL_GET_NUM_DEVICE_INPUT_BUFFERS    = 0x000B01A0;
inline constexpr uint32_t IOCTL_SET_NUM_DEVICE_INPUT_BUFFERS    = 0x000B01A4;
inline constexpr uint32_t IOCTL_HID_GET_COLLECTION_INFORMATION  = 0x000B01A8;
inline constexpr uint32_t IOCTL_HID_GET_COLLECTION_DESCRIPTOR   = 0x000B01AC;
inline constexpr uint32_t IOCTL_HID_FLUSH_QUEUE                 = 0x000B01B0;
inline constexpr uint32_t IOCTL_HID_SET_FEATURE                 = 0x000B0191;
inline constexpr uint32_t IOCTL_HID_GET_FEATURE                 = 0x000B0192;
inline constexpr uint32_t IOCTL_HID_WRITE_REPORT                = 0x000B0193;
inline constexpr uint32_t IOCTL_HID_READ_REPORT                 = 0x000B0197;
inline constexpr uint32_t IOCTL_HID_GET_INPUT_REPORT            = 0x000B01A2;
inline constexpr uint32_t IOCTL_HID_SET_OUTPUT_REPORT           = 0x000B01A5;
inline constexpr uint32_t IOCTL_HID_GET_MANUFACTURER_STRING     = 0x000B01B8;
inline constexpr uint32_t IOCTL_HID_GET_PRODUCT_STRING          = 0x000B01BC;
inline constexpr uint32_t IOCTL_HID_GET_SERIALNUMBER_STRING     = 0x000B01C0;

// Keyboard & Mouse Class Driver IOCTLs
inline constexpr uint32_t IOCTL_KEYBOARD_QUERY_ATTRIBUTES       = 0x000B0000;
inline constexpr uint32_t IOCTL_KEYBOARD_SET_TYPEMATIC          = 0x000B0004;
inline constexpr uint32_t IOCTL_KEYBOARD_SET_INDICATORS         = 0x000B0008;
inline constexpr uint32_t IOCTL_KEYBOARD_QUERY_TYPEMATIC        = 0x000B0020;
inline constexpr uint32_t IOCTL_KEYBOARD_QUERY_INDICATORS       = 0x000B0040;
inline constexpr uint32_t IOCTL_MOUSE_QUERY_ATTRIBUTES          = 0x000F0000;

// USB HID Class-Specific Control Requests (bRequest)
inline constexpr uint8_t USB_HID_REQ_GET_REPORT                 = 0x01;
inline constexpr uint8_t USB_HID_REQ_GET_IDLE                   = 0x02;
inline constexpr uint8_t USB_HID_REQ_GET_PROTOCOL               = 0x03;
inline constexpr uint8_t USB_HID_REQ_SET_REPORT                 = 0x09;
inline constexpr uint8_t USB_HID_REQ_SET_IDLE                   = 0x0A;
inline constexpr uint8_t USB_HID_REQ_SET_PROTOCOL               = 0x0B;

// Standard Device Interface GUIDs
struct HID_GUID {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t  Data4[8];

    constexpr bool operator==(const HID_GUID& other) const noexcept {
        if (Data1 != other.Data1 || Data2 != other.Data2 || Data3 != other.Data3) return false;
        for (int i = 0; i < 8; ++i) {
            if (Data4[i] != other.Data4[i]) return false;
        }
        return true;
    }
};

// {4D1E55B2-F16F-11CF-88CB-001111000030}
inline constexpr HID_GUID GUID_DEVINTERFACE_HID = {
    0x4D1E55B2, 0xF16F, 0x11CF, {0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30}
};
// {884B96C3-56EF-11D1-BC8C-00A0C91405DD}
inline constexpr HID_GUID GUID_DEVINTERFACE_KEYBOARD = {
    0x884B96C3, 0x56EF, 0x11D1, {0xBC, 0x8C, 0x00, 0xA0, 0xC9, 0x14, 0x05, 0xDD}
};
// {378DE44C-56EF-11D1-BC8C-00A0C91405DD}
inline constexpr HID_GUID GUID_DEVINTERFACE_MOUSE = {
    0x378DE44C, 0x56EF, 0x11D1, {0xBC, 0x8C, 0x00, 0xA0, 0xC9, 0x14, 0x05, 0xDD}
};

// ============================================================================
// 2. WDK / Win32 HID Data Structures (HIDD_ATTRIBUTES, HIDP_CAPS, Button/Value Caps)
// ============================================================================

enum HIDP_REPORT_TYPE : int32_t {
    HidP_Input   = 0,
    HidP_Output  = 1,
    HidP_Feature = 2
};

struct HIDD_ATTRIBUTES {
    ULONG  Size{sizeof(HIDD_ATTRIBUTES)};
    USHORT VendorID{0};
    USHORT ProductID{0};
    USHORT VersionNumber{0};
};

struct HID_COLLECTION_INFORMATION {
    ULONG   DescriptorSize{0};
    BOOLEAN Polled{FALSE_B};
    UCHAR   Reserved1[1]{0};
    USHORT  VendorID{0};
    USHORT  ProductID{0};
    USHORT  VersionNumber{0};
};

struct HIDP_CAPS {
    USAGE  Usage{0};
    USAGE  UsagePage{0};
    USHORT InputReportByteLength{0};
    USHORT OutputReportByteLength{0};
    USHORT FeatureReportByteLength{0};
    USHORT Reserved[17]{0};
    USHORT NumberLinkCollectionNodes{0};
    USHORT NumberInputButtonCaps{0};
    USHORT NumberInputValueCaps{0};
    USHORT NumberInputDataIndices{0};
    USHORT NumberOutputButtonCaps{0};
    USHORT NumberOutputValueCaps{0};
    USHORT NumberOutputDataIndices{0};
    USHORT NumberFeatureButtonCaps{0};
    USHORT NumberFeatureValueCaps{0};
    USHORT NumberFeatureDataIndices{0};
};

struct HIDP_BUTTON_CAPS {
    USAGE   UsagePage{0};
    UCHAR   ReportID{0};
    BOOLEAN IsAlias{FALSE_B};
    USHORT  BitField{0};
    USHORT  LinkCollection{0};
    USAGE   LinkUsage{0};
    USAGE   LinkUsagePage{0};
    BOOLEAN IsRange{FALSE_B};
    BOOLEAN IsStringRange{FALSE_B};
    BOOLEAN IsDesignatorRange{FALSE_B};
    BOOLEAN IsAbsolute{TRUE_B};
    USHORT  ReportCount{1};
    USHORT  Reserved2{0};
    ULONG   Reserved[9]{0};
    union {
        struct {
            USAGE  UsageMin;
            USAGE  UsageMax;
            USHORT StringMin;
            USHORT StringMax;
            USHORT DesignatorMin;
            USHORT DesignatorMax;
            USHORT DataIndexMin;
            USHORT DataIndexMax;
        } Range;
        struct {
            USAGE  Usage;
            USAGE  Reserved1;
            USHORT StringIndex;
            USHORT Reserved2;
            USHORT DesignatorIndex;
            USHORT Reserved3;
            USHORT DataIndex;
            USHORT Reserved4;
        } NotRange;
    };
    // Internal bit offset within the report (excluding ReportID byte if 0)
    uint32_t InternalBitOffset{0};
    uint32_t InternalBitSize{1};
};

struct HIDP_VALUE_CAPS {
    USAGE   UsagePage{0};
    UCHAR   ReportID{0};
    BOOLEAN IsAlias{FALSE_B};
    USHORT  BitField{0};
    USHORT  LinkCollection{0};
    USAGE   LinkUsage{0};
    USAGE   LinkUsagePage{0};
    BOOLEAN IsRange{FALSE_B};
    BOOLEAN IsStringRange{FALSE_B};
    BOOLEAN IsDesignatorRange{FALSE_B};
    BOOLEAN IsAbsolute{TRUE_B};
    BOOLEAN HasNull{FALSE_B};
    UCHAR   Reserved{0};
    USHORT  BitSize{0};
    USHORT  ReportCount{1};
    USHORT  Reserved2[5]{0};
    ULONG   UnitsExp{0};
    ULONG   Units{0};
    LONG    LogicalMin{0};
    LONG    LogicalMax{0};
    LONG    PhysicalMin{0};
    LONG    PhysicalMax{0};
    union {
        struct {
            USAGE  UsageMin;
            USAGE  UsageMax;
            USHORT StringMin;
            USHORT StringMax;
            USHORT DesignatorMin;
            USHORT DesignatorMax;
            USHORT DataIndexMin;
            USHORT DataIndexMax;
        } Range;
        struct {
            USAGE  Usage;
            USAGE  Reserved1;
            USHORT StringIndex;
            USHORT Reserved2;
            USHORT DesignatorIndex;
            USHORT Reserved3;
            USHORT DataIndex;
            USHORT Reserved4;
        } NotRange;
    };
    uint32_t InternalBitOffset{0};
};

// Preparsed Data Structure (Magic 'PDHI' = 0x49484450)
inline constexpr uint32_t HIDP_PREPARSED_DATA_SIGNATURE = 0x49484450;

struct _HIDP_PREPARSED_DATA {
    uint32_t                      Signature{HIDP_PREPARSED_DATA_SIGNATURE};
    HIDP_CAPS                     Caps{};
    std::vector<HIDP_BUTTON_CAPS> InputButtonCaps;
    std::vector<HIDP_VALUE_CAPS>  InputValueCaps;
    std::vector<HIDP_BUTTON_CAPS> OutputButtonCaps;
    std::vector<HIDP_VALUE_CAPS>  OutputValueCaps;
    std::vector<HIDP_BUTTON_CAPS> FeatureButtonCaps;
    std::vector<HIDP_VALUE_CAPS>  FeatureValueCaps;
    bool                          UsesReportIds{false};
};

using PHIDP_PREPARSED_DATA = _HIDP_PREPARSED_DATA*;

// ============================================================================
// 3. Keyboard & Mouse Class Structures (kbdclass.sys / mouclass.sys)
// ============================================================================

// Keyboard Input Flags
inline constexpr USHORT KEY_MAKE              = 0x0000;
inline constexpr USHORT KEY_BREAK             = 0x0001;
inline constexpr USHORT KEY_E0                = 0x0002;
inline constexpr USHORT KEY_E1                = 0x0004;

// Keyboard LED Indicators
inline constexpr USHORT KEYBOARD_SCROLL_LOCK_ON = 0x0001;
inline constexpr USHORT KEYBOARD_NUM_LOCK_ON    = 0x0002;
inline constexpr USHORT KEYBOARD_CAPS_LOCK_ON   = 0x0004;

struct KEYBOARD_INPUT_DATA {
    USHORT UnitId{0};
    USHORT MakeCode{0};
    USHORT Flags{KEY_MAKE};
    USHORT Reserved{0};
    ULONG  ExtraInformation{0};
};

struct KEYBOARD_INDICATOR_PARAMETERS {
    USHORT UnitId{0};
    USHORT LedFlags{0};
};

struct KEYBOARD_TYPEMATIC_PARAMETERS {
    USHORT UnitId{0};
    USHORT Rate{30};
    USHORT Delay{250};
};

struct KEYBOARD_ATTRIBUTES {
    UCHAR                         KeyboardMode{1}; // Scancode Set 1 translated
    UCHAR                         NumberOfFunctionKeys{12};
    UCHAR                         NumberOfIndicators{3};
    USHORT                        NumberOfKeysTotal{104};
    ULONG                         InputDataQueueLength{128};
    KEYBOARD_TYPEMATIC_PARAMETERS KeyRepeatMinimum{0, 2, 250};
    KEYBOARD_TYPEMATIC_PARAMETERS KeyRepeatMaximum{0, 30, 1000};
};

// Mouse Input Flags
inline constexpr USHORT MOUSE_MOVE_RELATIVE      = 0x0000;
inline constexpr USHORT MOUSE_MOVE_ABSOLUTE      = 0x0001;
inline constexpr USHORT MOUSE_VIRTUAL_DESKTOP    = 0x0002;
inline constexpr USHORT MOUSE_ATTRIBUTES_CHANGED = 0x0004;

// Mouse Button Flags
inline constexpr USHORT MOUSE_LEFT_BUTTON_DOWN   = 0x0001;
inline constexpr USHORT MOUSE_LEFT_BUTTON_UP     = 0x0002;
inline constexpr USHORT MOUSE_RIGHT_BUTTON_DOWN  = 0x0004;
inline constexpr USHORT MOUSE_RIGHT_BUTTON_UP    = 0x0008;
inline constexpr USHORT MOUSE_MIDDLE_BUTTON_DOWN = 0x0010;
inline constexpr USHORT MOUSE_MIDDLE_BUTTON_UP   = 0x0020;
inline constexpr USHORT MOUSE_BUTTON_4_DOWN      = 0x0040;
inline constexpr USHORT MOUSE_BUTTON_4_UP        = 0x0080;
inline constexpr USHORT MOUSE_BUTTON_5_DOWN      = 0x0100;
inline constexpr USHORT MOUSE_BUTTON_5_UP        = 0x0200;
inline constexpr USHORT MOUSE_WHEEL              = 0x0400;
inline constexpr USHORT MOUSE_HWHEEL             = 0x0800;

struct MOUSE_INPUT_DATA {
    USHORT UnitId{0};
    USHORT Flags{MOUSE_MOVE_RELATIVE};
    union {
        ULONG Buttons{0};
        struct {
            USHORT ButtonFlags;
            USHORT ButtonData;
        };
    };
    ULONG  RawButtons{0};
    LONG   LastX{0};
    LONG   LastY{0};
    ULONG  ExtraInformation{0};
};

struct MOUSE_ATTRIBUTES {
    USHORT MouseIdentifier{0x0100}; // WHEELMOUSE_HID_HARDWARE
    USHORT NumberOfButtons{5};
    USHORT SampleRate{1000};        // 1000 Hz polling rate
    ULONG  InputDataQueueLength{256};
};

// ============================================================================
// 4. Canonical HID Report Descriptors (Keyboard, Gaming Mouse, Gamepad)
// ============================================================================

namespace Descriptors {

// 1. Standard 6KRO + Modifier + LED Boot/HID Keyboard Descriptor (63 bytes)
inline constexpr std::array<uint8_t, 63> kStandardKeyboardReportDesc = {
    0x05, 0x01,       // Usage Page (Generic Desktop)
    0x09, 0x06,       // Usage (Keyboard)
    0xA1, 0x01,       // Collection (Application)
    0x05, 0x07,       //   Usage Page (Keyboard/Keypad)
    0x19, 0xE0,       //   Usage Minimum (224 - Left Control)
    0x29, 0xE7,       //   Usage Maximum (231 - Right GUI)
    0x15, 0x00,       //   Logical Minimum (0)
    0x25, 0x01,       //   Logical Maximum (1)
    0x75, 0x01,       //   Report Size (1)
    0x95, 0x08,       //   Report Count (8)
    0x81, 0x02,       //   Input (Data, Variable, Absolute) - Modifier byte
    0x95, 0x01,       //   Report Count (1)
    0x75, 0x08,       //   Report Size (8)
    0x81, 0x01,       //   Input (Constant) - Reserved byte
    0x95, 0x05,       //   Report Count (5)
    0x75, 0x01,       //   Report Size (1)
    0x05, 0x08,       //   Usage Page (LEDs)
    0x19, 0x01,       //   Usage Minimum (Num Lock)
    0x29, 0x05,       //   Usage Maximum (Kana)
    0x91, 0x02,       //   Output (Data, Variable, Absolute) - 5 LED bits
    0x95, 0x01,       //   Report Count (1)
    0x75, 0x03,       //   Report Size (3)
    0x91, 0x01,       //   Output (Constant) - 3-bit LED padding
    0x95, 0x06,       //   Report Count (6)
    0x75, 0x08,       //   Report Size (8)
    0x15, 0x00,       //   Logical Minimum (0)
    0x25, 0x65,       //   Logical Maximum (101)
    0x05, 0x07,       //   Usage Page (Keyboard/Keypad)
    0x19, 0x00,       //   Usage Minimum (0)
    0x29, 0x65,       //   Usage Maximum (101)
    0x81, 0x00,       //   Input (Data, Array) - 6 Key Rollover array
    0xC0              // End Collection
};

// 2. High-Precision 5-Button 16-bit X/Y + 8-bit Wheel Gaming Mouse Descriptor
inline constexpr std::array<uint8_t, 64> kGamingMouseReportDesc = {
    0x05, 0x01,             // Usage Page (Generic Desktop)
    0x09, 0x02,             // Usage (Mouse)
    0xA1, 0x01,             // Collection (Application)
    0x09, 0x01,             //   Usage (Pointer)
    0xA1, 0x00,             //   Collection (Physical)
    0x05, 0x09,             //     Usage Page (Buttons)
    0x19, 0x01,             //     Usage Minimum (Button 1)
    0x29, 0x05,             //     Usage Maximum (Button 5)
    0x15, 0x00,             //     Logical Minimum (0)
    0x25, 0x01,             //     Logical Maximum (1)
    0x95, 0x05,             //     Report Count (5)
    0x75, 0x01,             //     Report Size (1)
    0x81, 0x02,             //     Input (Data, Variable, Absolute) - 5 Buttons
    0x95, 0x01,             //     Report Count (1)
    0x75, 0x03,             //     Report Size (3)
    0x81, 0x01,             //     Input (Constant) - 3-bit padding (Byte 0 complete)
    0x05, 0x01,             //     Usage Page (Generic Desktop)
    0x09, 0x30,             //     Usage (X)
    0x09, 0x31,             //     Usage (Y)
    0x16, 0x01, 0x80,       //     Logical Minimum (-32767)
    0x26, 0xFF, 0x7F,       //     Logical Maximum (32767)
    0x75, 0x10,             //     Report Size (16)
    0x95, 0x02,             //     Report Count (2)
    0x81, 0x06,             //     Input (Data, Variable, Relative) - 16-bit X, 16-bit Y (Bytes 1..4)
    0x09, 0x38,             //     Usage (Wheel)
    0x15, 0x81,             //     Logical Minimum (-127)
    0x25, 0x7F,             //     Logical Maximum (127)
    0x75, 0x08,             //     Report Size (8)
    0x95, 0x01,             //     Report Count (1)
    0x81, 0x06,             //     Input (Data, Variable, Relative) - 8-bit Wheel (Byte 5)
    0xC0,                   //   End Collection
    0xC0                    // End Collection
};

// 3. Dual-Axis Gamepad with 12 Buttons, Hat Switch, X/Y/Z/Rz Axes & Feature Profile
inline constexpr std::array<uint8_t, 72> kDualGamepadReportDesc = {
    0x05, 0x01,             // Usage Page (Generic Desktop)
    0x09, 0x05,             // Usage (Game Pad)
    0xA1, 0x01,             // Collection (Application)
    0x05, 0x09,             //   Usage Page (Button)
    0x19, 0x01,             //   Usage Minimum (Button 1)
    0x29, 0x0C,             //   Usage Maximum (Button 12)
    0x15, 0x00,             //   Logical Minimum (0)
    0x25, 0x01,             //   Logical Maximum (1)
    0x75, 0x01,             //   Report Size (1)
    0x95, 0x0C,             //   Report Count (12)
    0x81, 0x02,             //   Input (Data, Variable, Absolute) - 12 buttons
    0x05, 0x01,             //   Usage Page (Generic Desktop)
    0x09, 0x39,             //   Usage (Hat Switch)
    0x15, 0x00,             //   Logical Minimum (0)
    0x25, 0x07,             //   Logical Maximum (7)
    0x75, 0x04,             //   Report Size (4)
    0x95, 0x01,             //   Report Count (1)
    0x81, 0x42,             //   Input (Data, Variable, Absolute, Null State) - 4-bit Hat (Bytes 0..1 complete)
    0x09, 0x30,             //   Usage (X)
    0x09, 0x31,             //   Usage (Y)
    0x09, 0x32,             //   Usage (Z)
    0x09, 0x35,             //   Usage (Rz)
    0x15, 0x00,             //   Logical Minimum (0)
    0x26, 0xFF, 0x00,       //   Logical Maximum (255)
    0x75, 0x08,             //   Report Size (8)
    0x95, 0x04,             //   Report Count (4)
    0x81, 0x02,             //   Input (Data, Variable, Absolute) - 4 Analog Axes (Bytes 2..5)
    0x06, 0x00, 0xFF,       //   Usage Page (Vendor Defined 0xFF00)
    0x09, 0x20,             //   Usage (0x20 - Rumble / Calibration Feature)
    0x15, 0x00,             //   Logical Minimum (0)
    0x26, 0xFF, 0x00,       //   Logical Maximum (255)
    0x75, 0x08,             //   Report Size (8)
    0x95, 0x04,             //   Report Count (4)
    0xB1, 0x02,             //   Feature (Data, Variable, Absolute) - 4-byte Feature Report
    0xC0                    // End Collection
};

} // namespace Descriptors

// ============================================================================
// 5. HID Report Descriptor Bytecode Parser (hidparse.sys)
// ============================================================================

class HidReportDescriptorParser {
public:
    struct GlobalState {
        USAGE    UsagePage{0};
        LONG     LogicalMin{0};
        LONG     LogicalMax{0};
        LONG     PhysicalMin{0};
        LONG     PhysicalMax{0};
        ULONG    UnitExp{0};
        ULONG    Units{0};
        uint32_t ReportSize{0};
        uint8_t  ReportID{0};
        uint32_t ReportCount{0};
    };

    struct LocalState {
        std::vector<USAGE> Usages;
        USAGE              UsageMin{0};
        USAGE              UsageMax{0};
        bool               HasUsageRange{false};

        void reset() {
            Usages.clear();
            UsageMin = 0;
            UsageMax = 0;
            HasUsageRange = false;
        }
    };

    static bool ParseDescriptor(
        const uint8_t* descriptorBytes,
        size_t length,
        _HIDP_PREPARSED_DATA& outData
    ) {
        if (!descriptorBytes || length == 0) return false;

        outData = _HIDP_PREPARSED_DATA{};
        outData.Signature = HIDP_PREPARSED_DATA_SIGNATURE;

        GlobalState global{};
        std::vector<GlobalState> globalStack;
        LocalState local{};

        uint32_t inputBitOffset   = 0;
        uint32_t outputBitOffset  = 0;
        uint32_t featureBitOffset = 0;

        uint16_t inputDataIndex   = 0;
        uint16_t outputDataIndex  = 0;
        uint16_t featureDataIndex = 0;

        uint16_t collectionDepth  = 0;
        uint16_t totalCollections = 0;
        bool     topLevelUsageSet = false;

        size_t i = 0;
        while (i < length) {
            uint8_t prefix = descriptorBytes[i++];
            if (prefix == 0xFE) {
                // Long item
                if (i + 2 > length) break;
                uint8_t dataSize = descriptorBytes[i];
                i += 2 + dataSize;
                continue;
            }

            uint8_t sizeCode = prefix & 0x03;
            uint8_t itemSize = (sizeCode == 3) ? 4 : sizeCode;
            uint8_t itemType = (prefix >> 2) & 0x03;
            uint8_t itemTag  = (prefix >> 4) & 0x0F;

            if (i + itemSize > length) break;

            uint32_t uVal = 0;
            int32_t  sVal = 0;
            if (itemSize == 1) {
                uVal = descriptorBytes[i];
                sVal = static_cast<int8_t>(descriptorBytes[i]);
            } else if (itemSize == 2) {
                uVal = static_cast<uint16_t>(descriptorBytes[i] | (descriptorBytes[i + 1] << 8));
                sVal = static_cast<int16_t>(uVal);
            } else if (itemSize == 4) {
                uVal = static_cast<uint32_t>(descriptorBytes[i]) |
                       (static_cast<uint32_t>(descriptorBytes[i + 1]) << 8) |
                       (static_cast<uint32_t>(descriptorBytes[i + 2]) << 16) |
                       (static_cast<uint32_t>(descriptorBytes[i + 3]) << 24);
                sVal = static_cast<int32_t>(uVal);
            }
            i += itemSize;

            if (itemType == 1) {
                // Global Item
                switch (itemTag) {
                    case 0x0: global.UsagePage   = static_cast<USAGE>(uVal); break;
                    case 0x1: global.LogicalMin  = sVal; break;
                    case 0x2: global.LogicalMax  = sVal; break;
                    case 0x3: global.PhysicalMin = sVal; break;
                    case 0x4: global.PhysicalMax = sVal; break;
                    case 0x5: global.UnitExp     = uVal; break;
                    case 0x6: global.Units       = uVal; break;
                    case 0x7: global.ReportSize  = uVal; break;
                    case 0x8:
                        global.ReportID = static_cast<uint8_t>(uVal);
                        outData.UsesReportIds = true;
                        break;
                    case 0x9: global.ReportCount = uVal; break;
                    case 0xA: globalStack.push_back(global); break;
                    case 0xB:
                        if (!globalStack.empty()) {
                            global = globalStack.back();
                            globalStack.pop_back();
                        }
                        break;
                    default: break;
                }
            } else if (itemType == 2) {
                // Local Item
                switch (itemTag) {
                    case 0x0: local.Usages.push_back(static_cast<USAGE>(uVal)); break;
                    case 0x1:
                        local.UsageMin = static_cast<USAGE>(uVal);
                        local.HasUsageRange = true;
                        break;
                    case 0x2:
                        local.UsageMax = static_cast<USAGE>(uVal);
                        local.HasUsageRange = true;
                        break;
                    default: break;
                }
            } else if (itemType == 0) {
                // Main Item
                if (itemTag == 0xA) {
                    // Collection
                    collectionDepth++;
                    totalCollections++;
                    if (!topLevelUsageSet) {
                        outData.Caps.UsagePage = global.UsagePage;
                        if (!local.Usages.empty()) {
                            outData.Caps.Usage = local.Usages.front();
                        } else if (local.HasUsageRange) {
                            outData.Caps.Usage = local.UsageMin;
                        }
                        topLevelUsageSet = true;
                    }
                    local.reset();
                } else if (itemTag == 0xC) {
                    // End Collection
                    if (collectionDepth > 0) collectionDepth--;
                    local.reset();
                } else if (itemTag == 0x8 || itemTag == 0x9 || itemTag == 0xB) {
                    // Input (0x8), Output (0x9), or Feature (0xB)
                    uint32_t& currentBitOffset = (itemTag == 0x8) ? inputBitOffset :
                                                 (itemTag == 0x9) ? outputBitOffset : featureBitOffset;
                    uint16_t& currentDataIdx   = (itemTag == 0x8) ? inputDataIndex :
                                                 (itemTag == 0x9) ? outputDataIndex : featureDataIndex;
                    auto& btnCapsVec           = (itemTag == 0x8) ? outData.InputButtonCaps :
                                                 (itemTag == 0x9) ? outData.OutputButtonCaps : outData.FeatureButtonCaps;
                    auto& valCapsVec           = (itemTag == 0x8) ? outData.InputValueCaps :
                                                 (itemTag == 0x9) ? outData.OutputValueCaps : outData.FeatureValueCaps;

                    uint32_t totalBits = global.ReportSize * global.ReportCount;
                    bool isConstant = (uVal & 0x01) != 0;
                    bool isVariable = (uVal & 0x02) != 0;
                    bool isRelative = (uVal & 0x04) != 0;
                    bool hasNull    = (uVal & 0x40) != 0;

                    if (!isConstant && global.ReportCount > 0 && global.ReportSize > 0) {
                        // Classification: 1-bit fields or Variable=0 (Array of keycodes) are ButtonCaps
                        bool isButtonOrKeyArray = (global.ReportSize == 1) || (!isVariable && global.UsagePage == HID_USAGE_PAGE_KEYBOARD);

                        if (isButtonOrKeyArray) {
                            HIDP_BUTTON_CAPS bcap{};
                            bcap.UsagePage          = global.UsagePage;
                            bcap.ReportID           = global.ReportID;
                            bcap.BitField           = static_cast<USHORT>(uVal);
                            bcap.LinkCollection     = collectionDepth;
                            bcap.LinkUsage          = outData.Caps.Usage;
                            bcap.LinkUsagePage      = outData.Caps.UsagePage;
                            bcap.IsAbsolute         = isRelative ? FALSE_B : TRUE_B;
                            bcap.ReportCount        = static_cast<USHORT>(global.ReportCount);
                            bcap.InternalBitOffset  = currentBitOffset;
                            bcap.InternalBitSize    = global.ReportSize;

                            if (local.HasUsageRange) {
                                bcap.IsRange            = TRUE_B;
                                bcap.Range.UsageMin     = local.UsageMin;
                                bcap.Range.UsageMax     = local.UsageMax;
                                bcap.Range.DataIndexMin = currentDataIdx;
                                uint16_t span = static_cast<uint16_t>(
                                    (local.UsageMax >= local.UsageMin) ? (local.UsageMax - local.UsageMin + 1) : global.ReportCount);
                                bcap.Range.DataIndexMax = static_cast<uint16_t>(currentDataIdx + span - 1);
                                currentDataIdx = static_cast<uint16_t>(currentDataIdx + span);
                            } else {
                                bcap.IsRange            = FALSE_B;
                                bcap.NotRange.Usage     = local.Usages.empty() ? 0 : local.Usages.front();
                                bcap.NotRange.DataIndex = currentDataIdx++;
                            }
                            btnCapsVec.push_back(bcap);
                        } else {
                            // Value Caps: emit one entry per usage if multiple discrete usages are listed
                            if (!local.Usages.empty() && local.Usages.size() == global.ReportCount) {
                                for (size_t uIdx = 0; uIdx < local.Usages.size(); ++uIdx) {
                                    HIDP_VALUE_CAPS vcap{};
                                    vcap.UsagePage         = global.UsagePage;
                                    vcap.ReportID          = global.ReportID;
                                    vcap.BitField          = static_cast<USHORT>(uVal);
                                    vcap.LinkCollection    = collectionDepth;
                                    vcap.LinkUsage         = outData.Caps.Usage;
                                    vcap.LinkUsagePage     = outData.Caps.UsagePage;
                                    vcap.IsRange           = FALSE_B;
                                    vcap.IsAbsolute        = isRelative ? FALSE_B : TRUE_B;
                                    vcap.HasNull           = hasNull ? TRUE_B : FALSE_B;
                                    vcap.BitSize           = static_cast<USHORT>(global.ReportSize);
                                    vcap.ReportCount       = 1;
                                    vcap.UnitsExp          = global.UnitExp;
                                    vcap.Units             = global.Units;
                                    vcap.LogicalMin        = global.LogicalMin;
                                    vcap.LogicalMax        = global.LogicalMax;
                                    vcap.PhysicalMin       = global.PhysicalMin;
                                    vcap.PhysicalMax       = global.PhysicalMax;
                                    vcap.NotRange.Usage    = local.Usages[uIdx];
                                    vcap.NotRange.DataIndex= currentDataIdx++;
                                    vcap.InternalBitOffset = currentBitOffset + static_cast<uint32_t>(uIdx) * global.ReportSize;
                                    valCapsVec.push_back(vcap);
                                }
                            } else {
                                HIDP_VALUE_CAPS vcap{};
                                vcap.UsagePage         = global.UsagePage;
                                vcap.ReportID          = global.ReportID;
                                vcap.BitField          = static_cast<USHORT>(uVal);
                                vcap.LinkCollection    = collectionDepth;
                                vcap.LinkUsage         = outData.Caps.Usage;
                                vcap.LinkUsagePage     = outData.Caps.UsagePage;
                                vcap.IsAbsolute        = isRelative ? FALSE_B : TRUE_B;
                                vcap.HasNull           = hasNull ? TRUE_B : FALSE_B;
                                vcap.BitSize           = static_cast<USHORT>(global.ReportSize);
                                vcap.ReportCount       = static_cast<USHORT>(global.ReportCount);
                                vcap.UnitsExp          = global.UnitExp;
                                vcap.Units             = global.Units;
                                vcap.LogicalMin        = global.LogicalMin;
                                vcap.LogicalMax        = global.LogicalMax;
                                vcap.PhysicalMin       = global.PhysicalMin;
                                vcap.PhysicalMax       = global.PhysicalMax;
                                vcap.InternalBitOffset = currentBitOffset;

                                if (local.HasUsageRange) {
                                    vcap.IsRange            = TRUE_B;
                                    vcap.Range.UsageMin     = local.UsageMin;
                                    vcap.Range.UsageMax     = local.UsageMax;
                                    vcap.Range.DataIndexMin = currentDataIdx;
                                    vcap.Range.DataIndexMax = static_cast<uint16_t>(currentDataIdx + global.ReportCount - 1);
                                    currentDataIdx = static_cast<uint16_t>(currentDataIdx + global.ReportCount);
                                } else {
                                    vcap.IsRange            = FALSE_B;
                                    vcap.NotRange.Usage     = local.Usages.empty() ? 0 : local.Usages.front();
                                    vcap.NotRange.DataIndex = currentDataIdx++;
                                }
                                valCapsVec.push_back(vcap);
                            }
                        }
                    }

                    currentBitOffset += totalBits;
                    local.reset();
                }
            }
        }

        // Compute byte lengths (Windows HID reports always include 1-byte ReportID prefix in length calculations)
        auto bitsToReportBytes = [](uint32_t bits) -> USHORT {
            if (bits == 0) return 0;
            return static_cast<USHORT>(((bits + 7) / 8) + 1); // +1 for Report ID byte
        };

        outData.Caps.InputReportByteLength      = bitsToReportBytes(inputBitOffset);
        outData.Caps.OutputReportByteLength     = bitsToReportBytes(outputBitOffset);
        outData.Caps.FeatureReportByteLength    = bitsToReportBytes(featureBitOffset);
        outData.Caps.NumberLinkCollectionNodes  = std::max<USHORT>(1, totalCollections);

        outData.Caps.NumberInputButtonCaps      = static_cast<USHORT>(outData.InputButtonCaps.size());
        outData.Caps.NumberInputValueCaps       = static_cast<USHORT>(outData.InputValueCaps.size());
        outData.Caps.NumberInputDataIndices     = inputDataIndex;

        outData.Caps.NumberOutputButtonCaps     = static_cast<USHORT>(outData.OutputButtonCaps.size());
        outData.Caps.NumberOutputValueCaps      = static_cast<USHORT>(outData.OutputValueCaps.size());
        outData.Caps.NumberOutputDataIndices    = outputDataIndex;

        outData.Caps.NumberFeatureButtonCaps    = static_cast<USHORT>(outData.FeatureButtonCaps.size());
        outData.Caps.NumberFeatureValueCaps     = static_cast<USHORT>(outData.FeatureValueCaps.size());
        outData.Caps.NumberFeatureDataIndices   = featureDataIndex;

        return true;
    }

    // Bitfield read helper (starts after ReportID byte at offset 1)
    static uint32_t ReadBits(const uint8_t* payload, size_t payloadBytes, uint32_t bitOffset, uint32_t bitSize) {
        if (!payload || bitSize == 0 || bitSize > 32) return 0;
        uint32_t result = 0;
        for (uint32_t b = 0; b < bitSize; ++b) {
            uint32_t totalBit = bitOffset + b;
            size_t byteIdx = totalBit / 8;
            uint32_t bitInByte = totalBit % 8;
            if (byteIdx < payloadBytes) {
                if ((payload[byteIdx] >> bitInByte) & 0x01) {
                    result |= (1u << b);
                }
            }
        }
        return result;
    }

    // Bitfield write helper
    static void WriteBits(uint8_t* payload, size_t payloadBytes, uint32_t bitOffset, uint32_t bitSize, uint32_t value) {
        if (!payload || bitSize == 0 || bitSize > 32) return;
        for (uint32_t b = 0; b < bitSize; ++b) {
            uint32_t totalBit = bitOffset + b;
            size_t byteIdx = totalBit / 8;
            uint32_t bitInByte = totalBit % 8;
            if (byteIdx < payloadBytes) {
                if ((value >> b) & 0x01) {
                    payload[byteIdx] |= static_cast<uint8_t>(1u << bitInByte);
                } else {
                    payload[byteIdx] &= static_cast<uint8_t>(~(1u << bitInByte));
                }
            }
        }
    }
};

// ============================================================================
// 6. PS/2 i8042 Controller Port Driver (i8042prt.sys)
// ============================================================================

inline constexpr uint16_t I8042_DATA_PORT    = 0x0060;
inline constexpr uint16_t I8042_STATUS_PORT  = 0x0064;
inline constexpr uint16_t I8042_COMMAND_PORT = 0x0064;

// i8042 Status Register Bits
inline constexpr uint8_t I8042_STS_OBF       = 0x01; // Output Buffer Full
inline constexpr uint8_t I8042_STS_IBF       = 0x02; // Input Buffer Full
inline constexpr uint8_t I8042_STS_SYS       = 0x04; // System Flag (POST passed)
inline constexpr uint8_t I8042_STS_A2        = 0x08; // Command/Data (1=Command)
inline constexpr uint8_t I8042_STS_AUXOBF    = 0x20; // Mouse Output Buffer Full

// i8042 Controller Commands
inline constexpr uint8_t I8042_CMD_READ_CFG  = 0x20;
inline constexpr uint8_t I8042_CMD_WRITE_CFG = 0x60;
inline constexpr uint8_t I8042_CMD_DIS_MOUSE = 0xA7;
inline constexpr uint8_t I8042_CMD_EN_MOUSE  = 0xA8;
inline constexpr uint8_t I8042_CMD_SELF_TEST = 0xAA;
inline constexpr uint8_t I8042_CMD_KBD_TEST  = 0xAB;
inline constexpr uint8_t I8042_CMD_EN_KBD    = 0xAE;
inline constexpr uint8_t I8042_CMD_WRITE_AUX = 0xD4;

// ============================================================================
// 7. Top-Level Collection (TLC) Device Representation & Subsystem Telemetry
// ============================================================================

struct HID_TOP_LEVEL_COLLECTION {
    win32::HANDLE            DeviceHandle{nullptr};
    std::string              DevicePath;
    std::wstring             HardwareId;
    std::wstring             ManufacturerString;
    std::wstring             ProductString;
    std::wstring             SerialNumberString;
    HIDD_ATTRIBUTES          Attributes{};
    HID_GUID                 InterfaceGuid{GUID_DEVINTERFACE_HID};
    uint8_t                  XhciSlotId{0};
    ULONG                    NumInputBuffers{32};
    ULONG                    PollFrequencyMsec{1}; // 1ms = 1000Hz
    std::vector<uint8_t>     RawReportDescriptor;
    _HIDP_PREPARSED_DATA     PreparsedData{};
    std::deque<std::vector<uint8_t>> PendingInputReports;
    std::vector<uint8_t>     LastOutputReport;
    std::vector<uint8_t>     LastFeatureReport;
};

struct HID_SUBSYSTEM_TELEMETRY {
    uint64_t totalDescriptorsParsed{0};
    uint64_t totalHidInputReportsRead{0};
    uint64_t totalHidOutputReportsWritten{0};
    uint64_t totalHidFeatureTransfers{0};
    uint64_t totalPingPongUrbsDispatched{0};
    uint64_t totalKeyboardPacketsQueued{0};
    uint64_t totalMousePacketsQueued{0};
    uint64_t totalI8042InterruptsServiced{0};
    uint64_t totalWin32kMessagesInjected{0};
    uint32_t activeTopLevelCollections{0};
    bool     i8042ControllerHealthy{true};
};

// ============================================================================
// 8. Sovereign HID & Kernel Input Stack Engine (TitanHidInputSubsystem)
// ============================================================================

class TitanHidInputSubsystem {
public:
    static TitanHidInputSubsystem& Instance() {
        static TitanHidInputSubsystem s_instance;
        return s_instance;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_initialized) return;
        setupInternalState();
        m_initialized = true;
    }

    void resetForTesting() {
        std::lock_guard<std::mutex> lock(m_mutex);
        setupInternalState();
        m_initialized = true;
    }

    // ------------------------------------------------------------------------
    // Top-Level Collection Lookup
    // ------------------------------------------------------------------------

    HID_TOP_LEVEL_COLLECTION* findCollection(win32::HANDLE handle) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_collections.find(reinterpret_cast<uintptr_t>(handle));
        if (it == m_collections.end()) return nullptr;
        return &it->second;
    }

    std::vector<HID_TOP_LEVEL_COLLECTION> getAllCollections() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<HID_TOP_LEVEL_COLLECTION> out;
        for (const auto& [k, v] : m_collections) {
            out.push_back(v);
        }
        return out;
    }

    // ------------------------------------------------------------------------
    // USB HID Miniport (hidusb.sys) <-> xHCI URB Pipeline
    // ------------------------------------------------------------------------

    NtStatus submitPingPongInterruptReadUrb(
        win32::HANDLE collectionHandle,
        const uint8_t* simulatedDevicePayload,
        size_t payloadLen
    ) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_collections.find(reinterpret_cast<uintptr_t>(collectionHandle));
        if (it == m_collections.end()) return NtStatus::InvalidHandle;

        auto& col = it->second;

        // Allocate a real URB via USBD_UrbAllocate and dispatch through xhci.hpp
        xhci::URB* urb = xhci::USBD_UrbAllocate();
        if (!urb) return NtStatus::InsufficientResources;

        std::vector<uint8_t> transferBuf(payloadLen, 0);
        if (simulatedDevicePayload && payloadLen > 0) {
            std::memcpy(transferBuf.data(), simulatedDevicePayload, payloadLen);
        }

        urb->UrbHeader.Function = xhci::URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER;
        urb->UrbBulkOrInterruptTransfer.TransferBuffer       = transferBuf.data();
        urb->UrbBulkOrInterruptTransfer.TransferBufferLength = static_cast<uint32_t>(payloadLen);
        urb->UrbBulkOrInterruptTransfer.TransferFlags        = xhci::USBD_TRANSFER_DIRECTION_IN | xhci::USBD_SHORT_TRANSFER_OK;

        // Submit to xHCI slot (default to Slot 2 Logitech Mouse if slot is 0)
        uint8_t slotId = (col.XhciSlotId != 0) ? col.XhciSlotId : 2;
        NtStatus st = xhci::XhciSubmitUrb(urb, slotId);

        // Preserve caller's injected HID report frame if provided
        if (simulatedDevicePayload && payloadLen > 0) {
            std::vector<uint8_t> reportFrame(simulatedDevicePayload, simulatedDevicePayload + payloadLen);
            if (col.PendingInputReports.size() >= col.NumInputBuffers) {
                col.PendingInputReports.pop_front();
            }
            col.PendingInputReports.push_back(std::move(reportFrame));
        }

        xhci::USBD_UrbFree(urb);
        m_telemetry.totalPingPongUrbsDispatched++;
        return st;
    }

    NtStatus submitHidClassControlUrb(
        win32::HANDLE collectionHandle,
        uint8_t bRequest,
        uint8_t reportType,
        uint8_t reportId,
        void* buffer,
        uint16_t bufferLength
    ) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_collections.find(reinterpret_cast<uintptr_t>(collectionHandle));
        if (it == m_collections.end()) return NtStatus::InvalidHandle;

        auto& col = it->second;
        xhci::URB* urb = xhci::USBD_UrbAllocate();
        if (!urb) return NtStatus::InsufficientResources;

        urb->UrbHeader.Function = xhci::URB_FUNCTION_CONTROL_TRANSFER_EX;
        auto* ct = &urb->UrbControlTransferEx;
        ct->TransferBuffer       = buffer;
        ct->TransferBufferLength = bufferLength;

        bool isRead = (bRequest == USB_HID_REQ_GET_REPORT ||
                       bRequest == USB_HID_REQ_GET_IDLE ||
                       bRequest == USB_HID_REQ_GET_PROTOCOL);

        ct->TransferFlags = isRead ? xhci::USBD_TRANSFER_DIRECTION_IN : xhci::USBD_TRANSFER_DIRECTION_OUT;

        usb::USB_DEFAULT_PIPE_SETUP_PACKET setup{};
        setup.bmRequestType = usb::RequestType::Class | usb::RequestType::RecipientInterface |
                              (isRead ? usb::RequestType::DirectionIn : usb::RequestType::DirectionOut);
        setup.bRequest      = bRequest;
        setup.wValue        = static_cast<uint16_t>((static_cast<uint16_t>(reportType) << 8) | reportId);
        setup.wIndex        = 0;
        setup.wLength       = bufferLength;
        std::memcpy(ct->SetupPacket, &setup, sizeof(setup));

        uint8_t slotId = (col.XhciSlotId != 0) ? col.XhciSlotId : 2;
        NtStatus st = xhci::XhciSubmitUrb(urb, slotId);

        if (bRequest == USB_HID_REQ_SET_REPORT && buffer && bufferLength > 0) {
            const uint8_t* raw = static_cast<const uint8_t*>(buffer);
            if (reportType == 0x02) { // Output Report
                col.LastOutputReport.assign(raw, raw + bufferLength);
                m_telemetry.totalHidOutputReportsWritten++;
            } else if (reportType == 0x03) { // Feature Report
                col.LastFeatureReport.assign(raw, raw + bufferLength);
                m_telemetry.totalHidFeatureTransfers++;
            }
        } else if (bRequest == USB_HID_REQ_GET_REPORT && buffer && bufferLength > 0) {
            uint8_t* out = static_cast<uint8_t*>(buffer);
            if (reportType == 0x03 && !col.LastFeatureReport.empty()) {
                size_t toCopy = std::min<size_t>(bufferLength, col.LastFeatureReport.size());
                std::memcpy(out, col.LastFeatureReport.data(), toCopy);
                m_telemetry.totalHidFeatureTransfers++;
            } else if (reportType == 0x01 && !col.PendingInputReports.empty()) {
                const auto& front = col.PendingInputReports.front();
                size_t toCopy = std::min<size_t>(bufferLength, front.size());
                std::memcpy(out, front.data(), toCopy);
                col.PendingInputReports.pop_front();
                m_telemetry.totalHidInputReportsRead++;
            }
        }

        xhci::USBD_UrbFree(urb);
        m_telemetry.totalPingPongUrbsDispatched++;
        return st;
    }

    // ------------------------------------------------------------------------
    // Keyboard Class Driver (kbdclass.sys) Operations
    // ------------------------------------------------------------------------

    void keyboardClassServiceCallback(const KEYBOARD_INPUT_DATA* start, const KEYBOARD_INPUT_DATA* end, ULONG* consumed) {
        std::lock_guard<std::mutex> lock(m_mutex);
        ULONG count = 0;
        for (const KEYBOARD_INPUT_DATA* p = start; p < end; ++p) {
            if (m_keyboardQueue.size() >= m_keyboardAttributes.InputDataQueueLength) {
                m_keyboardQueue.pop_front();
            }
            m_keyboardQueue.push_back(*p);
            count++;
            m_telemetry.totalKeyboardPacketsQueued++;
        }
        if (consumed) *consumed = count;
    }

    bool popKeyboardInputData(KEYBOARD_INPUT_DATA& outData) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_keyboardQueue.empty()) return false;
        outData = m_keyboardQueue.front();
        m_keyboardQueue.pop_front();
        return true;
    }

    size_t getKeyboardQueueDepth() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_keyboardQueue.size();
    }

    NtStatus dispatchKeyboardIoctl(uint32_t ioctlCode, const void* inBuf, size_t inLen, void* outBuf, size_t outLen) {
        std::lock_guard<std::mutex> lock(m_mutex);
        switch (ioctlCode) {
            case IOCTL_KEYBOARD_QUERY_ATTRIBUTES:
                if (!outBuf || outLen < sizeof(KEYBOARD_ATTRIBUTES)) return NtStatus::BufferTooSmall;
                *static_cast<KEYBOARD_ATTRIBUTES*>(outBuf) = m_keyboardAttributes;
                return NtStatus::Success;

            case IOCTL_KEYBOARD_QUERY_INDICATORS:
                if (!outBuf || outLen < sizeof(KEYBOARD_INDICATOR_PARAMETERS)) return NtStatus::BufferTooSmall;
                *static_cast<KEYBOARD_INDICATOR_PARAMETERS*>(outBuf) = m_keyboardIndicators;
                return NtStatus::Success;

            case IOCTL_KEYBOARD_SET_INDICATORS:
                if (!inBuf || inLen < sizeof(KEYBOARD_INDICATOR_PARAMETERS)) return NtStatus::InvalidParameter;
                m_keyboardIndicators = *static_cast<const KEYBOARD_INDICATOR_PARAMETERS*>(inBuf);
                return NtStatus::Success;

            case IOCTL_KEYBOARD_QUERY_TYPEMATIC:
                if (!outBuf || outLen < sizeof(KEYBOARD_TYPEMATIC_PARAMETERS)) return NtStatus::BufferTooSmall;
                *static_cast<KEYBOARD_TYPEMATIC_PARAMETERS*>(outBuf) = m_keyboardTypematic;
                return NtStatus::Success;

            case IOCTL_KEYBOARD_SET_TYPEMATIC:
                if (!inBuf || inLen < sizeof(KEYBOARD_TYPEMATIC_PARAMETERS)) return NtStatus::InvalidParameter;
                m_keyboardTypematic = *static_cast<const KEYBOARD_TYPEMATIC_PARAMETERS*>(inBuf);
                return NtStatus::Success;

            default:
                return NtStatus::InvalidDeviceRequest;
        }
    }

    // ------------------------------------------------------------------------
    // Mouse Class Driver (mouclass.sys) Operations
    // ------------------------------------------------------------------------

    void mouseClassServiceCallback(const MOUSE_INPUT_DATA* start, const MOUSE_INPUT_DATA* end, ULONG* consumed) {
        std::lock_guard<std::mutex> lock(m_mutex);
        ULONG count = 0;
        for (const MOUSE_INPUT_DATA* p = start; p < end; ++p) {
            if (m_mouseQueue.size() >= m_mouseAttributes.InputDataQueueLength) {
                m_mouseQueue.pop_front();
            }
            m_mouseQueue.push_back(*p);
            count++;
            m_telemetry.totalMousePacketsQueued++;
        }
        if (consumed) *consumed = count;
    }

    bool popMouseInputData(MOUSE_INPUT_DATA& outData) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_mouseQueue.empty()) return false;
        outData = m_mouseQueue.front();
        m_mouseQueue.pop_front();
        return true;
    }

    size_t getMouseQueueDepth() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_mouseQueue.size();
    }

    NtStatus dispatchMouseIoctl(uint32_t ioctlCode, void* outBuf, size_t outLen) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (ioctlCode == IOCTL_MOUSE_QUERY_ATTRIBUTES) {
            if (!outBuf || outLen < sizeof(MOUSE_ATTRIBUTES)) return NtStatus::BufferTooSmall;
            *static_cast<MOUSE_ATTRIBUTES*>(outBuf) = m_mouseAttributes;
            return NtStatus::Success;
        }
        return NtStatus::InvalidDeviceRequest;
    }

    // ------------------------------------------------------------------------
    // PS/2 i8042 Controller Port Driver (i8042prt.sys) Port 0x60/0x64 Engine
    // ------------------------------------------------------------------------

    uint8_t i8042ReadPort(uint16_t port) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (port == I8042_STATUS_PORT) {
            return m_i8042StatusReg;
        }
        if (port == I8042_DATA_PORT) {
            m_i8042StatusReg &= static_cast<uint8_t>(~(I8042_STS_OBF | I8042_STS_AUXOBF));
            return m_i8042OutputBuffer;
        }
        return 0xFF;
    }

    void i8042WritePort(uint16_t port, uint8_t value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (port == I8042_COMMAND_PORT) {
            m_i8042LastCommand = value;
            switch (value) {
                case I8042_CMD_SELF_TEST:
                    m_i8042OutputBuffer = 0x55; // Self-test passed
                    m_i8042StatusReg |= (I8042_STS_OBF | I8042_STS_SYS);
                    break;
                case I8042_CMD_KBD_TEST:
                    m_i8042OutputBuffer = 0x00; // Interface test passed
                    m_i8042StatusReg |= I8042_STS_OBF;
                    break;
                case I8042_CMD_READ_CFG:
                    m_i8042OutputBuffer = m_i8042ConfigByte;
                    m_i8042StatusReg |= I8042_STS_OBF;
                    break;
                case I8042_CMD_EN_MOUSE:
                    m_i8042ConfigByte |= 0x02; // Enable IRQ12
                    break;
                case I8042_CMD_DIS_MOUSE:
                    m_i8042ConfigByte &= static_cast<uint8_t>(~0x02);
                    break;
                case I8042_CMD_EN_KBD:
                    m_i8042ConfigByte |= 0x01; // Enable IRQ1
                    break;
                default:
                    break;
            }
        } else if (port == I8042_DATA_PORT) {
            if (m_i8042LastCommand == I8042_CMD_WRITE_CFG) {
                m_i8042ConfigByte = value;
                m_i8042LastCommand = 0;
            } else if (m_i8042LastCommand == I8042_CMD_WRITE_AUX) {
                // Command sent to PS/2 mouse -> ACK (0xFA)
                m_i8042OutputBuffer = 0xFA;
                m_i8042StatusReg |= (I8042_STS_OBF | I8042_STS_AUXOBF);
                m_i8042LastCommand = 0;
            } else {
                // Command sent to PS/2 keyboard -> ACK (0xFA)
                m_i8042OutputBuffer = 0xFA;
                m_i8042StatusReg |= I8042_STS_OBF;
            }
        }
    }

    // Simulate IRQ1 Keyboard Interrupt (Scancode Set 1 byte)
    void i8042InjectKeyboardIrq1(uint8_t scancodeByte) {
        KEYBOARD_INPUT_DATA pkt{};
        bool emitPacket = false;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_telemetry.totalI8042InterruptsServiced++;
            m_i8042OutputBuffer = scancodeByte;
            m_i8042StatusReg |= I8042_STS_OBF;

            if (scancodeByte == 0xE0) {
                m_i8042ExtendedE0 = true;
                return;
            }
            if (scancodeByte == 0xE1) {
                m_i8042ExtendedE1 = true;
                return;
            }

            pkt.UnitId   = 0;
            pkt.MakeCode = static_cast<USHORT>(scancodeByte & 0x7F);
            pkt.Flags    = (scancodeByte & 0x80) ? KEY_BREAK : KEY_MAKE;
            if (m_i8042ExtendedE0) {
                pkt.Flags |= KEY_E0;
                m_i8042ExtendedE0 = false;
            }
            if (m_i8042ExtendedE1) {
                pkt.Flags |= KEY_E1;
                m_i8042ExtendedE1 = false;
            }
            emitPacket = true;
        }

        if (emitPacket) {
            ULONG consumed = 0;
            keyboardClassServiceCallback(&pkt, &pkt + 1, &consumed);
        }
    }

    // Simulate IRQ12 PS/2 Mouse 4-Byte IntelliMouse Packet
    void i8042InjectMouseIrq12Packet(uint8_t statusByte, int8_t deltaX, int8_t deltaY, int8_t wheelZ = 0) {
        MOUSE_INPUT_DATA pkt{};
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_telemetry.totalI8042InterruptsServiced++;
            pkt.UnitId = 0;
            pkt.Flags  = MOUSE_MOVE_RELATIVE;
            pkt.LastX  = deltaX;
            pkt.LastY  = -deltaY; // PS/2 Y axis is inverted relative to screen coordinates

            uint16_t btnFlags = 0;
            bool leftNow   = (statusByte & 0x01) != 0;
            bool rightNow  = (statusByte & 0x02) != 0;
            bool middleNow = (statusByte & 0x04) != 0;

            if (leftNow && !m_ps2LeftDown)     btnFlags |= MOUSE_LEFT_BUTTON_DOWN;
            if (!leftNow && m_ps2LeftDown)     btnFlags |= MOUSE_LEFT_BUTTON_UP;
            if (rightNow && !m_ps2RightDown)   btnFlags |= MOUSE_RIGHT_BUTTON_DOWN;
            if (!rightNow && m_ps2RightDown)   btnFlags |= MOUSE_RIGHT_BUTTON_UP;
            if (middleNow && !m_ps2MiddleDown) btnFlags |= MOUSE_MIDDLE_BUTTON_DOWN;
            if (!middleNow && m_ps2MiddleDown) btnFlags |= MOUSE_MIDDLE_BUTTON_UP;

            m_ps2LeftDown   = leftNow;
            m_ps2RightDown  = rightNow;
            m_ps2MiddleDown = middleNow;

            if (wheelZ != 0) {
                btnFlags |= MOUSE_WHEEL;
                pkt.ButtonData = static_cast<USHORT>(static_cast<int16_t>(wheelZ) * 120);
            }
            pkt.ButtonFlags = btnFlags;
            pkt.RawButtons  = statusByte & 0x07;
        }

        ULONG consumed = 0;
        mouseClassServiceCallback(&pkt, &pkt + 1, &consumed);
    }

    // ------------------------------------------------------------------------
    // Kernel Raw Input Thread (RIT) -> Win32k Window Message Bridge
    // ------------------------------------------------------------------------

    uint32_t pumpRawInputToWin32k(win32k::HWND targetHwnd) {
        uint32_t injectedCount = 0;
        auto& w32k = win32k::TitanWin32kSubsystem::Instance();

        // 1. Drain Keyboard Class Ring Buffer
        KEYBOARD_INPUT_DATA kbdPkt{};
        while (popKeyboardInputData(kbdPkt)) {
            uint32_t msg = (kbdPkt.Flags & KEY_BREAK) ? win32k::WM_KEYUP : win32k::WM_KEYDOWN;
            uint64_t vk  = translateScancodeToVirtualKey(kbdPkt.MakeCode);
            int64_t lParam = static_cast<int64_t>((static_cast<uint64_t>(kbdPkt.MakeCode) << 16) | 1ULL);
            if (kbdPkt.Flags & KEY_BREAK) {
                lParam |= static_cast<int64_t>(3ULL << 30);
            }
            w32k.postMessage(targetHwnd, msg, vk, lParam);
            injectedCount++;
        }

        // 2. Drain Mouse Class Ring Buffer
        MOUSE_INPUT_DATA mouPkt{};
        while (popMouseInputData(mouPkt)) {
            if (mouPkt.LastX != 0 || mouPkt.LastY != 0) {
                int64_t lParam = static_cast<int64_t>(
                    (static_cast<uint64_t>(static_cast<uint16_t>(mouPkt.LastY)) << 16) |
                    static_cast<uint64_t>(static_cast<uint16_t>(mouPkt.LastX)));
                w32k.postMessage(targetHwnd, win32k::WM_MOUSEMOVE, mouPkt.RawButtons, lParam);
                injectedCount++;
            }
            if (mouPkt.ButtonFlags & MOUSE_LEFT_BUTTON_DOWN) {
                w32k.postMessage(targetHwnd, win32k::WM_LBUTTONDOWN, 0x0001, 0);
                injectedCount++;
            }
            if (mouPkt.ButtonFlags & MOUSE_LEFT_BUTTON_UP) {
                w32k.postMessage(targetHwnd, win32k::WM_LBUTTONUP, 0x0000, 0);
                injectedCount++;
            }
            if (mouPkt.ButtonFlags & MOUSE_WHEEL) {
                uint64_t wParam = static_cast<uint64_t>(mouPkt.ButtonData) << 16;
                w32k.postMessage(targetHwnd, 0x020A /* WM_MOUSEWHEEL */, wParam, 0);
                injectedCount++;
            }
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_telemetry.totalWin32kMessagesInjected += injectedCount;
        }
        return injectedCount;
    }

    static uint32_t translateScancodeToVirtualKey(uint16_t makeCode) noexcept {
        switch (makeCode) {
            case 0x01: return 0x1B; // VK_ESCAPE
            case 0x0E: return 0x08; // VK_BACK
            case 0x0F: return 0x09; // VK_TAB
            case 0x1C: return 0x0D; // VK_RETURN
            case 0x1D: return 0x11; // VK_CONTROL
            case 0x1E: return 'A';
            case 0x1F: return 'S';
            case 0x20: return 'D';
            case 0x11: return 'W';
            case 0x2A: return 0x10; // VK_SHIFT
            case 0x39: return 0x20; // VK_SPACE
            default:   return static_cast<uint32_t>(0x30 + (makeCode & 0x0F));
        }
    }

    HID_SUBSYSTEM_TELEMETRY getTelemetry() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.activeTopLevelCollections = static_cast<uint32_t>(m_collections.size());
        return m_telemetry;
    }

    void recordDescriptorParsed() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.totalDescriptorsParsed++;
    }

    void recordInputReportRead() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.totalHidInputReportsRead++;
    }

    void recordOutputReportWritten() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.totalHidOutputReportsWritten++;
    }

    void recordFeatureTransfer() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.totalHidFeatureTransfers++;
    }

private:
    TitanHidInputSubsystem() {
        setupInternalState();
    }

    void setupInternalState() {
        m_collections.clear();
        m_keyboardQueue.clear();
        m_mouseQueue.clear();
        m_telemetry = HID_SUBSYSTEM_TELEMETRY{};

        m_keyboardAttributes = KEYBOARD_ATTRIBUTES{};
        m_keyboardIndicators = KEYBOARD_INDICATOR_PARAMETERS{0, KEYBOARD_NUM_LOCK_ON};
        m_keyboardTypematic  = KEYBOARD_TYPEMATIC_PARAMETERS{0, 30, 250};
        m_mouseAttributes    = MOUSE_ATTRIBUTES{};

        m_i8042StatusReg    = I8042_STS_SYS;
        m_i8042OutputBuffer = 0x55;
        m_i8042ConfigByte   = 0x47; // IRQ1 enabled, IRQ12 enabled, System Flag, Translation enabled
        m_i8042LastCommand  = 0;
        m_i8042ExtendedE0   = false;
        m_i8042ExtendedE1   = false;
        m_ps2LeftDown       = false;
        m_ps2RightDown      = false;
        m_ps2MiddleDown     = false;

        // Register 3 Canonical Top-Level Collections:
        // 1. Collection 0x4101: Sovereign Mechanical RGB Keyboard
        {
            HID_TOP_LEVEL_COLLECTION kbd{};
            kbd.DeviceHandle       = reinterpret_cast<win32::HANDLE>(0x4101);
            kbd.DevicePath         = "\\Device\\00000041_HID_KBD";
            kbd.HardwareId         = L"HID\\VID_045E&PID_082C&REV_0110";
            kbd.ManufacturerString = L"Barrer Software Sovereign Hardware";
            kbd.ProductString      = L"MicaNT Prism Mechanical Keyboard";
            kbd.SerialNumberString = L"MICA-KBD-0001";
            kbd.Attributes         = {sizeof(HIDD_ATTRIBUTES), 0x045E, 0x082C, 0x0110};
            kbd.InterfaceGuid      = GUID_DEVINTERFACE_KEYBOARD;
            kbd.XhciSlotId         = 2;
            kbd.RawReportDescriptor.assign(
                Descriptors::kStandardKeyboardReportDesc.begin(),
                Descriptors::kStandardKeyboardReportDesc.end());
            HidReportDescriptorParser::ParseDescriptor(
                kbd.RawReportDescriptor.data(),
                kbd.RawReportDescriptor.size(),
                kbd.PreparsedData);
            m_telemetry.totalDescriptorsParsed++;
            m_collections[0x4101] = std::move(kbd);
        }

        // 2. Collection 0x4102: Logitech Optical Gaming Mouse (Slot 2)
        {
            HID_TOP_LEVEL_COLLECTION mou{};
            mou.DeviceHandle       = reinterpret_cast<win32::HANDLE>(0x4102);
            mou.DevicePath         = "\\Device\\00000042_HID_MOU";
            mou.HardwareId         = L"HID\\VID_046D&PID_C077&REV_7200";
            mou.ManufacturerString = L"Logitech";
            mou.ProductString      = L"Logitech USB Optical Gaming Mouse";
            mou.SerialNumberString = L"LOGI-MOU-0042";
            mou.Attributes         = {sizeof(HIDD_ATTRIBUTES), 0x046D, 0xC077, 0x7200};
            mou.InterfaceGuid      = GUID_DEVINTERFACE_MOUSE;
            mou.XhciSlotId         = 2;
            mou.RawReportDescriptor.assign(
                Descriptors::kGamingMouseReportDesc.begin(),
                Descriptors::kGamingMouseReportDesc.end());
            HidReportDescriptorParser::ParseDescriptor(
                mou.RawReportDescriptor.data(),
                mou.RawReportDescriptor.size(),
                mou.PreparsedData);
            m_telemetry.totalDescriptorsParsed++;
            m_collections[0x4102] = std::move(mou);
        }

        // 3. Collection 0x4103: Sovereign Dual-Axis Wireless Controller
        {
            HID_TOP_LEVEL_COLLECTION pad{};
            pad.DeviceHandle       = reinterpret_cast<win32::HANDLE>(0x4103);
            pad.DevicePath         = "\\Device\\00000043_HID_GAMEPAD";
            pad.HardwareId         = L"HID\\VID_045E&PID_0B12&REV_0509";
            pad.ManufacturerString = L"Barrer Software Sovereign Hardware";
            pad.ProductString      = L"MicaNT Sovereign Pro Controller";
            pad.SerialNumberString = L"MICA-PAD-0043";
            pad.Attributes         = {sizeof(HIDD_ATTRIBUTES), 0x045E, 0x0B12, 0x0509};
            pad.InterfaceGuid      = GUID_DEVINTERFACE_HID;
            pad.XhciSlotId         = 2;
            pad.RawReportDescriptor.assign(
                Descriptors::kDualGamepadReportDesc.begin(),
                Descriptors::kDualGamepadReportDesc.end());
            HidReportDescriptorParser::ParseDescriptor(
                pad.RawReportDescriptor.data(),
                pad.RawReportDescriptor.size(),
                pad.PreparsedData);
            pad.LastFeatureReport = {0x00, 0x80, 0x80, 0x40, 0xFF}; // Default calibration feature report
            m_telemetry.totalDescriptorsParsed++;
            m_collections[0x4103] = std::move(pad);
        }

        m_telemetry.activeTopLevelCollections = static_cast<uint32_t>(m_collections.size());
    }

    std::mutex m_mutex;
    bool       m_initialized{false};
    std::map<uintptr_t, HID_TOP_LEVEL_COLLECTION> m_collections;

    // Keyboard & Mouse Class State
    std::deque<KEYBOARD_INPUT_DATA> m_keyboardQueue;
    std::deque<MOUSE_INPUT_DATA>    m_mouseQueue;
    KEYBOARD_ATTRIBUTES             m_keyboardAttributes{};
    KEYBOARD_INDICATOR_PARAMETERS   m_keyboardIndicators{};
    KEYBOARD_TYPEMATIC_PARAMETERS   m_keyboardTypematic{};
    MOUSE_ATTRIBUTES                m_mouseAttributes{};

    // i8042 Controller State
    uint8_t m_i8042StatusReg{I8042_STS_SYS};
    uint8_t m_i8042OutputBuffer{0x55};
    uint8_t m_i8042ConfigByte{0x47};
    uint8_t m_i8042LastCommand{0};
    bool    m_i8042ExtendedE0{false};
    bool    m_i8042ExtendedE1{false};
    bool    m_ps2LeftDown{false};
    bool    m_ps2RightDown{false};
    bool    m_ps2MiddleDown{false};

    HID_SUBSYSTEM_TELEMETRY m_telemetry{};
};

// ============================================================================
// 9. Standard C ABI Exports (hid.dll, hidparse.sys, hidclass.sys, kbdclass.sys, mouclass.sys)
// ============================================================================

extern "C" {

// --- hid.dll User-Mode Client APIs ---

inline void WINAPI HidD_GetHidGuid(HID_GUID* HidGuid) {
    if (HidGuid) {
        *HidGuid = GUID_DEVINTERFACE_HID;
    }
}

inline BOOLEAN WINAPI HidD_GetAttributes(
    win32::HANDLE HidDeviceObject,
    HIDD_ATTRIBUTES* Attributes
) {
    if (!Attributes) return FALSE_B;
    auto* col = TitanHidInputSubsystem::Instance().findCollection(HidDeviceObject);
    if (!col) return FALSE_B;
    *Attributes = col->Attributes;
    return TRUE_B;
}

inline BOOLEAN WINAPI HidD_GetPreparsedData(
    win32::HANDLE HidDeviceObject,
    PHIDP_PREPARSED_DATA* PreparsedData
) {
    if (!PreparsedData) return FALSE_B;
    auto* col = TitanHidInputSubsystem::Instance().findCollection(HidDeviceObject);
    if (!col) return FALSE_B;
    auto* copy = new _HIDP_PREPARSED_DATA(col->PreparsedData);
    *PreparsedData = copy;
    return TRUE_B;
}

inline BOOLEAN WINAPI HidD_FreePreparsedData(PHIDP_PREPARSED_DATA PreparsedData) {
    if (!PreparsedData || PreparsedData->Signature != HIDP_PREPARSED_DATA_SIGNATURE) {
        return FALSE_B;
    }
    delete PreparsedData;
    return TRUE_B;
}

inline BOOLEAN WINAPI HidD_GetCollectionInformation(
    win32::HANDLE HidDeviceObject,
    HID_COLLECTION_INFORMATION* Buffer,
    ULONG BufferSize
) {
    if (!Buffer || BufferSize < sizeof(HID_COLLECTION_INFORMATION)) return FALSE_B;
    auto* col = TitanHidInputSubsystem::Instance().findCollection(HidDeviceObject);
    if (!col) return FALSE_B;
    Buffer->DescriptorSize = static_cast<ULONG>(sizeof(_HIDP_PREPARSED_DATA));
    Buffer->Polled         = FALSE_B;
    Buffer->VendorID       = col->Attributes.VendorID;
    Buffer->ProductID      = col->Attributes.ProductID;
    Buffer->VersionNumber  = col->Attributes.VersionNumber;
    return TRUE_B;
}

inline BOOLEAN WINAPI HidD_GetManufacturerString(
    win32::HANDLE HidDeviceObject,
    PVOID Buffer,
    ULONG BufferLength
) {
    if (!Buffer || BufferLength < sizeof(wchar_t)) return FALSE_B;
    auto* col = TitanHidInputSubsystem::Instance().findCollection(HidDeviceObject);
    if (!col) return FALSE_B;
    size_t maxChars = (BufferLength / sizeof(wchar_t)) - 1;
    size_t copyLen  = std::min(maxChars, col->ManufacturerString.size());
    wchar_t* out    = static_cast<wchar_t*>(Buffer);
    std::memcpy(out, col->ManufacturerString.data(), copyLen * sizeof(wchar_t));
    out[copyLen] = L'\0';
    return TRUE_B;
}

inline BOOLEAN WINAPI HidD_GetProductString(
    win32::HANDLE HidDeviceObject,
    PVOID Buffer,
    ULONG BufferLength
) {
    if (!Buffer || BufferLength < sizeof(wchar_t)) return FALSE_B;
    auto* col = TitanHidInputSubsystem::Instance().findCollection(HidDeviceObject);
    if (!col) return FALSE_B;
    size_t maxChars = (BufferLength / sizeof(wchar_t)) - 1;
    size_t copyLen  = std::min(maxChars, col->ProductString.size());
    wchar_t* out    = static_cast<wchar_t*>(Buffer);
    std::memcpy(out, col->ProductString.data(), copyLen * sizeof(wchar_t));
    out[copyLen] = L'\0';
    return TRUE_B;
}

inline BOOLEAN WINAPI HidD_GetSerialNumberString(
    win32::HANDLE HidDeviceObject,
    PVOID Buffer,
    ULONG BufferLength
) {
    if (!Buffer || BufferLength < sizeof(wchar_t)) return FALSE_B;
    auto* col = TitanHidInputSubsystem::Instance().findCollection(HidDeviceObject);
    if (!col) return FALSE_B;
    size_t maxChars = (BufferLength / sizeof(wchar_t)) - 1;
    size_t copyLen  = std::min(maxChars, col->SerialNumberString.size());
    wchar_t* out    = static_cast<wchar_t*>(Buffer);
    std::memcpy(out, col->SerialNumberString.data(), copyLen * sizeof(wchar_t));
    out[copyLen] = L'\0';
    return TRUE_B;
}

inline BOOLEAN WINAPI HidD_GetNumInputBuffers(
    win32::HANDLE HidDeviceObject,
    ULONG* NumberBuffers
) {
    if (!NumberBuffers) return FALSE_B;
    auto* col = TitanHidInputSubsystem::Instance().findCollection(HidDeviceObject);
    if (!col) return FALSE_B;
    *NumberBuffers = col->NumInputBuffers;
    return TRUE_B;
}

inline BOOLEAN WINAPI HidD_SetNumInputBuffers(
    win32::HANDLE HidDeviceObject,
    ULONG NumberBuffers
) {
    if (NumberBuffers < 2) return FALSE_B;
    auto* col = TitanHidInputSubsystem::Instance().findCollection(HidDeviceObject);
    if (!col) return FALSE_B;
    col->NumInputBuffers = NumberBuffers;
    return TRUE_B;
}

inline BOOLEAN WINAPI HidD_GetFeature(
    win32::HANDLE HidDeviceObject,
    PVOID ReportBuffer,
    ULONG ReportBufferLength
) {
    if (!ReportBuffer || ReportBufferLength == 0) return FALSE_B;
    uint8_t reportId = *static_cast<uint8_t*>(ReportBuffer);
    NtStatus st = TitanHidInputSubsystem::Instance().submitHidClassControlUrb(
        HidDeviceObject, USB_HID_REQ_GET_REPORT, 0x03, reportId, ReportBuffer, static_cast<uint16_t>(ReportBufferLength));
    return (st == NtStatus::Success) ? TRUE_B : FALSE_B;
}

inline BOOLEAN WINAPI HidD_SetFeature(
    win32::HANDLE HidDeviceObject,
    PVOID ReportBuffer,
    ULONG ReportBufferLength
) {
    if (!ReportBuffer || ReportBufferLength == 0) return FALSE_B;
    uint8_t reportId = *static_cast<const uint8_t*>(ReportBuffer);
    NtStatus st = TitanHidInputSubsystem::Instance().submitHidClassControlUrb(
        HidDeviceObject, USB_HID_REQ_SET_REPORT, 0x03, reportId, ReportBuffer, static_cast<uint16_t>(ReportBufferLength));
    return (st == NtStatus::Success) ? TRUE_B : FALSE_B;
}

inline BOOLEAN WINAPI HidD_GetInputReport(
    win32::HANDLE HidDeviceObject,
    PVOID ReportBuffer,
    ULONG ReportBufferLength
) {
    if (!ReportBuffer || ReportBufferLength == 0) return FALSE_B;
    uint8_t reportId = *static_cast<uint8_t*>(ReportBuffer);
    NtStatus st = TitanHidInputSubsystem::Instance().submitHidClassControlUrb(
        HidDeviceObject, USB_HID_REQ_GET_REPORT, 0x01, reportId, ReportBuffer, static_cast<uint16_t>(ReportBufferLength));
    return (st == NtStatus::Success) ? TRUE_B : FALSE_B;
}

inline BOOLEAN WINAPI HidD_SetOutputReport(
    win32::HANDLE HidDeviceObject,
    PVOID ReportBuffer,
    ULONG ReportBufferLength
) {
    if (!ReportBuffer || ReportBufferLength == 0) return FALSE_B;
    uint8_t reportId = *static_cast<const uint8_t*>(ReportBuffer);
    NtStatus st = TitanHidInputSubsystem::Instance().submitHidClassControlUrb(
        HidDeviceObject, USB_HID_REQ_SET_REPORT, 0x02, reportId, ReportBuffer, static_cast<uint16_t>(ReportBufferLength));
    return (st == NtStatus::Success) ? TRUE_B : FALSE_B;
}

// --- hidparse.sys / hid.dll Report Parsing & Extraction APIs ---

inline HIDP_STATUS WINAPI HidP_GetCaps(
    PHIDP_PREPARSED_DATA PreparsedData,
    HIDP_CAPS* Capabilities
) {
    if (!PreparsedData || PreparsedData->Signature != HIDP_PREPARSED_DATA_SIGNATURE) {
        return HIDP_STATUS_INVALID_PREPARSED_DATA;
    }
    if (!Capabilities) return HIDP_STATUS_NULL;
    *Capabilities = PreparsedData->Caps;
    return HIDP_STATUS_SUCCESS;
}

inline HIDP_STATUS WINAPI HidP_GetButtonCaps(
    HIDP_REPORT_TYPE ReportType,
    HIDP_BUTTON_CAPS* ButtonCaps,
    USHORT* ButtonCapsLength,
    PHIDP_PREPARSED_DATA PreparsedData
) {
    if (!PreparsedData || PreparsedData->Signature != HIDP_PREPARSED_DATA_SIGNATURE) {
        return HIDP_STATUS_INVALID_PREPARSED_DATA;
    }
    if (!ButtonCapsLength) return HIDP_STATUS_NULL;

    const auto* vec = (ReportType == HidP_Input)   ? &PreparsedData->InputButtonCaps :
                      (ReportType == HidP_Output)  ? &PreparsedData->OutputButtonCaps :
                      (ReportType == HidP_Feature) ? &PreparsedData->FeatureButtonCaps : nullptr;
    if (!vec) return HIDP_STATUS_INVALID_REPORT_TYPE;

    USHORT available = *ButtonCapsLength;
    *ButtonCapsLength = static_cast<USHORT>(vec->size());
    if (!ButtonCaps || available < vec->size()) {
        return HIDP_STATUS_BUFFER_TOO_SMALL;
    }

    for (size_t i = 0; i < vec->size(); ++i) {
        ButtonCaps[i] = (*vec)[i];
    }
    return HIDP_STATUS_SUCCESS;
}

inline HIDP_STATUS WINAPI HidP_GetValueCaps(
    HIDP_REPORT_TYPE ReportType,
    HIDP_VALUE_CAPS* ValueCaps,
    USHORT* ValueCapsLength,
    PHIDP_PREPARSED_DATA PreparsedData
) {
    if (!PreparsedData || PreparsedData->Signature != HIDP_PREPARSED_DATA_SIGNATURE) {
        return HIDP_STATUS_INVALID_PREPARSED_DATA;
    }
    if (!ValueCapsLength) return HIDP_STATUS_NULL;

    const auto* vec = (ReportType == HidP_Input)   ? &PreparsedData->InputValueCaps :
                      (ReportType == HidP_Output)  ? &PreparsedData->OutputValueCaps :
                      (ReportType == HidP_Feature) ? &PreparsedData->FeatureValueCaps : nullptr;
    if (!vec) return HIDP_STATUS_INVALID_REPORT_TYPE;

    USHORT available = *ValueCapsLength;
    *ValueCapsLength = static_cast<USHORT>(vec->size());
    if (!ValueCaps || available < vec->size()) {
        return HIDP_STATUS_BUFFER_TOO_SMALL;
    }

    for (size_t i = 0; i < vec->size(); ++i) {
        ValueCaps[i] = (*vec)[i];
    }
    return HIDP_STATUS_SUCCESS;
}

inline HIDP_STATUS WINAPI HidP_GetUsages(
    HIDP_REPORT_TYPE ReportType,
    USAGE UsagePage,
    USHORT LinkCollection,
    PUSAGE UsageList,
    ULONG* UsageLength,
    PHIDP_PREPARSED_DATA PreparsedData,
    PCHAR Report,
    ULONG ReportLength
) {
    (void)LinkCollection;
    if (!PreparsedData || PreparsedData->Signature != HIDP_PREPARSED_DATA_SIGNATURE) {
        return HIDP_STATUS_INVALID_PREPARSED_DATA;
    }
    if (!UsageLength || !Report || ReportLength < 2) {
        return HIDP_STATUS_INVALID_REPORT_LENGTH;
    }

    const auto* vec = (ReportType == HidP_Input)   ? &PreparsedData->InputButtonCaps :
                      (ReportType == HidP_Output)  ? &PreparsedData->OutputButtonCaps :
                      (ReportType == HidP_Feature) ? &PreparsedData->FeatureButtonCaps : nullptr;
    if (!vec) return HIDP_STATUS_INVALID_REPORT_TYPE;

    // Payload starts after 1-byte ReportID
    const uint8_t* payload = reinterpret_cast<const uint8_t*>(Report) + 1;
    size_t payloadBytes = ReportLength - 1;

    std::vector<USAGE> activeUsages;
    bool matchedUsagePage = false;

    for (const auto& bcap : *vec) {
        if (bcap.UsagePage != UsagePage) continue;
        matchedUsagePage = true;

        if (bcap.InternalBitSize == 1) {
            uint32_t count = bcap.ReportCount;
            for (uint32_t idx = 0; idx < count; ++idx) {
                uint32_t bitVal = HidReportDescriptorParser::ReadBits(
                    payload, payloadBytes, bcap.InternalBitOffset + idx, 1);
                if (bitVal != 0) {
                    USAGE u = (bcap.IsRange == TRUE_B)
                        ? static_cast<USAGE>(bcap.Range.UsageMin + idx)
                        : bcap.NotRange.Usage;
                    activeUsages.push_back(u);
                }
            }
        } else if (bcap.InternalBitSize == 8) {
            // 6KRO Key Array
            for (uint32_t idx = 0; idx < bcap.ReportCount; ++idx) {
                uint32_t byteVal = HidReportDescriptorParser::ReadBits(
                    payload, payloadBytes, bcap.InternalBitOffset + idx * 8, 8);
                if (byteVal != 0) {
                    activeUsages.push_back(static_cast<USAGE>(byteVal));
                }
            }
        }
    }

    if (!matchedUsagePage) return HIDP_STATUS_USAGE_NOT_FOUND;

    ULONG maxOut = *UsageLength;
    *UsageLength = static_cast<ULONG>(activeUsages.size());
    if (!UsageList && !activeUsages.empty()) return HIDP_STATUS_BUFFER_TOO_SMALL;
    if (maxOut < activeUsages.size()) return HIDP_STATUS_BUFFER_TOO_SMALL;

    for (size_t i = 0; i < activeUsages.size(); ++i) {
        UsageList[i] = activeUsages[i];
    }
    TitanHidInputSubsystem::Instance().recordInputReportRead();
    return HIDP_STATUS_SUCCESS;
}

inline HIDP_STATUS WINAPI HidP_SetUsages(
    HIDP_REPORT_TYPE ReportType,
    USAGE UsagePage,
    USHORT LinkCollection,
    PUSAGE UsageList,
    ULONG* UsageLength,
    PHIDP_PREPARSED_DATA PreparsedData,
    PCHAR Report,
    ULONG ReportLength
) {
    (void)LinkCollection;
    if (!PreparsedData || PreparsedData->Signature != HIDP_PREPARSED_DATA_SIGNATURE) {
        return HIDP_STATUS_INVALID_PREPARSED_DATA;
    }
    if (!UsageLength || !UsageList || !Report || ReportLength < 2) {
        return HIDP_STATUS_INVALID_REPORT_LENGTH;
    }

    const auto* vec = (ReportType == HidP_Input)   ? &PreparsedData->InputButtonCaps :
                      (ReportType == HidP_Output)  ? &PreparsedData->OutputButtonCaps :
                      (ReportType == HidP_Feature) ? &PreparsedData->FeatureButtonCaps : nullptr;
    if (!vec) return HIDP_STATUS_INVALID_REPORT_TYPE;

    uint8_t* payload = reinterpret_cast<uint8_t*>(Report) + 1;
    size_t payloadBytes = ReportLength - 1;

    for (ULONG uIdx = 0; uIdx < *UsageLength; ++uIdx) {
        USAGE targetUsage = UsageList[uIdx];
        bool placed = false;

        for (const auto& bcap : *vec) {
            if (bcap.UsagePage != UsagePage) continue;
            if (bcap.InternalBitSize == 1) {
                if (bcap.IsRange == TRUE_B &&
                    targetUsage >= bcap.Range.UsageMin &&
                    targetUsage <= bcap.Range.UsageMax) {
                    uint32_t offset = bcap.InternalBitOffset + (targetUsage - bcap.Range.UsageMin);
                    HidReportDescriptorParser::WriteBits(payload, payloadBytes, offset, 1, 1);
                    placed = true;
                    break;
                } else if (bcap.IsRange == FALSE_B && targetUsage == bcap.NotRange.Usage) {
                    HidReportDescriptorParser::WriteBits(payload, payloadBytes, bcap.InternalBitOffset, 1, 1);
                    placed = true;
                    break;
                }
            } else if (bcap.InternalBitSize == 8) {
                // Find first zero slot in key array
                for (uint32_t slot = 0; slot < bcap.ReportCount; ++slot) {
                    uint32_t existing = HidReportDescriptorParser::ReadBits(
                        payload, payloadBytes, bcap.InternalBitOffset + slot * 8, 8);
                    if (existing == 0 || existing == targetUsage) {
                        HidReportDescriptorParser::WriteBits(
                            payload, payloadBytes, bcap.InternalBitOffset + slot * 8, 8, targetUsage);
                        placed = true;
                        break;
                    }
                }
                if (placed) break;
            }
        }
        if (!placed) return HIDP_STATUS_USAGE_NOT_FOUND;
    }

    TitanHidInputSubsystem::Instance().recordOutputReportWritten();
    return HIDP_STATUS_SUCCESS;
}

inline HIDP_STATUS WINAPI HidP_GetUsageValue(
    HIDP_REPORT_TYPE ReportType,
    USAGE UsagePage,
    USHORT LinkCollection,
    USAGE Usage,
    ULONG* UsageValue,
    PHIDP_PREPARSED_DATA PreparsedData,
    PCHAR Report,
    ULONG ReportLength
) {
    (void)LinkCollection;
    if (!PreparsedData || PreparsedData->Signature != HIDP_PREPARSED_DATA_SIGNATURE) {
        return HIDP_STATUS_INVALID_PREPARSED_DATA;
    }
    if (!UsageValue || !Report || ReportLength < 2) {
        return HIDP_STATUS_INVALID_REPORT_LENGTH;
    }

    const auto* vec = (ReportType == HidP_Input)   ? &PreparsedData->InputValueCaps :
                      (ReportType == HidP_Output)  ? &PreparsedData->OutputValueCaps :
                      (ReportType == HidP_Feature) ? &PreparsedData->FeatureValueCaps : nullptr;
    if (!vec) return HIDP_STATUS_INVALID_REPORT_TYPE;

    const uint8_t* payload = reinterpret_cast<const uint8_t*>(Report) + 1;
    size_t payloadBytes = ReportLength - 1;

    for (const auto& vcap : *vec) {
        if (vcap.UsagePage != UsagePage) continue;
        bool match = false;
        uint32_t elemIndex = 0;
        if (vcap.IsRange == TRUE_B) {
            if (Usage >= vcap.Range.UsageMin && Usage <= vcap.Range.UsageMax) {
                match = true;
                elemIndex = Usage - vcap.Range.UsageMin;
            }
        } else {
            if (Usage == vcap.NotRange.Usage) {
                match = true;
            }
        }

        if (match) {
            uint32_t bitOffset = vcap.InternalBitOffset + elemIndex * vcap.BitSize;
            uint32_t rawVal = HidReportDescriptorParser::ReadBits(payload, payloadBytes, bitOffset, vcap.BitSize);

            // Sign-extend if LogicalMin < 0
            if (vcap.LogicalMin < 0 && vcap.BitSize > 1 && vcap.BitSize < 32) {
                uint32_t signBit = 1u << (vcap.BitSize - 1);
                if (rawVal & signBit) {
                    uint32_t mask = ~((1u << vcap.BitSize) - 1u);
                    rawVal |= mask;
                }
            }
            *UsageValue = rawVal;
            return HIDP_STATUS_SUCCESS;
        }
    }

    return HIDP_STATUS_USAGE_NOT_FOUND;
}

inline HIDP_STATUS WINAPI HidP_SetUsageValue(
    HIDP_REPORT_TYPE ReportType,
    USAGE UsagePage,
    USHORT LinkCollection,
    USAGE Usage,
    ULONG UsageValue,
    PHIDP_PREPARSED_DATA PreparsedData,
    PCHAR Report,
    ULONG ReportLength
) {
    (void)LinkCollection;
    if (!PreparsedData || PreparsedData->Signature != HIDP_PREPARSED_DATA_SIGNATURE) {
        return HIDP_STATUS_INVALID_PREPARSED_DATA;
    }
    if (!Report || ReportLength < 2) {
        return HIDP_STATUS_INVALID_REPORT_LENGTH;
    }

    const auto* vec = (ReportType == HidP_Input)   ? &PreparsedData->InputValueCaps :
                      (ReportType == HidP_Output)  ? &PreparsedData->OutputValueCaps :
                      (ReportType == HidP_Feature) ? &PreparsedData->FeatureValueCaps : nullptr;
    if (!vec) return HIDP_STATUS_INVALID_REPORT_TYPE;

    uint8_t* payload = reinterpret_cast<uint8_t*>(Report) + 1;
    size_t payloadBytes = ReportLength - 1;

    for (const auto& vcap : *vec) {
        if (vcap.UsagePage != UsagePage) continue;
        bool match = false;
        uint32_t elemIndex = 0;
        if (vcap.IsRange == TRUE_B) {
            if (Usage >= vcap.Range.UsageMin && Usage <= vcap.Range.UsageMax) {
                match = true;
                elemIndex = Usage - vcap.Range.UsageMin;
            }
        } else {
            if (Usage == vcap.NotRange.Usage) {
                match = true;
            }
        }

        if (match) {
            uint32_t bitOffset = vcap.InternalBitOffset + elemIndex * vcap.BitSize;
            HidReportDescriptorParser::WriteBits(payload, payloadBytes, bitOffset, vcap.BitSize, UsageValue);
            return HIDP_STATUS_SUCCESS;
        }
    }

    return HIDP_STATUS_USAGE_NOT_FOUND;
}

// --- Kernel Driver Exports (kbdclass.sys, mouclass.sys, i8042prt.sys, hidclass.sys) ---

inline void WINAPI KeyboardClassServiceCallback(
    const KEYBOARD_INPUT_DATA* InputDataStart,
    const KEYBOARD_INPUT_DATA* InputDataEnd,
    ULONG* InputDataConsumed
) {
    TitanHidInputSubsystem::Instance().keyboardClassServiceCallback(InputDataStart, InputDataEnd, InputDataConsumed);
}

inline void WINAPI MouseClassServiceCallback(
    const MOUSE_INPUT_DATA* InputDataStart,
    const MOUSE_INPUT_DATA* InputDataEnd,
    ULONG* InputDataConsumed
) {
    TitanHidInputSubsystem::Instance().mouseClassServiceCallback(InputDataStart, InputDataEnd, InputDataConsumed);
}

inline uint8_t WINAPI I8042ReadPortUchar(uint16_t Port) {
    return TitanHidInputSubsystem::Instance().i8042ReadPort(Port);
}

inline void WINAPI I8042WritePortUchar(uint16_t Port, uint8_t Value) {
    TitanHidInputSubsystem::Instance().i8042WritePort(Port, Value);
}

inline BOOL WINAPI HidGetTelemetry(HID_SUBSYSTEM_TELEMETRY* outTelemetry) {
    if (!outTelemetry) return FALSE_VAL;
    *outTelemetry = TitanHidInputSubsystem::Instance().getTelemetry();
    return TRUE_VAL;
}

} // extern "C"

inline HID_SUBSYSTEM_TELEMETRY HidQueryTelemetry() noexcept {
    return TitanHidInputSubsystem::Instance().getTelemetry();
}

// ============================================================================
// 10. Subsystem Export Registration (JanusLDR, SCM, VersionDatabase)
// ============================================================================

inline void InitializeHidSubsystemExports() {
    xhci::InitializeXhciSubsystemExports();

    auto& ldr = ldr::DynamicLoader::get();

    // 1. Register hid.dll exports
    ldr.registerExport("hid.dll", "HidD_GetHidGuid", reinterpret_cast<void*>(HidD_GetHidGuid));
    ldr.registerExport("hid.dll", "HidD_GetAttributes", reinterpret_cast<void*>(HidD_GetAttributes));
    ldr.registerExport("hid.dll", "HidD_GetPreparsedData", reinterpret_cast<void*>(HidD_GetPreparsedData));
    ldr.registerExport("hid.dll", "HidD_FreePreparsedData", reinterpret_cast<void*>(HidD_FreePreparsedData));
    ldr.registerExport("hid.dll", "HidD_GetCollectionInformation", reinterpret_cast<void*>(HidD_GetCollectionInformation));
    ldr.registerExport("hid.dll", "HidD_GetManufacturerString", reinterpret_cast<void*>(HidD_GetManufacturerString));
    ldr.registerExport("hid.dll", "HidD_GetProductString", reinterpret_cast<void*>(HidD_GetProductString));
    ldr.registerExport("hid.dll", "HidD_GetSerialNumberString", reinterpret_cast<void*>(HidD_GetSerialNumberString));
    ldr.registerExport("hid.dll", "HidD_GetNumInputBuffers", reinterpret_cast<void*>(HidD_GetNumInputBuffers));
    ldr.registerExport("hid.dll", "HidD_SetNumInputBuffers", reinterpret_cast<void*>(HidD_SetNumInputBuffers));
    ldr.registerExport("hid.dll", "HidD_GetFeature", reinterpret_cast<void*>(HidD_GetFeature));
    ldr.registerExport("hid.dll", "HidD_SetFeature", reinterpret_cast<void*>(HidD_SetFeature));
    ldr.registerExport("hid.dll", "HidD_GetInputReport", reinterpret_cast<void*>(HidD_GetInputReport));
    ldr.registerExport("hid.dll", "HidD_SetOutputReport", reinterpret_cast<void*>(HidD_SetOutputReport));
    ldr.registerExport("hid.dll", "HidP_GetCaps", reinterpret_cast<void*>(HidP_GetCaps));
    ldr.registerExport("hid.dll", "HidP_GetButtonCaps", reinterpret_cast<void*>(HidP_GetButtonCaps));
    ldr.registerExport("hid.dll", "HidP_GetValueCaps", reinterpret_cast<void*>(HidP_GetValueCaps));
    ldr.registerExport("hid.dll", "HidP_GetUsages", reinterpret_cast<void*>(HidP_GetUsages));
    ldr.registerExport("hid.dll", "HidP_SetUsages", reinterpret_cast<void*>(HidP_SetUsages));
    ldr.registerExport("hid.dll", "HidP_GetUsageValue", reinterpret_cast<void*>(HidP_GetUsageValue));
    ldr.registerExport("hid.dll", "HidP_SetUsageValue", reinterpret_cast<void*>(HidP_SetUsageValue));

    // 2. Register hidparse.sys exports
    ldr.registerExport("hidparse.sys", "HidP_GetCaps", reinterpret_cast<void*>(HidP_GetCaps));
    ldr.registerExport("hidparse.sys", "HidP_GetButtonCaps", reinterpret_cast<void*>(HidP_GetButtonCaps));
    ldr.registerExport("hidparse.sys", "HidP_GetValueCaps", reinterpret_cast<void*>(HidP_GetValueCaps));
    ldr.registerExport("hidparse.sys", "HidP_GetUsages", reinterpret_cast<void*>(HidP_GetUsages));
    ldr.registerExport("hidparse.sys", "HidP_SetUsages", reinterpret_cast<void*>(HidP_SetUsages));
    ldr.registerExport("hidparse.sys", "HidP_GetUsageValue", reinterpret_cast<void*>(HidP_GetUsageValue));
    ldr.registerExport("hidparse.sys", "HidP_SetUsageValue", reinterpret_cast<void*>(HidP_SetUsageValue));

    // 3. Register hidclass.sys & hidusb.sys exports
    ldr.registerExport("hidclass.sys", "HidGetTelemetry", reinterpret_cast<void*>(HidGetTelemetry));
    ldr.registerExport("hidusb.sys", "HidGetTelemetry", reinterpret_cast<void*>(HidGetTelemetry));

    // 4. Register kbdclass.sys, mouclass.sys & i8042prt.sys exports
    ldr.registerExport("kbdclass.sys", "KeyboardClassServiceCallback", reinterpret_cast<void*>(KeyboardClassServiceCallback));
    ldr.registerExport("mouclass.sys", "MouseClassServiceCallback", reinterpret_cast<void*>(MouseClassServiceCallback));
    ldr.registerExport("i8042prt.sys", "I8042ReadPortUchar", reinterpret_cast<void*>(I8042ReadPortUchar));
    ldr.registerExport("i8042prt.sys", "I8042WritePortUchar", reinterpret_cast<void*>(I8042WritePortUchar));

    // 5. Register SCM Service Records
    auto& scm = scm::ServiceControlManager::get();

    auto registerKernelDriver = [&](const std::wstring& name, const std::wstring& display, uint32_t startType, const std::wstring& bin) {
        auto svc = std::make_shared<scm::ServiceRecord>();
        svc->serviceName            = name;
        svc->displayName            = display;
        svc->serviceType            = scm::SERVICE_KERNEL_DRIVER;
        svc->startType              = startType;
        svc->errorControl           = scm::SERVICE_ERROR_NORMAL;
        svc->binaryPath             = bin;
        svc->status.dwCurrentState  = scm::SERVICE_RUNNING;
        scm.registerServiceRecord(svc);
    };

    registerKernelDriver(L"hidclass", L"Sovereign HID Class Driver", scm::SERVICE_BOOT_START, L"\\SystemRoot\\System32\\drivers\\hidclass.sys");
    registerKernelDriver(L"hidparse", L"Sovereign HID Report Descriptor Parser", scm::SERVICE_BOOT_START, L"\\SystemRoot\\System32\\drivers\\hidparse.sys");
    registerKernelDriver(L"hidusb",   L"Sovereign USB HID Miniport Driver", scm::SERVICE_SYSTEM_START, L"\\SystemRoot\\System32\\drivers\\hidusb.sys");
    registerKernelDriver(L"kbdclass", L"Sovereign Keyboard Class Driver", scm::SERVICE_BOOT_START, L"\\SystemRoot\\System32\\drivers\\kbdclass.sys");
    registerKernelDriver(L"mouclass", L"Sovereign Mouse Class Driver", scm::SERVICE_BOOT_START, L"\\SystemRoot\\System32\\drivers\\mouclass.sys");
    registerKernelDriver(L"i8042prt", L"Sovereign PS/2 i8042 Port Driver", scm::SERVICE_SYSTEM_START, L"\\SystemRoot\\System32\\drivers\\i8042prt.sys");

    // 6. Register in VersionDatabase
    auto& verDb = version::VersionDatabase::Instance();
    verDb.RegisterModule("hidclass.sys", "10.0.26100.1", "Sovereign HID Class Driver");
    verDb.RegisterModule("hidparse.sys", "10.0.26100.1", "Sovereign HID Report Descriptor Parser");
    verDb.RegisterModule("hidusb.sys",   "10.0.26100.1", "Sovereign USB HID Miniport Driver");
    verDb.RegisterModule("kbdclass.sys", "10.0.26100.1", "Sovereign Keyboard Class Driver");
    verDb.RegisterModule("mouclass.sys", "10.0.26100.1", "Sovereign Mouse Class Driver");
    verDb.RegisterModule("i8042prt.sys", "10.0.26100.1", "Sovereign PS/2 i8042 Port Controller Driver");
    verDb.RegisterModule("hid.dll",      "10.0.26100.1", "Sovereign User-Mode HID Client Library");

    TitanHidInputSubsystem::Instance().initialize();
}

} // namespace micant::km::hid

// Forward into micant parent namespace
namespace micant {
    using km::hid::TitanHidInputSubsystem;
    using km::hid::HidReportDescriptorParser;
    using km::hid::HIDD_ATTRIBUTES;
    using km::hid::HID_COLLECTION_INFORMATION;
    using km::hid::HIDP_CAPS;
    using km::hid::HIDP_BUTTON_CAPS;
    using km::hid::HIDP_VALUE_CAPS;
    using km::hid::HIDP_REPORT_TYPE;
    using km::hid::PHIDP_PREPARSED_DATA;
    using km::hid::KEYBOARD_INPUT_DATA;
    using km::hid::KEYBOARD_ATTRIBUTES;
    using km::hid::KEYBOARD_INDICATOR_PARAMETERS;
    using km::hid::MOUSE_INPUT_DATA;
    using km::hid::MOUSE_ATTRIBUTES;
    using km::hid::HID_SUBSYSTEM_TELEMETRY;
    using km::hid::HidD_GetHidGuid;
    using km::hid::HidD_GetAttributes;
    using km::hid::HidD_GetPreparsedData;
    using km::hid::HidD_FreePreparsedData;
    using km::hid::HidD_GetCollectionInformation;
    using km::hid::HidD_GetManufacturerString;
    using km::hid::HidD_GetProductString;
    using km::hid::HidD_GetSerialNumberString;
    using km::hid::HidD_GetNumInputBuffers;
    using km::hid::HidD_SetNumInputBuffers;
    using km::hid::HidD_GetFeature;
    using km::hid::HidD_SetFeature;
    using km::hid::HidD_GetInputReport;
    using km::hid::HidD_SetOutputReport;
    using km::hid::HidP_GetCaps;
    using km::hid::HidP_GetButtonCaps;
    using km::hid::HidP_GetValueCaps;
    using km::hid::HidP_GetUsages;
    using km::hid::HidP_SetUsages;
    using km::hid::HidP_GetUsageValue;
    using km::hid::HidP_SetUsageValue;
    using km::hid::KeyboardClassServiceCallback;
    using km::hid::MouseClassServiceCallback;
    using km::hid::I8042ReadPortUchar;
    using km::hid::I8042WritePortUchar;
    using km::hid::HidGetTelemetry;
    using km::hid::HidQueryTelemetry;
    using km::hid::InitializeHidSubsystemExports;
} // namespace micant
