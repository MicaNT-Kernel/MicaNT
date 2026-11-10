#pragma once

#include <cstddef>
#include <cstdint>

// MicaNT's boot-start driver ABI. Images are ordinary PE32+ native .sys files
// loaded by BOOTX64.EFI and entered by the kernel after paging is established.
// Keep this contract POD-only so a driver never depends on the kernel's STL.
namespace micant::boot_driver {

inline constexpr uint32_t DRIVER_ABI_VERSION = 1;
inline constexpr uint32_t DRIVER_FLAG_BOOT_START = 0x00000001;
inline constexpr uint32_t DRIVER_FLAG_DISPLAY = 0x00000002;

struct UnicodeString {
    uint16_t Length;
    uint16_t MaximumLength;
    wchar_t* Buffer;
};

struct DriverObject;
using DriverEntryRoutine = int32_t (*)(DriverObject*, const UnicodeString*);
using DriverUnloadRoutine = void (*)(DriverObject*);

struct KernelServices {
    uint32_t Size;
    uint32_t Version;
    uint32_t BootWidth;
    uint32_t BootHeight;
    void* Context;
    uint64_t (*TranslateVirtualAddress)(void*, const void*);
    void* (*MapDeviceMemory)(void*, uint64_t, size_t);
    void (*Log)(void*, const char*);
};

using PresentFrameRoutine = bool (*)(void*, const uint32_t*, uint32_t, uint32_t,
                                     uint32_t, int32_t, int32_t);

struct CompositionLayer {
    uint64_t SurfaceId;
    const uint32_t* Pixels;
    uint32_t Width;
    uint32_t Height;
    uint32_t Pitch;
    int32_t X;
    int32_t Y;
    uint8_t Opacity;
    uint8_t Reserved[7];
};

using PresentCompositionRoutine = bool (*)(void*, const uint32_t*, uint32_t, uint32_t,
                                            uint32_t, const CompositionLayer*, uint32_t);

struct DisplayDevice {
    uint32_t Size;
    uint32_t Version;
    uint32_t Width;
    uint32_t Height;
    uint32_t Pitch;
    uint32_t PixelFormat;
    void* Context;
    PresentFrameRoutine PresentFrame;
    PresentCompositionRoutine PresentComposition;
};

struct DriverObject {
    uint32_t Size;
    uint32_t Version;
    uint32_t Flags;
    uint32_t Reserved;
    void* DriverStart;
    size_t DriverSize;
    void* HardwareResources; // KernelServices for this boot-start device driver.
    void* DriverExtension;
    void* DeviceExtension;
    DriverUnloadRoutine DriverUnload;
    DisplayDevice* Display;
};

inline constexpr int32_t STATUS_SUCCESS = 0;
inline constexpr int32_t STATUS_NOT_SUPPORTED = static_cast<int32_t>(0xC00000BBu);
inline constexpr int32_t STATUS_DEVICE_NOT_READY = static_cast<int32_t>(0xC00000A3u);
inline constexpr int32_t STATUS_INVALID_PARAMETER = static_cast<int32_t>(0xC000000Du);

} // namespace micant::boot_driver
