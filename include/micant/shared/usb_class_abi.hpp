#pragma once

#include <cstddef>
#include <cstdint>

// Private, size-gated boot-driver contract. USBD owns transport and device
// lifetime; a separate class image owns protocol state and class behavior.
namespace micant::shared::usb {
inline constexpr uint32_t ClassServicesVersion = 1;
inline constexpr uint32_t TransferServicesVersion = 1;
enum class TransferKind : uint8_t { InterruptIn = 1, BulkPair = 2 };

struct InterfaceV1 {
    uint32_t Size;
    TransferKind Kind;
    uint8_t Configuration, InterfaceNumber, Alternate, Protocol;
    uint8_t InEndpoint, OutEndpoint, Interval;
    uint16_t InPacket, OutPacket, ReportDescriptorLength;
    uint8_t InBurst, OutBurst;
    uint16_t BytesPerInterval;
    bool InCompanion, OutCompanion;
    uint8_t DescriptorRequestType, DescriptorRequest, ControlRequestType, ControlRequest;
    uint16_t DescriptorValue, DescriptorIndex, ControlValue, ControlIndex;
    bool NeedsControl;
};
struct TransferServicesV1 {
    uint32_t Size, Version;
    void* Context;
    bool (*Begin)(void*);
    void (*End)(void*);
    int32_t (*BulkTransfer)(void*, uint8_t, void*, size_t, size_t*, uint32_t);
    int32_t (*BulkReset)(void*, uint8_t, uint8_t, uint8_t);
    uint32_t (*SubmitKeyboard)(void*, const void*, uint32_t);
    uint32_t (*SubmitMouse)(void*, const void*, uint32_t);
};
struct ClassServicesV1 {
    uint32_t Size, Version;
    void* Context;
    // Success means matched and filled; STATUS_NOT_SUPPORTED means no match.
    int32_t (*MatchConfiguration)(void*, const uint8_t*, size_t, InterfaceV1*);
    int32_t (*Create)(void*, const InterfaceV1*, const TransferServicesV1*, uint16_t, void**);
    int32_t (*ValidateReportDescriptor)(void*, const uint8_t*, size_t);
    int32_t (*Start)(void*);
    int32_t (*Report)(void*, const uint8_t*, size_t);
    // On DEVICE_BUSY the instance remains owned and removal must be retried.
    int32_t (*Remove)(void*);
};
}
