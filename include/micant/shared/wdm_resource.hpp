#pragma once

#include <cstddef>
#include <cstdint>

// Public WDM resource-list layouts used by IRP_MN_START_DEVICE. These are
// variable-length ABI records; keep allocator-owned C++ containers out of them.
namespace micant::wdm {

#pragma pack(push, 4)
struct CM_PARTIAL_RESOURCE_DESCRIPTOR {
    uint8_t Type;
    uint8_t ShareDisposition;
    uint16_t Flags;
    union {
        struct { int64_t Start; uint32_t Length; } Generic;
        struct { int64_t Start; uint32_t Length; } Port;
        struct { uint32_t Level; uint32_t Vector; uintptr_t Affinity; } Interrupt;
        struct { int64_t Start; uint32_t Length; } Memory;
        struct { uint32_t Channel; uint32_t Port; uint32_t Reserved1; } Dma;
        struct { uint32_t Data[3]; } DevicePrivate;
        struct { uint32_t Start; uint32_t Length; uint32_t Reserved; } BusNumber;
        struct { uint32_t DataSize; uint32_t Reserved1; uint32_t Reserved2; } DeviceSpecificData;
    } u;
};

struct CM_PARTIAL_RESOURCE_LIST {
    uint16_t Version;
    uint16_t Revision;
    uint32_t Count;
    CM_PARTIAL_RESOURCE_DESCRIPTOR PartialDescriptors[1];
};

struct CM_FULL_RESOURCE_DESCRIPTOR {
    int32_t InterfaceType;
    uint32_t BusNumber;
    CM_PARTIAL_RESOURCE_LIST PartialResourceList;
};

struct CM_RESOURCE_LIST {
    uint32_t Count;
    CM_FULL_RESOURCE_DESCRIPTOR List[1];
};
#pragma pack(pop)

using PCM_RESOURCE_LIST = CM_RESOURCE_LIST*;

static_assert(sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) == (sizeof(uintptr_t) == 8 ? 20 : 16));
static_assert(offsetof(CM_PARTIAL_RESOURCE_LIST, PartialDescriptors) == 8);
static_assert(offsetof(CM_FULL_RESOURCE_DESCRIPTOR, PartialResourceList) == 8);
static_assert(offsetof(CM_RESOURCE_LIST, List) == 4);

} // namespace micant::wdm
