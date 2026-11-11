#include "micant/km/core/syscalls.hpp"

#include <cassert>

int main() {
    using namespace micant;
    using namespace micant::sys;

    assert(NtTerminateProcess(0, NtStatus::Success) == NtStatus::NotImplemented);

    ProcessBasicInformation information{};
    information.uniqueProcessId = 42;
    uint32_t returned = 123;
    assert(NtQueryInformationProcess(0, ProcessInformationClass::ProcessBasicInformation,
                                     &information, sizeof(information), &returned)
           == NtStatus::NotImplemented);
    assert(information.uniqueProcessId == 42 && returned == 123);
    assert(NtQueryInformationProcess(0, ProcessInformationClass::ProcessBasicInformation,
                                     &information, sizeof(information) - 1, &returned)
           == NtStatus::InfoLengthMismatch);

    uintptr_t address = 0x10000;
    size_t size = 4096;
    uint32_t oldProtection = 99;
    assert(NtProtectVirtualMemory(0, &address, &size, 0x04, &oldProtection)
           == NtStatus::NotImplemented);
    assert(address == 0x10000 && size == 4096 && oldProtection == 99);
    assert(NtProtectVirtualMemory(0, nullptr, &size, 0x04, &oldProtection)
           == NtStatus::InvalidParameter);

    Handle section = 0;
    assert(NtCreateSection(&section, 0, nullptr, nullptr, 0x04, 0, 0) == NtStatus::Success);
    uintptr_t view = 0;
    size_t viewSize = 4096;
    assert(NtMapViewOfSection(section, 0, &view, 0, 0, nullptr, &viewSize, 0, 0, 0x04)
           == NtStatus::NotImplemented);
    assert(view == 0 && viewSize == 4096);
}
