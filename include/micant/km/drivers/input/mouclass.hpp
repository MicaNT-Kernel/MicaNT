#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "micant/km/drivers/input/mouclass_contract.hpp"
#include "micant/km/executive/io.hpp"
#include "micant/km/core/ke.hpp"
#include "micant/um/native/janusldr.hpp"

namespace micant::km::hid {

// The mouse class driver owns the class-device IRP contract and the bounded
// queue of pending reads. The HID/port stack supplies packets through the
// Windows MouseClassServiceCallback contract below.
class MouClassDriver {
public:
    static MouClassDriver& Instance() {
        static MouClassDriver instance;
        return instance;
    }

    NtStatus dispatch(io::DeviceObject*, io::Irp* irp) {
        if (!irp) return NtStatus::InvalidParameter;

        switch (irp->majorFunction) {
            case io::IRP_MJ_CREATE:
                return complete(irp, NtStatus::Success, 1); // FILE_OPENED
            case io::IRP_MJ_CLOSE:
            case io::IRP_MJ_CLEANUP:
            case io::IRP_MJ_FLUSH_BUFFERS:
                return complete(irp, NtStatus::Success);
            case io::IRP_MJ_READ:
                return dispatchRead(irp);
            case io::IRP_MJ_DEVICE_CONTROL:
                return dispatchDeviceControl(irp);
            default:
                return complete(irp, NtStatus::InvalidDeviceRequest);
        }
    }

    bool popInputData(MOUSE_INPUT_DATA& data) {
        ke::SpinLockGuard lock(m_lock);
        if (m_queueCount == 0) return false;
        data = m_queue[m_queueHead];
        m_queueHead = (m_queueHead + 1) % m_queue.size();
        --m_queueCount;
        return true;
    }

    size_t queueDepth() {
        ke::SpinLockGuard lock(m_lock);
        return m_queueCount;
    }

    uint64_t packetsQueuedTotal() {
        ke::SpinLockGuard lock(m_lock);
        return m_totalPacketsQueued;
    }

    NtStatus queryAttributes(void* outputBuffer, size_t outputLength) const {
        if (!outputBuffer || outputLength < sizeof(MOUSE_ATTRIBUTES)) return NtStatus::BufferTooSmall;
        std::memcpy(outputBuffer, &m_attributes, sizeof(m_attributes));
        return NtStatus::Success;
    }

    void reset() {
        ke::SpinLockGuard lock(m_lock);
        m_queue.fill(MOUSE_INPUT_DATA{});
        m_queueHead = 0;
        m_queueCount = 0;
        m_totalPacketsQueued = 0;
        m_pendingReads.fill(nullptr);
        m_pendingHead = 0;
        m_pendingCount = 0;
    }

    void service(MOUSE_INPUT_DATA* start, MOUSE_INPUT_DATA* end, uint32_t* consumed) {
        if (consumed) *consumed = 0;
        if (!start || !end || end < start) return;

        std::array<io::Irp*, PendingReadCapacity> ready{};
        size_t readyCount = 0;
        uint32_t accepted = 0;
        {
            ke::SpinLockGuard lock(m_lock);
            for (MOUSE_INPUT_DATA* packet = start; packet < end; ++packet) {
                if (m_queueCount == m_queue.size()) {
                    m_queueHead = (m_queueHead + 1) % m_queue.size();
                    --m_queueCount;
                }
                const size_t tail = (m_queueHead + m_queueCount) % m_queue.size();
                m_queue[tail] = *packet;
                ++m_queueCount;
                ++accepted;
            }
            m_totalPacketsQueued += accepted;

            while (m_pendingCount != 0 && readyCount < ready.size()) {
                io::Irp* irp = m_pendingReads[m_pendingHead];
                const NtStatus status = readIntoIrp(irp);
                if (status == NtStatus::Pending) break;
                m_pendingHead = (m_pendingHead + 1) % m_pendingReads.size();
                --m_pendingCount;
                ready[readyCount++] = irp;
            }
        }

        if (consumed) *consumed = accepted;
        for (size_t index = 0; index < readyCount; ++index) {
            (void)io::IoCompleteRequest(ready[index]);
        }
    }

private:
    static constexpr size_t PendingReadCapacity = 64;

    static NtStatus complete(io::Irp* irp, NtStatus status, uintptr_t information = 0) {
        irp->ioStatus.status = status;
        irp->ioStatus.information = information;
        return status;
    }

    NtStatus dispatchRead(io::Irp* irp) {
        NtStatus status;
        {
            ke::SpinLockGuard lock(m_lock);
            status = readIntoIrp(irp);
            if (status == NtStatus::Pending) {
                if (m_pendingCount == m_pendingReads.size()) {
                    return complete(irp, NtStatus::InsufficientResources);
                }
                const size_t tail = (m_pendingHead + m_pendingCount) % m_pendingReads.size();
                m_pendingReads[tail] = irp;
                ++m_pendingCount;
                return complete(irp, NtStatus::Pending);
            }
        }
        return status;
    }

    NtStatus readIntoIrp(io::Irp* irp) {
        if (irp->cancel) return complete(irp, NtStatus::Cancelled);
        if (irp->length == 0) return complete(irp, NtStatus::Success);

        void* buffer = irp->systemBuffer ? irp->systemBuffer : irp->userBuffer;
        if (!buffer) return complete(irp, NtStatus::InvalidParameter);
        const size_t packetCapacity = irp->length / sizeof(MOUSE_INPUT_DATA);
        if (packetCapacity == 0) return complete(irp, NtStatus::BufferTooSmall);

        auto* packets = static_cast<MOUSE_INPUT_DATA*>(buffer);
        size_t packetCount = 0;
        while (packetCount < packetCapacity && m_queueCount != 0) {
            packets[packetCount++] = m_queue[m_queueHead];
            m_queueHead = (m_queueHead + 1) % m_queue.size();
            --m_queueCount;
        }
        if (packetCount == 0) return complete(irp, NtStatus::Pending);
        return complete(irp, NtStatus::Success, packetCount * sizeof(MOUSE_INPUT_DATA));
    }

    NtStatus dispatchDeviceControl(io::Irp* irp) {
        const auto* stack = irp->getCurrentStack();
        uint32_t code = stack ? stack->parameters.deviceIoControl.ioControlCode : 0;
        if (code == 0) code = irp->byteOffset.lowPart;
        if (code != IOCTL_MOUSE_QUERY_ATTRIBUTES) return complete(irp, NtStatus::InvalidDeviceRequest);

        void* output = irp->systemBuffer ? irp->systemBuffer : irp->userBuffer;
        const uint32_t inputLength = stack && stack->parameters.deviceIoControl.inputBufferLength != 0
            ? stack->parameters.deviceIoControl.inputBufferLength
            : static_cast<uint32_t>(irp->byteOffset.highPart);
        const uint32_t outputLength = stack && stack->parameters.deviceIoControl.outputBufferLength != 0
            ? stack->parameters.deviceIoControl.outputBufferLength : irp->length;
        if (inputLength != 0 && inputLength < sizeof(MOUSE_UNIT_ID_PARAMETER))
            return complete(irp, NtStatus::InvalidParameter);
        if (inputLength > 0 && !output) return complete(irp, NtStatus::InvalidParameter);
        if (inputLength >= sizeof(MOUSE_UNIT_ID_PARAMETER)) {
            MOUSE_UNIT_ID_PARAMETER unit{};
            std::memcpy(&unit, output, sizeof(unit));
            if (unit.UnitId != 0) return complete(irp, NtStatus::NoSuchDevice);
        }
        const NtStatus status = queryAttributes(output, outputLength);
        return complete(irp, status, NT_SUCCESS(status) ? sizeof(MOUSE_ATTRIBUTES) : 0);
    }

    ke::SpinLock m_lock{};
    std::array<MOUSE_INPUT_DATA, 256> m_queue{};
    size_t m_queueHead{0};
    size_t m_queueCount{0};
    uint64_t m_totalPacketsQueued{0};
    MOUSE_ATTRIBUTES m_attributes{};
    std::array<io::Irp*, PendingReadCapacity> m_pendingReads{};
    size_t m_pendingHead{0};
    size_t m_pendingCount{0};
};

inline void WINAPI MouseClassServiceCallback(
    io::DeviceObject* DeviceObject,
    MOUSE_INPUT_DATA* InputDataStart,
    MOUSE_INPUT_DATA* InputDataEnd,
    uint32_t* InputDataConsumed
) {
    (void)DeviceObject;
    MouClassDriver::Instance().service(InputDataStart, InputDataEnd, InputDataConsumed);
}

inline void InitializeMouClassDriver(io::DriverObject& driver) {
    driver.driverName = L"\\Driver\\Mouclass";
    driver.setDispatch(io::IRP_MJ_CREATE, [](io::DeviceObject* dev, io::Irp* irp) {
        return MouClassDriver::Instance().dispatch(dev, irp);
    });
    driver.setDispatch(io::IRP_MJ_CLOSE, [](io::DeviceObject* dev, io::Irp* irp) {
        return MouClassDriver::Instance().dispatch(dev, irp);
    });
    driver.setDispatch(io::IRP_MJ_CLEANUP, [](io::DeviceObject* dev, io::Irp* irp) {
        return MouClassDriver::Instance().dispatch(dev, irp);
    });
    driver.setDispatch(io::IRP_MJ_FLUSH_BUFFERS, [](io::DeviceObject* dev, io::Irp* irp) {
        return MouClassDriver::Instance().dispatch(dev, irp);
    });
    driver.setDispatch(io::IRP_MJ_READ, [](io::DeviceObject* dev, io::Irp* irp) {
        return MouClassDriver::Instance().dispatch(dev, irp);
    });
    driver.setDispatch(io::IRP_MJ_DEVICE_CONTROL, [](io::DeviceObject* dev, io::Irp* irp) {
        return MouClassDriver::Instance().dispatch(dev, irp);
    });
}

inline void InitializeMouClassExports() {
    auto& loader = ldr::DynamicLoader::get();
    loader.registerExport("mouclass.sys", "MouseClassServiceCallback",
                          reinterpret_cast<void*>(MouseClassServiceCallback));
}

inline bool MouClassPopInputData(MOUSE_INPUT_DATA& data) {
    return MouClassDriver::Instance().popInputData(data);
}

inline size_t MouClassQueueDepth() {
    return MouClassDriver::Instance().queueDepth();
}

inline NtStatus MouClassQueryAttributes(void* outputBuffer, size_t outputLength) {
    return MouClassDriver::Instance().queryAttributes(outputBuffer, outputLength);
}

inline void MouClassResetState() {
    MouClassDriver::Instance().reset();
}

inline uint64_t MouClassPacketsQueuedTotal() {
    return MouClassDriver::Instance().packetsQueuedTotal();
}

} // namespace micant::km::hid
