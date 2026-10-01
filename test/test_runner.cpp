#include <iostream>
#include <cassert>
#include <cstring>
#include <vector>
#include <string>
#include <span>
#include <memory>
#include <thread>
#include <fstream>
#include "micant/ntstatus.hpp"
#include "micant/ntdef.hpp"
#include "micant/ob.hpp"
#include "micant/mm.hpp"
#include "micant/pe.hpp"
#include "micant/syscalls.hpp"
#include "micant/dispatcher.hpp"
#include "micant/ps.hpp"
#include "micant/section.hpp"
#include "micant/sync.hpp"
#include "micant/io.hpp"
#include "micant/cm.hpp"
#include "micant/se.hpp"
#include "micant/lpc.hpp"
#include "micant/ke.hpp"
#include "micant/ex.hpp"
#include "micant/trap.hpp"
#include "micant/hal.hpp"
#include "micant/fs.hpp"
#include "micant/ex_work.hpp"
#include "micant/boot.hpp"
#include "micant/probe.hpp"
#include "micant/driver.hpp"
#include "micant/timer.hpp"
#include "micant/lookaside.hpp"
#include "micant/po.hpp"
#include "micant/heap.hpp"
#include "micant/ldr.hpp"
#include "micant/ntdll.hpp"
#include "micant/uefi.hpp"
#include "micant/bootvid.hpp"
#include "micant/csrss.hpp"
#include "micant/conhost.hpp"
#include "micant/kernel32.hpp"
#include "micant/wow64.hpp"
#include "micant/cpu.hpp"
#include "unmodified_fixture.hpp"

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
        std::cout << "[RUNNING] " << #fn << "...\n" << std::flush; \
        int before = g_FailedTests; \
        fn(); \
        if (g_FailedTests == before) { \
            std::cout << "  [PASS] " << #fn << "\n" << std::flush; \
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

// ============================================================================
// Suite 6: Synchronization Primitives Tests (KEVENT, KMUTANT, KSEMAPHORE)
// ============================================================================
void Test_SynchronizationPrimitives() {
    // 1. Notification Event (Manual Reset)
    sync::EventObject notifEvent(sync::EventType::NotificationEvent, false);
    TEST_ASSERT(!notifEvent.isSignaled(), "Event should initially be unsignaled");
    notifEvent.set();
    TEST_ASSERT(notifEvent.isSignaled(), "Event should be signaled after set()");
    TEST_ASSERT(notifEvent.wait(0), "Wait on signaled notification event should succeed without timeout");
    TEST_ASSERT(notifEvent.isSignaled(), "Notification event should remain signaled after wait");
    notifEvent.reset();
    TEST_ASSERT(!notifEvent.isSignaled(), "Event should be unsignaled after reset()");

    // 2. Synchronization Event (Auto Reset)
    sync::EventObject syncEvent(sync::EventType::SynchronizationEvent, false);
    syncEvent.set();
    TEST_ASSERT(syncEvent.wait(0), "Wait on signaled synchronization event should succeed");
    TEST_ASSERT(!syncEvent.isSignaled(), "Synchronization event must auto-reset after wait");

    // 3. Mutant Object (Recursive Mutex)
    sync::MutantObject mutant(false);
    TEST_ASSERT(mutant.acquire(1, 0), "Thread 1 must acquire free mutant");
    TEST_ASSERT(mutant.acquire(1, 0), "Thread 1 must recursively acquire mutant");
    TEST_ASSERT(mutant.getRecursionCount() == 2, "Recursion count should be 2");
    TEST_ASSERT(!mutant.acquire(2, 0), "Thread 2 must fail to acquire held mutant with timeout 0");
    TEST_ASSERT(!mutant.release(2), "Thread 2 must not be able to release Thread 1's mutant");
    TEST_ASSERT(mutant.release(1), "Thread 1 releases recursion 1");
    TEST_ASSERT(mutant.release(1), "Thread 1 releases recursion 2 (mutant now free)");
    TEST_ASSERT(mutant.getOwner() == 0, "Mutant must be unowned");

    // 4. Semaphore Object
    sync::SemaphoreObject sem(1, 3);
    TEST_ASSERT(sem.wait(0), "Wait on semaphore with count 1 should succeed");
    TEST_ASSERT(sem.getCount() == 0, "Count should be 0 after wait");
    TEST_ASSERT(!sem.wait(0), "Wait on exhausted semaphore should fail");
    int32_t prev = 0;
    TEST_ASSERT(sem.release(2, &prev), "Releasing 2 counts should succeed");
    TEST_ASSERT(prev == 0, "Previous count should be 0");
    TEST_ASSERT(sem.getCount() == 2, "Current count should be 2");
    TEST_ASSERT(!sem.release(2), "Exceeding max count (3) should fail");
}

// ============================================================================
// Suite 7: I/O Request Packet (IRP) & I/O Completion Port (IOCP) Tests
// ============================================================================
void Test_IoAndCompletionPorts() {
    auto& ioMgr = io::IoManager::get();

    // 1. Driver and Device Object Graph
    io::DriverObject nullDriver;
    nullDriver.driverName = L"\\Driver\\Null";
    nullDriver.setDispatch(io::IRP_MJ_CREATE, [](io::DeviceObject*, io::Irp* irp) -> NtStatus {
        irp->ioStatus.status = NtStatus::Success;
        return NtStatus::Success;
    });
    nullDriver.setDispatch(io::IRP_MJ_WRITE, [](io::DeviceObject*, io::Irp* irp) -> NtStatus {
        irp->ioStatus.status = NtStatus::Success;
        irp->ioStatus.information = irp->length; // Discard bytes successfully
        return NtStatus::Success;
    });

    auto nullDev = ioMgr.createDevice(&nullDriver, L"\\Device\\Null", io::DeviceType::Null);
    TEST_ASSERT(nullDev != nullptr, "Null device creation should succeed");

    auto lookedUp = ioMgr.lookupDevice(L"\\Device\\Null");
    TEST_ASSERT(lookedUp == nullDev, "Device lookup in \\Device should succeed");

    // Dispatch IRP_MJ_WRITE
    io::Irp testIrp{
        .majorFunction = io::IRP_MJ_WRITE,
        .deviceObject = nullDev.get(),
        .length = 1024
    };
    NtStatus irpStatus = nullDriver.dispatch(nullDev.get(), &testIrp);
    TEST_ASSERT(NT_SUCCESS(irpStatus), "Dispatched IRP_MJ_WRITE must succeed");
    TEST_ASSERT(testIrp.ioStatus.information == 1024, "IRP information must report 1024 bytes processed");

    // 2. I/O Completion Port (IOCP)
    io::IoCompletionPort iocp(4);
    TEST_ASSERT(iocp.getQueuedCount() == 0, "New IOCP must have 0 queued packets");

    // Post completions
    NtStatus post1 = iocp.postCompletion(0x1337, 0x00007FF71000, NtStatus::Success, 4096);
    TEST_ASSERT(NT_SUCCESS(post1), "Posting completion 1 must succeed");
    NtStatus post2 = iocp.postCompletion(0xABCD, 0x00007FF72000, NtStatus::BufferOverflow, 512);
    TEST_ASSERT(NT_SUCCESS(post2), "Posting completion 2 must succeed");
    TEST_ASSERT(iocp.getQueuedCount() == 2, "IOCP should have 2 queued packets");

    // Remove completions in FIFO order
    uint64_t key1 = 0;
    uintptr_t ov1 = 0;
    IoStatusBlock iosb1{};
    NtStatus rem1 = iocp.removeCompletion(key1, ov1, iosb1, 0);
    TEST_ASSERT(NT_SUCCESS(rem1), "Removing packet 1 must succeed");
    TEST_ASSERT(key1 == 0x1337, "Key 1 must match 0x1337");
    TEST_ASSERT(ov1 == 0x00007FF71000, "Overlapped 1 must match");
    TEST_ASSERT(iosb1.information == 4096, "Bytes transferred must match 4096");

    uint64_t key2 = 0;
    uintptr_t ov2 = 0;
    IoStatusBlock iosb2{};
    NtStatus rem2 = iocp.removeCompletion(key2, ov2, iosb2, 0);
    TEST_ASSERT(rem2 == NtStatus::BufferOverflow, "Packet 2 status must match BufferOverflow");
    TEST_ASSERT(key2 == 0xABCD, "Key 2 must match 0xABCD");
    TEST_ASSERT(iosb2.information == 512, "Bytes transferred must match 512");

    // Check empty queue with timeout
    uint64_t emptyKey = 0;
    uintptr_t emptyOv = 0;
    IoStatusBlock emptyIosb{};
    NtStatus emptyRem = iocp.removeCompletion(emptyKey, emptyOv, emptyIosb, 0);
    TEST_ASSERT(emptyRem == NtStatus::Timeout, "Removing from empty IOCP must return Timeout");
}

// ============================================================================
// Suite 8: Configuration Manager (CM) Registry Hive & Path Resolution Tests
// ============================================================================
void Test_ConfigurationManager_HiveAndValues() {
    auto& cm = cm::ConfigurationManager::get();
    auto root = cm.getRootKey();
    TEST_ASSERT(root != nullptr, "Registry root key must not be null");
    TEST_ASSERT(root->getName() == L"\\Registry", "Root key name must be \\Registry");

    // Resolve standard NT Session Manager path
    auto sessionMgr = cm.resolvePath(L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Control\\Session Manager");
    TEST_ASSERT(sessionMgr != nullptr, "Session Manager key path must resolve successfully");

    // Verify predefined values
    const auto* osNameVal = sessionMgr->getValue(L"OSName");
    TEST_ASSERT(osNameVal != nullptr, "OSName value must exist");
    TEST_ASSERT(osNameVal->type == cm::RegType::Sz, "OSName must be REG_SZ");
    TEST_ASSERT(osNameVal->asString() == L"MicaNT Modern C++23 Clean-Room Executive", "OSName content must match");

    const auto* osBuildVal = sessionMgr->getValue(L"OSBuild");
    TEST_ASSERT(osBuildVal != nullptr, "OSBuild value must exist");
    TEST_ASSERT(osBuildVal->type == cm::RegType::Dword, "OSBuild must be REG_DWORD");
    TEST_ASSERT(osBuildVal->asDword() == 26100, "OSBuild must be 26100");

    const auto* telemetryVal = sessionMgr->getValue(L"ZeroTelemetryEnabled");
    TEST_ASSERT(telemetryVal != nullptr, "ZeroTelemetryEnabled must exist");
    TEST_ASSERT(telemetryVal->asDword() == 1, "ZeroTelemetryEnabled must be 1");

    // Create custom key hierarchy: \Registry\Machine\SOFTWARE\MicaCorp\Config
    auto software = cm.resolvePath(L"Machine\\SOFTWARE");
    TEST_ASSERT(software != nullptr, "SOFTWARE key must exist");
    auto micaCorp = software->createSubkey(L"MicaCorp");
    auto config = micaCorp->createSubkey(L"Config");
    config->setValueString(L"ReleaseRing", L"Canary");
    config->setValueDword(L"MaxThreads", 64);
    config->setValueQword(L"MaxMemoryQuota", 0x100000000ULL);

    // Verify resolved custom key
    auto resolvedConfig = cm.resolvePath(L"\\Registry\\Machine\\SOFTWARE\\MicaCorp\\Config");
    TEST_ASSERT(resolvedConfig != nullptr, "Created custom key path must resolve");
    TEST_ASSERT(resolvedConfig->getValue(L"ReleaseRing")->asString() == L"Canary", "ReleaseRing must be Canary");
    TEST_ASSERT(resolvedConfig->getValue(L"MaxThreads")->asDword() == 64, "MaxThreads must be 64");
    TEST_ASSERT(resolvedConfig->getValue(L"MaxMemoryQuota")->asQword() == 0x100000000ULL, "MaxMemoryQuota must match");

    // Nonexistent path must return nullptr
    auto badKey = cm.resolvePath(L"\\Registry\\Machine\\NonExistent\\Key");
    TEST_ASSERT(badKey == nullptr, "Nonexistent path must return nullptr");
}

// ============================================================================
// Suite 9: Security Reference Monitor (SRM) SIDs, DACLs, Tokens & AccessCheck
// ============================================================================
void Test_SecurityReferenceMonitor_AccessCheck() {
    // 1. SIDs string formatting & equality
    se::Sid localSystem = se::Sid::localSystem();
    se::Sid admins = se::Sid::administrators();
    se::Sid users = se::Sid::users();
    se::Sid everyone = se::Sid::everyone();

    TEST_ASSERT(localSystem.toString() == L"S-1-5-18", "LocalSystem SID must format as S-1-5-18");
    TEST_ASSERT(admins.toString() == L"S-1-5-32-544", "Admins SID must format as S-1-5-32-544");
    TEST_ASSERT(users.toString() == L"S-1-5-32-545", "Users SID must format as S-1-5-32-545");
    TEST_ASSERT(everyone.toString() == L"S-1-1-0", "Everyone SID must format as S-1-1-0");
    TEST_ASSERT(!(admins == users), "Admins and Users SIDs must not be equal");

    // 2. Token creation and privilege validation
    auto sysToken = se::TokenObject::createSystemToken();
    TEST_ASSERT(sysToken->getUserSid() == localSystem, "System token user SID must be LocalSystem");
    TEST_ASSERT(sysToken->hasSid(admins), "System token must contain Administrators group");
    TEST_ASSERT(sysToken->hasPrivilege(se::SE_DEBUG_NAME), "System token must hold SeDebugPrivilege enabled");

    se::Sid userSid(5, {21, 100, 200, 300, 1001});
    auto userToken = se::TokenObject::createUserToken(userSid);
    TEST_ASSERT(userToken->getUserSid() == userSid, "User token user SID must match created SID");
    TEST_ASSERT(userToken->hasSid(users), "User token must contain Users group");
    TEST_ASSERT(!userToken->hasSid(admins), "User token must NOT contain Administrators group");
    TEST_ASSERT(!userToken->hasPrivilege(se::SE_DEBUG_NAME), "User token must NOT have SeDebugPrivilege");

    // 3. Security Descriptor & DACL Evaluation
    // Case A: Null DACL (No DACL present) -> Full access granted
    se::SecurityDescriptor nullDaclSd;
    uint32_t granted = 0;
    NtStatus s1 = se::SecurityReferenceMonitor::accessCheck(*userToken, nullDaclSd, 0x1234, granted);
    TEST_ASSERT(NT_SUCCESS(s1), "Null DACL must grant full access");
    TEST_ASSERT(granted == 0x1234, "Granted access must match desired access");

    // Case B: Empty DACL (DACL present with 0 ACEs) -> Access Denied
    se::SecurityDescriptor emptyDaclSd;
    emptyDaclSd.setDacl(se::Acl());
    NtStatus s2 = se::SecurityReferenceMonitor::accessCheck(*userToken, emptyDaclSd, 0x1, granted);
    TEST_ASSERT(s2 == NtStatus::AccessDenied, "Empty DACL must deny all access");

    // Case C: DACL allowing Admins, denying Users
    se::Acl acl;
    acl.addDeniedAce(users, 0x00000001); // Deny read to Users
    acl.addAllowedAce(admins, 0x00000003); // Allow read/write to Admins
    acl.addAllowedAce(everyone, 0x00000001); // Allow read to Everyone

    se::SecurityDescriptor sdWithAcl;
    sdWithAcl.setDacl(acl);

    // Admin should succeed with 0x3
    NtStatus sAdmin = se::SecurityReferenceMonitor::accessCheck(*sysToken, sdWithAcl, 0x00000003, granted);
    TEST_ASSERT(NT_SUCCESS(sAdmin), "Admin token must be granted desired access");
    TEST_ASSERT(granted == 0x00000003, "Admin granted mask must match 0x3");

    // User requests 0x1 -> Denied ACE matches first, immediate denial!
    NtStatus sUser = se::SecurityReferenceMonitor::accessCheck(*userToken, sdWithAcl, 0x00000001, granted);
    TEST_ASSERT(sUser == NtStatus::AccessDenied, "User token must be denied by Deny ACE");

    // Case D: Owner Rights (Owner is always granted READ_CONTROL & WRITE_DAC)
    se::SecurityDescriptor ownerSd;
    ownerSd.setOwner(userSid);
    ownerSd.setDacl(se::Acl()); // Even with empty DACL, owner gets READ_CONTROL
    NtStatus sOwner = se::SecurityReferenceMonitor::accessCheck(*userToken, ownerSd, se::READ_CONTROL | se::WRITE_DAC, granted);
    TEST_ASSERT(NT_SUCCESS(sOwner), "Owner must be granted READ_CONTROL and WRITE_DAC");
    TEST_ASSERT((granted & (se::READ_CONTROL | se::WRITE_DAC)) == (se::READ_CONTROL | se::WRITE_DAC), "Owner mask must match");
}

// ============================================================================
// Suite 10: Advanced Local Procedure Call (ALPC) Message Rendezvous Tests
// ============================================================================
void Test_Alpc_MessageRendezvous() {
    auto& portMgr = lpc::PortManager::get();

    // 1. Create named connection port
    auto serverPort = portMgr.createPort(L"\\RPC Control\\MicaLpcTest");
    TEST_ASSERT(serverPort != nullptr, "Creating LPC port should succeed");
    TEST_ASSERT(serverPort->getType() == lpc::PortType::ConnectionPort, "Port type must be ConnectionPort");

    // Duplicate create should fail
    auto dupPort = portMgr.createPort(L"\\RPC Control\\MicaLpcTest");
    TEST_ASSERT(dupPort == nullptr, "Duplicate port creation must fail");

    // 2. Connect client and server communication endpoints
    std::shared_ptr<lpc::PortObject> clientComm;
    std::shared_ptr<lpc::PortObject> serverComm;
    NtStatus connStatus = portMgr.connectPort(L"MicaLpcTest", clientComm, serverComm);
    TEST_ASSERT(NT_SUCCESS(connStatus), "Port connection should succeed");
    TEST_ASSERT(clientComm != nullptr && serverComm != nullptr, "Both comm endpoints must be valid");
    TEST_ASSERT(clientComm->isConnected(), "Client port must be connected");
    TEST_ASSERT(serverComm->isConnected(), "Server port must be connected");

    // 3. Multithreaded Synchronous Request-Wait-Reply Rendezvous
    // Start server worker thread to listen, process request, and send reply
    std::thread serverThread([serverComm]() {
        lpc::PortMessage reqMsg{};
        std::vector<uint8_t> reqPayload;

        NtStatus recStatus = serverComm->receiveMessage(reqMsg, reqPayload, 3000);
        if (NT_SUCCESS(recStatus)) {
            std::string reqStr(reinterpret_cast<char*>(reqPayload.data()), reqPayload.size());
            if (reqStr == "PING_REQUEST") {
                std::string replyStr = "PONG_REPLY_OK";
                std::vector<uint8_t> replyBytes(replyStr.begin(), replyStr.end());
                serverComm->reply(reqMsg, replyBytes);
            }
        }
    });

    // Client initiates synchronous rendezvous
    lpc::PortMessage sendMsg{};
    std::string clientReq = "PING_REQUEST";
    std::vector<uint8_t> clientData(clientReq.begin(), clientReq.end());

    lpc::PortMessage replyMsg{};
    std::vector<uint8_t> replyData;

    NtStatus rpcStatus = clientComm->requestWaitReply(
        sendMsg,
        clientData,
        replyMsg,
        replyData,
        3000
    );

    serverThread.join();

    TEST_ASSERT(NT_SUCCESS(rpcStatus), "Synchronous requestWaitReply must succeed");
    std::string replyString(reinterpret_cast<char*>(replyData.data()), replyData.size());
    TEST_ASSERT(replyString == "PONG_REPLY_OK", "Reply payload must match PONG_REPLY_OK");
    TEST_ASSERT(replyMsg.u2.type == static_cast<uint16_t>(lpc::PortMessageType::LpcReply), "Message type must be LpcReply");
}

// ============================================================================
// Suite 11: Kernel Core (KE) IRQLs, Spinlocks, DPCs, and Priority Scheduler
// ============================================================================
void Test_KernelCore_IrqlSpinLockAndScheduler() {
    // 1. IRQL state machine
    TEST_ASSERT(ke::KeGetCurrentIrql() == ke::PASSIVE_LEVEL, "Initial IRQL must be PASSIVE_LEVEL");
    ke::KIRQL old = ke::KfRaiseIrql(ke::DISPATCH_LEVEL);
    TEST_ASSERT(old == ke::PASSIVE_LEVEL, "Previous IRQL must be PASSIVE_LEVEL");
    TEST_ASSERT(ke::KeGetCurrentIrql() == ke::DISPATCH_LEVEL, "Current IRQL must be DISPATCH_LEVEL");
    ke::KeLowerIrql(ke::PASSIVE_LEVEL);
    TEST_ASSERT(ke::KeGetCurrentIrql() == ke::PASSIVE_LEVEL, "Lowered IRQL must be PASSIVE_LEVEL");

    // 2. Kernel Spinlock
    ke::SpinLock spinLock;
    {
        ke::SpinLockGuard guard(spinLock);
        TEST_ASSERT(spinLock.isLocked(), "Spinlock must be in locked state");
        TEST_ASSERT(ke::KeGetCurrentIrql() == ke::DISPATCH_LEVEL, "Acquiring spinlock must raise IRQL to DISPATCH_LEVEL");
        TEST_ASSERT(guard.getPreviousIrql() == ke::PASSIVE_LEVEL, "Previous IRQL in guard must be PASSIVE_LEVEL");
    }
    TEST_ASSERT(!spinLock.isLocked(), "Spinlock must be unlocked after guard destruction");
    TEST_ASSERT(ke::KeGetCurrentIrql() == ke::PASSIVE_LEVEL, "IRQL must be restored to PASSIVE_LEVEL");

    // 3. Deferred Procedure Call (KDPC)
    static bool s_DpcExecuted = false;
    static void* s_DpcArg = nullptr;
    ke::KDPC dpc{
        .routine = [](ke::KDPC*, void*, void* arg1, void*) {
            s_DpcExecuted = true;
            s_DpcArg = arg1;
        },
        .deferredContext = nullptr
    };

    auto& dpcQueue = ke::DpcQueue::get();
    bool queued = dpcQueue.queueDpc(&dpc, reinterpret_cast<void*>(0x1337));
    TEST_ASSERT(queued, "Queueing DPC must succeed");
    TEST_ASSERT(dpcQueue.getQueuedCount() == 1, "DPC queue must contain 1 entry");

    size_t drained = dpcQueue.drainDpcs();
    TEST_ASSERT(drained == 1, "Draining DPC queue must process 1 entry");
    TEST_ASSERT(s_DpcExecuted, "DPC routine must have executed");
    TEST_ASSERT(s_DpcArg == reinterpret_cast<void*>(0x1337), "DPC argument must match 0x1337");
    TEST_ASSERT(ke::KeGetCurrentIrql() == ke::PASSIVE_LEVEL, "IRQL must return to PASSIVE_LEVEL after DPC drain");

    // 4. 32-Queue Priority Thread Scheduler
    auto& scheduler = ke::PriorityScheduler::get();
    ke::ScheduledThreadEntry tIdle{ .tid = 0, .basePriority = ke::PRIORITY_IDLE, .currentPriority = ke::PRIORITY_IDLE, .name = "SystemIdle" };
    ke::ScheduledThreadEntry tNormal{ .tid = 100, .basePriority = ke::PRIORITY_NORMAL, .currentPriority = ke::PRIORITY_NORMAL, .name = "NormalWorker" };
    ke::ScheduledThreadEntry tRealtime{ .tid = 200, .basePriority = 24, .currentPriority = 24, .name = "RealTimeAudio" };

    scheduler.readyThread(tIdle);
    scheduler.readyThread(tNormal);
    scheduler.readyThread(tRealtime);

    // Highest priority (Realtime 24) must be selected first
    auto next1 = scheduler.selectNextThread();
    TEST_ASSERT(next1.has_value(), "Scheduler must select runnable thread");
    TEST_ASSERT(next1->tid == 200, "Real-time thread 200 must be scheduled first");
    TEST_ASSERT(next1->currentPriority == 24, "Priority must be 24");

    // Next must be Normal (8)
    auto next2 = scheduler.selectNextThread();
    TEST_ASSERT(next2.has_value() && next2->tid == 100, "Normal thread 100 must be scheduled next");

    // Next must be Idle (0)
    auto next3 = scheduler.selectNextThread();
    TEST_ASSERT(next3.has_value() && next3->tid == 0, "Idle thread 0 must be scheduled last");

    // Queue empty
    auto next4 = scheduler.selectNextThread();
    TEST_ASSERT(!next4.has_value(), "Run queues must now be empty");
}

// ============================================================================
// Suite 12: Executive Memory Pools (EX) NonPaged/Paged Pools, Tags, and IRQL
// ============================================================================
void Test_ExecutivePools_AllocationAndIrql() {
    auto& pool = ex::ExecutivePool::get();

    // 1. Allocate NonPagedPool with tag 'Mica'
    void* pNonPaged = ex::ExAllocatePoolWithTag(ex::PoolType::NonPagedPool, 4096, ex::TAG_MICA_CORE);
    TEST_ASSERT(pNonPaged != nullptr, "NonPagedPool allocation of 4KB must succeed");
    std::memset(pNonPaged, 0xAA, 4096);
    TEST_ASSERT(pool.getTotalNonPagedBytes() >= 4096, "NonPaged bytes tracking must reflect allocation");

    auto tagStats = pool.getTagStats(ex::TAG_MICA_CORE);
    TEST_ASSERT(tagStats.activeAllocations >= 1, "Tag 'Mica' active allocations must be >= 1");
    TEST_ASSERT(tagStats.activeBytes >= 4096, "Tag 'Mica' active bytes must be >= 4096");

    // 2. Allocate PagedPool with tag 'Proc' at PASSIVE_LEVEL
    TEST_ASSERT(ke::KeGetCurrentIrql() == ke::PASSIVE_LEVEL, "Current IRQL must be PASSIVE_LEVEL");
    void* pPaged = ex::ExAllocatePoolWithTag(ex::PoolType::PagedPool, 8192, ex::TAG_PROCESS);
    TEST_ASSERT(pPaged != nullptr, "PagedPool allocation at PASSIVE_LEVEL must succeed");
    std::memset(pPaged, 0xBB, 8192);
    TEST_ASSERT(pool.getTotalPagedBytes() >= 8192, "Paged bytes tracking must reflect allocation");

    // 3. Verify IRQL Enforcement: Attempting to allocate PagedPool at DISPATCH_LEVEL must fail!
    ke::KIRQL old = ke::KfRaiseIrql(ke::DISPATCH_LEVEL);
    void* pIllegalPaged = ex::ExAllocatePoolWithTag(ex::PoolType::PagedPool, 1024, ex::TAG_SECTION);
    TEST_ASSERT(pIllegalPaged == nullptr, "PagedPool allocation at DISPATCH_LEVEL must fail and be rejected!");

    // Allocating NonPagedPool at DISPATCH_LEVEL is valid and allowed
    void* pValidNonPagedAtDpc = ex::ExAllocatePoolWithTag(ex::PoolType::NonPagedPool, 1024, ex::TAG_SECTION);
    TEST_ASSERT(pValidNonPagedAtDpc != nullptr, "NonPagedPool allocation at DISPATCH_LEVEL must succeed");
    ke::KeLowerIrql(old);

    // 4. Free allocations with tag verification
    ex::ExFreePoolWithTag(pValidNonPagedAtDpc, ex::TAG_SECTION);
    ex::ExFreePoolWithTag(pPaged, ex::TAG_PROCESS);
    ex::ExFreePoolWithTag(pNonPaged, ex::TAG_MICA_CORE);

    auto finalMicaStats = pool.getTagStats(ex::TAG_MICA_CORE);
    TEST_ASSERT(finalMicaStats.activeAllocations == 0, "All 'Mica' allocations must be freed");
    TEST_ASSERT(finalMicaStats.activeBytes == 0, "Active 'Mica' bytes must be 0");
}

// ============================================================================
// Suite 13: Trap & Fault Engine (KE/TRAP) Page Faults, SEH, and BugCheck
// ============================================================================
void Test_TrapEngine_PageFaultAndBugCheck() {
    auto& trap = ke::TrapEngine::get();
    auto& pm = ps::ProcessManager::get();
    auto proc = pm.createProcess(L"TrapTestProcess.exe");

    // 1. Setup VAD region with PAGE_WRITECOPY
    uintptr_t vadBase = 0;
    size_t vadSize = 64 * 1024;
    NtStatus allocStatus = proc->getAddressSpace().allocate(
        vadBase,
        vadSize,
        mm::MEM_RESERVE,
        mm::PAGE_WRITECOPY
    );
    TEST_ASSERT(NT_SUCCESS(allocStatus), "VAD allocation should succeed");

    // Test Demand Paging & Copy-on-Write page fault
    NtStatus pfStatus = trap.handlePageFault(*proc, vadBase + 0x1000, true, false);
    TEST_ASSERT(NT_SUCCESS(pfStatus), "Page fault handler should resolve demand page");
    auto* vad = proc->getAddressSpace().findVad(vadBase + 0x1000);
    TEST_ASSERT(vad != nullptr, "VAD region must exist");
    TEST_ASSERT(vad->committed, "Demand paging must commit VAD page");
    TEST_ASSERT(vad->protection == mm::PAGE_READWRITE, "Copy-on-Write fault must transition to PAGE_READWRITE");

    // Test access fault at unmapped address (must return STATUS_ACCESS_VIOLATION)
    NtStatus badPf = trap.handlePageFault(*proc, 0x00007FFF99990000ULL, true, false);
    TEST_ASSERT(badPf == NtStatus::AccessViolation, "Unmapped address fault must return AccessViolation");

    // Test instruction execution fault on non-executable page
    NtStatus execPf = trap.handlePageFault(*proc, vadBase + 0x1000, false, true);
    TEST_ASSERT(execPf == NtStatus::AccessViolation, "Executing non-executable page must return AccessViolation");

    // 2. Structured Exception Dispatcher (KiDispatchException)
    auto thread = proc->createThread(0x140001000);
    ke::ExceptionRecord record{
        .exceptionCode = ke::EXCEPTION_BREAKPOINT,
        .exceptionAddress = reinterpret_cast<void*>(0x140001000)
    };
    ps::ContextFrame ctx = thread->getContext();
    ctx.cs = 0x33; // User mode

    // Install user SEH filter
    static bool s_SehHandled = false;
    trap.setUserSehHandler([](const ke::ExceptionRecord& rec, ps::ContextFrame&) -> bool {
        if (rec.exceptionCode == ke::EXCEPTION_BREAKPOINT) {
            s_SehHandled = true;
            return true; // Exception Handled
        }
        return false;
    });

    NtStatus dispRes = trap.dispatchException(record, ctx, *thread, true);
    TEST_ASSERT(NT_SUCCESS(dispRes), "User SEH handler should handle first-chance breakpoint");
    TEST_ASSERT(s_SehHandled, "SEH callback flag must be set");

    // Second chance unhandled terminates process
    trap.setUserSehHandler(nullptr);
    ke::ExceptionRecord unhandledRecord{
        .exceptionCode = ke::EXCEPTION_ACCESS_VIOLATION,
        .exceptionAddress = reinterpret_cast<void*>(0x140002000)
    };
    NtStatus unhandledRes = trap.dispatchException(unhandledRecord, ctx, *thread, false);
    TEST_ASSERT(unhandledRes == NtStatus::ProcessIsTerminating, "Unhandled second-chance exception must terminate process");
    TEST_ASSERT(proc->isTerminated(), "Process must be terminated");

    // 3. Kernel Bug Check (KeBugCheckEx)
    ke::KeBugCheckEx(ke::PAGE_FAULT_IN_NONPAGED_AREA, 0x1111, 0x2222, 0x3333, 0x4444);
    const auto& lastBugCheck = trap.getLastBugCheck();
    TEST_ASSERT(lastBugCheck.dumpGenerated, "BugCheck must record crash dump generated");
    TEST_ASSERT(lastBugCheck.bugCheckCode == ke::PAGE_FAULT_IN_NONPAGED_AREA, "BugCheck code must match PAGE_FAULT_IN_NONPAGED_AREA (0x50)");
    TEST_ASSERT(lastBugCheck.param1 == 0x1111, "Param 1 must match");
    TEST_ASSERT(lastBugCheck.param2 == 0x2222, "Param 2 must match");
}

// ============================================================================
// Suite 14: Hardware Abstraction Layer (HAL) KPCR, KPRCB, and Timers
// ============================================================================
void Test_HardwareAbstractionLayer_KPCRAndTimers() {
    auto& hal = hal::HardwareAbstractionLayer::get();
    TEST_ASSERT(hal.getProcessorCount() >= 1, "HAL must detect at least 1 processor core");

    auto* kpcr0 = hal.getKpcr(0);
    TEST_ASSERT(kpcr0 != nullptr, "KPCR 0 must not be null");
    TEST_ASSERT(kpcr0->self == kpcr0, "KPCR self pointer must point to itself");
    TEST_ASSERT(kpcr0->prcb.cpuId == 0, "CPU ID must be 0");
    TEST_ASSERT(kpcr0->prcb.architecture == hal::ProcessorArchitecture::Amd64, "Architecture must be AMD64");

    // Performance counter
    LargeInteger c1{};
    LargeInteger c2{};
    LargeInteger freq{};
    hal::KeQueryPerformanceCounter(c1, &freq);
    TEST_ASSERT(freq.quadPart == 1'000'000'000, "Performance frequency must be 1 GHz (nanosecond precision)");

    hal::KeStallExecutionProcessor(50); // Stall 50 microseconds
    hal::KeQueryPerformanceCounter(c2, &freq);
    TEST_ASSERT(c2.quadPart > c1.quadPart, "Performance counter must strictly advance over stall");

    // Clock tick accounting
    uint64_t initialInterrupts = kpcr0->prcb.interruptsServiced;
    hal.dispatchClockTick(0);
    hal.dispatchClockTick(0);
    TEST_ASSERT(kpcr0->prcb.interruptsServiced == initialInterrupts + 2, "Dispatched clock ticks must increment interrupt count");
}

// ============================================================================
// Suite 15: Virtual File System & Fastfat Driver Tests
// ============================================================================
void Test_VirtualFileSystem_Fat32AndFileObjects() {
    auto& vfs = fs::VirtualFileSystem::get();
    vfs.initialize();

    auto* partitionDev = vfs.getPartitionDevice();
    TEST_ASSERT(partitionDev != nullptr, "Partition device node must be registered");
    TEST_ASSERT(partitionDev->deviceType == io::DeviceType::FileSystem, "Device type must be FileSystem");

    // Test 1: Create a file via NtCreateFile
    UnicodeString filePath(L"\\DosDevices\\C:\\Windows\\test_output.log");
    ObjectAttributes objAttr{};
    objAttr.objectName = &filePath;
    Handle fileHandle = 0;
    IoStatusBlock iosb{};

    NtStatus st = sys::NtCreateFile(
        &fileHandle,
        fs::FILE_GENERIC_READ | fs::FILE_GENERIC_WRITE,
        &objAttr,
        &iosb,
        nullptr,
        fs::FILE_ATTRIBUTE_NORMAL,
        0,
        fs::FILE_CREATE,
        0,
        nullptr,
        0
    );
    TEST_ASSERT(NT_SUCCESS(st), "NtCreateFile must succeed creating file");
    TEST_ASSERT(fileHandle != 0, "File handle must be valid");
    TEST_ASSERT(iosb.information == 2, "iosb.information must be 2 (FILE_CREATED)");

    // Test 2: Write data to file via NtWriteFile
    std::string testPayload = "Hello MicaNT Kernel Storage Subsystem!";
    st = sys::NtWriteFile(
        fileHandle,
        0,
        nullptr,
        nullptr,
        &iosb,
        testPayload.data(),
        static_cast<uint32_t>(testPayload.size()),
        nullptr,
        nullptr
    );
    TEST_ASSERT(NT_SUCCESS(st), "NtWriteFile must succeed");
    TEST_ASSERT(iosb.information == testPayload.size(), "Bytes written must match payload length");

    // Close the file
    st = sys::NtClose(fileHandle);
    TEST_ASSERT(NT_SUCCESS(st), "NtClose must close file handle");

    // Test 3: Re-open file via NtOpenFile and read back contents
    fileHandle = 0;
    st = sys::NtOpenFile(
        &fileHandle,
        fs::FILE_GENERIC_READ,
        &objAttr,
        &iosb,
        0,
        0
    );
    TEST_ASSERT(NT_SUCCESS(st), "NtOpenFile must reopen created file");
    TEST_ASSERT(fileHandle != 0, "Reopened handle must be valid");

    std::vector<char> readBuffer(64, 0);
    st = sys::NtReadFile(
        fileHandle,
        0,
        nullptr,
        nullptr,
        &iosb,
        readBuffer.data(),
        static_cast<uint32_t>(readBuffer.size()),
        nullptr,
        nullptr
    );
    TEST_ASSERT(NT_SUCCESS(st), "NtReadFile must succeed");
    TEST_ASSERT(iosb.information == testPayload.size(), "Bytes read must match written bytes");
    std::string readStr(readBuffer.data(), iosb.information);
    TEST_ASSERT(readStr == testPayload, "Read content must match payload");

    sys::NtClose(fileHandle);

    // Test 4: Open pre-seeded system binary \DosDevices\C:\Windows\System32\ntdll.dll
    UnicodeString ntdllPath(L"\\DosDevices\\C:\\Windows\\System32\\ntdll.dll");
    ObjectAttributes ntdllAttr{};
    ntdllAttr.objectName = &ntdllPath;
    Handle ntdllHandle = 0;
    st = sys::NtOpenFile(&ntdllHandle, fs::FILE_GENERIC_READ, &ntdllAttr, &iosb, 0, 0);
    TEST_ASSERT(NT_SUCCESS(st), "NtOpenFile must locate system binary ntdll.dll");
    TEST_ASSERT(ntdllHandle != 0, "ntdll handle must be valid");

    std::vector<char> ntdllHeader(32, 0);
    st = sys::NtReadFile(ntdllHandle, 0, nullptr, nullptr, &iosb, ntdllHeader.data(), 32, nullptr, nullptr);
    TEST_ASSERT(NT_SUCCESS(st), "NtReadFile must read ntdll header");
    std::string headerStr(ntdllHeader.data(), iosb.information);
    TEST_ASSERT(headerStr.find("MZ-MICANT") != std::string::npos, "ntdll header magic must be found");

    sys::NtClose(ntdllHandle);
}

// ============================================================================
// Suite 16: Executive Worker Queues Tests
// ============================================================================
void Test_ExecutiveWorkQueues_Dispatch() {
    auto& workMgr = ex::ExecutiveWorkQueueManager::get();
    workMgr.initialize(2);
    TEST_ASSERT(workMgr.getActiveWorkerCount() == 2, "Active worker count must be 2");

    std::atomic<int> completedTasks{0};
    auto workerCallback = [](void* ctx) {
        auto* counter = static_cast<std::atomic<int>*>(ctx);
        (*counter)++;
    };

    ex::WorkQueueItem item1;
    ex::ExInitializeWorkItem(&item1, workerCallback, &completedTasks);
    ex::ExQueueWorkItem(&item1, ex::WorkQueueType::CriticalWorkQueue);

    ex::WorkQueueItem item2;
    ex::ExInitializeWorkItem(&item2, workerCallback, &completedTasks);
    ex::ExQueueWorkItem(&item2, ex::WorkQueueType::DelayedWorkQueue);

    // Wait for workers to complete
    int retries = 50;
    while (completedTasks.load() < 2 && retries-- > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    TEST_ASSERT(completedTasks.load() == 2, "Both executive work items must be processed");
    TEST_ASSERT(workMgr.getTotalProcessed() >= 2, "Total processed counter must be >= 2");
}

// ============================================================================
// Suite 17: Loader Parameter Block & Boot Contract Tests
// ============================================================================
void Test_BootContract_LoaderParameterBlock() {
    auto lpb = boot::createDefaultUefiBootBlock();
    TEST_ASSERT(lpb.hasOption("/ZERO_TELEMETRY=1"), "Loader block must have zero telemetry enabled");
    TEST_ASSERT(lpb.hasOption("/DEBUG"), "Loader block must contain /DEBUG option");
    TEST_ASSERT(lpb.osBuildNumber == 26100, "Build number must match 26100");

    TEST_ASSERT(!lpb.memoryDescriptors.empty(), "Memory descriptors must not be empty");
    uint64_t totalRam = lpb.getTotalMemoryBytes();
    uint64_t freeRam  = lpb.getFreeMemoryBytes();
    TEST_ASSERT(totalRam > 0, "Total RAM must be positive");
    TEST_ASSERT(freeRam > 0 && freeRam <= totalRam, "Free RAM must be valid portion of total");

    TEST_ASSERT(lpb.bootModules.size() >= 3, "Must have at least 3 boot modules (ntoskrnl, hal, fastfat)");
    TEST_ASSERT(lpb.bootModules[0].baseDllName == L"ntoskrnl.exe", "Module 0 must be ntoskrnl.exe");
    TEST_ASSERT(lpb.bootModules[1].baseDllName == L"hal.dll", "Module 1 must be hal.dll");
    TEST_ASSERT(lpb.bootModules[2].baseDllName == L"fastfat.sys", "Module 2 must be fastfat.sys");
    TEST_ASSERT(lpb.acpiTablePhysicalAddress != 0, "ACPI RSDP physical address must be populated");
}

// ============================================================================
// Suite 18: Kernel Stress, Concurrency & Pointer Probing Hardening
// ============================================================================
void Test_KernelStressAndConcurrencyHardening() {
    // 1. ProbeForRead and ProbeForWrite Validation
    int validUserStackVal = 42;
    TEST_ASSERT(NT_SUCCESS(mm::ProbeForRead(&validUserStackVal, sizeof(validUserStackVal), alignof(int))), 
                "Valid user stack address probe must succeed");

    TEST_ASSERT(mm::ProbeForRead(nullptr, 4, 4) == NtStatus::AccessViolation, 
                "Null pointer probe must return AccessViolation");

    // Unaligned address probe
    uintptr_t unalignedPtr = reinterpret_cast<uintptr_t>(&validUserStackVal) | 1;
    TEST_ASSERT(mm::ProbeForRead(reinterpret_cast<void*>(unalignedPtr), 4, 4) == NtStatus::DatatypeMisalignment, 
                "Unaligned pointer probe must return DatatypeMisalignment");

    // Kernel-space address probe (must reject)
    void* kernelAddr = reinterpret_cast<void*>(0xFFFFF80000000000ULL);
    TEST_ASSERT(mm::ProbeForRead(kernelAddr, 64, 8) == NtStatus::AccessViolation, 
                "Kernel space address in user probe must return AccessViolation");

    // 2. High-Throughput Spinlock Contention & IRQL Verification
    ke::SpinLock stressLock;
    std::atomic<uint64_t> sharedCounter{0};
    constexpr int NUM_SPIN_THREADS = 8;
    constexpr int ITERATIONS_PER_THREAD = 1000;
    std::vector<std::thread> spinThreads;
    spinThreads.reserve(NUM_SPIN_THREADS);

    for (int t = 0; t < NUM_SPIN_THREADS; ++t) {
        spinThreads.emplace_back([&]() {
            for (int i = 0; i < ITERATIONS_PER_THREAD; ++i) {
                ke::SpinLockGuard guard(stressLock);
                sharedCounter.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto& t : spinThreads) {
        t.join();
    }
    TEST_ASSERT(sharedCounter.load() == NUM_SPIN_THREADS * ITERATIONS_PER_THREAD, 
                "Spinlock protected shared counter must equal exactly 8,000 without data races");
    TEST_ASSERT(ke::KeGetCurrentIrql() == ke::PASSIVE_LEVEL, 
                "IRQL must return to PASSIVE_LEVEL after spinlock release");

    // 3. Multithreaded Executive Pool Concurrency Stress
    constexpr int NUM_POOL_THREADS = 8;
    constexpr int ALLOCS_PER_THREAD = 50;
    std::vector<std::thread> poolThreads;
    poolThreads.reserve(NUM_POOL_THREADS);
    std::atomic<bool> poolSuccess{true};

    for (int t = 0; t < NUM_POOL_THREADS; ++t) {
        poolThreads.emplace_back([&, t]() {
            for (int i = 0; i < ALLOCS_PER_THREAD; ++i) {
                size_t sz = 64 + ((i * 17) % 512);
                void* p = ex::ExAllocatePoolWithTag(ex::PoolType::NonPagedPool, sz, ex::TAG_MICA_CORE);
                if (!p) {
                    poolSuccess = false;
                    break;
                }
                // Write and verify pattern
                uint8_t byteVal = static_cast<uint8_t>((t ^ i) & 0xFF);
                std::memset(p, byteVal, sz);
                auto* bytes = static_cast<uint8_t*>(p);
                for (size_t b = 0; b < sz; ++b) {
                    if (bytes[b] != byteVal) {
                        poolSuccess = false;
                        break;
                    }
                }
                ex::ExFreePoolWithTag(p, ex::TAG_MICA_CORE);
            }
        });
    }

    for (auto& t : poolThreads) {
        t.join();
    }
    TEST_ASSERT(poolSuccess.load(), "Multithreaded pool allocation and data integrity must hold 100%");

    // 4. High-Throughput I/O Completion Port (IOCP) Multi-Producer / Multi-Consumer Stress
    io::IoCompletionPort stressIocp(4);
    constexpr int NUM_PRODUCERS = 4;
    constexpr int PACKETS_PER_PRODUCER = 250;
    constexpr int TOTAL_PACKETS = NUM_PRODUCERS * PACKETS_PER_PRODUCER; // 1,000 packets

    std::vector<std::thread> producers;
    for (int p = 0; p < NUM_PRODUCERS; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < PACKETS_PER_PRODUCER; ++i) {
                uint64_t key = (static_cast<uint64_t>(p) << 32) | static_cast<uint64_t>(i);
                stressIocp.postCompletion(key, 0, NtStatus::Success, 64);
            }
        });
    }

    std::atomic<int> packetsConsumed{0};
    std::atomic<uint64_t> totalBytesTransferred{0};
    constexpr int NUM_CONSUMERS = 4;
    std::vector<std::thread> consumers;

    for (int c = 0; c < NUM_CONSUMERS; ++c) {
        consumers.emplace_back([&]() {
            while (packetsConsumed.load() < TOTAL_PACKETS) {
                uint64_t key = 0;
                uintptr_t ov = 0;
                IoStatusBlock iosb{};
                NtStatus st = stressIocp.removeCompletion(key, ov, iosb, 100);
                if (NT_SUCCESS(st)) {
                    packetsConsumed.fetch_add(1, std::memory_order_relaxed);
                    totalBytesTransferred.fetch_add(iosb.information, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    TEST_ASSERT(packetsConsumed.load() >= TOTAL_PACKETS, "All 1,000 IOCP packets must be consumed");
    TEST_ASSERT(totalBytesTransferred.load() == static_cast<uint64_t>(TOTAL_PACKETS * 64), 
                "Total transferred bytes across all IOCP completions must match 64,000 bytes");
}

// ============================================================================
// Suite 19: Driver Model, DriverEntry & NtDeviceIoControlFile
// ============================================================================
namespace test_driver {

struct MicaTelemetryData {
    uint32_t cpuTemperature;
    uint32_t fanSpeedRpm;
    uint64_t uptimeNanoseconds;
    uint32_t activeCores;
};

inline constexpr uint32_t FILE_DEVICE_MICA_SENSOR = 0x8000;
inline constexpr uint32_t IOCTL_MICA_GET_VITALS = driver::CTL_CODE(
    FILE_DEVICE_MICA_SENSOR, 0x801, driver::METHOD_BUFFERED, driver::FILE_READ_ACCESS
);
inline constexpr uint32_t IOCTL_MICA_SET_POWER_MODE = driver::CTL_CODE(
    FILE_DEVICE_MICA_SENSOR, 0x802, driver::METHOD_BUFFERED, driver::FILE_WRITE_ACCESS
);

static uint32_t g_DriverPowerMode = 1;

static NtStatus MicaDriverDispatchDeviceControl(io::DeviceObject* dev, io::Irp* irp) {
    (void)dev;
    if (!irp) return NtStatus::InvalidParameter;

    uint32_t ioctl = irp->byteOffset.lowPart;
    if (ioctl == IOCTL_MICA_GET_VITALS) {
        if (!irp->userBuffer || irp->length < sizeof(MicaTelemetryData)) {
            irp->ioStatus.status = NtStatus::BufferTooSmall;
            irp->ioStatus.information = 0;
            return NtStatus::BufferTooSmall;
        }

        auto* vitals = static_cast<MicaTelemetryData*>(irp->userBuffer);
        vitals->cpuTemperature = 48; // 48 deg C
        vitals->fanSpeedRpm = 1850;  // 1850 RPM
        vitals->uptimeNanoseconds = 1'000'000'000ULL;
        vitals->activeCores = 4;

        irp->ioStatus.status = NtStatus::Success;
        irp->ioStatus.information = sizeof(MicaTelemetryData);
        return NtStatus::Success;
    } else if (ioctl == IOCTL_MICA_SET_POWER_MODE) {
        if (!irp->systemBuffer || irp->byteOffset.highPart < sizeof(uint32_t)) {
            irp->ioStatus.status = NtStatus::InvalidParameter;
            return NtStatus::InvalidParameter;
        }
        g_DriverPowerMode = *static_cast<const uint32_t*>(irp->systemBuffer);
        irp->ioStatus.status = NtStatus::Success;
        irp->ioStatus.information = sizeof(uint32_t);
        return NtStatus::Success;
    }

    irp->ioStatus.status = NtStatus::InvalidDeviceRequest;
    return NtStatus::InvalidDeviceRequest;
}

static NtStatus MicaDriverEntry(io::DriverObject* driverObject, const UnicodeString* registryPath) {
    (void)registryPath;
    if (!driverObject) return NtStatus::InvalidParameter;

    // Register IOCTL handler
    driverObject->setDispatch(io::IRP_MJ_DEVICE_CONTROL, &MicaDriverDispatchDeviceControl);

    // Create device node \Device\MicaVitals
    auto devNode = io::IoManager::get().createDevice(
        driverObject,
        L"\\Device\\MicaVitals",
        static_cast<io::DeviceType>(FILE_DEVICE_MICA_SENSOR)
    );

    return devNode ? NtStatus::Success : NtStatus::Unsuccessful;
}

} // namespace test_driver

void Test_DriverModel_DriverEntryAndDeviceIoControl() {
    auto& drvMgr = driver::DriverManager::get();
    NtStatus loadStatus = drvMgr.loadDriver(
        L"\\Driver\\MicaVitals",
        test_driver::MicaDriverEntry,
        L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\MicaVitals"
    );
    TEST_ASSERT(NT_SUCCESS(loadStatus), "DriverManager must successfully load driver via DriverEntry");
    TEST_ASSERT(drvMgr.getLoadedDriverCount() >= 1, "Loaded driver count must be at least 1");
    TEST_ASSERT(drvMgr.lookupDriver(L"\\Driver\\MicaVitals") != nullptr, "Driver lookup must succeed");

    // Open handle to \Device\MicaVitals via NtOpenFile
    UnicodeString devPath(L"\\Device\\MicaVitals");
    ObjectAttributes objAttr{};
    objAttr.objectName = &devPath;
    Handle devHandle = 0;
    IoStatusBlock iosb{};

    NtStatus openStatus = sys::NtOpenFile(&devHandle, fs::FILE_GENERIC_READ | fs::FILE_GENERIC_WRITE, &objAttr, &iosb, 0, 0);
    TEST_ASSERT(NT_SUCCESS(openStatus), "NtOpenFile must succeed opening \\Device\\MicaVitals");
    TEST_ASSERT(devHandle != 0, "Device handle must be valid");

    // 1. Send IOCTL_MICA_GET_VITALS via NtDeviceIoControlFile
    test_driver::MicaTelemetryData vitalsOut{};
    NtStatus ioctlStatus = sys::NtDeviceIoControlFile(
        devHandle,
        0,
        nullptr,
        nullptr,
        &iosb,
        test_driver::IOCTL_MICA_GET_VITALS,
        nullptr,
        0,
        &vitalsOut,
        sizeof(vitalsOut)
    );
    TEST_ASSERT(NT_SUCCESS(ioctlStatus), "NtDeviceIoControlFile for GET_VITALS must succeed");
    TEST_ASSERT(iosb.information == sizeof(test_driver::MicaTelemetryData), "Returned size must match struct size");
    TEST_ASSERT(vitalsOut.cpuTemperature == 48, "Vitals CPU temperature must match driver value (48 C)");
    TEST_ASSERT(vitalsOut.fanSpeedRpm == 1850, "Vitals fan speed must match driver value (1850 RPM)");
    TEST_ASSERT(vitalsOut.activeCores == 4, "Vitals active cores must match driver value (4)");

    // 2. Send IOCTL_MICA_SET_POWER_MODE via NtDeviceIoControlFile
    uint32_t newPowerMode = 2; // High Performance
    ioctlStatus = sys::NtDeviceIoControlFile(
        devHandle,
        0,
        nullptr,
        nullptr,
        &iosb,
        test_driver::IOCTL_MICA_SET_POWER_MODE,
        &newPowerMode,
        sizeof(newPowerMode),
        nullptr,
        0
    );
    TEST_ASSERT(NT_SUCCESS(ioctlStatus), "NtDeviceIoControlFile for SET_POWER_MODE must succeed");
    TEST_ASSERT(test_driver::g_DriverPowerMode == 2, "Driver state must be updated to High Performance mode");

    // 3. Test buffer too small condition
    ioctlStatus = sys::NtDeviceIoControlFile(
        devHandle,
        0,
        nullptr,
        nullptr,
        &iosb,
        test_driver::IOCTL_MICA_GET_VITALS,
        nullptr,
        0,
        &vitalsOut,
        4 // Too small
    );
    TEST_ASSERT(ioctlStatus == NtStatus::BufferTooSmall, "Small output buffer must return STATUS_BUFFER_TOO_SMALL");

    // Close device handle
    sys::NtClose(devHandle);
}

void Test_KernelTimers_DpcAndDelayExecution() {
    // 1. Initialize KTIMER
    timer::KTIMER timerObj;
    timer::KeInitializeTimer(&timerObj);
    TEST_ASSERT(!timer::KeReadStateTimer(&timerObj), "Newly initialized timer must not be signaled");

    // 2. Set Timer with KDPC callback
    static std::atomic<uint32_t> s_DpcCount{0};
    s_DpcCount = 0;
    ke::KDPC dpcObj{};
    dpcObj.routine = [](ke::KDPC*, void*, void*, void*) {
        s_DpcCount++;
    };

    LargeInteger dueTime{};
    dueTime.quadPart = -10000; // -1ms
    bool wasSet = timer::KeSetTimer(&timerObj, dueTime, &dpcObj);
    TEST_ASSERT(!wasSet, "Initial KeSetTimer should report previously uninserted");
    TEST_ASSERT(timer::TimerManager::get().getActiveTimerCount() >= 1, "TimerManager must contain registered timer");

    // Sleep 5ms so deadline passes
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    size_t expiredCount = timer::TimerManager::get().processTimers();
    TEST_ASSERT(expiredCount >= 1, "processTimers must process at least 1 expired timer");
    TEST_ASSERT(timer::KeReadStateTimer(&timerObj), "Expired timer must be signaled");

    // Drain DPC queue and verify execution at DISPATCH_LEVEL
    size_t dpcsDrained = ke::DpcQueue::get().drainDpcs();
    TEST_ASSERT(dpcsDrained >= 1, "DpcQueue must drain at least 1 DPC");
    TEST_ASSERT(s_DpcCount == 1, "Timer DPC callback must have executed exactly once");

    // 3. Test KeCancelTimer
    timer::KTIMER cancelTestTimer;
    timer::KeInitializeTimer(&cancelTestTimer);
    dueTime.quadPart = -100000000; // -10 seconds
    timer::KeSetTimer(&cancelTestTimer, dueTime, nullptr);
    bool cancelled = timer::KeCancelTimer(&cancelTestTimer);
    TEST_ASSERT(cancelled, "KeCancelTimer must return true for active unexpired timer");

    // 4. Test NtDelayExecution syscall
    auto start = std::chrono::steady_clock::now();
    LargeInteger delayInterval{};
    delayInterval.quadPart = -30000; // -3ms
    NtStatus delayStatus = sys::NtDelayExecution(false, &delayInterval);
    TEST_ASSERT(NT_SUCCESS(delayStatus), "NtDelayExecution must succeed");
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    TEST_ASSERT(elapsed >= 2, "NtDelayExecution must delay execution for at least requested duration");

    // Zero delay yields
    delayInterval.quadPart = 0;
    TEST_ASSERT(NT_SUCCESS(sys::NtDelayExecution(false, &delayInterval)), "Zero delay execution must succeed");

    // Null pointer returns AccessViolation
    TEST_ASSERT(sys::NtDelayExecution(false, nullptr) == NtStatus::AccessViolation, "Null delay pointer must return STATUS_ACCESS_VIOLATION");

    // 5. Test Timer Syscalls via SyscallDispatcher (KiSystemCall64)
    auto& dispatcher = sys::SyscallDispatcher::get();

    Handle hTimer = 0;
    sys::SyscallFrame frame{};
    frame.ssn = sys::SSN_NtCreateTimer;
    frame.arg1 = reinterpret_cast<uint64_t>(&hTimer);
    frame.arg2 = 0x1F0003; // TIMER_ALL_ACCESS
    frame.arg3 = 0;
    frame.arg4 = 0; // NotificationTimer
    NtStatus status = dispatcher.dispatch(frame);
    TEST_ASSERT(NT_SUCCESS(status) && hTimer != 0, "NtCreateTimer must succeed with valid handle");

    LargeInteger timerDue{};
    timerDue.quadPart = -20000; // -2ms
    frame = sys::SyscallFrame{};
    frame.ssn = sys::SSN_NtSetTimer;
    frame.arg1 = static_cast<uint64_t>(hTimer);
    frame.arg2 = reinterpret_cast<uint64_t>(&timerDue);
    frame.arg3 = 0;
    frame.arg4 = 0;
    status = dispatcher.dispatch(frame);
    TEST_ASSERT(NT_SUCCESS(status), "NtSetTimer must succeed");

    // Sleep 4ms and signal timer
    std::this_thread::sleep_for(std::chrono::milliseconds(4));
    auto timObj = sync::DispatcherRegistry::get().lookupAs<timer::TimerObject>(hTimer);
    TEST_ASSERT(timObj != nullptr, "TimerObject must be retrievable from DispatcherRegistry");
    timObj->setSignaled();

    // Wait on timer handle via NtWaitForSingleObject
    frame = sys::SyscallFrame{};
    frame.ssn = sys::SSN_NtWaitForSingleObject;
    frame.arg1 = static_cast<uint64_t>(hTimer);
    frame.arg2 = 0;
    frame.arg3 = 0;
    status = dispatcher.dispatch(frame);
    TEST_ASSERT(status == NtStatus::Success, "NtWaitForSingleObject on signaled timer must succeed");

    // Close timer handle
    frame = sys::SyscallFrame{};
    frame.ssn = sys::SSN_NtClose;
    frame.arg1 = static_cast<uint64_t>(hTimer);
    (void)dispatcher.dispatch(frame);
}

void Test_WaitMultipleObjects_MultiHandleSync() {
    auto& dispatcher = sys::SyscallDispatcher::get();

    // 1. Create 3 events
    Handle ev1 = 0, ev2 = 0, ev3 = 0;
    sys::SyscallFrame frame{};

    frame.ssn = sys::SSN_NtCreateEvent;
    frame.arg1 = reinterpret_cast<uint64_t>(&ev1);
    frame.arg3 = 0; // NotificationEvent
    frame.arg4 = 0; // Unsignaled
    (void)dispatcher.dispatch(frame);

    frame.arg1 = reinterpret_cast<uint64_t>(&ev2);
    (void)dispatcher.dispatch(frame);

    frame.arg1 = reinterpret_cast<uint64_t>(&ev3);
    (void)dispatcher.dispatch(frame);

    TEST_ASSERT(ev1 != 0 && ev2 != 0 && ev3 != 0, "All 3 events must be created");

    Handle handles[3] = { ev1, ev2, ev3 };

    // 2. Test WaitAny when none are signaled with 5ms timeout -> Timeout
    LargeInteger timeout{};
    timeout.quadPart = -50000; // -5ms
    NtStatus waitStatus = sys::NtWaitForMultipleObjects(3, handles, WaitType::WaitAny, false, &timeout);
    TEST_ASSERT(waitStatus == NtStatus::Timeout, "NtWaitForMultipleObjects WaitAny with none signaled must timeout");

    // 3. Signal Event 2 (index 1), verify WaitAny returns STATUS_WAIT_1 (0x00000001)
    frame = sys::SyscallFrame{};
    frame.ssn = sys::SSN_NtSetEvent;
    frame.arg1 = static_cast<uint64_t>(ev2);
    (void)dispatcher.dispatch(frame);

    waitStatus = sys::NtWaitForMultipleObjects(3, handles, WaitType::WaitAny, false, nullptr);
    TEST_ASSERT(waitStatus == NtStatus::Wait1, "WaitAny must return STATUS_WAIT_1 when handle index 1 is signaled");

    // Reset Event 2
    frame.ssn = sys::SSN_NtResetEvent;
    (void)dispatcher.dispatch(frame);

    // 4. Test WaitAll: signal ev1 and ev2, leave ev3 unsignaled -> Timeout
    frame = sys::SyscallFrame{};
    frame.ssn = sys::SSN_NtSetEvent;
    frame.arg1 = static_cast<uint64_t>(ev1);
    (void)dispatcher.dispatch(frame);
    frame.arg1 = static_cast<uint64_t>(ev2);
    (void)dispatcher.dispatch(frame);

    waitStatus = sys::NtWaitForMultipleObjects(3, handles, WaitType::WaitAll, false, &timeout);
    TEST_ASSERT(waitStatus == NtStatus::Timeout, "WaitAll must timeout if not all handles are signaled");

    // Signal ev3 -> now all 3 are signaled
    frame.arg1 = static_cast<uint64_t>(ev3);
    (void)dispatcher.dispatch(frame);

    waitStatus = sys::NtWaitForMultipleObjects(3, handles, WaitType::WaitAll, false, &timeout);
    TEST_ASSERT(waitStatus == NtStatus::Success, "WaitAll must succeed when all handles are signaled");

    // 5. Test Parameter Validation
    TEST_ASSERT(sys::NtWaitForMultipleObjects(0, handles, WaitType::WaitAny, false, nullptr) == NtStatus::InvalidParameter1,
                "Count 0 must return STATUS_INVALID_PARAMETER_1");
    TEST_ASSERT(sys::NtWaitForMultipleObjects(65, handles, WaitType::WaitAny, false, nullptr) == NtStatus::InvalidParameter1,
                "Count > 64 must return STATUS_INVALID_PARAMETER_1");
    TEST_ASSERT(sys::NtWaitForMultipleObjects(3, nullptr, WaitType::WaitAny, false, nullptr) == NtStatus::AccessViolation,
                "Null handles pointer must return STATUS_ACCESS_VIOLATION");

    Handle invalidHandles[2] = { ev1, 0x99999 };
    TEST_ASSERT(sys::NtWaitForMultipleObjects(2, invalidHandles, WaitType::WaitAny, false, nullptr) == NtStatus::InvalidHandle,
                "Invalid handle in array must return STATUS_INVALID_HANDLE");

    // 6. Test Multi-Wait via KiSystemCall64
    uint64_t stackArgs[1] = { reinterpret_cast<uint64_t>(&timeout) };
    frame = sys::SyscallFrame{};
    frame.ssn = sys::SSN_NtWaitForMultipleObjects;
    frame.arg1 = 3;
    frame.arg2 = reinterpret_cast<uint64_t>(handles);
    frame.arg3 = static_cast<uint64_t>(WaitType::WaitAll);
    frame.arg4 = 0;
    frame.stackArgs = stackArgs;
    frame.stackArgCount = 1;
    waitStatus = dispatcher.dispatch(frame);
    TEST_ASSERT(waitStatus == NtStatus::Success, "NtWaitForMultipleObjects dispatched through KiSystemCall64 must succeed");

    // Clean up handles
    sys::NtClose(ev1);
    sys::NtClose(ev2);
    sys::NtClose(ev3);
}

void Test_LookasideLists_FastAllocAndTelemetry() {
    // 1. NonPaged Lookaside List
    ex::NPagedLookasideList npList;
    uint32_t tag = ex::makePoolTag('L', 'o', 'o', 'k');
    npList.initialize(128, tag, 16);

    std::vector<void*> blocks;
    // Initial 8 allocations: empty cache -> 8 pool misses
    for (int i = 0; i < 8; ++i) {
        void* ptr = npList.allocate();
        TEST_ASSERT(ptr != nullptr, "NPagedLookasideList allocate must return non-null pointer");
        std::memset(ptr, 0xAA, 128);
        blocks.push_back(ptr);
    }

    const auto& stats1 = npList.getStats();
    TEST_ASSERT(stats1.totalAllocates == 8, "Total allocates must be 8");
    TEST_ASSERT(stats1.allocateMisses == 8, "All 8 initial allocates must be pool misses");
    TEST_ASSERT(stats1.depth == 0, "Depth must be 0 while blocks are active");

    // Free all 8 blocks back to list
    for (void* ptr : blocks) {
        npList.free(ptr);
    }
    blocks.clear();

    const auto& stats2 = npList.getStats();
    TEST_ASSERT(stats2.totalFrees == 8, "Total frees must be 8");
    TEST_ASSERT(stats2.freeMisses == 0, "All 8 frees must be cached in lookaside list");
    TEST_ASSERT(stats2.depth == 8, "Depth must be 8 after caching freed blocks");

    // Re-allocate 8 blocks: 100% cache hits, zero new pool misses!
    for (int i = 0; i < 8; ++i) {
        void* ptr = npList.allocate();
        TEST_ASSERT(ptr != nullptr, "Cached allocate must return non-null pointer");
        blocks.push_back(ptr);
    }

    const auto& stats3 = npList.getStats();
    TEST_ASSERT(stats3.totalAllocates == 16, "Total allocates must be 16");
    TEST_ASSERT(stats3.allocateMisses == 8, "Allocate misses must still be 8 (8 consecutive cache hits!)");
    TEST_ASSERT(stats3.depth == 0, "Depth must be 0 after re-allocating all cached blocks");

    // Free and flush
    for (void* ptr : blocks) {
        npList.free(ptr);
    }
    npList.flush();
    TEST_ASSERT(npList.getStats().depth == 0, "Depth must be 0 after flush");

    // 2. Paged Lookaside List & IRQL Enforcement
    ex::PagedLookasideList pList;
    pList.initialize(64, tag, 8);

    void* pagedPtr = pList.allocate();
    TEST_ASSERT(pagedPtr != nullptr, "PagedLookasideList allocate at PASSIVE_LEVEL must succeed");
    pList.free(pagedPtr);

    // Raise IRQL to DISPATCH_LEVEL and verify BugCheck / refusal to allocate PagedPool
    ke::KIRQL oldIrql = ke::KfRaiseIrql(ke::DISPATCH_LEVEL);
    void* illegalPtr = pList.allocate();
    TEST_ASSERT(illegalPtr == nullptr, "PagedLookasideList must refuse allocation at DISPATCH_LEVEL");
    ke::KeLowerIrql(oldIrql);
}

namespace test_power {
    static std::atomic<uint32_t> g_PowerIrpCount{0};
    static std::atomic<uint32_t> g_LastMinor{0xFF};
    static std::atomic<uint32_t> g_LastSystemState{0xFF};
    static std::atomic<uint32_t> g_LastDeviceState{0xFF};

    static NtStatus PowerDispatchRoutine(io::DeviceObject* dev, io::Irp* irp) {
        (void)dev;
        if (irp && irp->majorFunction == io::IRP_MJ_POWER) {
            g_PowerIrpCount++;
            g_LastMinor = irp->minorFunction;
            g_LastSystemState = irp->byteOffset.lowPart;
            g_LastDeviceState = static_cast<uint32_t>(irp->byteOffset.highPart);
            irp->ioStatus.status = NtStatus::Success;
            return NtStatus::Success;
        }
        return NtStatus::InvalidDeviceRequest;
    }
}

void Test_PowerManagement_IrpAndShutdown() {
    po::PowerManager::get().resetForTesting();
    TEST_ASSERT(po::PowerManager::get().getSystemPowerState() == po::SystemPowerState::PowerSystemWorking,
                "Initial system power state must be PowerSystemWorking (S0)");

    // 1. Create power-aware device and driver
    io::DriverObject pwrDriver{};
    pwrDriver.driverName = L"TestPowerDriver";
    pwrDriver.setDispatch(io::IRP_MJ_POWER, test_power::PowerDispatchRoutine);

    io::DeviceObject pwrDevice{};
    pwrDevice.driverObject = &pwrDriver;
    pwrDevice.deviceType = io::DeviceType::Unknown;
    pwrDevice.deviceName = L"\\Device\\TestPowerDevice";

    po::PowerManager::get().registerDevice(&pwrDevice);
    TEST_ASSERT(po::PowerManager::get().getRegisteredDeviceCount() == 1, "Registered device count must be 1");

    // 2. Request power IRP
    test_power::g_PowerIrpCount = 0;
    NtStatus pwrStatus = po::PoRequestPowerIrp(
        &pwrDevice,
        po::IRP_MN_QUERY_POWER,
        po::SystemPowerState::PowerSystemSleeping3,
        po::DevicePowerState::PowerDeviceD2
    );
    TEST_ASSERT(NT_SUCCESS(pwrStatus), "PoRequestPowerIrp must succeed");
    TEST_ASSERT(test_power::g_PowerIrpCount == 1, "Power driver must have received 1 power IRP");
    TEST_ASSERT(test_power::g_LastMinor == po::IRP_MN_QUERY_POWER, "Minor must match IRP_MN_QUERY_POWER");
    TEST_ASSERT(test_power::g_LastSystemState == static_cast<uint32_t>(po::SystemPowerState::PowerSystemSleeping3),
                "System state must be PowerSystemSleeping3");

    // 3. Test NtShutdownSystem syscall via SyscallDispatcher (SSN 0x0118)
    auto& dispatcher = sys::SyscallDispatcher::get();

    sys::SyscallFrame frame{};
    frame.ssn = sys::SSN_NtShutdownSystem;
    frame.arg1 = static_cast<uint64_t>(po::ShutdownAction::ShutdownPowerOff);
    NtStatus shutdownStatus = dispatcher.dispatch(frame);
    TEST_ASSERT(NT_SUCCESS(shutdownStatus), "NtShutdownSystem via dispatcher must succeed");

    TEST_ASSERT(po::PowerManager::get().isShutdown(), "PowerManager must register system is shut down");
    TEST_ASSERT(po::PowerManager::get().getLastShutdownAction() == po::ShutdownAction::ShutdownPowerOff,
                "Last shutdown action must be ShutdownPowerOff");
    TEST_ASSERT(po::PowerManager::get().getSystemPowerState() == po::SystemPowerState::PowerSystemShutdown,
                "System power state must be PowerSystemShutdown (S5)");
    TEST_ASSERT(test_power::g_LastDeviceState == static_cast<uint32_t>(po::DevicePowerState::PowerDeviceD3),
                "Hardware device must have been transitioned to PowerDeviceD3 (powered off)");

    // Unregister and reset
    po::PowerManager::get().unregisterDevice(&pwrDevice);
    po::PowerManager::get().resetForTesting();
}

// ============================================================================
// Suite 24: Ring 3 Userland Bridge, TEB/PEB Context, and Dynamic Loader (ntdll)
// ============================================================================
void Test_Ntdll_SyscallStubsAndPebTeb() {
    auto& dispatcher = sys::SyscallDispatcher::get();
    dispatcher.initializeStandardTable();

    // 1. Verify TEB and PEB userland context linkage
    ps::Peb testPeb{};
    ps::Teb testTeb{};
    testPeb.imageBaseAddress = 0x0000000140000000ULL;

    ntdll::RtlSetCurrentTeb(&testTeb);
    TEST_ASSERT(ntdll::RtlGetCurrentTeb() == &testTeb, "RtlGetCurrentTeb must return set TEB context");

    NtStatus ldrStatus = ntdll::LdrInitializeThunk(&testPeb, &testTeb, 0x0000000140001000ULL);
    TEST_ASSERT(NT_SUCCESS(ldrStatus), "LdrInitializeThunk must succeed");
    TEST_ASSERT(testTeb.processEnvironmentBlock == reinterpret_cast<uint64_t>(&testPeb),
                "TEB must link directly to PEB");
    TEST_ASSERT(testTeb.ntTib.self == reinterpret_cast<uint64_t>(&testTeb),
                "TEB self pointer must point to TEB structure");
    TEST_ASSERT(ntdll::RtlGetCurrentPeb() == &testPeb,
                "RtlGetCurrentPeb must resolve to current process PEB");
    TEST_ASSERT(testPeb.processHeap != 0, "LdrInitializeThunk must allocate default process heap");
    TEST_ASSERT(testPeb.ldr != 0, "LdrInitializeThunk must initialize PEB_LDR_DATA");
    TEST_ASSERT(testPeb.processParameters != 0, "LdrInitializeThunk must initialize RTL_USER_PROCESS_PARAMETERS");

    auto* ldrData = reinterpret_cast<ldr::PebLdrData*>(testPeb.ldr);
    TEST_ASSERT(ldrData->initialized == 1, "PebLdrData must be flagged as initialized");
    TEST_ASSERT(!ldrData->inLoadOrderModuleList.isEmpty(), "Load order module list must contain entries");
    TEST_ASSERT(ldr::DynamicLoader::get().getLoadedModuleCount() >= 2,
                "DynamicLoader must register ntdll.dll and host application modules");

    // 2. Test Dynamic Module Loading and Symbol Resolution
    uintptr_t kernel32Base = 0;
    UnicodeString k32Name(L"kernel32.dll");
    NtStatus loadStatus = ntdll::LdrLoadDll(nullptr, nullptr, &k32Name, &kernel32Base);
    TEST_ASSERT(NT_SUCCESS(loadStatus), "LdrLoadDll for kernel32.dll must succeed");
    TEST_ASSERT(kernel32Base != 0, "Loaded module base must be non-zero");

    ldr::DynamicLoader::get().registerExport("ntdll.dll", "RtlAllocateHeap", reinterpret_cast<void*>(ntdll::RtlAllocateHeap));
    void* procAddr = nullptr;
    NtStatus getProcStatus = ntdll::LdrGetProcedureAddress(0x00007FF800000000ULL, "RtlAllocateHeap", 0, &procAddr);
    TEST_ASSERT(NT_SUCCESS(getProcStatus), "LdrGetProcedureAddress must find registered export");
    TEST_ASSERT(procAddr == reinterpret_cast<void*>(ntdll::RtlAllocateHeap), "Resolved procedure address must match");

    // 3. Test Userland Syscall Stubs (Nt* and Zw* Parity)
    Handle evHandle = 0;
    NtStatus createEvStatus = ntdll::NtCreateEvent(&evHandle, 0x1F0003, nullptr, 0, false);
    TEST_ASSERT(NT_SUCCESS(createEvStatus) && evHandle != 0, "NtCreateEvent userland stub must succeed");

    NtStatus setStatus = ntdll::NtSetEvent(evHandle);
    TEST_ASSERT(NT_SUCCESS(setStatus), "NtSetEvent userland stub must succeed");

    NtStatus waitStatus = ntdll::NtWaitForSingleObject(evHandle, false, nullptr);
    TEST_ASSERT(NT_SUCCESS(waitStatus), "NtWaitForSingleObject userland stub must succeed on signaled event");

    NtStatus resetStatus = ntdll::NtResetEvent(evHandle);
    TEST_ASSERT(NT_SUCCESS(resetStatus), "NtResetEvent userland stub must succeed");

    // Test Zw* alias equivalence
    NtStatus zwSetStatus = ntdll::ZwSetEvent(evHandle);
    TEST_ASSERT(NT_SUCCESS(zwSetStatus), "ZwSetEvent alias must function identically to NtSetEvent");

    ntdll::NtClose(evHandle);

    // 4. Test Virtual Memory Syscall Stubs
    uintptr_t testVmBase = 0;
    size_t testVmSize = 4096;
    NtStatus vmAllocStatus = ntdll::NtAllocateVirtualMemory(
        0, &testVmBase, 0, &testVmSize, mm::MEM_COMMIT | mm::MEM_RESERVE, mm::PAGE_READWRITE
    );
    TEST_ASSERT(NT_SUCCESS(vmAllocStatus), "NtAllocateVirtualMemory userland stub must succeed");
    TEST_ASSERT(testVmBase != 0, "Allocated base address must be non-zero");

    NtStatus vmFreeStatus = ntdll::NtFreeVirtualMemory(0, &testVmBase, &testVmSize, mm::MEM_RELEASE);
    TEST_ASSERT(NT_SUCCESS(vmFreeStatus), "NtFreeVirtualMemory userland stub must succeed");

    // 5. Test Delay Execution Syscall Stub
    LargeInteger zeroDelay{};
    zeroDelay.quadPart = 0;
    NtStatus delayStatus = ntdll::NtDelayExecution(false, &zeroDelay);
    TEST_ASSERT(NT_SUCCESS(delayStatus), "NtDelayExecution with zero interval must cooperatively yield");

    // 6. Test Console Output Syscall Stub
    const char banner[] = "MicaNT NTDLL Syscall Output Test\n";
    IoStatusBlock iosb{};
    NtStatus writeStatus = ntdll::NtWriteFile(0x14, 0, nullptr, nullptr, &iosb, banner, static_cast<uint32_t>(sizeof(banner) - 1));
    TEST_ASSERT(NT_SUCCESS(writeStatus) && iosb.information == sizeof(banner) - 1,
                "NtWriteFile to standard output console handle must succeed");
    ntdll::RtlSetCurrentTeb(nullptr);
}

// ============================================================================
// Suite 25: Userland Heap Manager (RtlAllocateHeap & Coalescing)
// ============================================================================
void Test_UserlandHeap_RtlAllocateAndCoalescing() {
    // 1. Create growable userland heap
    void* heap = ntdll::RtlCreateHeap(
        ntdll::HEAP_GROWABLE,
        nullptr,
        1024 * 1024, // 1 MB reserve
        64 * 1024,   // 64 KB initial commit
        nullptr,
        nullptr
    );
    TEST_ASSERT(heap != nullptr, "RtlCreateHeap must return valid heap handle");

    auto* userHeap = reinterpret_cast<heap::UserHeap*>(heap);

    // 2. Test Zero-Memory Allocation
    void* pZero = ntdll::RtlAllocateHeap(heap, ntdll::HEAP_ZERO_MEMORY, 128);
    TEST_ASSERT(pZero != nullptr, "RtlAllocateHeap with HEAP_ZERO_MEMORY must succeed");
    const auto* bytePtr = static_cast<const uint8_t*>(pZero);
    bool isZeroed = true;
    for (size_t i = 0; i < 128; ++i) {
        if (bytePtr[i] != 0) { isZeroed = false; break; }
    }
    TEST_ASSERT(isZeroed, "HEAP_ZERO_MEMORY must guarantee all allocated bytes are 0");

    size_t zeroSize = ntdll::RtlSizeHeap(heap, 0, pZero);
    TEST_ASSERT(zeroSize >= 128, "RtlSizeHeap must report at least requested allocation size");
    bool freeZeroOk = ntdll::RtlFreeHeap(heap, 0, pZero);
    TEST_ASSERT(freeZeroOk, "RtlFreeHeap must release pZero cleanly");

    // 3. Test Sequential Allocations and Bidirectional Coalescing
    void* pA = ntdll::RtlAllocateHeap(heap, 0, 64);
    void* pB = ntdll::RtlAllocateHeap(heap, 0, 128);
    void* pC = ntdll::RtlAllocateHeap(heap, 0, 256);
    void* pD = ntdll::RtlAllocateHeap(heap, 0, 512);

    TEST_ASSERT(pA && pB && pC && pD, "All 4 sequential heap allocations must succeed");
    TEST_ASSERT(userHeap->getTelemetry().activeAllocates >= 4, "Active allocation count must track 4 blocks");

    // Free pB and then pC: should trigger forward and backward coalescing
    uint64_t initialCoalesce = userHeap->getTelemetry().coalesceCount;
    bool freeBOk = ntdll::RtlFreeHeap(heap, 0, pB);
    TEST_ASSERT(freeBOk, "RtlFreeHeap(pB) must succeed");

    bool freeCOk = ntdll::RtlFreeHeap(heap, 0, pC);
    TEST_ASSERT(freeCOk, "RtlFreeHeap(pC) must succeed and trigger coalescing with adjacent free block");
    TEST_ASSERT(userHeap->getTelemetry().coalesceCount > initialCoalesce,
                "Freeing adjacent blocks must increment heap coalesce count");

    // Allocate 350 bytes: exceeds pB (128) and pC (256) individually, but fits in coalesced pB+pC!
    void* pMerged = ntdll::RtlAllocateHeap(heap, 0, 350);
    TEST_ASSERT(pMerged != nullptr, "Allocation fitting into coalesced space must succeed");
    TEST_ASSERT(pMerged == pB, "Allocation must reuse the coalesced address of the merged blocks");

    // 4. Test RtlReAllocateHeap
    void* pRealloc = ntdll::RtlReAllocateHeap(heap, 0, pMerged, 700);
    TEST_ASSERT(pRealloc != nullptr, "RtlReAllocateHeap to larger size must succeed");
    TEST_ASSERT(ntdll::RtlSizeHeap(heap, 0, pRealloc) >= 700, "RtlSizeHeap must report at least 700 bytes");

    // Clean up allocated blocks
    ntdll::RtlFreeHeap(heap, 0, pA);
    ntdll::RtlFreeHeap(heap, 0, pRealloc);
    ntdll::RtlFreeHeap(heap, 0, pD);
    TEST_ASSERT(userHeap->getTelemetry().activeAllocates == 0,
                "Active allocation count must return to 0 after all blocks freed");

    // 5. Test Dynamic Heap Growth
    size_t initialSegments = userHeap->getTelemetry().segmentCount;
    void* pHuge = ntdll::RtlAllocateHeap(heap, 0, 128 * 1024); // 128 KB allocation exceeds 64 KB initial segment
    TEST_ASSERT(pHuge != nullptr, "Growable heap must expand to satisfy large allocation");
    TEST_ASSERT(userHeap->getTelemetry().segmentCount > initialSegments,
                "Dynamic segment count must increase after heap expansion");
    ntdll::RtlFreeHeap(heap, 0, pHuge);

    // 6. Test Destroy Heap
    void* destroyed = ntdll::RtlDestroyHeap(heap);
    TEST_ASSERT(destroyed == nullptr, "RtlDestroyHeap must return nullptr and tear down heap");
}

void Test_UefiBootloader_GopAndMemoryMap() {
    // 1. Test EfiGuid equality
    uefi::EfiGuid g1 = uefi::EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    uefi::EfiGuid g2 = uefi::EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    uefi::EfiGuid g3 = uefi::ACPI_20_TABLE_GUID;
    TEST_ASSERT(g1 == g2, "Identical EFI GUIDs must evaluate equal");
    TEST_ASSERT(!(g1 == g3), "Distinct EFI GUIDs must not evaluate equal");

    // 2. Test EfiMemoryType to NT LoaderMemoryType mapping
    TEST_ASSERT(uefi::EfiMemoryTypeToNt(uefi::EfiMemoryType::EfiConventionalMemory) == boot::LoaderMemoryType::LoaderFree,
                "Conventional memory must map to LoaderFree");
    TEST_ASSERT(uefi::EfiMemoryTypeToNt(uefi::EfiMemoryType::EfiLoaderCode) == boot::LoaderMemoryType::LoaderSystemCode,
                "LoaderCode must map to LoaderSystemCode");
    TEST_ASSERT(uefi::EfiMemoryTypeToNt(uefi::EfiMemoryType::EfiBootServicesCode) == boot::LoaderMemoryType::LoaderFirmwareTemporary,
                "BootServicesCode must map to LoaderFirmwareTemporary");
    TEST_ASSERT(uefi::EfiMemoryTypeToNt(uefi::EfiMemoryType::EfiACPIReclaimMemory) == boot::LoaderMemoryType::LoaderSpecialMemory,
                "ACPIReclaim must map to LoaderSpecialMemory");

    // 3. Setup mock GOP Protocol
    uefi::EfiGraphicsOutputModeInformation modeInfo{
        .version = 0,
        .horizontalResolution = 1920,
        .verticalResolution = 1080,
        .pixelFormat = uefi::EfiGraphicsPixelFormat::PixelBlueGreenRedReserved8BitPerColor,
        .pixelInformation = {},
        .pixelsPerScanLine = 1920
    };

    uefi::EfiGraphicsOutputProtocolMode gopMode{
        .maxMode = 1,
        .mode = 0,
        .info = &modeInfo,
        .sizeOfInfo = sizeof(uefi::EfiGraphicsOutputModeInformation),
        .frameBufferBase = 0x00000000E0000000ULL,
        .frameBufferSize = 1920 * 1080 * 4
    };

    uefi::EfiGraphicsOutputProtocol gop{
        .queryMode = nullptr,
        .setMode = nullptr,
        .blt = nullptr,
        .mode = &gopMode
    };

    // 4. Setup mock ACPI 2.0 System Table
    uint64_t fakeRsdpAddress = 0x000000007FEF0000ULL;
    uefi::EfiConfigurationTable configTables[1] = {
        {
            .vendorGuid = uefi::ACPI_20_TABLE_GUID,
            .vendorTable = reinterpret_cast<void*>(fakeRsdpAddress)
        }
    };

    uefi::EfiSystemTable sysTable{};
    sysTable.numberOfTableEntries = 1;
    sysTable.configurationTable = configTables;

    // 5. Setup mock UEFI memory descriptors
    std::vector<uefi::EfiMemoryDescriptor> uefiMap = {
        {
            .type = static_cast<uint32_t>(uefi::EfiMemoryType::EfiConventionalMemory),
            .pad = 0,
            .physicalStart = 0x1000000, // 16 MB
            .virtualStart = 0,
            .numberOfPages = 65536,     // 256 MB
            .attribute = 0
        },
        {
            .type = static_cast<uint32_t>(uefi::EfiMemoryType::EfiBootServicesCode),
            .pad = 0,
            .physicalStart = 0x11000000,
            .virtualStart = 0,
            .numberOfPages = 1024,      // 4 MB
            .attribute = 0
        }
    };

    // 6. Execute BuildLpbFromUefi
    boot::LoaderParameterBlock lpb = uefi::BuildLpbFromUefi(&sysTable, &gop, uefiMap);

    TEST_ASSERT(lpb.framebuffer.physicalBase == 0x00000000E0000000ULL,
                "LPB must inherit GOP physical framebuffer base");
    TEST_ASSERT(lpb.framebuffer.width == 1920 && lpb.framebuffer.height == 1080,
                "LPB must inherit GOP resolution");
    TEST_ASSERT(lpb.framebuffer.pixelFormat == 1,
                "LPB must reflect BGRA pixel format");
    TEST_ASSERT(lpb.acpiTablePhysicalAddress == fakeRsdpAddress,
                "LPB must discover ACPI 2.0 RSDP in EFI configuration table");
    TEST_ASSERT(lpb.memoryDescriptors.size() == 2,
                "LPB must translate all UEFI memory descriptors");
    TEST_ASSERT(lpb.memoryDescriptors[0].memoryType == boot::LoaderMemoryType::LoaderFree,
                "First descriptor must be translated to LoaderFree");
    TEST_ASSERT(lpb.memoryDescriptors[1].memoryType == boot::LoaderMemoryType::LoaderFirmwareTemporary,
                "Second descriptor must be translated to LoaderFirmwareTemporary");
}

void Test_BootVid_FramebufferAndSplashRenderer() {
    // 1. Initialize Virtual Framebuffer Driver
    bootvid::BootVideoDriver driver;
    bool initOk = driver.initializeVirtual(640, 480);
    TEST_ASSERT(initOk, "Virtual framebuffer initialization must succeed");
    TEST_ASSERT(driver.getWidth() == 640 && driver.getHeight() == 480, "Driver width and height must match 640x480");
    TEST_ASSERT(driver.getPitch() == 640, "Default pitch must match width");

    // 2. Clear Screen and Put/Get Pixel
    driver.clear(bootvid::Color::micaDark());
    TEST_ASSERT(driver.getPixel(0, 0) == bootvid::Color::micaDark(), "Origin pixel must match clear color");
    TEST_ASSERT(driver.getPixel(639, 479) == bootvid::Color::micaDark(), "Corner pixel must match clear color");

    driver.putPixel(120, 80, bootvid::Color::micaCyan());
    TEST_ASSERT(driver.getPixel(120, 80) == bootvid::Color::micaCyan(), "PutPixel at (120,80) must match micaCyan");

    // 3. Fill and Draw Rectangle
    driver.fillRectangle(10, 10, 40, 30, bootvid::Color::micaBlue());
    TEST_ASSERT(driver.getPixel(20, 20) == bootvid::Color::micaBlue(), "Inside filled rectangle must match micaBlue");
    TEST_ASSERT(driver.getPixel(5, 5) == bootvid::Color::micaDark(), "Outside filled rectangle must remain micaDark");

    driver.drawRectangle(100, 100, 50, 50, bootvid::Color::white(), 2);
    TEST_ASSERT(driver.getPixel(100, 100) == bootvid::Color::white(), "Border pixel must be white");
    TEST_ASSERT(driver.getPixel(125, 125) == bootvid::Color::micaDark(), "Interior of hollow rectangle must remain unchanged");

    // 4. Gradients, Lines, and Circles
    driver.drawVerticalGradient(200, 200, 40, 40, bootvid::Color::white(), bootvid::Color::black());
    TEST_ASSERT(driver.getPixel(200, 200) == bootvid::Color::white(), "Gradient top must be white");
    TEST_ASSERT(driver.getPixel(200, 239) != bootvid::Color::white(), "Gradient bottom must interpolate towards black");

    driver.drawLine(0, 0, 20, 20, bootvid::Color::micaAmber());
    TEST_ASSERT(driver.getPixel(10, 10) == bootvid::Color::micaAmber(), "Diagonal line pixel must match micaAmber");

    driver.drawCircle(300, 150, 25, bootvid::Color::micaViolet(), true);
    TEST_ASSERT(driver.getPixel(300, 150) == bootvid::Color::micaViolet(), "Filled circle center must match micaViolet");

    // 5. Typography (8x8 font rendering)
    driver.drawString(10, 300, "MicaNT", bootvid::Color::white(), bootvid::Color::black(), 2);
    bool foundFg = false;
    for (uint32_t py = 300; py < 316; ++py) {
        for (uint32_t px = 10; px < 26; ++px) {
            if (driver.getPixel(px, py) == bootvid::Color::white()) {
                foundFg = true;
                break;
            }
        }
        if (foundFg) break;
    }
    TEST_ASSERT(foundFg, "Rendered font glyph must produce white foreground pixels");

    // 6. Dave Cutler's 1988 DEC Mica Prism Procedural Emblem
    driver.drawMicaPrism(320, 240, 32);
    TEST_ASSERT(driver.getPixel(320, 240) == bootvid::Color::white(),
                "Center ridge of Mica Prism must contain white quartz highlight");

    // 7. BMP Codec (User-Custom Boot Logo)
    bootvid::BmpImage customLogo;
    customLogo.width = 16;
    customLogo.height = 16;
    customLogo.bpp = 32;
    customLogo.pixels.resize(16 * 16, bootvid::Color::micaSurface());
    for (uint32_t i = 0; i < 16; ++i) {
        customLogo.setPixel(i, 8, bootvid::Color::micaCyan());
        customLogo.setPixel(8, i, bootvid::Color::micaAmber());
    }

    // Encode to BMP byte buffer
    std::vector<uint8_t> encodedBmp = bootvid::BmpCodec::encode(customLogo);
    TEST_ASSERT(!encodedBmp.empty(), "BMP encoder must produce non-empty byte vector");
    TEST_ASSERT(encodedBmp[0] == 'B' && encodedBmp[1] == 'M', "BMP magic must be 'BM' (0x4D42)");

    // Decode back from BMP byte buffer
    bootvid::BmpImage decodedLogo;
    bool decodeOk = bootvid::BmpCodec::decode(encodedBmp, decodedLogo);
    TEST_ASSERT(decodeOk, "BMP decoder must successfully parse generated 32-bit BMP");
    TEST_ASSERT(decodedLogo.width == 16 && decodedLogo.height == 16, "Decoded dimensions must match 16x16");
    TEST_ASSERT(decodedLogo.getPixel(8, 8) == bootvid::Color::micaAmber(), "Decoded pixel must match original cross-point");

    // Blit custom logo to framebuffer
    driver.drawBitmap(50, 50, decodedLogo);
    TEST_ASSERT(driver.getPixel(58, 58) == bootvid::Color::micaAmber(),
                "Framebuffer must reflect blitted custom BMP image");

    // 8. Render Full Boot Splash Screen with Custom Logo
    driver.renderBootSplash("Testing MicaNT Bootvid...", 0.85f, &decodedLogo);
    TEST_ASSERT(driver.getPixel(320, 10) != bootvid::Color::black(),
                "Boot splash background gradient must be rendered");

    // 9. Classic NT bootvid.dll Export API Compatibility
    auto& subsys = bootvid::BootVideoSubsystem::get();
    bool subsysInit = subsys.initializeVirtual(320, 240);
    TEST_ASSERT(subsysInit, "BootVideoSubsystem virtual initialization must succeed");
    subsys.vidSolidColorFill(10, 10, 30, 30, bootvid::Color::micaGreen());
    TEST_ASSERT(subsys.getDriver().getPixel(20, 20) == bootvid::Color::micaGreen(),
                "vidSolidColorFill must plot pixels correctly");
    subsys.vidResetDisplay(true);
    TEST_ASSERT(subsys.getDriver().getPixel(20, 20) == bootvid::Color::black(),
                "vidResetDisplay must clear screen to black");
}

void Test_Csrss_ProcessRegistrationAndAlpc() {
    auto& server = csrss::CsrSubsystemServer::get();
    bool started = server.start();
    TEST_ASSERT(started, "CSRSS server must start and bind \\RPC Control\\WindowsSubsystem");
    TEST_ASSERT(server.isRunning(), "CSRSS server isRunning must return true");

    // 1. Register Process
    NtStatus regStatus = server.registerProcess(2001, 1000, L"C:\\Windows\\System32\\cmd.exe", 0x14);
    TEST_ASSERT(NT_SUCCESS(regStatus), "CSRSS registerProcess must succeed");
    TEST_ASSERT(server.getActiveProcessCount() == 1, "Active process count must equal 1");

    auto procOpt = server.getProcess(2001);
    TEST_ASSERT(procOpt.has_value(), "Registered process must be retrievable by PID");
    TEST_ASSERT(procOpt->processId == 2001, "Retrieved process ID must match");
    TEST_ASSERT(procOpt->parentProcessId == 1000, "Parent process ID must match");
    TEST_ASSERT(procOpt->consoleHandle == 0x14, "Console handle must match");

    // 2. Register Thread
    NtStatus thStatus = server.registerThread(2001, 10, 0x100);
    TEST_ASSERT(NT_SUCCESS(thStatus), "CSRSS registerThread must succeed");
    auto procWithThread = server.getProcess(2001);
    TEST_ASSERT(procWithThread->threads.size() == 1, "Thread list must reflect registered thread");

    // 3. Dispatch CSR API Message
    csrss::CsrApiMessage apiMsg{};
    apiMsg.apiNumber = csrss::CsrApiNumber::ProcessCreate;
    apiMsg.data.processCreate.processId = 2002;
    apiMsg.data.processCreate.parentProcessId = 2001;
    apiMsg.data.processCreate.flags = 0;
    apiMsg.data.processCreate.consoleHandle = 0x20;
    apiMsg.stringPayload = L"C:\\Windows\\System32\\notepad.exe";

    NtStatus dispStatus = server.dispatchApi(apiMsg);
    TEST_ASSERT(NT_SUCCESS(dispStatus), "CSRSS dispatchApi(ProcessCreate) must succeed");
    TEST_ASSERT(server.getActiveProcessCount() == 2, "Active process count must equal 2 after message dispatch");

    // 4. Terminate Process
    NtStatus termStatus = server.terminateProcess(2002, 0);
    TEST_ASSERT(NT_SUCCESS(termStatus), "CSRSS terminateProcess must succeed");
    TEST_ASSERT(server.getActiveProcessCount() == 1, "Active process count must return to 1 after termination");

    // 5. Telemetry & Stop
    auto telemetry = server.getTelemetry();
    TEST_ASSERT(telemetry.totalProcessesCreated >= 2, "Telemetry must record at least 2 processes created");
    TEST_ASSERT(telemetry.totalProcessesTerminated >= 1, "Telemetry must record at least 1 process terminated");

    server.stop();
    TEST_ASSERT(!server.isRunning(), "CSRSS server isRunning must return false after stop");
}

void Test_Conhost_ScreenBufferAndFramebufferBlit() {
    // 1. Test ConsoleScreenBuffer dimensions & coordinates
    conhost::ConsoleScreenBuffer buffer(40, 10);
    TEST_ASSERT(buffer.getWidth() == 40 && buffer.getHeight() == 10, "Buffer dimensions must match 40x10");
    TEST_ASSERT(buffer.getCursorPosition().x == 0 && buffer.getCursorPosition().y == 0,
                "Initial cursor must be at (0, 0)");

    // 2. Character Output & Formatting
    buffer.writeString(L"Hello, MicaNT!\n");
    TEST_ASSERT(buffer.getCursorPosition().x == 0, "Newline must reset cursor X to 0");
    TEST_ASSERT(buffer.getCursorPosition().y == 1, "Newline must increment cursor Y to 1");

    // Test cell contents
    TEST_ASSERT(buffer.getCell(0, 0).character == L'H', "Cell (0,0) must contain 'H'");
    TEST_ASSERT(buffer.getCell(1, 0).character == L'e', "Cell (1,0) must contain 'e'");

    // 3. Tab and Backspace
    buffer.writeChar(L'A');
    buffer.writeChar(L'\b');
    TEST_ASSERT(buffer.getCell(0, 1).character == L' ', "Backspace must clear cell to space");
    TEST_ASSERT(buffer.getCursorPosition().x == 0, "Backspace must decrement cursor X");

    buffer.writeChar(L'\t');
    TEST_ASSERT(buffer.getCursorPosition().x == 8, "Tab must advance cursor to column 8");

    // 4. Scrolling
    for (int i = 0; i < 15; ++i) {
        buffer.writeString(L"Scroll Line\n");
    }
    TEST_ASSERT(buffer.getCursorPosition().y == 9, "Cursor Y must be clamped to bottom row (height - 1)");

    // 5. ConhostManager Session Allocation
    auto& mgr = conhost::ConhostManager::get();
    auto session = mgr.allocateConsole(3001, L"Mica Shell");
    TEST_ASSERT(session != nullptr, "ConhostManager allocateConsole must return non-null session");
    TEST_ASSERT(session->getTitle() == L"Mica Shell", "Console title must match");
    TEST_ASSERT(session->getInputHandle() == 0x10, "Default input handle must be 0x10");
    TEST_ASSERT(session->getOutputHandle() == 0x14, "Default output handle must be 0x14");

    session->writeOutput(L"Terminal Active\n");
    TEST_ASSERT(session->getScreenBuffer().getCell(0, 0).character == L'T', "ConsoleSession output must write to screen buffer");

    // Input queueing & reading
    session->queueInput(L"dir\r\n");
    wchar_t readBuf[16]{};
    size_t readCount = session->readInput(readBuf, 5);
    TEST_ASSERT(readCount == 5, "readInput must return requested character count");
    TEST_ASSERT(readBuf[0] == L'd' && readBuf[1] == L'i' && readBuf[2] == L'r', "readInput must match queued text");

    // 6. Framebuffer Terminal Blitting
    bootvid::BootVideoDriver vdriver;
    bool vInit = vdriver.initializeVirtual(640, 480);
    TEST_ASSERT(vInit, "Virtual framebuffer initialization must succeed");
    vdriver.clear(bootvid::Color::black());

    session->getScreenBuffer().renderToFramebuffer(vdriver, 20, 20, 1);
    // Character cell (0,0) had 'T'. Verify foreground pixels were plotted
    bool hasFg = false;
    for (uint32_t py = 20; py < 28; ++py) {
        for (uint32_t px = 20; px < 28; ++px) {
            if (vdriver.getPixel(px, py) != bootvid::Color::black()) {
                hasFg = true;
                break;
            }
        }
        if (hasFg) break;
    }
    TEST_ASSERT(hasFg, "Console terminal renderToFramebuffer must blit characters to framebuffer");

    mgr.freeConsole(3001);
}

void Test_Kernel32_Win32ApiParity() {
    // 1. Process & Thread Identity
    win32::DWORD pid = win32::GetCurrentProcessId();
    win32::DWORD tid = win32::GetCurrentThreadId();
    TEST_ASSERT(pid > 0 && tid > 0, "GetCurrentProcessId and GetCurrentThreadId must return non-zero IDs");

    // 2. Default Process Heap
    win32::HANDLE hHeap = win32::GetProcessHeap();
    TEST_ASSERT(hHeap != nullptr, "GetProcessHeap must return non-null default heap");

    win32::LPVOID pBlock = win32::HeapAlloc(hHeap, 0, 128);
    TEST_ASSERT(pBlock != nullptr, "HeapAlloc 128 bytes must succeed");
    win32::SIZE_T blockSize = win32::HeapSize(hHeap, 0, pBlock);
    TEST_ASSERT(blockSize >= 128, "HeapSize must report at least 128 bytes");

    win32::LPVOID pRealloc = win32::HeapReAlloc(hHeap, 0, pBlock, 256);
    TEST_ASSERT(pRealloc != nullptr, "HeapReAlloc 256 bytes must succeed");
    win32::BOOL freeOk = win32::HeapFree(hHeap, 0, pRealloc);
    TEST_ASSERT(freeOk == win32::TRUE, "HeapFree must succeed");

    // 3. VirtualAlloc & VirtualFree
    win32::LPVOID pPages = win32::VirtualAlloc(nullptr, 65536, win32::MEM_COMMIT | win32::MEM_RESERVE, win32::PAGE_READWRITE);
    TEST_ASSERT(pPages != nullptr, "VirtualAlloc 64KB must succeed");
    win32::BOOL virtFree = win32::VirtualFree(pPages, 0, win32::MEM_RELEASE);
    TEST_ASSERT(virtFree == win32::TRUE, "VirtualFree must succeed");

    // 4. Console Management
    win32::BOOL allocCon = win32::AllocConsole();
    TEST_ASSERT(allocCon == win32::TRUE, "AllocConsole must succeed");

    win32::BOOL setTitle = win32::SetConsoleTitleW(L"MicaNT Test Console");
    TEST_ASSERT(setTitle == win32::TRUE, "SetConsoleTitleW must succeed");

    wchar_t titleBuf[32]{};
    win32::DWORD titleLen = win32::GetConsoleTitleW(titleBuf, 32);
    TEST_ASSERT(titleLen > 0, "GetConsoleTitleW must return non-zero length");
    TEST_ASSERT(std::wstring_view(titleBuf) == L"MicaNT Test Console", "GetConsoleTitleW must match set title");

    win32::HANDLE hOut = win32::GetStdHandle(win32::STD_OUTPUT_HANDLE);
    TEST_ASSERT(hOut != win32::INVALID_HANDLE_VALUE, "GetStdHandle(STD_OUTPUT_HANDLE) must be valid");

    const wchar_t* helloMsg = L"Test message\n";
    win32::DWORD written = 0;
    win32::BOOL writeOk = win32::WriteConsoleW(hOut, helloMsg, static_cast<win32::DWORD>(wcslen(helloMsg)), &written, nullptr);
    TEST_ASSERT(writeOk == win32::TRUE, "WriteConsoleW must succeed");
    TEST_ASSERT(written == static_cast<win32::DWORD>(wcslen(helloMsg)), "Chars written must match requested count");

    win32::BOOL freeCon = win32::FreeConsole();
    TEST_ASSERT(freeCon == win32::TRUE, "FreeConsole must succeed");

    // 5. Event Synchronization
    win32::HANDLE hEv = win32::CreateEventW(nullptr, win32::FALSE, win32::FALSE, L"TestWin32Event");
    TEST_ASSERT(hEv != nullptr, "CreateEventW must succeed");

    win32::DWORD wait1 = win32::WaitForSingleObject(hEv, 0);
    TEST_ASSERT(wait1 == win32::WAIT_TIMEOUT, "WaitForSingleObject on unsignaled event must return WAIT_TIMEOUT");

    win32::BOOL setOk = win32::SetEvent(hEv);
    TEST_ASSERT(setOk == win32::TRUE, "SetEvent must succeed");

    win32::DWORD wait2 = win32::WaitForSingleObject(hEv, 50);
    TEST_ASSERT(wait2 == win32::WAIT_OBJECT_0, "WaitForSingleObject on signaled event must return WAIT_OBJECT_0");

    win32::CloseHandle(hEv);

    // 6. System Info and Time
    win32::SYSTEM_INFO sysInfo{};
    win32::GetSystemInfo(&sysInfo);
    TEST_ASSERT(sysInfo.dwNumberOfProcessors == 4, "GetSystemInfo must report 4 processors");
    TEST_ASSERT(sysInfo.dwPageSize == 4096, "GetSystemInfo page size must be 4096");

    uint64_t tick1 = win32::GetTickCount64();
    win32::Sleep(5);
    uint64_t tick2 = win32::GetTickCount64();
    TEST_ASSERT(tick2 >= tick1, "GetTickCount64 must be monotonic non-decreasing");
}

// ============================================================================
// Suite 31: WoW64 Subsystem, PEB32/TEB32, and Heaven's Gate Mode Transition
// ============================================================================
void Test_Wow64_PebTebAndHeavensGate() {
    // 1. Test 32-Bit PE Header Parsing (PeLoader::inspect32)
    std::vector<uint8_t> pe32Buffer(sizeof(pe::ImageDosHeader) + sizeof(pe::ImageNtHeaders32) + sizeof(pe::ImageSectionHeader), 0);
    auto* dos = reinterpret_cast<pe::ImageDosHeader*>(pe32Buffer.data());
    dos->e_magic = pe::DOS_MAGIC;
    dos->e_lfanew = sizeof(pe::ImageDosHeader);

    auto* nt32 = reinterpret_cast<pe::ImageNtHeaders32*>(pe32Buffer.data() + dos->e_lfanew);
    nt32->signature = pe::NT_SIGNATURE;
    nt32->fileHeader.machine = pe::MACHINE_I386;
    nt32->fileHeader.numberOfSections = 1;
    nt32->fileHeader.sizeOfOptionalHeader = sizeof(pe::ImageOptionalHeader32);
    nt32->optionalHeader.magic = pe::PE32_MAGIC;
    nt32->optionalHeader.addressOfEntryPoint = 0x1200;
    nt32->optionalHeader.imageBase = 0x00400000;

    auto* sec = reinterpret_cast<pe::ImageSectionHeader*>(
        pe32Buffer.data() + dos->e_lfanew + sizeof(uint32_t) + sizeof(pe::ImageFileHeader) + nt32->fileHeader.sizeOfOptionalHeader
    );
    std::memcpy(sec->name, ".text\0\0\0", 8);
    sec->virtualAddress = 0x1000;
    sec->misc.virtualSize = 0x1500;
    sec->characteristics = pe::IMAGE_SCN_MEM_EXECUTE | pe::IMAGE_SCN_MEM_READ;

    pe::ImageNtHeaders32 parsedHeaders{};
    std::vector<pe::ImageSectionHeader> parsedSections;
    NtStatus stInspect = pe::PeLoader::inspect32(pe32Buffer, parsedHeaders, parsedSections);
    TEST_ASSERT(NT_SUCCESS(stInspect), "PeLoader::inspect32 must successfully parse valid 32-bit PE");
    TEST_ASSERT(parsedHeaders.fileHeader.machine == pe::MACHINE_I386, "Machine must be IMAGE_FILE_MACHINE_I386 (0x014C)");
    TEST_ASSERT(parsedHeaders.optionalHeader.magic == pe::PE32_MAGIC, "Optional header magic must be PE32 (0x010B)");
    TEST_ASSERT(parsedHeaders.optionalHeader.imageBase == 0x00400000, "Image base must be 0x00400000");
    TEST_ASSERT(parsedSections.size() == 1, "Must contain exactly 1 section");
    TEST_ASSERT(parsedSections[0].getName() == ".text", "Section name must match .text");

    // 2. Test PEB32 and TEB32 Layout & Linkage
    wow64::ProcessEnvironmentBlock32 peb32{};
    peb32.imageBaseAddress = 0x00400000;
    peb32.osBuildNumber = 26100;
    peb32.numberOfProcessors = 4;

    wow64::ThreadEnvironmentBlock32 teb32{};
    teb32.self = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&teb32) & 0xFFFFFFFF);
    teb32.processEnvironmentBlock = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&peb32) & 0xFFFFFFFF);
    teb32.clientId.uniqueProcess = 1001;
    teb32.clientId.uniqueThread = 10;
    teb32.currentLocale = 0x0409;

    wow64::RtlSetCurrentTeb32(&teb32, &peb32);
    TEST_ASSERT(wow64::RtlGetCurrentTeb32() == &teb32, "RtlGetCurrentTeb32 must return registered TEB32");
    TEST_ASSERT(wow64::RtlGetCurrentPeb32() == &peb32, "RtlGetCurrentPeb32 must resolve through TEB32 to PEB32");
    TEST_ASSERT(wow64::RtlGetCurrentTeb32()->clientId.uniqueProcess == 1001, "UniqueProcess must match 1001");
    TEST_ASSERT(wow64::RtlGetCurrentTeb32()->clientId.uniqueThread == 10, "UniqueThread must match 10");
    TEST_ASSERT(wow64::RtlGetCurrentPeb32()->osBuildNumber == 26100, "PEB32 OS build number must match 26100");

    // 3. Test Heaven's Gate Mode Switcher (HeavensGate)
    wow64::Wow64Context32 ctx32{};
    ctx32.segCs = wow64::WOW64_CS_32BIT;
    ctx32.segSs = wow64::WOW64_SS_32BIT;
    ctx32.eip = 0x00401020;
    ctx32.eax = 0x00000018; // SSN for NtAllocateVirtualMemory
    ctx32.ebx = 0x12345678;

    uint64_t rip64 = 0;
    uint64_t targetRip64 = 0x00007FF800050000ULL; // 64-bit wow64cpu thunk entry
    bool entered = wow64::HeavensGate::enter64BitMode(ctx32, rip64, targetRip64);
    TEST_ASSERT(entered, "HeavensGate::enter64BitMode must succeed from 32-bit compatibility mode");
    TEST_ASSERT(ctx32.segCs == wow64::WOW64_CS_64BIT, "Code segment selector must switch to 0x33 (64-bit long mode)");
    TEST_ASSERT(rip64 == targetRip64, "RIP must be configured to target 64-bit entry point");

    // Re-entering when already in 64-bit mode must fail
    bool reenter = wow64::HeavensGate::enter64BitMode(ctx32, rip64, targetRip64);
    TEST_ASSERT(!reenter, "HeavensGate::enter64BitMode must reject re-entry when already in 64-bit mode");

    // Exit back to 32-bit compatibility mode
    uint32_t returnEip = 0x00401025;
    bool exited = wow64::HeavensGate::exitTo32BitMode(rip64, ctx32, returnEip);
    TEST_ASSERT(exited, "HeavensGate::exitTo32BitMode must succeed from 64-bit mode");
    TEST_ASSERT(ctx32.segCs == wow64::WOW64_CS_32BIT, "Code segment selector must return to 0x23 (32-bit compatibility mode)");
    TEST_ASSERT(ctx32.eip == returnEip, "EIP must be restored to return address");
    TEST_ASSERT(ctx32.ebx == 0x12345678, "General-purpose register state must be preserved across Heaven's Gate");
}

// ============================================================================
// Suite 32: WoW64 System Call Thunking & FS/Registry Redirection
// ============================================================================
void Test_Wow64_SyscallThunkingAndFsRedirection() {
    // 1. Test File System Redirection (Wow64FsRedirection)
    std::wstring p1 = wow64::Wow64FsRedirection::translatePath(L"C:\\Windows\\System32\\notepad.exe");
    TEST_ASSERT(p1 == L"C:\\Windows\\SysWOW64\\notepad.exe", "System32 path must redirect to SysWOW64");

    // Exemption paths: drivers\etc, spool, catroot
    std::wstring pEtc = wow64::Wow64FsRedirection::translatePath(L"C:\\Windows\\System32\\drivers\\etc\\hosts");
    TEST_ASSERT(pEtc == L"C:\\Windows\\System32\\drivers\\etc\\hosts", "drivers\\etc path must be exempted from redirection");

    std::wstring pSpool = wow64::Wow64FsRedirection::translatePath(L"C:\\Windows\\System32\\spool\\printers");
    TEST_ASSERT(pSpool == L"C:\\Windows\\System32\\spool\\printers", "spool path must be exempted from redirection");

    std::wstring pCat = wow64::Wow64FsRedirection::translatePath(L"C:\\Windows\\System32\\catroot\\catalog.cat");
    TEST_ASSERT(pCat == L"C:\\Windows\\System32\\catroot\\catalog.cat", "catroot path must be exempted from redirection");

    // Non-System32 path remains unchanged
    std::wstring pProg = wow64::Wow64FsRedirection::translatePath(L"C:\\Program Files\\App\\app.exe");
    TEST_ASSERT(pProg == L"C:\\Program Files\\App\\app.exe", "Non-System32 paths must not be modified");

    // Thread-local Disable & Revert
    wow64::PVOID32 oldState = 0;
    wow64::Wow64FsRedirection::disable(&oldState);
    TEST_ASSERT(wow64::Wow64FsRedirection::isRedirectionDisabled(), "Redirection must be reported as disabled");
    std::wstring pDisabled = wow64::Wow64FsRedirection::translatePath(L"C:\\Windows\\System32\\notepad.exe");
    TEST_ASSERT(pDisabled == L"C:\\Windows\\System32\\notepad.exe", "Redirection must not occur when disabled");

    wow64::Wow64FsRedirection::revert(oldState);
    TEST_ASSERT(!wow64::Wow64FsRedirection::isRedirectionDisabled(), "Redirection must be active after revert");
    std::wstring pReverted = wow64::Wow64FsRedirection::translatePath(L"C:\\Windows\\System32\\notepad.exe");
    TEST_ASSERT(pReverted == L"C:\\Windows\\SysWOW64\\notepad.exe", "Redirection must resume after revert");

    // 2. Test Registry Redirection (translateRegistryKey)
    std::wstring r1 = wow64::Wow64FsRedirection::translateRegistryKey(L"\\Registry\\Machine\\Software\\Microsoft\\Windows NT\\CurrentVersion");
    TEST_ASSERT(r1 == L"\\Registry\\Machine\\Software\\WOW6432Node\\Microsoft\\Windows NT\\CurrentVersion",
                "HKLM\\Software must redirect to HKLM\\Software\\WOW6432Node");

    std::wstring r2 = wow64::Wow64FsRedirection::translateRegistryKey(L"\\Registry\\Machine\\Software\\WOW6432Node\\TestApp");
    TEST_ASSERT(r2 == L"\\Registry\\Machine\\Software\\WOW6432Node\\TestApp",
                "Already redirected key must not be double redirected");

    // 3. Test 32-to-64 Bit System Call Thunk Engine (Wow64ThunkDispatcher)
    auto& thunk = wow64::Wow64ThunkDispatcher::get();

    // VirtualAlloc Thunk: 32-bit allocation
    wow64::PVOID32 base32 = 0;
    wow64::SIZE_T32 size32 = 64 * 1024;
    NtStatus stAlloc = thunk.thunkNtAllocateVirtualMemory(
        0, &base32, 0, &size32, mm::MEM_COMMIT | mm::MEM_RESERVE, mm::PAGE_READWRITE
    );
    TEST_ASSERT(NT_SUCCESS(stAlloc), "thunkNtAllocateVirtualMemory must succeed");
    TEST_ASSERT(base32 != 0, "Allocated 32-bit base address must be non-zero");
    TEST_ASSERT(base32 <= 0xFFFFFFFFULL, "Allocated address must reside within 32-bit address space");
    TEST_ASSERT(size32 >= 64 * 1024, "Allocated 32-bit size must be at least requested size");

    // VirtualFree Thunk
    NtStatus stFree = thunk.thunkNtFreeVirtualMemory(0, &base32, &size32, mm::MEM_RELEASE);
    TEST_ASSERT(NT_SUCCESS(stFree), "thunkNtFreeVirtualMemory must succeed");

    // File I/O Thunk: Write to standard output handle 0x14 with IoStatusBlock32
    const char wowBanner[] = "MicaNT WoW64 Subsystem Syscall Thunk Test\n";
    wow64::IoStatusBlock32 iosb32{};
    NtStatus stWrite = thunk.thunkNtWriteFile(
        0x14, 0, 0, 0, &iosb32, wowBanner, static_cast<uint32_t>(sizeof(wowBanner) - 1), nullptr, 0
    );
    TEST_ASSERT(NT_SUCCESS(stWrite), "thunkNtWriteFile must succeed");
    TEST_ASSERT(iosb32.information == sizeof(wowBanner) - 1, "IoStatusBlock32 information must match written byte count");

    // Wait Thunk: Create an event and wait with timeout
    Handle evHandle = 0;
    sys::SyscallFrame frameEv{};
    frameEv.ssn = sys::SSN_NtCreateEvent;
    frameEv.arg1 = reinterpret_cast<uint64_t>(&evHandle);
    frameEv.arg3 = 0;
    frameEv.arg4 = 0;
    (void)sys::SyscallDispatcher::get().dispatch(frameEv);
    TEST_ASSERT(evHandle != 0, "CreateEvent for WoW64 wait test must succeed");

    wow64::LargeInteger32 timeout32 = wow64::LargeInteger32::fromInt64(-10000); // 1ms
    NtStatus stWait = thunk.thunkNtWaitForSingleObject(static_cast<wow64::HANDLE32>(evHandle), false, &timeout32);
    TEST_ASSERT(stWait == NtStatus::Timeout, "thunkNtWaitForSingleObject on unsignaled event must return Timeout");

    // Signal event and wait again with 0 timeout
    frameEv = sys::SyscallFrame{};
    frameEv.ssn = sys::SSN_NtSetEvent;
    frameEv.arg1 = static_cast<uint64_t>(evHandle);
    (void)sys::SyscallDispatcher::get().dispatch(frameEv);

    timeout32 = wow64::LargeInteger32::fromInt64(0);
    stWait = thunk.thunkNtWaitForSingleObject(static_cast<wow64::HANDLE32>(evHandle), false, &timeout32);
    TEST_ASSERT(stWait == NtStatus::Success, "thunkNtWaitForSingleObject on signaled event must succeed");

    // Close Handle Thunk
    NtStatus stClose = thunk.thunkNtClose(static_cast<wow64::HANDLE32>(evHandle));
    TEST_ASSERT(NT_SUCCESS(stClose), "thunkNtClose must succeed");
}

void Test_PeLoader_DynamicImportBindingAndUnmodifiedBinary() {
    // 1. Synthesize an authentic, valid 64-bit PE executable binary in memory
    // Layout:
    // Header size: 0x400 (DOS Header, Stub, NT Headers, Section Headers)
    // Section 1 (.text):   RVA 0x1000, Size 0x200, Raw 0x400, Size 0x200
    // Section 2 (.rdata):  RVA 0x2000, Size 0x400, Raw 0x600, Size 0x400 (contains .idata)
    // Section 3 (.reloc):  RVA 0x3000, Size 0x200, Raw 0xA00, Size 0x200 (contains base relocations)
    // Total raw file size: 0xC00 (3072 bytes)

    std::vector<uint8_t> fileBytes(0xC00, 0);

    // DOS Header
    auto* dos = reinterpret_cast<pe::ImageDosHeader*>(fileBytes.data());
    dos->e_magic = pe::IMAGE_DOS_SIGNATURE; // "MZ"
    dos->e_lfanew = 0x80; // Offset to NT Headers

    // NT Headers 64
    auto* nt = reinterpret_cast<pe::ImageNtHeaders64*>(fileBytes.data() + 0x80);
    nt->signature = pe::IMAGE_NT_SIGNATURE; // "PE\0\0"
    nt->fileHeader.machine = pe::IMAGE_FILE_MACHINE_AMD64; // 0x8664
    nt->fileHeader.numberOfSections = 3;
    nt->fileHeader.sizeOfOptionalHeader = sizeof(pe::ImageOptionalHeader64);
    nt->fileHeader.characteristics = pe::IMAGE_FILE_EXECUTABLE_IMAGE | pe::IMAGE_FILE_LARGE_ADDRESS_AWARE;

    nt->optionalHeader.magic = pe::IMAGE_NT_OPTIONAL_HDR64_MAGIC;
    nt->optionalHeader.addressOfEntryPoint = 0x1000;
    nt->optionalHeader.imageBase = 0x0000000140000000ULL;
    nt->optionalHeader.sectionAlignment = 0x1000;
    nt->optionalHeader.fileAlignment = 0x200;
    nt->optionalHeader.sizeOfImage = 0x4000;
    nt->optionalHeader.sizeOfHeaders = 0x400;
    nt->optionalHeader.subsystem = pe::IMAGE_SUBSYSTEM_WINDOWS_CUI;
    nt->optionalHeader.numberOfRvaAndSizes = pe::IMAGE_NUMBEROF_DIRECTORY_ENTRIES;

    // Data Directory entries
    nt->optionalHeader.dataDirectory[pe::IMAGE_DIRECTORY_ENTRY_IMPORT].virtualAddress = 0x2000; // .rdata
    nt->optionalHeader.dataDirectory[pe::IMAGE_DIRECTORY_ENTRY_IMPORT].size = sizeof(pe::ImageImportDescriptor) * 2;

    nt->optionalHeader.dataDirectory[pe::IMAGE_DIRECTORY_ENTRY_BASERELOC].virtualAddress = 0x3000; // .reloc
    nt->optionalHeader.dataDirectory[pe::IMAGE_DIRECTORY_ENTRY_BASERELOC].size = sizeof(pe::ImageBaseRelocation) + 2 * sizeof(uint16_t);

    // Section Headers (immediately following optional header)
    auto* secHeaders = reinterpret_cast<pe::ImageSectionHeader*>(
        fileBytes.data() + 0x80 + sizeof(uint32_t) + sizeof(pe::ImageFileHeader) + sizeof(pe::ImageOptionalHeader64)
    );

    // Section 1: .text
    std::memcpy(secHeaders[0].name, ".text\0\0\0", 8);
    secHeaders[0].misc.virtualSize = 0x200;
    secHeaders[0].virtualAddress = 0x1000;
    secHeaders[0].sizeOfRawData = 0x200;
    secHeaders[0].pointerToRawData = 0x400;
    secHeaders[0].characteristics = pe::IMAGE_SCN_MEM_READ | pe::IMAGE_SCN_MEM_EXECUTE;

    // Section 2: .rdata
    std::memcpy(secHeaders[1].name, ".rdata\0\0", 8);
    secHeaders[1].misc.virtualSize = 0x400;
    secHeaders[1].virtualAddress = 0x2000;
    secHeaders[1].sizeOfRawData = 0x400;
    secHeaders[1].pointerToRawData = 0x600;
    secHeaders[1].characteristics = pe::IMAGE_SCN_MEM_READ;

    // Section 3: .reloc
    std::memcpy(secHeaders[2].name, ".reloc\0\0", 8);
    secHeaders[2].misc.virtualSize = 0x200;
    secHeaders[2].virtualAddress = 0x3000;
    secHeaders[2].sizeOfRawData = 0x200;
    secHeaders[2].pointerToRawData = 0xA00;
    secHeaders[2].characteristics = pe::IMAGE_SCN_MEM_READ | pe::IMAGE_SCN_MEM_DISCARDABLE;

    // Populate .rdata (raw offset 0x600, virtual RVA 0x2000):
    // Layout in .rdata:
    // 0x2000 (raw 0x600): ImageImportDescriptor for kernel32.dll
    //   - originalFirstThunk = 0x2050 (INT)
    //   - name = 0x2030 (points to "kernel32.dll\0")
    //   - firstThunk = 0x2080 (IAT)
    // 0x2014 (raw 0x614): Null terminating ImageImportDescriptor (zeros)
    // 0x2030 (raw 0x630): DLL Name: "kernel32.dll\0"
    // 0x2050 (raw 0x650): Import Lookup Table (INT) - 5 entries:
    //   - INT[0]: 0x2100 (ImageImportByName for "GetTickCount64")
    //   - INT[1]: 0x2120 (ImageImportByName for "ExitProcess")
    //   - INT[2]: 0x2140 (ImageImportByName for "WriteConsoleW")
    //   - INT[3]: pe::IMAGE_ORDINAL_FLAG64 | 42 (Ordinal 42)
    //   - INT[4]: 0 (terminator)
    // 0x2080 (raw 0x680): Initial IAT (same RVAs as INT)
    //   - IAT[0]: 0x2100
    //   - IAT[1]: 0x2120
    //   - IAT[2]: 0x2140
    //   - IAT[3]: pe::IMAGE_ORDINAL_FLAG64 | 42
    //   - IAT[4]: 0
    // 0x2100 (raw 0x700): ImageImportByName: hint=0, name="GetTickCount64\0"
    // 0x2120 (raw 0x720): ImageImportByName: hint=1, name="ExitProcess\0"
    // 0x2140 (raw 0x740): ImageImportByName: hint=2, name="WriteConsoleW\0"

    auto* importDesc = reinterpret_cast<pe::ImageImportDescriptor*>(fileBytes.data() + 0x600);
    importDesc->originalFirstThunk = 0x2050;
    importDesc->name = 0x2030;
    importDesc->firstThunk = 0x2080;

    // DLL name string
    const char dllName[] = "kernel32.dll";
    std::memcpy(fileBytes.data() + 0x630, dllName, sizeof(dllName));

    // INT table
    auto* intTable = reinterpret_cast<uint64_t*>(fileBytes.data() + 0x650);
    intTable[0] = 0x2100;
    intTable[1] = 0x2120;
    intTable[2] = 0x2140;
    intTable[3] = pe::IMAGE_ORDINAL_FLAG64 | 42;
    intTable[4] = 0;

    // IAT table
    auto* iatTable = reinterpret_cast<uint64_t*>(fileBytes.data() + 0x680);
    iatTable[0] = 0x2100;
    iatTable[1] = 0x2120;
    iatTable[2] = 0x2140;
    iatTable[3] = pe::IMAGE_ORDINAL_FLAG64 | 42;
    iatTable[4] = 0;

    // Symbols
    auto* ibn1 = reinterpret_cast<pe::ImageImportByName*>(fileBytes.data() + 0x700);
    ibn1->hint = 0;
    std::memcpy(ibn1->name, "GetTickCount64\0", sizeof("GetTickCount64\0"));

    auto* ibn2 = reinterpret_cast<pe::ImageImportByName*>(fileBytes.data() + 0x720);
    ibn2->hint = 1;
    std::memcpy(ibn2->name, "ExitProcess\0", sizeof("ExitProcess\0"));

    auto* ibn3 = reinterpret_cast<pe::ImageImportByName*>(fileBytes.data() + 0x740);
    ibn3->hint = 2;
    std::memcpy(ibn3->name, "WriteConsoleW\0", sizeof("WriteConsoleW\0"));

    // Populate .reloc (raw offset 0xA00, virtual RVA 0x3000):
    // ImageBaseRelocation pointing to .text (RVA 0x1000) at offset 0x20
    auto* relocBlock = reinterpret_cast<pe::ImageBaseRelocation*>(fileBytes.data() + 0xA00);
    relocBlock->virtualAddress = 0x1000;
    relocBlock->sizeOfBlock = sizeof(pe::ImageBaseRelocation) + 2 * sizeof(uint16_t);

    auto* relocEntries = reinterpret_cast<uint16_t*>(fileBytes.data() + 0xA00 + sizeof(pe::ImageBaseRelocation));
    relocEntries[0] = static_cast<uint16_t>((pe::IMAGE_REL_BASED_DIR64 << 12) | 0x0020);
    relocEntries[1] = 0; // Padding (IMAGE_REL_BASED_ABSOLUTE)

    // In .text at raw offset 0x420 (RVA 0x1020), put a 64-bit absolute address pointing to preferred base:
    *reinterpret_cast<uint64_t*>(fileBytes.data() + 0x420) = 0x0000000140001000ULL;

    // 2. Validate PE Header Inspection via PeLoader
    pe::ImageNtHeaders64 parsedHeaders{};
    std::vector<pe::ImageSectionHeader> parsedSections;
    NtStatus stInspect = pe::PeLoader::inspect(fileBytes, parsedHeaders, parsedSections);
    TEST_ASSERT(NT_SUCCESS(stInspect), "pe::PeLoader::inspect must successfully parse the 64-bit PE binary");
    TEST_ASSERT(parsedSections.size() == 3, "Parsed sections count must equal 3 (.text, .rdata, .reloc)");
    TEST_ASSERT(parsedHeaders.optionalHeader.imageBase == 0x0000000140000000ULL, "Preferred ImageBase must match 0x140000000");

    // 3. Test Import Directory Parsing (INT -> Symbols & IAT RVAs)
    std::vector<pe::ImportedLibrary> imports;
    NtStatus stImports = pe::PeLoader::parseImports(fileBytes, parsedHeaders, parsedSections, imports);
    TEST_ASSERT(NT_SUCCESS(stImports), "pe::PeLoader::parseImports must succeed on valid .idata directory");
    TEST_ASSERT(imports.size() == 1, "Must parse exactly 1 imported DLL (kernel32.dll)");
    TEST_ASSERT(imports[0].libraryName == "kernel32.dll", "Imported library name must match kernel32.dll");
    TEST_ASSERT(imports[0].symbols.size() == 4, "Must parse 4 imported symbols");
    TEST_ASSERT(imports[0].symbols[0].name == "GetTickCount64", "Symbol 0 must be GetTickCount64");
    TEST_ASSERT(imports[0].symbols[0].iatRva == 0x2080, "GetTickCount64 IAT RVA must be 0x2080");
    TEST_ASSERT(imports[0].symbols[1].name == "ExitProcess", "Symbol 1 must be ExitProcess");
    TEST_ASSERT(imports[0].symbols[1].iatRva == 0x2088, "ExitProcess IAT RVA must be 0x2088");
    TEST_ASSERT(imports[0].symbols[2].name == "WriteConsoleW", "Symbol 2 must be WriteConsoleW");
    TEST_ASSERT(imports[0].symbols[2].iatRva == 0x2090, "WriteConsoleW IAT RVA must be 0x2090");
    TEST_ASSERT(imports[0].symbols[3].isOrdinal, "Symbol 3 must be imported by ordinal");
    TEST_ASSERT(imports[0].symbols[3].ordinal == 42, "Symbol 3 ordinal must be 42");
    TEST_ASSERT(imports[0].symbols[3].iatRva == 0x2098, "Symbol 3 IAT RVA must be 0x2098");

    // 4. Initialize Win32 Subsystem Exports
    win32::InitializeWin32SubsystemExports();

    // 5. Simulate Memory-Mapped Executable Image
    // A PE loader maps sections from file raw offsets to virtual memory RVAs
    std::vector<uint8_t> mappedImage(parsedHeaders.optionalHeader.sizeOfImage, 0);
    // Copy headers
    std::memcpy(mappedImage.data(), fileBytes.data(), parsedHeaders.optionalHeader.sizeOfHeaders);
    // Copy sections to virtual addresses
    for (const auto& sec : parsedSections) {
        std::memcpy(mappedImage.data() + sec.virtualAddress, fileBytes.data() + sec.pointerToRawData, sec.sizeOfRawData);
    }

    // 6. Bind Imports into Mapped Image IAT
    size_t boundCount = pe::PeLoader::bindImports(
        mappedImage.data(),
        imports,
        [](std::string_view mod, std::string_view fn, uint16_t ord) -> void* {
            if (ord == 42) {
                return reinterpret_cast<void*>(0xDEADBEEFCAFEBABEULL);
            }
            return ldr::DynamicLoader::get().getExport(mod, fn);
        }
    );
    TEST_ASSERT(boundCount == 4, "PeLoader::bindImports must bind all 4 symbols successfully");

    // Verify IAT entries in mapped memory
    const auto* mappedIat = reinterpret_cast<const uint64_t*>(mappedImage.data() + 0x2080);
    TEST_ASSERT(mappedIat[0] == reinterpret_cast<uint64_t>(win32::GetTickCount64), "IAT[0] must contain GetTickCount64 pointer");
    TEST_ASSERT(mappedIat[1] == reinterpret_cast<uint64_t>(win32::ExitProcess), "IAT[1] must contain ExitProcess pointer");
    TEST_ASSERT(mappedIat[2] == reinterpret_cast<uint64_t>(win32::WriteConsoleW), "IAT[2] must contain WriteConsoleW pointer");
    TEST_ASSERT(mappedIat[3] == 0xDEADBEEFCAFEBABEULL, "IAT[3] must contain ordinal resolved pointer");

    // 7. Invoke Bound Win32 Function Directly Through Mapped IAT
    using FnGetTickCount64 = uint64_t(*)();
    auto fnGetTickCount64 = reinterpret_cast<FnGetTickCount64>(mappedIat[0]);
    uint64_t tick = fnGetTickCount64();
    TEST_ASSERT(tick > 0, "Invoking GetTickCount64 directly through mapped PE IAT slot must return valid timestamp");

    // 8. Apply Base Relocations (Simulate loading at different base address)
    // Original base = 0x0000000140000000, New base = 0x0000000240000000 (delta = +0x0000000100000000)
    uintptr_t rebasedAddress = 0x0000000240000000ULL;
    NtStatus stReloc = pe::PeLoader::applyRelocations(
        mappedImage.data(),
        mappedImage.size(),
        parsedHeaders,
        rebasedAddress
    );
    TEST_ASSERT(NT_SUCCESS(stReloc), "pe::PeLoader::applyRelocations must succeed");

    // The address at RVA 0x1020 in .text was originally 0x0000000140001000; after relocation it must be 0x0000000240001000
    uint64_t relocatedPointer = *reinterpret_cast<const uint64_t*>(mappedImage.data() + 0x1020);
    TEST_ASSERT(relocatedPointer == 0x0000000240001000ULL,
                "Relocated pointer in .text must reflect +0x100000000 rebase delta");
}

void Test_Cpu_GdtTssAndRing3HardwareTransitions() {
    auto& cpuEngine = cpu::CpuHardwareEngine::get();
    uint64_t dummyStackTop = 0x00007FFFFFF00000ULL;
    uint64_t dummySyscallAddr = 0x00007FF800010000ULL;
    cpuEngine.initialize(dummyStackTop, dummySyscallAddr);

    TEST_ASSERT(cpuEngine.isInitialized(), "CpuHardwareEngine must be initialized");

    // 1. Validate Segment Selectors
    TEST_ASSERT(cpu::SELECTOR_KCODE64 == 0x0010, "Kernel Code Selector must be 0x10");
    TEST_ASSERT(cpu::SELECTOR_KDATA64 == 0x0018, "Kernel Data Selector must be 0x18");
    TEST_ASSERT(cpu::SELECTOR_UCODE32 == 0x0023, "Compatibility Mode User Code Selector must be 0x23 (WoW64)");
    TEST_ASSERT(cpu::SELECTOR_UDATA64 == 0x002B, "User Data Selector must be 0x2B (RPL 3)");
    TEST_ASSERT(cpu::SELECTOR_UCODE64 == 0x0033, "User Code Selector must be 0x33 (RPL 3)");
    TEST_ASSERT(cpu::SELECTOR_TSS64 == 0x0040, "TSS64 Selector must be 0x40");

    // 2. Validate GDT Layout & TSS
    const auto& gdt = cpuEngine.getGdt();
    TEST_ASSERT(gdt.kernelCode.access == 0x9A, "Kernel Code access byte must be 0x9A (Ring 0, Exec/Read)");
    TEST_ASSERT(gdt.kernelData.access == 0x92, "Kernel Data access byte must be 0x92 (Ring 0, Read/Write)");
    TEST_ASSERT(gdt.userCmCode.access == 0xFA, "User CM Code access byte must be 0xFA (Ring 3)");
    TEST_ASSERT(gdt.userData.access == 0xF2, "User Data access byte must be 0xF2 (Ring 3, Read/Write)");
    TEST_ASSERT(gdt.userCode.access == 0xFA, "User Code access byte must be 0xFA (Ring 3, Exec/Read)");
    TEST_ASSERT(gdt.tssDesc.access == 0x89, "TSS descriptor access byte must be 0x89 (Available 64-bit TSS)");

    // Validate TSS64 RSP0
    const auto& tss = cpuEngine.getTss();
    TEST_ASSERT(tss.rsp0 == dummyStackTop, "TSS RSP0 must point to kernel interrupt/syscall stack top");
    TEST_ASSERT(tss.ist1 == dummyStackTop, "TSS IST1 must point to interrupt stack table");

    // 3. Validate MSR Configuration for KiSystemCall64
    TEST_ASSERT(cpuEngine.getMsrLstar() == dummySyscallAddr, "MSR_LSTAR must hold KiSystemCall64 entry address");
    TEST_ASSERT((cpuEngine.getMsrSfmask() & 0x200) != 0, "MSR_SFMASK must mask IF (Interrupt Flag)");
    TEST_ASSERT((cpuEngine.getMsrEfer() & cpu::EFER_SCE) != 0, "MSR_EFER must have SCE (Syscall Enable) bit set");

    // MSR_STAR verification: High 32 bits contain target selectors
    uint64_t star = cpuEngine.getMsrStar();
    uint16_t kernelCs = static_cast<uint16_t>((star >> 32) & 0xFFFF);
    uint16_t userCs = static_cast<uint16_t>((star >> 48) & 0xFFFF);
    TEST_ASSERT(kernelCs == cpu::KGDT64_R0_CODE, "STAR MSR must specify Kernel Code 0x10 at bits 47:32");
    TEST_ASSERT(userCs == cpu::KGDT64_R3_CMCODE, "STAR MSR must specify User Base 0x20 at bits 63:48");

    // 4. Validate Ring 3 Entry Frame Construction (iretq)
    uint64_t entryPoint = 0x0000000140001000ULL;
    uint64_t userRsp = 0x00007FFFFFE00000ULL;
    cpu::IretFrame64 frame = cpuEngine.createRing3EntryFrame(entryPoint, userRsp);

    TEST_ASSERT(frame.rip == entryPoint, "IretFrame RIP must match entry point");
    TEST_ASSERT(frame.cs == cpu::SELECTOR_UCODE64, "IretFrame CS must be User 64-bit Code (0x33)");
    TEST_ASSERT(frame.rsp == userRsp, "IretFrame RSP must match user stack");
    TEST_ASSERT(frame.ss == cpu::SELECTOR_UDATA64, "IretFrame SS must be User Data (0x2B)");
    TEST_ASSERT((frame.rflags & 0x200) != 0, "IretFrame RFLAGS must have Interrupt Flag (IF) enabled");
    TEST_ASSERT((frame.rflags & 0x3000) == 0, "IretFrame RFLAGS IOPL must be 0 for Ring 3");
}

namespace host_mem {
    extern "C" void* __stdcall VirtualAlloc(void* lpAddress, size_t dwSize, uint32_t flAllocationType, uint32_t flProtect);
    extern "C" int   __stdcall VirtualFree(void* lpAddress, size_t dwSize, uint32_t dwFreeType);
}

namespace host_thread {
    extern "C" void*    __stdcall CreateThread(void* lpThreadAttributes, size_t dwStackSize, uint32_t (__stdcall *lpStartAddress)(void*), void* lpParameter, uint32_t dwCreationFlags, uint32_t* lpThreadId);
    extern "C" uint32_t __stdcall WaitForSingleObject(void* hHandle, uint32_t dwMilliseconds);
    extern "C" int      __stdcall CloseHandle(void* hObject);
    extern "C" void     __stdcall ExitThread(uint32_t dwExitCode);
}

static uint32_t s_UnmodifiedExitCode = 0xFFFFFFFF;
static bool s_UnmodifiedExitCalled = false;

[[noreturn]] static void MockUnmodifiedExitProcess(uint32_t code) {
    s_UnmodifiedExitCode = code;
    s_UnmodifiedExitCalled = true;
    host_thread::ExitThread(code);
    for (;;) {}
}


void Test_Execution_UnmodifiedThirdPartyBinary() {
    // 1. Read the real, compiled, unmodified Windows binary from disk or embedded clean fixture
    const char* candidatePaths[] = {
        "bin/unmodified_sample.exe",
        "../bin/unmodified_sample.exe",
        "unmodified_sample.exe",
        "test/fixtures/unmodified_sample.exe"
    };

    std::vector<uint8_t> fileBytes;
    for (const char* path : candidatePaths) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (file.is_open()) {
            std::streamsize fileSize = file.tellg();
            if (fileSize > 0) {
                file.seekg(0, std::ios::beg);
                fileBytes.resize(static_cast<size_t>(fileSize));
                file.read(reinterpret_cast<char*>(fileBytes.data()), fileSize);
                file.close();
                break;
            }
        }
    }

    if (fileBytes.empty()) {
        fileBytes.assign(std::begin(kUnmodifiedSampleBinary), std::end(kUnmodifiedSampleBinary));
    }
    TEST_ASSERT(!fileBytes.empty(), "Unmodified binary bytes must be loaded");

    // 2. Parse PE Headers & Sections via PeLoader
    pe::ImageNtHeaders64 headers{};
    std::vector<pe::ImageSectionHeader> sections;
    NtStatus stInspect = pe::PeLoader::inspect(fileBytes, headers, sections);
    TEST_ASSERT(NT_SUCCESS(stInspect), "PeLoader::inspect must parse unmodified binary headers");
    TEST_ASSERT(headers.fileHeader.machine == pe::MACHINE_AMD64, "Target machine must be AMD64 (x86_64)");
    TEST_ASSERT(sections.size() >= 3, "Binary must contain at least 3 sections (.text, .rdata, .pdata)");
    TEST_ASSERT(headers.optionalHeader.addressOfEntryPoint == 0x1000, "Entry point RVA must be 0x1000");

    // 3. Parse Dynamic Import Directory
    std::vector<pe::ImportedLibrary> imports;
    NtStatus stImports = pe::PeLoader::parseImports(fileBytes, headers, sections, imports);
    TEST_ASSERT(NT_SUCCESS(stImports), "PeLoader::parseImports must parse unmodified import table");
    TEST_ASSERT(imports.size() == 1, "Must have exactly 1 imported library (KERNEL32.dll)");
    TEST_ASSERT(imports[0].libraryName == "KERNEL32.dll", "Imported DLL must be KERNEL32.dll");
    TEST_ASSERT(imports[0].symbols.size() == 4, "Must have 4 imported symbols from KERNEL32.dll");

    bool hasWriteFile = false, hasExitProcess = false, hasGetTickCount64 = false, hasGetStdHandle = false;
    for (const auto& sym : imports[0].symbols) {
        if (sym.name == "WriteFile") hasWriteFile = true;
        if (sym.name == "ExitProcess") hasExitProcess = true;
        if (sym.name == "GetTickCount64") hasGetTickCount64 = true;
        if (sym.name == "GetStdHandle") hasGetStdHandle = true;
    }
    TEST_ASSERT(hasWriteFile && hasExitProcess && hasGetTickCount64 && hasGetStdHandle,
                "All 4 symbols (WriteFile, ExitProcess, GetTickCount64, GetStdHandle) must be parsed");

    // 4. Initialize Clean-Room Win32 Subsystem Exports
    win32::InitializeWin32SubsystemExports();

    // Intercept ExitProcess during test execution so it records return without killing test runner
    s_UnmodifiedExitCalled = false;
    s_UnmodifiedExitCode = 0xFFFFFFFF;
    ldr::DynamicLoader::get().registerExport("kernel32.dll", "ExitProcess", reinterpret_cast<void*>(MockUnmodifiedExitProcess));

    // 5. Map Image into Executable Memory
    size_t imageSize = headers.optionalHeader.sizeOfImage;
    uint8_t* mappedBase = reinterpret_cast<uint8_t*>(
        host_mem::VirtualAlloc(nullptr, imageSize, 0x1000 /* MEM_COMMIT */ | 0x2000 /* MEM_RESERVE */, 0x40 /* PAGE_EXECUTE_READWRITE */)
    );
    TEST_ASSERT(mappedBase != nullptr, "VirtualAlloc must allocate executable image memory");

    NtStatus stMap = pe::PeLoader::mapImage(fileBytes, headers, sections, mappedBase, imageSize);
    TEST_ASSERT(NT_SUCCESS(stMap), "PeLoader::mapImage must copy headers and sections to virtual addresses");

    // 6. Bind Imports directly into the mapped image's IAT
    size_t boundCount = pe::PeLoader::bindImports(
        mappedBase,
        imports,
        [](std::string_view mod, std::string_view fn, uint16_t /*ord*/) -> void* {
            return ldr::DynamicLoader::get().getExport(mod, fn);
        }
    );
    TEST_ASSERT(boundCount == 4, "PeLoader::bindImports must bind all 4 symbols in unmodified binary");

    // 7. Verify Bound IAT entries
    for (const auto& sym : imports[0].symbols) {
        uint64_t boundAddr = *reinterpret_cast<const uint64_t*>(mappedBase + sym.iatRva);
        std::cout << "  [IAT] " << sym.name << " => 0x" << std::hex << boundAddr << std::dec << "\n" << std::flush;
        TEST_ASSERT(boundAddr != 0, "Bound IAT slot must not be null");
    }

    // 8. Execute the unmodified binary's entry point in an isolated execution thread!
    using EntryFunc = void(*)();
    auto fnEntry = reinterpret_cast<EntryFunc>(mappedBase + headers.optionalHeader.addressOfEntryPoint);

    std::cout << "\n[Test Runner] Launching unmodified third-party 64-bit PE entry point at 0x" 
              << reinterpret_cast<void*>(fnEntry) << "...\n";

    auto threadProc = [](void* param) -> unsigned long {
        auto entry = reinterpret_cast<EntryFunc>(param);
        entry();
        return 0;
    };
    void* hThread = host_thread::CreateThread(
        nullptr, 0,
        reinterpret_cast<uint32_t(__stdcall*)(void*)>(+threadProc),
        reinterpret_cast<void*>(fnEntry),
        0, nullptr
    );
    if (hThread) {
        host_thread::WaitForSingleObject(hThread, 0xFFFFFFFF);
        host_thread::CloseHandle(hThread);
    }

    std::cout << "[Test Runner] Unmodified third-party PE invoked ExitProcess(" << s_UnmodifiedExitCode << ") cleanly!\n";

    // 9. Verify that the unmodified binary executed all calls successfully
    TEST_ASSERT(s_UnmodifiedExitCalled, "Unmodified binary must invoke ExitProcess through bound IAT");
    TEST_ASSERT(s_UnmodifiedExitCode == 0, "Unmodified binary must exit with code 0 (success)");

    // Restore real ExitProcess in DynamicLoader & free image
    ldr::DynamicLoader::get().registerExport("kernel32.dll", "ExitProcess", reinterpret_cast<void*>(win32::ExitProcess));
    host_mem::VirtualFree(mappedBase, 0, 0x8000 /* MEM_RELEASE */);
}

void Test_ExpandedWin32AndNtSystemCalls() {
    using namespace micant::win32;

    // 1. Initialize and verify expanded Win32 Subsystem Exports
    win32::InitializeWin32SubsystemExports();
    auto& ldr = ldr::DynamicLoader::get();

    TEST_ASSERT(ldr.getExport("kernel32.dll", "GetLastError") != nullptr, "GetLastError must be exported");
    TEST_ASSERT(ldr.getExport("kernel32.dll", "CreateFileMappingW") != nullptr, "CreateFileMappingW must be exported");
    TEST_ASSERT(ldr.getExport("kernel32.dll", "FindFirstFileW") != nullptr, "FindFirstFileW must be exported");
    TEST_ASSERT(ldr.getExport("kernel32.dll", "QueryPerformanceCounter") != nullptr, "QueryPerformanceCounter must be exported");
    TEST_ASSERT(ldr.getExport("ntdll.dll", "NtCreateSection") != nullptr, "NtCreateSection must be exported from ntdll");
    TEST_ASSERT(ldr.getExport("ntdll.dll", "NtQueryInformationFile") != nullptr, "NtQueryInformationFile must be exported from ntdll");
    TEST_ASSERT(ldr.getExport("ntdll.dll", "NtQueryDirectoryFile") != nullptr, "NtQueryDirectoryFile must be exported from ntdll");
    TEST_ASSERT(ldr.getExport("ntdll.dll", "RtlNtStatusToDosError") != nullptr, "RtlNtStatusToDosError must be exported from ntdll");

    // 2. Error Handling & TEB last error synchronization
    win32::SetLastError(12345);
    TEST_ASSERT(win32::GetLastError() == 12345, "GetLastError must return code set by SetLastError");
    TEST_ASSERT(ntdll::RtlNtStatusToDosError(NtStatus::NoSuchFile) == 2, "RtlNtStatusToDosError must return ERROR_FILE_NOT_FOUND (2)");
    TEST_ASSERT(ntdll::RtlNtStatusToDosError(NtStatus::AccessDenied) == 5, "RtlNtStatusToDosError must return ERROR_ACCESS_DENIED (5)");
    TEST_ASSERT(ntdll::RtlNtStatusToDosError(NtStatus::NoMemory) == 14, "RtlNtStatusToDosError must return ERROR_OUTOFMEMORY (14)");

    // 3. Environment & Current Directory Management
    wchar_t envBuf[256]{};
    DWORD len = win32::GetEnvironmentVariableW(L"OS", envBuf, 256);
    TEST_ASSERT(len > 0 && std::wstring(envBuf) == L"MicaNT", "Environment variable OS must be MicaNT");

    win32::SetEnvironmentVariableW(L"MICANT_TEST_VAR", L"Active");
    len = win32::GetEnvironmentVariableW(L"MICANT_TEST_VAR", envBuf, 256);
    TEST_ASSERT(len > 0 && std::wstring(envBuf) == L"Active", "SetEnvironmentVariableW must set custom variable");

    wchar_t dirBuf[256]{};
    win32::GetCurrentDirectoryW(256, dirBuf);
    TEST_ASSERT(std::wstring(dirBuf) == L"C:\\Windows\\System32", "Initial working directory must be C:\\Windows\\System32");

    win32::SetCurrentDirectoryW(L"C:\\Users\\Default");
    win32::GetCurrentDirectoryW(256, dirBuf);
    TEST_ASSERT(std::wstring(dirBuf) == L"C:\\Users\\Default", "SetCurrentDirectoryW must change directory");
    win32::SetCurrentDirectoryW(L"C:\\Windows\\System32"); // restore

    wchar_t fullPath[256]{};
    wchar_t* filePart = nullptr;
    win32::GetFullPathNameW(L"cmd.exe", 256, fullPath, &filePart);
    TEST_ASSERT(std::wstring(fullPath) == L"C:\\Windows\\System32\\cmd.exe", "GetFullPathNameW must resolve relative file");

    // 4. Module & Image Introspection
    HMODULE hKernel32 = win32::GetModuleHandleW(L"KERNEL32.DLL");
    TEST_ASSERT(hKernel32 != nullptr, "GetModuleHandleW for kernel32 must return valid HMODULE");
    win32::GetModuleFileNameW(nullptr, dirBuf, 256);
    TEST_ASSERT(std::wstring(dirBuf).find(L"micant.exe") != std::wstring::npos, "GetModuleFileNameW for main module must return micant.exe");

    // 5. Directory Operations & FindFirstFile / FindNextFile
    win32::CreateDirectoryW(L"C:\\ApiTestDir", nullptr);
    DWORD attrs = win32::GetFileAttributesW(L"C:\\ApiTestDir");
    TEST_ASSERT((attrs & win32::FILE_ATTRIBUTE_DIRECTORY) != 0, "ApiTestDir must have FILE_ATTRIBUTE_DIRECTORY attribute");

    win32::WIN32_FIND_DATAW findData{};
    win32::HANDLE hFind = win32::FindFirstFileW(L"C:\\Windows\\System32\\*", &findData);
    TEST_ASSERT(hFind != win32::INVALID_HANDLE_VALUE, "FindFirstFileW must succeed on C:\\Windows\\System32\\*");
    std::vector<std::wstring> foundNames;
    foundNames.push_back(findData.cFileName);
    while (win32::FindNextFileW(hFind, &findData)) {
        foundNames.push_back(findData.cFileName);
    }
    win32::FindClose(hFind);
    TEST_ASSERT(foundNames.size() >= 2, "Must find at least 2 entries in System32 (e.g. ntdll.dll, kernel32.dll)");

    // 6. File Operations, Sizing & Seeking
    win32::HANDLE hFile = win32::CreateFileW(L"C:\\api_seek_test.txt", win32::GENERIC_READ | win32::GENERIC_WRITE, 0, nullptr, win32::CREATE_ALWAYS, 0, nullptr);
    TEST_ASSERT(hFile != nullptr, "CreateFileW must create C:\\api_seek_test.txt");

    const char testText[] = "MicaNT Clean-Room Kernel System Calls Test Buffer";
    DWORD written = 0;
    win32::WriteFile(hFile, testText, sizeof(testText), &written, nullptr);
    TEST_ASSERT(written == sizeof(testText), "WriteFile must write all bytes");

    LargeInteger fileSize{};
    win32::GetFileSizeEx(hFile, &fileSize);
    TEST_ASSERT(fileSize.quadPart == sizeof(testText), "GetFileSizeEx must match written size");

    LargeInteger newPos{}, move{};
    move.quadPart = 7;
    win32::SetFilePointerEx(hFile, move, &newPos, win32::FILE_BEGIN);
    TEST_ASSERT(newPos.quadPart == 7, "SetFilePointerEx must move offset to 7");

    char readBuf[16]{};
    DWORD readBytes = 0;
    win32::ReadFile(hFile, readBuf, 10, &readBytes, nullptr);
    TEST_ASSERT(readBytes == 10, "ReadFile must read 10 bytes from offset 7");
    win32::CloseHandle(hFile);
    win32::DeleteFileW(L"C:\\api_seek_test.txt");
    win32::RemoveDirectoryW(L"C:\\ApiTestDir");

    // 7. Memory Mapping & Section Objects
    win32::HANDLE hMap = win32::CreateFileMappingW(win32::INVALID_HANDLE_VALUE, nullptr, win32::PAGE_READWRITE, 0, 8192, L"TestSection");
    TEST_ASSERT(hMap != nullptr, "CreateFileMappingW must create 8KB section");
    win32::LPVOID pView = win32::MapViewOfFile(hMap, win32::FILE_MAP_ALL_ACCESS, 0, 0, 8192);
    TEST_ASSERT(pView != nullptr, "MapViewOfFile must map section view");
    std::memset(pView, 0x5A, 4096);
    TEST_ASSERT(*reinterpret_cast<uint8_t*>(pView) == 0x5A, "Mapped view memory must be writable and readable");
    win32::UnmapViewOfFile(pView);
    win32::CloseHandle(hMap);

    // 8. High-Precision Performance Counter
    LargeInteger qpc1{}, qpc2{}, freq{};
    win32::QueryPerformanceFrequency(&freq);
    TEST_ASSERT(freq.quadPart == 1000000000LL, "Performance frequency must be 1 GHz (1,000,000,000 Hz)");
    win32::QueryPerformanceCounter(&qpc1);
    win32::Sleep(1);
    win32::QueryPerformanceCounter(&qpc2);
    TEST_ASSERT(qpc2.quadPart >= qpc1.quadPart, "QPC counter must monotonically advance");

    // 9. Console Controls & Screen Buffer
    conhost::ConsoleScreenBufferInfo csbi{};
    win32::GetConsoleScreenBufferInfo(win32::GetStdHandle(win32::STD_OUTPUT_HANDLE), &csbi);
    TEST_ASSERT(csbi.dwSize.x == 80 && csbi.dwSize.y == 25, "Default console buffer size must be 80x25");
    DWORD consoleMode = 0;
    win32::GetConsoleMode(win32::GetStdHandle(win32::STD_OUTPUT_HANDLE), &consoleMode);
    TEST_ASSERT((consoleMode & win32::ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0, "VT100 processing must be enabled in console mode");

    // 10. Process Query & Thread Yield
    DWORD exitCode = 0xFFFFFFFF;
    win32::GetExitCodeProcess(win32::GetCurrentProcess(), &exitCode);
    TEST_ASSERT(exitCode == 0, "GetExitCodeProcess must return 0 for running process");
    TEST_ASSERT(win32::SwitchToThread() == win32::TRUE, "SwitchToThread must return TRUE");
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
    RUN_TEST(Test_SynchronizationPrimitives);
    RUN_TEST(Test_IoAndCompletionPorts);
    RUN_TEST(Test_ConfigurationManager_HiveAndValues);
    RUN_TEST(Test_SecurityReferenceMonitor_AccessCheck);
    RUN_TEST(Test_Alpc_MessageRendezvous);
    RUN_TEST(Test_KernelCore_IrqlSpinLockAndScheduler);
    RUN_TEST(Test_ExecutivePools_AllocationAndIrql);
    RUN_TEST(Test_TrapEngine_PageFaultAndBugCheck);
    RUN_TEST(Test_HardwareAbstractionLayer_KPCRAndTimers);
    RUN_TEST(Test_VirtualFileSystem_Fat32AndFileObjects);
    RUN_TEST(Test_ExecutiveWorkQueues_Dispatch);
    RUN_TEST(Test_BootContract_LoaderParameterBlock);
    RUN_TEST(Test_KernelStressAndConcurrencyHardening);
    RUN_TEST(Test_DriverModel_DriverEntryAndDeviceIoControl);
    RUN_TEST(Test_KernelTimers_DpcAndDelayExecution);
    RUN_TEST(Test_WaitMultipleObjects_MultiHandleSync);
    RUN_TEST(Test_LookasideLists_FastAllocAndTelemetry);
    RUN_TEST(Test_PowerManagement_IrpAndShutdown);
    RUN_TEST(Test_Ntdll_SyscallStubsAndPebTeb);
    RUN_TEST(Test_UserlandHeap_RtlAllocateAndCoalescing);
    RUN_TEST(Test_UefiBootloader_GopAndMemoryMap);
    RUN_TEST(Test_BootVid_FramebufferAndSplashRenderer);
    RUN_TEST(Test_Csrss_ProcessRegistrationAndAlpc);
    RUN_TEST(Test_Conhost_ScreenBufferAndFramebufferBlit);
    RUN_TEST(Test_Kernel32_Win32ApiParity);
    RUN_TEST(Test_Wow64_PebTebAndHeavensGate);
    RUN_TEST(Test_Wow64_SyscallThunkingAndFsRedirection);
    RUN_TEST(Test_PeLoader_DynamicImportBindingAndUnmodifiedBinary);
    RUN_TEST(Test_Cpu_GdtTssAndRing3HardwareTransitions);
    RUN_TEST(Test_Execution_UnmodifiedThirdPartyBinary);
    RUN_TEST(Test_ExpandedWin32AndNtSystemCalls);


    std::cout << "\n------------------------------------------------------------------------\n";
    std::cout << "Summary: " << g_PassedTests << " Passed, " << g_FailedTests << " Failed\n";
    std::cout << "------------------------------------------------------------------------\n";

    return (g_FailedTests == 0) ? 0 : 1;
}

