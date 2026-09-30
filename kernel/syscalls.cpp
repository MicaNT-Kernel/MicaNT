#include "micant/syscalls.hpp"
#include "micant/ob.hpp"
#include "micant/mm.hpp"
#include <iostream>

namespace micant::sys {

// Global simulated kernel state
static ob::HandleTable g_KernelHandleTable;
static mm::ProcessAddressSpace g_KernelAddressSpace;

NtStatus NtAllocateVirtualMemory(
    Handle processHandle,
    uintptr_t* baseAddress,
    uintptr_t /*zeroBits*/,
    size_t* regionSize,
    uint32_t allocationType,
    uint32_t protect
) {
    if (!baseAddress || !regionSize || *regionSize == 0) {
        return NtStatus::InvalidParameter;
    }

    uintptr_t addr = *baseAddress;
    NtStatus status = g_KernelAddressSpace.allocate(addr, *regionSize, allocationType, protect);
    if (NT_SUCCESS(status)) {
        *baseAddress = addr;
    }
    return status;
}

NtStatus NtFreeVirtualMemory(
    Handle processHandle,
    uintptr_t* baseAddress,
    size_t* regionSize,
    uint32_t freeType
) {
    if (!baseAddress || *baseAddress == 0) {
        return NtStatus::InvalidParameter;
    }

    size_t sz = regionSize ? *regionSize : 0;
    return g_KernelAddressSpace.free(*baseAddress, sz, freeType);
}

NtStatus NtProtectVirtualMemory(
    Handle processHandle,
    uintptr_t* baseAddress,
    size_t* regionSize,
    uint32_t newProtect,
    uint32_t* oldProtect
) {
    if (!baseAddress || !regionSize) {
        return NtStatus::InvalidParameter;
    }
    if (oldProtect) {
        *oldProtect = mm::PAGE_READWRITE;
    }
    return NtStatus::Success;
}

NtStatus NtClose(Handle handle) {
    if (handle == 0 || handle == InvalidHandleValue) {
        return NtStatus::InvalidHandle;
    }
    return g_KernelHandleTable.closeHandle(handle);
}

NtStatus NtTerminateProcess(Handle processHandle, NtStatus exitStatus) {
    std::cout << "[MicaNT Executive] NtTerminateProcess called (PID handle: " 
              << processHandle << ", exit status: " << static_cast<uint32_t>(exitStatus) << ")\n";
    return NtStatus::Success;
}

NtStatus NtWaitForSingleObject(Handle handle, bool alertable, LargeInteger* timeout) {
    if (handle == 0) return NtStatus::InvalidHandle;
    return NtStatus::Success;
}

NtStatus NtQuerySystemInformation(
    uint32_t systemInformationClass,
    void* systemInformation,
    uint32_t systemInformationLength,
    uint32_t* returnLength
) {
    if (!systemInformation && systemInformationLength > 0) {
        return NtStatus::AccessViolation;
    }
    if (returnLength) {
        *returnLength = 0;
    }
    return NtStatus::Success;
}

} // namespace micant::sys
