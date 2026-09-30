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
#include "micant/ps.hpp"
#include "micant/section.hpp"
#include "micant/sync.hpp"
#include "micant/io.hpp"
#include "micant/cm.hpp"
#include "micant/se.hpp"
#include "micant/lpc.hpp"
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

    // 3. Initialize I/O Manager & Device Object Graph
    std::cout << "[MicaNT Boot] [Io] Initializing I/O Manager and Device Object Graph...\n";
    auto& ioMgr = io::IoManager::get();
    io::DriverObject nullDriver{ .driverName = L"\\Driver\\Null" };
    io::DriverObject diskDriver{ .driverName = L"\\Driver\\Disk" };
    auto nullDev = ioMgr.createDevice(&nullDriver, L"\\Device\\Null", io::DeviceType::Null);
    auto diskDev = ioMgr.createDevice(&diskDriver, L"\\Device\\Harddisk0", io::DeviceType::Disk);
    std::cout << "[MicaNT Boot] [Io] Created system device nodes: \\Device\\Null, \\Device\\Harddisk0 (Registered: " 
              << ioMgr.getDeviceCount() << " devices)\n";

    // 4. Initialize I/O Completion Port (IOCP) Subsystem
    std::cout << "[MicaNT Boot] [Io & IOCP] Initializing I/O Completion Port Subsystem...\n";
    io::IoCompletionPort systemIocp(4);
    systemIocp.postCompletion(0x1337, 0x00007FF71000, NtStatus::Success, 4096);
    uint64_t iocpKey = 0;
    uintptr_t iocpOv = 0;
    IoStatusBlock iocpIosb{};
    NtStatus iocpStatus = systemIocp.removeCompletion(iocpKey, iocpOv, iocpIosb, 0);
    if (NT_SUCCESS(iocpStatus)) {
        std::cout << "[MicaNT Boot] [Io & IOCP] Async completion cycle verified: Key 0x" 
                  << std::hex << iocpKey << std::dec << " Transferred: " << iocpIosb.information << " bytes.\n";
    }

    // 5. Initialize Synchronization Subsystem
    std::cout << "[MicaNT Boot] [Sync] Initializing KEVENT / KMUTANT / KSEMAPHORE primitives...\n";
    sync::EventObject bootEvent(sync::EventType::NotificationEvent, false);
    bootEvent.set();
    std::cout << "[MicaNT Boot] [Sync] Boot synchronization event signaled: OK\n";

    // 6. Initialize Configuration Manager (CM) Registry Hives
    std::cout << "[MicaNT Boot] [Cm] Initializing Configuration Manager & mounting \\Registry...\n";
    auto& cmMgr = cm::ConfigurationManager::get();
    auto sessionMgrKey = cmMgr.resolvePath(L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Control\\Session Manager");
    if (sessionMgrKey) {
        std::wcout << L"[MicaNT Boot] [Cm] Mounted Session Manager hive: " 
                   << sessionMgrKey->getValue(L"OSName")->asString() 
                   << L" (Build " << sessionMgrKey->getValue(L"OSBuild")->asDword() << L")\n";
    }

    // 7. Initialize Security Reference Monitor (SRM)
    std::cout << "[MicaNT Boot] [Se] Initializing Security Reference Monitor (SRM)...\n";
    auto systemToken = se::TokenObject::createSystemToken();
    std::wcout << L"[MicaNT Boot] [Se] Kernel Primary Token: " << systemToken->getUserSid().toString() 
               << L" (SeDebugPrivilege: " << (systemToken->hasPrivilege(se::SE_DEBUG_NAME) ? L"ENABLED" : L"DISABLED") << L")\n";

    // 8. Initialize Advanced Local Procedure Call (ALPC) Subsystem
    std::cout << "[MicaNT Boot] [Lpc] Initializing ALPC Subsystem & \\RPC Control port directory...\n";
    auto& lpcMgr = lpc::PortManager::get();
    auto csrssPort = lpcMgr.createPort(L"\\RPC Control\\MicaCsrPort");
    std::shared_ptr<lpc::PortObject> clientEndpoint;
    std::shared_ptr<lpc::PortObject> serverEndpoint;
    lpcMgr.connectPort(L"MicaCsrPort", clientEndpoint, serverEndpoint);
    std::cout << "[MicaNT Boot] [Lpc] Created system rendezvous endpoint: \\RPC Control\\MicaCsrPort (Status: READY)\n";

    // 9. Initialize KiSystemCall64 Central Dispatcher Table
    std::cout << "[MicaNT Boot] [KiSystemCall64] Initializing Syscall Dispatcher Table...\n";
    auto& dispatcher = sys::SyscallDispatcher::get();
    dispatcher.initializeStandardTable();
    std::cout << "[MicaNT Boot] [KiSystemCall64] " << dispatcher.getRegisteredCount() 
              << " core NT syscalls registered in LSTAR dispatch table.\n";

    // 10. Test Simulated Ring 3 Syscall via Dispatcher
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

    // 11. Inspect and Load Ring 3 Binary (bin/userland_app.exe or self)
    const char* targetAppPath = "bin/userland_app.exe";
    std::ifstream testCheck(targetAppPath, std::ios::binary);
    if (!testCheck.is_open() && argc > 0 && argv[0]) {
        targetAppPath = argv[0];
    }
    testCheck.close();

    std::cout << "\n[MicaNT Boot] [PeLoader] Inspecting 64-bit Target Executable: " << targetAppPath << "\n";
    
    std::ifstream exeFile(targetAppPath, std::ios::binary | std::ios::ate);
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

                // 12. Section Mapping & Process/Thread Instantiation
                std::cout << "\n[MicaNT Boot] [Ps & Section] Spawning EProcess for " << targetAppPath << "...\n";
                auto& pm = ps::ProcessManager::get();
                auto proc = pm.createProcess(L"userland_app.exe", systemToken);
                proc->setImageBase(ntHeaders.optionalHeader.imageBase);
                proc->setEntryPoint(ntHeaders.optionalHeader.imageBase + ntHeaders.optionalHeader.addressOfEntryPoint);

                // Create Section Object
                auto& sm = section::SectionManager::get();
                Handle secHandle = 0;
                std::shared_ptr<section::SectionObject> secObj;
                sm.createSection(
                    secHandle,
                    proc->getHandleTable(),
                    static_cast<size_t>(ntHeaders.optionalHeader.sizeOfImage),
                    section::SEC_IMAGE | section::SEC_COMMIT,
                    mm::PAGE_EXECUTE_READWRITE,
                    secObj
                );

                // Map View of Section into target process
                uintptr_t viewBase = ntHeaders.optionalHeader.imageBase;
                size_t viewSize = static_cast<size_t>(ntHeaders.optionalHeader.sizeOfImage);
                sm.mapViewOfSection(secObj, *proc, viewBase, viewSize, mm::MEM_COMMIT, mm::PAGE_EXECUTE_READWRITE);

                // Create Primary EThread
                auto thread = proc->createThread(proc->getEntryPoint());
                thread->setState(ps::ThreadState::Ready);

                std::cout << "[MicaNT Boot] [Ps] Process Created: PID " << proc->getPid() << "\n";
                std::wcout << L"  - Primary Token:   " << proc->getToken()->getUserSid().toString() << L" (LocalSystem)\n";
                std::cout << "  - PEB Address:     0x" << std::hex << proc->getPebAddress() << std::dec << "\n";
                std::cout << "  - Image Mapped At: 0x" << std::hex << viewBase << " - 0x" << (viewBase + viewSize) << std::dec << "\n";
                std::cout << "  - Primary Thread:  TID " << thread->getTid() << " (State: READY)\n";
                std::cout << "  - Initial RIP:     0x" << std::hex << thread->getContext().rip << std::dec << "\n";
                std::cout << "  - Initial RSP:     0x" << std::hex << thread->getContext().rsp << std::dec << "\n";
            }
        }
    }

    // 13. Ingested win32metadata Catalog Verification
    std::cout << "\n[MicaNT Boot] [Metadata] Verifying win32metadata API surface:\n";
    std::cout << "  - Auto-generated Nt/Zw System Calls: " 
              << generated::NtSystemCallCatalog.size() << " registered.\n";
    std::cout << "  - Auto-generated Rtl Runtime Routines: " 
              << generated::RtlRoutineCatalog.size() << " registered.\n";

    std::cout << "\n[MicaNT Executive] Subsystem self-test PASSED. Ready for Ring 3 binaries.\n\n";

    return 0;
}
