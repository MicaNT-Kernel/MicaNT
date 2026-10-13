#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/executive/ps.hpp"
#include "micant/km/executive/po.hpp"
#include <iostream>

namespace micant::sys {

NtStatus NtTerminateProcess(Handle processHandle, NtStatus exitStatus) {
    std::cout << "[MicaNT Executive] NtTerminateProcess called (PID handle: " 
              << processHandle << ", exit status: " << static_cast<uint32_t>(exitStatus) << ")\n";
    return NtStatus::Success;
}

NtStatus NtShutdownSystem(uint32_t action) {
    auto shutdownAction = static_cast<po::ShutdownAction>(action);
    return po::PowerManager::get().shutdownSystem(shutdownAction);
}

NtStatus NtQuerySystemInformation(
    uint32_t /*systemInformationClass*/,
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

NtStatus NtQueryInformationProcess(
    Handle /*processHandle*/,
    ProcessInformationClass processInformationClass,
    void* processInformation,
    uint32_t processInformationLength,
    uint32_t* returnLength
) {
    if (!processInformation) return NtStatus::InvalidParameter;
    if (processInformationClass == ProcessInformationClass::ProcessBasicInformation) {
        if (processInformationLength < sizeof(ProcessBasicInformation)) return NtStatus::InfoLengthMismatch;
        auto* pbi = reinterpret_cast<ProcessBasicInformation*>(processInformation);
        pbi->exitStatus = NtStatus::Success;
        pbi->pebBaseAddress = 0x00007FFDF0000000ULL;
        pbi->affinityMask = 0x0F;
        pbi->basePriority = 8;
        pbi->uniqueProcessId = 1000;
        pbi->inheritedFromUniqueProcessId = 0;
        if (returnLength) *returnLength = sizeof(ProcessBasicInformation);
        return NtStatus::Success;
    }
    return NtStatus::NotImplemented;
}

} // namespace micant::sys
