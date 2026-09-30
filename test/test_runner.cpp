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

    std::cout << "\n------------------------------------------------------------------------\n";
    std::cout << "Summary: " << g_PassedTests << " Passed, " << g_FailedTests << " Failed\n";
    std::cout << "------------------------------------------------------------------------\n";

    return (g_FailedTests == 0) ? 0 : 1;
}

