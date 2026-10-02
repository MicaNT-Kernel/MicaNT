#include <cstdint>
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
#include "micant/msvcrt.hpp"
#include "micant/advapi32.hpp"
#include "micant/user32.hpp"
#include "micant/ws2_32.hpp"
#include "micant/shell.hpp"
#include "micant/storage.hpp"
#include "micant/fat32.hpp"
#include "micant/ndis.hpp"
#include "micant/tcpip.hpp"
#include "micant/iphlpapi.hpp"
#include "micant/arm64.hpp"
#include "micant/npfs.hpp"
#include "micant/scm.hpp"
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

void Test_MsvcrtBridge_And_CommandShell() {
    using namespace micant;

    // 1. Initialize Subsystems and verify dynamic export registration
    win32::InitializeWin32SubsystemExports();
    msvcrt::InitializeMsvcrtSubsystemExports();
    advapi32::InitializeAdvapi32SubsystemExports();
    user32::InitializeUser32SubsystemExports();
    ws2_32::InitializeWs2_32SubsystemExports();

    auto& ldr = ldr::DynamicLoader::get();

    // Verify MSVCRT exports
    TEST_ASSERT(ldr.getExport("msvcrt.dll", "printf") != nullptr, "msvcrt.dll!printf must be exported");
    TEST_ASSERT(ldr.getExport("msvcrt.dll", "malloc") != nullptr, "msvcrt.dll!malloc must be exported");
    TEST_ASSERT(ldr.getExport("msvcrt.dll", "free") != nullptr, "msvcrt.dll!free must be exported");
    TEST_ASSERT(ldr.getExport("msvcrt.dll", "calloc") != nullptr, "msvcrt.dll!calloc must be exported");
    TEST_ASSERT(ldr.getExport("msvcrt.dll", "realloc") != nullptr, "msvcrt.dll!realloc must be exported");
    TEST_ASSERT(ldr.getExport("msvcrt.dll", "strlen") != nullptr, "msvcrt.dll!strlen must be exported");
    TEST_ASSERT(ldr.getExport("msvcrt.dll", "strcmp") != nullptr, "msvcrt.dll!strcmp must be exported");
    TEST_ASSERT(ldr.getExport("msvcrt.dll", "strcpy") != nullptr, "msvcrt.dll!strcpy must be exported");
    TEST_ASSERT(ldr.getExport("msvcrt.dll", "sprintf") != nullptr, "msvcrt.dll!sprintf must be exported");
    TEST_ASSERT(ldr.getExport("msvcrt.dll", "__getmainargs") != nullptr, "msvcrt.dll!__getmainargs must be exported");
    TEST_ASSERT(ldr.getExport("msvcrt.dll", "_initterm") != nullptr, "msvcrt.dll!_initterm must be exported");

    // Verify ADVAPI32 exports
    TEST_ASSERT(ldr.getExport("advapi32.dll", "CryptAcquireContextA") != nullptr, "advapi32.dll!CryptAcquireContextA must be exported");
    TEST_ASSERT(ldr.getExport("advapi32.dll", "CryptGenRandom") != nullptr, "advapi32.dll!CryptGenRandom must be exported");
    TEST_ASSERT(ldr.getExport("advapi32.dll", "CryptReleaseContext") != nullptr, "advapi32.dll!CryptReleaseContext must be exported");

    // Verify USER32 exports
    TEST_ASSERT(ldr.getExport("user32.dll", "ShowWindow") != nullptr, "user32.dll!ShowWindow must be exported");
    TEST_ASSERT(ldr.getExport("user32.dll", "IsWindowVisible") != nullptr, "user32.dll!IsWindowVisible must be exported");
    TEST_ASSERT(ldr.getExport("user32.dll", "PeekMessageA") != nullptr, "user32.dll!PeekMessageA must be exported");

    // Verify WS2_32 exports
    TEST_ASSERT(ldr.getExport("ws2_32.dll", "WSAStartup") != nullptr, "ws2_32.dll!WSAStartup must be exported");
    TEST_ASSERT(ldr.getExport("ws2_32.dll", "WSACleanup") != nullptr, "ws2_32.dll!WSACleanup must be exported");
    TEST_ASSERT(ldr.getExport("ws2_32.dll", "inet_addr") != nullptr, "ws2_32.dll!inet_addr must be exported");

    // 2. Functional Test: Clean-Room CRT memory & string manipulation
    void* mem = msvcrt::malloc(64);
    TEST_ASSERT(mem != nullptr, "msvcrt::malloc must allocate valid memory");
    std::memset(mem, 0x5A, 64);
    void* reallocMem = msvcrt::realloc(mem, 128);
    TEST_ASSERT(reallocMem != nullptr, "msvcrt::realloc must reallocate memory block");
    msvcrt::free(reallocMem);

    void* zeroMem = msvcrt::calloc(4, 16);
    TEST_ASSERT(zeroMem != nullptr, "msvcrt::calloc must allocate zeroed memory");
    uint8_t zeroBuf[64]{};
    TEST_ASSERT(std::memcmp(zeroMem, zeroBuf, 64) == 0, "msvcrt::calloc buffer must be zeroed");
    msvcrt::free(zeroMem);

    char testStr[64]{};
    msvcrt::strcpy(testStr, "MicaNT Clean-Room CRT");
    TEST_ASSERT(msvcrt::strlen(testStr) == 21, "msvcrt::strlen must report correct length");
    TEST_ASSERT(msvcrt::strcmp(testStr, "MicaNT Clean-Room CRT") == 0, "msvcrt::strcmp must match exact string");

    char formatted[128]{};
    int numChars = msvcrt::sprintf(formatted, "Status: %s (code %d)", "Active", 200);
    TEST_ASSERT(numChars > 0, "msvcrt::sprintf must format string successfully");
    TEST_ASSERT(std::string(formatted) == "Status: Active (code 200)", "msvcrt::sprintf must format correctly");

    // 3. Functional Test: Crypto, UI & Network Stubs
    uintptr_t hCrypt = 0;
    TEST_ASSERT(advapi32::CryptAcquireContextA(&hCrypt, nullptr, nullptr, 0, 0) == win32::TRUE, "CryptAcquireContextA must return TRUE");
    uint8_t randBytes[16]{};
    TEST_ASSERT(advapi32::CryptGenRandom(hCrypt, 16, randBytes) == win32::TRUE, "CryptGenRandom must generate random bytes");
    bool hasNonZero = false;
    for (int i = 0; i < 16; ++i) {
        if (randBytes[i] != 0) hasNonZero = true;
    }
    TEST_ASSERT(hasNonZero, "CryptGenRandom must generate non-zero random entropy");
    TEST_ASSERT(advapi32::CryptReleaseContext(hCrypt, 0) == win32::TRUE, "CryptReleaseContext must return TRUE");

    ws2_32::WSADATA wsaData{};
    TEST_ASSERT(ws2_32::WSAStartup(0x0202, &wsaData) == 0, "WSAStartup must succeed with 0");
    TEST_ASSERT(ws2_32::inet_addr("127.0.0.1") == 0x0100007F, "inet_addr must resolve 127.0.0.1 to 0x0100007F");
    TEST_ASSERT(ws2_32::WSACleanup() == 0, "WSACleanup must succeed");

    // 4. MicaNT Interactive Command Prompt Shell Built-in Commands
    shell::CommandShell cmdShell;
    std::ostringstream oss;

    // Test ver
    oss.str("");
    int rc = cmdShell.execute("ver", oss);
    TEST_ASSERT(rc == 0, "Shell ver command must succeed");
    TEST_ASSERT(oss.str().find("MicaNT") != std::string::npos, "Shell ver must output MicaNT");

    // Test echo with variable expansion
    oss.str("");
    rc = cmdShell.execute("echo Operating System: %OS%", oss);
    TEST_ASSERT(rc == 0, "Shell echo command must succeed");
    TEST_ASSERT(oss.str().find("Operating System: MicaNT") != std::string::npos, "Shell must expand %OS% to MicaNT");

    // Test set command
    oss.str("");
    rc = cmdShell.execute("set SHELL_TEST=Passed", oss);
    TEST_ASSERT(rc == 0, "Shell set command must succeed");
    oss.str("");
    rc = cmdShell.execute("echo Result: %SHELL_TEST%", oss);
    TEST_ASSERT(oss.str().find("Result: Passed") != std::string::npos, "Shell must expand user-defined variable %SHELL_TEST%");

    // Test time / mem / systeminfo
    oss.str("");
    rc = cmdShell.execute("time", oss);
    TEST_ASSERT(rc == 0, "Shell time command must succeed");
    TEST_ASSERT(oss.str().find("System Time") != std::string::npos, "Shell time output must show System Time");

    oss.str("");
    rc = cmdShell.execute("mem", oss);
    TEST_ASSERT(rc == 0, "Shell mem command must succeed");
    TEST_ASSERT(oss.str().find("Kernel NonPaged Pool") != std::string::npos, "Shell mem output must show pool statistics");

    oss.str("");
    rc = cmdShell.execute("systeminfo", oss);
    TEST_ASSERT(rc == 0, "Shell systeminfo command must succeed");
    TEST_ASSERT(oss.str().find("x86_64") != std::string::npos, "Shell systeminfo must show x86_64 architecture");

    // Test dir
    oss.str("");
    rc = cmdShell.execute("dir", oss);
    TEST_ASSERT(rc == 0, "Shell dir command must succeed");
    TEST_ASSERT(oss.str().find("Directory of") != std::string::npos, "Shell dir must output directory header");

    // 5. Unmodified Third-Party PE Execution via Shell Engine
    oss.str("");
    std::cout << "\n[Shell Test] Executing unmodified CRT binary 'bin/unmodified_crt_sample.exe' via CommandShell...\n";
    int execRc = cmdShell.execute("exec bin/unmodified_crt_sample.exe", oss);
    std::string execOutput = oss.str();
    std::cout << execOutput << std::flush;

    TEST_ASSERT(execRc == 0, "Shell execution of unmodified_crt_sample.exe must return exit code 0");
    TEST_ASSERT(execOutput.find("Process finished with exit code 0") != std::string::npos,
                "Process must finish with exit code 0");
}

// ============================================================================
// Suite 38: Storage Stack & FAT32 Filesystem Engine Tests
// ============================================================================
void Test_StorageAndFat32FileSystem() {
    // 1. In-Memory RamDisk Block Device
    constexpr uint32_t SECTOR_SIZE = 512;
    constexpr uint64_t TOTAL_DISK_BYTES = 64 * 1024 * 1024; // 64 MB
    auto ramDisk = std::make_shared<storage::RamDiskDevice>(
        L"\\Device\\Harddisk0\\Partition0",
        TOTAL_DISK_BYTES,
        SECTOR_SIZE
    );
    TEST_ASSERT(ramDisk->getBlockSize() == SECTOR_SIZE, "RAM disk sector size must be 512");
    TEST_ASSERT(ramDisk->getTotalBlocks() == (TOTAL_DISK_BYTES / SECTOR_SIZE), "RAM disk total blocks must match 131072");
    TEST_ASSERT(ramDisk->getTotalBytes() == TOTAL_DISK_BYTES, "RAM disk total bytes must match 64 MB");

    // Test raw sector write and read
    std::vector<uint8_t> testSec(SECTOR_SIZE, 0xA5);
    NtStatus st = ramDisk->writeBlocks(100, 1, testSec.data());
    TEST_ASSERT(NT_SUCCESS(st), "RamDisk writeBlocks to sector 100 must succeed");

    std::vector<uint8_t> readSec(SECTOR_SIZE, 0);
    st = ramDisk->readBlocks(100, 1, readSec.data());
    TEST_ASSERT(NT_SUCCESS(st), "RamDisk readBlocks from sector 100 must succeed");
    TEST_ASSERT(std::memcmp(testSec.data(), readSec.data(), SECTOR_SIZE) == 0, "RamDisk sector 100 data must match exactly");

    // 2. MBR Partition Table Creation & Parsing
    std::vector<storage::MbrPartitionEntry> writePartitions(1);
    writePartitions[0].bootIndicator = 0x80; // Bootable
    writePartitions[0].partitionType = storage::MBR_TYPE_FAT32_LBA; // 0x0C
    writePartitions[0].startLba = 2048; // 1 MB boundary alignment
    writePartitions[0].sectorCount = static_cast<uint32_t>(ramDisk->getTotalBlocks() - 2048); // 129024 sectors (~63 MB)

    st = storage::PartitionManager::writeMbr(*ramDisk, writePartitions);
    TEST_ASSERT(NT_SUCCESS(st), "PartitionManager::writeMbr must succeed");

    std::vector<storage::MbrPartitionEntry> readPartitions;
    st = storage::PartitionManager::parseMbr(*ramDisk, readPartitions);
    TEST_ASSERT(NT_SUCCESS(st), "PartitionManager::parseMbr must succeed");
    TEST_ASSERT(readPartitions.size() == 1, "MBR must contain 1 active partition");
    TEST_ASSERT(readPartitions[0].bootIndicator == 0x80, "MBR partition must have boot indicator 0x80");
    TEST_ASSERT(readPartitions[0].partitionType == storage::MBR_TYPE_FAT32_LBA, "Partition type must be FAT32 LBA");
    TEST_ASSERT(readPartitions[0].startLba == 2048, "Partition start LBA must be 2048");
    TEST_ASSERT(readPartitions[0].sectorCount == (ramDisk->getTotalBlocks() - 2048), "Partition sector count must match");

    // 3. Partition Device Slicing
    auto partition1 = std::make_shared<storage::PartitionDevice>(
        L"\\Device\\Harddisk0\\Partition1",
        ramDisk,
        readPartitions[0].startLba,
        readPartitions[0].sectorCount
    );
    TEST_ASSERT(partition1->getBlockSize() == SECTOR_SIZE, "Partition device block size must be 512");
    TEST_ASSERT(partition1->getTotalBlocks() == readPartitions[0].sectorCount, "Partition device total blocks must match");
    TEST_ASSERT(partition1->getStartLba() == 2048, "Partition device start LBA must be 2048");

    // 4. FAT32 Filesystem Formatting
    // Format partition1 with 8 sectors per cluster (4 KB clusters)
    st = fat32::Fat32FileSystem::format(*partition1, "MICANT_SYS", 8);
    TEST_ASSERT(NT_SUCCESS(st), "Fat32FileSystem::format must succeed");

    // 5. FAT32 Mounting & BPB Geometry
    fat32::Fat32FileSystem fs;
    st = fs.mount(partition1);
    TEST_ASSERT(NT_SUCCESS(st), "Fat32FileSystem::mount must succeed");
    TEST_ASSERT(fs.isMounted(), "Filesystem must report mounted status");
    TEST_ASSERT(fs.getBytesPerCluster() == 4096, "Cluster size must be 4096 bytes (4 KB)");
    TEST_ASSERT(fs.getRootCluster() == 2, "FAT32 root cluster must be cluster 2");
    TEST_ASSERT(fs.getTotalClusters() > 15000, "Volume must have > 15000 total clusters");

    // 6. Directory Tree Creation
    st = fs.createDirectory(L"Windows");
    TEST_ASSERT(NT_SUCCESS(st), "createDirectory 'Windows' must succeed");

    st = fs.createDirectory(L"Windows\\System32");
    TEST_ASSERT(NT_SUCCESS(st), "createDirectory 'Windows\\System32' must succeed");

    st = fs.createDirectory(L"Windows\\System32\\drivers");
    TEST_ASSERT(NT_SUCCESS(st), "createDirectory 'Windows\\System32\\drivers' must succeed");

    st = fs.createDirectory(L"Users");
    TEST_ASSERT(NT_SUCCESS(st), "createDirectory 'Users' must succeed");

    st = fs.createDirectory(L"Users\\Administrator");
    TEST_ASSERT(NT_SUCCESS(st), "createDirectory 'Users\\Administrator' must succeed");

    // 7. Short Filename (8.3) Creation & Verification
    std::string ntdllStub = "MZ-MICANT-CLEANROOM-NTDLL-CORE-PAYLOAD";
    std::vector<uint8_t> ntdllBytes(ntdllStub.begin(), ntdllStub.end());
    st = fs.createFile(L"Windows\\System32\\ntdll.dll", ntdllBytes);
    TEST_ASSERT(NT_SUCCESS(st), "createFile 'Windows\\System32\\ntdll.dll' must succeed");

    std::vector<uint8_t> readNtdll;
    st = fs.readFile(L"Windows\\System32\\ntdll.dll", 0, ntdllBytes.size(), readNtdll);
    TEST_ASSERT(NT_SUCCESS(st), "readFile 'Windows\\System32\\ntdll.dll' must succeed");
    TEST_ASSERT(readNtdll == ntdllBytes, "Read ntdll content must match written content");

    // 8. Long File Name (LFN) Unicode Reconstruction
    std::wstring lfnName = L"MicaNT Executive Advanced Architecture Specification Document.json";
    std::wstring lfnFullPath = L"Windows\\System32\\" + lfnName;
    std::string lfnPayload = "{\"Architecture\":\"Dave Cutler Clean-Room\",\"Subsystems\":[\"ob\",\"mm\",\"ps\",\"fat32\"]}";
    std::vector<uint8_t> lfnBytes(lfnPayload.begin(), lfnPayload.end());

    st = fs.createFile(lfnFullPath, lfnBytes);
    TEST_ASSERT(NT_SUCCESS(st), "createFile with Long File Name (LFN) must succeed");

    std::vector<uint8_t> readLfn;
    st = fs.readFile(lfnFullPath, 0, lfnBytes.size(), readLfn);
    TEST_ASSERT(NT_SUCCESS(st), "readFile via Long File Name must succeed");
    TEST_ASSERT(readLfn == lfnBytes, "Read LFN file content must match written content");

    // 9. Multi-Cluster File Integrity Across Cluster Chain Boundaries
    // Cluster size is 4096 bytes. We write 14,336 bytes (spanning 3.5 clusters = 4 clusters allocated)
    constexpr size_t MULTI_CLUSTER_SIZE = 14336;
    std::vector<uint8_t> multiData(MULTI_CLUSTER_SIZE);
    for (size_t i = 0; i < MULTI_CLUSTER_SIZE; ++i) {
        multiData[i] = static_cast<uint8_t>((i * 13 + 0x47) & 0xFF);
    }

    std::wstring driverPath = L"Windows\\System32\\drivers\\fastfat_test.sys";
    st = fs.createFile(driverPath, multiData);
    TEST_ASSERT(NT_SUCCESS(st), "createFile multi-cluster driver must succeed");

    // Read full file
    std::vector<uint8_t> readMultiFull;
    st = fs.readFile(driverPath, 0, MULTI_CLUSTER_SIZE, readMultiFull);
    TEST_ASSERT(NT_SUCCESS(st), "readFile full multi-cluster must succeed");
    TEST_ASSERT(readMultiFull.size() == MULTI_CLUSTER_SIZE, "Multi-cluster read length must match");
    TEST_ASSERT(std::memcmp(readMultiFull.data(), multiData.data(), MULTI_CLUSTER_SIZE) == 0,
                "Multi-cluster payload must match 100% across all cluster boundaries");

    // Boundary read: 128 bytes spanning cluster boundary at 4096 (offset 4032 to 4160)
    std::vector<uint8_t> boundaryRead1;
    st = fs.readFile(driverPath, 4032, 128, boundaryRead1);
    TEST_ASSERT(NT_SUCCESS(st), "readFile across cluster 0->1 boundary must succeed");
    TEST_ASSERT(boundaryRead1.size() == 128, "Boundary 1 read size must be 128");
    TEST_ASSERT(std::memcmp(boundaryRead1.data(), multiData.data() + 4032, 128) == 0,
                "Boundary 1 data across cluster 0->1 boundary must match exactly");

    // Boundary read: 256 bytes spanning cluster boundary at 8192 (offset 8100 to 8356)
    std::vector<uint8_t> boundaryRead2;
    st = fs.readFile(driverPath, 8100, 256, boundaryRead2);
    TEST_ASSERT(NT_SUCCESS(st), "readFile across cluster 1->2 boundary must succeed");
    TEST_ASSERT(boundaryRead2.size() == 256, "Boundary 2 read size must be 256");
    TEST_ASSERT(std::memcmp(boundaryRead2.data(), multiData.data() + 8100, 256) == 0,
                "Boundary 2 data across cluster 1->2 boundary must match exactly");

    // 10. Directory Enumeration & Path Traversal Verification
    fat32::FatFileInfo sys32Info{};
    st = fs.findPath(L"Windows\\System32", sys32Info);
    TEST_ASSERT(NT_SUCCESS(st), "findPath 'Windows\\System32' must succeed");
    TEST_ASSERT(sys32Info.isDirectory, "'Windows\\System32' must be a directory");

    std::vector<fat32::FatFileInfo> sys32Entries;
    st = fs.readDirectory(sys32Info.firstCluster, sys32Entries);
    TEST_ASSERT(NT_SUCCESS(st), "readDirectory for 'Windows\\System32' must succeed");
    TEST_ASSERT(sys32Entries.size() >= 3, "Directory must contain at least 3 entries");

    bool foundNtdll = false;
    bool foundLfnDoc = false;
    bool foundDriversDir = false;
    for (const auto& entry : sys32Entries) {
        if (entry.name == L"ntdll.dll") foundNtdll = true;
        if (entry.name == lfnName) foundLfnDoc = true;
        if (entry.name == L"drivers" && entry.isDirectory) foundDriversDir = true;
    }
    TEST_ASSERT(foundNtdll, "Directory enumeration must find 'ntdll.dll'");
    TEST_ASSERT(foundLfnDoc, "Directory enumeration must reconstruct full LFN name");
    TEST_ASSERT(foundDriversDir, "Directory enumeration must find 'drivers' subdirectory");

    // 11. VirtualFileSystem Integration Test
    // Mount the partition block device into VirtualFileSystem
    st = fs::VirtualFileSystem::get().mountBlockDevice(partition1);
    TEST_ASSERT(NT_SUCCESS(st), "VirtualFileSystem::mountBlockDevice must succeed");
    TEST_ASSERT(fs::VirtualFileSystem::get().getMountedFat32() != nullptr, "VFS mounted FAT32 pointer must be valid");
    TEST_ASSERT(fs::VirtualFileSystem::get().getMountedBlockDevice() == partition1, "VFS mounted block device must match partition1");

    // 12. GPT Partition Parsing Guardrail Check
    auto blankDisk = std::make_shared<storage::RamDiskDevice>(
        L"\\Device\\Harddisk1\\Partition0",
        16 * 1024 * 1024,
        SECTOR_SIZE
    );
    std::vector<storage::GptPartitionEntry> gptEntries;
    st = storage::PartitionManager::parseGpt(*blankDisk, gptEntries);
    TEST_ASSERT(st == NtStatus::UnrecognizedVolume, "Blank disk must return UnrecognizedVolume for GPT");
}

// ============================================================================
// Suite 39: NDIS 6.x, TCP/IP, Next-Gen QUIC, and Winsock Subsystem Tests
// ============================================================================
void Test_NdisAndTcpIpNetworkStack() {
    // 1. NDIS 6.x Virtual Ethernet Adapter Verification
    auto vNic = std::make_shared<ndis::VirtualNetworkAdapter>(
        L"\\Device\\NdisTestNic",
        L"MicaNT Test Ethernet",
        ndis::MacAddress(0x02, 0x00, 0x4D, 0x49, 0x43, 0x41), // "MICA"
        1500,
        10'000'000'000ULL
    );
    TEST_ASSERT(vNic->getMacAddress().toString() == "02-00-4D-49-43-41", "MAC address string format must match");
    TEST_ASSERT(vNic->getMtu() == 1500, "MTU must be 1500");
    TEST_ASSERT(vNic->getSpeedBps() == 10'000'000'000ULL, "Speed must be 10 Gbps");
    TEST_ASSERT(vNic->getLinkState() == ndis::MediaConnectState::Connected, "Link state must be Connected");

    // Frame transmission
    std::string testPayload = "MicaNT NDIS 6.x Frame Transmission Test Payload";
    auto frame = ndis::buildEthernetFrame(
        ndis::MacAddress::broadcast(),
        vNic->getMacAddress(),
        ndis::ETHERTYPE_IPV4,
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(testPayload.data()), testPayload.size())
    );
    TEST_ASSERT(frame.size() == ndis::ETH_HLEN + testPayload.size(), "Frame size must match header + payload");

    const auto* ethHdr = reinterpret_cast<const ndis::EthernetHeader*>(frame.data());
    TEST_ASSERT(ethHdr->getEtherType() == ndis::ETHERTYPE_IPV4, "EtherType must be IPv4 (0x0800)");
    TEST_ASSERT(ethHdr->destMac.isBroadcast(), "Destination MAC must be broadcast");

    NtStatus st = vNic->sendPacket(frame);
    TEST_ASSERT(NT_SUCCESS(st), "vNic->sendPacket must succeed");
    auto stats = vNic->getStatistics();
    TEST_ASSERT(stats.txPackets == 1, "Tx packet count must be 1");
    TEST_ASSERT(stats.txBytes == frame.size(), "Tx bytes must match frame length");

    // 2. TCP/IP Stack & ARP Resolution
    auto& net = tcpip::NetworkStack::get();
    net.initialize(vNic);

    TEST_ASSERT(net.getLocalIp().toString() == "192.168.1.100", "Local IP must be 192.168.1.100");
    TEST_ASSERT(net.getSubnetMask().toString() == "255.255.255.0", "Subnet mask must be 255.255.255.0");
    TEST_ASSERT(net.getGateway().toString() == "192.168.1.1", "Gateway IP must be 192.168.1.1");
    TEST_ASSERT(net.getDnsServer().toString() == "8.8.8.8", "DNS server must be 8.8.8.8");

    ndis::MacAddress resolvedMac;
    bool resolved = net.resolveArp(tcpip::Ipv4Address::loopback(), resolvedMac);
    TEST_ASSERT(resolved, "Loopback ARP resolution must succeed");
    TEST_ASSERT(resolvedMac == vNic->getMacAddress(), "Loopback MAC must match adapter MAC");

    resolved = net.resolveArp(net.getGateway(), resolvedMac);
    TEST_ASSERT(resolved, "Gateway ARP resolution must succeed from cache");

    // 3. IPv4 Internet Checksum Algorithm (RFC 1071)
    uint8_t sampleIpHeader[] = {
        0x45, 0x00, 0x00, 0x3c, 0x1c, 0x46, 0x40, 0x00,
        0x40, 0x06, 0x00, 0x00, // zero checksum for calculation
        0xc0, 0xa8, 0x01, 0x64, // 192.168.1.100
        0xc0, 0xa8, 0x01, 0x01  // 192.168.1.1
    };
    uint16_t calcCsum = tcpip::calculateInternetChecksum(sampleIpHeader, sizeof(sampleIpHeader));
    TEST_ASSERT(calcCsum != 0, "Checksum must be calculated");
    *reinterpret_cast<uint16_t*>(sampleIpHeader + 10) = calcCsum;
    uint16_t verifyCsum = tcpip::calculateInternetChecksum(sampleIpHeader, sizeof(sampleIpHeader));
    TEST_ASSERT(verifyCsum == 0, "Checksum verification must yield 0 (100% valid)");

    // 4. IPv6 Addressing & Loopback (RFC 8200)
    auto v6Loopback = tcpip::Ipv6Address::loopback();
    TEST_ASSERT(v6Loopback.isLoopback(), "IPv6 address must report isLoopback");
    TEST_ASSERT(v6Loopback.toString() == "::1", "IPv6 loopback string must be '::1'");

    auto v6LinkLocal = tcpip::Ipv6Address::linkLocalMica();
    TEST_ASSERT(!v6LinkLocal.isZero(), "IPv6 link local must not be zero");
    TEST_ASSERT(v6LinkLocal.toString().find("fe80:") == 0, "IPv6 link local must begin with fe80:");

    // 5. ICMPv4 Echo Request & Reply (Ping Engine)
    auto pingResult = net.ping(tcpip::Ipv4Address::loopback());
    TEST_ASSERT(pingResult.success, "Ping 127.0.0.1 must succeed");
    TEST_ASSERT(pingResult.bytesReceived == 32, "Ping bytes received must be 32");
    TEST_ASSERT(pingResult.ttl == 64, "Ping TTL must be 64");
    TEST_ASSERT(pingResult.rttMs <= 10, "Ping round trip time must be <= 10ms for in-memory stack");

    // 6. UDP Transport Layer Datagram Transmission
    int udpServer = net.createSocket(tcpip::AF_INET, tcpip::SOCK_DGRAM, tcpip::IPPROTO_UDP);
    TEST_ASSERT(udpServer > 0, "createSocket UDP server must succeed");
    bool bound = net.bindSocket(udpServer, tcpip::Ipv4Address::loopback(), 9876);
    TEST_ASSERT(bound, "bindSocket UDP server to port 9876 must succeed");

    int udpClient = net.createSocket(tcpip::AF_INET, tcpip::SOCK_DGRAM, tcpip::IPPROTO_UDP);
    TEST_ASSERT(udpClient > 0, "createSocket UDP client must succeed");

    std::string udpMsg = "MicaNT-UDP-FastPath-Datagram";
    int sentUdp = net.sendToSocket(udpClient, udpMsg.data(), udpMsg.size(), tcpip::Ipv4Address::loopback(), 9876);
    TEST_ASSERT(sentUdp == static_cast<int>(udpMsg.size()), "sendToSocket must transmit all bytes");

    char udpRecvBuf[128]{};
    int recvdUdp = net.recvSocket(udpServer, udpRecvBuf, sizeof(udpRecvBuf));
    TEST_ASSERT(recvdUdp == static_cast<int>(udpMsg.size()), "recvSocket must receive all UDP bytes");
    TEST_ASSERT(std::string(udpRecvBuf, recvdUdp) == udpMsg, "Received UDP payload must match transmitted string");

    net.closeSocket(udpServer);
    net.closeSocket(udpClient);

    // 7. TCP Connection 3-Way Handshake & Bidirectional Stream
    int tcpServer = net.createSocket(tcpip::AF_INET, tcpip::SOCK_STREAM, tcpip::IPPROTO_TCP);
    TEST_ASSERT(tcpServer > 0, "createSocket TCP server must succeed");
    bound = net.bindSocket(tcpServer, tcpip::Ipv4Address::loopback(), 8080);
    TEST_ASSERT(bound, "bindSocket TCP server port 8080 must succeed");
    bool listening = net.listenSocket(tcpServer, 5);
    TEST_ASSERT(listening, "listenSocket TCP server must succeed");

    int tcpClient = net.createSocket(tcpip::AF_INET, tcpip::SOCK_STREAM, tcpip::IPPROTO_TCP);
    TEST_ASSERT(tcpClient > 0, "createSocket TCP client must succeed");
    bool connected = net.connectSocket(tcpClient, tcpip::Ipv4Address::loopback(), 8080);
    TEST_ASSERT(connected, "connectSocket TCP client to 127.0.0.1:8080 must succeed");

    tcpip::Ipv4Address acceptedClientIp;
    uint16_t acceptedClientPort = 0;
    int acceptedSock = net.acceptSocket(tcpServer, acceptedClientIp, acceptedClientPort);
    TEST_ASSERT(acceptedSock > 0, "acceptSocket on TCP server must return valid connected socket");

    // Client -> Server Stream Transfer (HTTP Request)
    std::string httpRequest = "GET /index.html HTTP/1.1\r\nHost: 127.0.0.1:8080\r\n\r\n";
    int sentTcp = net.sendSocket(tcpClient, httpRequest.data(), httpRequest.size());
    TEST_ASSERT(sentTcp == static_cast<int>(httpRequest.size()), "Client sendSocket must transmit all bytes");

    char serverRecvBuf[256]{};
    int recvdTcp = net.recvSocket(acceptedSock, serverRecvBuf, sizeof(serverRecvBuf));
    TEST_ASSERT(recvdTcp == static_cast<int>(httpRequest.size()), "Server recvSocket must receive all bytes");
    TEST_ASSERT(std::string(serverRecvBuf, recvdTcp) == httpRequest, "Server received payload must match HTTP request");

    // Server -> Client Stream Transfer (HTTP Response)
    std::string httpResponse = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 17\r\n\r\nHello from MicaNT";
    sentTcp = net.sendSocket(acceptedSock, httpResponse.data(), httpResponse.size());
    TEST_ASSERT(sentTcp == static_cast<int>(httpResponse.size()), "Server sendSocket must transmit all bytes");

    char clientRecvBuf[256]{};
    recvdTcp = net.recvSocket(tcpClient, clientRecvBuf, sizeof(clientRecvBuf));
    TEST_ASSERT(recvdTcp == static_cast<int>(httpResponse.size()), "Client recvSocket must receive all bytes");
    TEST_ASSERT(std::string(clientRecvBuf, recvdTcp) == httpResponse, "Client received payload must match HTTP response");

    net.closeSocket(tcpClient);
    net.closeSocket(acceptedSock);
    net.closeSocket(tcpServer);

    // 8. Next-Gen QUIC Protocol Header Framing (RFC 9000)
    // Long Header: Initial packet (Packet Type 0x00, Version 1)
    tcpip::QuicLongHeader qlh{};
    qlh.flags = 0xC0; // Long Header (0x80) | Fixed Bit (0x40) | Initial (0x00)
    qlh.version = tcpip::htonl(0x00000001); // QUIC v1
    qlh.dcil = 8;
    qlh.scil = 8;

    TEST_ASSERT(qlh.isLongHeader(), "QUIC packet must be recognized as Long Header");
    TEST_ASSERT(qlh.getPacketType() == 0, "QUIC packet type must be 0 (Initial)");
    TEST_ASSERT(qlh.getVersion() == 1, "QUIC version must be 1 (RFC 9000)");

    // Short Header: 1-RTT packet (Spin bit 1, Key Phase 0, Packet Number Length 2)
    tcpip::QuicShortHeader qsh{};
    qsh.flags = 0x61; // Short Header (0x00) | Fixed Bit (0x40) | Spin Bit (0x20) | PN Len (0x01 = 2 bytes)
    TEST_ASSERT(qsh.isShortHeader(), "QUIC packet must be recognized as Short Header");
    TEST_ASSERT(qsh.getSpinBit(), "QUIC spin bit must be true");
    TEST_ASSERT(qsh.getPacketNumberLength() == 2, "QUIC packet number length must be 2 bytes");

    // 9. Winsock 2 (ws2_32.dll) High-Level API Verification
    ws2_32::WSADATA wsaData{};
    int wsaRc = ws2_32::WSAStartup(0x0202, &wsaData);
    TEST_ASSERT(wsaRc == 0, "WSAStartup must succeed");
    TEST_ASSERT(wsaData.wVersion == 0x0202, "Winsock version must be 2.2");

    ws2_32::SOCKET wsaSock = ws2_32::socket(ws2_32::AF_INET, ws2_32::SOCK_STREAM, ws2_32::IPPROTO_TCP);
    TEST_ASSERT(wsaSock != ws2_32::INVALID_SOCKET, "Winsock socket() must return valid descriptor");

    ws2_32::sockaddr_in saddr{};
    saddr.sin_family = ws2_32::AF_INET;
    saddr.sin_port = ws2_32::htons(12345);
    saddr.sin_addr.S_un.S_addr = ws2_32::inet_addr("127.0.0.1");
    int bindRc = ws2_32::bind(wsaSock, reinterpret_cast<const ws2_32::sockaddr*>(&saddr), sizeof(saddr));
    TEST_ASSERT(bindRc == 0, "Winsock bind() must succeed");

    int closeRc = ws2_32::closesocket(wsaSock);
    TEST_ASSERT(closeRc == 0, "Winsock closesocket() must succeed");

    char hostBuf[64]{};
    int hostRc = ws2_32::gethostname(hostBuf, sizeof(hostBuf));
    TEST_ASSERT(hostRc == 0, "Winsock gethostname() must succeed");
    TEST_ASSERT(std::string(hostBuf) == "MicaNT-Workstation", "Hostname must be MicaNT-Workstation");

    // 10. IP Helper API (iphlpapi.dll) Verification
    iphlpapi::IP_ADAPTER_INFO adaptInfo{};
    uint32_t adaptBufLen = sizeof(adaptInfo);
    uint32_t iphlpRc = iphlpapi::GetAdaptersInfo(&adaptInfo, &adaptBufLen);
    TEST_ASSERT(iphlpRc == iphlpapi::ERROR_SUCCESS, "GetAdaptersInfo must return ERROR_SUCCESS");
    TEST_ASSERT(std::string(adaptInfo.ipAddressList.ipAddress.str) == "192.168.1.100", "Adapter IP must be 192.168.1.100");
    TEST_ASSERT(std::string(adaptInfo.gatewayList.ipAddress.str) == "192.168.1.1", "Gateway must be 192.168.1.1");
    TEST_ASSERT(adaptInfo.address[0] == 0x02 && adaptInfo.address[1] == 0x00, "Adapter MAC prefix must be 02-00");

    iphlpapi::FIXED_INFO fixedInfo{};
    uint32_t fixedBufLen = sizeof(fixedInfo);
    iphlpRc = iphlpapi::GetNetworkParams(&fixedInfo, &fixedBufLen);
    TEST_ASSERT(iphlpRc == iphlpapi::ERROR_SUCCESS, "GetNetworkParams must return ERROR_SUCCESS");
    TEST_ASSERT(std::string(fixedInfo.hostName) == "MicaNT-Workstation", "HostName must be MicaNT-Workstation");
    TEST_ASSERT(std::string(fixedInfo.dnsServerList.ipAddress.str) == "8.8.8.8", "DNS Server must be 8.8.8.8");

    // 11. Shell Built-in Network Commands (ipconfig, ping, netstat)
    shell::CommandShell cmdShell;
    std::ostringstream oss;

    // ipconfig
    oss.str("");
    int shRc = cmdShell.execute("ipconfig", oss);
    TEST_ASSERT(shRc == 0, "Shell ipconfig command must succeed");
    TEST_ASSERT(oss.str().find("192.168.1.100") != std::string::npos, "ipconfig output must include 192.168.1.100");
    TEST_ASSERT(oss.str().find("255.255.255.0") != std::string::npos, "ipconfig output must include 255.255.255.0");

    // ipconfig /all
    oss.str("");
    shRc = cmdShell.execute("ipconfig /all", oss);
    TEST_ASSERT(shRc == 0, "Shell ipconfig /all command must succeed");
    TEST_ASSERT(oss.str().find("Physical Address") != std::string::npos, "ipconfig /all must output MAC address");
    TEST_ASSERT(oss.str().find("DNS Servers") != std::string::npos, "ipconfig /all must output DNS servers");

    // ping
    oss.str("");
    shRc = cmdShell.execute("ping 127.0.0.1", oss);
    TEST_ASSERT(shRc == 0, "Shell ping 127.0.0.1 command must succeed");
    TEST_ASSERT(oss.str().find("Reply from 127.0.0.1") != std::string::npos, "ping output must show Echo Reply");
    TEST_ASSERT(oss.str().find("Packets: Sent = 4, Received = 4") != std::string::npos, "ping output must show 4 packets received (0% loss)");

    // netstat
    oss.str("");
    shRc = cmdShell.execute("netstat", oss);
    TEST_ASSERT(shRc == 0, "Shell netstat command must succeed");
    TEST_ASSERT(oss.str().find("Active Connections") != std::string::npos, "netstat output must show Active Connections table");
}

// ============================================================================
// Suite 40: AArch64 (ARM64) Hardware Architecture & Fast Syscall Engine
// ============================================================================
void Test_Arm64HardwareArchitectureAndSyscall() {
    // 1. General-Purpose Register File & Condition Flags (PSTATE)
    arm64::Arm64Context ctx{};
    ctx.x0 = 0x1111222233334444ULL;
    ctx.x8 = sys::SSN_NtAllocateVirtualMemory; // SSN in X8
    ctx.fp = 0x00007FFFFFFFE000ULL;           // X29
    ctx.lr = 0x0000000140001050ULL;           // X30
    ctx.sp_el0 = 0x00007FFFFFFFDFF0ULL;       // User stack
    ctx.sp_el1 = 0xFFFF800000100000ULL;       // Kernel stack
    ctx.pc = 0x0000000140001000ULL;
    ctx.pstate = arm64::pstate::MODE_EL0t;

    TEST_ASSERT(ctx.x0 == 0x1111222233334444ULL, "ARM64 X0 register must match initialized value");
    TEST_ASSERT(ctx.x[8] == sys::SSN_NtAllocateVirtualMemory, "ARM64 X8 register array access must match");
    TEST_ASSERT(ctx.fp == ctx.x[29], "ARM64 FP must alias X29");
    TEST_ASSERT(ctx.lr == ctx.x[30], "ARM64 LR must alias X30");
    TEST_ASSERT(ctx.getCurrentEl() == arm64::ExceptionLevel::EL0, "Initial mode must be EL0 (Userland)");

    ctx.pstate = arm64::pstate::MODE_EL1h;
    TEST_ASSERT(ctx.getCurrentEl() == arm64::ExceptionLevel::EL1, "Mode EL1h must report EL1 (Kernel Executive)");

    ctx.setConditionFlags(true, true, false, false);
    TEST_ASSERT(ctx.isNegativeFlag(), "PSTATE N flag must be set");
    TEST_ASSERT(ctx.isZeroFlag(), "PSTATE Z flag must be set");

    // 2. 128-bit SIMD / NEON Vector Register File (Q0 - Q31)
    ctx.v[0].d[0] = 0xAAAAAAAAAAAAAAAAULL;
    ctx.v[0].d[1] = 0xBBBBBBBBBBBBBBBBULL;
    TEST_ASSERT(!ctx.v[0].isZero(), "Vector register Q0 must not be zero");
    TEST_ASSERT(ctx.v[0].low64 == 0xAAAAAAAAAAAAAAAAULL, "Vector register Q0 low 64-bit must match D0");
    ctx.fpcr = 0x03C00000; // Default FPCR
    TEST_ASSERT(ctx.fpcr == 0x03C00000, "FPCR register must match value");

    // 3. Exception Syndrome Register (ESR_EL1) & Fault Handling
    arm64::Arm64ExceptionSyndrome svcSyndrome{};
    // Construct ESR for SVC in AArch64 (EC = 0x15, IL = 1, imm16 = 1)
    svcSyndrome.rawEsr = (arm64::esr::EC_SVC64 << 26) | (1U << 25) | 1U;
    TEST_ASSERT(svcSyndrome.isSupervisorCall(), "Syndrome must identify as Supervisor Call (SVC)");
    TEST_ASSERT(svcSyndrome.getExceptionClass() == arm64::esr::EC_SVC64, "Exception class must be EC_SVC64 (0x15)");
    TEST_ASSERT(svcSyndrome.getSvcImmediate() == 1, "SVC immediate must be #1 for NT system calls");

    arm64::Arm64ExceptionSyndrome dabtSyndrome{};
    // Construct ESR for Data Abort from lower EL (EC = 0x24, IL = 1, WnR = 1 (bit 6), DFSC = 0x07 (L3 translation fault))
    dabtSyndrome.rawEsr = (arm64::esr::EC_DABT_LOW << 26) | (1U << 25) | (1U << 6) | arm64::esr::DFSC_TRANS_L3;
    dabtSyndrome.faultAddress = 0x00007FFDF0001000ULL;
    TEST_ASSERT(dabtSyndrome.isDataAbort(), "Syndrome must identify as Data Abort");
    TEST_ASSERT(dabtSyndrome.isWriteNotRead(), "WnR bit must report Write operation");
    TEST_ASSERT(dabtSyndrome.getDataFaultStatusCode() == arm64::esr::DFSC_TRANS_L3, "DFSC must match L3 translation fault");
    TEST_ASSERT(dabtSyndrome.faultAddress == 0x00007FFDF0001000ULL, "FAR_EL1 fault address must match");

    // 4. AArch64 VMSA 48-bit 4-Level Translation Tables (MMU)
    arm64::Arm64Mmu mmu;
    TEST_ASSERT(mmu.getTtbr0() == 0x10000000ULL, "Default TTBR0_EL1 must be initialized");
    TEST_ASSERT(mmu.getTtbr1() == 0x20000000ULL, "Default TTBR1_EL1 must be initialized");
    TEST_ASSERT((mmu.getMair() & 0xFF) == 0xFF, "MAIR_EL1 Attr0 must be Normal WBWA (0xFF)");

    // Test 4-level index extraction
    uint64_t sampleVa = 0x0000'1234'5678'9ABCULL;
    auto indices = arm64::Arm64Mmu::extractIndices(sampleVa);
    TEST_ASSERT(indices.offset == 0xABC, "Offset bits [11:0] must match 0xABC");

    // Map a 4KB page and translate
    uint64_t testVa = 0x0000'0000'4000'0000ULL; // 1 GB boundary
    uint64_t testPa = 0x0000'0001'8000'0000ULL; // Physical RAM
    mmu.mapPage(testVa, testPa, arm64::mmu::AP_RW_ALL, arm64::mmu::ATTR_IDX_NORMAL_WBWA, false, false);

    uint64_t resolvedPa = 0;
    arm64::Arm64Pte resolvedPte{};
    NtStatus trStatus = mmu.translateVirtualAddress(testVa + 0x120, resolvedPa, resolvedPte);
    TEST_ASSERT(NT_SUCCESS(trStatus), "Virtual address translation must succeed for mapped page");
    TEST_ASSERT(resolvedPa == (testPa + 0x120), "Resolved physical address must match mapped base + offset");
    TEST_ASSERT(resolvedPte.isValid(), "Resolved PTE must be valid");

    // Unmapped page must return AccessViolation
    trStatus = mmu.translateVirtualAddress(0x0000'0000'5000'0000ULL, resolvedPa, resolvedPte);
    TEST_ASSERT(trStatus == NtStatus::AccessViolation, "Unmapped virtual address must return STATUS_ACCESS_VIOLATION");

    // 5. Fast System Call Dispatcher via SVC #1 (Windows on ARM64 ABI)
    // Dispatch NtAllocateVirtualMemory using ARM64 register frame
    uintptr_t allocBase = 0;
    size_t allocSize = 64 * 1024; // 64 KB

    arm64::Arm64Context svcCtx{};
    svcCtx.x8 = sys::SSN_NtAllocateVirtualMemory; // SSN in X8
    svcCtx.x0 = 0; // CurrentProcess
    svcCtx.x1 = reinterpret_cast<uint64_t>(&allocBase);
    svcCtx.x2 = 0;
    svcCtx.x3 = reinterpret_cast<uint64_t>(&allocSize);
    svcCtx.x4 = mm::MEM_COMMIT | mm::MEM_RESERVE;
    svcCtx.x5 = mm::PAGE_READWRITE;
    svcCtx.pc = 0x0000000140002000ULL;

    NtStatus svcResult = arm64::Arm64SyscallBridge::get().dispatchSvc(svcCtx, svcSyndrome);
    TEST_ASSERT(NT_SUCCESS(svcResult), "ARM64 SVC #1 dispatch for NtAllocateVirtualMemory must succeed");
    TEST_ASSERT(svcCtx.x0 == static_cast<uint64_t>(NtStatus::Success), "ARM64 X0 return register must hold STATUS_SUCCESS");
    TEST_ASSERT(allocBase != 0, "Allocated base address must be non-zero");
    TEST_ASSERT(svcCtx.pc == 0x0000000140002004ULL, "ARM64 PC must advance by 4 bytes past SVC instruction");

    // Free the allocated memory
    sys::NtFreeVirtualMemory(0, &allocBase, &allocSize, mm::MEM_RELEASE);

    // 6. Thread Pointer Register Binding (TPIDR_EL0 for TEB, TPIDR_EL1 for KPCR)
    ctx.tpidr_el0 = 0x00007FFDF0000000ULL; // TEB
    ctx.tpidr_el1 = 0xFFFF800000000000ULL; // KPCR
    TEST_ASSERT(ctx.tpidr_el0 == 0x00007FFDF0000000ULL, "TPIDR_EL0 must hold TEB address");
    TEST_ASSERT(ctx.tpidr_el1 == 0xFFFF800000000000ULL, "TPIDR_EL1 must hold KPCR address");

    // 7. Multi-Architecture HAL SMP Initialization (ARM64 8-Core Topology)
    auto& hal = hal::HardwareAbstractionLayer::get();
    hal.initialize(8, hal::ProcessorArchitecture::Arm64, 4000); // 8-core ARM64 @ 4.0 GHz
    TEST_ASSERT(hal.getProcessorCount() == 8, "HAL processor count must report 8 cores");
    auto* armKpcr = hal.getKpcr(0);
    TEST_ASSERT(armKpcr != nullptr, "HAL KPCR 0 must be non-null");
    TEST_ASSERT(armKpcr->prcb.architecture == hal::ProcessorArchitecture::Arm64, "PRCB architecture must be Arm64");
    TEST_ASSERT(armKpcr->prcb.coreClockMhz == 4000, "PRCB core clock must be 4000 MHz");

    // Revert HAL back to 4-core AMD64 for standard baseline
    hal.initialize(4, hal::ProcessorArchitecture::Amd64, 3600);
    TEST_ASSERT(hal.getProcessorCount() == 4, "HAL reset to 4 cores verified");
}

// ============================================================================
// Suite 41: Named Pipes & Mailslots IPC Subsystem (NPFS / MSFS)
// ============================================================================
void Test_NamedPipesAndMailslotsIpc() {
    using namespace win32;

    // Initialize Win32 exports and VFS
    InitializeWin32SubsystemExports();
    fs::VirtualFileSystem::get().initialize();

    // ------------------------------------------------------------------------
    // 1. Named Pipe Creation & Server Configuration
    // ------------------------------------------------------------------------
    HANDLE hServerPipe = CreateNamedPipeW(
        L"\\\\.\\pipe\\MicaTestPipe",
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        2,    // max 2 instances
        1024, // out buffer
        1024, // in buffer
        100   // 100ms default timeout
    );
    TEST_ASSERT(hServerPipe != INVALID_HANDLE_VALUE, "CreateNamedPipeW must create valid pipe handle");

    DWORD flags = 0, outBuf = 0, inBuf = 0, maxInst = 0;
    BOOL infoOk = GetNamedPipeInfo(hServerPipe, &flags, &outBuf, &inBuf, &maxInst);
    TEST_ASSERT(infoOk == TRUE, "GetNamedPipeInfo must succeed on valid pipe handle");
    TEST_ASSERT((flags & PIPE_SERVER_END) != 0, "Pipe flags must contain PIPE_SERVER_END");
    TEST_ASSERT((flags & PIPE_TYPE_MESSAGE) != 0, "Pipe flags must contain PIPE_TYPE_MESSAGE");
    TEST_ASSERT(outBuf == 1024 && inBuf == 1024, "Buffer sizes must match creation parameters");
    TEST_ASSERT(maxInst == 2, "Max instances must report 2");

    // ------------------------------------------------------------------------
    // 2. Client Connection & Handshake (CreateFileW & ConnectNamedPipe)
    // ------------------------------------------------------------------------
    std::atomic<bool> clientConnected{false};
    HANDLE hClientPipe = INVALID_HANDLE_VALUE;

    std::thread clientThread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        hClientPipe = CreateFileW(
            L"\\\\.\\pipe\\MicaTestPipe",
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            0
        );
        clientConnected = (hClientPipe != INVALID_HANDLE_VALUE);
    });

    BOOL connOk = ConnectNamedPipe(hServerPipe);
    TEST_ASSERT(connOk == TRUE, "ConnectNamedPipe must return TRUE on successful client rendezvous");

    if (clientThread.joinable()) {
        clientThread.join();
    }
    TEST_ASSERT(clientConnected.load(), "Client CreateFileW to \\\\.\\pipe\\MicaTestPipe must succeed");
    TEST_ASSERT(hClientPipe != INVALID_HANDLE_VALUE, "Client pipe handle must be valid");

    // ConnectNamedPipe on already connected pipe returns FALSE with ERROR_PIPE_CONNECTED
    BOOL secondConn = ConnectNamedPipe(hServerPipe);
    TEST_ASSERT(secondConn == FALSE, "ConnectNamedPipe on connected pipe must return FALSE");
    TEST_ASSERT(GetLastError() == ERROR_PIPE_CONNECTED, "GetLastError must return ERROR_PIPE_CONNECTED");

    // ------------------------------------------------------------------------
    // 3. Bidirectional Duplex I/O (Client -> Server and Server -> Client)
    // ------------------------------------------------------------------------
    const char clientMsg[] = "MicaNT RPC Request: QueryHostVitals";
    DWORD bytesWritten = 0;
    BOOL writeOk = WriteFile(hClientPipe, clientMsg, static_cast<DWORD>(strlen(clientMsg)), &bytesWritten);
    TEST_ASSERT(writeOk == TRUE, "Client WriteFile must succeed");
    TEST_ASSERT(bytesWritten == strlen(clientMsg), "Bytes written must match request length");

    char serverRecvBuf[128]{};
    DWORD bytesRead = 0;
    BOOL readOk = ReadFile(hServerPipe, serverRecvBuf, sizeof(serverRecvBuf), &bytesRead);
    TEST_ASSERT(readOk == TRUE, "Server ReadFile must succeed");
    TEST_ASSERT(bytesRead == strlen(clientMsg), "Server bytes read must match request length");
    TEST_ASSERT(std::string_view(serverRecvBuf, bytesRead) == clientMsg, "Server received text must match client payload");

    const char serverReply[] = "MicaNT RPC Response: CPU0=4.0GHz STATUS=NOMINAL";
    writeOk = WriteFile(hServerPipe, serverReply, static_cast<DWORD>(strlen(serverReply)), &bytesWritten);
    TEST_ASSERT(writeOk == TRUE, "Server WriteFile must succeed");

    char clientRecvBuf[128]{};
    readOk = ReadFile(hClientPipe, clientRecvBuf, sizeof(clientRecvBuf), &bytesRead);
    TEST_ASSERT(readOk == TRUE, "Client ReadFile must succeed");
    TEST_ASSERT(bytesRead == strlen(serverReply), "Client bytes read must match server reply length");
    TEST_ASSERT(std::string_view(clientRecvBuf, bytesRead) == serverReply, "Client received text must match server reply");

    // ------------------------------------------------------------------------
    // 4. Message-Mode Framing, Partial Reads & PeekNamedPipe
    // ------------------------------------------------------------------------
    const char packetMsg[] = "PACKET_HEADER_AND_PAYLOAD_BODY_0123456789";
    const DWORD packetLen = static_cast<DWORD>(strlen(packetMsg));
    writeOk = WriteFile(hServerPipe, packetMsg, packetLen, &bytesWritten);
    TEST_ASSERT(writeOk == TRUE && bytesWritten == packetLen, "Server packet write must succeed");

    // Peek 13 bytes from the pipe without consuming
    char peekBuf[14]{};
    DWORD peekBytesRead = 0, totalAvail = 0, leftMsg = 0;
    BOOL peekOk = PeekNamedPipe(hClientPipe, peekBuf, 13, &peekBytesRead, &totalAvail, &leftMsg);
    TEST_ASSERT(peekOk == TRUE, "PeekNamedPipe must succeed");
    TEST_ASSERT(peekBytesRead == 13, "Peeked bytes read must be 13");
    TEST_ASSERT(totalAvail == packetLen, "Total available bytes must match packetLen");
    TEST_ASSERT(leftMsg == packetLen, "Bytes left this message must match packetLen");
    TEST_ASSERT(std::string_view(peekBuf, 13) == "PACKET_HEADER", "Peeked data must match prefix");

    // Partial ReadFile (buffer smaller than message -> returns FALSE + ERROR_MORE_DATA)
    char partialBuf[13]{};
    readOk = ReadFile(hClientPipe, partialBuf, 13, &bytesRead);
    TEST_ASSERT(readOk == FALSE, "Partial ReadFile in message mode must return FALSE");
    TEST_ASSERT(GetLastError() == ERROR_MORE_DATA, "GetLastError must return ERROR_MORE_DATA (234)");
    TEST_ASSERT(bytesRead == 13, "Bytes read must match partial buffer length");
    TEST_ASSERT(std::string_view(partialBuf, 13) == "PACKET_HEADER", "Partial read content must match prefix");

    // Read the remainder of the message
    char remainBuf[64]{};
    readOk = ReadFile(hClientPipe, remainBuf, sizeof(remainBuf), &bytesRead);
    TEST_ASSERT(readOk == TRUE, "Second ReadFile for remaining message must return TRUE");
    TEST_ASSERT(bytesRead == (packetLen - 13), "Remaining bytes read must complete the message");
    TEST_ASSERT(std::string_view(remainBuf, bytesRead) == "_AND_PAYLOAD_BODY_0123456789", "Remaining payload must match remainder");

    // ------------------------------------------------------------------------
    // 5. Transactional RPC (TransactNamedPipe)
    // ------------------------------------------------------------------------
    std::thread rpcServerThread([&]() {
        char req[64]{};
        DWORD reqRead = 0;
        if (ReadFile(hServerPipe, req, sizeof(req), &reqRead)) {
            if (std::string_view(req, reqRead) == "TRANSACT_ECHO_TEST") {
                const char resp[] = "TRANSACT_ECHO_ACK";
                DWORD rWritten = 0;
                WriteFile(hServerPipe, resp, static_cast<DWORD>(strlen(resp)), &rWritten);
            }
        }
    });

    const char transactReq[] = "TRANSACT_ECHO_TEST";
    char transactResp[64]{};
    DWORD transactRead = 0;
    BOOL transactOk = TransactNamedPipe(
        hClientPipe,
        const_cast<char*>(transactReq),
        static_cast<DWORD>(strlen(transactReq)),
        transactResp,
        sizeof(transactResp),
        &transactRead
    );
    TEST_ASSERT(transactOk == TRUE, "TransactNamedPipe must complete atomic write-and-read transaction");
    TEST_ASSERT(std::string_view(transactResp, transactRead) == "TRANSACT_ECHO_ACK", "Transact response must match ACK");

    if (rpcServerThread.joinable()) {
        rpcServerThread.join();
    }

    // ------------------------------------------------------------------------
    // 6. Multi-Instance Pipes & WaitNamedPipeW
    // ------------------------------------------------------------------------
    HANDLE hServerPipe2 = CreateNamedPipeW(
        L"\\\\.\\pipe\\MicaTestPipe",
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        2, 1024, 1024, 100
    );
    TEST_ASSERT(hServerPipe2 != INVALID_HANDLE_VALUE, "Second instance creation must succeed");

    HANDLE hServerPipe3 = CreateNamedPipeW(
        L"\\\\.\\pipe\\MicaTestPipe",
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        2, 1024, 1024, 100
    );
    TEST_ASSERT(hServerPipe3 == INVALID_HANDLE_VALUE, "Third instance creation must fail (PIPE_BUSY)");
    TEST_ASSERT(GetLastError() == ERROR_PIPE_BUSY, "GetLastError must report ERROR_PIPE_BUSY (231)");

    BOOL waitOk = WaitNamedPipeW(L"\\\\.\\pipe\\MicaTestPipe", 50);
    TEST_ASSERT(waitOk == TRUE, "WaitNamedPipeW must succeed while instance 2 is listening");

    CloseHandle(hServerPipe2);

    // ------------------------------------------------------------------------
    // 7. Server Disconnection & Broken Pipe Handling
    // ------------------------------------------------------------------------
    BOOL discOk = DisconnectNamedPipe(hServerPipe);
    TEST_ASSERT(discOk == TRUE, "DisconnectNamedPipe must succeed");

    const char failMsg[] = "DeadWrite";
    DWORD deadWritten = 0;
    BOOL failWrite = WriteFile(hClientPipe, failMsg, static_cast<DWORD>(strlen(failMsg)), &deadWritten);
    TEST_ASSERT(failWrite == FALSE, "WriteFile to disconnected pipe must fail");
    TEST_ASSERT(GetLastError() == ERROR_BROKEN_PIPE || GetLastError() == ERROR_PIPE_NOT_CONNECTED,
                "GetLastError must return ERROR_BROKEN_PIPE or ERROR_PIPE_NOT_CONNECTED");

    CloseHandle(hClientPipe);
    CloseHandle(hServerPipe);

    // ------------------------------------------------------------------------
    // 8. Mailslot Subsystem (MSFS): Create, Broadcast Datagrams, Info, Timeouts
    // ------------------------------------------------------------------------
    HANDLE hMailslot = CreateMailslotW(
        L"\\\\.\\mailslot\\MicaAlertChannel",
        256,  // max message size
        50    // 50ms read timeout
    );
    TEST_ASSERT(hMailslot != INVALID_HANDLE_VALUE, "CreateMailslotW must return valid mailslot handle");

    DWORD slotMaxSize = 0, slotNextSize = 0, slotCount = 0, slotTimeout = 0;
    BOOL slotInfoOk = GetMailslotInfo(hMailslot, &slotMaxSize, &slotNextSize, &slotCount, &slotTimeout);
    TEST_ASSERT(slotInfoOk == TRUE, "GetMailslotInfo must succeed on newly created mailslot");
    TEST_ASSERT(slotMaxSize == 256, "Max message size must match 256");
    TEST_ASSERT(slotCount == 0, "Initial message count must be 0");
    TEST_ASSERT(slotNextSize == MAILSLOT_NO_MESSAGE, "Initial next size must report MAILSLOT_NO_MESSAGE");
    TEST_ASSERT(slotTimeout == 50, "Read timeout must report 50ms");

    HANDLE hClientWriter1 = CreateFileW(
        L"\\\\.\\mailslot\\MicaAlertChannel",
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        0
    );
    TEST_ASSERT(hClientWriter1 != INVALID_HANDLE_VALUE, "Client 1 CreateFileW to mailslot must succeed");

    const char alert1[] = "ALERT_1: SECURE_BOOT_VERIFIED";
    DWORD alert1Written = 0;
    writeOk = WriteFile(hClientWriter1, alert1, static_cast<DWORD>(strlen(alert1)), &alert1Written);
    TEST_ASSERT(writeOk == TRUE && alert1Written == strlen(alert1), "Client 1 datagram write must succeed");

    HANDLE hClientWriter2 = CreateFileW(
        L"\\\\.\\mailslot\\MicaAlertChannel",
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        0
    );
    TEST_ASSERT(hClientWriter2 != INVALID_HANDLE_VALUE, "Client 2 CreateFileW to mailslot must succeed");

    const char alert2[] = "ALERT_2: KERNEL_INTEGRITY_PASSED";
    DWORD alert2Written = 0;
    writeOk = WriteFile(hClientWriter2, alert2, static_cast<DWORD>(strlen(alert2)), &alert2Written);
    TEST_ASSERT(writeOk == TRUE && alert2Written == strlen(alert2), "Client 2 datagram write must succeed");

    slotInfoOk = GetMailslotInfo(hMailslot, nullptr, &slotNextSize, &slotCount, nullptr);
    TEST_ASSERT(slotInfoOk == TRUE, "GetMailslotInfo must succeed with pending datagrams");
    TEST_ASSERT(slotCount == 2, "Mailslot message count must report 2");
    TEST_ASSERT(slotNextSize == strlen(alert1), "Next message size must match alert1 length");

    char dgramBuf[128]{};
    DWORD dgramRead = 0;
    readOk = ReadFile(hMailslot, dgramBuf, sizeof(dgramBuf), &dgramRead);
    TEST_ASSERT(readOk == TRUE, "Mailslot server ReadFile must retrieve datagram 1");
    TEST_ASSERT(dgramRead == strlen(alert1), "Datagram 1 read length must match written size");
    TEST_ASSERT(std::string_view(dgramBuf, dgramRead) == alert1, "Datagram 1 payload must match alert1");

    readOk = ReadFile(hMailslot, dgramBuf, sizeof(dgramBuf), &dgramRead);
    TEST_ASSERT(readOk == TRUE, "Mailslot server ReadFile must retrieve datagram 2");
    TEST_ASSERT(dgramRead == strlen(alert2), "Datagram 2 read length must match written size");
    TEST_ASSERT(std::string_view(dgramBuf, dgramRead) == alert2, "Datagram 2 payload must match alert2");

    GetMailslotInfo(hMailslot, nullptr, &slotNextSize, &slotCount, nullptr);
    TEST_ASSERT(slotCount == 0, "Queue must be empty after reading both messages");
    TEST_ASSERT(slotNextSize == MAILSLOT_NO_MESSAGE, "Next size must be MAILSLOT_NO_MESSAGE");

    SetMailslotInfo(hMailslot, 10); // set 10ms timeout
    readOk = ReadFile(hMailslot, dgramBuf, sizeof(dgramBuf), &dgramRead);
    TEST_ASSERT(readOk == FALSE, "ReadFile on empty mailslot must time out");

    CloseHandle(hClientWriter1);
    CloseHandle(hClientWriter2);
    CloseHandle(hMailslot);
}

// ============================================================================
// Test Suite 42: NTFS Filesystem, Master File Table ($MFT) & Alternate Streams
// ============================================================================

void Test_NtfsFileSystemAndMasterFileTable() {
    using namespace micant;
    using namespace micant::ntfs;

    // 1. Format and Mount NTFS on Block Device (8 MB RAM disk)
    auto ramDisk = std::make_shared<storage::RamDiskDevice>(L"\\Device\\HarddiskNtfs", 8 * 1024 * 1024ULL, storage::SECTOR_SIZE_512); // 8 MB
    auto ntfs = std::make_shared<NtfsFileSystem>();

    NtStatus stFormat = ntfs->format(*ramDisk, DEFAULT_CLUSTER_SIZE, L"MicaNT_System");
    TEST_ASSERT(NT_SUCCESS(stFormat), "NTFS format must succeed on 8MB block device");

    NtStatus stMount = ntfs->mount(ramDisk);
    TEST_ASSERT(NT_SUCCESS(stMount), "NTFS volume mount must succeed");
    TEST_ASSERT(ntfs->getClusterSize() == 4096, "NTFS cluster size must be 4096 bytes");
    TEST_ASSERT(ntfs->getVolumeSerialNumber() != 0, "NTFS volume serial number must be non-zero");

    // 2. MFT Record Engine & USA Fixup Validation
    std::vector<uint8_t> rootRecord = ntfs->serializeRecord(MFT_REC_ROOT);
    TEST_ASSERT(rootRecord.size() == MFT_RECORD_SIZE, "Serialized MFT record must be 1024 bytes");
    const auto* rootHdr = reinterpret_cast<const MftRecordHeader*>(rootRecord.data());
    TEST_ASSERT(rootHdr->magic == NTFS_FILE_SIGNATURE, "MFT record magic must be 'FILE' (0x454C4946)");
    TEST_ASSERT((rootHdr->flags & MFT_RECORD_IN_USE) != 0, "Root MFT record must be marked IN_USE");
    TEST_ASSERT((rootHdr->flags & MFT_RECORD_DIRECTORY) != 0, "Root MFT record must be marked DIRECTORY");

    // Test USA fixup integrity & corruption detection
    std::vector<uint8_t> corruptedRecord = rootRecord;
    corruptedRecord[510] ^= 0xFF; // Tamper with sector 0 USA sequence number
    bool fixupBad = UsaEngine::applyFixups(corruptedRecord.data(), corruptedRecord.size());
    TEST_ASSERT(fixupBad == false, "USA engine must detect torn write / corrupted sector boundary");

    bool fixupOk = UsaEngine::applyFixups(rootRecord.data(), rootRecord.size());
    TEST_ASSERT(fixupOk == true, "USA engine must cleanly apply fixups to valid MFT record");

    // 3. Data Run Encoding and Decoding (LCN/VCN Runs)
    std::vector<DataRun> inputRuns = {
        DataRun{ .vcnStart = 0,   .lcnStart = 100, .clusterCount = 16 },
        DataRun{ .vcnStart = 16,  .lcnStart = 250, .clusterCount = 32 },
        DataRun{ .vcnStart = 48,  .lcnStart = -1,  .clusterCount = 8  }, // Sparse run
        DataRun{ .vcnStart = 56,  .lcnStart = 300, .clusterCount = 64 }
    };
    std::vector<uint8_t> encodedRuns = DataRunCodec::encode(inputRuns);
    TEST_ASSERT(!encodedRuns.empty(), "Encoded data runs must not be empty");
    TEST_ASSERT(encodedRuns.back() == 0, "Data run list must terminate with zero byte");

    std::vector<DataRun> decodedRuns;
    bool decOk = DataRunCodec::decode(encodedRuns.data(), encodedRuns.size(), 0, decodedRuns);
    TEST_ASSERT(decOk == true, "Data run decoding must succeed");
    TEST_ASSERT(decodedRuns.size() == inputRuns.size(), "Decoded run count must match input");
    for (size_t i = 0; i < inputRuns.size(); ++i) {
        TEST_ASSERT(decodedRuns[i].vcnStart == inputRuns[i].vcnStart, "VCN start must match");
        TEST_ASSERT(decodedRuns[i].lcnStart == inputRuns[i].lcnStart, "LCN start must match");
        TEST_ASSERT(decodedRuns[i].clusterCount == inputRuns[i].clusterCount, "Cluster count must match");
    }

    // 4. File Creation, Streaming & Offset Reading
    uint64_t recHosts = 0;
    NtStatus stCr = ntfs->createFile(L"drivers\\etc\\hosts", fs::FILE_ATTRIBUTE_NORMAL, recHosts);
    TEST_ASSERT(NT_SUCCESS(stCr), "createFile for hosts must succeed");
    TEST_ASSERT(recHosts >= MFT_REC_USER_START, "User file record must be >= 16");

    const std::string hostsData = "127.0.0.1 localhost\n::1 localhost\n192.168.1.1 router\n";
    uint64_t bytesWritten = 0;
    NtStatus stWr = ntfs->writeFile(recHosts, L"", hostsData.data(), hostsData.size(), 0, bytesWritten);
    TEST_ASSERT(NT_SUCCESS(stWr), "writeFile to primary stream must succeed");
    TEST_ASSERT(bytesWritten == hostsData.size(), "Bytes written must match hosts data length");

    std::vector<char> readBuf(hostsData.size() + 1, 0);
    uint64_t bytesRead = 0;
    NtStatus stRd = ntfs->readFile(recHosts, L"", readBuf.data(), hostsData.size(), 0, bytesRead);
    TEST_ASSERT(NT_SUCCESS(stRd), "readFile from primary stream must succeed");
    TEST_ASSERT(bytesRead == hostsData.size(), "Bytes read must match hosts data length");
    TEST_ASSERT(std::string_view(readBuf.data(), bytesRead) == hostsData, "Primary stream content must match");

    // Partial / Offset Read
    char partialBuf[16]{};
    uint64_t partialRead = 0;
    NtStatus stPart = ntfs->readFile(recHosts, L"", partialBuf, 9, 10, partialRead);
    TEST_ASSERT(NT_SUCCESS(stPart), "Offset read from primary stream must succeed");
    TEST_ASSERT(partialRead == 9, "Partial read bytes must be 9");
    TEST_ASSERT(std::string_view(partialBuf, 9) == "localhost", "Offset read payload must match 'localhost'");

    // 5. Alternate Data Streams (ADS)
    const std::string zoneData = "[ZoneTransfer]\nZoneId=3\nReferrerUrl=https://micant.org\n";
    uint64_t zoneWritten = 0;
    NtStatus stZoneWr = ntfs->writeFile(recHosts, L"Zone.Identifier", zoneData.data(), zoneData.size(), 0, zoneWritten);
    TEST_ASSERT(NT_SUCCESS(stZoneWr), "writeFile to Alternate Data Stream must succeed");
    TEST_ASSERT(zoneWritten == zoneData.size(), "Zone bytes written must match");

    // Verify Primary stream is still intact and unaffected
    std::vector<char> verifyPrimBuf(hostsData.size(), 0);
    uint64_t verifyPrimRead = 0;
    ntfs->readFile(recHosts, L"", verifyPrimBuf.data(), hostsData.size(), 0, verifyPrimRead);
    TEST_ASSERT(std::string_view(verifyPrimBuf.data(), verifyPrimRead) == hostsData, "Primary stream must remain intact after ADS write");

    // Verify Alternate Data Stream reading
    std::vector<char> zoneReadBuf(zoneData.size() + 1, 0);
    uint64_t zoneRead = 0;
    NtStatus stZoneRd = ntfs->readFile(recHosts, L"Zone.Identifier", zoneReadBuf.data(), zoneData.size(), 0, zoneRead);
    TEST_ASSERT(NT_SUCCESS(stZoneRd), "readFile from Alternate Data Stream must succeed");
    TEST_ASSERT(zoneRead == zoneData.size(), "Zone bytes read must match");
    TEST_ASSERT(std::string_view(zoneReadBuf.data(), zoneRead) == zoneData, "ADS stream content must match zoneData");

    // Reading non-existent stream must fail
    uint64_t badRead = 0;
    char dummy[8]{};
    NtStatus stBadStream = ntfs->readFile(recHosts, L"NonExistentStream", dummy, sizeof(dummy), 0, badRead);
    TEST_ASSERT(stBadStream == NtStatus::NoSuchFile, "Reading non-existent stream must return NoSuchFile");

    // 6. $LogFile Transaction Journal & WAL Verification
    auto* journal = ntfs->getJournal();
    TEST_ASSERT(journal != nullptr, "NTFS journal must be initialized");
    TEST_ASSERT(journal->getEntryCount() >= 3, "Journal must have logged file creation and writes");
    uint64_t lsnBeforeCheckpoint = journal->getLastLsn();

    journal->checkpoint();
    TEST_ASSERT(journal->getLastCheckpointLsn() > lsnBeforeCheckpoint, "Checkpoint LSN must advance");

    const std::string appendData = "10.0.0.1 db.server\n";
    uint64_t appendWritten = 0;
    ntfs->writeFile(recHosts, L"", appendData.data(), appendData.size(), hostsData.size(), appendWritten);

    auto recentEntries = journal->getEntriesSinceCheckpoint();
    TEST_ASSERT(!recentEntries.empty(), "Journal must contain log entries since last checkpoint");
    bool foundWriteOp = false;
    for (const auto& entry : recentEntries) {
        if (entry.op == LogOperation::WriteResidentData && entry.recordNumber == recHosts) {
            foundWriteOp = true;
            break;
        }
    }
    TEST_ASSERT(foundWriteOp == true, "Journal must record WriteResidentData operation in WAL");

    // 7. Virtual File System (VFS) Seamless Routing & ADS Integration
    auto& vfs = fs::VirtualFileSystem::get();
    vfs.setMountedNtfs(ntfs);

    std::shared_ptr<fs::FileObject> vfsFile;
    NtStatus stVfsCr = vfs.createOrOpenFile(
        L"D:\\SecurityReport.log:Summary",
        fs::FILE_GENERIC_READ | fs::FILE_GENERIC_WRITE,
        fs::FILE_CREATE,
        vfsFile
    );
    TEST_ASSERT(NT_SUCCESS(stVfsCr), "VFS createOrOpenFile on NTFS ADS must succeed");
    TEST_ASSERT(vfsFile != nullptr, "VFS FileObject must be non-null");

    const std::string repSummary = "AUDIT_RESULT: ZERO_CORRUPTION_DETECTED";
    uint32_t vfsWritten = 0;
    NtStatus stVfsWr = vfs.writeFile(vfsFile.get(), repSummary.data(), static_cast<uint32_t>(repSummary.size()), nullptr, vfsWritten);
    TEST_ASSERT(NT_SUCCESS(stVfsWr), "VFS writeFile to NTFS ADS must succeed");
    TEST_ASSERT(vfsWritten == repSummary.size(), "VFS bytes written must match summary length");

    // Read back via VFS
    char vfsReadBuf[64]{};
    uint32_t vfsBytesRead = 0;
    LargeInteger readOffset{};
    readOffset.quadPart = 0;
    NtStatus stVfsRd = vfs.readFile(vfsFile.get(), vfsReadBuf, sizeof(vfsReadBuf), &readOffset, vfsBytesRead);
    TEST_ASSERT(NT_SUCCESS(stVfsRd), "VFS readFile from NTFS ADS must succeed");
    TEST_ASSERT(vfsBytesRead == repSummary.size(), "VFS bytes read must match summary length");
    TEST_ASSERT(std::string_view(vfsReadBuf, vfsBytesRead) == repSummary, "VFS payload must match repSummary");

    vfs.closeFile(vfsFile.get());
}

void Test_ServiceControlManager_And_SvcHost() {
    using namespace scm;

    // 1. SCM Initialization & Built-in System Services
    auto& scm = ServiceControlManager::get();
    scm.initialize();

    TEST_ASSERT(scm.getServiceCount() >= 7, "SCM must have at least 7 default system services");

    auto rpcSs = scm.getServiceRecord(L"RpcSs");
    TEST_ASSERT(rpcSs != nullptr, "RpcSs service must exist");
    TEST_ASSERT(rpcSs->serviceType == SERVICE_WIN32_SHARE_PROCESS, "RpcSs must be share process");
    TEST_ASSERT(rpcSs->status.dwCurrentState == SERVICE_RUNNING, "RpcSs must be running");
    TEST_ASSERT(rpcSs->svchostGroup == "DcomLaunch", "RpcSs group must be DcomLaunch");
    TEST_ASSERT(rpcSs->status.dwProcessId > 0, "RpcSs must have non-zero PID");

    auto tcpip = scm.getServiceRecord(L"Tcpip");
    TEST_ASSERT(tcpip != nullptr, "Tcpip service must exist");
    TEST_ASSERT(tcpip->serviceType == SERVICE_KERNEL_DRIVER, "Tcpip must be a kernel driver");
    TEST_ASSERT(tcpip->status.dwCurrentState == SERVICE_RUNNING, "Tcpip must be running");
    TEST_ASSERT(tcpip->status.dwProcessId == 4, "Tcpip driver PID must be 4 (System)");

    auto dhcp = scm.getServiceRecord(L"Dhcp");
    TEST_ASSERT(dhcp != nullptr, "Dhcp service must exist");
    TEST_ASSERT(dhcp->status.dwCurrentState == SERVICE_RUNNING, "Dhcp must be running");
    TEST_ASSERT(dhcp->svchostGroup == "netsvcs", "Dhcp group must be netsvcs");
    TEST_ASSERT(!dhcp->dependencies.empty() && dhcp->dependencies[0] == L"Tcpip", "Dhcp must depend on Tcpip");

    // 2. Win32 SCM API Parity (advapi32.dll)
    advapi32::SC_HANDLE hScm = advapi32::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    TEST_ASSERT(hScm != nullptr, "OpenSCManagerW must succeed with SC_MANAGER_ALL_ACCESS");

    advapi32::SC_HANDLE hSpooler = advapi32::CreateServiceW(
        hScm,
        L"Spooler",
        L"Print Spooler",
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_NORMAL,
        L"C:\\Windows\\System32\\spoolsv.exe",
        L"",
        nullptr,
        L"RpcSs\0",
        L"LocalSystem",
        nullptr
    );
    TEST_ASSERT(hSpooler != nullptr, "CreateServiceW for Spooler must succeed");

    SERVICE_STATUS spoolerStatus{};
    win32::BOOL bQuery = advapi32::QueryServiceStatus(hSpooler, &spoolerStatus);
    TEST_ASSERT(bQuery == win32::TRUE, "QueryServiceStatus for Spooler must succeed");
    TEST_ASSERT(spoolerStatus.dwCurrentState == SERVICE_STOPPED, "Newly created Spooler must be in SERVICE_STOPPED state");
    TEST_ASSERT((spoolerStatus.dwControlsAccepted & SERVICE_ACCEPT_STOP) != 0, "Spooler must accept STOP control");

    // Attempting to create duplicate service must fail with ERROR_SERVICE_EXISTS
    advapi32::SC_HANDLE hDup = advapi32::CreateServiceW(
        hScm,
        L"Spooler",
        L"Duplicate",
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_NORMAL,
        L"C:\\Windows\\System32\\spoolsv.exe",
        nullptr, nullptr, nullptr, nullptr, nullptr
    );
    TEST_ASSERT(hDup == nullptr, "Create duplicate service must fail");
    TEST_ASSERT(win32::GetLastError() == ERROR_SERVICE_EXISTS, "Error must be ERROR_SERVICE_EXISTS");

    // 3. Topological Dependency Resolution & Auto-Start
    advapi32::SC_HANDLE hSvcA = advapi32::CreateServiceW(
        hScm, L"TestSvcA", L"Base Service A", SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
        L"svca.exe", nullptr, nullptr, nullptr, nullptr, nullptr
    );
    TEST_ASSERT(hSvcA != nullptr, "CreateServiceW for TestSvcA must succeed");

    advapi32::SC_HANDLE hSvcB = advapi32::CreateServiceW(
        hScm, L"TestSvcB", L"Intermediate Service B", SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
        L"svcb.exe", nullptr, nullptr, L"TestSvcA\0", nullptr, nullptr
    );
    TEST_ASSERT(hSvcB != nullptr, "CreateServiceW for TestSvcB must succeed");

    advapi32::SC_HANDLE hSvcC = advapi32::CreateServiceW(
        hScm, L"TestSvcC", L"Top-Level Service C", SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
        L"svcc.exe", nullptr, nullptr, L"TestSvcB\0", nullptr, nullptr
    );
    TEST_ASSERT(hSvcC != nullptr, "CreateServiceW for TestSvcC must succeed");

    std::vector<std::wstring> depOrder;
    uint32_t depRes = scm.resolveDependencies(L"TestSvcC", depOrder);
    TEST_ASSERT(depRes == ERROR_SUCCESS, "resolveDependencies on TestSvcC must succeed");
    TEST_ASSERT(depOrder.size() == 3, "Dependency order must include exactly 3 services");
    TEST_ASSERT(depOrder[0] == L"TestSvcA", "First start prerequisite must be TestSvcA");
    TEST_ASSERT(depOrder[1] == L"TestSvcB", "Second start prerequisite must be TestSvcB");
    TEST_ASSERT(depOrder[2] == L"TestSvcC", "Third target must be TestSvcC");

    // Starting TestSvcC must automatically start TestSvcA and TestSvcB
    win32::BOOL bStartC = advapi32::StartServiceW(hSvcC, 0, nullptr);
    TEST_ASSERT(bStartC == win32::TRUE, "StartServiceW on TestSvcC must succeed");

    SERVICE_STATUS stA{}, stB{}, stC{};
    advapi32::QueryServiceStatus(hSvcA, &stA);
    advapi32::QueryServiceStatus(hSvcB, &stB);
    advapi32::QueryServiceStatus(hSvcC, &stC);
    TEST_ASSERT(stA.dwCurrentState == SERVICE_RUNNING, "TestSvcA must be auto-started and RUNNING");
    TEST_ASSERT(stB.dwCurrentState == SERVICE_RUNNING, "TestSvcB must be auto-started and RUNNING");
    TEST_ASSERT(stC.dwCurrentState == SERVICE_RUNNING, "TestSvcC must be RUNNING");

    // 4. Circular Dependency Detection
    advapi32::CreateServiceW(
        hScm, L"CycleAlpha", L"Cycle Alpha", SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
        L"cycle1.exe", nullptr, nullptr, L"CycleBeta\0", nullptr, nullptr
    );
    advapi32::SC_HANDLE hCycleBeta = advapi32::CreateServiceW(
        hScm, L"CycleBeta", L"Cycle Beta", SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
        L"cycle2.exe", nullptr, nullptr, L"CycleAlpha\0", nullptr, nullptr
    );
    TEST_ASSERT(hCycleBeta != nullptr, "CreateServiceW for CycleBeta must succeed");

    std::vector<std::wstring> cycleOrder;
    uint32_t cycleRes = scm.resolveDependencies(L"CycleBeta", cycleOrder);
    TEST_ASSERT(cycleRes == ERROR_CIRCULAR_DEPENDENCY, "Circular dependency must be detected");

    win32::BOOL bCycleStart = advapi32::StartServiceW(hCycleBeta, 0, nullptr);
    TEST_ASSERT(bCycleStart == win32::FALSE, "Starting cyclic service must fail");
    TEST_ASSERT(win32::GetLastError() == ERROR_CIRCULAR_DEPENDENCY, "Error must be ERROR_CIRCULAR_DEPENDENCY");

    // 5. Dependent Service Running Guard
    // TestSvcC depends on TestSvcB, which depends on TestSvcA.
    // Attempting to stop TestSvcA while TestSvcB / TestSvcC are running must fail!
    SERVICE_STATUS stopStatus{};
    win32::BOOL bStopA = advapi32::ControlService(hSvcA, SERVICE_CONTROL_STOP, &stopStatus);
    TEST_ASSERT(bStopA == win32::FALSE, "Stopping TestSvcA while dependent services run must fail");
    TEST_ASSERT(win32::GetLastError() == ERROR_DEPENDENT_SERVICES_RUNNING, "Error must be ERROR_DEPENDENT_SERVICES_RUNNING");

    // Stop TestSvcC first
    win32::BOOL bStopC = advapi32::ControlService(hSvcC, SERVICE_CONTROL_STOP, &stopStatus);
    TEST_ASSERT(bStopC == win32::TRUE, "Stopping TestSvcC must succeed");
    TEST_ASSERT(stopStatus.dwCurrentState == SERVICE_STOPPED, "TestSvcC must be stopped");

    // Stop TestSvcB next
    win32::BOOL bStopB = advapi32::ControlService(hSvcB, SERVICE_CONTROL_STOP, &stopStatus);
    TEST_ASSERT(bStopB == win32::TRUE, "Stopping TestSvcB must succeed");
    TEST_ASSERT(stopStatus.dwCurrentState == SERVICE_STOPPED, "TestSvcB must be stopped");

    // Now stopping TestSvcA must succeed
    bStopA = advapi32::ControlService(hSvcA, SERVICE_CONTROL_STOP, &stopStatus);
    TEST_ASSERT(bStopA == win32::TRUE, "Stopping TestSvcA must now succeed");
    TEST_ASSERT(stopStatus.dwCurrentState == SERVICE_STOPPED, "TestSvcA must be stopped");

    // 6. Service Handler Callbacks & State Transitions (Pause / Continue / Interrogate)
    static std::vector<uint32_t> s_ReceivedControls;
    auto testDaemon = scm.getServiceRecord(L"Spooler");
    TEST_ASSERT(testDaemon != nullptr, "Spooler record must exist");
    testDaemon->handler = [](uint32_t ctrl) {
        s_ReceivedControls.push_back(ctrl);
    };

    win32::BOOL bStartSpooler = advapi32::StartServiceW(hSpooler, 0, nullptr);
    TEST_ASSERT(bStartSpooler == win32::TRUE, "StartServiceW on Spooler must succeed");
    advapi32::QueryServiceStatus(hSpooler, &spoolerStatus);
    TEST_ASSERT(spoolerStatus.dwCurrentState == SERVICE_RUNNING, "Spooler must be RUNNING");

    // Pause
    advapi32::ControlService(hSpooler, SERVICE_CONTROL_PAUSE, &spoolerStatus);
    TEST_ASSERT(spoolerStatus.dwCurrentState == SERVICE_PAUSED, "Spooler must be PAUSED");

    // Continue
    advapi32::ControlService(hSpooler, SERVICE_CONTROL_CONTINUE, &spoolerStatus);
    TEST_ASSERT(spoolerStatus.dwCurrentState == SERVICE_RUNNING, "Spooler must be RUNNING after CONTINUE");

    // Interrogate
    advapi32::ControlService(hSpooler, SERVICE_CONTROL_INTERROGATE, &spoolerStatus);

    // Stop
    advapi32::ControlService(hSpooler, SERVICE_CONTROL_STOP, &spoolerStatus);
    TEST_ASSERT(spoolerStatus.dwCurrentState == SERVICE_STOPPED, "Spooler must be STOPPED");

    TEST_ASSERT(s_ReceivedControls.size() >= 4, "Handler must receive pause, continue, interrogate, and stop");
    TEST_ASSERT(s_ReceivedControls[0] == SERVICE_CONTROL_PAUSE, "First control must be PAUSE");
    TEST_ASSERT(s_ReceivedControls[1] == SERVICE_CONTROL_CONTINUE, "Second control must be CONTINUE");
    TEST_ASSERT(s_ReceivedControls[2] == SERVICE_CONTROL_INTERROGATE, "Third control must be INTERROGATE");
    TEST_ASSERT(s_ReceivedControls[3] == SERVICE_CONTROL_STOP, "Fourth control must be STOP");

    // 7. Shared Process Service Host (svchost.exe) Grouping
    auto dhcpRec = scm.getServiceRecord(L"Dhcp");
    auto dnsRec = scm.getServiceRecord(L"Dnscache");
    auto wrkRec = scm.getServiceRecord(L"LanmanWorkstation");
    TEST_ASSERT(dhcpRec != nullptr && dnsRec != nullptr && wrkRec != nullptr, "netsvcs services must exist");

    uint32_t netsvcsPid = SvcHostManager::get().getGroupPid("netsvcs");
    TEST_ASSERT(netsvcsPid > 0, "netsvcs host PID must be non-zero");
    TEST_ASSERT(dhcpRec->status.dwProcessId == netsvcsPid, "Dhcp PID must match netsvcs host PID");
    TEST_ASSERT(dnsRec->status.dwProcessId == netsvcsPid, "Dnscache PID must match netsvcs host PID");
    TEST_ASSERT(wrkRec->status.dwProcessId == netsvcsPid, "LanmanWorkstation PID must match netsvcs host PID");

    auto hostedInNetsvcs = SvcHostManager::get().getServicesInGroup("netsvcs");
    TEST_ASSERT(hostedInNetsvcs.size() >= 3, "netsvcs group must have at least 3 services");

    // Distinct groups must have distinct PIDs
    uint32_t dcomPid = SvcHostManager::get().getGroupPid("DcomLaunch");
    uint32_t localSvcPid = SvcHostManager::get().getGroupPid("LocalService");
    TEST_ASSERT(dcomPid != 0 && localSvcPid != 0, "DcomLaunch and LocalService must have valid PIDs");
    TEST_ASSERT(dcomPid != netsvcsPid, "DcomLaunch PID must be distinct from netsvcs");
    TEST_ASSERT(localSvcPid != netsvcsPid, "LocalService PID must be distinct from netsvcs");
    TEST_ASSERT(localSvcPid != dcomPid, "LocalService PID must be distinct from DcomLaunch");

    // 8. Kernel Driver Service Integration (SERVICE_KERNEL_DRIVER)
    static bool s_TestDriverLoaded = false;
    advapi32::SC_HANDLE hDrv = advapi32::CreateServiceW(
        hScm,
        L"MicaVirtStorageDriver",
        L"MicaNT Virtual Storage Miniport Driver",
        SERVICE_ALL_ACCESS,
        SERVICE_KERNEL_DRIVER,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_NORMAL,
        L"System32\\drivers\\micavstore.sys",
        L"SCSI miniport",
        nullptr, nullptr, nullptr, nullptr
    );
    TEST_ASSERT(hDrv != nullptr, "CreateServiceW for kernel driver must succeed");

    auto drvRec = scm.getServiceRecord(L"MicaVirtStorageDriver");
    TEST_ASSERT(drvRec != nullptr, "Driver record must exist");
    drvRec->driverEntry = [](io::DriverObject* drv, const UnicodeString* reg) -> NtStatus {
        (void)drv;
        (void)reg;
        s_TestDriverLoaded = true;
        return NtStatus::Success;
    };

    win32::BOOL bStartDrv = advapi32::StartServiceW(hDrv, 0, nullptr);
    TEST_ASSERT(bStartDrv == win32::TRUE, "StartServiceW on kernel driver must succeed");
    TEST_ASSERT(s_TestDriverLoaded == true, "Kernel driver DriverEntry must have been invoked");
    TEST_ASSERT(drvRec->status.dwCurrentState == SERVICE_RUNNING, "Driver service state must be RUNNING");
    TEST_ASSERT(drvRec->status.dwProcessId == 4, "Driver service PID must be 4 (System)");
    TEST_ASSERT(driver::DriverManager::get().lookupDriver(L"MicaVirtStorageDriver") != nullptr,
                "Driver must be registered in Ring 0 DriverManager");

    // 9. Named Pipe RPC Protocol over \\.\pipe\ntsvcs
    ScmRpcHeader reqHdr{
        .magic = SCM_RPC_REQ_MAGIC,
        .opCode = SCM_RPC_QUERY_STATUS,
        .dataLength = sizeof(ScmRpcRequestPayload)
    };
    ScmRpcRequestPayload reqPayload{};
    wcscpy_s(reqPayload.serviceName, L"Dhcp");

    // Open handle to Dhcp to pass to RPC
    advapi32::SC_HANDLE hDhcp = advapi32::OpenServiceW(hScm, L"Dhcp", SERVICE_QUERY_STATUS);
    TEST_ASSERT(hDhcp != nullptr, "OpenServiceW for Dhcp must succeed");
    reqPayload.param1 = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(hDhcp));

    std::vector<uint8_t> reqBuf(sizeof(ScmRpcHeader) + sizeof(ScmRpcRequestPayload));
    std::memcpy(reqBuf.data(), &reqHdr, sizeof(ScmRpcHeader));
    std::memcpy(reqBuf.data() + sizeof(ScmRpcHeader), &reqPayload, sizeof(ScmRpcRequestPayload));

    auto respBytes = scm.processRpcRequest(reqBuf.data(), reqBuf.size());
    TEST_ASSERT(respBytes.size() >= sizeof(ScmRpcHeader) + sizeof(ScmRpcResponsePayload),
                "RPC response size must be valid");

    const auto* respHdr = reinterpret_cast<const ScmRpcHeader*>(respBytes.data());
    const auto* respPl = reinterpret_cast<const ScmRpcResponsePayload*>(respBytes.data() + sizeof(ScmRpcHeader));
    TEST_ASSERT(respHdr->magic == SCM_RPC_RESP_MAGIC, "RPC response magic must be 'SCM2'");
    TEST_ASSERT(respPl->win32Error == ERROR_SUCCESS, "RPC query status must return ERROR_SUCCESS");
    TEST_ASSERT(respPl->currentState == SERVICE_RUNNING, "RPC returned state must be RUNNING");
    TEST_ASSERT(respPl->processId == netsvcsPid, "RPC returned PID must match netsvcs PID");

    advapi32::CloseServiceHandle(hDhcp);

    // 10. Command Shell net and sc Built-in Commands Verification
    shell::CommandShell testShell;
    std::ostringstream out;

    // Test 'net start' lists running services
    testShell.execute("net start", out);
    std::string netOut = out.str();
    TEST_ASSERT(netOut.find("These Windows services are started:") != std::string::npos, "'net start' header found");
    TEST_ASSERT(netOut.find("DHCP Client") != std::string::npos, "'net start' lists DHCP Client");
    TEST_ASSERT(netOut.find("Workstation") != std::string::npos, "'net start' lists Workstation");

    // Test 'sc query Dhcp'
    out.str("");
    out.clear();
    testShell.execute("sc query Dhcp", out);
    std::string scOut = out.str();
    TEST_ASSERT(scOut.find("SERVICE_NAME: Dhcp") != std::string::npos, "'sc query Dhcp' returns service name");
    TEST_ASSERT(scOut.find("STATE              : 4  RUNNING") != std::string::npos, "'sc query Dhcp' returns RUNNING");

    // Test 'sc stop MicaSec' and 'sc start MicaSec'
    out.str("");
    out.clear();
    testShell.execute("sc stop MicaSec", out);
    TEST_ASSERT(out.str().find("[SC] ControlService SUCCESS") != std::string::npos, "'sc stop MicaSec' succeeds");

    auto micaSecRec = scm.getServiceRecord(L"MicaSec");
    TEST_ASSERT(micaSecRec != nullptr && micaSecRec->status.dwCurrentState == SERVICE_STOPPED,
                "MicaSec state must be STOPPED");

    out.str("");
    out.clear();
    testShell.execute("sc start MicaSec", out);
    TEST_ASSERT(out.str().find("[SC] StartService SUCCESS") != std::string::npos, "'sc start MicaSec' succeeds");
    TEST_ASSERT(micaSecRec->status.dwCurrentState == SERVICE_RUNNING, "MicaSec state must be RUNNING");

    // Cleanup handles
    advapi32::CloseServiceHandle(hSpooler);
    advapi32::CloseServiceHandle(hSvcA);
    advapi32::CloseServiceHandle(hSvcB);
    advapi32::CloseServiceHandle(hSvcC);
    advapi32::CloseServiceHandle(hCycleBeta);
    advapi32::CloseServiceHandle(hDrv);
    advapi32::CloseServiceHandle(hScm);
}

void Test_Lsass_Winlogon_And_SamDatabase() {
    using namespace micant;

    // ========================================================================
    // 1. SAM Database Engine & Cryptographic NT-Hash
    // ========================================================================
    auto& samDb = sam::SamDatabase::get();

    // Verify default builtin accounts
    auto adminUser = samDb.getUser(L"Administrator");
    TEST_ASSERT(adminUser.has_value(), "Builtin Administrator account must exist in SAM");
    TEST_ASSERT(adminUser->rid == sam::DOMAIN_USER_RID_ADMIN, "Administrator RID must be 500");
    TEST_ASSERT(adminUser->primaryGroupRid == sam::DOMAIN_ALIAS_RID_ADMINS, "Administrator primary group must be Builtin Administrators");

    auto guestUser = samDb.getUser(L"Guest");
    TEST_ASSERT(guestUser.has_value(), "Builtin Guest account must exist in SAM");
    TEST_ASSERT(guestUser->rid == sam::DOMAIN_USER_RID_GUEST, "Guest RID must be 501");
    TEST_ASSERT((guestUser->userFlags & sam::USER_ACCOUNT_DISABLED) != 0, "Guest account must be disabled by default");

    auto micaUser = samDb.getUser(L"admin");
    TEST_ASSERT(micaUser.has_value(), "Default interactive admin user must exist in SAM");
    TEST_ASSERT(micaUser->rid == 1000, "Default user RID must be 1000");

    // Test NT-Hash (MD4 of little-endian UTF-16 password)
    auto ntHashAdmin = sam::crypto::computeNtHash(L"AdminPassword123!");
    TEST_ASSERT(ntHashAdmin.size() == 16, "NT-Hash must be exactly 16 bytes");
    TEST_ASSERT(ntHashAdmin == adminUser->ntHash, "Administrator NT-Hash must match computed MD4 UTF-16 hash");

    // Test user creation
    NTSTATUS stCreate = samDb.createUser(L"testdeveloper", L"Pass@word2026!", L"Test Developer", L"Software Engineer", true);
    TEST_ASSERT(stCreate == STATUS_SUCCESS, "Creating new user account must succeed");

    // Duplicate creation must fail with STATUS_USER_EXISTS
    NTSTATUS stDup = samDb.createUser(L"testdeveloper", L"AnotherPass!");
    TEST_ASSERT(stDup == STATUS_USER_EXISTS, "Creating duplicate account must return STATUS_USER_EXISTS");

    // Verify credentials
    uint32_t outRid = 0;
    NTSTATUS stAuth = samDb.verifyCredentials(L"testdeveloper", L"Pass@word2026!", outRid);
    TEST_ASSERT(stAuth == STATUS_SUCCESS, "Verifying valid credentials must succeed");
    TEST_ASSERT(outRid >= 1001, "New user RID must be allocated dynamically above 1000");

    // Test bad credentials and account lockout
    NTSTATUS stBad = samDb.verifyCredentials(L"testdeveloper", L"WrongPassword1", outRid);
    TEST_ASSERT(stBad == STATUS_LOGON_FAILURE, "Verifying wrong password must return STATUS_LOGON_FAILURE");
    samDb.verifyCredentials(L"testdeveloper", L"WrongPassword2", outRid);
    samDb.verifyCredentials(L"testdeveloper", L"WrongPassword3", outRid);
    samDb.verifyCredentials(L"testdeveloper", L"WrongPassword4", outRid);
    samDb.verifyCredentials(L"testdeveloper", L"WrongPassword5", outRid);

    NTSTATUS stLocked = samDb.verifyCredentials(L"testdeveloper", L"Pass@word2026!", outRid);
    TEST_ASSERT(stLocked == STATUS_ACCOUNT_LOCKED_OUT, "Account must be locked out after 5 consecutive bad passwords");

    // Unlock account
    samDb.unlockUser(L"testdeveloper");
    NTSTATUS stUnlocked = samDb.verifyCredentials(L"testdeveloper", L"Pass@word2026!", outRid);
    TEST_ASSERT(stUnlocked == STATUS_SUCCESS, "Verifying credentials after unlock must succeed");

    // Test group membership retrieval
    auto groups = samDb.getGroupSidsForUser(outRid);
    TEST_ASSERT(!groups.empty(), "User must belong to group SIDs");
    bool hasEveryone = std::any_of(groups.begin(), groups.end(), [](const se::Sid& s) { return s == se::Sid::everyone(); });
    bool hasAdmins = std::any_of(groups.begin(), groups.end(), [](const se::Sid& s) { return s == se::Sid::administrators(); });
    TEST_ASSERT(hasEveryone, "Group SIDs must include S-1-1-0 Everyone");
    TEST_ASSERT(hasAdmins, "Admin user must belong to Builtin Administrators");

    // Clean up test user
    NTSTATUS stDel = samDb.deleteUser(L"testdeveloper");
    TEST_ASSERT(stDel == STATUS_SUCCESS, "Deleting user account must succeed");
    TEST_ASSERT(!samDb.getUser(L"testdeveloper").has_value(), "Deleted user must no longer exist in SAM");

    // Built-in Administrator cannot be deleted
    NTSTATUS stDelAdmin = samDb.deleteUser(L"Administrator");
    TEST_ASSERT(stDelAdmin == STATUS_ACCESS_DENIED, "Attempting to delete built-in Administrator must return STATUS_ACCESS_DENIED");

    // ========================================================================
    // 2. LSASS Subsystem & Authentication Packages
    // ========================================================================
    auto& lsa = lsass::LocalSecurityAuthority::get();

    // Verify MSV1_0 package registration
    auto msvPkg = lsa.getAuthenticationPackage(L"MSV1_0");
    TEST_ASSERT(msvPkg != nullptr, "MSV1_0 authentication package must be registered with LSASS");
    TEST_ASSERT(msvPkg->getPackageName() == L"MSV1_0", "Package name must match MSV1_0");

    // Verify LSASS IPC status
    TEST_ASSERT(lsa.isRpcServerRunning(), "LSASS RPC server must be operational");
    TEST_ASSERT(lsa.getPipeName() == L"\\\\.\\pipe\\lsass", "LSASS named pipe must be \\\\.\\pipe\\lsass");
    TEST_ASSERT(lsa.getAlpcPortName() == L"\\LsaAuthenticationPort", "LSASS ALPC port must be \\LsaAuthenticationPort");

    // Interactive logon via LSASS
    std::shared_ptr<se::TokenObject> adminToken;
    Luid adminLogonId{0, 0};
    NTSTATUS stLsaLogon = lsa.logonUser(
        L"MICANT",
        L"admin",
        L"mica",
        lsass::SecurityLogonType::Interactive,
        L"MSV1_0",
        adminToken,
        adminLogonId
    );
    TEST_ASSERT(stLsaLogon == STATUS_SUCCESS, "LSASS logonUser for admin must succeed");
    TEST_ASSERT(adminToken != nullptr, "Logon token must be generated");
    TEST_ASSERT(adminLogonId.toUint64() != 0, "Logon session LUID must be nonzero");
    TEST_ASSERT(adminToken->getAuthenticationId() == adminLogonId, "Token AuthenticationId must match session LUID");
    TEST_ASSERT(adminToken->getSessionId() == 1, "Interactive logon token session ID must be 1");

    // Verify privileges in token
    TEST_ASSERT(adminToken->hasPrivilege(se::SE_DEBUG_NAME), "Admin token must possess SeDebugPrivilege");
    TEST_ASSERT(adminToken->hasPrivilege(se::SE_SHUTDOWN_NAME), "Admin token must possess SeShutdownPrivilege");
    TEST_ASSERT(adminToken->hasPrivilege(se::SE_TCB_NAME), "Admin token must possess SeTcbPrivilege");

    // Test failed logon
    std::shared_ptr<se::TokenObject> failToken;
    Luid failLuid{0, 0};
    NTSTATUS stLsaFail = lsa.logonUser(
        L"MICANT",
        L"admin",
        L"IncorrectPassword!",
        lsass::SecurityLogonType::Interactive,
        L"MSV1_0",
        failToken,
        failLuid
    );
    TEST_ASSERT(stLsaFail == STATUS_LOGON_FAILURE, "LSASS logon with wrong password must fail");

    // Test NTLM challenge-response verification
    std::vector<uint8_t> serverChallenge = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    auto adminNtHash = micaUser->ntHash;
    auto clientResponse = lsass::crypto::computeChallengeResponse(adminNtHash, serverChallenge);
    TEST_ASSERT(clientResponse.size() == 16, "NTLM challenge response digest must be 16 bytes");

    std::shared_ptr<se::TokenObject> crToken;
    Luid crLogonId{0, 0};
    NTSTATUS stCrLogon = lsa.logonUserWithChallenge(
        L"MICANT",
        L"admin",
        serverChallenge,
        clientResponse,
        lsass::SecurityLogonType::Network,
        L"MSV1_0",
        crToken,
        crLogonId
    );
    TEST_ASSERT(stCrLogon == STATUS_SUCCESS, "LSASS NTLM challenge-response logon must succeed");
    TEST_ASSERT(crToken != nullptr, "Challenge-response token must be created");

    // Enumerate logon sessions
    auto sessions = lsa.enumerateLogonSessions();
    TEST_ASSERT(sessions.size() >= 2, "Must have at least System and interactive logon sessions");

    auto sessionData = lsa.getLogonSessionData(adminLogonId);
    TEST_ASSERT(sessionData.has_value(), "Must find logon session data for admin LUID");
    TEST_ASSERT(sessionData->userName == L"admin", "Logon session username must match admin");
    TEST_ASSERT(sessionData->domainName == L"MICANT", "Logon session domain must match MICANT");
    TEST_ASSERT(sessionData->logonType == lsass::SecurityLogonType::Interactive, "Logon type must be Interactive");

    // Test SID and Name Resolution
    std::wstring outName, outDomain;
    bool sidFound = lsa.lookupAccountSid(se::Sid::localSystem(), outName, outDomain);
    TEST_ASSERT(sidFound && outName == L"SYSTEM" && outDomain == L"NT AUTHORITY", "S-1-5-18 must resolve to NT AUTHORITY\\SYSTEM");

    sidFound = lsa.lookupAccountSid(se::Sid::administrators(), outName, outDomain);
    TEST_ASSERT(sidFound && outName == L"Administrators" && outDomain == L"BUILTIN", "S-1-5-32-544 must resolve to BUILTIN\\Administrators");

    se::Sid outSid;
    bool nameFound = lsa.lookupAccountName(L"admin", outSid, outDomain);
    TEST_ASSERT(nameFound && outDomain == L"MICANT", "admin account name must resolve to MICANT domain");

    // Clean up network logon session
    lsa.logoffUser(crLogonId);
    TEST_ASSERT(!lsa.getLogonSessionData(crLogonId).has_value(), "Logged off session must be removed");

    // ========================================================================
    // 3. Winlogon Subsystem & Desktop Isolation
    // ========================================================================
    auto& winlogon = winlogon::WinlogonManager::get();

    // Reset to clean state for test
    winlogon.logoff();
    TEST_ASSERT(winlogon.getState() == winlogon::LogonState::LoggedOff, "Winlogon state after logoff must be LoggedOff");
    TEST_ASSERT(winlogon.getActiveDesktop() == winlogon::DesktopType::Winlogon, "LoggedOff state must present secure Winlogon desktop");

    // SAS in LoggedOff triggers LogonPrompt
    auto sasAction = winlogon.triggerSas();
    TEST_ASSERT(sasAction == winlogon::SasAction::LogonPrompt, "SAS when logged off must trigger LogonPrompt");

    // Interactive Logon
    NTSTATUS stWLogon = winlogon.initiateLogon(L"admin", L"mica");
    TEST_ASSERT(stWLogon == STATUS_SUCCESS, "Winlogon initiateLogon must succeed for valid credentials");
    TEST_ASSERT(winlogon.getState() == winlogon::LogonState::LoggedOn, "Winlogon state must transition to LoggedOn");
    TEST_ASSERT(winlogon.getActiveDesktop() == winlogon::DesktopType::Default, "Active desktop must switch to Default for user shell");
    TEST_ASSERT(winlogon.getLoggedOnUser() == L"admin", "Logged-on user must be admin");
    TEST_ASSERT(winlogon.getShellPid() != 0, "User shell PID must be assigned");

    // Lock Workstation
    bool locked = winlogon.lockWorkstation();
    TEST_ASSERT(locked, "Winlogon lockWorkstation must succeed");
    TEST_ASSERT(winlogon.getState() == winlogon::LogonState::Locked, "State must transition to Locked");
    TEST_ASSERT(winlogon.getActiveDesktop() == winlogon::DesktopType::Winlogon, "Locked state must switch desktop to secure Winlogon desktop");

    // SAS in Locked triggers UnlockPrompt
    sasAction = winlogon.triggerSas();
    TEST_ASSERT(sasAction == winlogon::SasAction::UnlockPrompt, "SAS when locked must trigger UnlockPrompt");

    // Attempt unlock with wrong password
    NTSTATUS stBadUnlock = winlogon.unlockWorkstation(L"WrongPassword");
    TEST_ASSERT(stBadUnlock == STATUS_LOGON_FAILURE, "Unlocking with wrong password must return STATUS_LOGON_FAILURE");
    TEST_ASSERT(winlogon.getState() == winlogon::LogonState::Locked, "State must remain Locked after failed unlock");

    // Unlock with valid password
    NTSTATUS stGoodUnlock = winlogon.unlockWorkstation(L"mica");
    TEST_ASSERT(stGoodUnlock == STATUS_SUCCESS, "Unlocking with valid password must succeed");
    TEST_ASSERT(winlogon.getState() == winlogon::LogonState::LoggedOn, "State must transition back to LoggedOn");
    TEST_ASSERT(winlogon.getActiveDesktop() == winlogon::DesktopType::Default, "Active desktop must restore to Default");

    // SAS in LoggedOn triggers SecurityOptions
    sasAction = winlogon.triggerSas();
    TEST_ASSERT(sasAction == winlogon::SasAction::SecurityOptions, "SAS when logged on must trigger SecurityOptions");
    TEST_ASSERT(winlogon.getActiveDesktop() == winlogon::DesktopType::Winlogon, "SecurityOptions must switch to Winlogon desktop");

    winlogon.dismissSecurityOptions();
    TEST_ASSERT(winlogon.getActiveDesktop() == winlogon::DesktopType::Default, "Dismissing security options must restore Default desktop");

    // ========================================================================
    // 4. Win32 advapi32.dll & user32.dll API Integration
    // ========================================================================
    win32::HANDLE hLogonToken = nullptr;
    win32::BOOL bLogon = advapi32::LogonUserW(
        L"admin",
        L"MICANT",
        L"mica",
        2, // LOGON32_LOGON_INTERACTIVE
        0,
        &hLogonToken
    );
    TEST_ASSERT(bLogon == win32::TRUE, "advapi32::LogonUserW must return TRUE for valid credentials");
    TEST_ASSERT(hLogonToken != nullptr, "advapi32::LogonUserW must return valid token handle");

    // Test LookupAccountSidW
    wchar_t acctName[64]{};
    uint32_t cchAcct = 64;
    wchar_t domName[64]{};
    uint32_t cchDom = 64;
    uint32_t sidUse = 0;
    se::Sid adminSid = se::Sid::administrators();
    win32::BOOL bLookupSid = advapi32::LookupAccountSidW(
        nullptr,
        &adminSid,
        acctName,
        &cchAcct,
        domName,
        &cchDom,
        &sidUse
    );
    TEST_ASSERT(bLookupSid == win32::TRUE, "advapi32::LookupAccountSidW must return TRUE");
    TEST_ASSERT(wcscmp(acctName, L"Administrators") == 0, "Account name must match Administrators");
    TEST_ASSERT(wcscmp(domName, L"BUILTIN") == 0, "Domain name must match BUILTIN");

    // Test LookupPrivilegeValueW
    Luid privLuid{0, 0};
    win32::BOOL bPrivVal = advapi32::LookupPrivilegeValueW(nullptr, L"SeDebugPrivilege", &privLuid);
    TEST_ASSERT(bPrivVal == win32::TRUE, "advapi32::LookupPrivilegeValueW must succeed for SeDebugPrivilege");
    TEST_ASSERT(privLuid.lowPart == 20, "SeDebugPrivilege LUID lowPart must be 20");

    wchar_t privName[64]{};
    uint32_t cchPriv = 64;
    win32::BOOL bPrivName = advapi32::LookupPrivilegeNameW(nullptr, &privLuid, privName, &cchPriv);
    TEST_ASSERT(bPrivName == win32::TRUE, "advapi32::LookupPrivilegeNameW must succeed for LUID 20");
    TEST_ASSERT(wcscmp(privName, L"SeDebugPrivilege") == 0, "Resolved privilege name must match SeDebugPrivilege");

    // Test user32 LockWorkStation
    win32::BOOL bLock = user32::LockWorkStation();
    TEST_ASSERT(bLock == win32::TRUE, "user32::LockWorkStation must return TRUE");
    TEST_ASSERT(winlogon.getState() == winlogon::LogonState::Locked, "Workstation must be locked after user32::LockWorkStation");
    winlogon.unlockWorkstation(L"mica");

    // ========================================================================
    // 5. Command Shell Security Commands Integration
    // ========================================================================
    shell::CommandShell testShell;

    // Test 'whoami'
    std::ostringstream ssWhoami;
    int rcWhoami = testShell.execute("whoami", ssWhoami);
    TEST_ASSERT(rcWhoami == 0, "Executing 'whoami' must succeed");
    TEST_ASSERT(ssWhoami.str().find("MICANT\\admin") != std::string::npos, "whoami output must contain MICANT\\admin");

    // Test 'whoami /user'
    std::ostringstream ssWhoamiUser;
    testShell.execute("whoami /user", ssWhoamiUser);
    TEST_ASSERT(ssWhoamiUser.str().find("USER INFORMATION") != std::string::npos, "whoami /user must output header");
    TEST_ASSERT(ssWhoamiUser.str().find("S-1-5-21-") != std::string::npos, "whoami /user must output SID");

    // Test 'whoami /groups'
    std::ostringstream ssWhoamiGroups;
    testShell.execute("whoami /groups", ssWhoamiGroups);
    TEST_ASSERT(ssWhoamiGroups.str().find("GROUP INFORMATION") != std::string::npos, "whoami /groups must output header");
    TEST_ASSERT(ssWhoamiGroups.str().find("BUILTIN\\Administrators") != std::string::npos, "whoami /groups must include Administrators");

    // Test 'whoami /priv'
    std::ostringstream ssWhoamiPriv;
    testShell.execute("whoami /priv", ssWhoamiPriv);
    TEST_ASSERT(ssWhoamiPriv.str().find("PRIVILEGES INFORMATION") != std::string::npos, "whoami /priv must output header");
    TEST_ASSERT(ssWhoamiPriv.str().find("SeDebugPrivilege") != std::string::npos, "whoami /priv must include SeDebugPrivilege");
    TEST_ASSERT(ssWhoamiPriv.str().find("SeShutdownPrivilege") != std::string::npos, "whoami /priv must include SeShutdownPrivilege");

    // Test 'net user'
    std::ostringstream ssNetUser;
    testShell.execute("net user", ssNetUser);
    TEST_ASSERT(ssNetUser.str().find("User accounts for \\\\MICANT-DESKTOP") != std::string::npos, "net user must list accounts");
    TEST_ASSERT(ssNetUser.str().find("Administrator") != std::string::npos, "net user must include Administrator");
    TEST_ASSERT(ssNetUser.str().find("admin") != std::string::npos, "net user must include admin");

    // Test 'net user admin' (detailed user info)
    std::ostringstream ssNetUserAdmin;
    testShell.execute("net user admin", ssNetUserAdmin);
    TEST_ASSERT(ssNetUserAdmin.str().find("User name                    admin") != std::string::npos, "net user admin must display username");
    TEST_ASSERT(ssNetUserAdmin.str().find("Account active               Yes") != std::string::npos, "net user admin must show account active");
    TEST_ASSERT(ssNetUserAdmin.str().find("*Administrators") != std::string::npos, "net user admin must show Administrators group");

    // Test 'net user alice Password123! /add'
    std::ostringstream ssAddUser;
    testShell.execute("net user alice Password123! /add", ssAddUser);
    TEST_ASSERT(ssAddUser.str().find("The command completed successfully") != std::string::npos, "net user /add must succeed");
    TEST_ASSERT(samDb.getUser(L"alice").has_value(), "User 'alice' must exist in SAM after /add");

    // Test 'net user alice /delete'
    std::ostringstream ssDelUser;
    testShell.execute("net user alice /delete", ssDelUser);
    TEST_ASSERT(ssDelUser.str().find("The command completed successfully") != std::string::npos, "net user /delete must succeed");
    TEST_ASSERT(!samDb.getUser(L"alice").has_value(), "User 'alice' must be removed from SAM after /delete");

    // Test 'lock'
    std::ostringstream ssLock;
    testShell.execute("lock", ssLock);
    TEST_ASSERT(ssLock.str().find("The workstation is now locked") != std::string::npos, "lock command must notify workstation locked");
    TEST_ASSERT(winlogon.getState() == winlogon::LogonState::Locked, "Workstation must be locked after 'lock'");

    // Unlock and test 'logoff'
    winlogon.unlockWorkstation(L"mica");
    std::ostringstream ssLogoff;
    testShell.execute("logoff", ssLogoff);
    TEST_ASSERT(ssLogoff.str().find("Session terminated. User logged off") != std::string::npos, "logoff command must notify user logged off");
    TEST_ASSERT(winlogon.getState() == winlogon::LogonState::LoggedOff, "State must be LoggedOff after 'logoff'");
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
    RUN_TEST(Test_MsvcrtBridge_And_CommandShell);
    RUN_TEST(Test_StorageAndFat32FileSystem);
    RUN_TEST(Test_NdisAndTcpIpNetworkStack);
    RUN_TEST(Test_Arm64HardwareArchitectureAndSyscall);
    RUN_TEST(Test_NamedPipesAndMailslotsIpc);
    RUN_TEST(Test_NtfsFileSystemAndMasterFileTable);
    RUN_TEST(Test_ServiceControlManager_And_SvcHost);
    RUN_TEST(Test_Lsass_Winlogon_And_SamDatabase);

    std::cout << "\n------------------------------------------------------------------------\n";
    std::cout << "Summary: " << g_PassedTests << " Passed, " << g_FailedTests << " Failed\n";
    std::cout << "------------------------------------------------------------------------\n";

    return (g_FailedTests == 0) ? 0 : 1;
}

