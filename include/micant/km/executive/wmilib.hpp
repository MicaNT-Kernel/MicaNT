#pragma once

// Windows WMI library contracts used by kernel drivers. The implementation
// below handles the standard data-block query IRPs and leaves optional
// operations to WMILIB_CONTEXT callbacks, matching wmilib.h's public ABI.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "micant/km/executive/io.hpp"

namespace micant::km::wmilib {

enum class WMIEnableDisableControl : uint32_t { Event, DataBlock };
enum class SysctlIrpDisposition : uint32_t { Processed, NotCompleted, NotWmi, Forward };

struct WMIGUIDREGINFO {
    const GUID* Guid;
    uint32_t InstanceCount;
    uint32_t Flags;
};

using WmiQueryDataBlock = NtStatus (NTAPI *)(io::DeviceObject*, io::Irp*, uint32_t guidIndex,
    uint32_t instanceIndex, uint32_t instanceCount, uint32_t* instanceLengthArray,
    uint32_t bufferAvailable, uint8_t* buffer);
using WmiQueryRegInfo = NtStatus (NTAPI *)(io::DeviceObject*, uint32_t* registrationFlags,
    UNICODE_STRING* instanceName, UNICODE_STRING** registryPath,
    UNICODE_STRING* mofResourceName, io::DeviceObject** pdo);
using WmiSetDataBlock = NtStatus (NTAPI *)(io::DeviceObject*, io::Irp*, uint32_t, uint32_t,
    uint32_t, uint8_t*);
using WmiSetDataItem = NtStatus (NTAPI *)(io::DeviceObject*, io::Irp*, uint32_t, uint32_t,
    uint32_t, uint32_t, uint8_t*);
using WmiExecuteMethod = NtStatus (NTAPI *)(io::DeviceObject*, io::Irp*, uint32_t, uint32_t,
    uint32_t, uint32_t, uint32_t, uint8_t*);
using WmiFunctionControl = NtStatus (NTAPI *)(io::DeviceObject*, io::Irp*, uint32_t,
    WMIEnableDisableControl, bool);

struct WMILIB_CONTEXT {
    uint32_t GuidCount{};
    WMIGUIDREGINFO* GuidList{};
    WmiQueryRegInfo QueryWmiRegInfo{};
    WmiQueryDataBlock QueryWmiDataBlock{};
    WmiSetDataBlock SetWmiDataBlock{};
    WmiSetDataItem SetWmiDataItem{};
    WmiExecuteMethod ExecuteWmiMethod{};
    WmiFunctionControl WmiFunctionControl{};
};

struct alignas(8) WNODE_HEADER {
    uint32_t BufferSize{};
    uint32_t ProviderId{};
    uint64_t HistoricalContext{};
    int64_t TimeStamp{};
    GUID Guid{};
    uint32_t ClientContext{};
    uint32_t Flags{};
};
struct WNODE_SINGLE_INSTANCE {
    WNODE_HEADER WnodeHeader{};
    uint32_t OffsetInstanceName{};
    uint32_t InstanceIndex{};
    uint32_t DataBlockOffset{};
    uint32_t SizeDataBlock{};
};
struct WNODE_ALL_DATA {
    WNODE_HEADER WnodeHeader{};
    uint32_t DataBlockOffset{};
    uint32_t InstanceCount{};
    uint32_t OffsetInstanceNameOffsets{};
    uint32_t FixedInstanceSize{};
};
struct WNODE_TOO_SMALL {
    WNODE_HEADER WnodeHeader{};
    uint32_t SizeNeeded{};
};
struct WMIREGGUID {
    GUID Guid{};
    uint32_t Flags{};
    uint32_t InstanceCount{};
    uintptr_t InstanceInfo{};
};
struct WMIREGINFO_HEADER {
    uint32_t BufferSize{};
    uint32_t NextWmiRegInfo{};
    uint32_t RegistryPath{};
    uint32_t MofResourceName{};
    uint32_t GuidCount{};
};

inline constexpr uint32_t IRP_MN_QUERY_ALL_DATA = 0x00;
inline constexpr uint32_t IRP_MN_QUERY_SINGLE_INSTANCE = 0x01;
inline constexpr uint32_t IRP_MN_CHANGE_SINGLE_INSTANCE = 0x02;
inline constexpr uint32_t IRP_MN_CHANGE_SINGLE_ITEM = 0x03;
inline constexpr uint32_t IRP_MN_ENABLE_EVENTS = 0x04;
inline constexpr uint32_t IRP_MN_DISABLE_EVENTS = 0x05;
inline constexpr uint32_t IRP_MN_ENABLE_COLLECTION = 0x06;
inline constexpr uint32_t IRP_MN_DISABLE_COLLECTION = 0x07;
inline constexpr uint32_t IRP_MN_REGINFO = 0x08;
inline constexpr uint32_t IRP_MN_EXECUTE_METHOD = 0x09;
inline constexpr uint32_t IRP_MN_REGINFO_EX = 0x0B;
inline constexpr uint32_t WNODE_FLAG_ALL_DATA = 0x00000001;
inline constexpr uint32_t WNODE_FLAG_SINGLE_INSTANCE = 0x00000002;
inline constexpr uint32_t WNODE_FLAG_STATIC_INSTANCE_NAMES = 0x00000080;
inline constexpr uint32_t WNODE_FLAG_PDO_INSTANCE_NAMES = 0x00010000;
inline constexpr uint32_t WMIREG_FLAG_INSTANCE_PDO = 0x00000020;

static_assert(sizeof(WNODE_HEADER) == 48);
static_assert(sizeof(WNODE_SINGLE_INSTANCE) == 64);
static_assert(sizeof(WNODE_ALL_DATA) == 64);
static_assert(sizeof(WNODE_TOO_SMALL) == 56);

inline NtStatus NTAPI WmiSystemControl(const WMILIB_CONTEXT* context, io::DeviceObject* device,
    io::Irp* irp, SysctlIrpDisposition* disposition) noexcept {
    if (!context || !device || !irp || !disposition) return NtStatus::InvalidParameter;
    *disposition = SysctlIrpDisposition::NotWmi;
    if (irp->majorFunction != io::IRP_MJ_SYSTEM_CONTROL) return NtStatus::Success;

    auto* stack = irp->getCurrentStack();
    if (!stack) return NtStatus::InvalidParameter;
    if (stack->parameters.wmi.providerId != reinterpret_cast<uintptr_t>(device)) {
        *disposition = SysctlIrpDisposition::Forward;
        return NtStatus::Success;
    }
    *disposition = SysctlIrpDisposition::Processed;
    auto finish = [irp](NtStatus status, uint32_t information = 0) {
        irp->ioStatus.status = status;
        irp->ioStatus.information = information;
        return status;
    };
    const uint32_t minor = stack->minorFunction;
    auto* guid = static_cast<const GUID*>(stack->parameters.wmi.dataPath);
    auto* buffer = static_cast<uint8_t*>(stack->parameters.wmi.buffer);
    const uint32_t bufferSize = stack->parameters.wmi.bufferSize;
    if (minor == IRP_MN_REGINFO || minor == IRP_MN_REGINFO_EX) {
        if (!context->QueryWmiRegInfo || !context->GuidList || !buffer)
            return finish(NtStatus::InvalidDeviceRequest);
        uint32_t registrationFlags = 0;
        UNICODE_STRING instanceName{};
        UNICODE_STRING* registryPath = nullptr;
        UNICODE_STRING mofResourceName{};
        io::DeviceObject* pdo = nullptr;
        NtStatus status = context->QueryWmiRegInfo(device, &registrationFlags,
            &instanceName, &registryPath, &mofResourceName, &pdo);
        if (!NT_SUCCESS(status)) return finish(status);

        const size_t entriesOffset = (sizeof(WMIREGINFO_HEADER) + alignof(WMIREGGUID) - 1) &
            ~(alignof(WMIREGGUID) - 1);
        const size_t entriesSize = static_cast<size_t>(context->GuidCount) * sizeof(WMIREGGUID);
        size_t totalSize = entriesOffset + entriesSize;
        const auto countedStringSize = [](const UNICODE_STRING* value) -> size_t {
            return value && value->buffer && value->length
                ? sizeof(uint16_t) * 2 + value->length : 0;
        };
        const size_t registryOffset = countedStringSize(registryPath) ? totalSize : 0;
        totalSize += countedStringSize(registryPath);
        const size_t mofOffset = countedStringSize(&mofResourceName) ? totalSize : 0;
        totalSize += countedStringSize(&mofResourceName);
        if (totalSize > UINT32_MAX) return finish(NtStatus::BufferTooSmall);
        if (bufferSize < totalSize) return finish(NtStatus::BufferTooSmall,
            static_cast<uint32_t>(totalSize));

        std::memset(buffer, 0, totalSize);
        auto* regInfo = reinterpret_cast<WMIREGINFO_HEADER*>(buffer);
        regInfo->BufferSize = static_cast<uint32_t>(totalSize);
        regInfo->RegistryPath = static_cast<uint32_t>(registryOffset);
        regInfo->MofResourceName = static_cast<uint32_t>(mofOffset);
        regInfo->GuidCount = context->GuidCount;
        auto* entries = reinterpret_cast<WMIREGGUID*>(buffer + entriesOffset);
        for (uint32_t index = 0; index < context->GuidCount; ++index) {
            if (!context->GuidList[index].Guid) return finish(NtStatus::InvalidParameter);
            entries[index].Guid = *context->GuidList[index].Guid;
            entries[index].Flags = registrationFlags | context->GuidList[index].Flags;
            entries[index].InstanceCount = context->GuidList[index].InstanceCount;
            if (entries[index].Flags & WMIREG_FLAG_INSTANCE_PDO)
                entries[index].InstanceInfo = reinterpret_cast<uintptr_t>(pdo);
        }
        const auto writeCountedString = [buffer](size_t offset, const UNICODE_STRING* value) {
            if (!offset || !value) return;
            std::memcpy(buffer + offset, &value->length, sizeof(value->length));
            std::memcpy(buffer + offset + sizeof(uint16_t), &value->maximumLength,
                sizeof(value->maximumLength));
            if (value->length)
                std::memcpy(buffer + offset + 2 * sizeof(uint16_t), value->buffer, value->length);
        };
        writeCountedString(registryOffset, registryPath);
        writeCountedString(mofOffset, &mofResourceName);
        return finish(NtStatus::Success, static_cast<uint32_t>(totalSize));
    }
    if (!guid || !context->GuidList || !context->QueryWmiDataBlock)
        return finish(NtStatus::InvalidParameter);
    uint32_t guidIndex = context->GuidCount;
    for (uint32_t index = 0; index < context->GuidCount; ++index) {
        const auto* registered = context->GuidList[index].Guid;
        if (registered && std::memcmp(registered, guid, sizeof(GUID)) == 0) {
            guidIndex = index;
            break;
        }
    }
    if (guidIndex == context->GuidCount) return finish(NtStatus::WmiGuidNotFound);

    if (!buffer) return finish(NtStatus::InvalidParameter);
    uint32_t instanceLength = 0;
    NtStatus status = NtStatus::InvalidDeviceRequest;

    if (minor == IRP_MN_QUERY_SINGLE_INSTANCE) {
        if (bufferSize < sizeof(WNODE_SINGLE_INSTANCE)) return finish(NtStatus::BufferTooSmall);
        auto* wnode = reinterpret_cast<WNODE_SINGLE_INSTANCE*>(buffer);
        const uint32_t instanceIndex = wnode->InstanceIndex;
        const bool staticNames = (wnode->WnodeHeader.Flags & WNODE_FLAG_STATIC_INSTANCE_NAMES) != 0;
        if (staticNames && instanceIndex >= context->GuidList[guidIndex].InstanceCount)
            return finish(NtStatus::WmiInstanceNotFound);
        const uint32_t dataOffset = wnode->DataBlockOffset;
        if (dataOffset < sizeof(WNODE_SINGLE_INSTANCE) || dataOffset > bufferSize)
            return finish(NtStatus::InvalidParameter);
        const uint32_t available = bufferSize - dataOffset;
        status = context->QueryWmiDataBlock(device, irp, guidIndex, instanceIndex, 1,
            &instanceLength, available, buffer + dataOffset);
        if (status == NtStatus::BufferTooSmall) {
            if (bufferSize < sizeof(WNODE_TOO_SMALL)) return finish(status);
            auto* tooSmall = reinterpret_cast<WNODE_TOO_SMALL*>(buffer);
            tooSmall->WnodeHeader.BufferSize = sizeof(WNODE_TOO_SMALL);
            tooSmall->WnodeHeader.Flags = 0;
            tooSmall->SizeNeeded = dataOffset + instanceLength;
            return finish(NtStatus::Success, sizeof(WNODE_TOO_SMALL));
        }
        if (!NT_SUCCESS(status)) return finish(status);
        wnode->SizeDataBlock = instanceLength;
        wnode->WnodeHeader.BufferSize = dataOffset + instanceLength;
        return finish(NtStatus::Success, wnode->WnodeHeader.BufferSize);
    }

    if (minor == IRP_MN_QUERY_ALL_DATA) {
        if (bufferSize < sizeof(WNODE_ALL_DATA)) return finish(NtStatus::BufferTooSmall);
        auto* wnode = reinterpret_cast<WNODE_ALL_DATA*>(buffer);
        const uint32_t requestedCount = wnode->InstanceCount;
        if (requestedCount == 0 || requestedCount > context->GuidList[guidIndex].InstanceCount)
            return finish(NtStatus::WmiInstanceNotFound);
        const uint32_t dataOffset = wnode->DataBlockOffset;
        if (dataOffset < sizeof(WNODE_ALL_DATA) || dataOffset > bufferSize)
            return finish(NtStatus::InvalidParameter);
        status = context->QueryWmiDataBlock(device, irp, guidIndex, 0, requestedCount,
            &instanceLength, bufferSize - dataOffset, buffer + dataOffset);
        if (status == NtStatus::BufferTooSmall) {
            if (bufferSize < sizeof(WNODE_TOO_SMALL)) return finish(status);
            auto* tooSmall = reinterpret_cast<WNODE_TOO_SMALL*>(buffer);
            tooSmall->WnodeHeader.BufferSize = sizeof(WNODE_TOO_SMALL);
            tooSmall->WnodeHeader.Flags = 0;
            tooSmall->SizeNeeded = dataOffset + instanceLength;
            return finish(NtStatus::Success, sizeof(WNODE_TOO_SMALL));
        }
        if (!NT_SUCCESS(status)) return finish(status);
        wnode->FixedInstanceSize = instanceLength;
        wnode->InstanceCount = requestedCount;
        wnode->WnodeHeader.BufferSize = dataOffset + instanceLength * requestedCount;
        return finish(NtStatus::Success, wnode->WnodeHeader.BufferSize);
    }

    if (minor == IRP_MN_ENABLE_EVENTS || minor == IRP_MN_DISABLE_EVENTS ||
        minor == IRP_MN_ENABLE_COLLECTION || minor == IRP_MN_DISABLE_COLLECTION) {
        if (!context->WmiFunctionControl) return finish(NtStatus::Success);
        const bool enable = minor == IRP_MN_ENABLE_EVENTS || minor == IRP_MN_ENABLE_COLLECTION;
        const auto control = (minor == IRP_MN_ENABLE_EVENTS || minor == IRP_MN_DISABLE_EVENTS)
            ? WMIEnableDisableControl::Event : WMIEnableDisableControl::DataBlock;
        return finish(context->WmiFunctionControl(device, irp, guidIndex, control, enable));
    }
    if (minor == IRP_MN_CHANGE_SINGLE_INSTANCE && context->SetWmiDataBlock)
        return finish(NtStatus::NotSupported);
    if (minor == IRP_MN_CHANGE_SINGLE_ITEM && context->SetWmiDataItem)
        return finish(NtStatus::NotSupported);
    if (minor == IRP_MN_EXECUTE_METHOD && context->ExecuteWmiMethod)
        return finish(NtStatus::NotSupported);
    return finish(NtStatus::InvalidDeviceRequest);
}

} // namespace micant::km::wmilib
