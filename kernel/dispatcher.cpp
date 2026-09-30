#include "micant/dispatcher.hpp"
#include <iostream>

namespace micant::sys {

void SyscallDispatcher::initializeStandardTable() {
    // 1. NtAllocateVirtualMemory (SSN: 0x0018)
    registerSyscall(SSN_NtAllocateVirtualMemory, "NtAllocateVirtualMemory", 6, [](const SyscallFrame& f) -> NtStatus {
        Handle proc = static_cast<Handle>(f.arg1);
        auto* baseAddr = reinterpret_cast<uintptr_t*>(f.arg2);
        uintptr_t zeroBits = f.arg3;
        auto* regSize = reinterpret_cast<size_t*>(f.arg4);

        uint32_t allocType = 0;
        uint32_t protect = 0;
        if (f.stackArgs && f.stackArgCount >= 2) {
            allocType = static_cast<uint32_t>(f.stackArgs[0]);
            protect = static_cast<uint32_t>(f.stackArgs[1]);
        }

        return NtAllocateVirtualMemory(proc, baseAddr, zeroBits, regSize, allocType, protect);
    });

    // 2. NtFreeVirtualMemory (SSN: 0x001E)
    registerSyscall(SSN_NtFreeVirtualMemory, "NtFreeVirtualMemory", 4, [](const SyscallFrame& f) -> NtStatus {
        Handle proc = static_cast<Handle>(f.arg1);
        auto* baseAddr = reinterpret_cast<uintptr_t*>(f.arg2);
        auto* regSize = reinterpret_cast<size_t*>(f.arg3);
        uint32_t freeType = static_cast<uint32_t>(f.arg4);
        return NtFreeVirtualMemory(proc, baseAddr, regSize, freeType);
    });

    // 3. NtClose (SSN: 0x000F)
    registerSyscall(SSN_NtClose, "NtClose", 1, [](const SyscallFrame& f) -> NtStatus {
        Handle h = static_cast<Handle>(f.arg1);
        return NtClose(h);
    });

    // 4. NtTerminateProcess (SSN: 0x002C)
    registerSyscall(SSN_NtTerminateProcess, "NtTerminateProcess", 2, [](const SyscallFrame& f) -> NtStatus {
        Handle proc = static_cast<Handle>(f.arg1);
        NtStatus exitStatus = static_cast<NtStatus>(f.arg2);
        return NtTerminateProcess(proc, exitStatus);
    });

    // 5. NtWaitForSingleObject (SSN: 0x0004)
    registerSyscall(SSN_NtWaitForSingleObject, "NtWaitForSingleObject", 3, [](const SyscallFrame& f) -> NtStatus {
        Handle h = static_cast<Handle>(f.arg1);
        bool alertable = f.arg2 != 0;
        auto* timeout = reinterpret_cast<LargeInteger*>(f.arg3);
        return NtWaitForSingleObject(h, alertable, timeout);
    });

    // 6. NtQuerySystemInformation (SSN: 0x0036)
    registerSyscall(SSN_NtQuerySystemInformation, "NtQuerySystemInformation", 4, [](const SyscallFrame& f) -> NtStatus {
        uint32_t infoClass = static_cast<uint32_t>(f.arg1);
        void* info = reinterpret_cast<void*>(f.arg2);
        uint32_t infoLen = static_cast<uint32_t>(f.arg3);
        auto* retLen = reinterpret_cast<uint32_t*>(f.arg4);
        return NtQuerySystemInformation(infoClass, info, infoLen, retLen);
    });
}

} // namespace micant::sys
