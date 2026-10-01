#include "micant/dispatcher.hpp"
#include "micant/sync.hpp"
#include "micant/timer.hpp"
#include "micant/po.hpp"
#include "micant/io.hpp"
#include "micant/cm.hpp"
#include "micant/se.hpp"
#include "micant/lpc.hpp"
#include "micant/ps.hpp"
#include <iostream>

namespace micant::sys {

// Global simulated kernel sync & IOCP structures
static std::unordered_map<Handle, std::shared_ptr<io::IoCompletionPort>> g_KernelIocpPorts;
static std::unordered_map<Handle, std::shared_ptr<cm::KeyObject>> g_KernelKeys;
static std::unordered_map<Handle, std::shared_ptr<se::TokenObject>> g_KernelTokens;
static std::unordered_map<Handle, std::shared_ptr<lpc::PortObject>> g_KernelPorts;
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
        bool hadIocp = g_KernelIocpPorts.erase(h) > 0;
        bool hadKey = g_KernelKeys.erase(h) > 0;
        bool hadToken = g_KernelTokens.erase(h) > 0;
        bool hadPort = g_KernelPorts.erase(h) > 0;
        NtStatus status = NtClose(h);
        if (hadIocp || hadKey || hadToken || hadPort) {
            return NtStatus::Success;
        }
        return status;
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

    // 7. NtCreateEvent (SSN: 0x0048)
    registerSyscall(SSN_NtCreateEvent, "NtCreateEvent", 5, [](const SyscallFrame& f) -> NtStatus {
        auto* outHandle = reinterpret_cast<Handle*>(f.arg1);
        if (!outHandle) return NtStatus::InvalidParameter;

        sync::EventType evType = (f.arg3 == 0) ? sync::EventType::NotificationEvent : sync::EventType::SynchronizationEvent;
        bool initialState = (f.arg4 != 0);

        auto ev = std::make_shared<sync::EventObject>(evType, initialState);
        Handle h = sync::DispatcherRegistry::get().registerObject(ev);
        *outHandle = h;
        return NtStatus::Success;
    });

    // 8. NtSetEvent (SSN: 0x004E)
    registerSyscall(SSN_NtSetEvent, "NtSetEvent", 2, [](const SyscallFrame& f) -> NtStatus {
        Handle h = static_cast<Handle>(f.arg1);
        auto ev = sync::DispatcherRegistry::get().lookupAs<sync::EventObject>(h);
        if (!ev) return NtStatus::InvalidHandle;
        ev->set();
        return NtStatus::Success;
    });

    // 9. NtResetEvent (SSN: 0x004F)
    registerSyscall(SSN_NtResetEvent, "NtResetEvent", 2, [](const SyscallFrame& f) -> NtStatus {
        Handle h = static_cast<Handle>(f.arg1);
        auto ev = sync::DispatcherRegistry::get().lookupAs<sync::EventObject>(h);
        if (!ev) return NtStatus::InvalidHandle;
        ev->reset();
        return NtStatus::Success;
    });

    // 10. NtCreateMutant (SSN: 0x00B2)
    registerSyscall(SSN_NtCreateMutant, "NtCreateMutant", 4, [](const SyscallFrame& f) -> NtStatus {
        auto* outHandle = reinterpret_cast<Handle*>(f.arg1);
        if (!outHandle) return NtStatus::InvalidParameter;

        bool initialOwner = (f.arg3 != 0);
        auto mut = std::make_shared<sync::MutantObject>(initialOwner);
        Handle h = sync::DispatcherRegistry::get().registerObject(mut);
        *outHandle = h;
        return NtStatus::Success;
    });

    // 11. NtReleaseMutant (SSN: 0x001D)
    registerSyscall(SSN_NtReleaseMutant, "NtReleaseMutant", 2, [](const SyscallFrame& f) -> NtStatus {
        Handle h = static_cast<Handle>(f.arg1);
        auto mut = sync::DispatcherRegistry::get().lookupAs<sync::MutantObject>(h);
        if (!mut) return NtStatus::InvalidHandle;
        bool ok = mut->release(1);
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

    // 15. NtOpenKey (SSN: 0x0012)
    registerSyscall(SSN_NtOpenKey, "NtOpenKey", 3, [](const SyscallFrame& f) -> NtStatus {
        auto* outHandle = reinterpret_cast<Handle*>(f.arg1);
        auto* objAttr = reinterpret_cast<ObjectAttributes*>(f.arg3);
        if (!outHandle || !objAttr || !objAttr->objectName) return NtStatus::InvalidParameter;

        auto keyObj = cm::ConfigurationManager::get().resolvePath(objAttr->objectName->view());
        if (!keyObj) return NtStatus::ObjectNameNotFound;

        Handle h = g_NextSyncHandle;
        g_NextSyncHandle += 4;
        g_KernelKeys[h] = keyObj;
        *outHandle = h;
        return NtStatus::Success;
    });

    // 16. NtQueryValueKey (SSN: 0x0016)
    registerSyscall(SSN_NtQueryValueKey, "NtQueryValueKey", 6, [](const SyscallFrame& f) -> NtStatus {
        Handle h = static_cast<Handle>(f.arg1);
        auto* valName = reinterpret_cast<UnicodeString*>(f.arg2);
        void* outBuffer = reinterpret_cast<void*>(f.arg4);
        if (!valName) return NtStatus::InvalidParameter;

        auto it = g_KernelKeys.find(h);
        if (it == g_KernelKeys.end()) return NtStatus::InvalidHandle;

        const auto* val = it->second->getValue(valName->view());
        if (!val) return NtStatus::ObjectNameNotFound;

        size_t bufLen = 0;
        size_t* retLen = nullptr;
        if (f.stackArgs && f.stackArgCount >= 2) {
            bufLen = static_cast<size_t>(f.stackArgs[0]);
            retLen = reinterpret_cast<size_t*>(f.stackArgs[1]);
        }

        if (retLen) *retLen = val->data.size();
        if (outBuffer && bufLen >= val->data.size()) {
            std::memcpy(outBuffer, val->data.data(), val->data.size());
            return NtStatus::Success;
        }

        return (bufLen < val->data.size()) ? NtStatus::BufferTooSmall : NtStatus::Success;
    });

    // 17. NtSetValueKey (SSN: 0x0060)
    registerSyscall(SSN_NtSetValueKey, "NtSetValueKey", 6, [](const SyscallFrame& f) -> NtStatus {
        Handle h = static_cast<Handle>(f.arg1);
        auto* valName = reinterpret_cast<UnicodeString*>(f.arg2);
        uint32_t type = static_cast<uint32_t>(f.arg4);
        if (!valName) return NtStatus::InvalidParameter;

        auto it = g_KernelKeys.find(h);
        if (it == g_KernelKeys.end()) return NtStatus::InvalidHandle;

        const void* data = nullptr;
        uint32_t dataSize = 0;
        if (f.stackArgs && f.stackArgCount >= 2) {
            data = reinterpret_cast<const void*>(f.stackArgs[0]);
            dataSize = static_cast<uint32_t>(f.stackArgs[1]);
        }
        if (!data || dataSize == 0) return NtStatus::InvalidParameter;

        if (type == static_cast<uint32_t>(cm::RegType::Sz)) {
            std::wstring strVal(reinterpret_cast<const wchar_t*>(data), dataSize / sizeof(wchar_t));
            it->second->setValueString(valName->view(), strVal);
        } else if (type == static_cast<uint32_t>(cm::RegType::Dword) && dataSize >= sizeof(uint32_t)) {
            it->second->setValueDword(valName->view(), *reinterpret_cast<const uint32_t*>(data));
        } else if (type == static_cast<uint32_t>(cm::RegType::Qword) && dataSize >= sizeof(uint64_t)) {
            it->second->setValueQword(valName->view(), *reinterpret_cast<const uint64_t*>(data));
        }

        return NtStatus::Success;
    });

    // 18. NtOpenProcessToken (SSN: 0x00BE)
    registerSyscall(SSN_NtOpenProcessToken, "NtOpenProcessToken", 3, [](const SyscallFrame& f) -> NtStatus {
        Handle procHandle = static_cast<Handle>(f.arg1);
        auto* outTokenHandle = reinterpret_cast<Handle*>(f.arg3);
        if (!outTokenHandle) return NtStatus::InvalidParameter;

        auto proc = ps::ProcessManager::get().getProcess(procHandle);
        std::shared_ptr<se::TokenObject> token;
        if (proc) {
            token = proc->getToken();
        }
        if (!token) {
            token = se::TokenObject::createSystemToken();
        }

        Handle h = g_NextSyncHandle;
        g_NextSyncHandle += 4;
        g_KernelTokens[h] = token;
        *outTokenHandle = h;
        return NtStatus::Success;
    });

    // 19. NtAccessCheck (SSN: 0x0182)
    registerSyscall(SSN_NtAccessCheck, "NtAccessCheck", 5, [](const SyscallFrame& f) -> NtStatus {
        auto* secDesc = reinterpret_cast<se::SecurityDescriptor*>(f.arg1);
        Handle tokenHandle = static_cast<Handle>(f.arg2);
        uint32_t desiredAccess = static_cast<uint32_t>(f.arg3);
        auto* grantedAccess = reinterpret_cast<uint32_t*>(f.arg4);
        auto* accessStatus = (f.stackArgs && f.stackArgCount >= 1) ? reinterpret_cast<NtStatus*>(f.stackArgs[0]) : nullptr;

        if (!secDesc || !grantedAccess) return NtStatus::InvalidParameter;

        auto it = g_KernelTokens.find(tokenHandle);
        if (it == g_KernelTokens.end()) return NtStatus::InvalidHandle;

        NtStatus res = se::SecurityReferenceMonitor::accessCheck(*it->second, *secDesc, desiredAccess, *grantedAccess);
        if (accessStatus) *accessStatus = res;
        return res;
    });

    // 20. NtCreatePort (SSN: 0x0093)
    registerSyscall(SSN_NtCreatePort, "NtCreatePort", 4, [](const SyscallFrame& f) -> NtStatus {
        auto* outPortHandle = reinterpret_cast<Handle*>(f.arg1);
        auto* objAttr = reinterpret_cast<ObjectAttributes*>(f.arg2);
        if (!outPortHandle || !objAttr || !objAttr->objectName) return NtStatus::InvalidParameter;

        auto port = lpc::PortManager::get().createPort(objAttr->objectName->view());
        if (!port) return NtStatus::ObjectNameCollision;

        Handle h = g_NextSyncHandle;
        g_NextSyncHandle += 4;
        g_KernelPorts[h] = port;
        *outPortHandle = h;
        return NtStatus::Success;
    });

    // 21. NtConnectPort (SSN: 0x0096)
    registerSyscall(SSN_NtConnectPort, "NtConnectPort", 4, [](const SyscallFrame& f) -> NtStatus {
        auto* outPortHandle = reinterpret_cast<Handle*>(f.arg1);
        auto* portName = reinterpret_cast<UnicodeString*>(f.arg2);
        if (!outPortHandle || !portName) return NtStatus::InvalidParameter;

        std::shared_ptr<lpc::PortObject> clientPort;
        std::shared_ptr<lpc::PortObject> serverPort;
        NtStatus status = lpc::PortManager::get().connectPort(portName->view(), clientPort, serverPort);
        if (!NT_SUCCESS(status)) return status;

        Handle h = g_NextSyncHandle;
        g_NextSyncHandle += 4;
        g_KernelPorts[h] = clientPort;
        *outPortHandle = h;
        return NtStatus::Success;
    });

    // 22. NtRequestWaitReplyPort (SSN: 0x0022)
    registerSyscall(SSN_NtRequestWaitReplyPort, "NtRequestWaitReplyPort", 3, [](const SyscallFrame& f) -> NtStatus {
        Handle h = static_cast<Handle>(f.arg1);
        auto* reqMsg = reinterpret_cast<lpc::PortMessage*>(f.arg2);
        auto* replyMsg = reinterpret_cast<lpc::PortMessage*>(f.arg3);
        if (!reqMsg || !replyMsg) return NtStatus::InvalidParameter;

        auto it = g_KernelPorts.find(h);
        if (it == g_KernelPorts.end()) return NtStatus::InvalidHandle;

        const uint8_t* reqData = reinterpret_cast<const uint8_t*>(reqMsg + 1);
        size_t reqDataLen = reqMsg->u1.dataLength;

        std::vector<uint8_t> replyData;
        NtStatus res = it->second->requestWaitReply(*reqMsg, std::span<const uint8_t>(reqData, reqDataLen), *replyMsg, replyData, 5000);
        if (NT_SUCCESS(res) && !replyData.empty()) {
            uint8_t* outReplyData = reinterpret_cast<uint8_t*>(replyMsg + 1);
            std::memcpy(outReplyData, replyData.data(), std::min<size_t>(replyData.size(), 256));
        }
        return res;
    });

    // 23. NtCreateFile (SSN: 0x0055)
    registerSyscall(SSN_NtCreateFile, "NtCreateFile", 11, [](const SyscallFrame& f) -> NtStatus {
        auto* fileHandle = reinterpret_cast<Handle*>(f.arg1);
        uint32_t desiredAccess = static_cast<uint32_t>(f.arg2);
        auto* objAttr = reinterpret_cast<ObjectAttributes*>(f.arg3);
        auto* iosb = reinterpret_cast<IoStatusBlock*>(f.arg4);

        LargeInteger* allocSize = nullptr;
        uint32_t fileAttr = 0;
        uint32_t share = 0;
        uint32_t disposition = 1; // FILE_OPEN
        uint32_t options = 0;
        void* eaBuf = nullptr;
        uint32_t eaLen = 0;

        if (f.stackArgs && f.stackArgCount >= 4) {
            allocSize = reinterpret_cast<LargeInteger*>(f.stackArgs[0]);
            fileAttr = static_cast<uint32_t>(f.stackArgs[1]);
            share = static_cast<uint32_t>(f.stackArgs[2]);
            disposition = static_cast<uint32_t>(f.stackArgs[3]);
            if (f.stackArgCount >= 5) options = static_cast<uint32_t>(f.stackArgs[4]);
            if (f.stackArgCount >= 6) eaBuf = reinterpret_cast<void*>(f.stackArgs[5]);
            if (f.stackArgCount >= 7) eaLen = static_cast<uint32_t>(f.stackArgs[6]);
        }

        return NtCreateFile(fileHandle, desiredAccess, objAttr, iosb, allocSize, fileAttr, share, disposition, options, eaBuf, eaLen);
    });

    // 24. NtOpenFile (SSN: 0x0033)
    registerSyscall(SSN_NtOpenFile, "NtOpenFile", 6, [](const SyscallFrame& f) -> NtStatus {
        auto* fileHandle = reinterpret_cast<Handle*>(f.arg1);
        uint32_t desiredAccess = static_cast<uint32_t>(f.arg2);
        auto* objAttr = reinterpret_cast<ObjectAttributes*>(f.arg3);
        auto* iosb = reinterpret_cast<IoStatusBlock*>(f.arg4);

        uint32_t share = 0;
        uint32_t options = 0;
        if (f.stackArgs && f.stackArgCount >= 2) {
            share = static_cast<uint32_t>(f.stackArgs[0]);
            options = static_cast<uint32_t>(f.stackArgs[1]);
        }

        return NtOpenFile(fileHandle, desiredAccess, objAttr, iosb, share, options);
    });

    // 25. NtReadFile (SSN: 0x0006)
    registerSyscall(SSN_NtReadFile, "NtReadFile", 9, [](const SyscallFrame& f) -> NtStatus {
        Handle fileHandle = static_cast<Handle>(f.arg1);
        Handle event = static_cast<Handle>(f.arg2);
        void* apcRoutine = reinterpret_cast<void*>(f.arg3);
        void* apcContext = reinterpret_cast<void*>(f.arg4);

        IoStatusBlock* iosb = nullptr;
        void* buffer = nullptr;
        uint32_t length = 0;
        LargeInteger* byteOffset = nullptr;
        uint32_t* key = nullptr;

        if (f.stackArgs && f.stackArgCount >= 3) {
            iosb = reinterpret_cast<IoStatusBlock*>(f.stackArgs[0]);
            buffer = reinterpret_cast<void*>(f.stackArgs[1]);
            length = static_cast<uint32_t>(f.stackArgs[2]);
            if (f.stackArgCount >= 4) byteOffset = reinterpret_cast<LargeInteger*>(f.stackArgs[3]);
            if (f.stackArgCount >= 5) key = reinterpret_cast<uint32_t*>(f.stackArgs[4]);
        }

        return NtReadFile(fileHandle, event, apcRoutine, apcContext, iosb, buffer, length, byteOffset, key);
    });

    // 26. NtWriteFile (SSN: 0x0008)
    registerSyscall(SSN_NtWriteFile, "NtWriteFile", 9, [](const SyscallFrame& f) -> NtStatus {
        Handle fileHandle = static_cast<Handle>(f.arg1);
        Handle event = static_cast<Handle>(f.arg2);
        void* apcRoutine = reinterpret_cast<void*>(f.arg3);
        void* apcContext = reinterpret_cast<void*>(f.arg4);

        IoStatusBlock* iosb = nullptr;
        const void* buffer = nullptr;
        uint32_t length = 0;
        LargeInteger* byteOffset = nullptr;
        uint32_t* key = nullptr;

        if (f.stackArgs && f.stackArgCount >= 3) {
            iosb = reinterpret_cast<IoStatusBlock*>(f.stackArgs[0]);
            buffer = reinterpret_cast<const void*>(f.stackArgs[1]);
            length = static_cast<uint32_t>(f.stackArgs[2]);
            if (f.stackArgCount >= 4) byteOffset = reinterpret_cast<LargeInteger*>(f.stackArgs[3]);
            if (f.stackArgCount >= 5) key = reinterpret_cast<uint32_t*>(f.stackArgs[4]);
        }

        return NtWriteFile(fileHandle, event, apcRoutine, apcContext, iosb, buffer, length, byteOffset, key);
    });

    // 27. NtDeviceIoControlFile (SSN: 0x0007)
    registerSyscall(SSN_NtDeviceIoControlFile, "NtDeviceIoControlFile", 10, [](const SyscallFrame& f) -> NtStatus {
        Handle fileHandle = static_cast<Handle>(f.arg1);
        Handle event = static_cast<Handle>(f.arg2);
        void* apcRoutine = reinterpret_cast<void*>(f.arg3);
        void* apcContext = reinterpret_cast<void*>(f.arg4);

        IoStatusBlock* iosb = nullptr;
        uint32_t ioControlCode = 0;
        const void* inBuf = nullptr;
        uint32_t inLen = 0;
        void* outBuf = nullptr;
        uint32_t outLen = 0;

        if (f.stackArgs && f.stackArgCount >= 2) {
            iosb = reinterpret_cast<IoStatusBlock*>(f.stackArgs[0]);
            ioControlCode = static_cast<uint32_t>(f.stackArgs[1]);
            if (f.stackArgCount >= 3) inBuf = reinterpret_cast<const void*>(f.stackArgs[2]);
            if (f.stackArgCount >= 4) inLen = static_cast<uint32_t>(f.stackArgs[3]);
            if (f.stackArgCount >= 5) outBuf = reinterpret_cast<void*>(f.stackArgs[4]);
            if (f.stackArgCount >= 6) outLen = static_cast<uint32_t>(f.stackArgs[5]);
        }

        return NtDeviceIoControlFile(fileHandle, event, apcRoutine, apcContext, iosb, ioControlCode, inBuf, inLen, outBuf, outLen);
    });

    // 28. NtWaitForMultipleObjects (SSN: 0x005A)
    registerSyscall(SSN_NtWaitForMultipleObjects, "NtWaitForMultipleObjects", 5, [](const SyscallFrame& f) -> NtStatus {
        uint32_t count = static_cast<uint32_t>(f.arg1);
        auto* handles = reinterpret_cast<const Handle*>(f.arg2);
        WaitType waitType = static_cast<WaitType>(f.arg3);
        bool alertable = f.arg4 != 0;
        LargeInteger* timeout = nullptr;
        if (f.stackArgs && f.stackArgCount >= 1) {
            timeout = reinterpret_cast<LargeInteger*>(f.stackArgs[0]);
        }
        return NtWaitForMultipleObjects(count, handles, waitType, alertable, timeout);
    });

    // 29. NtDelayExecution (SSN: 0x0034)
    registerSyscall(SSN_NtDelayExecution, "NtDelayExecution", 2, [](const SyscallFrame& f) -> NtStatus {
        bool alertable = f.arg1 != 0;
        auto* interval = reinterpret_cast<const LargeInteger*>(f.arg2);
        return NtDelayExecution(alertable, interval);
    });

    // 30. NtCreateTimer (SSN: 0x0057)
    registerSyscall(SSN_NtCreateTimer, "NtCreateTimer", 4, [](const SyscallFrame& f) -> NtStatus {
        auto* outHandle = reinterpret_cast<Handle*>(f.arg1);
        uint32_t desiredAccess = static_cast<uint32_t>(f.arg2);
        auto* objAttr = reinterpret_cast<ObjectAttributes*>(f.arg3);
        uint32_t timerType = static_cast<uint32_t>(f.arg4);
        return NtCreateTimer(outHandle, desiredAccess, objAttr, timerType);
    });

    // 31. NtSetTimer (SSN: 0x0078)
    registerSyscall(SSN_NtSetTimer, "NtSetTimer", 7, [](const SyscallFrame& f) -> NtStatus {
        Handle h = static_cast<Handle>(f.arg1);
        auto* dueTime = reinterpret_cast<LargeInteger*>(f.arg2);
        void* apcRoutine = reinterpret_cast<void*>(f.arg3);
        void* apcContext = reinterpret_cast<void*>(f.arg4);
        bool resumeTimer = false;
        uint32_t period = 0;
        bool* previousState = nullptr;
        if (f.stackArgs && f.stackArgCount >= 1) resumeTimer = f.stackArgs[0] != 0;
        if (f.stackArgs && f.stackArgCount >= 2) period = static_cast<uint32_t>(f.stackArgs[1]);
        if (f.stackArgs && f.stackArgCount >= 3) previousState = reinterpret_cast<bool*>(f.stackArgs[2]);
        return NtSetTimer(h, dueTime, apcRoutine, apcContext, resumeTimer, period, previousState);
    });

    // 32. NtCancelTimer (SSN: 0x0077)
    registerSyscall(SSN_NtCancelTimer, "NtCancelTimer", 2, [](const SyscallFrame& f) -> NtStatus {
        Handle h = static_cast<Handle>(f.arg1);
        auto* currentSignaledState = reinterpret_cast<bool*>(f.arg2);
        return NtCancelTimer(h, currentSignaledState);
    });

    // 33. NtShutdownSystem (SSN: 0x0118)
    registerSyscall(SSN_NtShutdownSystem, "NtShutdownSystem", 1, [](const SyscallFrame& f) -> NtStatus {
        uint32_t action = static_cast<uint32_t>(f.arg1);
        return NtShutdownSystem(action);
    });
}

} // namespace micant::sys
