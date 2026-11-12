#include "micant/km/executive/ps.hpp"

extern "C" bool MicantPsTerminateFailedUserException(uint32_t exitStatus) noexcept {
    auto* current = micant::ke::KeGetCurrentThread();
    if (!current || current->previousUserMode != true || !current->owningThread ||
        !current->owningProcess || micant::ke::KeGetCurrentIrql() != micant::ke::PASSIVE_LEVEL)
        return false;

    // A missing dispatcher, exhausted stack guarantee, or invalid user
    // delivery frame is an unhandled user exception. Terminate through Ps so
    // APC rundown, thread exit and deferred process cleanup still run.
    (void)current->owningProcess->terminateCurrent(static_cast<micant::NtStatus>(exitStatus));
    return false;
}
