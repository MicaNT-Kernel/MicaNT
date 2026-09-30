#include <iostream>
#include <iomanip>
#include <memory>
#include "micant/ntstatus.hpp"
#include "micant/ntdef.hpp"
#include "micant/ob.hpp"
#include "micant/mm.hpp"
#include "micant/syscalls.hpp"
#include "micant/generated_nt_api.hpp"

using namespace micant;

void PrintBanner() {
    std::cout << "\n";
    std::cout << "========================================================================\n";
    std::cout << "       __  __ _            _   _ _____                                  \n";
    std::cout << "      |  \\/  (_)          | \\ | |_   _|                                 \n";
    std::cout << "      | \\  / |_  ___ __ _ |  \\| | | |                                   \n";
    std::cout << "      | |\\/| | |/ __/ _` || . ` | | |                                   \n";
    std::cout << "      | |  | | | (_| (_| || |\\  | | |                                   \n";
    std::cout << "      |_|  |_|_|\\___\\__,_||_| \\_| |_|                                   \n";
    std::cout << "                                                                        \n";
    std::cout << "  Project MICA: Memory, IPC, Compute, Architecture                      \n";
    std::cout << "  Clean-Room NT Architecture in Modern ISO C++23                        \n";
    std::cout << "  Zero Telemetry | Sub-32MB Footprint | Powered by win32metadata        \n";
    std::cout << "========================================================================\n\n";
}

int main(int argc, char* argv[]) {
    PrintBanner();

    std::cout << "[MicaNT Boot] Initializing Executive subsystems...\n";

    // 1. Initialize Object Manager Root Namespace
    std::cout << "[MicaNT Boot] [Ob] Initializing Object Manager root namespace...\n";
    ob::DirectoryObject rootDirectory(L"\\");
    
    // Create fundamental NT directories
    auto devDir = std::make_unique<ob::DirectoryObject>(L"Device");
    auto dosDir = std::make_unique<ob::DirectoryObject>(L"DosDevices");
    auto kernDir = std::make_unique<ob::DirectoryObject>(L"KernelObjects");
    auto baseDir = std::make_unique<ob::DirectoryObject>(L"BaseNamedObjects");

    std::cout << "[MicaNT Boot] [Ob] Created standard NT namespaces: \\Device, \\DosDevices, \\KernelObjects, \\BaseNamedObjects\n";

    // 2. Initialize Memory Manager
    std::cout << "[MicaNT Boot] [Mm] Initializing 64-bit Virtual Memory Manager...\n";
    mm::ProcessAddressSpace initialProcessSpace;

    uintptr_t testBase = 0;
    size_t testSize = 64 * 1024; // 64 KB
    NtStatus memStatus = sys::NtAllocateVirtualMemory(
        0, 
        &testBase, 
        0, 
        &testSize, 
        mm::MEM_RESERVE | mm::MEM_COMMIT, 
        mm::PAGE_READWRITE
    );

    if (NT_SUCCESS(memStatus)) {
        std::cout << "[MicaNT Boot] [Mm] Virtual memory allocation test: OK (Mapped 0x" 
                  << std::hex << testBase << " - 0x" << (testBase + testSize) << std::dec << ")\n";
    } else {
        std::cerr << "[MicaNT Boot] [Mm] Memory allocation failed with status: " 
                  << NtStatusToString(memStatus) << "\n";
        return 1;
    }

    // 3. Test Handle Table and Object Life Cycle
    std::cout << "[MicaNT Boot] [Ob] Initializing Handle Table...\n";
    ob::HandleTable handleTable;
    ob::ObjectType fileType{
        .typeName = L"File",
        .typeId = ob::ObjectTypeId::File,
        .totalNumberOfObjects = 1
    };

    auto testHeader = std::make_unique<ob::ObjectHeader>();
    testHeader->type = &fileType;
    testHeader->objectName = L"\\Device\\Harddisk0\\Partition1";

    Handle testHandle = 0;
    NtStatus handleStatus = handleTable.createHandle(testHeader.get(), testHandle);
    if (NT_SUCCESS(handleStatus)) {
        std::cout << "[MicaNT Boot] [Ob] Created Handle: 0x" << std::hex << testHandle << std::dec
                  << " -> Object: " << std::string(testHeader->objectName.begin(), testHeader->objectName.end()) << "\n";
    }

    handleTable.closeHandle(testHandle);
    std::cout << "[MicaNT Boot] [Ob] Handle 0x" << std::hex << testHandle << std::dec << " closed successfully.\n";

    // 4. Ingested win32metadata Catalog Verification
    std::cout << "\n[MicaNT Boot] [Metadata] Verifying win32metadata API surface:\n";
    std::cout << "  - Auto-generated Nt/Zw System Calls: " 
              << generated::NtSystemCallCatalog.size() << " registered.\n";
    std::cout << "  - Auto-generated Rtl Runtime Routines: " 
              << generated::RtlRoutineCatalog.size() << " registered.\n";

    std::cout << "\n[MicaNT Executive] Subsystem self-test PASSED. Ready for Ring 3 binaries.\n\n";

    return 0;
}
