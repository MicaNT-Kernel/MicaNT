#pragma once

#include <cstddef>
#include <cstdint>

namespace micant::hal::amd64 {

bool initializeBootstrapAddressSpace(void (*log)(const char*) noexcept) noexcept;
uint64_t translateBootstrapAddress(const void* address) noexcept;
void* mapBootstrapDeviceMemory(uint64_t physical, size_t size) noexcept;

// Runs isolated CPL=3 probes with checked #BP/#PF returns and page protections.
// This validates privilege transitions, not a process loader or Win32 session.
bool verifyRing3Transition() noexcept;

} // namespace micant::hal::amd64
