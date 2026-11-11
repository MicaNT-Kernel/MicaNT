#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace micant::boot {

inline constexpr uint64_t UEFI_KERNEL_HANDOFF_SIGNATURE = 0x4D4943414E54424FULL;
inline constexpr uint32_t UEFI_KERNEL_HANDOFF_VERSION = 1;

struct KernelFramebufferV1 {
    uint64_t physicalBase;
    uint64_t size;
    uint32_t width;
    uint32_t height;
    uint32_t pixelsPerScanLine;
    uint32_t pixelFormat;
};

struct UefiKernelHandoffV1 {
    uint64_t signature;
    uint32_t size;
    uint32_t version;
    uint64_t memoryMapAddress;
    uint64_t memoryMapSize;
    uint32_t memoryMapDescriptorSize;
    uint32_t memoryMapDescriptorVersion;
    KernelFramebufferV1 framebuffer;
    uint64_t acpiTablePhysicalAddress;
};

static_assert(std::is_standard_layout_v<UefiKernelHandoffV1>);
static_assert(std::is_trivially_copyable_v<UefiKernelHandoffV1>);
static_assert(sizeof(UefiKernelHandoffV1) == 80);

} // namespace micant::boot
