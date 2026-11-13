#pragma once

#include <cstddef>
#include <cstdint>

// POD contract between a boot-start OEM miniport and the canonical kernel
// NDIS/TCPIP bridge. No STL object, function object, or kernel type crosses it.
namespace micant::shared::ndis {

inline constexpr uint32_t AdapterServicesVersion = 1;
using ReceivePacketRoutine = void (*)(void* receiveContext, const uint8_t* frame,
                                      size_t frameLength);

struct AdapterServicesV1 {
    uint32_t Size;
    uint32_t Version;
    void* Context;
    uint32_t Mtu;
    uint32_t LinkState; // 0 disconnected, 1 connected
    uint64_t SpeedBps; // 0 when the miniport cannot report a negotiated link speed
    uint8_t MacAddress[6];
    uint8_t ReservedMac[2];
    int32_t (*SendPacket)(void* context, const uint8_t* frame, size_t frameLength);
    // Installs or removes the kernel indication callback (nullptr clears it).
    int32_t (*SetReceiveHandler)(void* context, ReceivePacketRoutine routine,
                                 void* receiveContext);
    // Polls and indicates zero or more received frames at PASSIVE_LEVEL.
    int32_t (*PollReceive)(void* context);
    // Stops DMA/interrupt production. Called after receive callback removal.
    int32_t (*Stop)(void* context);
};

static_assert(offsetof(AdapterServicesV1, Context) == 8);
static_assert(offsetof(AdapterServicesV1, SendPacket) % alignof(void*) == 0);
static_assert(sizeof(AdapterServicesV1) == 72);

} // namespace micant::shared::ndis
