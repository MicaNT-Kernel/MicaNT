#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/executive/ps.hpp"
#include "micant/km/executive/po.hpp"

namespace micant::sys {

NtStatus NtTerminateProcess(Handle /*processHandle*/, NtStatus /*exitStatus*/) {
    return NtStatus::NotImplemented;
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
    uint32_t* /*returnLength*/
) {
    if (!processInformation) return NtStatus::InvalidParameter;
    if (processInformationClass == ProcessInformationClass::ProcessBasicInformation) {
        if (processInformationLength < sizeof(ProcessBasicInformation)) return NtStatus::InfoLengthMismatch;
    }
    return NtStatus::NotImplemented;
}

NtStatus NtQueryInformationThread(Handle, ThreadInformationClass, void*, uint32_t, uint32_t*) {
    return NtStatus::NotImplemented;
}

NtStatus NtAlertThread(Handle) { return NtStatus::NotImplemented; }
NtStatus NtTestAlert() { return NtStatus::NotImplemented; }

} // namespace micant::sys
