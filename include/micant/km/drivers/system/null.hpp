#pragma once

#include <cstring>

#include "micant/km/executive/io.hpp"

namespace micant::null_driver {

inline NtStatus Dispatch(io::DeviceObject*, io::Irp* irp) {
    if (!irp) return NtStatus::InvalidParameter;

    auto complete = [irp](NtStatus status, uintptr_t information = 0) {
        irp->ioStatus.status = status;
        irp->ioStatus.information = information;
        return status;
    };

    switch (irp->majorFunction) {
        case io::IRP_MJ_CREATE:
            return complete(NtStatus::Success, 1); // FILE_OPENED
        case io::IRP_MJ_CLOSE:
        case io::IRP_MJ_CLEANUP:
        case io::IRP_MJ_FLUSH_BUFFERS:
            return complete(NtStatus::Success);
        case io::IRP_MJ_READ:
            if (irp->length != 0 && !irp->userBuffer) return complete(NtStatus::InvalidParameter);
            if (irp->length != 0) std::memset(irp->userBuffer, 0, irp->length);
            return complete(NtStatus::Success, irp->length);
        case io::IRP_MJ_WRITE:
            if (irp->length != 0 && !irp->userBuffer) return complete(NtStatus::InvalidParameter);
            return complete(NtStatus::Success, irp->length);
        default:
            return complete(NtStatus::InvalidDeviceRequest);
    }
}

inline void Initialize(io::DriverObject& driver) {
    driver.driverName = L"\\Driver\\Null";
    driver.setDispatch(io::IRP_MJ_CREATE, Dispatch);
    driver.setDispatch(io::IRP_MJ_CLOSE, Dispatch);
    driver.setDispatch(io::IRP_MJ_CLEANUP, Dispatch);
    driver.setDispatch(io::IRP_MJ_READ, Dispatch);
    driver.setDispatch(io::IRP_MJ_WRITE, Dispatch);
    driver.setDispatch(io::IRP_MJ_FLUSH_BUFFERS, Dispatch);
}

} // namespace micant::null_driver
