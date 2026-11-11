#include "micant/um/subsystems/winbase/kernel32.hpp"

#include <cassert>

namespace {
unsigned allocateCalls = 0;
unsigned freeCalls = 0;
}

namespace micant::sys {
void SyscallDispatcher::initializeStandardTable() {
    registerSyscall(SSN_NtAllocateVirtualMemory, "NtAllocateVirtualMemory", 6,
        [](const SyscallFrame& f) {
            ++allocateCalls;
            return NtAllocateVirtualMemory(static_cast<Handle>(f.arg1),
                reinterpret_cast<uintptr_t*>(f.arg2), f.arg3,
                reinterpret_cast<size_t*>(f.arg4), static_cast<uint32_t>(f.stackArgs[0]),
                static_cast<uint32_t>(f.stackArgs[1]));
        });
    registerSyscall(SSN_NtFreeVirtualMemory, "NtFreeVirtualMemory", 4,
        [](const SyscallFrame& f) {
            ++freeCalls;
            return NtFreeVirtualMemory(static_cast<Handle>(f.arg1),
                reinterpret_cast<uintptr_t*>(f.arg2), reinterpret_cast<size_t*>(f.arg3),
                static_cast<uint32_t>(f.arg4));
        });
}
}

int main() {
    using namespace micant;
    auto& dispatcher = sys::SyscallDispatcher::get();
    constexpr uintptr_t requested = 0x220012fffULL;
    const unsigned callsBeforeZero = allocateCalls;
    assert(win32::VirtualAlloc(reinterpret_cast<void*>(requested), 0,
        win32::MEM_RESERVE, mm::PAGE_READWRITE) == nullptr);
    assert(win32::GetLastError() == 87 && allocateCalls == callsBeforeZero);
    void* allocated = win32::VirtualAlloc(reinterpret_cast<void*>(requested), 2,
        win32::MEM_RESERVE | win32::MEM_COMMIT, mm::PAGE_READWRITE);
    assert(reinterpret_cast<uintptr_t>(allocated) == 0x220010000ULL);
    uintptr_t base = reinterpret_cast<uintptr_t>(allocated);
    size_t size = 0;
    assert(sys::NtFreeVirtualMemory(InvalidHandleValue, &base, &size, mm::MEM_RELEASE)
        == NtStatus::Success);
    assert(size == 0x4000);
    assert(win32::VirtualAlloc(reinterpret_cast<void*>(requested),
        static_cast<size_t>(-1), win32::MEM_RESERVE, mm::PAGE_READWRITE) == nullptr);

    const unsigned freesBefore = freeCalls;
    void* first = win32::VirtualAlloc(nullptr, 1, win32::MEM_RESERVE, mm::PAGE_READWRITE);
    void* second = win32::VirtualAlloc(nullptr, 1, win32::MEM_RESERVE, mm::PAGE_READWRITE);
    assert(first && second && first != second);
    assert((reinterpret_cast<uintptr_t>(first) & 0xffff) == 0);
    assert((reinterpret_cast<uintptr_t>(second) & 0xffff) == 0);
    assert(freeCalls == freesBefore);
    assert(win32::VirtualFree(first, 0, win32::MEM_RELEASE));
    assert(win32::VirtualFree(second, 0, win32::MEM_RELEASE));

    dispatcher.registerSyscall(sys::SSN_NtAllocateVirtualMemory, "test allocation", 6,
        [](const sys::SyscallFrame& f) {
            *reinterpret_cast<uintptr_t*>(f.arg2) = 0x10001000;
            return NtStatus::Success;
        });
    dispatcher.registerSyscall(sys::SSN_NtFreeVirtualMemory, "test cleanup failure", 4,
        [](const sys::SyscallFrame& f) {
            ++freeCalls;
            assert(*reinterpret_cast<uintptr_t*>(f.arg2) == 0x10001000);
            assert(*reinterpret_cast<size_t*>(f.arg3) == 0);
            assert(f.arg4 == mm::MEM_RELEASE);
            return NtStatus::AccessDenied;
        });
    const unsigned freesBeforeFailure = freeCalls;
    assert(win32::VirtualAlloc(nullptr, 1, win32::MEM_RESERVE, mm::PAGE_READWRITE) == nullptr);
    assert(freeCalls == freesBeforeFailure + 1 && win32::GetLastError() == 5);
    dispatcher.registerSyscall(sys::SSN_NtFreeVirtualMemory, "test cleanup success", 4,
        [](const sys::SyscallFrame&) { return NtStatus::Success; });
    assert(win32::VirtualAlloc(nullptr, 1, win32::MEM_RESERVE, mm::PAGE_READWRITE) == nullptr);
    assert(win32::GetLastError() == 487);
}
