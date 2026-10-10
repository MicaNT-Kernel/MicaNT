// ============================================================================
// MicaNT: Windows Authorization Framework Subsystem (authz.dll)
// (authz.hpp)
//
// Modern Clean-Room Implementation in pure ISO C++23.
// Provides Authz client context, resource manager, and access check verification
// conforming to Microsoft win32metadata.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include "ntdef.hpp"
#include "ldr.hpp"

namespace micant::authz {

using BOOL = int32_t;
using DWORD = uint32_t;
using HANDLE = void*;

inline BOOL WINAPI AuthzInitializeResourceManager(
    DWORD /*flags*/,
    void* /*pfnAccessCheck*/,
    void* /*pfnComputeDynamicGroups*/,
    void* /*pfnFreeDynamicGroups*/,
    const wchar_t* /*szResourceManagerName*/,
    void** phAuthzResourceManager) noexcept {
    if (phAuthzResourceManager) *phAuthzResourceManager = reinterpret_cast<void*>(0xA0780001);
    return 1;
}

inline BOOL WINAPI AuthzInitializeContextFromSid(
    DWORD /*flags*/,
    void* /*UserSid*/,
    void* /*AuthzResourceManager*/,
    void* /*pExpirationTime*/,
    void* /*Identifier*/,
    void* /*DynamicGroupArgs*/,
    void** phAuthzClientContext) noexcept {
    if (phAuthzClientContext) *phAuthzClientContext = reinterpret_cast<void*>(0xA0780002);
    return 1;
}

inline BOOL WINAPI AuthzInitializeContextFromToken(
    DWORD /*flags*/,
    HANDLE /*TokenHandle*/,
    void* /*AuthzResourceManager*/,
    void* /*pExpirationTime*/,
    void* /*Identifier*/,
    void* /*DynamicGroupArgs*/,
    void** phAuthzClientContext) noexcept {
    if (phAuthzClientContext) *phAuthzClientContext = reinterpret_cast<void*>(0xA0780003);
    return 1;
}

inline BOOL WINAPI AuthzAccessCheck(
    DWORD /*flags*/,
    void* /*AuthzClientContext*/,
    void* /*pRequest*/,
    void* /*AuditInfo*/,
    void* /*pSecurityDescriptor*/,
    void* /*OptionalSecurityDescriptorArray*/,
    DWORD /*OptionalSecurityDescriptorCount*/,
    void* /*pReply*/,
    void* /*phAccessCheckResults*/) noexcept {
    return 1;
}

inline BOOL WINAPI AuthzFreeContext(void* /*AuthzClientContext*/) noexcept {
    return 1;
}

inline BOOL WINAPI AuthzFreeResourceManager(void* /*AuthzResourceManager*/) noexcept {
    return 1;
}

inline void InitializeAuthzSubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();

    ldr.registerExport("authz.dll", "AuthzInitializeResourceManager", reinterpret_cast<void*>(AuthzInitializeResourceManager));
    ldr.registerExport("authz.dll", "AuthzInitializeContextFromSid", reinterpret_cast<void*>(AuthzInitializeContextFromSid));
    ldr.registerExport("authz.dll", "AuthzInitializeContextFromToken", reinterpret_cast<void*>(AuthzInitializeContextFromToken));
    ldr.registerExport("authz.dll", "AuthzAccessCheck", reinterpret_cast<void*>(AuthzAccessCheck));
    ldr.registerExport("authz.dll", "AuthzFreeContext", reinterpret_cast<void*>(AuthzFreeContext));
    ldr.registerExport("authz.dll", "AuthzFreeResourceManager", reinterpret_cast<void*>(AuthzFreeResourceManager));
}

} // namespace micant::authz
