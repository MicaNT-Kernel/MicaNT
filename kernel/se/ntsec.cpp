#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/executive/se.hpp"
#include "micant/km/executive/ps.hpp"
#include <unordered_map>
#include <memory>

namespace micant::sys::internal {

static std::unordered_map<Handle, std::shared_ptr<se::TokenObject>> g_KernelTokens;

bool SeCloseTokenHandle(Handle h) {
    return g_KernelTokens.erase(h) > 0;
}

NtStatus NtOpenProcessTokenSys(const SyscallFrame& f) {
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

    Handle h = ObAllocateExecutiveHandle();
    g_KernelTokens[h] = token;
    *outTokenHandle = h;
    return NtStatus::Success;
}

NtStatus NtAccessCheckSys(const SyscallFrame& f) {
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
}

} // namespace micant::sys::internal
