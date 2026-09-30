#include "micant/dispatcher.hpp"
#include "micant/sync.hpp"
#include "micant/io.hpp"
#include <iostream>

namespace micant::sys {

// Global simulated kernel sync & IOCP structures
static std::unordered_map<Handle, std::shared_ptr<sync::EventObject>> g_KernelEvents;
static std::unordered_map<Handle, std::shared_ptr<sync::MutantObject>> g_KernelMutants;
static std::unordered_map<Handle, std::shared_ptr<io::IoCompletionPort>> g_KernelIocpPorts;
static Handle g_NextSyncHandle = 0x100;

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
        g_KernelEvents.erase(h);
        g_KernelMutants.erase(h);
        g_KernelIocpPorts.erase(h);
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

        auto it = g_KernelEvents.find(h);
        if (it != g_KernelEvents.end()) {
            uint32_t ms = timeout ? static_cast<uint32_t>(timeout->quadPart / -10000) : 0xFFFFFFFF;
            bool signaled = it->second->wait(ms);
            return signaled ? NtStatus::Success : NtStatus::Timeout;
        }

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

    // 7. NtCreateEvent (SSN: 0x0048)
    registerSyscall(SSN_NtCreateEvent, "NtCreateEvent", 5, [](const SyscallFrame& f) -> NtStatus {
        auto* outHandle = reinterpret_cast<Handle*>(f.arg1);
        if (!outHandle) return NtStatus::InvalidParameter;

        sync::EventType evType = (f.arg3 == 0) ? sync::EventType::NotificationEvent : sync::EventType::SynchronizationEvent;
        bool initialState = (f.arg4 != 0);

        Handle h = g_NextSyncHandle;
        g_NextSyncHandle += 4;
        g_KernelEvents[h] = std::make_shared<sync::EventObject>(evType, initialState);
        *outHandle = h;
        return NtStatus::Success;
    });

    // 8. NtSetEvent (SSN: 0x004E)
    registerSyscall(SSN_NtSetEvent, "NtSetEvent", 2, [](const SyscallFrame& f) -> NtStatus {
        Handle h = static_cast<Handle>(f.arg1);
        auto it = g_KernelEvents.find(h);
        if (it == g_KernelEvents.end()) return NtStatus::InvalidHandle;
        it->second->set();
        return NtStatus::Success;
    });

    // 9. NtResetEvent (SSN: 0x004F)
    registerSyscall(SSN_NtResetEvent, "NtResetEvent", 2, [](const SyscallFrame& f) -> NtStatus {
        Handle h = static_cast<Handle>(f.arg1);
        auto it = g_KernelEvents.find(h);
        if (it == g_KernelEvents.end()) return NtStatus::InvalidHandle;
        it->second->reset();
        return NtStatus::Success;
    });

    // 10. NtCreateMutant (SSN: 0x00B2)
    registerSyscall(SSN_NtCreateMutant, "NtCreateMutant", 4, [](const SyscallFrame& f) -> NtStatus {
        auto* outHandle = reinterpret_cast<Handle*>(f.arg1);
        if (!outHandle) return NtStatus::InvalidParameter;

        bool initialOwner = (f.arg3 != 0);
        Handle h = g_NextSyncHandle;
        g_NextSyncHandle += 4;
        g_KernelMutants[h] = std::make_shared<sync::MutantObject>(initialOwner);
        *outHandle = h;
        return NtStatus::Success;
    });

    // 11. NtReleaseMutant (SSN: 0x001D)
    registerSyscall(SSN_NtReleaseMutant, "NtReleaseMutant", 2, [](const SyscallFrame& f) -> NtStatus {
        Handle h = static_cast<Handle>(f.arg1);
        auto it = g_KernelMutants.find(h);
        if (it == g_KernelMutants.end()) return NtStatus::InvalidHandle;
        bool ok = it->second->release(1);
        return ok ? NtStatus::Success : NtStatus::Unsuccessful;
    });

    // 12. NtCreateIoCompletion (SSN: 0x0164)
    registerSyscall(SSN_NtCreateIoCompletion, "NtCreateIoCompletion", 4, [](const SyscallFrame& f) -> NtStatus {
        auto* outHandle = reinterpret_cast<Handle*>(f.arg1);
        if (!outHandle) return NtStatus::InvalidParameter;

        uint32_t maxThreads = static_cast<uint32_t>(f.arg4);
        Handle h = g_NextSyncHandle;
        g_NextSyncHandle += 4;
        g_KernelIocpPorts[h] = std::make_shared<io::IoCompletionPort>(maxThreads);
        *outHandle = h;
        return NtStatus::Success;
    });

    // 13. NtSetIoCompletion (SSN: 0x0165)
    registerSyscall(SSN_NtSetIoCompletion, "NtSetIoCompletion", 5, [](const SyscallFrame& f) -> NtStatus {
        Handle h = static_cast<Handle>(f.arg1);
        auto it = g_KernelIocpPorts.find(h);
        if (it == g_KernelIocpPorts.end()) return NtStatus::InvalidHandle;

        uint64_t key = f.arg2;
        uintptr_t overlapped = f.arg3;
        NtStatus status = static_cast<NtStatus>(f.arg4);
        uint32_t bytes = 0;
        if (f.stackArgs && f.stackArgCount >= 1) {
            bytes = static_cast<uint32_t>(f.stackArgs[0]);
        }

        return it->second->postCompletion(key, overlapped, status, bytes);
    });

    // 14. NtRemoveIoCompletion (SSN: 0x0009)
    registerSyscall(SSN_NtRemoveIoCompletion, "NtRemoveIoCompletion", 5, [](const SyscallFrame& f) -> NtStatus {
        Handle h = static_cast<Handle>(f.arg1);
        auto it = g_KernelIocpPorts.find(h);
        if (it == g_KernelIocpPorts.end()) return NtStatus::InvalidHandle;

        auto* outKey = reinterpret_cast<uint64_t*>(f.arg2);
        auto* outOverlapped = reinterpret_cast<uintptr_t*>(f.arg3);
        auto* outIoStatus = reinterpret_cast<IoStatusBlock*>(f.arg4);
        auto* timeout = (f.stackArgs && f.stackArgCount >= 1) ? reinterpret_cast<LargeInteger*>(f.stackArgs[0]) : nullptr;

        uint64_t key = 0;
        uintptr_t ov = 0;
        IoStatusBlock iosb{};
        uint32_t ms = timeout ? static_cast<uint32_t>(timeout->quadPart / -10000) : 0xFFFFFFFF;

        NtStatus res = it->second->removeCompletion(key, ov, iosb, ms);
        if (outKey) *outKey = key;
        if (outOverlapped) *outOverlapped = ov;
        if (outIoStatus) *outIoStatus = iosb;
        return res;
    });
}

} // namespace micant::sys
