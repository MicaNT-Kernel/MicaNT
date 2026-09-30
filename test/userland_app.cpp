/**
 * MicaNT Standalone Ring 3 Userland Test Application
 * Built using MicaNT's clean-room ntdll.hpp userland runtime.
 */

#include <iostream>
#include <cstring>
#include "micant/ntstatus.hpp"
#include "micant/ntdef.hpp"
#include "micant/ntdll.hpp"

using namespace micant;

int main() {
    std::cout << "========================================================================\n";
    std::cout << "               MicaNT Ring 3 Userland Test Application                  \n";
    std::cout << "========================================================================\n\n";

    // 1. Initialize userland execution environment via LdrInitializeThunk
    ps::Peb userPeb{};
    ps::Teb userTeb{};
    userPeb.imageBaseAddress = 0x0000000140000000ULL;

    NtStatus ldrStatus = ntdll::LdrInitializeThunk(&userPeb, &userTeb, 0x0000000140001000ULL);
    if (!NT_SUCCESS(ldrStatus)) {
        std::cerr << "[Userland App] [FAIL] LdrInitializeThunk failed!\n";
        return 1;
    }
    ntdll::RtlSetCurrentTeb(&userTeb);
    std::cout << "[Userland App] [Ldr] LdrInitializeThunk initialized PEB & TEB successfully.\n";
    std::cout << "  - Process Heap: 0x" << std::hex << userPeb.processHeap << std::dec << "\n";
    std::cout << "  - Loaded Modules: " << ldr::DynamicLoader::get().getLoadedModuleCount() << " registered\n";

    // 2. Allocate memory from Default Process Heap via RtlAllocateHeap
    void* heapHandle = reinterpret_cast<void*>(userPeb.processHeap);
    char* buffer = static_cast<char*>(ntdll::RtlAllocateHeap(heapHandle, ntdll::HEAP_ZERO_MEMORY, 256));
    if (!buffer) {
        std::cerr << "[Userland App] [FAIL] RtlAllocateHeap failed!\n";
        return 2;
    }
    std::cout << "[Userland App] [Heap] RtlAllocateHeap allocated 256 bytes at 0x" 
              << static_cast<void*>(buffer) << "\n";

    const char message[] = "[Ring 3 Userland] Hello from MicaNT Userland Subsystem via clean-room ntdll.dll!\n";
    std::memcpy(buffer, message, sizeof(message));
    std::cout << "[Userland App] [Heap] Buffer payload: " << buffer;

    // 3. Write via NtWriteFile to stdout handle (0x14)
    IoStatusBlock iosb{};
    NtStatus writeStatus = ntdll::NtWriteFile(
        0x14, // standardOutput handle
        0, nullptr, nullptr,
        &iosb,
        buffer,
        static_cast<uint32_t>(std::strlen(buffer))
    );
    std::cout << "[Userland App] [Io] NtWriteFile status: " 
              << (NT_SUCCESS(writeStatus) ? "STATUS_SUCCESS" : "STATUS_UNSUCCESSFUL") 
              << " (" << iosb.information << " bytes written)\n";

    // 4. Test Userland Virtual Memory Allocation via NtAllocateVirtualMemory
    uintptr_t vmAddr = 0;
    size_t vmSize = 64 * 1024; // 64 KB
    NtStatus vmStatus = ntdll::NtAllocateVirtualMemory(
        0, // Current process
        &vmAddr,
        0,
        &vmSize,
        mm::MEM_COMMIT | mm::MEM_RESERVE,
        mm::PAGE_READWRITE
    );
    if (NT_SUCCESS(vmStatus)) {
        std::cout << "[Userland App] [Mm] NtAllocateVirtualMemory succeeded: 64 KB allocated at 0x" 
                  << std::hex << vmAddr << std::dec << "\n";
        ntdll::NtFreeVirtualMemory(0, &vmAddr, &vmSize, mm::MEM_RELEASE);
        std::cout << "[Userland App] [Mm] NtFreeVirtualMemory released region cleanly.\n";
    }

    // 5. Free Heap Memory via RtlFreeHeap
    bool freeOk = ntdll::RtlFreeHeap(heapHandle, 0, buffer);
    std::cout << "[Userland App] [Heap] RtlFreeHeap returned: " << (freeOk ? "SUCCESS" : "FAILED") << "\n";

    std::cout << "\n[Userland App] Exiting cleanly with status code 42.\n";
    ntdll::NtTerminateProcess(0, static_cast<NtStatus>(42));
    return 42;
}
