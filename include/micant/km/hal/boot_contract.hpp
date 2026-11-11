#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace micant::uefi {

enum class EfiMemoryType : uint32_t {
    EfiReservedMemoryType          = 0,
    EfiLoaderCode                  = 1,
    EfiLoaderData                  = 2,
    EfiBootServicesCode            = 3,
    EfiBootServicesData            = 4,
    EfiRuntimeServicesCode         = 5,
    EfiRuntimeServicesData         = 6,
    EfiConventionalMemory          = 7,
    EfiUnusableMemory              = 8,
    EfiACPIReclaimMemory           = 9,
    EfiACPIMemoryNVS               = 10,
    EfiMemoryMappedIO              = 11,
    EfiMemoryMappedIOPortSpace     = 12,
    EfiPalCode                     = 13,
    EfiPersistentMemory            = 14,
    EfiMaxMemoryType               = 15
};

struct EfiMemoryDescriptor {
    uint32_t type{0};
    uint32_t pad{0};
    uint64_t physicalStart{0};
    uint64_t virtualStart{0};
    uint64_t numberOfPages{0};
    uint64_t attribute{0};
};

static_assert(sizeof(EfiMemoryDescriptor) == 40);
static_assert(offsetof(EfiMemoryDescriptor, physicalStart) == 8);
static_assert(offsetof(EfiMemoryDescriptor, numberOfPages) == 24);
static_assert(std::is_trivially_copyable_v<EfiMemoryDescriptor>);

} // namespace micant::uefi

namespace micant::boot {

inline constexpr uint64_t UEFI_KERNEL_HANDOFF_SIGNATURE = 0x4D4943414E54424FULL;
inline constexpr uint32_t UEFI_KERNEL_HANDOFF_VERSION = 2;

struct KernelFramebufferV1 {
    uint64_t physicalBase;
    uint64_t size;
    uint32_t width;
    uint32_t height;
    uint32_t pixelsPerScanLine;
    uint32_t pixelFormat;
};

struct KernelBootImageV1 {
    uint64_t base;
    uint64_t size;
    uint64_t entryPoint;
};

struct UefiKernelHandoffV2 {
    uint64_t signature;
    uint32_t size;
    uint32_t version;
    uint64_t memoryMapAddress;
    uint64_t memoryMapSize;
    uint32_t memoryMapDescriptorSize;
    uint32_t memoryMapDescriptorVersion;
    KernelFramebufferV1 framebuffer;
    uint64_t acpiTablePhysicalAddress;
    KernelBootImageV1 displayDriver;
};

static_assert(std::is_standard_layout_v<UefiKernelHandoffV2>);
static_assert(std::is_trivially_copyable_v<UefiKernelHandoffV2>);
static_assert(sizeof(UefiKernelHandoffV2) == 104);
static_assert(offsetof(UefiKernelHandoffV2, displayDriver) == 80);

} // namespace micant::boot
