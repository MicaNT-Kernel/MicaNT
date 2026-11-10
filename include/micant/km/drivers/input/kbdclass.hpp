#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "micant/km/drivers/input/kbdclass_contract.hpp"
#include "micant/km/executive/io.hpp"
#include "micant/km/core/ke.hpp"
#include "micant/um/native/janusldr.hpp"

namespace micant::km::hid {

class KbdClassDriver {
public:
    static KbdClassDriver& Instance() { static KbdClassDriver instance; return instance; }

    NtStatus dispatch(io::DeviceObject*, io::Irp* irp) {
        if (!irp) return NtStatus::InvalidParameter;
        switch (irp->majorFunction) {
        case io::IRP_MJ_CREATE: return finish(irp, NtStatus::Success, 1);
        case io::IRP_MJ_CLOSE: case io::IRP_MJ_CLEANUP: case io::IRP_MJ_FLUSH_BUFFERS:
            return finish(irp, NtStatus::Success);
        case io::IRP_MJ_READ: return read(irp);
        case io::IRP_MJ_DEVICE_CONTROL: return deviceControl(irp);
        default: return finish(irp, NtStatus::InvalidDeviceRequest);
        }
    }

    void service(KEYBOARD_INPUT_DATA* start, KEYBOARD_INPUT_DATA* end, uint32_t* consumed) {
        if (consumed) *consumed = 0;
        if (!start || !end || end < start) return;
        std::array<io::Irp*, PendingCapacity> ready{};
        size_t completed = 0;
        uint32_t count = 0;
        {
            ke::SpinLockGuard guard(m_lock);
            for (auto* packet = start; packet < end; ++packet) {
                if (m_count == m_packets.size()) { m_head = (m_head + 1) % m_packets.size(); --m_count; }
                m_packets[(m_head + m_count) % m_packets.size()] = *packet;
                ++m_count; ++count;
            }
            m_totalPacketsQueued += count;
            while (m_pendingCount && completed < ready.size()) {
                auto* irp = m_pending[m_pendingHead];
                if (readQueued(irp) == NtStatus::Pending) break;
                ready[completed++] = irp;
                m_pendingHead = (m_pendingHead + 1) % m_pending.size();
                --m_pendingCount;
            }
        }
        if (consumed) *consumed = count;
        for (size_t i = 0; i < completed; ++i) (void)io::IoCompleteRequest(ready[i]);
    }

    bool pop(KEYBOARD_INPUT_DATA& data) {
        ke::SpinLockGuard guard(m_lock);
        if (!m_count) return false;
        data = m_packets[m_head]; m_head = (m_head + 1) % m_packets.size(); --m_count; return true;
    }
    size_t depth() { ke::SpinLockGuard guard(m_lock); return m_count; }
    uint64_t packetsQueuedTotal() { ke::SpinLockGuard guard(m_lock); return m_totalPacketsQueued; }
    void setClassDevice(io::DeviceObject* device) { ke::SpinLockGuard guard(m_lock); m_classDevice = device; }
    io::DeviceObject* classDevice() { ke::SpinLockGuard guard(m_lock); return m_classDevice; }

    NtStatus queryAttributes(void* output, size_t length) const {
        if (!output || length < sizeof(m_attributes)) return NtStatus::BufferTooSmall;
        std::memcpy(output, &m_attributes, sizeof(m_attributes)); return NtStatus::Success;
    }
    NtStatus control(uint32_t code, void* buffer, size_t inputLength, size_t outputLength) {
        ke::SpinLockGuard guard(m_lock);
        switch (code) {
        case IOCTL_KEYBOARD_QUERY_ATTRIBUTES:
            if (!buffer || outputLength < sizeof(m_attributes)) return NtStatus::BufferTooSmall;
            std::memcpy(buffer, &m_attributes, sizeof(m_attributes)); return NtStatus::Success;
        case IOCTL_KEYBOARD_QUERY_INDICATORS:
            if (!buffer || outputLength < sizeof(m_indicators)) return NtStatus::BufferTooSmall;
            std::memcpy(buffer, &m_indicators, sizeof(m_indicators)); return NtStatus::Success;
        case IOCTL_KEYBOARD_SET_INDICATORS:
            if (!buffer || inputLength < sizeof(m_indicators)) return NtStatus::InvalidParameter;
            std::memcpy(&m_indicators, buffer, sizeof(m_indicators)); return NtStatus::Success;
        case IOCTL_KEYBOARD_QUERY_TYPEMATIC:
            if (!buffer || outputLength < sizeof(m_typematic)) return NtStatus::BufferTooSmall;
            std::memcpy(buffer, &m_typematic, sizeof(m_typematic)); return NtStatus::Success;
        case IOCTL_KEYBOARD_SET_TYPEMATIC:
            if (!buffer || inputLength < sizeof(m_typematic)) return NtStatus::InvalidParameter;
            std::memcpy(&m_typematic, buffer, sizeof(m_typematic)); return NtStatus::Success;
        default: return NtStatus::InvalidDeviceRequest;
        }
    }
    void reset() {
        ke::SpinLockGuard guard(m_lock); m_packets.fill({}); m_head = m_count = 0;
        m_pending.fill(nullptr); m_pendingHead = m_pendingCount = 0; m_totalPacketsQueued = 0;
        m_indicators = {0, KEYBOARD_NUM_LOCK_ON}; m_typematic = {0, 30, 250};
    }

private:
    static constexpr size_t PendingCapacity = 64;
    static NtStatus finish(io::Irp* irp, NtStatus status, uintptr_t info = 0) {
        irp->ioStatus.status = status; irp->ioStatus.information = info; return status;
    }
    NtStatus readQueued(io::Irp* irp) {
        if (irp->cancel) return finish(irp, NtStatus::Cancelled);
        if (!irp->length) return finish(irp, NtStatus::Success);
        void* buffer = irp->systemBuffer ? irp->systemBuffer : irp->userBuffer;
        if (!buffer) return finish(irp, NtStatus::InvalidParameter);
        size_t capacity = irp->length / sizeof(KEYBOARD_INPUT_DATA);
        if (!capacity) return finish(irp, NtStatus::BufferTooSmall);
        auto* output = static_cast<KEYBOARD_INPUT_DATA*>(buffer);
        size_t n = 0;
        while (n < capacity && m_count) {
            output[n++] = m_packets[m_head]; m_head = (m_head + 1) % m_packets.size(); --m_count;
        }
        return n ? finish(irp, NtStatus::Success, n * sizeof(KEYBOARD_INPUT_DATA))
                 : finish(irp, NtStatus::Pending);
    }
    NtStatus read(io::Irp* irp) {
        ke::SpinLockGuard guard(m_lock);
        auto status = readQueued(irp);
        if (status != NtStatus::Pending) return status;
        if (m_pendingCount == m_pending.size()) return finish(irp, NtStatus::InsufficientResources);
        m_pending[(m_pendingHead + m_pendingCount) % m_pending.size()] = irp; ++m_pendingCount;
        return finish(irp, NtStatus::Pending);
    }
    NtStatus deviceControl(io::Irp* irp) {
        auto* stack = irp->getCurrentStack();
        uint32_t code = stack ? stack->parameters.deviceIoControl.ioControlCode : 0;
        if (!code) code = irp->byteOffset.lowPart;
        void* buffer = irp->systemBuffer ? irp->systemBuffer : irp->userBuffer;
        size_t inLength = stack ? stack->parameters.deviceIoControl.inputBufferLength : 0;
        size_t outLength = stack ? stack->parameters.deviceIoControl.outputBufferLength : irp->length;
        auto status = control(code, buffer, inLength, outLength);
        size_t info = NT_SUCCESS(status) ? ((code == IOCTL_KEYBOARD_QUERY_ATTRIBUTES) ? sizeof(m_attributes) :
            ((code == IOCTL_KEYBOARD_QUERY_INDICATORS) ? sizeof(m_indicators) : sizeof(m_typematic))) : 0;
        return finish(irp, status, info);
    }

    ke::SpinLock m_lock{};
    std::array<KEYBOARD_INPUT_DATA, 128> m_packets{};
    size_t m_head{0}, m_count{0};
    uint64_t m_totalPacketsQueued{0};
    io::DeviceObject* m_classDevice{nullptr};
    std::array<io::Irp*, PendingCapacity> m_pending{};
    size_t m_pendingHead{0}, m_pendingCount{0};
    KEYBOARD_ATTRIBUTES m_attributes{};
    KEYBOARD_INDICATOR_PARAMETERS m_indicators{0, KEYBOARD_NUM_LOCK_ON};
    KEYBOARD_TYPEMATIC_PARAMETERS m_typematic{0, 30, 250};
};

inline void WINAPI KeyboardClassServiceCallback(io::DeviceObject* deviceObject,
    KEYBOARD_INPUT_DATA* start, KEYBOARD_INPUT_DATA* end, uint32_t* consumed) {
    (void)deviceObject; KbdClassDriver::Instance().service(start, end, consumed);
}
inline void InitializeKbdClassDriver(io::DriverObject& driver) {
    driver.driverName = L"\\Driver\\Kbdclass";
    for (uint8_t major : {io::IRP_MJ_CREATE, io::IRP_MJ_CLOSE, io::IRP_MJ_CLEANUP,
            io::IRP_MJ_FLUSH_BUFFERS, io::IRP_MJ_READ, io::IRP_MJ_DEVICE_CONTROL}) {
        driver.setDispatch(major, [](io::DeviceObject* dev, io::Irp* irp) {
            return KbdClassDriver::Instance().dispatch(dev, irp);
        });
    }
}
inline void InitializeKbdClassExports() {
    ldr::DynamicLoader::get().registerExport("kbdclass.sys", "KeyboardClassServiceCallback",
        reinterpret_cast<void*>(KeyboardClassServiceCallback));
}
inline bool KbdClassPopInputData(KEYBOARD_INPUT_DATA& data) { return KbdClassDriver::Instance().pop(data); }
inline size_t KbdClassQueueDepth() { return KbdClassDriver::Instance().depth(); }
inline uint64_t KbdClassPacketsQueuedTotal() { return KbdClassDriver::Instance().packetsQueuedTotal(); }
inline void KbdClassSetDeviceObject(io::DeviceObject* device) { KbdClassDriver::Instance().setClassDevice(device); }
inline void KbdClassResetState() { KbdClassDriver::Instance().reset(); }
inline NtStatus KbdClassQueryAttributes(void* output, size_t length) {
    return KbdClassDriver::Instance().queryAttributes(output, length);
}
inline NtStatus KbdClassDeviceControl(uint32_t code, void* buffer, size_t inLength, size_t outLength) {
    return KbdClassDriver::Instance().control(code, buffer, inLength, outLength);
}

} // namespace micant::km::hid
