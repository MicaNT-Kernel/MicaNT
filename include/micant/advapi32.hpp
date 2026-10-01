#pragma once

/**
 * @file advapi32.hpp
 * @brief MicaNT Clean-Room Advanced Windows 32 Base API (advapi32.dll) Bridge.
 *
 * Implements security, crypto random, and token management exports.
 */

#include <cstdint>
#include <cstring>
#include <random>
#include "ntdef.hpp"
#include "ntstatus.hpp"
#include "kernel32.hpp"
#include "ldr.hpp"

namespace micant::advapi32 {

inline win32::BOOL CryptAcquireContextA(
    uintptr_t* phProv,
    const char* /*szContainer*/,
    const char* /*szProvider*/,
    uint32_t /*dwProvType*/,
    uint32_t /*dwFlags*/
) noexcept {
    if (phProv) *phProv = 0xCAFE0001;
    return win32::TRUE;
}

inline win32::BOOL CryptReleaseContext(uintptr_t /*hProv*/, uint32_t /*dwFlags*/) noexcept {
    return win32::TRUE;
}

inline win32::BOOL CryptGenRandom(uintptr_t /*hProv*/, uint32_t dwLen, uint8_t* pbBuffer) noexcept {
    if (!pbBuffer) return win32::FALSE;
    static std::mt19937_64 rng(0x1988DEC);
    for (uint32_t i = 0; i < dwLen; ++i) {
        pbBuffer[i] = static_cast<uint8_t>(rng() & 0xFF);
    }
    return win32::TRUE;
}

inline win32::BOOL OpenProcessToken(
    win32::HANDLE /*ProcessHandle*/,
    uint32_t /*DesiredAccess*/,
    win32::HANDLE* TokenHandle
) noexcept {
    if (TokenHandle) *TokenHandle = reinterpret_cast<win32::HANDLE>(0x0000000000000100ULL);
    return win32::TRUE;
}

inline win32::BOOL GetTokenInformation(
    win32::HANDLE /*TokenHandle*/,
    uint32_t /*TokenInformationClass*/,
    void* /*TokenInformation*/,
    uint32_t /*TokenInformationLength*/,
    uint32_t* ReturnLength
) noexcept {
    if (ReturnLength) *ReturnLength = 64;
    return win32::TRUE;
}

inline void InitializeAdvapi32SubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("advapi32.dll", "CryptAcquireContextA", reinterpret_cast<void*>(CryptAcquireContextA));
    ldr.registerExport("advapi32.dll", "CryptReleaseContext", reinterpret_cast<void*>(CryptReleaseContext));
    ldr.registerExport("advapi32.dll", "CryptGenRandom", reinterpret_cast<void*>(CryptGenRandom));
    ldr.registerExport("advapi32.dll", "OpenProcessToken", reinterpret_cast<void*>(OpenProcessToken));
    ldr.registerExport("advapi32.dll", "GetTokenInformation", reinterpret_cast<void*>(GetTokenInformation));
}

} // namespace micant::advapi32
