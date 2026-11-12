#pragma once

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include <cstddef>
#include <span>
#include <string_view>

namespace micant::ob { struct ObjectHeader; }
namespace micant::ps {
class EProcess;
class EThread;

namespace detail {
bool deferProcessObjectDelete(ob::ObjectHeader* header) noexcept;
// Single-BSP internal helpers; the caller serializes access with interrupts
// disabled. The process pointer returned by creation is borrowed from handle.
NtStatus createProcessHandle(uint32_t access, Handle& handle, EProcess*& process,
                             uint32_t handleAttributes = 0) noexcept;
NtStatus referenceProcessHandle(Handle handle, uint32_t access, EProcess*& process,
                                ob::ObjectHeader*& reference) noexcept;
struct BootstrapProcessImage {
    std::string_view name;
    std::span<const uint8_t> bytes;
    std::u16string_view path;
};
// Private BSP composition of existing Ps/Mm/Ob/Ke operations. Inputs must be
// kernel resident and are captured before switching address spaces.
NtStatus createBootstrapUserProcess(std::span<const BootstrapProcessImage> images,
                                    std::u16string_view imagePath, std::u16string_view commandLine,
                                    size_t kernelStackSize, size_t userStackSize, bool interruptible,
                                    uint32_t processAccess, uint32_t threadAccess,
                                    Handle& processHandle, Handle& threadHandle) noexcept;
// Internal BSP factory; a successfully created object remains pinned until reap.
NtStatus createThreadHandle(EProcess& process, size_t stackSize,
                            void (*start)(void*) noexcept, void* argument, uint32_t access,
                            Handle& handle, EThread*& thread) noexcept;
// Caller holds the target process root and keeps the supplied process alive.
NtStatus createUserThreadHandle(EProcess& process, uintptr_t entry, size_t kernelStackSize,
                                size_t userStackSize, bool interruptible, uint32_t access,
                                Handle& handle, EThread*& thread, size_t commitSize = 0,
                                uint32_t handleAttributes = 0) noexcept;
NtStatus referenceThreadHandle(Handle handle, uint32_t access, EThread*& thread,
                               ob::ObjectHeader*& reference) noexcept;
NtStatus referenceCurrentProcess(EProcess*& process, ob::ObjectHeader*& reference) noexcept;
NtStatus referenceCurrentThread(EThread*& thread, ob::ObjectHeader*& reference) noexcept;
bool isProcessObject(const ob::ObjectHeader& header) noexcept;
bool isThreadObject(const ob::ObjectHeader& header) noexcept;
uint32_t bootstrapProcessObjectCount() noexcept;
uint32_t bootstrapThreadObjectCount() noexcept;
}
}
