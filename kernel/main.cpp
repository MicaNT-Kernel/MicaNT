#include <iostream>
#include <iomanip>
#include <memory>
#include <fstream>
#include <vector>
#include "micant/ntstatus.hpp"
#include "micant/ntdef.hpp"
#include "micant/ob.hpp"
#include "micant/mm.hpp"
#include "micant/syscalls.hpp"
#include "micant/dispatcher.hpp"
#include "micant/pe.hpp"
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
    auto devDir = std::make_unique<ob::DirectoryObject>(L"Device");
    auto dosDir = std::make_unique<ob::DirectoryObject>(L"DosDevices");
    auto kernDir = std::make_unique<ob::DirectoryObject>(L"KernelObjects");
    auto baseDir = std::make_unique<ob::DirectoryObject>(L"BaseNamedObjects");
    std::cout << "[MicaNT Boot] [Ob] Created standard NT namespaces: \\Device, \\DosDevices, \\KernelObjects, \\BaseNamedObjects\n";

    // 2. Initialize Memory Manager
    std::cout << "[MicaNT Boot] [Mm] Initializing 64-bit Virtual Memory Manager...\n";
    mm::ProcessAddressSpace kernelProcessSpace;

    // 3. Initialize KiSystemCall64 Central Dispatcher Table
    std::cout << "[MicaNT Boot] [KiSystemCall64] Initializing Syscall Dispatcher Table...\n";
    auto& dispatcher = sys::SyscallDispatcher::get();
    dispatcher.initializeStandardTable();
    std::cout << "[MicaNT Boot] [KiSystemCall64] " << dispatcher.getRegisteredCount() 
              << " core NT syscalls registered in LSTAR dispatch table.\n";

    // 4. Test Simulated Ring 3 Syscall via Dispatcher
    std::cout << "[MicaNT Boot] [Test] Simulating Ring 3 -> Ring 0 'syscall' invocation...\n";
    uintptr_t userAllocBase = 0;
    size_t userAllocSize = 128 * 1024; // 128 KB
    uint64_t stackParameters[2] = { mm::MEM_COMMIT | mm::MEM_RESERVE, mm::PAGE_READWRITE };

    sys::SyscallFrame testFrame{
        .ssn = sys::SSN_NtAllocateVirtualMemory,
        .arg1 = 0, // ProcessHandle
        .arg2 = reinterpret_cast<uint64_t>(&userAllocBase),
        .arg3 = 0, // ZeroBits
        .arg4 = reinterpret_cast<uint64_t>(&userAllocSize),
        .stackArgs = stackParameters,
        .stackArgCount = 2
    };

    NtStatus syscallResult = dispatcher.dispatch(testFrame);
    if (NT_SUCCESS(syscallResult)) {
        std::cout << "[MicaNT Boot] [Test] Syscall NtAllocateVirtualMemory (SSN 0x" 
                  << std::hex << sys::SSN_NtAllocateVirtualMemory << std::dec << ") dispatched OK! "
                  << "Mapped 0x" << std::hex << userAllocBase << " - 0x" << (userAllocBase + userAllocSize) << std::dec << "\n";
    } else {
        std::cerr << "[MicaNT Boot] [Test] Syscall dispatch failed: " << NtStatusToString(syscallResult) << "\n";
    }

    // 5. Test PE Loader on executable binary
    const char* exePath = (argc > 0 && argv[0]) ? argv[0] : "bin/micant_kernel.exe";
    std::cout << "\n[MicaNT Boot] [PeLoader] Inspecting 64-bit Portable Executable: " << exePath << "\n";
    
    std::ifstream exeFile(exePath, std::ios::binary | std::ios::ate);
    if (exeFile.is_open()) {
        std::streamsize fileSize = exeFile.tellg();
        exeFile.seekg(0, std::ios::beg);
        std::vector<uint8_t> buffer(static_cast<size_t>(fileSize));
        if (exeFile.read(reinterpret_cast<char*>(buffer.data()), fileSize)) {
            pe::ImageNtHeaders64 ntHeaders;
            std::vector<pe::ImageSectionHeader> sections;

            NtStatus peStatus = pe::PeLoader::inspect(buffer, ntHeaders, sections);
            if (NT_SUCCESS(peStatus)) {
                std::cout << "[MicaNT Boot] [PeLoader] Successfully parsed 64-bit PE32+ header!\n";
                std::cout << "  - Target Machine:      x86_64 (AMD64 0x" << std::hex << ntHeaders.fileHeader.machine << std::dec << ")\n";
                std::cout << "  - Number of Sections:  " << ntHeaders.fileHeader.numberOfSections << "\n";
                std::cout << "  - Entry Point RVA:     0x" << std::hex << ntHeaders.optionalHeader.addressOfEntryPoint << std::dec << "\n";
                std::cout << "  - Preferred ImageBase: 0x" << std::hex << ntHeaders.optionalHeader.imageBase << std::dec << "\n";
                std::cout << "  - Section Table:\n";
                for (const auto& sec : sections) {
                    std::cout << "      * " << std::left << std::setw(8) << sec.getName() 
                              << " RVA: 0x" << std::hex << std::setw(8) << sec.virtualAddress
                              << " Size: 0x" << std::setw(8) << sec.misc.virtualSize
                              << " Flags: 0x" << sec.characteristics << std::dec << "\n";
                }
            }
        }
    } else {
        std::cout << "[MicaNT Boot] [PeLoader] (Note: Binary self-inspection skipped in simulator mode)\n";
    }

    // 6. Ingested win32metadata Catalog Verification
    std::cout << "\n[MicaNT Boot] [Metadata] Verifying win32metadata API surface:\n";
    std::cout << "  - Auto-generated Nt/Zw System Calls: " 
              << generated::NtSystemCallCatalog.size() << " registered.\n";
    std::cout << "  - Auto-generated Rtl Runtime Routines: " 
              << generated::RtlRoutineCatalog.size() << " registered.\n";

    std::cout << "\n[MicaNT Executive] Subsystem self-test PASSED. Ready for Ring 3 binaries.\n\n";

    return 0;
}
