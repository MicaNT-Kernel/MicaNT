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
#include <sstream>
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
#include "micant/prismx.hpp"
#include "micant/prism3d.hpp"
#include "micant/prism3d12.hpp"
#include "micant/prism_shader_vm.hpp"
#include "micant/dxgkrnl.hpp"
#include "micant/vulkan.hpp"
#include "micant/emeraldfs.hpp"
#include "micant/daytonamm.hpp"
#include "micant/d3dcompiler.hpp"
#include "micant/prismaudio.hpp"
#include "micant/xinput.hpp"
#include "micant/vanguarddriver.hpp"
#include "micant/aegissandbox.hpp"
#include "micant/polarisdiag.hpp"
#include "micant/cipherksp.hpp"
#include "micant/janusldr.hpp"
#include "micant/dinput.hpp"
#include "micant/prism_viewer.hpp"
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

// ============================================================================
// Suite 45: PrismX & Prism3D Graphics Subsystem (DXGI, Direct3D, WDDM D3DKMT)
// ============================================================================
void Test_PrismX_And_Prism3D_GraphicsSubsystem() {
    using namespace micant::prismx;
    using namespace micant::prism3d;
    using namespace micant::dxgkrnl;

    // ------------------------------------------------------------------------
    // 1. DXGI Factory, Adapter, and Display Output Enumeration
    // ------------------------------------------------------------------------
    IDXGIFactory1* factory = nullptr;
    int32_t hrFactory = CreateDXGIFactory1(IID_IDXGIFactory1, reinterpret_cast<void**>(&factory));
    TEST_ASSERT(hrFactory == 0 && factory != nullptr, "CreateDXGIFactory1 must succeed and return factory");

    IDXGIAdapter1* adapter0 = nullptr;
    int32_t hrAdapter = factory->EnumAdapters1(0, &adapter0);
    TEST_ASSERT(hrAdapter == 0 && adapter0 != nullptr, "EnumAdapters1 for Adapter 0 must succeed");

    DXGI_ADAPTER_DESC1 desc0{};
    adapter0->GetDesc1(&desc0);
    std::wstring wDesc0(desc0.Description);
    TEST_ASSERT(wDesc0.find(L"PRISM") != std::wstring::npos, "Adapter 0 description must indicate Prism hardware");
    TEST_ASSERT(desc0.VendorId == 0x1414, "VendorId must match 0x1414");
    TEST_ASSERT(desc0.DedicatedVideoMemory == 4ULL * 1024 * 1024 * 1024, "Adapter 0 must have 4GB Dedicated VRAM");

    IDXGIOutput* output0 = nullptr;
    int32_t hrOutput = adapter0->EnumOutputs(0, &output0);
    TEST_ASSERT(hrOutput == 0 && output0 != nullptr, "Adapter 0 must have at least one connected display output");

    DXGI_OUTPUT_DESC oDesc0{};
    output0->GetDesc(&oDesc0);
    TEST_ASSERT((oDesc0.DesktopCoordinates.right - oDesc0.DesktopCoordinates.left) == 1920, "Output desktop width must be 1920");
    TEST_ASSERT((oDesc0.DesktopCoordinates.bottom - oDesc0.DesktopCoordinates.top) == 1080, "Output desktop height must be 1080");

    uint32_t numModes = 0;
    output0->GetDisplayModeList(DXGI_FORMAT_B8G8R8A8_UNORM, 0, &numModes, nullptr);
    TEST_ASSERT(numModes == 3, "Display mode list must contain 3 standard modes");

    // ------------------------------------------------------------------------
    // 2. Direct3D 11 Device & Presentation SwapChain Creation
    // ------------------------------------------------------------------------
    DXGI_SWAP_CHAIN_DESC scDesc{};
    scDesc.BufferDesc.Width = 640;
    scDesc.BufferDesc.Height = 480;
    scDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scDesc.BufferCount = 2;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain* swapChain = nullptr;
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_9_1;

    int32_t hrDev = D3D11CreateDeviceAndSwapChain(
        adapter0,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        nullptr,
        0,
        7,
        &scDesc,
        &swapChain,
        &device,
        &featureLevel,
        &context
    );
    TEST_ASSERT(hrDev == 0, "D3D11CreateDeviceAndSwapChain must succeed");
    TEST_ASSERT(device != nullptr && context != nullptr && swapChain != nullptr, "Device, Context, and SwapChain must be valid");
    TEST_ASSERT(featureLevel == D3D_FEATURE_LEVEL_11_0, "Feature level must default to D3D_FEATURE_LEVEL_11_0");

    // Test back-buffer surface access
    IDXGISurface* backBuffer = nullptr;
    int32_t hrBuf = swapChain->GetBuffer(0, IID_IDXGISurface, reinterpret_cast<void**>(&backBuffer));
    TEST_ASSERT(hrBuf == 0 && backBuffer != nullptr, "GetBuffer(0) must return valid IDXGISurface");

    DXGI_MODE_DESC surfDesc{};
    backBuffer->GetDesc(&surfDesc);
    TEST_ASSERT(surfDesc.Width == 640 && surfDesc.Height == 480, "Surface dimensions must match 640x480");

    // ------------------------------------------------------------------------
    // 3. Render Target View, Viewport, and Clear Operations
    // ------------------------------------------------------------------------
    ID3D11RenderTargetView* rtv = nullptr;
    int32_t hrRtv = device->CreateRenderTargetView(reinterpret_cast<ID3D11Resource*>(backBuffer), nullptr, &rtv);
    TEST_ASSERT(hrRtv == 0 && rtv != nullptr, "CreateRenderTargetView must succeed");

    D3D11_VIEWPORT vp{ 0.0f, 0.0f, 640.0f, 480.0f, 0.0f, 1.0f };
    context->RSSetViewports(1, &vp);
    context->OMSetRenderTargets(1, &rtv, nullptr);

    // Clear render target to solid dark blue: R=0.1, G=0.2, B=0.8, A=1.0
    const float clearColor[4] = { 0.1f, 0.2f, 0.8f, 1.0f };
    context->ClearRenderTargetView(rtv, clearColor);

    auto* surfImpl = static_cast<PrismXSurfaceImpl*>(backBuffer);
    const uint32_t* rawPixels = reinterpret_cast<const uint32_t*>(surfImpl->GetRawData());
    TEST_ASSERT(rawPixels[0] != 0, "First pixel of back-buffer must not be 0 after clear");

    // Check BGRA clear values
    uint8_t expectedB = static_cast<uint8_t>(0.8f * 255.0f);
    uint8_t expectedG = static_cast<uint8_t>(0.2f * 255.0f);
    uint8_t expectedR = static_cast<uint8_t>(0.1f * 255.0f);
    uint8_t expectedA = 255;
    uint32_t expectedPixel = (expectedA << 24) | (expectedR << 16) | (expectedG << 8) | expectedB;
    TEST_ASSERT(rawPixels[0] == expectedPixel, "Pixel value after clear must match expected packed 32-bpp BGRA");

    // ------------------------------------------------------------------------
    // 4. Prism3D Vertex Buffer Creation & Barycentric Triangle Rasterization
    // ------------------------------------------------------------------------
    VertexPositionColor triVertices[3] = {
        {  0.0f,  0.5f, 0.0f,  1.0f, 0.0f, 0.0f, 1.0f }, // Top (Pure Red)
        { -0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f, 1.0f }, // Bottom-Left (Pure Green)
        {  0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f, 1.0f }  // Bottom-Right (Pure Blue)
    };

    D3D11_BUFFER_DESC vbDesc{};
    vbDesc.ByteWidth = sizeof(triVertices);
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.StructureByteStride = sizeof(VertexPositionColor);

    D3D11_SUBRESOURCE_DATA vbData{};
    vbData.pSysMem = triVertices;

    ID3D11Buffer* vertexBuffer = nullptr;
    int32_t hrVb = device->CreateBuffer(&vbDesc, &vbData, &vertexBuffer);
    TEST_ASSERT(hrVb == 0 && vertexBuffer != nullptr, "CreateBuffer for vertex data must succeed");

    uint32_t stride = sizeof(VertexPositionColor);
    uint32_t offset = 0;
    context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Rasterize triangle to back-buffer surface
    context->Draw(3, 0);

    // The center of the triangle in screen coords is (320, 240)
    // In NDC: (0, 0) -> Screen: (320, 240).
    // The center should be shaded with interpolated colors (approx R=0.33, G=0.33, B=0.33)
    // and definitely different from clearColor!
    size_t centerPixelIdx = 240ULL * 640 + 320;
    uint32_t centerPixel = rawPixels[centerPixelIdx];
    TEST_ASSERT(centerPixel != expectedPixel, "Center pixel inside triangle must have been shaded by rasterizer");

    // Present the frame
    int32_t hrPresent = swapChain->Present(1, 0);
    TEST_ASSERT(hrPresent == 0, "Present must succeed");

    uint32_t lastPresentCount = 0;
    swapChain->GetLastPresentCount(&lastPresentCount);
    TEST_ASSERT(lastPresentCount == 1, "Swapchain present count must be 1");

    // ------------------------------------------------------------------------
    // 5. 3D Mathematics, Matrix Pipeline & Vector Operations
    // ------------------------------------------------------------------------
    Matrix4x4 id = Matrix4x4::Identity();
    Vector4 v0{ 1.0f, 2.0f, 3.0f, 1.0f };
    Vector4 v0T = id.Transform(v0);
    TEST_ASSERT(v0T.x == 1.0f && v0T.y == 2.0f && v0T.z == 3.0f && v0T.w == 1.0f, "Identity matrix transform must preserve vector");

    Matrix4x4 trans = Matrix4x4::Translation(10.0f, -5.0f, 20.0f);
    Vector4 vOrigin{ 0.0f, 0.0f, 0.0f, 1.0f };
    Vector4 vTrans = trans.Transform(vOrigin);
    TEST_ASSERT(vTrans.x == 10.0f && vTrans.y == -5.0f && vTrans.z == 20.0f && vTrans.w == 1.0f, "Translation matrix must translate origin");

    Matrix4x4 rotX = Matrix4x4::RotationX(3.14159265f / 2.0f);
    Vector4 vUp{ 0.0f, 1.0f, 0.0f, 1.0f };
    Vector4 vRotX = rotX.Transform(vUp);
    TEST_ASSERT(std::abs(vRotX.x) < 1e-4f && std::abs(vRotX.y) < 1e-4f && std::abs(vRotX.z - 1.0f) < 1e-4f, "RotationX by 90deg must rotate +Y to +Z");

    Matrix4x4 rotY = Matrix4x4::RotationY(3.14159265f / 2.0f);
    Vector4 vForward{ 0.0f, 0.0f, 1.0f, 1.0f };
    Vector4 vRotY = rotY.Transform(vForward);
    TEST_ASSERT(std::abs(vRotY.x - 1.0f) < 1e-4f && std::abs(vRotY.y) < 1e-4f, "RotationY by 90deg must rotate +Z to +X in LH coordinates");

    Matrix4x4 proj = Matrix4x4::PerspectiveFovLH(1.047f, 640.0f / 480.0f, 0.1f, 100.0f);
    TEST_ASSERT(proj.m[2][3] == 1.0f, "LH perspective projection m[2][3] must be 1.0f for w-buffering");
    TEST_ASSERT(proj.m[0][0] > 0.0f && proj.m[1][1] > 0.0f, "Focal lengths must be positive");

    Matrix4x4 view = Matrix4x4::LookAtLH(Vector3{ 0.0f, 0.0f, -5.0f }, Vector3{ 0.0f, 0.0f, 0.0f }, Vector3{ 0.0f, 1.0f, 0.0f });
    TEST_ASSERT(std::abs(view.m[3][2] - 5.0f) < 1e-4f, "LookAtLH eye at -5z must translate z by +5");

    // ------------------------------------------------------------------------
    // 6. Direct3D 11 Pipeline State Objects & Resource Views
    // ------------------------------------------------------------------------
    // Rasterizer States (Solid & Wireframe)
    D3D11_RASTERIZER_DESC rsSolidDesc{};
    rsSolidDesc.FillMode = D3D11_FILL_SOLID;
    rsSolidDesc.CullMode = D3D11_CULL_BACK;
    ID3D11RasterizerState* rsSolid = nullptr;
    int32_t hrRsSolid = device->CreateRasterizerState(&rsSolidDesc, &rsSolid);
    TEST_ASSERT(hrRsSolid == 0 && rsSolid != nullptr, "CreateRasterizerState (Solid) must succeed");

    D3D11_RASTERIZER_DESC rsWireDesc{};
    rsWireDesc.FillMode = D3D11_FILL_WIREFRAME;
    rsWireDesc.CullMode = D3D11_CULL_NONE;
    ID3D11RasterizerState* rsWire = nullptr;
    int32_t hrRsWire = device->CreateRasterizerState(&rsWireDesc, &rsWire);
    TEST_ASSERT(hrRsWire == 0 && rsWire != nullptr, "CreateRasterizerState (Wireframe) must succeed");

    context->RSSetState(rsSolid);

    // Depth Stencil View & State
    ID3D11DepthStencilView* dsv = nullptr;
    int32_t hrDsv = device->CreateDepthStencilView(nullptr, nullptr, &dsv);
    TEST_ASSERT(hrDsv == 0 && dsv != nullptr, "CreateDepthStencilView must succeed");

    D3D11_DEPTH_STENCIL_DESC dsDesc{};
    dsDesc.DepthEnable = 1;
    dsDesc.DepthWriteMask = 1;
    dsDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    ID3D11DepthStencilState* dsState = nullptr;
    int32_t hrDsState = device->CreateDepthStencilState(&dsDesc, &dsState);
    TEST_ASSERT(hrDsState == 0 && dsState != nullptr, "CreateDepthStencilState must succeed");
    context->OMSetDepthStencilState(dsState, 0);

    // Blend State
    D3D11_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].BlendEnable = 1;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = 0x0F;
    ID3D11BlendState* blendState = nullptr;
    int32_t hrBlend = device->CreateBlendState(&blendDesc, &blendState);
    TEST_ASSERT(hrBlend == 0 && blendState != nullptr, "CreateBlendState must succeed");
    float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    context->OMSetBlendState(blendState, blendFactor, 0xFFFFFFFF);

    // Texture 2D, SRV & Sampler State
    D3D11_TEXTURE2D_DESC texDesc{};
    texDesc.Width = 4;
    texDesc.Height = 4;
    texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    uint32_t checkerPixels[16];
    for (int i = 0; i < 16; ++i) checkerPixels[i] = ((i % 2) == 0) ? 0xFFFFFFFF : 0xFF000000;
    D3D11_SUBRESOURCE_DATA texInit{};
    texInit.pSysMem = checkerPixels;
    texInit.SysMemPitch = 4 * sizeof(uint32_t);
    ID3D11Texture2D* texture2D = nullptr;
    int32_t hrTex = device->CreateTexture2D(&texDesc, &texInit, &texture2D);
    TEST_ASSERT(hrTex == 0 && texture2D != nullptr, "CreateTexture2D must succeed");

    ID3D11ShaderResourceView* srv = nullptr;
    int32_t hrSrv = device->CreateShaderResourceView(texture2D, nullptr, &srv);
    TEST_ASSERT(hrSrv == 0 && srv != nullptr, "CreateShaderResourceView must succeed");
    context->PSSetShaderResources(0, 1, &srv);

    D3D11_SAMPLER_DESC sampDesc{};
    ID3D11SamplerState* samplerState = nullptr;
    int32_t hrSamp = device->CreateSamplerState(&sampDesc, &samplerState);
    TEST_ASSERT(hrSamp == 0 && samplerState != nullptr, "CreateSamplerState must succeed");
    context->PSSetSamplers(0, 1, &samplerState);

    // ------------------------------------------------------------------------
    // 7. Indexed Drawing (DrawIndexed), Constant Buffers, and 3D Cube Test
    // ------------------------------------------------------------------------
    VertexPositionColor cubeVerts[8] = {
        { -1.0f, -1.0f, -1.0f,  1.0f, 0.0f, 0.0f, 1.0f }, // 0: Red
        { -1.0f,  1.0f, -1.0f,  0.0f, 1.0f, 0.0f, 1.0f }, // 1: Green
        {  1.0f,  1.0f, -1.0f,  0.0f, 0.0f, 1.0f, 1.0f }, // 2: Blue
        {  1.0f, -1.0f, -1.0f,  1.0f, 1.0f, 0.0f, 1.0f }, // 3: Yellow
        { -1.0f, -1.0f,  1.0f,  1.0f, 0.0f, 1.0f, 1.0f }, // 4: Magenta
        { -1.0f,  1.0f,  1.0f,  0.0f, 1.0f, 1.0f, 1.0f }, // 5: Cyan
        {  1.0f,  1.0f,  1.0f,  1.0f, 1.0f, 1.0f, 1.0f }, // 6: White
        {  1.0f, -1.0f,  1.0f,  0.5f, 0.5f, 0.5f, 1.0f }  // 7: Grey
    };
    D3D11_BUFFER_DESC cubeVbDesc{};
    cubeVbDesc.ByteWidth = sizeof(cubeVerts);
    cubeVbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    cubeVbDesc.StructureByteStride = sizeof(VertexPositionColor);
    D3D11_SUBRESOURCE_DATA cubeVbInit{};
    cubeVbInit.pSysMem = cubeVerts;
    ID3D11Buffer* cubeVb = nullptr;
    device->CreateBuffer(&cubeVbDesc, &cubeVbInit, &cubeVb);
    TEST_ASSERT(cubeVb != nullptr, "CreateBuffer for 3D cube vertex buffer must succeed");

    uint16_t cubeIndices[36] = {
        0, 1, 2,  0, 2, 3,  // Front
        4, 6, 5,  4, 7, 6,  // Back
        4, 5, 1,  4, 1, 0,  // Left
        3, 2, 6,  3, 6, 7,  // Right
        1, 5, 6,  1, 6, 2,  // Top
        4, 0, 3,  4, 3, 7   // Bottom
    };
    D3D11_BUFFER_DESC cubeIbDesc{};
    cubeIbDesc.ByteWidth = sizeof(cubeIndices);
    cubeIbDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA cubeIbInit{};
    cubeIbInit.pSysMem = cubeIndices;
    ID3D11Buffer* cubeIb = nullptr;
    device->CreateBuffer(&cubeIbDesc, &cubeIbInit, &cubeIb);
    TEST_ASSERT(cubeIb != nullptr, "CreateBuffer for 3D cube index buffer must succeed");

    // Model-View-Projection Matrix
    Matrix4x4 world = Matrix4x4::Multiply(Matrix4x4::RotationX(0.5f), Matrix4x4::RotationY(0.7f));
    Matrix4x4 camView = Matrix4x4::LookAtLH(Vector3{ 0.0f, 0.0f, -3.5f }, Vector3{ 0.0f, 0.0f, 0.0f }, Vector3{ 0.0f, 1.0f, 0.0f });
    Matrix4x4 camProj = Matrix4x4::PerspectiveFovLH(1.047f, 640.0f / 480.0f, 0.1f, 100.0f);
    Matrix4x4 mvp = Matrix4x4::Multiply(world, Matrix4x4::Multiply(camView, camProj));

    D3D11_BUFFER_DESC cbDesc{};
    cbDesc.ByteWidth = sizeof(Matrix4x4);
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    D3D11_SUBRESOURCE_DATA cbInit{};
    cbInit.pSysMem = &mvp;
    ID3D11Buffer* cb = nullptr;
    device->CreateBuffer(&cbDesc, &cbInit, &cb);
    TEST_ASSERT(cb != nullptr, "CreateBuffer for MVP constant buffer must succeed");

    // Bind Render Target & Depth Stencil
    context->OMSetRenderTargets(1, &rtv, dsv);
    context->ClearRenderTargetView(rtv, clearColor);
    context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);

    uint32_t cubeStride = sizeof(VertexPositionColor);
    uint32_t cubeOffset = 0;
    context->IASetVertexBuffers(0, 1, &cubeVb, &cubeStride, &cubeOffset);
    context->IASetIndexBuffer(cubeIb, DXGI_FORMAT_R16_UINT, 0);
    context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetConstantBuffers(0, 1, &cb);
    context->RSSetState(rsSolid);

    // Render 3D Cube with DrawIndexed
    context->DrawIndexed(36, 0, 0);

    // Verify center pixel or screen area was shaded by cube
    bool shadedPixelFound = false;
    for (size_t y = 200; y < 280; ++y) {
        for (size_t x = 280; x < 360; ++x) {
            if (rawPixels[y * 640 + x] != expectedPixel) {
                shadedPixelFound = true;
                break;
            }
        }
        if (shadedPixelFound) break;
    }
    TEST_ASSERT(shadedPixelFound, "3D Cube DrawIndexed must rasterize and shade back-buffer pixels");

    // Also test wireframe DrawIndexed
    context->RSSetState(rsWire);
    context->DrawIndexed(36, 0, 0);

    swapChain->Present(1, 0);
    swapChain->GetLastPresentCount(&lastPresentCount);
    TEST_ASSERT(lastPresentCount == 2, "Swapchain present count must be 2 after cube render");

    // Cleanup resources
    cb->Release();
    cubeIb->Release();
    cubeVb->Release();
    samplerState->Release();
    srv->Release();
    texture2D->Release();
    blendState->Release();
    dsState->Release();
    dsv->Release();
    rsWire->Release();
    rsSolid->Release();

    // ------------------------------------------------------------------------
    // 8. WDDM Kernel Subsystem Syscall Validation (dxgkrnl / D3DKMT)
    // ------------------------------------------------------------------------
    D3DKMT_OPENADAPTERFROMHDC openHdc{};
    openHdc.hDc = reinterpret_cast<void*>(0x2001);
    NtStatus stOpen = NtGdiDdD3DKMTOpenAdapterFromHdc(&openHdc);
    TEST_ASSERT(NT_SUCCESS(stOpen) && openHdc.hAdapter != 0, "D3DKMTOpenAdapterFromHdc must return valid adapter handle");

    D3DKMT_CREATEDEVICE createDev{};
    createDev.hAdapter = openHdc.hAdapter;
    NtStatus stDev = NtGdiDdD3DKMTCreateDevice(&createDev);
    TEST_ASSERT(NT_SUCCESS(stDev) && createDev.hDevice != 0, "D3DKMTCreateDevice must return valid device handle");

    D3DKMT_CREATEALLOCATIONINFO allocInfo[2]{};
    D3DKMT_CREATEALLOCATION createAlloc{};
    createAlloc.hDevice = createDev.hDevice;
    createAlloc.NumAllocations = 2;
    createAlloc.pAllocationInfo = allocInfo;
    NtStatus stAlloc = NtGdiDdD3DKMTCreateAllocation(&createAlloc);
    TEST_ASSERT(NT_SUCCESS(stAlloc), "D3DKMTCreateAllocation must succeed");
    TEST_ASSERT(allocInfo[0].hAllocation != 0 && allocInfo[1].hAllocation != 0, "Allocations must have non-zero handles");

    auto& dxg = DxgkrnlSubsystem::GetInstance();
    TEST_ASSERT(dxg.GetActiveAllocationsCount() >= 2, "Active allocations count must be at least 2");

    // Test SubmitCommand and Present syscalls
    D3DKMT_SUBMITCOMMAND submitCmd{};
    submitCmd.Commands = 0x7FF000000000ULL;
    submitCmd.CommandLength = 512;
    NtStatus stSubmit = NtGdiDdD3DKMTSubmitCommand(&submitCmd);
    TEST_ASSERT(NT_SUCCESS(stSubmit), "D3DKMTSubmitCommand must succeed");
    TEST_ASSERT(dxg.GetTotalSubmissions() >= 1, "Total GPU command submissions must increment");

    D3DKMT_PRESENT presentKmt{};
    presentKmt.hDevice = createDev.hDevice;
    NtStatus stKmtPres = NtGdiDdD3DKMTPresent(&presentKmt);
    TEST_ASSERT(NT_SUCCESS(stKmtPres), "D3DKMTPresent must succeed");
    TEST_ASSERT(dxg.GetTotalPresents() >= 1, "Total compositor presents must increment");

    // Cleanup Allocations and Device
    D3DKMT_HANDLE allocList[2] = { allocInfo[0].hAllocation, allocInfo[1].hAllocation };
    D3DKMT_DESTROYALLOCATION destroyAlloc{};
    destroyAlloc.hDevice = createDev.hDevice;
    destroyAlloc.phAllocationList = allocList;
    destroyAlloc.AllocationCount = 2;
    NtStatus stDestrAlloc = NtGdiDdD3DKMTDestroyAllocation(&destroyAlloc);
    TEST_ASSERT(NT_SUCCESS(stDestrAlloc), "D3DKMTDestroyAllocation must succeed");

    D3DKMT_DESTROYDEVICE destroyDev{};
    destroyDev.hDevice = createDev.hDevice;
    NtStatus stDestrDev = NtGdiDdD3DKMTDestroyDevice(&destroyDev);
    TEST_ASSERT(NT_SUCCESS(stDestrDev), "D3DKMTDestroyDevice must succeed");

    // ------------------------------------------------------------------------
    // 9. Shell Integration Test ('prismx', 'prismx test', 'prismx cube', 'prismx wireframe')
    // ------------------------------------------------------------------------
    shell::CommandShell testShell;
    std::ostringstream ssGpu;
    testShell.execute("prismx", ssGpu);
    std::string outGpu = ssGpu.str();
    TEST_ASSERT(outGpu.find("PrismX & Prism3D Graphics Subsystem") != std::string::npos, "prismx command must display header");
    TEST_ASSERT(outGpu.find("Dedicated Video Memory:  4096 MB") != std::string::npos, "prismx command must display 4096 MB VRAM");
    TEST_ASSERT(outGpu.find("WDDM Kernel Telemetry") != std::string::npos, "prismx command must display WDDM telemetry");

    std::ostringstream ssTest;
    testShell.execute("prismx test", ssTest);
    std::string outTest = ssTest.str();
    TEST_ASSERT(outTest.find("3D Barycentric Shaded Triangle rendered successfully") != std::string::npos, "prismx test must render 3D triangle");
    TEST_ASSERT(outTest.find("Swapchain: 800x600") != std::string::npos, "prismx test must output swapchain size");

    std::ostringstream ssCube;
    testShell.execute("prismx cube", ssCube);
    std::string outCube = ssCube.str();
    TEST_ASSERT(outCube.find("3D Cube rendered successfully via DrawIndexed!") != std::string::npos, "prismx cube must render 3D cube");
    TEST_ASSERT(outCube.find("Mesh: 8 Vertices, 36 Indices (12 Triangles)") != std::string::npos, "prismx cube must verify mesh topology");
    TEST_ASSERT(outCube.find("SOLID (Barycentric Gouraud)") != std::string::npos, "prismx cube must report solid rasterizer");

    std::ostringstream ssWire;
    testShell.execute("prismx wireframe", ssWire);
    std::string outWire = ssWire.str();
    TEST_ASSERT(outWire.find("3D Cube rendered successfully via DrawIndexed!") != std::string::npos, "prismx wireframe must render wireframe cube");
    TEST_ASSERT(outWire.find("WIREFRAME (Bresenham line)") != std::string::npos, "prismx wireframe must report wireframe rasterizer");

    // Clean up COM resources
    vertexBuffer->Release();
    rtv->Release();
    backBuffer->Release();
    swapChain->Release();
    context->Release();
    device->Release();
    output0->Release();
    adapter0->Release();
    factory->Release();
}

void Test_VulkanLoader_And_PrismVK_Subsystem() {
    using namespace micant::vulkan;

    // 1. Initialize Vulkan Loader Subsystem Exports
    InitializeVulkanSubsystemExports();

    auto& ldr = ldr::DynamicLoader::get();
    TEST_ASSERT(ldr.getExport("vulkan-1.dll", "vkCreateInstance") != nullptr, "vulkan-1.dll vkCreateInstance must be exported");
    TEST_ASSERT(ldr.getExport("vulkan-1.dll", "vkEnumeratePhysicalDevices") != nullptr, "vulkan-1.dll vkEnumeratePhysicalDevices must be exported");
    TEST_ASSERT(ldr.getExport("vulkan-1.dll", "vkCreateDevice") != nullptr, "vulkan-1.dll vkCreateDevice must be exported");
    TEST_ASSERT(ldr.getExport("vulkan-1.dll", "vkCreateWin32SurfaceKHR") != nullptr, "vulkan-1.dll vkCreateWin32SurfaceKHR must be exported");
    TEST_ASSERT(ldr.getExport("vulkan-1.dll", "vkCreateSwapchainKHR") != nullptr, "vulkan-1.dll vkCreateSwapchainKHR must be exported");
    TEST_ASSERT(ldr.getExport("vulkan-1.dll", "vkQueueSubmit") != nullptr, "vulkan-1.dll vkQueueSubmit must be exported");

    // 2. Registry Driver Discovery Check
    TEST_ASSERT(VulkanLoader::get().isIcdRegistered(), "Khronos Vulkan Driver registry hive must be initialized");

    // 3. Create Vulkan Instance
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "MicaNT Vulkan Test App";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "PrismVK";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 3, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo instInfo{};
    instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instInfo.pApplicationInfo = &appInfo;

    VkInstance instance = nullptr;
    VkResult res = vkCreateInstance(&instInfo, nullptr, &instance);
    TEST_ASSERT(res == VK_SUCCESS && instance != nullptr, "vkCreateInstance must succeed");

    // 4. Enumerate Physical Devices & Properties
    uint32_t physCount = 0;
    res = vkEnumeratePhysicalDevices(instance, &physCount, nullptr);
    TEST_ASSERT(res == VK_SUCCESS && physCount >= 1, "vkEnumeratePhysicalDevices count must be at least 1");

    std::vector<VkPhysicalDevice> physDevices(physCount);
    res = vkEnumeratePhysicalDevices(instance, &physCount, physDevices.data());
    TEST_ASSERT(res == VK_SUCCESS && physDevices[0] != nullptr, "vkEnumeratePhysicalDevices must return physical device handle");

    VkPhysicalDevice phys = physDevices[0];
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(phys, &props);
    TEST_ASSERT(props.apiVersion == VK_API_VERSION_1_3, "Vulkan API version must be 1.3");
    TEST_ASSERT(props.vendorID == 0x1414, "Vendor ID must match 0x1414");
    TEST_ASSERT(props.deviceID == 0x008C, "Device ID must match 0x008C");
    TEST_ASSERT(props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Device must be Discrete GPU");

    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(phys, &memProps);
    TEST_ASSERT(memProps.memoryHeapCount >= 2, "Must have at least 2 memory heaps");
    TEST_ASSERT(memProps.memoryHeaps[0].size == 8192ULL * 1024 * 1024, "Heap 0 must have 8192 MB VRAM");

    // 5. Create Logical Device & Queues
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo qci{};
    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = 0;
    qci.queueCount = 1;
    qci.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo devInfo{};
    devInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    devInfo.queueCreateInfoCount = 1;
    devInfo.pQueueCreateInfos = &qci;

    VkDevice device = nullptr;
    res = vkCreateDevice(phys, &devInfo, nullptr, &device);
    TEST_ASSERT(res == VK_SUCCESS && device != nullptr, "vkCreateDevice must succeed");

    VkQueue queue = nullptr;
    vkGetDeviceQueue(device, 0, 0, &queue);
    TEST_ASSERT(queue != nullptr, "vkGetDeviceQueue must return valid queue");

    // 6. Win32 Surface & Swapchain
    VkWin32SurfaceCreateInfoKHR surfInfo{};
    surfInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfInfo.hwnd = reinterpret_cast<void*>(0x20002);
    surfInfo.hinstance = reinterpret_cast<void*>(0x400000);

    VkSurfaceKHR surface = 0;
    res = vkCreateWin32SurfaceKHR(instance, &surfInfo, nullptr, &surface);
    TEST_ASSERT(res == VK_SUCCESS && surface != 0, "vkCreateWin32SurfaceKHR must succeed");

    VkSurfaceCapabilitiesKHR caps{};
    res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, surface, &caps);
    TEST_ASSERT(res == VK_SUCCESS && caps.minImageCount == 2, "Surface caps minImageCount must be 2");

    VkSwapchainCreateInfoKHR scInfo{};
    scInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    scInfo.surface = surface;
    scInfo.minImageCount = 2;
    scInfo.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    scInfo.imageExtent = { 1280, 720 };
    scInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    scInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;

    VkSwapchainKHR swapchain = 0;
    res = vkCreateSwapchainKHR(device, &scInfo, nullptr, &swapchain);
    TEST_ASSERT(res == VK_SUCCESS && swapchain != 0, "vkCreateSwapchainKHR must succeed");

    uint32_t imageCount = 0;
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
    TEST_ASSERT(imageCount == 2, "Swapchain image count must be 2");

    // 7. Command Buffer Recording & Queue Submission
    VkCommandPool commandPool = 0;
    VkCommandPoolCreateInfo cpInfo{};
    cpInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpInfo.queueFamilyIndex = 0;
    res = vkCreateCommandPool(device, &cpInfo, nullptr, &commandPool);
    TEST_ASSERT(res == VK_SUCCESS && commandPool != 0, "vkCreateCommandPool must succeed");

    VkCommandBufferAllocateInfo cbAlloc{};
    cbAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbAlloc.commandPool = commandPool;
    cbAlloc.commandBufferCount = 1;
    VkCommandBuffer cmdBuf = nullptr;
    res = vkAllocateCommandBuffers(device, &cbAlloc, &cmdBuf);
    TEST_ASSERT(res == VK_SUCCESS && cmdBuf != nullptr, "vkAllocateCommandBuffers must succeed");

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    res = vkBeginCommandBuffer(cmdBuf, &beginInfo);
    TEST_ASSERT(res == VK_SUCCESS, "vkBeginCommandBuffer must succeed");

    VkClearValue clearColor{};
    clearColor.color.float32[0] = 0.05f;
    clearColor.color.float32[1] = 0.1f;
    clearColor.color.float32[2] = 0.25f;
    clearColor.color.float32[3] = 1.0f;

    VkRenderPassBeginInfo rpBegin{};
    rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.renderArea.extent = { 1280, 720 };
    rpBegin.clearValueCount = 1;
    rpBegin.pClearValues = &clearColor;
    vkCmdBeginRenderPass(cmdBuf, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport vp{ 0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f };
    vkCmdSetViewport(cmdBuf, 0, 1, &vp);
    vkCmdDraw(cmdBuf, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmdBuf);

    res = vkEndCommandBuffer(cmdBuf);
    TEST_ASSERT(res == VK_SUCCESS, "vkEndCommandBuffer must succeed");

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuf;
    res = vkQueueSubmit(queue, 1, &submitInfo, 0);
    TEST_ASSERT(res == VK_SUCCESS, "vkQueueSubmit must succeed");

    uint32_t imageIndex = 0;
    res = vkAcquireNextImageKHR(device, swapchain, 0, 0, 0, &imageIndex);
    TEST_ASSERT(res == VK_SUCCESS && imageIndex == 0, "vkAcquireNextImageKHR must acquire image 0");

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &imageIndex;
    res = vkQueuePresentKHR(queue, &presentInfo);
    TEST_ASSERT(res == VK_SUCCESS, "vkQueuePresentKHR must succeed");

    // 8. Shell Command Verification ('vulkan' and 'vulkan test')
    shell::CommandShell testShell;
    std::ostringstream ssVk;
    testShell.execute("vulkan", ssVk);
    std::string outVk = ssVk.str();
    TEST_ASSERT(outVk.find("MicaNT PrismVK & Vulkan 1.3 ICD Subsystem") != std::string::npos, "vulkan command must display header");
    TEST_ASSERT(outVk.find("vulkan-1.dll (Khronos Vulkan 1.3.0 Specification)") != std::string::npos, "vulkan command must list 1.3 spec");
    TEST_ASSERT(outVk.find("8192 MB") != std::string::npos, "vulkan command must list 8192 MB VRAM");

    std::ostringstream ssVkTest;
    testShell.execute("vulkan test", ssVkTest);
    std::string outVkTest = ssVkTest.str();
    TEST_ASSERT(outVkTest.find("Vulkan 1.3 pipeline verified successfully") != std::string::npos, "vulkan test must verify pipeline");

    // 9. Cleanup
    vkFreeCommandBuffers(device, commandPool, 1, &cmdBuf);
    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroySwapchainKHR(device, swapchain, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
}

// ============================================================================
// Suite 47: Prism3D12 Low-Level Subsystem & Programmable Shader Bytecode VM
// ============================================================================
void Test_Prism3D12_And_ProgrammableShaderVM() {
    using namespace micant::prismx;
    using namespace micant::prism3d12;
    using namespace micant::prism_vm;

    // ------------------------------------------------------------------------
    // 1. Direct3D 12 Device, Command Queue, and Allocator Creation
    // ------------------------------------------------------------------------
    ID3D12Device* device = nullptr;
    int32_t hrDev = D3D12CreateDevice(nullptr, prism3d::D3D_FEATURE_LEVEL_12_0, IID_ID3D12Device, reinterpret_cast<void**>(&device));
    TEST_ASSERT(hrDev == 0 && device != nullptr, "D3D12CreateDevice must succeed");

    D3D12_COMMAND_QUEUE_DESC qDesc{};
    qDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    qDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    ID3D12CommandQueue* queue = nullptr;
    int32_t hrQ = device->CreateCommandQueue(&qDesc, IID_ID3D12CommandQueue, reinterpret_cast<void**>(&queue));
    TEST_ASSERT(hrQ == 0 && queue != nullptr, "CreateCommandQueue must succeed");

    ID3D12CommandAllocator* allocator = nullptr;
    int32_t hrAlloc = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_ID3D12CommandAllocator, reinterpret_cast<void**>(&allocator));
    TEST_ASSERT(hrAlloc == 0 && allocator != nullptr, "CreateCommandAllocator must succeed");

    // ------------------------------------------------------------------------
    // 2. Direct3D 12 Fence Synchronization
    // ------------------------------------------------------------------------
    ID3D12Fence* fence = nullptr;
    int32_t hrFence = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_ID3D12Fence, reinterpret_cast<void**>(&fence));
    TEST_ASSERT(hrFence == 0 && fence != nullptr, "CreateFence must succeed");
    TEST_ASSERT(fence->GetCompletedValue() == 0, "Initial fence value must be 0");

    fence->Signal(42);
    TEST_ASSERT(fence->GetCompletedValue() == 42, "Signaled fence value must be 42");

    // ------------------------------------------------------------------------
    // 3. Direct3D 12 Resources, Barriers, and Descriptor Heaps
    // ------------------------------------------------------------------------
    D3D12_RESOURCE_DESC bufDesc{};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = 1024;
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    ID3D12Resource* buffer = nullptr;
    int32_t hrRes = device->CreateCommittedResource(nullptr, 0, &bufDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_ID3D12Resource, reinterpret_cast<void**>(&buffer));
    TEST_ASSERT(hrRes == 0 && buffer != nullptr, "CreateCommittedResource for buffer must succeed");

    void* mappedPtr = nullptr;
    int32_t hrMap = buffer->Map(0, nullptr, &mappedPtr);
    TEST_ASSERT(hrMap == 0 && mappedPtr != nullptr, "Buffer Map() must return non-null pointer");
    std::memset(mappedPtr, 0xAA, 1024);
    buffer->Unmap(0, nullptr);

    // Texture Resource & RTV Descriptor Heap
    D3D12_RESOURCE_DESC texDesc{};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = 640;
    texDesc.Height = 480;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    ID3D12Resource* renderTarget = nullptr;
    device->CreateCommittedResource(nullptr, 0, &texDesc, D3D12_RESOURCE_STATE_PRESENT, nullptr, IID_ID3D12Resource, reinterpret_cast<void**>(&renderTarget));
    TEST_ASSERT(renderTarget != nullptr, "CreateCommittedResource for render target must succeed");

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.NumDescriptors = 4;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    ID3D12DescriptorHeap* rtvHeap = nullptr;
    int32_t hrHeap = device->CreateDescriptorHeap(&heapDesc, IID_ID3D12DescriptorHeap, reinterpret_cast<void**>(&rtvHeap));
    TEST_ASSERT(hrHeap == 0 && rtvHeap != nullptr, "CreateDescriptorHeap for RTV must succeed");

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    TEST_ASSERT(rtvHandle.ptr != 0, "Descriptor heap start handle must be non-zero");
    device->CreateRenderTargetView(renderTarget, nullptr, rtvHandle);

    // ------------------------------------------------------------------------
    // 4. Command List Recording, Execution, and Pipeline State
    // ------------------------------------------------------------------------
    ID3D12GraphicsCommandList* cmdList = nullptr;
    int32_t hrCmd = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, nullptr, IID_ID3D12GraphicsCommandList, reinterpret_cast<void**>(&cmdList));
    TEST_ASSERT(hrCmd == 0 && cmdList != nullptr, "CreateCommandList must succeed");
    TEST_ASSERT(cmdList->GetType() == D3D12_COMMAND_LIST_TYPE_DIRECT, "Command list type must be DIRECT");

    // Barrier 1: Transition PRESENT -> RENDER_TARGET
    D3D12_RESOURCE_BARRIER b1{};
    b1.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b1.Transition.pResource = renderTarget;
    b1.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    b1.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    cmdList->ResourceBarrier(1, &b1);

    // Viewport & Scissor
    D3D12_VIEWPORT vp{ 0.0f, 0.0f, 640.0f, 480.0f, 0.0f, 1.0f };
    D3D12_RECT scissor{ 0, 0, 640, 480 };
    cmdList->RSSetViewports(1, &vp);
    cmdList->RSSetScissorRects(1, &scissor);

    // Render Targets & Clear
    cmdList->OMSetRenderTargets(1, &rtvHandle, 0, nullptr);
    const float clearColor[4] = { 0.2f, 0.4f, 0.8f, 1.0f };
    cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    // Draw
    cmdList->IASetPrimitiveTopology(prism3d::D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->DrawInstanced(3, 1, 0, 0);

    // Barrier 2: Transition RENDER_TARGET -> PRESENT
    D3D12_RESOURCE_BARRIER b2{};
    b2.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b2.Transition.pResource = renderTarget;
    b2.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    b2.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    cmdList->ResourceBarrier(1, &b2);

    int32_t hrClose = cmdList->Close();
    TEST_ASSERT(hrClose == 0, "CommandList Close() must succeed");

    auto* cmdListImpl = static_cast<Prism3D12GraphicsCommandListImpl*>(cmdList);
    TEST_ASSERT(cmdListImpl->GetRecordedCommands().size() == 8, "Command list must have recorded 8 commands");

    // Execute on Command Queue
    ID3D12CommandList* ppLists[] = { cmdList };
    queue->ExecuteCommandLists(1, ppLists);

    auto* resImpl = static_cast<Prism3D12ResourceImpl*>(renderTarget);
    TEST_ASSERT(resImpl->GetCurrentState() == D3D12_RESOURCE_STATE_PRESENT, "Resource state after command execution must be PRESENT");

    // Fence signal from Queue
    queue->Signal(fence, 100);
    TEST_ASSERT(fence->GetCompletedValue() == 100, "Fence must reach signaled value 100 after Queue execution");

    // Reset allocator and command list
    int32_t hrResetAlloc = allocator->Reset();
    TEST_ASSERT(hrResetAlloc == 0, "CommandAllocator Reset() must succeed");
    int32_t hrResetList = cmdList->Reset(allocator, nullptr);
    TEST_ASSERT(hrResetList == 0, "CommandList Reset() must succeed");
    cmdList->Close();

    // ------------------------------------------------------------------------
    // 5. Programmable Shader Bytecode Virtual Machine (PrismShaderVM)
    // ------------------------------------------------------------------------
    // Test Vertex Shader MVP Transform
    auto vsProg = PrismShaderVM::BuildMVPTransformVS();
    TEST_ASSERT(vsProg.InstructionCount() >= 5, "MVP Vertex Shader must contain at least 5 bytecode instructions");

    VectorRegister inPos(2.0f, 3.0f, 4.0f, 1.0f);
    VectorRegister inCol(0.9f, 0.7f, 0.5f, 1.0f);
    VectorRegister inUV(0.25f, 0.75f, 0.0f, 0.0f);
    VectorRegister inNorm(0.0f, 1.0f, 0.0f, 0.0f);

    std::array<VectorRegister, 16> consts{};
    // Scaling matrix: scale X by 2, Y by 3, Z by 4
    consts[0] = VectorRegister(2.0f, 0.0f, 0.0f, 0.0f);
    consts[1] = VectorRegister(0.0f, 3.0f, 0.0f, 0.0f);
    consts[2] = VectorRegister(0.0f, 0.0f, 4.0f, 0.0f);
    consts[3] = VectorRegister(0.0f, 0.0f, 0.0f, 1.0f);

    VectorRegister outPos, outCol;
    PrismShaderVM::ExecuteVertexShader(vsProg, inPos, inCol, inUV, inNorm, consts, outPos, outCol);

    TEST_ASSERT(outPos.x() == 4.0f, "VS output X must be 2.0 * 2.0 = 4.0");
    TEST_ASSERT(outPos.y() == 9.0f, "VS output Y must be 3.0 * 3.0 = 9.0");
    TEST_ASSERT(outPos.z() == 16.0f, "VS output Z must be 4.0 * 4.0 = 16.0");
    TEST_ASSERT(outPos.w() == 1.0f, "VS output W must be 1.0");
    TEST_ASSERT(outCol.x() == 0.9f && outCol.y() == 0.7f, "VS color pass-through must match input color");

    // Test Pixel Shader Texture Modulation
    auto psProg = PrismShaderVM::BuildTexturedModulatePS();
    auto samplerFn = [](uint8_t, float u, float v) -> VectorRegister {
        return VectorRegister(u, v, 0.5f, 1.0f);
    };

    VectorRegister psOutCol;
    PrismShaderVM::ExecutePixelShader(psProg, outPos, inCol, inUV, inNorm, consts, samplerFn, psOutCol);
    // psOutCol = inCol * texel(0.25, 0.75, 0.5, 1.0)
    TEST_ASSERT(std::abs(psOutCol.x() - (0.9f * 0.25f)) < 1e-4f, "PS output R must be modulated by texture U coordinate");
    TEST_ASSERT(std::abs(psOutCol.y() - (0.7f * 0.75f)) < 1e-4f, "PS output G must be modulated by texture V coordinate");

    // Test Directional Lighting Pixel Shader
    auto lightProg = PrismShaderVM::BuildDirectionalLightingPS();
    consts[0] = VectorRegister(0.0f, 1.0f, 0.0f, 0.0f); // Light Dir: +Y
    consts[1] = VectorRegister(0.1f, 0.1f, 0.1f, 1.0f); // Ambient: 0.1
    consts[2] = VectorRegister(0.8f, 0.8f, 0.8f, 1.0f); // Diffuse: 0.8
    consts[4] = VectorRegister(0.0f, 0.0f, 0.0f, 0.0f); // Zero vector for max(N.L, 0)

    VectorRegister litCol;
    PrismShaderVM::ExecutePixelShader(lightProg, outPos, VectorRegister(1.0f, 1.0f, 1.0f, 1.0f), inUV, inNorm, consts, nullptr, litCol);
    // N = (0, 1, 0), L = (0, 1, 0) => dot = 1.0 => Ambient (0.1) + Diffuse (0.8) = 0.9
    TEST_ASSERT(std::abs(litCol.x() - 0.9f) < 1e-4f, "Fully lit diffuse pixel must equal ambient + diffuse (0.9)");

    // ------------------------------------------------------------------------
    // 6. Shell Command Integration ('prismx d3d12' and 'prismx vm')
    // ------------------------------------------------------------------------
    shell::CommandShell testShell;

    std::ostringstream ssD3D12;
    testShell.execute("prismx d3d12", ssD3D12);
    std::string outD3D12 = ssD3D12.str();
    TEST_ASSERT(outD3D12.find("Direct3D 12 Command Pipeline Executed Successfully!") != std::string::npos, "prismx d3d12 command must succeed");
    TEST_ASSERT(outD3D12.find("PRESENT -> RENDER_TARGET -> PRESENT") != std::string::npos, "prismx d3d12 must report barrier transitions");
    TEST_ASSERT(outD3D12.find("GPU Fence signaled successfully") != std::string::npos, "prismx d3d12 must report fence synchronization");

    std::ostringstream ssVm;
    testShell.execute("prismx vm", ssVm);
    std::string outVm = ssVm.str();
    TEST_ASSERT(outVm.find("Launching Sovereign Programmable Shader Bytecode Virtual Machine") != std::string::npos, "prismx vm command must start VM");
    TEST_ASSERT(outVm.find("Bytecode execution verified with 100% precision") != std::string::npos, "prismx vm must report precision verification");

    // Cleanup resources
    cmdList->Release();
    rtvHeap->Release();
    renderTarget->Release();
    buffer->Release();
    fence->Release();
    allocator->Release();
    queue->Release();
    device->Release();
}

// ============================================================================
// Suite 48: EmeraldFS Sovereign Storage Engine & DaytonaMM PFN/WSL Paging Tests
// ============================================================================
void Test_EmeraldFS_And_DaytonaMM_Subsystems() {
    // ------------------------------------------------------------------------
    // Part 1: EmeraldFS Unified Volume & Storage Engine
    // ------------------------------------------------------------------------
    auto ramDisk = std::make_shared<storage::RamDiskDevice>(L"\\Device\\Harddisk0\\Partition1", 16 * 1024 * 1024); // 16MB RamDisk
    auto& volMgr = emeraldfs::EmeraldVolumeManager::get();

    // 1. Format RamDisk with EmeraldFS
    NtStatus stFormat = volMgr.formatEmeraldFS(*ramDisk, 4096, L"MicaNT_System");
    TEST_ASSERT(NT_SUCCESS(stFormat), "EmeraldFS volume formatting must succeed");

    // 2. Mount Volume & Verify Signature
    NtStatus stMount = volMgr.mountVolume(ramDisk);
    TEST_ASSERT(NT_SUCCESS(stMount), "EmeraldFS volume mounting must succeed");
    TEST_ASSERT(volMgr.getMountedType() == emeraldfs::FileSystemType::EmeraldNTFS, "Mounted type must be EmeraldNTFS");

    // 3. Create Hierarchical Directories & Files
    uint64_t dirRec = 0;
    NtStatus stDir = volMgr.createDirectory(L"\\Windows\\System32", dirRec);
    TEST_ASSERT(NT_SUCCESS(stDir) && dirRec > 0, "Directory creation in EmeraldFS must succeed");

    uint64_t fileRec = 0;
    NtStatus stFile = volMgr.createFile(L"\\Windows\\System32\\ntoskrnl.exe", 0x20 /* Archive */, fileRec);
    TEST_ASSERT(NT_SUCCESS(stFile) && fileRec > 0, "File creation in EmeraldFS must succeed");

    // 4. Primary $DATA Stream Read/Write
    const std::string kernelCode = "MicaNT Sovereign Microkernel Binary Payload (Clean-Room ISO C++23)";
    uint64_t bytesWritten = 0;
    NtStatus stWritePrim = volMgr.writePrimaryStream(fileRec, kernelCode.data(), kernelCode.size(), 0, bytesWritten);
    TEST_ASSERT(NT_SUCCESS(stWritePrim) && bytesWritten == kernelCode.size(), "Writing primary $DATA stream must succeed");

    std::vector<char> readBuffer(kernelCode.size() + 1, 0);
    uint64_t bytesRead = 0;
    NtStatus stReadPrim = volMgr.readPrimaryStream(fileRec, readBuffer.data(), kernelCode.size(), 0, bytesRead);
    TEST_ASSERT(NT_SUCCESS(stReadPrim) && bytesRead == kernelCode.size(), "Reading primary $DATA stream must succeed");
    TEST_ASSERT(std::string_view(readBuffer.data(), bytesRead) == kernelCode, "Read primary data must match written content");

    // 5. Alternate Data Streams (ADS): Read/Write multiple streams per file
    const std::string zoneIdentifier = "[ZoneTransfer]\nZoneId=3";
    const std::string sha256Digest = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

    uint64_t ads1Written = 0;
    NtStatus stAds1 = volMgr.writeAlternateStream(fileRec, L"Zone.Identifier", zoneIdentifier.data(), zoneIdentifier.size(), 0, ads1Written);
    TEST_ASSERT(NT_SUCCESS(stAds1) && ads1Written == zoneIdentifier.size(), "Writing Zone.Identifier Alternate Data Stream must succeed");

    uint64_t ads2Written = 0;
    NtStatus stAds2 = volMgr.writeAlternateStream(fileRec, L"SHA256", sha256Digest.data(), sha256Digest.size(), 0, ads2Written);
    TEST_ASSERT(NT_SUCCESS(stAds2) && ads2Written == sha256Digest.size(), "Writing SHA256 Alternate Data Stream must succeed");

    // Verify Reading ADS independently
    std::vector<char> ads1Buffer(zoneIdentifier.size() + 1, 0);
    uint64_t ads1Read = 0;
    NtStatus stAds1Read = volMgr.readAlternateStream(fileRec, L"Zone.Identifier", ads1Buffer.data(), zoneIdentifier.size(), 0, ads1Read);
    TEST_ASSERT(NT_SUCCESS(stAds1Read) && ads1Read == zoneIdentifier.size(), "Reading Zone.Identifier ADS must succeed");
    TEST_ASSERT(std::string_view(ads1Buffer.data(), ads1Read) == zoneIdentifier, "Zone.Identifier content must match");

    std::vector<char> ads2Buffer(sha256Digest.size() + 1, 0);
    uint64_t ads2Read = 0;
    NtStatus stAds2Read = volMgr.readAlternateStream(fileRec, L"SHA256", ads2Buffer.data(), sha256Digest.size(), 0, ads2Read);
    TEST_ASSERT(NT_SUCCESS(stAds2Read) && ads2Read == sha256Digest.size(), "Reading SHA256 ADS must succeed");
    TEST_ASSERT(std::string_view(ads2Buffer.data(), ads2Read) == sha256Digest, "SHA256 content must match");

    // 6. Metadata inspection
    emeraldfs::FileMetadata meta{};
    NtStatus stMeta = volMgr.getFileMetadata(fileRec, meta);
    TEST_ASSERT(NT_SUCCESS(stMeta), "Retrieving file metadata must succeed");
    TEST_ASSERT(meta.primarySize == kernelCode.size(), "Metadata primary size must match");
    TEST_ASSERT(meta.alternateStreams.size() == 2, "File must possess exactly 2 Alternate Data Streams");

    // 7. Write-Ahead Logging (WAL) verification in EmeraldJournal
    auto* journal = volMgr.getJournal();
    TEST_ASSERT(journal != nullptr, "EmeraldFS volume must have active journal");
    TEST_ASSERT(journal->getEntryCount() >= 4, "Journal must record creation and stream write operations");

    // ------------------------------------------------------------------------
    // Part 2: DaytonaMM Virtual Memory Manager, PFN Database, and Working Set
    // ------------------------------------------------------------------------
    auto& daytona = daytonamm::DaytonaMemoryExecutive::get();
    daytona.initialize(4096); // 4096 pages = 16MB physical space

    auto telemInitial = daytona.getTelemetry();
    TEST_ASSERT(telemInitial.totalPhysicalPages == 4096, "Daytona total physical pages must equal 4096");
    TEST_ASSERT(telemInitial.zeroedPages > 0, "Daytona must have initialized zeroed pages");

    // Allocate virtual memory (VAD reservation + commit)
    mm::ProcessAddressSpace vas;
    daytonamm::ProcessWorkingSet ws(64); // Max 64 resident pages in working set

    uintptr_t baseAddr = 0;
    size_t allocSize = 64 * 1024; // 64KB = 16 pages
    NtStatus stAlloc = daytona.allocateVirtualMemory(vas, baseAddr, allocSize, mm::MEM_COMMIT | mm::MEM_RESERVE, mm::PAGE_READWRITE);
    TEST_ASSERT(NT_SUCCESS(stAlloc) && baseAddr != 0, "Daytona allocateVirtualMemory must succeed");

    // Trigger Demand-Zero Page Faults
    for (size_t p = 0; p < 8; ++p) {
        uintptr_t faultAddr = baseAddr + (p * mm::PageSize4KB) + 128;
        NtStatus stFault = daytona.handlePageFault(vas, ws, faultAddr, 0);
        TEST_ASSERT(NT_SUCCESS(stFault), "Demand-zero page fault must resolve successfully");
    }

    TEST_ASSERT(ws.getResidentPageCount() == 8, "Working set must now contain 8 resident active pages");
    auto telemAfterFaults = daytona.getTelemetry();
    TEST_ASSERT(telemAfterFaults.demandZeroFaults >= 8, "Telemetry must record at least 8 demand-zero page faults");
    TEST_ASSERT(telemAfterFaults.activePages >= 8, "Active PFN count must reflect resident pages");

    // Working Set Trimming: Evict 4 oldest pages to Standby list
    size_t evicted = daytona.trimWorkingSet(ws, 4);
    TEST_ASSERT(evicted == 4, "Daytona working set trimming must evict exactly 4 pages");
    TEST_ASSERT(ws.getResidentPageCount() == 4, "Working set must now contain 4 resident pages");

    auto telemAfterTrim = daytona.getTelemetry();
    TEST_ASSERT(telemAfterTrim.standbyPages >= 4, "Standby page list must receive evicted clean pages");

    // Trigger Transition Fault on an evicted page: resolves from Standby -> Active without zeroing
    uintptr_t standbyAddr = baseAddr + 128; // Page 0 was oldest
    NtStatus stTransFault = daytona.handlePageFault(vas, ws, standbyAddr, 0);
    TEST_ASSERT(NT_SUCCESS(stTransFault), "Transition fault on standby page must succeed");

    auto telemFinal = daytona.getTelemetry();
    TEST_ASSERT(telemFinal.transitionFaults >= 1, "Telemetry must record transition fault");
    TEST_ASSERT(ws.getResidentPageCount() == 5, "Working set must now have 5 resident pages after soft fault");
}

// ============================================================================
// Suite 49: DirectX Dynamic Loader Exports & DXBC Bytecode Container Tests
// ============================================================================
void Test_DirectX_DynamicLoader_And_DXBC_Container() {
    using namespace micant::prismx;
    using namespace micant::prism3d;
    using namespace micant::prism3d12;
    using namespace micant::prism_vm;
    using namespace micant::prism_compiler;

    // 1. Initialize and register all DirectX DLL exports in DynamicLoader
    InitializeDirectXSubsystemExports();
    auto& ldr = ldr::DynamicLoader::get();

    // Verify dxgi.dll exports
    void* fnCreateDXGIFactory1 = ldr.getExport("dxgi.dll", "CreateDXGIFactory1");
    TEST_ASSERT(fnCreateDXGIFactory1 != nullptr, "dxgi.dll!CreateDXGIFactory1 must be registered in DynamicLoader");

    // Verify d3d11.dll exports
    void* fnD3D11CreateDevice = ldr.getExport("d3d11.dll", "D3D11CreateDevice");
    TEST_ASSERT(fnD3D11CreateDevice != nullptr, "d3d11.dll!D3D11CreateDevice must be registered in DynamicLoader");

    // Verify d3d12.dll exports
    void* fnD3D12CreateDevice = ldr.getExport("d3d12.dll", "D3D12CreateDevice");
    TEST_ASSERT(fnD3D12CreateDevice != nullptr, "d3d12.dll!D3D12CreateDevice must be registered in DynamicLoader");

    // Verify d3dcompiler_47.dll exports
    void* fnD3DCreateBlob = ldr.getExport("d3dcompiler_47.dll", "D3DCreateBlob");
    void* fnD3DDisassemble = ldr.getExport("d3dcompiler_47.dll", "D3DDisassemble");
    void* fnD3DCompile = ldr.getExport("d3dcompiler_47.dll", "D3DCompile");
    TEST_ASSERT(fnD3DCreateBlob != nullptr, "d3dcompiler_47.dll!D3DCreateBlob must be registered in DynamicLoader");
    TEST_ASSERT(fnD3DDisassemble != nullptr, "d3dcompiler_47.dll!D3DDisassemble must be registered in DynamicLoader");
    TEST_ASSERT(fnD3DCompile != nullptr, "d3dcompiler_47.dll!D3DCompile must be registered in DynamicLoader");

    // 2. Test invoking DXGI via dynamic loader export pointer
    using PFN_CreateDXGIFactory1 = int32_t(*)(const IID&, void**);
    auto pfnCreateDXGIFactory1 = reinterpret_cast<PFN_CreateDXGIFactory1>(fnCreateDXGIFactory1);
    IDXGIFactory1* factory = nullptr;
    int32_t hrDxgi = pfnCreateDXGIFactory1(IID_IDXGIFactory1, reinterpret_cast<void**>(&factory));
    TEST_ASSERT(hrDxgi == 0 && factory != nullptr, "CreateDXGIFactory1 via dynamic loader export must succeed");

    IDXGIAdapter1* adapter = nullptr;
    int32_t hrAdapter = factory->EnumAdapters1(0, &adapter);
    TEST_ASSERT(hrAdapter == 0 && adapter != nullptr, "EnumAdapters1 on factory must succeed");

    DXGI_ADAPTER_DESC1 adDesc{};
    adapter->GetDesc1(&adDesc);
    TEST_ASSERT(adDesc.DedicatedVideoMemory > 0, "Adapter must report non-zero dedicated video memory");

    // 3. Test invoking D3D12 via dynamic loader export pointer
    using PFN_D3D12CreateDevice = int32_t(*)(IUnknown*, D3D_FEATURE_LEVEL, const IID&, void**);
    auto pfnD3D12CreateDevice = reinterpret_cast<PFN_D3D12CreateDevice>(fnD3D12CreateDevice);
    ID3D12Device* device12 = nullptr;
    int32_t hrDev12 = pfnD3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_ID3D12Device, reinterpret_cast<void**>(&device12));
    TEST_ASSERT(hrDev12 == 0 && device12 != nullptr, "D3D12CreateDevice via dynamic loader export must succeed");

    D3D12_COMMAND_QUEUE_DESC qDesc{ D3D12_COMMAND_LIST_TYPE_DIRECT, 0, D3D12_COMMAND_QUEUE_FLAG_NONE, 0 };
    ID3D12CommandQueue* queue = nullptr;
    int32_t hrQueue = device12->CreateCommandQueue(&qDesc, IID_ID3D12CommandQueue, reinterpret_cast<void**>(&queue));
    TEST_ASSERT(hrQueue == 0 && queue != nullptr, "CreateCommandQueue must succeed");

    ID3D12CommandAllocator* allocator = nullptr;
    int32_t hrAlloc = device12->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_ID3D12CommandAllocator, reinterpret_cast<void**>(&allocator));
    TEST_ASSERT(hrAlloc == 0 && allocator != nullptr, "CreateCommandAllocator must succeed");

    ID3D12GraphicsCommandList* cmdList = nullptr;
    int32_t hrList = device12->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, nullptr, IID_ID3D12GraphicsCommandList, reinterpret_cast<void**>(&cmdList));
    TEST_ASSERT(hrList == 0 && cmdList != nullptr, "CreateCommandList must succeed");

    cmdList->Close();
    ID3D12CommandList* ppLists[] = { cmdList };
    queue->ExecuteCommandLists(1, ppLists);

    // 4. Test ID3DBlob creation via dynamic loader export pointer
    using PFN_D3DCreateBlob = int32_t(*)(size_t, ID3DBlob**);
    auto pfnD3DCreateBlob = reinterpret_cast<PFN_D3DCreateBlob>(fnD3DCreateBlob);
    ID3DBlob* testBlob = nullptr;
    int32_t hrBlob = pfnD3DCreateBlob(512, &testBlob);
    TEST_ASSERT(hrBlob == 0 && testBlob != nullptr, "D3DCreateBlob via dynamic loader export must succeed");
    TEST_ASSERT(testBlob->GetBufferSize() == 512, "Blob size must equal 512 bytes");
    TEST_ASSERT(testBlob->GetBufferPointer() != nullptr, "Blob buffer pointer must be non-null");

    // 5. Test Standard DXBC Binary Container Generation & RIFF Chunks
    ShaderProgram vsProg = PrismShaderVM::BuildMVPTransformVS();
    std::vector<uint8_t> dxbcContainer = DxbcContainer::BuildContainer(1, 5, 0, vsProg);
    TEST_ASSERT(dxbcContainer.size() > sizeof(DxbcHeader), "DXBC binary container size must be greater than header size");
    TEST_ASSERT(DxbcContainer::IsDxbc(dxbcContainer.data(), dxbcContainer.size()), "Binary stream must be recognized as valid DXBC");

    // 6. Test Parsing DXBC Container and Signature / Bytecode Chunks
    DxbcContainer parsedContainer;
    bool bParsed = DxbcContainer::Parse(dxbcContainer.data(), dxbcContainer.size(), parsedContainer);
    TEST_ASSERT(bParsed, "DxbcContainer::Parse must succeed on valid DXBC container");
    TEST_ASSERT(parsedContainer.chunks.size() >= 3, "Parsed container must contain at least 3 chunks (ISGN, OSGN, SHDR)");
    TEST_ASSERT(parsedContainer.programType == 1, "Parsed program type must be 1 (Vertex Shader)");
    TEST_ASSERT(parsedContainer.majorVersion == 5 && parsedContainer.minorVersion == 0, "Parsed version must be 5.0 (vs_5_0)");

    // 7. Test D3DDisassemble on DXBC Binary
    using PFN_D3DDisassemble = int32_t(*)(const void*, size_t, uint32_t, const char*, ID3DBlob**);
    auto pfnD3DDisassemble = reinterpret_cast<PFN_D3DDisassemble>(fnD3DDisassemble);
    ID3DBlob* disasmBlob = nullptr;
    int32_t hrDisasm = pfnD3DDisassemble(dxbcContainer.data(), dxbcContainer.size(), 0, "MicaNT Test Shader", &disasmBlob);
    TEST_ASSERT(hrDisasm == 0 && disasmBlob != nullptr, "D3DDisassemble on DXBC binary must succeed");

    std::string_view disasmText(reinterpret_cast<const char*>(disasmBlob->GetBufferPointer()));
    TEST_ASSERT(disasmText.find("vs_5_0") != std::string_view::npos, "Disassembly must contain target profile vs_5_0");
    TEST_ASSERT(disasmText.find("dp4") != std::string_view::npos, "Disassembly must contain dp4 instruction");
    TEST_ASSERT(disasmText.find("ret") != std::string_view::npos, "Disassembly must contain ret instruction");

    // 8. Test Decoding and Executing the parsed DXBC Program in PrismShaderVM
    ShaderProgram decodedProg = parsedContainer.DecodeToProgram();
    TEST_ASSERT(decodedProg.InstructionCount() > 0, "Decoded program must contain instructions");

    VectorRegister inPos(1.0f, 2.0f, 3.0f, 1.0f);
    VectorRegister inCol(1.0f, 0.5f, 0.25f, 1.0f);
    VectorRegister inUV(0.0f, 0.0f, 0.0f, 0.0f);
    VectorRegister inNorm(0.0f, 0.0f, 1.0f, 0.0f);

    std::array<VectorRegister, 16> consts{};
    // Identity matrix in c0..c3
    consts[0] = VectorRegister(1.0f, 0.0f, 0.0f, 0.0f);
    consts[1] = VectorRegister(0.0f, 1.0f, 0.0f, 0.0f);
    consts[2] = VectorRegister(0.0f, 0.0f, 1.0f, 0.0f);
    consts[3] = VectorRegister(0.0f, 0.0f, 0.0f, 1.0f);

    VectorRegister outPos{}, outCol{};
    PrismShaderVM::ExecuteVertexShader(decodedProg, inPos, inCol, inUV, inNorm, consts, outPos, outCol);

    // Verify outPos (transformed position) is (1.0, 2.0, 3.0, 1.0)
    TEST_ASSERT(outPos.x() == 1.0f && outPos.y() == 2.0f &&
                outPos.z() == 3.0f && outPos.w() == 1.0f,
                "DXBC decoded program execution must produce exact transformed position");
    // Verify outCol (passed-through color)
    TEST_ASSERT(outCol.x() == 1.0f && outCol.y() == 0.5f &&
                outCol.z() == 0.25f && outCol.w() == 1.0f,
                "DXBC decoded program execution must produce exact passed-through color");

    // Cleanup COM objects
    disasmBlob->Release();
    testBlob->Release();
    cmdList->Release();
    allocator->Release();
    queue->Release();
    device12->Release();
    adapter->Release();
    factory->Release();
}

// ============================================================================
// Suite 50: DirectX PrismAudio & VectorHID (XAudio2 & XInput) Subsystems
// ============================================================================
void Test_DirectX_PrismAudio_And_XInput_Subsystems() {
    using namespace micant::audio;
    using namespace micant::hid;

    // 1. Initialize subsystem exports
    prism_compiler::InitializeDirectXSubsystemExports();

    // 2. Validate DynamicLoader exports for xaudio2 and xinput
    void* fnXAudio2Create = ldr::DynamicLoader::get().getExport("xaudio2_9.dll", "XAudio2Create");
    TEST_ASSERT(fnXAudio2Create != nullptr, "xaudio2_9.dll must export XAudio2Create");

    void* fnXAudio2Create8 = ldr::DynamicLoader::get().getExport("xaudio2_8.dll", "XAudio2Create");
    TEST_ASSERT(fnXAudio2Create8 != nullptr, "xaudio2_8.dll must export XAudio2Create");

    void* fnXInputGetState = ldr::DynamicLoader::get().getExport("xinput1_4.dll", "XInputGetState");
    TEST_ASSERT(fnXInputGetState != nullptr, "xinput1_4.dll must export XInputGetState");

    void* fnXInputSetState = ldr::DynamicLoader::get().getExport("xinput1_4.dll", "XInputSetState");
    TEST_ASSERT(fnXInputSetState != nullptr, "xinput1_4.dll must export XInputSetState");

    void* fnXInputGetCaps = ldr::DynamicLoader::get().getExport("xinput1_4.dll", "XInputGetCapabilities");
    TEST_ASSERT(fnXInputGetCaps != nullptr, "xinput1_4.dll must export XInputGetCapabilities");

    // 3. Test XAudio2 Engine Creation
    using PFN_XAudio2Create = int32_t(*)(IXAudio2**, uint32_t, uint32_t);
    auto pfnXAudio2Create = reinterpret_cast<PFN_XAudio2Create>(fnXAudio2Create);

    IXAudio2* audioEngine = nullptr;
    int32_t hrAudio = pfnXAudio2Create(&audioEngine, 0, 0);
    TEST_ASSERT(hrAudio == 0 && audioEngine != nullptr, "XAudio2Create must succeed");

    // 4. Test Mastering Voice Creation
    IXAudio2MasteringVoice* masteringVoice = nullptr;
    int32_t hrMaster = audioEngine->CreateMasteringVoice(&masteringVoice, 2, 48000, 0);
    TEST_ASSERT(hrMaster == 0 && masteringVoice != nullptr, "CreateMasteringVoice must succeed");

    uint32_t channelMask = 0;
    masteringVoice->GetChannelMask(&channelMask);
    TEST_ASSERT(channelMask == 0x3, "Mastering voice channel mask must be 0x3 (Stereo)");

    // 5. Test Source Voice Creation & Procedural Audio Synthesis
    WAVEFORMATEX fmt{};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 2;
    fmt.nSamplesPerSec = 48000;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = 4;
    fmt.nAvgBytesPerSec = 48000 * 4;

    IXAudio2SourceVoice* sourceVoice = nullptr;
    int32_t hrSource = audioEngine->CreateSourceVoice(&sourceVoice, &fmt);
    TEST_ASSERT(hrSource == 0 && sourceVoice != nullptr, "CreateSourceVoice must succeed");

    // Synthesize a 440 Hz (Concert A) sine tone for 0.05 seconds (2400 samples)
    std::vector<uint8_t> tonePcmMono = PrismAudioEngineImpl::SynthesizeTone(
        PrismAudioEngineImpl::ToneType::Sine, 440.0f, 0.05f, 48000, 0.8f
    );
    TEST_ASSERT(!tonePcmMono.empty(), "SynthesizeTone must generate non-empty PCM buffer");

    // Duplicate mono samples to stereo
    size_t monoSamples = tonePcmMono.size() / sizeof(int16_t);
    std::vector<int16_t> stereoPcm(monoSamples * 2);
    const int16_t* pMono = reinterpret_cast<const int16_t*>(tonePcmMono.data());
    for (size_t i = 0; i < monoSamples; ++i) {
        stereoPcm[i * 2 + 0] = pMono[i];
        stereoPcm[i * 2 + 1] = pMono[i];
    }

    // Submit buffer to source voice
    XAUDIO2_BUFFER audioBuf{};
    audioBuf.AudioBytes = static_cast<uint32_t>(stereoPcm.size() * sizeof(int16_t));
    audioBuf.pAudioData = reinterpret_cast<const uint8_t*>(stereoPcm.data());
    audioBuf.LoopCount = 0;

    int32_t hrSubmit = sourceVoice->SubmitSourceBuffer(&audioBuf);
    TEST_ASSERT(hrSubmit == 0, "SubmitSourceBuffer must return 0");

    int32_t hrStart = sourceVoice->Start(0);
    TEST_ASSERT(hrStart == 0, "SourceVoice::Start must return 0");

    // 6. Test Software Audio Mixing Cycle
    auto* rawEngine = static_cast<PrismAudioEngineImpl*>(audioEngine);
    std::vector<float> mixedOutput;
    rawEngine->ProcessMixingCycle(480, mixedOutput); // 10ms of 48000Hz stereo = 480 frames = 960 floats

    TEST_ASSERT(mixedOutput.size() == 960, "ProcessMixingCycle must output exactly 960 stereo float samples");

    // Verify non-zero mixed audio energy
    float maxAmp = 0.0f;
    for (float sample : mixedOutput) {
        maxAmp = std::max(maxAmp, std::abs(sample));
    }
    TEST_ASSERT(maxAmp > 0.1f, "Mixed audio must contain valid waveform energy from synthesized 440Hz tone");

    // Check voice state
    XAUDIO2_VOICE_STATE state{};
    sourceVoice->GetState(&state);
    TEST_ASSERT(state.SamplesPlayed >= 480, "Voice state must report at least 480 samples played");

    // 7. Test 3D Positional Spatial Audio Attenuation
    AudioListener listener{};
    listener.position = { 0.0f, 0.0f, 0.0f };

    AudioEmitter emitterNear{};
    emitterNear.position = { 0.0f, 0.0f, 5.0f };
    emitterNear.innerRadius = 1.0f;
    emitterNear.outerRadius = 50.0f;

    AudioEmitter emitterFar{};
    emitterFar.position = { 0.0f, 0.0f, 40.0f };
    emitterFar.innerRadius = 1.0f;
    emitterFar.outerRadius = 50.0f;

    float volNear = 0.0f, panLNear = 0.0f, panRNear = 0.0f;
    PrismAudioEngineImpl::Calculate3DSpatial(listener, emitterNear, volNear, panLNear, panRNear);

    float volFar = 0.0f, panLFar = 0.0f, panRFar = 0.0f;
    PrismAudioEngineImpl::Calculate3DSpatial(listener, emitterFar, volFar, panLFar, panRFar);

    TEST_ASSERT(volNear > volFar, "Near audio emitter must have higher volume factor than far emitter");
    TEST_ASSERT(volFar > 0.0f && volNear <= 1.0f, "Volume factor must be within valid attenuation range");

    // Test lateral panning: emitter on the right
    AudioEmitter emitterRight{};
    emitterRight.position = { 10.0f, 0.0f, 0.0f };
    float volR = 0.0f, panLR = 0.0f, panRR = 0.0f;
    PrismAudioEngineImpl::Calculate3DSpatial(listener, emitterRight, volR, panLR, panRR);
    TEST_ASSERT(panRR > panLR, "Right lateral emitter must pan louder to right channel than left channel");

    // 8. Test VectorHID XInput Gamepad Subsystem
    using PFN_XInputGetState = uint32_t(*)(uint32_t, XINPUT_STATE*);
    using PFN_XInputSetState = uint32_t(*)(uint32_t, const XINPUT_VIBRATION*);
    using PFN_XInputGetCaps  = uint32_t(*)(uint32_t, uint32_t, XINPUT_CAPABILITIES*);

    auto pfnXInputGetState = reinterpret_cast<PFN_XInputGetState>(fnXInputGetState);
    auto pfnXInputSetState = reinterpret_cast<PFN_XInputSetState>(fnXInputSetState);
    auto pfnXInputGetCaps  = reinterpret_cast<PFN_XInputGetCaps>(fnXInputGetCaps);

    // Query capabilities for controller 0
    XINPUT_CAPABILITIES caps{};
    uint32_t errCaps = pfnXInputGetCaps(0, 0, &caps);
    TEST_ASSERT(errCaps == ERROR_SUCCESS, "XInputGetCapabilities must succeed for connected slot 0");
    TEST_ASSERT(caps.Type == XINPUT_DEVTYPE_GAMEPAD, "Device type must be XINPUT_DEVTYPE_GAMEPAD");

    // Configure slot 1 with simulated game input
    VectorControllerManager::get().SetSlotConnected(1, true);

    XINPUT_GAMEPAD simPad{};
    simPad.wButtons = XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_START;
    simPad.bLeftTrigger = 120;
    simPad.bRightTrigger = 240;
    simPad.sThumbLX = 15000;
    simPad.sThumbLY = -12000;
    simPad.sThumbRX = -25000;
    simPad.sThumbRY = 20000;
    VectorControllerManager::get().SetSlotState(1, simPad);

    XINPUT_STATE queryState{};
    uint32_t errState = pfnXInputGetState(1, &queryState);
    TEST_ASSERT(errState == ERROR_SUCCESS, "XInputGetState must succeed on slot 1");
    TEST_ASSERT(queryState.Gamepad.wButtons == (XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_START),
                "Gamepad buttons must match simulated state");
    TEST_ASSERT(queryState.Gamepad.bLeftTrigger == 120 && queryState.Gamepad.bRightTrigger == 240,
                "Gamepad triggers must match simulated state");
    TEST_ASSERT(queryState.Gamepad.sThumbLX == 15000 && queryState.Gamepad.sThumbRX == -25000,
                "Gamepad thumbsticks must match simulated state");

    // Test Deadzone Normalization
    float normX = 0.0f, normY = 0.0f;
    VectorControllerManager::NormalizeThumbstick(5000, 5000, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, normX, normY);
    TEST_ASSERT(normX == 0.0f && normY == 0.0f, "Thumbstick inside deadzone must normalize to (0, 0)");

    VectorControllerManager::NormalizeThumbstick(32767, 0, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, normX, normY);
    TEST_ASSERT(normX > 0.99f && normY == 0.0f, "Thumbstick at max X must normalize to (1.0, 0.0)");

    // Test Force Feedback Vibration
    XINPUT_VIBRATION vib{ 32768, 65535 };
    uint32_t errVib = pfnXInputSetState(1, &vib);
    TEST_ASSERT(errVib == ERROR_SUCCESS, "XInputSetState must succeed");

    XINPUT_VIBRATION recordedVib = VectorControllerManager::get().GetSlotVibration(1);
    TEST_ASSERT(recordedVib.wLeftMotorSpeed == 32768 && recordedVib.wRightMotorSpeed == 65535,
                "Vibration speeds must match submitted values");

    // Test Disconnected Controller Slot
    VectorControllerManager::get().SetSlotConnected(3, false);
    uint32_t errDisc = pfnXInputGetState(3, &queryState);
    TEST_ASSERT(errDisc == ERROR_DEVICE_NOT_CONNECTED, "Disconnected slot must return ERROR_DEVICE_NOT_CONNECTED");

    // Cleanup Audio COM references
    sourceVoice->DestroyVoice();
    masteringVoice->DestroyVoice();
    audioEngine->Release();
}

// ============================================================================
// Suite 51: VanguardDriver Device Stack & PnP Engine Subsystem
// ============================================================================
void Test_VanguardDriver_DeviceStack_And_PnP_Subsystem() {
    using namespace micant::vanguarddriver;

    // 1. Layered Device Stack Tests
    // Create Driver Objects for Bus, Function, and Filter
    io::DriverObject busDriver{};
    busDriver.driverName = L"pci.sys";

    io::DriverObject funcDriver{};
    funcDriver.driverName = L"nvme.sys";

    io::DriverObject filterDriver{};
    filterDriver.driverName = L"diskperf.sys";

    // Track dispatch call order
    std::vector<std::wstring> callStack;

    busDriver.setDispatch(io::IRP_MJ_READ, [](io::DeviceObject* dev, io::Irp* irp) -> NtStatus {
        (void)dev;
        if (irp && irp->userBuffer) {
            auto* pStack = reinterpret_cast<std::vector<std::wstring>*>(irp->userBuffer);
            pStack->push_back(L"bus");
        }
        return NtStatus::Success;
    });

    funcDriver.setDispatch(io::IRP_MJ_READ, [](io::DeviceObject* dev, io::Irp* irp) -> NtStatus {
        (void)dev;
        if (irp && irp->userBuffer) {
            auto* pStack = reinterpret_cast<std::vector<std::wstring>*>(irp->userBuffer);
            pStack->push_back(L"function");
        }
        return NtStatus::Success;
    });

    filterDriver.setDispatch(io::IRP_MJ_READ, [](io::DeviceObject* dev, io::Irp* irp) -> NtStatus {
        (void)dev;
        if (irp && irp->userBuffer) {
            auto* pStack = reinterpret_cast<std::vector<std::wstring>*>(irp->userBuffer);
            pStack->push_back(L"filter");
        }
        return NtStatus::Success;
    });

    // Create Device Objects
    io::DeviceObject pdo{ .driverObject = &busDriver, .deviceName = L"NVMEDevice0" };
    io::DeviceObject fdo{ .driverObject = &funcDriver, .deviceName = L"NVMEFunctional0" };
    io::DeviceObject upperFilter{ .driverObject = &filterDriver, .deviceName = L"NVMEFilter0" };

    // Attach FDO on top of PDO
    io::DeviceObject* attached1 = IoAttachDeviceToDeviceStack(&fdo, &pdo);
    TEST_ASSERT(attached1 == &pdo, "IoAttachDeviceToDeviceStack must return lower device");
    TEST_ASSERT(pdo.attachedDevice == &fdo, "PDO attachedDevice must point to FDO");

    // Attach Upper Filter on top of FDO
    io::DeviceObject* attached2 = IoAttachDeviceToDeviceStack(&upperFilter, &pdo);
    TEST_ASSERT(attached2 == &fdo, "IoAttachDeviceToDeviceStack must attach to previous top device");
    TEST_ASSERT(fdo.attachedDevice == &upperFilter, "FDO attachedDevice must point to UpperFilter");

    // Verify IoGetAttachedDevice returns the top device
    io::DeviceObject* topDevice = IoGetAttachedDevice(&pdo);
    TEST_ASSERT(topDevice == &upperFilter, "IoGetAttachedDevice on PDO must return upper filter");

    // Forward an IRP to top of stack
    io::Irp readIrp{};
    readIrp.majorFunction = io::IRP_MJ_READ;
    readIrp.userBuffer = &callStack;

    NtStatus stCall = IoCallDriver(topDevice, &readIrp);
    TEST_ASSERT(NT_SUCCESS(stCall), "IoCallDriver to top of stack must succeed");
    TEST_ASSERT(!callStack.empty() && callStack[0] == L"filter", "Top of stack filter driver must be called first");

    // 2. Vanguard PnP Device Node & State Machine
    filterDriver.setDispatch(io::IRP_MJ_PNP, [](io::DeviceObject* dev, io::Irp* irp) -> NtStatus {
        (void)dev;
        if (irp) irp->ioStatus.status = NtStatus::Success;
        return NtStatus::Success;
    });
    filterDriver.setDispatch(io::IRP_MJ_POWER, [](io::DeviceObject* dev, io::Irp* irp) -> NtStatus {
        (void)dev;
        if (irp) irp->ioStatus.status = NtStatus::Success;
        return NtStatus::Success;
    });

    auto& pnpEngine = VanguardDriverEngine::get();
    auto devNode = pnpEngine.CreateDeviceNode(L"PCI\\VEN_10EC&DEV_8168&SUBSYS_0123\\0001", &pdo);
    TEST_ASSERT(devNode != nullptr, "CreateDeviceNode must return valid node");
    TEST_ASSERT(devNode->GetState() == PnpDeviceState::Initialized, "Initial PnP state must be Initialized");

    // Start Device sequence
    NtStatus stStart = pnpEngine.StartDevice(*devNode);
    TEST_ASSERT(NT_SUCCESS(stStart), "pnpEngine.StartDevice must return Success");
    TEST_ASSERT(devNode->GetState() == PnpDeviceState::Started, "Device state must be Started");

    // Stop Device sequence
    NtStatus stStop = pnpEngine.StopDevice(*devNode);
    TEST_ASSERT(NT_SUCCESS(stStop), "pnpEngine.StopDevice must return Success");
    TEST_ASSERT(devNode->GetState() == PnpDeviceState::Stopped, "Device state must be Stopped");

    // Remove Device sequence
    NtStatus stRemove = pnpEngine.RemoveDevice(*devNode);
    TEST_ASSERT(NT_SUCCESS(stRemove), "pnpEngine.RemoveDevice must return Success");
    TEST_ASSERT(devNode->GetState() == PnpDeviceState::Removed, "Device state must be Removed");

    // 3. Device Interface Registration (Disk & Audio)
    std::wstring diskInterfaceLink;
    NtStatus stRegDisk = pnpEngine.RegisterDeviceInterface(&pdo, GUID_DEVINTERFACE_DISK, L"", diskInterfaceLink);
    TEST_ASSERT(NT_SUCCESS(stRegDisk), "RegisterDeviceInterface for Disk must succeed");
    TEST_ASSERT(diskInterfaceLink.find(L"\\\\?\\") == 0, "Symbolic link must start with \\\\?\\ prefix");
    TEST_ASSERT(diskInterfaceLink.find(L"53f56307") != std::wstring::npos, "Symbolic link must contain Disk GUID");

    // Before enabling, enumeration should be empty
    auto interfacesBefore = pnpEngine.EnumerateDeviceInterfaces(GUID_DEVINTERFACE_DISK);
    TEST_ASSERT(interfacesBefore.empty(), "Disabled device interfaces must not be returned in enumeration");

    // Enable device interface
    NtStatus stEnable = pnpEngine.SetDeviceInterfaceState(diskInterfaceLink, true);
    TEST_ASSERT(NT_SUCCESS(stEnable), "SetDeviceInterfaceState(true) must succeed");

    // After enabling, enumeration must find it
    auto interfacesAfter = pnpEngine.EnumerateDeviceInterfaces(GUID_DEVINTERFACE_DISK);
    TEST_ASSERT(interfacesAfter.size() == 1, "EnumerateDeviceInterfaces must return 1 active interface");
    TEST_ASSERT(interfacesAfter[0] == diskInterfaceLink, "Enumerated interface must match registered symbolic link");

    // Disable device interface
    pnpEngine.SetDeviceInterfaceState(diskInterfaceLink, false);
    auto interfacesDisabled = pnpEngine.EnumerateDeviceInterfaces(GUID_DEVINTERFACE_DISK);
    TEST_ASSERT(interfacesDisabled.empty(), "Disabled device interface must no longer be returned");

    // 4. Power State Transition Management
    devNode->SetState(PnpDeviceState::Started);
    NtStatus stPowerD3 = devNode->DispatchSetPower(DevicePowerState::PowerDeviceD3);
    TEST_ASSERT(NT_SUCCESS(stPowerD3), "DispatchSetPower(D3) must return Success");
    TEST_ASSERT(devNode->GetPowerState() == DevicePowerState::PowerDeviceD3, "Power state must be PowerDeviceD3 (Sleep)");

    NtStatus stPowerD0 = devNode->DispatchSetPower(DevicePowerState::PowerDeviceD0);
    TEST_ASSERT(NT_SUCCESS(stPowerD0), "DispatchSetPower(D0) must return Success");
    TEST_ASSERT(devNode->GetPowerState() == DevicePowerState::PowerDeviceD0, "Power state must be PowerDeviceD0 (Full On)");

    // 5. Cancel-Safe IRP Queue
    CancelSafeIrpQueue safeQueue;
    io::Irp irpA{}, irpB{};
    irpA.majorFunction = io::IRP_MJ_READ;
    irpB.majorFunction = io::IRP_MJ_WRITE;

    safeQueue.InsertTail(&irpA);
    safeQueue.InsertTail(&irpB);
    TEST_ASSERT(safeQueue.Count() == 2, "CancelSafeIrpQueue must contain 2 IRPs");

    // Cancel irpA
    bool bCancelled = safeQueue.CancelIrp(&irpA);
    TEST_ASSERT(bCancelled, "CancelIrp on queued IRP must return true");
    TEST_ASSERT(irpA.ioStatus.status == NtStatus::Cancelled, "Cancelled IRP status must be NtStatus::Cancelled");
    TEST_ASSERT(safeQueue.Count() == 1, "Queue count after cancellation must be 1");

    // Remove remaining IRP
    io::Irp* nextIrp = safeQueue.RemoveNext();
    TEST_ASSERT(nextIrp == &irpB, "Next IRP in queue must be irpB");
    TEST_ASSERT(safeQueue.Count() == 0, "Queue must now be empty");
}

// ============================================================================
// Suite 52: AegisSandbox Sovereign Process Containment & Job Objects Tests
// ============================================================================
void Test_AegisSandbox_JobObjects_And_ProcessContainment() {
    using namespace micant::aegis;

    auto& sandboxMgr = AegisSandboxManager::get();
    sandboxMgr.reset();

    // 1. Creation and Named Registry Lookup
    auto job = sandboxMgr.createJobObject(L"\\BaseNamedObjects\\AegisSandboxJob");
    TEST_ASSERT(job != nullptr, "AegisJobObject creation must succeed");
    TEST_ASSERT(job->getName() == L"\\BaseNamedObjects\\AegisSandboxJob", "Job name must match initialization");

    auto openedJob = sandboxMgr.openJobObject(L"\\BaseNamedObjects\\AegisSandboxJob");
    TEST_ASSERT(openedJob == job, "openJobObject must return same instance as created");

    // 2. Active Process Quota Limit
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_ACTIVE_PROCESS |
                                              JOB_OBJECT_LIMIT_PROCESS_MEMORY |
                                              JOB_OBJECT_LIMIT_JOB_MEMORY |
                                              JOB_OBJECT_LIMIT_PROCESS_TIME;
    limits.BasicLimitInformation.ActiveProcessLimit = 2;
    limits.ProcessMemoryLimit = 16 * 1024 * 1024; // 16MB per process
    limits.JobMemoryLimit = 24 * 1024 * 1024;     // 24MB aggregate job memory
    limits.BasicLimitInformation.PerProcessUserTimeLimit = 1000; // 1000 units
    job->setExtendedLimits(limits);

    auto& procMgr = ps::ProcessManager::get();
    auto proc1 = procMgr.createProcess(L"sandbox_app1.exe");
    auto proc2 = procMgr.createProcess(L"sandbox_app2.exe");
    auto proc3 = procMgr.createProcess(L"sandbox_app3.exe");

    NtStatus stAssign1 = job->assignProcess(proc1);
    NtStatus stAssign2 = job->assignProcess(proc2);
    TEST_ASSERT(NT_SUCCESS(stAssign1), "Assigning proc1 to job must succeed");
    TEST_ASSERT(NT_SUCCESS(stAssign2), "Assigning proc2 to job must succeed");
    TEST_ASSERT(job->getActiveProcessCount() == 2, "Job active process count must be 2");

    // Exceed Active Process Limit (attempting to add 3rd process when limit is 2)
    NtStatus stAssign3 = job->assignProcess(proc3);
    TEST_ASSERT(stAssign3 == NtStatus::QuotaExceeded, "Adding 3rd process must fail with QuotaExceeded");
    TEST_ASSERT(job->getActiveProcessCount() == 2, "Active process count must remain 2");

    // Verify reverse process lookup
    auto locatedJob = sandboxMgr.getJobForProcess(proc1->getPid());
    TEST_ASSERT(locatedJob == job, "getJobForProcess must find parent job");

    // 3. Per-Process and Job-Wide Memory Limits
    // Allocate 10MB for proc1 (under 16MB limit)
    NtStatus stMem1 = job->checkAndRecordMemoryAlloc(proc1->getPid(), 10 * 1024 * 1024);
    TEST_ASSERT(NT_SUCCESS(stMem1), "Allocating 10MB for proc1 must succeed");

    // Allocate 8MB more for proc1 -> 10 + 8 = 18MB > 16MB limit -> QuotaExceeded
    NtStatus stMemExceedProc = job->checkAndRecordMemoryAlloc(proc1->getPid(), 8 * 1024 * 1024);
    TEST_ASSERT(stMemExceedProc == NtStatus::QuotaExceeded, "Exceeding per-process memory limit must return QuotaExceeded");

    // Allocate 12MB for proc2 -> Job memory = 10 + 12 = 22MB <= 24MB job limit -> Success
    NtStatus stMem2 = job->checkAndRecordMemoryAlloc(proc2->getPid(), 12 * 1024 * 1024);
    TEST_ASSERT(NT_SUCCESS(stMem2), "Allocating 12MB for proc2 must succeed");

    // Allocate 3MB more for proc2 -> Job memory = 22 + 3 = 25MB > 24MB job limit -> QuotaExceeded
    NtStatus stMemExceedJob = job->checkAndRecordMemoryAlloc(proc2->getPid(), 3 * 1024 * 1024);
    TEST_ASSERT(stMemExceedJob == NtStatus::QuotaExceeded, "Exceeding job aggregate memory limit must return QuotaExceeded");

    auto extLimits = job->getExtendedLimits();
    TEST_ASSERT(extLimits.PeakProcessMemoryUsed == 12 * 1024 * 1024, "Peak process memory must be 12MB");
    TEST_ASSERT(extLimits.PeakJobMemoryUsed == 22 * 1024 * 1024, "Peak job memory must be 22MB");

    // Free memory
    job->recordMemoryFree(proc1->getPid(), 4 * 1024 * 1024);

    // 4. Per-Process User Time Quota & Termination
    NtStatus stCpu1 = job->recordCpuExecution(proc1->getPid(), 400, 50);
    TEST_ASSERT(NT_SUCCESS(stCpu1), "Recording 400 user cycles must succeed");

    // Exceed user time limit: 400 + 700 = 1100 > 1000 limit -> terminates proc1
    NtStatus stCpuExceed = job->recordCpuExecution(proc1->getPid(), 700, 50);
    TEST_ASSERT(stCpuExceed == NtStatus::QuotaExceeded, "Exceeding user time quota must return QuotaExceeded");
    TEST_ASSERT(proc1->isTerminated(), "Process exceeding user time quota must be terminated");
    TEST_ASSERT(job->getActiveProcessCount() == 1, "Active process count must decrement after termination");

    // 5. Security Token Sandbox Confinement
    se::Sid sandboxSid(5, {21, 9999, 1});
    auto sandboxToken = std::make_shared<se::TokenObject>(sandboxSid, se::TokenType::Primary);
    job->setSandboxToken(sandboxToken);

    auto proc4 = procMgr.createProcess(L"sandbox_isolated.exe");
    NtStatus stAssign4 = job->assignProcess(proc4);
    TEST_ASSERT(NT_SUCCESS(stAssign4), "Assigning proc4 to job must succeed");
    TEST_ASSERT(proc4->getToken() == sandboxToken, "Proc4 must inherit restricted sandbox token");

    // 6. Kill On Job Close Enforcement
    auto limitsKill = job->getExtendedLimits();
    limitsKill.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    job->setExtendedLimits(limitsKill);

    TEST_ASSERT(!proc2->isTerminated(), "Proc2 must be active before handle close");
    job->onHandleClosed();
    TEST_ASSERT(proc2->isTerminated(), "Proc2 must be terminated on job handle close");
    TEST_ASSERT(proc4->isTerminated(), "Proc4 must be terminated on job handle close");
    TEST_ASSERT(job->getActiveProcessCount() == 0, "All processes must be terminated");

    auto accounting = job->getAccountingInfo();
    TEST_ASSERT(accounting.TotalProcesses >= 3, "Accounting TotalProcesses must reflect members");
    TEST_ASSERT(accounting.TotalTerminatedProcesses >= 3, "Accounting TotalTerminatedProcesses must reflect killed processes");
}

// ============================================================================
// Suite 53: PolarisDiag Sovereign Crash Diagnostics & Minidump Engine Tests
// ============================================================================
void Test_PolarisDiag_CrashDump_And_MinidumpWriter() {
    using namespace micant::polaris;

    auto& diagEngine = PolarisDiagnosticEngine::get();

    // 1. Prepare Crash Report
    CrashReport report{};
    report.bugCheckCode = ke::PAGE_FAULT_IN_NONPAGED_AREA;
    report.bugCheckName = std::string(PolarisDiagnosticEngine::getBugCheckName(report.bugCheckCode));
    report.param1 = 0x00007FFDF0001000ULL; // Faulting virtual address
    report.param2 = 0x0;                   // Read operation
    report.param3 = 0x00007FF614002340ULL; // Instruction pointer (RIP)
    report.param4 = 0x0;
    report.faultingModule = L"vanguard.sys";

    report.context.rip = 0x00007FF614002340ULL;
    report.context.rsp = 0x00007FFFFFFFDE00ULL;
    report.context.rbp = 0x00007FFFFFFFDE80ULL;
    report.context.rax = 0x0000000000000042ULL;
    report.context.rcx = 0x00007FFDF0001000ULL;
    report.context.rdx = 0x0000000000000100ULL;
    report.context.rflags = 0x202;

    // Loaded Kernel Modules
    std::vector<LoadedModuleDesc> modules = {
        { L"micant_kernel.exe", 0x00007FF614000000ULL, 0x180000 },
        { L"vanguard.sys",      0x00007FFF80000000ULL, 0x040000 },
        { L"emeraldfs.sys",     0x00007FFF80050000ULL, 0x060000 },
        { L"hal.dll",           0x00007FFF80100000ULL, 0x030000 }
    };

    // 2. Generate 64-bit Minidump (.dmp) Binary Stream
    std::vector<uint8_t> dump = diagEngine.generateMinidump(report, modules);
    TEST_ASSERT(!dump.empty(), "Minidump generator must return non-empty byte buffer");
    TEST_ASSERT(dump.size() >= sizeof(MINIDUMP_HEADER), "Minidump must exceed header size");

    // 3. Self-Parse & Validate Minidump Structs (WinDbg Parity)
    MinidumpSummary summary = diagEngine.parseMinidump(dump);
    TEST_ASSERT(summary.isValid, "Minidump validation must succeed");
    TEST_ASSERT(summary.version == MINIDUMP_VERSION, "Minidump version must match MINIDUMP_VERSION");
    TEST_ASSERT(summary.streamCount == 6, "Minidump must contain 6 distinct stream directories");

    // Verify stream types present
    auto hasStream = [&](uint32_t type) {
        return std::find(summary.streamTypes.begin(), summary.streamTypes.end(), type) != summary.streamTypes.end();
    };
    TEST_ASSERT(hasStream(SystemInfoStream), "Minidump must contain SystemInfoStream");
    TEST_ASSERT(hasStream(ExceptionStream), "Minidump must contain ExceptionStream");
    TEST_ASSERT(hasStream(ModuleListStream), "Minidump must contain ModuleListStream");
    TEST_ASSERT(hasStream(ThreadListStream), "Minidump must contain ThreadListStream");
    TEST_ASSERT(hasStream(MiscInfoStream), "Minidump must contain MiscInfoStream");
    TEST_ASSERT(hasStream(CommentStreamA), "Minidump must contain CommentStreamA");

    // Verify exception and register context accuracy
    TEST_ASSERT(summary.exceptionCode == ke::PAGE_FAULT_IN_NONPAGED_AREA, "Exception code in dump must match bug check");
    TEST_ASSERT(summary.exceptionAddress == 0x00007FF614002340ULL, "Exception address must match faulting RIP");
    TEST_ASSERT(summary.parameters[0] == 0x00007FFDF0001000ULL, "Parameter 1 must match faulting address");
    TEST_ASSERT(summary.rip == 0x00007FF614002340ULL, "Context RIP in dump must match report");
    TEST_ASSERT(summary.rsp == 0x00007FFFFFFFDE00ULL, "Context RSP in dump must match report");

    // Verify module list parsing
    TEST_ASSERT(summary.moduleNames.size() == 4, "Dump module list must parse all 4 modules");
    TEST_ASSERT(summary.moduleNames[0] == L"micant_kernel.exe", "Module 0 must be micant_kernel.exe");
    TEST_ASSERT(summary.moduleNames[1] == L"vanguard.sys", "Module 1 must be vanguard.sys");

    // Verify zero-telemetry sovereign comment
    TEST_ASSERT(summary.comment.find("Telemetry-Free") != std::string::npos, "Minidump must contain sovereign zero-telemetry notice");

    // 4. Panic Screen Rendering Verification
    std::string panicText = diagEngine.renderPanicScreenText(report);
    TEST_ASSERT(panicText.find("PAGE_FAULT_IN_NONPAGED_AREA") != std::string::npos, "Panic screen must display stop code name");
    TEST_ASSERT(panicText.find("vanguard.sys") != std::string::npos, "Panic screen must display faulting module");
    TEST_ASSERT(panicText.find("Telemetry:    DISABLED") != std::string::npos, "Panic screen must confirm zero telemetry");

    // Framebuffer rendering
    std::vector<uint32_t> fb(800 * 600, 0);
    diagEngine.renderPanicScreenFramebuffer(fb.data(), 800, 600, report);
    TEST_ASSERT(fb[0] == 0xFF0078D7, "Framebuffer must be cleared to Sovereign Azure #0078D7");

    // 5. Canonical BugCheck Stop Code Mappings
    TEST_ASSERT(PolarisDiagnosticEngine::getBugCheckName(ke::IRQL_NOT_LESS_OR_EQUAL) == "IRQL_NOT_LESS_OR_EQUAL", "IRQL stop code mapping");
    TEST_ASSERT(PolarisDiagnosticEngine::getBugCheckName(ke::CRITICAL_PROCESS_DIED) == "CRITICAL_PROCESS_DIED", "CRITICAL_PROCESS_DIED mapping");
    TEST_ASSERT(PolarisDiagnosticEngine::getBugCheckName(0x0000007B) == "INACCESSIBLE_BOOT_DEVICE", "INACCESSIBLE_BOOT_DEVICE mapping");
}

// ============================================================================
// Suite 54: CipherKSP Sovereign Cryptographic Services & CNG Engine Tests
// ============================================================================
void Test_CipherKSP_CryptographicServices_And_AES() {
    using namespace micant::crypto;

    auto toHex = [](std::span<const uint8_t> bytes) -> std::string {
        std::ostringstream oss;
        for (uint8_t b : bytes) {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
        }
        return oss.str();
    };

    // 1. FIPS 180-4 SHA-256 Official NIST Test Vectors
    {
        // Vector 1: Empty string ""
        auto hashEmpty = Sha256::hash(std::span<const uint8_t>{});
        TEST_ASSERT(toHex(hashEmpty) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
                    "NIST SHA-256 Empty Vector Verification");

        // Vector 2: "abc"
        std::string abc = "abc";
        auto hashAbc = Sha256::hash(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(abc.data()), abc.size()));
        TEST_ASSERT(toHex(hashAbc) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                    "NIST SHA-256 'abc' Vector Verification");

        // Vector 3: 56-byte string
        std::string str56 = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
        auto hash56 = Sha256::hash(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(str56.data()), str56.size()));
        TEST_ASSERT(toHex(hash56) == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
                    "NIST SHA-256 56-byte Vector Verification");
    }

    // 2. RFC 4231 HMAC-SHA256 Test Vectors
    {
        // RFC 4231 Case 1: Key = 20x 0x0b, Data = "Hi There"
        std::vector<uint8_t> key1(20, 0x0b);
        std::string data1 = "Hi There";
        auto hmac1 = HmacSha256::compute(
            key1,
            std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(data1.data()), data1.size())
        );
        TEST_ASSERT(toHex(hmac1) == "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7",
                    "RFC 4231 HMAC-SHA256 Case 1 Verification");

        // RFC 4231 Case 2: Key = "Jefe", Data = "what do ya want for nothing?"
        std::string key2 = "Jefe";
        std::string data2 = "what do ya want for nothing?";
        auto hmac2 = HmacSha256::compute(
            std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(key2.data()), key2.size()),
            std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(data2.data()), data2.size())
        );
        TEST_ASSERT(toHex(hmac2) == "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843",
                    "RFC 4231 HMAC-SHA256 Case 2 Verification");
    }

    // 3. FIPS 197 AES-128 Electronic Codebook (ECB) NIST Test Vector
    {
        // FIPS 197 Appendix C.1: AES-128
        // Key: 2b 7e 15 16 28 ae d2 a6 ab f7 15 88 09 cf 4f 3c
        // In:  6b c1 be e2 2e 40 9f 96 e9 3d 7e 11 73 93 17 2a
        // Out: 3a d7 7b b4 0d 7a 36 60 a8 9e ca f3 24 66 ef 97
        const uint8_t keyBytes[16] = {
            0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
            0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
        };
        const uint8_t plainBytes[16] = {
            0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
            0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a
        };

        Aes aes(keyBytes);
        uint8_t cipherBlock[16] = {0};
        aes.encryptBlock(plainBytes, cipherBlock);

        TEST_ASSERT(toHex(cipherBlock) == "3ad77bb40d7a3660a89ecaf32466ef97",
                    "FIPS 197 AES-128 ECB Block Encryption Verification");

        uint8_t decryptedBlock[16] = {0};
        aes.decryptBlock(cipherBlock, decryptedBlock);
        TEST_ASSERT(std::memcmp(plainBytes, decryptedBlock, 16) == 0,
                    "FIPS 197 AES-128 ECB Block Decryption Verification");
    }

    // 4. AES-256 CBC Mode with IV Chaining and PKCS#7 Padding
    {
        const uint8_t key256[32] = {
            0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe,
            0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81,
            0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61, 0x08, 0xd7,
            0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4
        };
        const uint8_t iv[16] = {
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
        };

        std::string secretMessage = "Project MicaNT Sovereign Cryptography Next Generation Engine";
        Aes aes256(key256);

        auto ciphertext = aes256.encrypt(
            std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(secretMessage.data()), secretMessage.size()),
            Aes::Mode::CBC,
            iv,
            true // PKCS#7 padding
        );

        TEST_ASSERT(ciphertext.size() > secretMessage.size(), "Ciphertext size must include PKCS#7 padding");
        TEST_ASSERT(ciphertext.size() % 16 == 0, "Ciphertext size must be a multiple of 16");

        bool ok = false;
        auto decrypted = aes256.decrypt(
            ciphertext,
            Aes::Mode::CBC,
            iv,
            true,
            &ok
        );

        TEST_ASSERT(ok, "AES-256 CBC decryption must report success");
        std::string decryptedMessage(decrypted.begin(), decrypted.end());
        TEST_ASSERT(decryptedMessage == secretMessage, "AES-256 CBC roundtrip must match original plaintext");

        // Verify tampering detection with PKCS#7
        ciphertext.back() ^= 0xFF; // Corrupt padding byte
        bool tamperOk = false;
        aes256.decrypt(ciphertext, Aes::Mode::CBC, iv, true, &tamperOk);
        TEST_ASSERT(!tamperOk, "Tampered ciphertext must fail PKCS#7 padding validation");
    }

    // 5. RFC 6070 PBKDF2-HMAC-SHA256 Test Vectors
    {
        std::string pass = "password";
        std::string salt = "salt";

        // Iterations = 1
        auto dk1 = Pbkdf2::derive(
            std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(pass.data()), pass.size()),
            std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(salt.data()), salt.size()),
            1, 32
        );
        TEST_ASSERT(toHex(dk1) == "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b",
                    "RFC 6070 PBKDF2 Iteration 1 Verification");

        // Iterations = 2
        auto dk2 = Pbkdf2::derive(
            std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(pass.data()), pass.size()),
            std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(salt.data()), salt.size()),
            2, 32
        );
        TEST_ASSERT(toHex(dk2) == "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43",
                    "RFC 6070 PBKDF2 Iteration 2 Verification");
    }

    // 6. CSPRNG Randomness & Entropy Verification
    {
        std::array<uint8_t, 32> randomBuf1{};
        std::array<uint8_t, 32> randomBuf2{};

        NTSTATUS status1 = BCryptGenRandom(nullptr, randomBuf1.data(), static_cast<uint32_t>(randomBuf1.size()), 0);
        NTSTATUS status2 = BCryptGenRandom(nullptr, randomBuf2.data(), static_cast<uint32_t>(randomBuf2.size()), 0);

        TEST_ASSERT(status1 == STATUS_SUCCESS, "BCryptGenRandom must succeed");
        TEST_ASSERT(status2 == STATUS_SUCCESS, "BCryptGenRandom must succeed on successive calls");
        TEST_ASSERT(randomBuf1 != randomBuf2, "CSPRNG must generate non-repeating entropy streams");

        bool nonZero = false;
        for (uint8_t b : randomBuf1) {
            if (b != 0) { nonZero = true; break; }
        }
        TEST_ASSERT(nonZero, "CSPRNG output buffer must contain non-zero entropy");
    }

    // 7. Windows CNG (BCrypt) High-Level API Lifecycle
    {
        BCRYPT_ALG_HANDLE hAesAlg = nullptr;
        NTSTATUS status = BCryptOpenAlgorithmProvider(&hAesAlg, BCRYPT_AES_ALGORITHM, nullptr, 0);
        TEST_ASSERT(status == STATUS_SUCCESS && hAesAlg != nullptr, "BCryptOpenAlgorithmProvider for AES must succeed");

        // Verify property retrieval
        uint32_t blockLen = 0;
        uint32_t resultLen = 0;
        status = BCryptGetProperty(hAesAlg, BCRYPT_BLOCK_LENGTH, reinterpret_cast<uint8_t*>(&blockLen), sizeof(blockLen), &resultLen, 0);
        TEST_ASSERT(status == STATUS_SUCCESS && blockLen == 16, "BCryptGetProperty for BlockLength must return 16");

        // Generate Symmetric Key Handle
        uint8_t aesKeySecret[16] = {
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
            0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10
        };
        BCRYPT_KEY_HANDLE hKey = nullptr;
        status = BCryptGenerateSymmetricKey(hAesAlg, &hKey, nullptr, 0, aesKeySecret, sizeof(aesKeySecret), 0);
        TEST_ASSERT(status == STATUS_SUCCESS && hKey != nullptr, "BCryptGenerateSymmetricKey must return valid key handle");

        // Encrypt & Decrypt via CNG API
        std::string plain = "CNG-Sovereign-Message";
        uint8_t iv[16] = { 0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0xEE, 0xFF };
        uint8_t ivDec[16];
        std::memcpy(ivDec, iv, 16);

        uint32_t cipherLen = 0;
        status = BCryptEncrypt(hKey, reinterpret_cast<uint8_t*>(plain.data()), static_cast<uint32_t>(plain.size()), nullptr, iv, sizeof(iv), nullptr, 0, &cipherLen, BCRYPT_BLOCK_PADDING);
        TEST_ASSERT(status == STATUS_SUCCESS && cipherLen >= plain.size(), "BCryptEncrypt sizing probe must succeed");

        std::vector<uint8_t> cipherBuf(cipherLen);
        status = BCryptEncrypt(hKey, reinterpret_cast<uint8_t*>(plain.data()), static_cast<uint32_t>(plain.size()), nullptr, iv, sizeof(iv), cipherBuf.data(), cipherLen, &cipherLen, BCRYPT_BLOCK_PADDING);
        TEST_ASSERT(status == STATUS_SUCCESS, "BCryptEncrypt payload execution must succeed");

        uint32_t decLen = 0;
        status = BCryptDecrypt(hKey, cipherBuf.data(), cipherLen, nullptr, ivDec, sizeof(ivDec), nullptr, 0, &decLen, BCRYPT_BLOCK_PADDING);
        TEST_ASSERT(status == STATUS_SUCCESS && decLen == plain.size(), "BCryptDecrypt sizing probe must match plaintext length");

        std::vector<uint8_t> decBuf(decLen);
        status = BCryptDecrypt(hKey, cipherBuf.data(), cipherLen, nullptr, ivDec, sizeof(ivDec), decBuf.data(), decLen, &decLen, BCRYPT_BLOCK_PADDING);
        TEST_ASSERT(status == STATUS_SUCCESS, "BCryptDecrypt payload execution must succeed");

        std::string recovered(decBuf.begin(), decBuf.end());
        TEST_ASSERT(recovered == plain, "BCrypt roundtrip plaintext must match original message");

        // Clean up key and algorithm
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAesAlg, 0);

        // Test CNG Hash API
        BCRYPT_ALG_HANDLE hShaAlg = nullptr;
        BCryptOpenAlgorithmProvider(&hShaAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
        BCRYPT_HASH_HANDLE hHash = nullptr;
        BCryptCreateHash(hShaAlg, &hHash, nullptr, 0, nullptr, 0, 0);

        std::string hashInput = "MicaNT-CNG-Hash";
        BCryptHashData(hHash, reinterpret_cast<uint8_t*>(hashInput.data()), static_cast<uint32_t>(hashInput.size()), 0);

        uint8_t digest[32];
        BCryptFinishHash(hHash, digest, sizeof(digest), 0);
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hShaAlg, 0);

        auto directHash = Sha256::hash(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(hashInput.data()), hashInput.size()));
        TEST_ASSERT(std::memcmp(digest, directHash.data(), 32) == 0, "CNG Hash API digest must match direct Sha256 digest");
    }

    // 8. Key Storage Provider (NCrypt) Lifecycle
    {
        NCRYPT_PROV_HANDLE hStorage = nullptr;
        NTSTATUS status = NCryptOpenStorageProvider(&hStorage, MS_KEY_STORAGE_PROVIDER, 0);
        TEST_ASSERT(status == STATUS_SUCCESS && hStorage != nullptr, "NCryptOpenStorageProvider must succeed");

        NCRYPT_KEY_HANDLE hPersisted = nullptr;
        status = NCryptCreatePersistedKey(hStorage, &hPersisted, BCRYPT_AES_ALGORITHM, L"SovereignAdminMasterKey", 0, 0);
        TEST_ASSERT(status == STATUS_SUCCESS && hPersisted != nullptr, "NCryptCreatePersistedKey must succeed");

        status = NCryptFinalizeKey(hPersisted, 0);
        TEST_ASSERT(status == STATUS_SUCCESS, "NCryptFinalizeKey must succeed and populate random master key");

        // Re-open persisted key by name
        NCRYPT_KEY_HANDLE hOpened = nullptr;
        status = NCryptOpenKey(hStorage, &hOpened, L"SovereignAdminMasterKey", 0, 0);
        TEST_ASSERT(status == STATUS_SUCCESS && hOpened != nullptr, "NCryptOpenKey must find persisted key by name");

        // Export key secret
        uint32_t exportSize = 0;
        status = NCryptExportKey(hOpened, nullptr, BCRYPT_OPAQUE_KEY_BLOB, nullptr, nullptr, 0, &exportSize, 0);
        TEST_ASSERT(status == STATUS_SUCCESS && exportSize == 32, "NCryptExportKey sizing probe must report 32 bytes");

        std::vector<uint8_t> exportedSecret(exportSize);
        status = NCryptExportKey(hOpened, nullptr, BCRYPT_OPAQUE_KEY_BLOB, nullptr, exportedSecret.data(), exportSize, &exportSize, 0);
        TEST_ASSERT(status == STATUS_SUCCESS, "NCryptExportKey must successfully export raw key data");

        // Delete key
        status = NCryptDeleteKey(hOpened, 0);
        TEST_ASSERT(status == STATUS_SUCCESS, "NCryptDeleteKey must succeed");

        NCryptFreeObject(hStorage);
    }
}

// ============================================================================
// Suite 55: JanusLDR Sovereign Dynamic Linker & Delay-Load Gateway Tests
// ============================================================================
void Test_JanusLDR_DelayLoadThunks_And_SxSManifest() {
    using namespace micant::janus;
    using namespace micant::ldr;

    auto& linker = JanusDynamicLinker::get();

    // 1. Delay-Load Helper Thunk Resolution (__delayLoadHelper2)
    {
        // Setup mock target function in dxgi.dll
        void* pfnCreateDXGIFactory1 = reinterpret_cast<void*>(0x00007FF8B0001234ULL);
        DynamicLoader::get().registerExport("dxgi.dll", "CreateDXGIFactory1", pfnCreateDXGIFactory1);

        // Prepare simulated image memory buffer containing delay descriptor & tables
        std::vector<uint8_t> mockImage(4096, 0);
        uintptr_t imageBase = reinterpret_cast<uintptr_t>(mockImage.data());

        // Offset 0x100: DLL Name "dxgi.dll"
        const char dllNameStr[] = "dxgi.dll";
        uint32_t rvaDLLName = 0x100;
        std::memcpy(&mockImage[rvaDLLName], dllNameStr, sizeof(dllNameStr));

        // Offset 0x200: HMODULE storage (uintptr_t)
        uint32_t rvaHmod = 0x200;

        // Offset 0x300: IAT entry table
        uint32_t rvaIAT = 0x300;
        void** pIAT = reinterpret_cast<void**>(&mockImage[rvaIAT]);
        pIAT[0] = reinterpret_cast<void*>(0xDEADBEEF); // Initial unthunked placeholder

        // Offset 0x400: Import Name Table (INT) pointing to Hint/Name at 0x500
        uint32_t rvaINT = 0x400;
        uintptr_t* pINT = reinterpret_cast<uintptr_t*>(&mockImage[rvaINT]);
        pINT[0] = 0x500; // RVA to Hint/Name

        // Offset 0x500: Hint (2 bytes) + "CreateDXGIFactory1\0"
        uint32_t rvaHintName = 0x500;
        mockImage[rvaHintName] = 0x01; // Hint low
        mockImage[rvaHintName + 1] = 0x00; // Hint high
        const char procNameStr[] = "CreateDXGIFactory1";
        std::memcpy(&mockImage[rvaHintName + 2], procNameStr, sizeof(procNameStr));

        // Build Delay Descriptor
        ImgDelayDescr desc{};
        desc.grAttrs = DLATTR_RVA;
        desc.rvaDLLName = rvaDLLName;
        desc.rvaHmod = rvaHmod;
        desc.rvaIAT = rvaIAT;
        desc.rvaINT = rvaINT;

        // Hook tracking
        std::vector<uint32_t> notifications;
        auto testHook = [](uint32_t dliNotify, DelayLoadInfo* pdli) -> void* {
            static std::vector<uint32_t>* pNotifs = nullptr;
            if (pdli && pdli->dwLastError == 0x1337) {
                pNotifs = reinterpret_cast<std::vector<uint32_t>*>(pdli->pfnCur);
            }
            if (pNotifs) {
                pNotifs->push_back(dliNotify);
            }
            return nullptr;
        };

        // Pass hook tracker
        DelayLoadInfo dummy{};
        dummy.dwLastError = 0x1337;
        dummy.pfnCur = &notifications;
        testHook(0, &dummy);

        void** targetIATEntry = &pIAT[0];
        void* resolved = linker.resolveDelayLoad(imageBase, &desc, targetIATEntry, testHook);

        TEST_ASSERT(resolved == pfnCreateDXGIFactory1, "Delay-load helper must resolve exact target function address");
        TEST_ASSERT(*targetIATEntry == pfnCreateDXGIFactory1, "IAT entry must be patched directly to target address");
        TEST_ASSERT(*reinterpret_cast<uintptr_t*>(&mockImage[rvaHmod]) != 0, "Descriptor HMODULE slot must be populated with loaded base");

        // Verify hook notifications fired in correct order
        TEST_ASSERT(std::find(notifications.begin(), notifications.end(), dliStartProcessing) != notifications.end(), "dliStartProcessing must fire");
        TEST_ASSERT(std::find(notifications.begin(), notifications.end(), dliNoteEndProcessing) != notifications.end(), "dliNoteEndProcessing must fire");
    }

    // 2. Export Forwarder Chain Resolution
    {
        void* pfnRtlEnterCritSec = reinterpret_cast<void*>(0x00007FF800054321ULL);
        DynamicLoader::get().registerExport("ntdll.dll", "RtlEnterCriticalSection", pfnRtlEnterCritSec);

        // Forward kernel32!EnterCriticalSection -> NTDLL.RtlEnterCriticalSection
        linker.registerForwarder("kernel32.dll", "EnterCriticalSection", "ntdll.dll.RtlEnterCriticalSection");

        // Multi-hop: api-ms-win-core-synch-l1-1-0.dll!EnterCriticalSection -> kernel32.dll.EnterCriticalSection
        linker.registerForwarder("api-ms-win-core-synch-l1-1-0.dll", "EnterCriticalSection", "kernel32.dll.EnterCriticalSection");

        // Test single-hop resolution
        void* resolvedSingle = linker.resolveExport("kernel32.dll", "EnterCriticalSection");
        TEST_ASSERT(resolvedSingle == pfnRtlEnterCritSec, "Single-hop forwarder must resolve to target NTDLL export");

        // Test multi-hop forwarder chain resolution
        void* resolvedMulti = linker.resolveExport("api-ms-win-core-synch-l1-1-0.dll", "EnterCriticalSection");
        TEST_ASSERT(resolvedMulti == pfnRtlEnterCritSec, "Multi-hop forwarder chain must resolve seamlessly to leaf implementation");

        // Test forwarder parser on ordinal forward
        auto ordTarget = ForwarderResolver::parseForwarderString("user32.dll.#42");
        TEST_ASSERT(ordTarget.has_value(), "Forwarder resolver must parse ordinal forward target");
        TEST_ASSERT(ordTarget->isOrdinal && ordTarget->ordinal == 42, "Parsed ordinal forward must extract #42");
        TEST_ASSERT(ordTarget->moduleName == "user32.dll", "Parsed ordinal forward must identify user32.dll");
    }

    // 3. Side-by-Side (SxS) Manifest Parsing
    {
        std::string manifestXml = R"(
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <assemblyIdentity type="win32" name="MicaNT.SovereignApp" version="1.2.3.4" processorArchitecture="amd64" publicKeyToken="12345678abcdef00" />
  <dependency>
    <dependentAssembly>
      <assemblyIdentity type="win32" name="Microsoft.Windows.Common-Controls" version="6.0.0.0" publicKeyToken="6595b64144ccf1df" />
    </dependentAssembly>
  </dependency>
  <file name="comctl32.dll" />
</assembly>
)";

        auto actCtx = ActivationContext::parseFromXml(manifestXml, L"C:\\Apps\\SovereignApp.exe.manifest");
        TEST_ASSERT(actCtx != nullptr, "ActivationContext::parseFromXml must return valid context");
        TEST_ASSERT(actCtx->identity.name == L"MicaNT.SovereignApp", "Parsed assembly identity name must match manifest");
        TEST_ASSERT(actCtx->identity.version == L"1.2.3.4", "Parsed assembly identity version must match manifest");
        TEST_ASSERT(actCtx->dependencies.size() == 1, "Parsed dependencies count must be 1");
        TEST_ASSERT(actCtx->dependencies[0].name == L"Microsoft.Windows.Common-Controls", "Dependent assembly name must match Common-Controls");
        TEST_ASSERT(actCtx->dependencies[0].version == L"6.0.0.0", "Dependent assembly version must match 6.0.0.0");
        TEST_ASSERT(actCtx->fileRedirections.find(L"comctl32.dll") != actCtx->fileRedirections.end(), "File redirection for comctl32.dll must be registered");

        // 4. Win32 Activation Context Stack Lifecycle
        uintptr_t hActCtx = linker.registerParsedContext(actCtx);
        TEST_ASSERT(hActCtx != 0, "Context registration must yield non-zero handle");

        uintptr_t cookie = 0;
        bool activated = ActivateActCtx(hActCtx, &cookie);
        TEST_ASSERT(activated && cookie != 0, "ActivateActCtx must successfully push context onto stack");

        auto active = linker.getActiveContext();
        TEST_ASSERT(active != nullptr && active->identity.name == L"MicaNT.SovereignApp", "getActiveContext must retrieve current top of stack");

        auto redir = linker.resolveAssemblyRedirection(L"comctl32.dll");
        TEST_ASSERT(redir.has_value() && *redir == L"comctl32.dll", "Assembly redirection must resolve active manifest files");

        bool deactivated = DeactivateActCtx(0, cookie);
        TEST_ASSERT(deactivated, "DeactivateActCtx with matching cookie must succeed");
        TEST_ASSERT(linker.getActiveContext() == nullptr, "Context stack must be empty after popping cookie");

        ReleaseActCtx(hActCtx);
    }
}

void Test_User32_WindowManager_SwapchainPresentation_And_DirectInput() {
    using namespace micant::user32;
    using namespace micant::dinput;
    using namespace micant::prismx;

    // 0. Initialize User32 and DirectInput exports
    InitializeUser32SubsystemExports();
    InitializeDirectInputSubsystemExports();

    // Tracking state for custom WindowProc
    static std::vector<uint32_t> receivedMessages;
    static uint32_t lastChar = 0;
    static uint32_t lastKey = 0;
    receivedMessages.clear();
    lastChar = 0;
    lastKey = 0;

    auto testWndProc = [](win32::HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) -> LRESULT {
        receivedMessages.push_back(uMsg);
        if (uMsg == WM_CHAR) {
            lastChar = static_cast<uint32_t>(wParam);
        } else if (uMsg == WM_KEYDOWN) {
            lastKey = static_cast<uint32_t>(wParam);
        }
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    };

    // 1. Register Window Class
    WNDCLASSEXW wcex{};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.lpfnWndProc = testWndProc;
    wcex.lpszClassName = L"MicaNT_GameEngineWindow";
    uint16_t atom = RegisterClassExW(&wcex);
    TEST_ASSERT(atom != 0, "RegisterClassExW must return valid non-zero class atom");

    WNDCLASSEXW queryWcex{};
    bool clsFound = GetClassInfoExW(nullptr, L"MicaNT_GameEngineWindow", &queryWcex);
    TEST_ASSERT(clsFound && queryWcex.lpfnWndProc == testWndProc, "GetClassInfoExW must retrieve registered window class");

    // 2. Window Creation and Hierarchy
    win32::HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"MicaNT_GameEngineWindow",
        L"MicaNT Sovereign 3D Game Engine",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        50, 50, 640, 480,
        nullptr, nullptr, nullptr, nullptr
    );
    TEST_ASSERT(hwnd != nullptr, "CreateWindowExW must return valid HWND");
    TEST_ASSERT(IsWindow(hwnd), "IsWindow must verify allocated HWND");
    TEST_ASSERT(IsWindowVisible(hwnd), "Window created with WS_VISIBLE must report visible");

    // Verify WM_CREATE was dispatched
    TEST_ASSERT(std::find(receivedMessages.begin(), receivedMessages.end(), WM_CREATE) != receivedMessages.end(), "WM_CREATE must be dispatched on window creation");

    // Test Geometry & Client Rect
    RECT clientRc{};
    GetClientRect(hwnd, &clientRc);
    TEST_ASSERT((clientRc.right - clientRc.left) == 640 && (clientRc.bottom - clientRc.top) == 480, "GetClientRect must reflect 640x480 client surface");

    RECT windowRc{};
    GetWindowRect(hwnd, &windowRc);
    TEST_ASSERT(windowRc.left == 50 && windowRc.top == 50 && windowRc.right == 690 && windowRc.bottom == 530, "GetWindowRect must reflect window coordinates");

    // Test Window Long Pointers (GWLP_USERDATA)
    uintptr_t magicUserData = 0xCAFEBABE1337BEEFULL;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, magicUserData);
    TEST_ASSERT(GetWindowLongPtrW(hwnd, GWLP_USERDATA) == magicUserData, "GWLP_USERDATA must persist and round-trip through GetWindowLongPtrW");

    // Drain initial window initialization messages (WM_SIZE, WM_PAINT)
    MSG initMsg{};
    while (PeekMessageW(&initMsg, hwnd, 0, 0, PM_REMOVE)) {
        DispatchMessageW(&initMsg);
    }
    TEST_ASSERT(std::find(receivedMessages.begin(), receivedMessages.end(), WM_SIZE) != receivedMessages.end(), "WM_SIZE must be dispatched on window layout");
    TEST_ASSERT(std::find(receivedMessages.begin(), receivedMessages.end(), WM_PAINT) != receivedMessages.end(), "WM_PAINT must be dispatched on initial presentation");

    // 3. Message Queue & Message Pump (Peek, Translate, Dispatch)
    PostMessageW(hwnd, WM_KEYDOWN, 0x41 /* 'A' */, 0);

    MSG msg{};
    bool hasMsg = PeekMessageW(&msg, hwnd, 0, 0, PM_REMOVE);
    TEST_ASSERT(hasMsg && msg.message == WM_KEYDOWN && msg.wParam == 0x41, "PeekMessageW must pull posted WM_KEYDOWN from queue");

    // TranslateMessage must synthesize WM_CHAR for printable character
    bool translated = TranslateMessage(&msg);
    TEST_ASSERT(translated, "TranslateMessage must synthesize WM_CHAR for key 0x41 ('A')");

    // Dispatch WM_KEYDOWN
    DispatchMessageW(&msg);
    TEST_ASSERT(lastKey == 0x41, "DispatchMessageW must execute window procedure for WM_KEYDOWN");

    // Peek and dispatch the translated WM_CHAR
    bool hasCharMsg = PeekMessageW(&msg, hwnd, 0, 0, PM_REMOVE);
    TEST_ASSERT(hasCharMsg && msg.message == WM_CHAR && msg.wParam == 0x41, "PeekMessageW must retrieve synthesized WM_CHAR");
    DispatchMessageW(&msg);
    TEST_ASSERT(lastChar == 0x41, "DispatchMessageW must execute window procedure for WM_CHAR");

    // Test PostQuitMessage and GetMessage termination
    PostQuitMessage(42);
    bool getRes = GetMessageW(&msg, nullptr, 0, 0);
    TEST_ASSERT(!getRes && msg.message == WM_QUIT && msg.wParam == 42, "GetMessageW must return FALSE upon retrieving WM_QUIT");

    // 4. DXGI SwapChain Backbuffer Presentation Bridge to HWND
    {
        DXGI_SWAP_CHAIN_DESC scDesc{};
        scDesc.BufferDesc.Width = 640;
        scDesc.BufferDesc.Height = 480;
        scDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        scDesc.BufferCount = 2;
        scDesc.OutputWindow = hwnd;
        scDesc.Windowed = 1;
        scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

        PrismXSwapChainImpl swapChain(scDesc);

        // Fetch backbuffer 0
        void* pBuf = nullptr;
        int32_t hr = swapChain.GetBuffer(0, IID_IDXGISurface, &pBuf);
        TEST_ASSERT(hr == 0 && pBuf != nullptr, "swapChain.GetBuffer must retrieve surface pointer");

        auto* surface = static_cast<PrismXSurfaceImpl*>(pBuf);

        // Write distinct pixel pattern to swapchain backbuffer
        uint32_t* pPixels = reinterpret_cast<uint32_t*>(surface->GetRawData());
        uint32_t testColor = 0xFF55AAEE; // ABGR test color
        std::fill(pPixels, pPixels + (640 * 480), testColor);

        // Present frame
        int32_t presentHr = swapChain.Present(1, 0);
        TEST_ASSERT(presentHr == 0, "swapChain.Present must return 0 (S_OK)");

        // Verify window's surface pixels now contain the presented backbuffer
        uint32_t surfW = 0, surfH = 0;
        const uint32_t* windowPixels = WindowManager::get().getWindowPixelBuffer(hwnd, &surfW, &surfH);
        TEST_ASSERT(windowPixels != nullptr && surfW == 640 && surfH == 480, "Window surface buffer must match backbuffer dimensions");
        TEST_ASSERT(windowPixels[0] == testColor && windowPixels[640 * 240 + 320] == testColor, "Window surface must contain exact presented backbuffer pixel data");

        surface->Release();
    }

    // 5. Win32 Raw Input API
    {
        RAWINPUTDEVICE rid{};
        rid.usUsagePage = 0x01; // Generic Desktop Controls
        rid.usUsage = 0x02;     // Mouse
        rid.dwFlags = RIDEV_INPUTSINK;
        rid.hwndTarget = hwnd;

        bool regOk = RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE));
        TEST_ASSERT(regOk, "RegisterRawInputDevices must succeed");

        // Inject raw mouse motion & left button down
        HRAWINPUT hRaw = WindowManager::get().injectRawMouse(hwnd, 25, -12, RI_MOUSE_LEFT_BUTTON_DOWN);
        TEST_ASSERT(hRaw != nullptr, "injectRawMouse must generate non-null HRAWINPUT");

        // Peek WM_INPUT message
        MSG rawMsg{};
        bool gotRaw = PeekMessageW(&rawMsg, hwnd, WM_INPUT, WM_INPUT, PM_REMOVE);
        TEST_ASSERT(gotRaw && rawMsg.message == WM_INPUT, "PeekMessageW must retrieve WM_INPUT");

        // Query raw input header
        RAWINPUTHEADER hdr{};
        UINT hdrSize = sizeof(RAWINPUTHEADER);
        UINT getHdr = GetRawInputData(reinterpret_cast<HRAWINPUT>(rawMsg.lParam), RID_HEADER, &hdr, &hdrSize, sizeof(RAWINPUTHEADER));
        TEST_ASSERT(getHdr == sizeof(RAWINPUTHEADER) && hdr.dwType == RIM_TYPEMOUSE, "GetRawInputData RID_HEADER must return RIM_TYPEMOUSE");

        // Query raw input full packet
        RAWINPUT fullInput{};
        UINT fullSize = sizeof(RAWINPUT);
        UINT getFull = GetRawInputData(reinterpret_cast<HRAWINPUT>(rawMsg.lParam), RID_INPUT, &fullInput, &fullSize, sizeof(RAWINPUTHEADER));
        TEST_ASSERT(getFull == sizeof(RAWINPUT), "GetRawInputData RID_INPUT must return full RAWINPUT size");
        TEST_ASSERT(fullInput.data.mouse.lLastX == 25 && fullInput.data.mouse.lLastY == -12, "Raw mouse delta coordinates must match injected motion");
        TEST_ASSERT((fullInput.data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN) != 0, "Raw mouse button down flag must be set");
    }

    // 6. DirectInput 8 Subsystem (IDirectInput8W & IDirectInputDevice8W)
    {
        void* pDIObj = nullptr;
        int32_t diHr = DirectInput8Create(nullptr, DIRECTINPUT_VERSION, IID_IDirectInput8W, &pDIObj, nullptr);
        TEST_ASSERT(diHr == DI_OK && pDIObj != nullptr, "DirectInput8Create must return DI_OK with valid interface");

        auto* pDI = static_cast<IDirectInput8W*>(pDIObj);

        // Create DirectInput Mouse Device
        IDirectInputDevice8W* pMouseDev = nullptr;
        int32_t devHr = pDI->CreateDevice(GUID_SysMouse, &pMouseDev, nullptr);
        TEST_ASSERT(devHr == DI_OK && pMouseDev != nullptr, "CreateDevice for GUID_SysMouse must succeed");

        DIDATAFORMAT mouseDf{};
        mouseDf.dwDataSize = sizeof(DIMOUSESTATE);
        TEST_ASSERT(pMouseDev->SetDataFormat(&mouseDf) == DI_OK, "SetDataFormat on mouse device must succeed");
        TEST_ASSERT(pMouseDev->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE) == DI_OK, "SetCooperativeLevel must succeed");
        TEST_ASSERT(pMouseDev->Acquire() == DI_OK, "Acquire on mouse device must succeed");

        // Inject mouse delta & button 0 (left click)
        auto* concreteMouse = static_cast<DirectInputMouseDevice*>(pMouseDev);
        concreteMouse->injectMotion(14, -7, 120);
        concreteMouse->injectButton(0, true);

        DIMOUSESTATE mouseState{};
        int32_t stateHr = pMouseDev->GetDeviceState(sizeof(DIMOUSESTATE), &mouseState);
        TEST_ASSERT(stateHr == DI_OK, "GetDeviceState for mouse must succeed");
        TEST_ASSERT(mouseState.lX == 14 && mouseState.lY == -7 && mouseState.lZ == 120, "DirectInput mouse deltas must match injected input");
        TEST_ASSERT((mouseState.rgbButtons[0] & 0x80) != 0, "DirectInput mouse button 0 must report down");

        // Second poll should clear relative deltas
        DIMOUSESTATE secondState{};
        pMouseDev->GetDeviceState(sizeof(DIMOUSESTATE), &secondState);
        TEST_ASSERT(secondState.lX == 0 && secondState.lY == 0 && secondState.lZ == 0, "DirectInput mouse deltas must reset after polling");

        pMouseDev->Unacquire();
        pMouseDev->Release();

        // Create DirectInput Keyboard Device
        IDirectInputDevice8W* pKbdDev = nullptr;
        int32_t kbdHr = pDI->CreateDevice(GUID_SysKeyboard, &pKbdDev, nullptr);
        TEST_ASSERT(kbdHr == DI_OK && pKbdDev != nullptr, "CreateDevice for GUID_SysKeyboard must succeed");
        TEST_ASSERT(pKbdDev->Acquire() == DI_OK, "Acquire on keyboard device must succeed");

        auto* concreteKbd = static_cast<DirectInputKeyboardDevice*>(pKbdDev);
        concreteKbd->injectKey(0x39 /* DIK_SPACE */, true);

        std::array<uint8_t, 256> kbdState{};
        int32_t kbdStateHr = pKbdDev->GetDeviceState(256, kbdState.data());
        TEST_ASSERT(kbdStateHr == DI_OK, "GetDeviceState for keyboard must succeed");
        TEST_ASSERT((kbdState[0x39] & 0x80) != 0, "DirectInput keyboard state for DIK_SPACE must report pressed");

        pKbdDev->Unacquire();
        pKbdDev->Release();

        pDI->Release();
    }

    // Destroy Window and clean up
    DestroyWindow(hwnd);
    TEST_ASSERT(!IsWindow(hwnd), "DestroyWindow must remove window from active map");
    TEST_ASSERT(std::find(receivedMessages.begin(), receivedMessages.end(), WM_DESTROY) != receivedMessages.end(), "WM_DESTROY must be received upon destruction");
}

void Test_PrismX_Interactive3DViewer_And_CameraPipeline() {
    using namespace micant::viewer;
    using namespace micant::user32;

    // 1. Orbit Camera Mathematics & Transformations
    {
        ViewerCamera camera{};
        TEST_ASSERT(std::abs(camera.radius - 4.0f) < 0.001f, "Camera initial radius must be 4.0");
        TEST_ASSERT(std::abs(camera.yaw - 0.785f) < 0.001f, "Camera initial yaw must be ~45 deg");
        TEST_ASSERT(std::abs(camera.pitch - 0.45f) < 0.001f, "Camera initial pitch must be ~26 deg");

        // Rotate camera
        camera.rotate(0.2f, 0.1f);
        TEST_ASSERT(std::abs(camera.yaw - 0.985f) < 0.001f, "Camera yaw rotation must update");
        TEST_ASSERT(std::abs(camera.pitch - 0.55f) < 0.001f, "Camera pitch rotation must update");

        // Test Pitch clamping (avoid gimbal lock)
        camera.rotate(0.0f, 5.0f);
        TEST_ASSERT(camera.pitch <= 1.45f, "Camera pitch must be clamped to <= 1.45 rad (~83 deg)");
        camera.rotate(0.0f, -10.0f);
        TEST_ASSERT(camera.pitch >= -1.45f, "Camera pitch must be clamped to >= -1.45 rad");

        // Reset camera pitch
        camera.pitch = 0.45f;

        // Test Zoom clamping
        camera.zoom(-10.0f);
        TEST_ASSERT(std::abs(camera.radius - camera.minRadius) < 0.001f, "Camera zoom must clamp to minRadius (1.0)");
        camera.zoom(100.0f);
        TEST_ASSERT(std::abs(camera.radius - camera.maxRadius) < 0.001f, "Camera zoom must clamp to maxRadius (25.0)");
        camera.radius = 4.0f;

        // Test Pan
        camera.pan(1.0f, -0.5f);
        TEST_ASSERT(std::abs(camera.target.x - 1.0f) < 0.001f && std::abs(camera.target.y - (-0.5f)) < 0.001f, "Camera pan must shift target point");
        camera.target = { 0.0f, 0.0f, 0.0f };

        // Eye position vector
        prism3d::Vector3 eye = camera.getEyePosition();
        float distFromTarget = std::sqrt(eye.x * eye.x + eye.y * eye.y + eye.z * eye.z);
        TEST_ASSERT(std::abs(distFromTarget - 4.0f) < 0.01f, "Eye distance from origin must match orbit radius");

        // View and Projection matrices
        auto viewMat = camera.getViewMatrix();
        TEST_ASSERT(viewMat.m[3][3] == 1.0f || viewMat.m[0][0] != 0.0f, "View matrix must be valid camera transform");
        auto projMat = camera.getProjectionMatrix(0.785f, 4.0f / 3.0f, 0.1f, 100.0f);
        TEST_ASSERT(projMat.m[3][2] != 0.0f || projMat.m[2][3] != 0.0f, "Projection matrix must be valid perspective transform");
    }

    // 2. Procedural Mesh Generation
    {
        // 2a. DEC PRISM Crystal Core
        auto crystal = MeshGenerator::createCrystal();
        TEST_ASSERT(crystal.vertices.size() == 18, "Crystal mesh must have 18 vertices (2 apexes + 8 upper + 8 lower)");
        TEST_ASSERT(crystal.indices.size() == 96, "Crystal mesh must have 96 indices (32 triangles: 8 top + 16 mid + 8 btm)");
        TEST_ASSERT(crystal.vertices[0].y > 1.5f, "Top apex vertex must have elevated Y coordinate");
        TEST_ASSERT(crystal.vertices[1].y < -1.5f, "Bottom apex vertex must have negative Y coordinate");

        // 2b. Parametric 3D Torus
        auto torus = MeshGenerator::createTorus(1.0f, 0.35f, 16, 12);
        TEST_ASSERT(torus.vertices.size() == 192, "Torus (16x12) must have 192 vertices");
        TEST_ASSERT(torus.indices.size() == 1152, "Torus (16x12) must have 1152 indices (384 triangles)");
        TEST_ASSERT(torus.vertices[0].g > 0.1f, "Torus vertices must have shaded lighting intensity");

        // 2c. Shaded Cube
        auto cube = MeshGenerator::createCube(1.0f);
        TEST_ASSERT(cube.vertices.size() == 8, "Cube mesh must have 8 vertices");
        TEST_ASSERT(cube.indices.size() == 36, "Cube mesh must have 36 indices (12 triangles)");
    }

    // 3. Interactive Viewer Session & Direct3D 11 Presentation
    {
        ViewerSession session(640, 480);
        bool ok = session.initialize(L"MicaNT Test Viewer Window");
        TEST_ASSERT(ok, "ViewerSession::initialize must succeed");
        TEST_ASSERT(session.getHwnd() != nullptr, "ViewerSession must create valid native HWND");

        // Test Input Events Injection
        float origYaw = session.getCamera().yaw;
        session.onMouseMove(40, -20, true);
        TEST_ASSERT(session.getCamera().yaw != origYaw, "onMouseMove with leftDrag must rotate camera");

        float origRadius = session.getCamera().radius;
        session.onMouseWheel(120);
        TEST_ASSERT(session.getCamera().radius < origRadius, "onMouseWheel with positive delta must zoom in");

        // Keyboard commands
        session.onKeyDown(0x57); // 'W' toggles wireframe
        session.onKeyDown(0x4D); // 'M' cycles model
        session.onKeyDown(0x20); // Space toggles auto-rotate

        // Gamepad injection
        session.onGamepad(0.6f, -0.4f, 0.5f);

        // Render multi-frame animated sequence
        ViewerStats stats = session.run(10);
        TEST_ASSERT(stats.frameCount >= 10, "ViewerSession::run must render requested number of frames");
        TEST_ASSERT(stats.triangleCount > 0, "ViewerSession must report non-zero triangle count");
        TEST_ASSERT(stats.vertexCount > 0, "ViewerSession must report non-zero vertex count");

        // Verify window surface buffer is updated with rendered 3D graphics
        uint32_t surfW = 0, surfH = 0;
        const uint32_t* winPixels = WindowManager::get().getWindowPixelBuffer(session.getHwnd(), &surfW, &surfH);
        TEST_ASSERT(winPixels != nullptr && surfW == 640 && surfH == 480, "Window surface buffer must exist at 640x480");

        // Count pixels distinct from top-left background to confirm actual geometry rasterization
        uint32_t bgPixel = winPixels[0];
        uint32_t geomPixels = 0;
        for (size_t i = 0; i < 640 * 480; ++i) {
            if (winPixels[i] != bgPixel) {
                geomPixels++;
            }
        }
        TEST_ASSERT(geomPixels > 100, "Window surface must contain rasterized 3D geometry pixels distinct from background");

        // Switch to Torus and Wireframe mode
        session.setModel(ViewerModelType::Torus);
        session.setWireframe(true);
        ViewerStats torusStats = session.run(5);
        TEST_ASSERT(torusStats.triangleCount == 384, "Torus active model must render 384 triangles");
        TEST_ASSERT(torusStats.wireframe == true, "Session must report wireframe state active");

        // Switch to Cube
        session.setModel(ViewerModelType::Cube);
        session.setWireframe(false);
        ViewerStats cubeStats = session.run(5);
        TEST_ASSERT(cubeStats.triangleCount == 12, "Cube active model must render 12 triangles");
        TEST_ASSERT(cubeStats.wireframe == false, "Session must report solid fill state active");
    }

    // 4. Shell Integration Command (view3d)
    {
        micant::shell::CommandShell shell;
        std::ostringstream out;

        shell.execute("view3d --crystal --frames 3", out);
        std::string res = out.str();
        TEST_ASSERT(res.find("[PrismX 3D Viewer]") != std::string::npos, "Shell view3d command must execute");
        TEST_ASSERT(res.find("DEC PRISM Crystal Core") != std::string::npos, "Shell output must list active Crystal model");
        TEST_ASSERT(res.find("Session Completed") != std::string::npos, "Shell output must confirm session completion");

        std::ostringstream out2;
        shell.execute("view3d --torus -w --frames 3", out2);
        std::string res2 = out2.str();
        TEST_ASSERT(res2.find("Parametric 3D Torus") != std::string::npos, "Shell output must list Torus model");
        TEST_ASSERT(res2.find("Wireframe") != std::string::npos, "Shell output must list Wireframe rasterizer");
    }
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
    RUN_TEST(Test_PrismX_And_Prism3D_GraphicsSubsystem);
    RUN_TEST(Test_VulkanLoader_And_PrismVK_Subsystem);
    RUN_TEST(Test_Prism3D12_And_ProgrammableShaderVM);
    RUN_TEST(Test_EmeraldFS_And_DaytonaMM_Subsystems);
    RUN_TEST(Test_DirectX_DynamicLoader_And_DXBC_Container);
    RUN_TEST(Test_DirectX_PrismAudio_And_XInput_Subsystems);
    RUN_TEST(Test_VanguardDriver_DeviceStack_And_PnP_Subsystem);
    RUN_TEST(Test_AegisSandbox_JobObjects_And_ProcessContainment);
    RUN_TEST(Test_PolarisDiag_CrashDump_And_MinidumpWriter);
    RUN_TEST(Test_CipherKSP_CryptographicServices_And_AES);
    RUN_TEST(Test_JanusLDR_DelayLoadThunks_And_SxSManifest);
    RUN_TEST(Test_User32_WindowManager_SwapchainPresentation_And_DirectInput);
    RUN_TEST(Test_PrismX_Interactive3DViewer_And_CameraPipeline);

    std::cout << "\n------------------------------------------------------------------------\n";
    std::cout << "Summary: " << g_PassedTests << " Passed, " << g_FailedTests << " Failed\n";
    std::cout << "------------------------------------------------------------------------\n";

    return (g_FailedTests == 0) ? 0 : 1;
}

