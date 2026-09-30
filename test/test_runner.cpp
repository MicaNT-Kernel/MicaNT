#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <span>
#include <memory>
#include <thread>
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

    std::cout << "\n------------------------------------------------------------------------\n";
    std::cout << "Summary: " << g_PassedTests << " Passed, " << g_FailedTests << " Failed\n";
    std::cout << "------------------------------------------------------------------------\n";

    return (g_FailedTests == 0) ? 0 : 1;
}

