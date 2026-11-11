#pragma once

#include <cstddef>
#include <cstdint>

namespace micant::hal::amd64 {

bool initializeBootstrapAddressSpace(void (*log)(const char*) noexcept) noexcept;
// IRQ-disabled, single-CPU bootstrap test of PFN-backed paging and reclamation.
bool verifyBootstrapPageTables() noexcept;
// Bootstrap CR3 only; callbacks require the BSP with interrupts disabled.
inline constexpr uintptr_t BootstrapManagedBase = (uintptr_t{128} << 39) + 0x20000000;
inline constexpr size_t BootstrapManagedSize = 16 * 1024 * 1024;
bool mapBootstrapManagedPage(void* context, uintptr_t address, uint64_t pfn,
                             uint32_t protection) noexcept;
bool unmapBootstrapManagedPage(void* context, uintptr_t address) noexcept;
uint64_t translateBootstrapAddress(const void* address) noexcept;
void* mapBootstrapDeviceMemory(uint64_t physical, size_t size) noexcept;

// Runs isolated CPL=3 probes with checked #BP/#PF returns and page protections.
// This validates privilege transitions, not a process loader or Win32 session.
bool verifyRing3Transition() noexcept;

using UserServiceRoutine = uint64_t (*)(uint64_t number, uint64_t first,
                                       uint64_t second) noexcept;
bool initializeBootstrapSyscalls(UserServiceRoutine dispatch) noexcept;
bool bootstrapUserRange(uint64_t address, size_t size, bool writable,
                        bool executable = false) noexcept;
bool bootstrapKernelRange(uint64_t address, size_t size, bool writable) noexcept;
bool verifyBootstrapSyscalls() noexcept;
bool mapBootstrapUserMemory(uint64_t address, const void* backing, size_t size,
                            bool writable, bool executable) noexcept;
bool verifyNativeImageLoading() noexcept;

} // namespace micant::hal::amd64
