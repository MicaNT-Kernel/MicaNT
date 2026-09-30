#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <span>
#include <memory>
#include "micant/ntstatus.hpp"
#include "micant/ntdef.hpp"
#include "micant/ob.hpp"
#include "micant/mm.hpp"
#include "micant/pe.hpp"
#include "micant/syscalls.hpp"
#include "micant/dispatcher.hpp"
#include "micant/ps.hpp"
#include "micant/section.hpp"

using namespace micant;

static int g_PassedTests = 0;
static int g_FailedTests = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  [FAIL] " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            g_FailedTests++; \
            return; \
        } \
    } while (0)

#define RUN_TEST(fn) \
    do { \
        std::cout << "[RUNNING] " << #fn << "...\n"; \
        int before = g_FailedTests; \
        fn(); \
        if (g_FailedTests == before) { \
            std::cout << "  [PASS] " << #fn << "\n"; \
            g_PassedTests++; \
        } \
    } while (0)

// ============================================================================
// Suite 1: Object Manager Tests
// ============================================================================
void Test_ObjectManager_DirectoryAndHandles() {
    ob::DirectoryObject root(L"\\");
    
    ob::ObjectType fileType{
        .typeName = L"File",
        .typeId = ob::ObjectTypeId::File,
        .totalNumberOfObjects = 1
    };

    auto obj1 = std::make_unique<ob::ObjectHeader>();
    obj1->type = &fileType;
    obj1->objectName = L"PhysicalDrive0";

    // Test directory insert & lookup
    NtStatus insStatus = root.insertObject(L"PhysicalDrive0", obj1.get());
    TEST_ASSERT(NT_SUCCESS(insStatus), "Directory insert should succeed");
    TEST_ASSERT(root.getEntryCount() == 1, "Directory should contain 1 object");

    // Test collision
    NtStatus dupStatus = root.insertObject(L"PhysicalDrive0", obj1.get());
    TEST_ASSERT(dupStatus == NtStatus::ObjectNameCollision, "Duplicate name should return ObjectNameCollision");

    // Test lookup
    ob::ObjectHeader* found = root.lookup(L"PhysicalDrive0");
    TEST_ASSERT(found == obj1.get(), "Lookup should return inserted object");

    // Test Handle Table
    ob::HandleTable handleTable;
    Handle h1 = 0;
    NtStatus hStatus = handleTable.createHandle(obj1.get(), h1);
    TEST_ASSERT(NT_SUCCESS(hStatus), "Handle creation should succeed");
    TEST_ASSERT(h1 == 4, "First handle index should be 4 (NT standard)");

    // Test handle lookup
    ob::ObjectHeader* fromHandle = handleTable.lookup(h1);
    TEST_ASSERT(fromHandle == obj1.get(), "Handle lookup should return correct object");

    // Test handle close
    NtStatus closeStatus = handleTable.closeHandle(h1);
    TEST_ASSERT(NT_SUCCESS(closeStatus), "Closing valid handle should succeed");
    TEST_ASSERT(handleTable.lookup(h1) == nullptr, "Closed handle must look up as nullptr");

    // Test invalid handle close
    NtStatus badClose = handleTable.closeHandle(999);
    TEST_ASSERT(badClose == NtStatus::InvalidHandle, "Closing unallocated handle must return InvalidHandle");
}

// ============================================================================
// Suite 2: Virtual Memory Manager Tests
// ============================================================================
void Test_MemoryManager_VADAllocation() {
    mm::ProcessAddressSpace space;

    uintptr_t addr1 = 0;
    size_t size1 = 16 * 1024; // 16 KB
    NtStatus status1 = space.allocate(addr1, size1, mm::MEM_COMMIT | mm::MEM_RESERVE, mm::PAGE_READWRITE);
    TEST_ASSERT(NT_SUCCESS(status1), "Allocation of 16KB should succeed");
    TEST_ASSERT(addr1 >= mm::ProcessAddressSpace::UserSpaceMin, "Address must be within user space bounds");
    TEST_ASSERT(space.getRegionCount() == 1, "Should have 1 active VAD");

    // Allocate second region
    uintptr_t addr2 = 0;
    size_t size2 = 64 * 1024; // 64 KB
    NtStatus status2 = space.allocate(addr2, size2, mm::MEM_COMMIT, mm::PAGE_EXECUTE_READ);
    TEST_ASSERT(NT_SUCCESS(status2), "Second allocation should succeed");
    TEST_ASSERT(addr2 != addr1, "Addresses must not collide");
    TEST_ASSERT(space.getRegionCount() == 2, "Should have 2 active VADs");

    // Free first region
    NtStatus freeStatus = space.free(addr1, size1, mm::MEM_RELEASE);
    TEST_ASSERT(NT_SUCCESS(freeStatus), "Releasing memory region should succeed");
    TEST_ASSERT(space.getRegionCount() == 1, "Should have 1 active VAD remaining");

    // Invalid free
    NtStatus badFree = space.free(0xDEADBEEF, 4096, mm::MEM_RELEASE);
    TEST_ASSERT(!NT_SUCCESS(badFree), "Freeing nonexistent address must fail");
}

// ============================================================================
// Suite 3: PE32+ Parser & Loader Tests
// ============================================================================
void Test_PeLoader_ValidAndCorruptedHeaders() {
    // 1. Test corrupt buffer (too small)
    uint8_t tinyBuf[16] = {0};
    pe::ImageNtHeaders64 ntHeaders;
    std::vector<pe::ImageSectionHeader> sections;
    NtStatus smallStatus = pe::PeLoader::inspect(tinyBuf, ntHeaders, sections);
    TEST_ASSERT(!NT_SUCCESS(smallStatus), "Too small buffer should fail inspect");

    // 2. Test synthetic valid PE64 header structure
    std::vector<uint8_t> peBuffer(sizeof(pe::ImageDosHeader) + sizeof(pe::ImageNtHeaders64) + sizeof(pe::ImageSectionHeader), 0);
    auto* dos = reinterpret_cast<pe::ImageDosHeader*>(peBuffer.data());
    dos->e_magic = pe::DOS_MAGIC;
    dos->e_lfanew = sizeof(pe::ImageDosHeader);

    auto* nt = reinterpret_cast<pe::ImageNtHeaders64*>(peBuffer.data() + dos->e_lfanew);
    nt->signature = pe::NT_SIGNATURE;
    nt->fileHeader.machine = pe::MACHINE_AMD64;
    nt->fileHeader.numberOfSections = 1;
    nt->fileHeader.sizeOfOptionalHeader = sizeof(pe::ImageOptionalHeader64);
    nt->optionalHeader.magic = pe::PE32PLUS_MAGIC;
    nt->optionalHeader.addressOfEntryPoint = 0x1000;
    nt->optionalHeader.imageBase = 0x140000000;

    auto* sec = reinterpret_cast<pe::ImageSectionHeader*>(
        peBuffer.data() + dos->e_lfanew + sizeof(uint32_t) + sizeof(pe::ImageFileHeader) + nt->fileHeader.sizeOfOptionalHeader
    );
    std::memcpy(sec->name, ".text\0\0\0", 8);
    sec->virtualAddress = 0x1000;
    sec->misc.virtualSize = 0x2000;
    sec->characteristics = pe::IMAGE_SCN_MEM_EXECUTE | pe::IMAGE_SCN_MEM_READ;

    NtStatus validStatus = pe::PeLoader::inspect(peBuffer, ntHeaders, sections);
    TEST_ASSERT(NT_SUCCESS(validStatus), "Valid PE64 buffer must parse successfully");
    TEST_ASSERT(ntHeaders.fileHeader.machine == pe::MACHINE_AMD64, "Machine must be AMD64");
    TEST_ASSERT(sections.size() == 1, "Must contain 1 section");
    TEST_ASSERT(sections[0].getName() == ".text", "Section name must match .text");
}

// ============================================================================
// Suite 4: KiSystemCall64 Central Dispatcher Tests
// ============================================================================
void Test_SyscallDispatcher_DispatchFlow() {
    auto& dispatcher = sys::SyscallDispatcher::get();
    dispatcher.initializeStandardTable();

    TEST_ASSERT(dispatcher.getRegisteredCount() >= 6, "Standard table must have at least 6 syscalls");

    // Test valid dispatch: NtAllocateVirtualMemory (SSN 0x18)
    uintptr_t base = 0;
    size_t size = 32 * 1024;
    uint64_t stackParams[2] = { mm::MEM_COMMIT | mm::MEM_RESERVE, mm::PAGE_READWRITE };

    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtAllocateVirtualMemory,
        .arg1 = 0,
        .arg2 = reinterpret_cast<uint64_t>(&base),
        .arg3 = 0,
        .arg4 = reinterpret_cast<uint64_t>(&size),
        .stackArgs = stackParams,
        .stackArgCount = 2
    };

    NtStatus res = dispatcher.dispatch(frame);
    TEST_ASSERT(NT_SUCCESS(res), "Dispatching NtAllocateVirtualMemory should succeed");
    TEST_ASSERT(base != 0, "Allocated base address must be nonzero");

    // Test invalid SSN
    sys::SyscallFrame badFrame{ .ssn = 0xFFFF };
    NtStatus badRes = dispatcher.dispatch(badFrame);
    TEST_ASSERT(badRes == NtStatus::InvalidDeviceRequest, "Unregistered SSN must return InvalidDeviceRequest");
}

// ============================================================================
// Suite 5: Process, Thread, and Section Objects Tests
// ============================================================================
void Test_ProcessAndSectionManager() {
    auto& pm = ps::ProcessManager::get();
    auto proc = pm.createProcess(L"TestProcess.exe");
    TEST_ASSERT(proc != nullptr, "Process creation must return valid EProcess pointer");
    TEST_ASSERT(proc->getPid() >= 1000, "PID must start in executive range >= 1000");
    TEST_ASSERT(proc->getPebAddress() != 0, "PEB must be mapped at nonzero address");

    // Create Thread
    uintptr_t testEntryPoint = 0x00007FF710001000ULL;
    auto thread = proc->createThread(testEntryPoint);
    TEST_ASSERT(thread != nullptr, "Thread creation must succeed");
    TEST_ASSERT(thread->getContext().rip == testEntryPoint, "Thread RIP must match entry point");
    TEST_ASSERT(thread->getState() == ps::ThreadState::Initialized, "Initial thread state must be Initialized");

    // Create Section Object
    auto& sm = section::SectionManager::get();
    Handle secHandle = 0;
    std::shared_ptr<section::SectionObject> secObj;
    NtStatus secStatus = sm.createSection(
        secHandle,
        proc->getHandleTable(),
        256 * 1024, // 256 KB
        section::SEC_COMMIT,
        mm::PAGE_READWRITE,
        secObj
    );
    TEST_ASSERT(NT_SUCCESS(secStatus), "Section creation must succeed");
    TEST_ASSERT(secObj != nullptr, "Section object pointer must be valid");

    // Map View of Section into target process
    uintptr_t viewBase = 0;
    size_t viewSize = 0;
    NtStatus mapStatus = sm.mapViewOfSection(
        secObj,
        *proc,
        viewBase,
        viewSize,
        mm::MEM_COMMIT,
        mm::PAGE_READWRITE
    );
    TEST_ASSERT(NT_SUCCESS(mapStatus), "Mapping view of section must succeed");
    TEST_ASSERT(viewBase != 0, "Mapped view base must be nonzero");
    TEST_ASSERT(viewSize == 256 * 1024, "Mapped view size must match section size");

    // Unmap View
    NtStatus unmapStatus = sm.unmapViewOfSection(*proc, viewBase);
    TEST_ASSERT(NT_SUCCESS(unmapStatus), "Unmapping section view must succeed");

    // Terminate Process
    proc->terminate(NtStatus::Success);
    TEST_ASSERT(proc->isTerminated(), "Process must be marked terminated");
    TEST_ASSERT(thread->getState() == ps::ThreadState::Terminated, "Threads must be transitioned to Terminated");
}

int main() {
    std::cout << "========================================================================\n";
    std::cout << "                   MicaNT Executive Unit Test Suite                     \n";
    std::cout << "========================================================================\n\n";

    RUN_TEST(Test_ObjectManager_DirectoryAndHandles);
    RUN_TEST(Test_MemoryManager_VADAllocation);
    RUN_TEST(Test_PeLoader_ValidAndCorruptedHeaders);
    RUN_TEST(Test_SyscallDispatcher_DispatchFlow);
    RUN_TEST(Test_ProcessAndSectionManager);

    std::cout << "\n------------------------------------------------------------------------\n";
    std::cout << "Summary: " << g_PassedTests << " Passed, " << g_FailedTests << " Failed\n";
    std::cout << "------------------------------------------------------------------------\n";

    return (g_FailedTests == 0) ? 0 : 1;
}
