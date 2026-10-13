#pragma once

/**
 * @file kernel_suites.hpp
 * @brief Kernel & Core Executive Subsystems (Milestones 1-30)
 */

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

    // 1. Directory insert & lookup
    NtStatus insStatus = root.insertObject(L"PhysicalDrive0", obj1.get());
    TEST_ASSERT(NT_SUCCESS(insStatus), "Directory insert should succeed");
    TEST_ASSERT(root.getEntryCount() == 1, "Directory should contain 1 object");

    // 2. Collision detection
    NtStatus dupStatus = root.insertObject(L"PhysicalDrive0", obj1.get());
    TEST_ASSERT(dupStatus == NtStatus::ObjectNameCollision, "Duplicate name should return ObjectNameCollision");

    // 3. 37-Bucket Hash Case-Insensitive Lookup
    ob::ObjectHeader* found = root.lookup(L"PhysicalDrive0");
    TEST_ASSERT(found == obj1.get(), "Lookup should return inserted object");
    ob::ObjectHeader* foundCaseInsensitive = root.lookup(L"physicaldrive0");
    TEST_ASSERT(foundCaseInsensitive == obj1.get(), "Case-insensitive lookup in 37-bucket hash chain must succeed");

    // 4. Handle Table Allocation & 4-byte Stride (NT Standard)
    ob::HandleTable handleTable;
    Handle h1 = 0;
    NtStatus hStatus = handleTable.createHandle(obj1.get(), h1, ob::GENERIC_READ);
    TEST_ASSERT(NT_SUCCESS(hStatus), "Handle creation should succeed");
    TEST_ASSERT(h1 == 4, "First handle index should be 4 (NT standard)");
    TEST_ASSERT(handleTable.getActiveHandleCount() == 1, "Active handle count must be 1");

    // 5. Handle Lookup with Access Mask Verification
    uint32_t granted = 0;
    ob::ObjectHeader* fromHandle = handleTable.lookup(h1, &granted);
    TEST_ASSERT(fromHandle == obj1.get(), "Handle lookup should return correct object");
    TEST_ASSERT(granted == ob::GENERIC_READ, "Granted access mask must match GENERIC_READ");

    // 6. Handle Duplication (Source to Target Table)
    ob::HandleTable targetTable;
    Handle hTarget = 0;
    NtStatus dupHandleStatus = handleTable.duplicateHandle(h1, targetTable, hTarget, ob::GENERIC_ALL, false, 0);
    TEST_ASSERT(NT_SUCCESS(dupHandleStatus), "Handle duplication must succeed");
    TEST_ASSERT(hTarget == 4, "First handle in target table must be 4");
    uint32_t targetAccess = 0;
    TEST_ASSERT(targetTable.lookup(hTarget, &targetAccess) == obj1.get(), "Duplicated handle must point to original object");
    TEST_ASSERT(targetAccess == ob::GENERIC_ALL, "Duplicated handle granted access must match GENERIC_ALL");

    // 7. ObReferenceObjectByHandle Type Safety & Access Rights
    ob::ObjectType eventType{
        .typeName = L"Event",
        .typeId = ob::ObjectTypeId::Event
    };

    ob::ObjectHeader* refObj = nullptr;
    // Type mismatch must fail
    NtStatus mismatchSt = ob::ObReferenceObjectByHandle(h1, handleTable, ob::GENERIC_READ, &eventType, &refObj);
    TEST_ASSERT(mismatchSt == NtStatus::ObjectTypeMismatch, "ObReferenceObjectByHandle with wrong type must return ObjectTypeMismatch");

    // Access denied must fail (requesting WRITE on READ-only handle)
    NtStatus deniedSt = ob::ObReferenceObjectByHandle(h1, handleTable, ob::GENERIC_WRITE, &fileType, &refObj);
    TEST_ASSERT(deniedSt == NtStatus::AccessDenied, "ObReferenceObjectByHandle with excessive access rights must return AccessDenied");

    // Valid reference must succeed
    NtStatus validRef = ob::ObReferenceObjectByHandle(h1, handleTable, ob::GENERIC_READ, &fileType, &refObj);
    TEST_ASSERT(NT_SUCCESS(validRef), "ObReferenceObjectByHandle with correct type and access must succeed");
    TEST_ASSERT(refObj == obj1.get(), "Referenced object must match obj1");
    ob::ObDereferenceObject(refObj);

    // 8. Global Namespace Tree & Hierarchical Resolution (ObpLookupObjectName)
    auto& obNs = ob::ObjectNamespace::get();
    obNs.initializeRoot();
    TEST_ASSERT(obNs.getDirectoryCount() >= 8, "Root namespace must contain standard NT directories");

    // Create nested directory and insert object
    obNs.createDirectory(L"\\Device\\Harddisk0");
    auto diskDir = obNs.getDirectory(L"\\Device\\Harddisk0");
    TEST_ASSERT(diskDir != nullptr, "Created nested directory must be retrievable");

    auto partitionObj = std::make_unique<ob::ObjectHeader>();
    partitionObj->type = &fileType;
    partitionObj->objectName = L"Partition1";
    diskDir->insertObject(L"Partition1", partitionObj.get());

    // Lookup via full path
    auto* foundPart = obNs.lookupObject(L"\\Device\\Harddisk0\\Partition1");
    TEST_ASSERT(foundPart == partitionObj.get(), "Hierarchical lookup of \\Device\\Harddisk0\\Partition1 must succeed");

    // 9. Symbolic Link Resolution & Prefix Normalization
    obNs.createSymbolicLink(L"\\DosDevices\\D:", L"\\Device\\Harddisk0\\Partition1");
    std::wstring resolved1 = obNs.resolvePath(L"\\DosDevices\\D:\\Windows\\System32");
    TEST_ASSERT(resolved1 == L"\\Device\\Harddisk0\\Partition1\\Windows\\System32", "Symbolic link prefix replacement must resolve path");

    // DOS device alias normalization (\??\ and \\.\)
    std::wstring resolved2 = obNs.resolvePath(L"\\??\\D:\\Kernel32.dll");
    TEST_ASSERT(resolved2 == L"\\Device\\Harddisk0\\Partition1\\Kernel32.dll", "\\??\\ alias normalization must resolve path");

    // 10. Handle Close & Invalid Handle Protection
    NtStatus closeStatus = handleTable.closeHandle(h1);
    TEST_ASSERT(NT_SUCCESS(closeStatus), "Closing valid handle should succeed");
    TEST_ASSERT(handleTable.lookup(h1) == nullptr, "Closed handle must look up as nullptr");
    TEST_ASSERT(handleTable.getActiveHandleCount() == 0, "Handle table must have 0 active handles");

    NtStatus badClose = handleTable.closeHandle(999);
    TEST_ASSERT(badClose == NtStatus::InvalidHandle, "Closing unallocated handle must return InvalidHandle");

    // 11. Object Attributes & Temporary/Permanent Lifecycle
    obj1->attributes |= ob::OBJ_PERMANENT;
    TEST_ASSERT(obj1->isPermanent(), "Object must report permanent attribute");
    ob::ObMakeTemporaryObject(obj1.get());
    TEST_ASSERT(!obj1->isPermanent(), "ObMakeTemporaryObject must strip permanent attribute");
}


// ============================================================================
// Suite 2: Virtual Memory Manager Tests (Horizon 1 Clean-Room Architecture)
// ============================================================================
void Test_MemoryManager_VADAllocation() {
    mm::ProcessAddressSpace space;

    // 1. Basic Allocation & Bounds
    uintptr_t addr1 = 0;
    size_t size1 = 16 * 1024; // 16 KB
    NtStatus status1 = space.allocate(addr1, size1, mm::MEM_COMMIT | mm::MEM_RESERVE, mm::PAGE_READWRITE);
    TEST_ASSERT(NT_SUCCESS(status1), "Allocation of 16KB should succeed");
    TEST_ASSERT(addr1 >= mm::ProcessAddressSpace::UserSpaceMin, "Address must be within user space bounds");
    TEST_ASSERT(space.getRegionCount() == 1, "Should have 1 active VAD");

    // 2. Multiple Allocations & AVL Insertion
    uintptr_t addr2 = 0;
    size_t size2 = 64 * 1024; // 64 KB
    NtStatus status2 = space.allocate(addr2, size2, mm::MEM_COMMIT, mm::PAGE_EXECUTE_READ);
    TEST_ASSERT(NT_SUCCESS(status2), "Second allocation should succeed");
    TEST_ASSERT(addr2 != addr1, "Addresses must not collide");
    TEST_ASSERT(space.getRegionCount() == 2, "Should have 2 active VADs");

    // 3. AVL Self-Balancing Multi-Node Stress
    uintptr_t addrs[6] = {0};
    for (size_t i = 0; i < 6; ++i) {
        NtStatus s = space.allocate(addrs[i], 4096, mm::MEM_RESERVE | mm::MEM_COMMIT, mm::PAGE_READWRITE);
        TEST_ASSERT(NT_SUCCESS(s), "Multi-node allocation must succeed");
    }
    TEST_ASSERT(space.getRegionCount() == 8, "VAD tree should contain 8 active nodes");

    // 4. VAD Lookup & Boundary Verification
    auto* vadFound = space.findVad(addrs[3] + 100);
    TEST_ASSERT(vadFound != nullptr, "Lookup inside allocated range must return valid VAD");
    TEST_ASSERT(vadFound->startingAddress == addrs[3], "Starting address must match descriptor");
    TEST_ASSERT(space.findVad(0x0000000000005000ULL) == nullptr, "Unallocated address must return nullptr");

    // 5. Physical Page Frame Number (PFN) Database Verification
    auto& pfnDb = mm::PfnDatabase::get();
    size_t initialZeroed = pfnDb.getZeroedCount();
    TEST_ASSERT(initialZeroed > 0, "Initial zeroed page count must be > 0");
    uint64_t allocatedPfn = pfnDb.allocatePage(true);
    TEST_ASSERT(allocatedPfn != 0, "PFN allocation should return nonzero physical page frame");
    TEST_ASSERT(pfnDb.getActiveCount() > 0, "Active page count must increment");
    pfnDb.referencePage(allocatedPfn);
    pfnDb.freePage(allocatedPfn); // Decrements ref count to 1
    pfnDb.freePage(allocatedPfn); // Decrements ref count to 0, transitions to Free list
    TEST_ASSERT(pfnDb.getFreeCount() > 0, "Freed page should be returned to Free list");

    // 6. Hardware 4-Level Paging Engine Simulation
    mm::HardwarePagingEngine hwPaging;
    uintptr_t testCr3 = 0x2000;
    uintptr_t testVa = 0x00007FF720000000ULL;
    hwPaging.mapPage(testCr3, testVa, allocatedPfn, mm::PAGE_READWRITE, true);
    auto pteOpt = hwPaging.queryPte(testCr3, testVa);
    TEST_ASSERT(pteOpt.has_value(), "PTE should be present in page table");
    TEST_ASSERT(pteOpt->present == 1, "PTE present bit must be 1");
    TEST_ASSERT(pteOpt->writable == 1, "PTE writable bit must be 1");
    TEST_ASSERT(pteOpt->pageFrameNumber == allocatedPfn, "PTE PFN must match mapped frame");
    hwPaging.modifyProtection(testCr3, testVa, mm::PAGE_READONLY);
    auto pteReadOnly = hwPaging.queryPte(testCr3, testVa);
    TEST_ASSERT(pteReadOnly->writable == 0, "PTE writable bit must be cleared after protection change");
    hwPaging.unmapPage(testCr3, testVa);
    TEST_ASSERT(!hwPaging.queryPte(testCr3, testVa).has_value(), "Unmapped page should return nullopt");

    // 7. Vector 14 (#PF) Demand-Zero Page Fault Handling
    uintptr_t faultVa = addr1 + 4096;
    size_t residentBefore = space.getResidentPages();
    NtStatus pfStatus = mm::MmAccessFault(space, faultVa, false, true, false);
    TEST_ASSERT(NT_SUCCESS(pfStatus), "Demand-zero fault on committed VAD must succeed");
    TEST_ASSERT(space.getResidentPages() == residentBefore + 1, "Resident pages must increment on demand fault");

    // 8. Copy-On-Write (PAGE_WRITECOPY) Fault Handling
    uintptr_t cowAddr = 0;
    space.allocate(cowAddr, 4096, mm::MEM_COMMIT | mm::MEM_RESERVE, mm::PAGE_WRITECOPY);
    auto* cowVad = space.findVad(cowAddr);
    TEST_ASSERT(cowVad != nullptr && cowVad->isCopyOnWrite, "VAD must be marked CopyOnWrite");
    NtStatus cowStatus = mm::MmAccessFault(space, cowAddr, true, true, false);
    TEST_ASSERT(NT_SUCCESS(cowStatus), "Writing to COW page should resolve fault and isolate page");
    TEST_ASSERT(!cowVad->isCopyOnWrite, "COW flag must be cleared after private page isolation");
    TEST_ASSERT(cowVad->protection == mm::PAGE_READWRITE, "Protection must transition to PAGE_READWRITE");

    // 9. Guard Page (PAGE_GUARD) Trip & Stack Expansion
    uintptr_t guardAddr = 0;
    space.allocate(guardAddr, 4096, mm::MEM_COMMIT | mm::MEM_RESERVE, mm::PAGE_READWRITE | mm::PAGE_GUARD);
    auto* guardVad = space.findVad(guardAddr);
    TEST_ASSERT(guardVad != nullptr && guardVad->isGuardPage, "VAD must be marked as Guard Page");
    NtStatus guardStatus = mm::MmAccessFault(space, guardAddr, true, true, false);
    TEST_ASSERT(guardStatus == NtStatus::GuardPageViolation, "Accessing guard page must return GuardPageViolation");
    TEST_ASSERT(!guardVad->isGuardPage, "Guard page flag must be stripped after violation");

    // 10. Access Violation (#PF) on Invalid Permissions
    NtStatus roFault = mm::MmAccessFault(space, addr2, true, true, false); // Write to PAGE_EXECUTE_READ
    TEST_ASSERT(roFault == NtStatus::AccessViolation, "Write to read-only/execute-read page must return AccessViolation");
    NtStatus badFault = mm::MmAccessFault(space, 0x0000000000008000ULL, false, true, false);
    TEST_ASSERT(badFault == NtStatus::AccessViolation, "Access to unallocated address must return AccessViolation");

    // 11. Decommit & Working Set Quotas
    size_t commitBefore = space.getCommitCharge();
    TEST_ASSERT(commitBefore > 0, "Commit charge should reflect committed pages");
    space.free(addrs[0], 4096, mm::MEM_DECOMMIT);
    TEST_ASSERT(space.getCommitCharge() < commitBefore, "Decommit should reduce commit charge");

    // 12. Full VAD Tree Release
    for (size_t i = 1; i < 6; ++i) {
        space.free(addrs[i], 4096, mm::MEM_RELEASE);
    }
    space.free(addrs[0], 4096, mm::MEM_RELEASE);
    space.free(cowAddr, 4096, mm::MEM_RELEASE);
    space.free(guardAddr, 4096, mm::MEM_RELEASE);
    space.free(addr1, size1, mm::MEM_RELEASE);
    space.free(addr2, size2, mm::MEM_RELEASE);
    TEST_ASSERT(space.getRegionCount() == 0, "All VADs should be released cleanly");
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
    sys::SyscallFrame badFrame{};
    badFrame.ssn = 0xFFFF;
    NtStatus badRes = dispatcher.dispatch(badFrame);
    TEST_ASSERT(badRes == NtStatus::InvalidDeviceRequest, "Unregistered SSN must return InvalidDeviceRequest");
}

// ============================================================================
// Suite 5: Process, Thread, and Section Objects Tests (Horizon 5 Ps Subsystem)
// ============================================================================
void Test_ProcessAndSectionManager() {
    auto& pm = ps::ProcessManager::get();

    // 1. Initial System Processes (PID 0 Idle and PID 4 System)
    auto idleProc = pm.getIdleProcess();
    TEST_ASSERT(idleProc != nullptr, "System Idle process must be initialized");
    TEST_ASSERT(idleProc->getPid() == 0, "Idle process PID must be 0");
    TEST_ASSERT(idleProc->getImageFileName() == L"Idle", "Idle image name must be 'Idle'");
    TEST_ASSERT(!idleProc->getThreads().empty(), "Idle process must contain at least 1 idle thread");
    TEST_ASSERT(idleProc->getThreads()[0]->getBasePriority() == 0, "Idle thread base priority must be 0");

    auto sysProc = pm.getSystemProcess();
    TEST_ASSERT(sysProc != nullptr, "System process must be initialized");
    TEST_ASSERT(sysProc->getPid() == 4, "System process PID must be 4");
    TEST_ASSERT(sysProc->getImageFileName() == L"System", "System image name must be 'System'");
    TEST_ASSERT(sysProc->getToken() != nullptr, "System process must hold SYSTEM security token");

    // 2. Userland Process Creation with Parent PID & CR3 Directory Table Base
    auto proc = pm.createProcess(L"TestProcess.exe", nullptr, 4); // Spawned by System (PID 4)
    TEST_ASSERT(proc != nullptr, "Process creation must return valid EProcess pointer");
    TEST_ASSERT(proc->getPid() >= 1000, "PID must start in executive range >= 1000");
    TEST_ASSERT(proc->getParentPid() == 4, "Parent PID must match 4 (System)");
    TEST_ASSERT(proc->getDirectoryTableBase() != 0, "CR3 DirectoryTableBase must be non-zero");
    TEST_ASSERT(proc->getPebAddress() != 0, "PEB must be mapped at non-zero address");

    // Verify PEB structure contents
    const auto& peb = proc->getPeb();
    TEST_ASSERT(peb.osBuildNumber == 26100, "PEB OS build number must be 26100");
    TEST_ASSERT(peb.osMajorVersion == 10, "PEB OS major version must be 10");
    TEST_ASSERT(peb.numberOfProcessors == 4, "PEB number of processors must be 4");

    // 3. Userland Thread Creation with TEB Allocation
    uintptr_t testEntryPoint = 0x00007FF710001000ULL;
    auto thread = proc->createThread(testEntryPoint);
    TEST_ASSERT(thread != nullptr, "Thread creation must succeed");
    TEST_ASSERT(thread->getContext().rip == testEntryPoint, "Thread RIP must match entry point");
    TEST_ASSERT(thread->getState() == ps::ThreadState::Initialized, "Initial thread state must be Initialized");
    TEST_ASSERT(thread->getTebAddress() != 0, "Thread must have allocated TEB");

    // Verify TEB structure contents (GS:[0x30] Self Pointer and ClientId)
    const auto& teb = thread->getTeb();
    TEST_ASSERT(teb.ntTib.self == thread->getTebAddress(), "TEB ntTib.self must point to TEB address");
    TEST_ASSERT(teb.ntTib.stackBase == thread->getStackBase(), "TEB stackBase must match thread stackBase");
    TEST_ASSERT(teb.ntTib.stackLimit == thread->getStackLimit(), "TEB stackLimit must match thread stackLimit");
    TEST_ASSERT(teb.clientId.uniqueProcess == proc->getPid(), "TEB ClientId.UniqueProcess must match process PID");
    TEST_ASSERT(teb.clientId.uniqueThread == thread->getTid(), "TEB ClientId.UniqueThread must match thread TID");
    TEST_ASSERT(teb.processEnvironmentBlock == proc->getPebAddress(), "TEB processEnvironmentBlock must match PEB address");


    // 4. Thread Impersonation Token Subsystem
    TEST_ASSERT(!thread->isImpersonating(), "Thread should not initially be impersonating");
    TEST_ASSERT(thread->getActiveToken() == proc->getToken(), "Active token must default to process primary token");

    auto impersonatedToken = se::TokenObject::createSystemToken();
    thread->impersonateToken(impersonatedToken, ps::SecurityImpersonationLevel::SecurityImpersonation);
    TEST_ASSERT(thread->isImpersonating(), "Thread must report impersonating state");
    TEST_ASSERT(thread->getActiveToken() == impersonatedToken, "Active token must match impersonated token");
    TEST_ASSERT(thread->getImpersonationLevel() == ps::SecurityImpersonationLevel::SecurityImpersonation, "Impersonation level must match");

    thread->revertToSelf();
    TEST_ASSERT(!thread->isImpersonating(), "Thread must no longer be impersonating after revertToSelf");
    TEST_ASSERT(thread->getActiveToken() == proc->getToken(), "Active token must return to process primary token");

    // 5. Standard NT Ps* Query APIs
    std::shared_ptr<ps::EProcess> lookedUpProc;
    NtStatus lookupSt = ps::PsLookupProcessByProcessId(proc->getPid(), lookedUpProc);
    TEST_ASSERT(NT_SUCCESS(lookupSt), "PsLookupProcessByProcessId must succeed");
    TEST_ASSERT(lookedUpProc == proc, "Looked up process must match proc");
    TEST_ASSERT(ps::PsGetProcessId(proc.get()) == proc->getPid(), "PsGetProcessId must return PID");
    TEST_ASSERT(ps::PsGetProcessImageFileName(proc.get()) == L"TestProcess.exe", "PsGetProcessImageFileName must match");

    std::shared_ptr<ps::EThread> lookedUpThread;
    NtStatus lookupThSt = ps::PsLookupThreadByThreadId(thread->getTid(), lookedUpThread);
    TEST_ASSERT(NT_SUCCESS(lookupThSt), "PsLookupThreadByThreadId must succeed");
    TEST_ASSERT(lookedUpThread == thread, "Looked up thread must match thread");
    TEST_ASSERT(ps::PsGetThreadId(thread.get()) == thread->getTid(), "PsGetThreadId must return TID");

    // 6. Create Section Object & Memory Mapping
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

    // 7. Terminate Process
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
// Suite 7: I/O Request Packet (IRP), Device Stacks & IOCP Tests (Horizon 6)
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

    // 2. Layered Device Stacks (PDO -> FDO -> FiDO)
    io::DriverObject pdoDriver;
    pdoDriver.driverName = L"\\Driver\\PciBus";
    auto pdo = ioMgr.createDevice(&pdoDriver, L"\\Device\\PhysicalDisk0", io::DeviceType::Disk);
    TEST_ASSERT(pdo->stackSize == 1, "Base PDO stack size must be 1");

    io::DriverObject fdoDriver;
    fdoDriver.driverName = L"\\Driver\\DiskClass";
    auto fdo = ioMgr.createDevice(&fdoDriver, L"\\Device\\HarddiskVolume0", io::DeviceType::Disk);
    io::IoAttachDeviceToDeviceStack(fdo.get(), pdo.get());
    TEST_ASSERT(pdo->attachedDevice == fdo.get(), "FDO must attach on top of PDO");
    TEST_ASSERT(fdo->stackSize == 2, "FDO stack size must be 2");

    io::DriverObject filterDriver;
    filterDriver.driverName = L"\\Driver\\DiskFilter";
    auto fido = ioMgr.createDevice(&filterDriver, L"\\Device\\DiskFilter0", io::DeviceType::Disk);
    io::IoAttachDeviceToDeviceStack(fido.get(), pdo.get());
    TEST_ASSERT(io::IoGetAttachedDevice(pdo.get()) == fido.get(), "IoGetAttachedDevice on PDO must return top of stack (FiDO)");
    TEST_ASSERT(fido->stackSize == 3, "Filter device stack size must be 3");

    // 3. Multi-tier IRP Stack Traversal & Completion Routine Unwinding
    static bool s_CompletionRoutineRan = false;
    static void* s_CompletionContext = nullptr;

    auto* multiTierIrp = io::IoAllocateIrp(3);
    TEST_ASSERT(multiTierIrp != nullptr, "IoAllocateIrp with 3 stack locations must succeed");
    TEST_ASSERT(multiTierIrp->stackCount == 3, "IRP stackCount must match 3");
    TEST_ASSERT(multiTierIrp->currentStackLocation == 3, "IRP currentStackLocation must start at 3");

    // Setup completion routine on next stack location
    io::IoSetCompletionRoutine(
        multiTierIrp,
        [](io::DeviceObject*, io::Irp* irp, void* ctx) -> NtStatus {
            s_CompletionRoutineRan = true;
            s_CompletionContext = ctx;
            irp->ioStatus.information = 2048;
            return NtStatus::Success;
        },
        reinterpret_cast<void*>(0xCAFE),
        true, // onSuccess
        false,
        false
    );

    // Advance stack location for child dispatch
    io::IoSetNextIrpStackLocation(multiTierIrp);
    TEST_ASSERT(multiTierIrp->currentStackLocation == 2, "IoSetNextIrpStackLocation must advance down stack");

    // Signal completion upwards through stack
    multiTierIrp->ioStatus.status = NtStatus::Success;
    io::IoCompleteRequest(multiTierIrp, 0);
    TEST_ASSERT(s_CompletionRoutineRan, "Completion routine must have executed during IoCompleteRequest");
    TEST_ASSERT(s_CompletionContext == reinterpret_cast<void*>(0xCAFE), "Completion context must match 0xCAFE");
    TEST_ASSERT(multiTierIrp->ioStatus.information == 2048, "Completion routine must have modified IRP information");

    io::IoFreeIrp(multiTierIrp);

    // 4. Fast I/O Dispatch Bypass (Zero-IRP Acceleration)
    static bool s_FastIoReadRan = false;
    io::FastIoDispatch fastIo{};
    fastIo.fastIoRead = [](void*, LargeInteger*, uint32_t length, bool, void* buffer, IoStatusBlock* ioStatus, io::DeviceObject*) -> bool {
        s_FastIoReadRan = true;
        if (buffer && length >= 4) {
            std::memcpy(buffer, "FAST", 4);
        }
        if (ioStatus) {
            ioStatus->status = NtStatus::Success;
            ioStatus->information = 4;
        }
        return true;
    };
    nullDriver.fastIoDispatch = &fastIo;

    char fastBuffer[16]{};
    IoStatusBlock fastIosb{};
    LargeInteger fastOffset{};
    bool fastSuccess = io::IoFastRead(nullDev.get(), nullptr, &fastOffset, 4, true, fastBuffer, &fastIosb);
    TEST_ASSERT(fastSuccess, "IoFastRead must return true for registered fast I/O handler");
    TEST_ASSERT(s_FastIoReadRan, "Fast I/O read callback must have executed");
    TEST_ASSERT(std::memcmp(fastBuffer, "FAST", 4) == 0, "Fast I/O buffer content must match 'FAST'");
    TEST_ASSERT(fastIosb.information == 4, "Fast I/O bytes transferred must be 4");

    // 5. Thread-Safe I/O Request Cancellation Subsystem
    static bool s_CancelRoutineRan = false;
    auto* cancelTestIrp = io::IoAllocateIrp(1);
    cancelTestIrp->deviceObject = nullDev.get();

    io::IoSetCancelRoutine(cancelTestIrp, [](io::DeviceObject*, io::Irp* irp) {
        s_CancelRoutineRan = true;
        irp->ioStatus.status = NtStatus::Cancelled;
        io::IoReleaseCancelSpinLock(irp->cancelIrql);
    });

    bool cancelResult = io::IoCancelIrp(cancelTestIrp);
    TEST_ASSERT(cancelResult, "IoCancelIrp must return true when cancel routine is invoked");
    TEST_ASSERT(s_CancelRoutineRan, "Cancel routine must have executed");
    TEST_ASSERT(cancelTestIrp->cancel, "IRP cancel flag must be true");
    TEST_ASSERT(cancelTestIrp->ioStatus.status == NtStatus::Cancelled, "IRP status must be Cancelled");
    TEST_ASSERT(cancelTestIrp->cancelRoutine == nullptr, "Cancel routine must be cleared after cancellation");

    io::IoFreeIrp(cancelTestIrp);

    // 6. Kernel Driver Manager Lifecycle (DriverEntry & DriverUnload)
    static bool s_DriverUnloaded = false;
    auto& drvMgr = driver::DriverManager::get();

    NtStatus loadStatus = drvMgr.loadDriver(
        L"\\Driver\\TestEchoDriver",
        [](io::DriverObject* drv, const UnicodeString*) -> NtStatus {
            drv->driverUnload = [](io::DriverObject*) {
                s_DriverUnloaded = true;
            };
            return NtStatus::Success;
        }
    );
    TEST_ASSERT(NT_SUCCESS(loadStatus), "DriverManager::loadDriver must succeed");
    TEST_ASSERT(drvMgr.lookupDriver(L"\\Driver\\TestEchoDriver") != nullptr, "Loaded driver must be discoverable");

    NtStatus unloadStatus = drvMgr.unloadDriver(L"\\Driver\\TestEchoDriver");
    TEST_ASSERT(NT_SUCCESS(unloadStatus), "DriverManager::unloadDriver must succeed");
    TEST_ASSERT(s_DriverUnloaded, "DriverUnload routine must have executed");
    TEST_ASSERT(drvMgr.lookupDriver(L"\\Driver\\TestEchoDriver") == nullptr, "Unloaded driver must be removed");

    // 7. I/O Completion Port (IOCP)
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

    // 2. Binary values, enumeration, and deletion
    std::vector<uint8_t> testBin = { 0xDE, 0xAD, 0xBE, 0xEF, 0x42 };
    config->setValueBinary(L"SecuritySeed", testBin);
    const auto* seedVal = config->getValue(L"SecuritySeed");
    TEST_ASSERT(seedVal != nullptr && seedVal->type == cm::RegType::Binary, "SecuritySeed must be REG_BINARY");
    TEST_ASSERT(seedVal->data == testBin, "Binary data content must match");
    auto subkeys = micaCorp->getSubkeyNames();
    TEST_ASSERT(std::find(subkeys.begin(), subkeys.end(), L"Config") != subkeys.end(), "Subkey list must contain Config");
    auto values = config->getValueNames();
    TEST_ASSERT(values.size() == 4, "Config must contain 4 values");
    config->deleteValue(L"SecuritySeed");
    TEST_ASSERT(config->getValue(L"SecuritySeed") == nullptr, "Deleted value must no longer exist");

    // 3. On-Disk REGF / HBIN Hive Serialization & Cell Allocator
    auto exportedHive = cm.exportToHive(*sessionMgr);
    TEST_ASSERT(exportedHive != nullptr, "Exported hive must be valid");
    TEST_ASSERT(exportedHive->isConsistent(), "Exported hive must be consistent");
    const auto& hdr = exportedHive->getHeader();
    TEST_ASSERT(hdr.signature == cm::REGF_SIGNATURE, "Hive header signature must be 'regf'");
    TEST_ASSERT(hdr.majorVersion == 1 && hdr.minorVersion == 5, "Hive version must be 1.5");
    TEST_ASSERT(hdr.sequence1 == 1 && hdr.sequence2 == 1, "Sequence numbers must be 1");
    TEST_ASSERT(hdr.checksum == cm::ComputeRegfChecksum(hdr), "Checksum must match computed XOR-32");
    TEST_ASSERT(hdr.rootCellOffset != 0, "Root cell offset must be non-zero");

    // Verify cell allocator data
    auto* nk = reinterpret_cast<const cm::NkRecordHeader*>(exportedHive->getCellData(hdr.rootCellOffset));
    TEST_ASSERT(nk != nullptr, "Root cell data must be accessible");
    TEST_ASSERT(nk->signature == cm::NK_SIGNATURE, "Root cell record signature must be 'nk'");
    TEST_ASSERT(nk->nameLength > 0, "Root cell name length must be > 0");

    // Test cell allocation and freeing in hive
    uint32_t cellA = exportedHive->allocateCell(64);
    uint32_t cellB = exportedHive->allocateCell(128);
    TEST_ASSERT(cellA != cm::HCELL_NIL && cellB != cm::HCELL_NIL, "Cell allocations must succeed");
    TEST_ASSERT(cellA != cellB, "Allocated cells must have distinct offsets");
    exportedHive->freeCell(cellA);
    exportedHive->freeCell(cellB);

    // 4. Hive Binary Stream Round-Trip
    std::vector<uint8_t> hiveBytes = exportedHive->serialize();
    TEST_ASSERT(!hiveBytes.empty(), "Serialized hive bytes must not be empty");
    TEST_ASSERT(hiveBytes.size() >= sizeof(cm::RegfHeader) + cm::HIVE_BIN_SIZE, "Hive size must be at least header + 1 bin");

    auto deserializedHive = cm::CmHive::deserialize(hiveBytes);
    TEST_ASSERT(deserializedHive != nullptr, "Deserialized hive must be valid");
    TEST_ASSERT(deserializedHive->isConsistent(), "Deserialized hive must be consistent");
    TEST_ASSERT(deserializedHive->getRootCellOffset() == hdr.rootCellOffset, "Root cell offset must match after deserialization");

    // 5. Transactional Registry (TxR) Journal Crash Recovery
    cm::TxRJournal journal;
    deserializedHive->setSequence(2, 1); // Simulate interrupted crash during write
    TEST_ASSERT(!deserializedHive->isConsistent(), "Mismatched sequence numbers must register as inconsistent");
    journal.recordSectorWrite(0, std::span<const uint8_t>(hiveBytes.data(), 512));
    bool recovered = journal.recoverHive(*deserializedHive);
    TEST_ASSERT(recovered, "Crash recovery must execute");
    TEST_ASSERT(deserializedHive->isConsistent(), "Hive must be consistent after journal recovery");
    TEST_ASSERT(deserializedHive->getHeader().sequence1 == 2, "Sequence numbers must synchronize after recovery");
    TEST_ASSERT(deserializedHive->getHeader().sequence2 == 2, "Sequence numbers must synchronize after recovery");

    // 6. Security Descriptor Integration & Key Access Validation
    se::Sid userSid(5, {21, 555, 666, 777, 1002});
    auto standardUserToken = se::TokenObject::createUserToken(userSid);
    auto adminToken = se::TokenObject::createAdminToken(userSid);

    uint32_t grantedAccess = 0;
    // Standard User query access (KEY_READ) -> Granted
    NtStatus stUserRead = sessionMgr->checkAccess(*standardUserToken, cm::KEY_READ, grantedAccess);
    TEST_ASSERT(NT_SUCCESS(stUserRead), "Standard user must have KEY_READ access to Session Manager");
    TEST_ASSERT((grantedAccess & cm::KEY_QUERY_VALUE) != 0, "Granted access must include KEY_QUERY_VALUE");

    // Standard User write access (KEY_SET_VALUE) -> Denied by DACL & MIC
    NtStatus stUserWrite = sessionMgr->checkAccess(*standardUserToken, cm::KEY_SET_VALUE, grantedAccess);
    TEST_ASSERT(stUserWrite == NtStatus::AccessDenied, "Standard user must be denied KEY_SET_VALUE on Session Manager");

    // Admin access (KEY_ALL_ACCESS) -> Granted
    NtStatus stAdminAll = sessionMgr->checkAccess(*adminToken, cm::KEY_ALL_ACCESS, grantedAccess);
    TEST_ASSERT(NT_SUCCESS(stAdminAll), "Administrator must have KEY_ALL_ACCESS to Session Manager");
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

    // 4. Mandatory Integrity Control (MIC) Enforcement
    se::Sid untrustedSid = se::Sid::integrityUntrusted();
    se::Sid lowSid = se::Sid::integrityLow();
    se::Sid medSid = se::Sid::integrityMedium();
    se::Sid highSid = se::Sid::integrityHigh();
    se::Sid sysSid = se::Sid::integritySystem();
    se::Sid ppSid = se::Sid::integrityProtectedProcess();

    TEST_ASSERT(untrustedSid.toString() == L"S-1-16-0", "Untrusted integrity SID must format as S-1-16-0");
    TEST_ASSERT(lowSid.toString() == L"S-1-16-4096", "Low integrity SID must format as S-1-16-4096");
    TEST_ASSERT(medSid.toString() == L"S-1-16-8192", "Medium integrity SID must format as S-1-16-8192");
    TEST_ASSERT(highSid.toString() == L"S-1-16-12288", "High integrity SID must format as S-1-16-12288");
    TEST_ASSERT(sysSid.toString() == L"S-1-16-16384", "System integrity SID must format as S-1-16-16384");
    TEST_ASSERT(ppSid.toString() == L"S-1-16-20480", "Protected Process integrity SID must format as S-1-16-20480");

    se::SecurityDescriptor highIntegritySd;
    se::Acl openDacl;
    openDacl.addAllowedAce(everyone, se::GENERIC_ALL | se::STANDARD_RIGHTS_ALL | 0xFFFFFFFF);
    highIntegritySd.setDacl(openDacl);
    highIntegritySd.setIntegrityLevel(se::SECURITY_MANDATORY_HIGH_RID, se::SYSTEM_MANDATORY_LABEL_NO_WRITE_UP);
    TEST_ASSERT(highIntegritySd.hasMandatoryLabel(), "Security descriptor must detect mandatory label in SACL");
    TEST_ASSERT(highIntegritySd.getIntegrityLevel() == se::SECURITY_MANDATORY_HIGH_RID, "Integrity level must match High RID");
    TEST_ASSERT(highIntegritySd.getIntegrityPolicy() == se::SYSTEM_MANDATORY_LABEL_NO_WRITE_UP, "Integrity policy must match NO_WRITE_UP");

    // Medium integrity user attempting write to High integrity object -> BLOCKED by NO_WRITE_UP despite open DACL
    uint32_t micGranted = 0;
    NtStatus stMicWrite = se::SecurityReferenceMonitor::accessCheck(*userToken, highIntegritySd, se::GENERIC_WRITE, micGranted);
    TEST_ASSERT(stMicWrite == NtStatus::AccessDenied, "Medium token write to High integrity object must be denied by MIC");
    TEST_ASSERT(micGranted == 0, "Granted access must be 0 on MIC denial");

    // Medium integrity user attempting read to High integrity object -> ALLOWED by NO_WRITE_UP policy
    NtStatus stMicRead = se::SecurityReferenceMonitor::accessCheck(*userToken, highIntegritySd, se::GENERIC_READ, micGranted);
    TEST_ASSERT(NT_SUCCESS(stMicRead), "Medium token read to High integrity object must be permitted under NO_WRITE_UP");

    // High integrity admin token attempting write to High integrity object -> ALLOWED
    auto adminToken = se::TokenObject::createAdminToken(userSid);
    TEST_ASSERT(adminToken->getIntegrityLevel() == se::SECURITY_MANDATORY_HIGH_RID, "Admin token must have High integrity");
    NtStatus stAdminWrite = se::SecurityReferenceMonitor::accessCheck(*adminToken, highIntegritySd, se::GENERIC_WRITE, micGranted);
    TEST_ASSERT(NT_SUCCESS(stAdminWrite), "High token write to High integrity object must be permitted");

    // High integrity object with NO_READ_UP blocks Medium integrity read
    se::SecurityDescriptor noReadSd;
    noReadSd.setDacl(openDacl);
    noReadSd.setIntegrityLevel(se::SECURITY_MANDATORY_HIGH_RID, se::SYSTEM_MANDATORY_LABEL_NO_READ_UP);
    NtStatus stMicBlockRead = se::SecurityReferenceMonitor::accessCheck(*userToken, noReadSd, se::GENERIC_READ, micGranted);
    TEST_ASSERT(stMicBlockRead == NtStatus::AccessDenied, "NO_READ_UP policy must deny read access from lower integrity");

    // 5. UAC Token Filtering Simulation & Deny-Only SIDs
    auto uacFilteredToken = adminToken->filterTokenForUac();
    TEST_ASSERT(uacFilteredToken != nullptr, "UAC filtered token must be created");
    TEST_ASSERT(uacFilteredToken->getIntegrityLevel() == se::SECURITY_MANDATORY_MEDIUM_RID, "UAC filtered token must drop to Medium integrity");
    TEST_ASSERT(!uacFilteredToken->hasPrivilege(se::SE_DEBUG_NAME, false), "Admin privileges must be stripped in UAC token");
    TEST_ASSERT(!uacFilteredToken->hasPrivilege(se::SE_SHUTDOWN_NAME, false), "SeShutdownPrivilege must be stripped in UAC token");

    // In UAC token, Administrators group is marked SE_GROUP_USE_FOR_DENY_ONLY
    TEST_ASSERT(uacFilteredToken->hasSid(admins, true), "Deny-only Administrators SID must match for deny checks");
    TEST_ASSERT(!uacFilteredToken->hasSid(admins, false), "Deny-only Administrators SID must NOT match for allow checks");

    // DACL granting only Administrators
    se::SecurityDescriptor adminOnlySd;
    se::Acl adminOnlyDacl;
    adminOnlyDacl.addAllowedAce(admins, 0x1);
    adminOnlySd.setDacl(adminOnlyDacl);

    // Full admin token succeeds
    NtStatus stFullAdmin = se::SecurityReferenceMonitor::accessCheck(*adminToken, adminOnlySd, 0x1, micGranted);
    TEST_ASSERT(NT_SUCCESS(stFullAdmin), "Full admin token must be granted access on Admin-only DACL");

    // UAC filtered token is denied because Administrators is deny-only
    NtStatus stFilteredAdmin = se::SecurityReferenceMonitor::accessCheck(*uacFilteredToken, adminOnlySd, 0x1, micGranted);
    TEST_ASSERT(stFilteredAdmin == NtStatus::AccessDenied, "UAC filtered token must be denied access on Admin-only DACL");

    // 6. Privilege Verification APIs
    TEST_ASSERT(se::SecurityReferenceMonitor::SeSinglePrivilegeCheck(se::SE_DEBUG_NAME, *sysToken, se::ProcessorMode::UserMode), "SeSinglePrivilegeCheck must pass when privilege enabled");
    TEST_ASSERT(!se::SecurityReferenceMonitor::SeSinglePrivilegeCheck(se::SE_DEBUG_NAME, *userToken, se::ProcessorMode::UserMode), "SeSinglePrivilegeCheck must fail when privilege absent");
    TEST_ASSERT(se::SecurityReferenceMonitor::SeSinglePrivilegeCheck(se::SE_DEBUG_NAME, *userToken, se::ProcessorMode::KernelMode), "SeSinglePrivilegeCheck must pass unconditionally for KernelMode");

    std::vector<std::wstring> requiredPrivs = { std::wstring(se::SE_DEBUG_NAME), std::wstring(se::SE_TCB_NAME) };
    TEST_ASSERT(se::SecurityReferenceMonitor::SePrivilegeCheck(requiredPrivs, *sysToken, true), "SePrivilegeCheck all required must pass on system token");
    TEST_ASSERT(!se::SecurityReferenceMonitor::SePrivilegeCheck(requiredPrivs, *userToken, true), "SePrivilegeCheck all required must fail on user token");
    TEST_ASSERT(!se::SecurityReferenceMonitor::SePrivilegeCheck(requiredPrivs, *userToken, false), "SePrivilegeCheck any required must fail on user token");

    // ACCESS_SYSTEM_SECURITY privilege check (requires SeSecurityPrivilege)
    se::SecurityDescriptor secSd;
    secSd.setDacl(openDacl);
    NtStatus stSecWithoutPriv = se::SecurityReferenceMonitor::accessCheck(*userToken, secSd, se::ACCESS_SYSTEM_SECURITY, micGranted);
    TEST_ASSERT(stSecWithoutPriv == NtStatus::PrivilegeNotHeld, "Requesting ACCESS_SYSTEM_SECURITY without privilege must return PrivilegeNotHeld");

    auto secToken = se::TokenObject::createUserToken(userSid);
    secToken->addPrivilege(se::SE_SECURITY_NAME, se::SE_PRIVILEGE_ENABLED);
    NtStatus stSecWithPriv = se::SecurityReferenceMonitor::accessCheck(*secToken, secSd, se::ACCESS_SYSTEM_SECURITY, micGranted);
    TEST_ASSERT(NT_SUCCESS(stSecWithPriv), "Requesting ACCESS_SYSTEM_SECURITY with privilege must succeed");

    // 7. SACL Auditing
    se::SecurityReferenceMonitor::clearAuditLog();
    se::SecurityDescriptor auditSd;
    auditSd.setDacl(openDacl);
    se::Acl sacl;
    sacl.addAuditAce(everyone, 0x1, se::SUCCESSFUL_ACCESS_ACE_FLAG | se::FAILED_ACCESS_ACE_FLAG);
    auditSd.setSacl(sacl);
    TEST_ASSERT(auditSd.hasSacl(), "Security descriptor must have SACL");
    TEST_ASSERT((auditSd.getControl() & se::SE_SACL_PRESENT) != 0, "Control flag SE_SACL_PRESENT must be set");

    NtStatus stAuditSuccess = se::SecurityReferenceMonitor::accessCheck(*userToken, auditSd, 0x1, micGranted, L"TestSecurableObject");
    TEST_ASSERT(NT_SUCCESS(stAuditSuccess), "Audit access check should succeed");
    const auto& auditLogs = se::SecurityReferenceMonitor::getAuditLog();
    TEST_ASSERT(!auditLogs.empty(), "Audit log entry must be generated for SACL audit ACE");
    TEST_ASSERT(auditLogs.back().accessGranted == true, "Audit record must register successful access");
    TEST_ASSERT(auditLogs.back().objectName == L"TestSecurableObject", "Audit record object name must match");
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

    // 4. ALPC Port Section & Shared Memory Data View (Large Bulk Data Transfer)
    uintptr_t secId = serverComm->createSection(64 * 1024); // 64 KB section
    TEST_ASSERT(secId != 0, "ALPC port section allocation must succeed");
    auto section = serverComm->getSection(secId);
    TEST_ASSERT(section != nullptr, "Created ALPC port section must be retrievable");
    TEST_ASSERT(section->getSize() == 64 * 1024, "ALPC port section size must match 64KB");

    // Populate section memory with test pattern
    std::vector<uint8_t> bulkData(4096, 0xAA);
    bool writeOk = section->writeData(1024, bulkData);
    TEST_ASSERT(writeOk, "Writing to ALPC section must succeed");

    // Map section view for message passing
    lpc::AlpcDataView dataView{};
    bool viewOk = serverComm->createDataView(secId, 1024, 4096, dataView);
    TEST_ASSERT(viewOk, "Creating data view must succeed");
    TEST_ASSERT(dataView.viewSize == 4096, "Data view size must match 4096");
    TEST_ASSERT(dataView.sectionOffset == 1024, "Data view offset must match 1024");
    TEST_ASSERT(dataView.viewBase != nullptr, "Data view base pointer must not be null");

    // Read back through viewBase
    TEST_ASSERT(*reinterpret_cast<const uint8_t*>(dataView.viewBase) == 0xAA, "Data in view must match written pattern");

    // 5. ALPC Direct Message Attributes (Security, View, and Handle passing)
    lpc::AlpcMessageAttributes sendAttrs{};
    sendAttrs.validAttributes = lpc::ALPC_MESSAGE_SECURITY_ATTRIBUTE | lpc::ALPC_MESSAGE_VIEW_ATTRIBUTE | lpc::ALPC_MESSAGE_HANDLE_ATTRIBUTE;
    sendAttrs.view = dataView;
    sendAttrs.handle = lpc::AlpcHandleAttribute{ .flags = 0, .handle = 0x1004, .desiredAccess = 0x20001 };
    sendAttrs.security = lpc::AlpcSecurityAttribute{ 
        .flags = 0, 
        .userSid = se::Sid::localSystem(), 
        .integrityLevel = se::SECURITY_MANDATORY_SYSTEM_RID 
    };

    lpc::PortMessage attrMsg{};
    attrMsg.messageId = 42;
    std::string attrPayload = "BULK_TRANSFER_NOTIFICATION";
    std::vector<uint8_t> attrBytes(attrPayload.begin(), attrPayload.end());

    NtStatus postStatus = serverComm->sendMessage(attrMsg, attrBytes, &sendAttrs);
    TEST_ASSERT(NT_SUCCESS(postStatus), "Posting ALPC message with attributes must succeed");

    lpc::PortMessage rcvMsg{};
    std::vector<uint8_t> rcvPayload;
    lpc::AlpcMessageAttributes rcvAttrs{};
    NtStatus rcvStatus = clientComm->receiveMessage(rcvMsg, rcvPayload, 1000, &rcvAttrs);
    TEST_ASSERT(NT_SUCCESS(rcvStatus), "Receiving ALPC message with attributes must succeed");
    TEST_ASSERT(rcvAttrs.view.has_value(), "Received attributes must include view attribute");
    TEST_ASSERT(rcvAttrs.view->viewSize == 4096, "Received view size must match");
    TEST_ASSERT(rcvAttrs.handle.has_value() && rcvAttrs.handle->handle == 0x1004, "Received handle must match 0x1004");
    TEST_ASSERT(rcvAttrs.security.has_value() && rcvAttrs.security->userSid == se::Sid::localSystem(), "Received security SID must match LocalSystem");

    // 6. Port Security & Access Check Enforcement
    se::SecurityDescriptor securePortSd;
    se::Acl portDacl;
    portDacl.addAllowedAce(se::Sid::administrators(), se::GENERIC_ALL); // Admins only
    securePortSd.setDacl(portDacl);

    auto securedPort = portMgr.createPort(L"\\RPC Control\\SecuredAdminPort", securePortSd);
    TEST_ASSERT(securedPort != nullptr, "Secured port must be created");

    se::Sid userSid(5, {21, 999, 888, 777, 1003});
    auto standardUserToken = se::TokenObject::createUserToken(userSid);
    auto adminToken = se::TokenObject::createAdminToken(userSid);

    std::shared_ptr<lpc::PortObject> cPort;
    std::shared_ptr<lpc::PortObject> sPort;

    // Standard user connection must be denied by port Security Descriptor
    NtStatus stDenied = portMgr.connectPort(L"SecuredAdminPort", cPort, sPort, standardUserToken.get());
    TEST_ASSERT(stDenied == NtStatus::AccessDenied, "Standard user must be denied connection to admin-only ALPC port");

    // Admin connection must succeed
    NtStatus stAllowed = portMgr.connectPort(L"SecuredAdminPort", cPort, sPort, adminToken.get());
    TEST_ASSERT(NT_SUCCESS(stAllowed), "Administrator connection to secured ALPC port must succeed");
    TEST_ASSERT(cPort != nullptr && sPort != nullptr, "Admin connection endpoints must be valid");

    // 7. Graceful Disconnection (LpcPortClosed)
    cPort->disconnect();
    lpc::PortMessage closedMsg{};
    std::vector<uint8_t> closedPayload;
    NtStatus stClosed = sPort->receiveMessage(closedMsg, closedPayload, 1000);
    TEST_ASSERT(NT_SUCCESS(stClosed), "Server must receive disconnection event");
    TEST_ASSERT(closedMsg.u2.type == static_cast<uint16_t>(lpc::PortMessageType::LpcPortClosed), "Message type must be LpcPortClosed");
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

    // 3. Deferred Procedure Call (KDPC) & Automatic Retirement
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

    // Software Interrupt Dispatcher (DPC Retirement at DISPATCH_LEVEL)
    static bool s_AutoDpcRun = false;
    ke::KDPC autoDpc{
        .routine = [](ke::KDPC*, void*, void*, void*) { s_AutoDpcRun = true; },
        .deferredContext = nullptr
    };
    ke::KfRaiseIrql(ke::DISPATCH_LEVEL);
    dpcQueue.queueDpc(&autoDpc);
    ke::KiDispatchSoftwareInterrupt(ke::DISPATCH_LEVEL);
    ke::KeLowerIrql(ke::PASSIVE_LEVEL);
    TEST_ASSERT(s_AutoDpcRun, "DPC must be retired when KiDispatchSoftwareInterrupt is invoked at DISPATCH_LEVEL");


    // 4. Canonical KTrapFrame Architecture
    ke::KTrapFrame trapFrame{};
    trapFrame.rip = 0x00007FF710001000ULL;
    trapFrame.rsp = 0x000000A010008000ULL;
    trapFrame.rax = 0x24; // NT Syscall number
    trapFrame.rcx = 0xDEADBEEF;
    trapFrame.rdx = 0xCAFEBABE;
    trapFrame.segCs = 0x33; // Userland 64-bit CS
    trapFrame.segSs = 0x2B; // Userland 64-bit SS
    trapFrame.previousMode = 1; // UserMode
    TEST_ASSERT(trapFrame.rip == 0x00007FF710001000ULL, "TrapFrame RIP must match entry address");
    TEST_ASSERT(trapFrame.segCs == 0x33, "TrapFrame CS must be UserMode 0x33");
    TEST_ASSERT(trapFrame.previousMode == 1, "Previous mode must be UserMode (1)");

    // 5. Hardware Context Switcher (KiSwapContext)
    ke::KThread threadA{};
    threadA.tid = 1001;
    threadA.pid = 4;
    threadA.state = ke::ThreadState::Running;
    threadA.directoryTableBase = 0x1000;
    threadA.context.rip = 0x7FF70001;
    threadA.context.rsp = 0x1008000;

    ke::KThread threadB{};
    threadB.tid = 1002;
    threadB.pid = 8;
    threadB.state = ke::ThreadState::Ready;
    threadB.directoryTableBase = 0x2000;
    threadB.context.rip = 0x7FF70002;
    threadB.context.rsp = 0x2008000;

    ke::g_CurrentCpu.currentThread = &threadA;
    uint64_t prevSwitches = ke::g_CurrentCpu.contextSwitches;

    // Execute KiSwapContext: switch from threadA to threadB
    bool switchSuccess = ke::KiSwapContext(&threadA, &threadB);
    TEST_ASSERT(switchSuccess, "KiSwapContext must complete successfully");
    TEST_ASSERT(threadA.state == ke::ThreadState::Ready, "Outgoing thread must transition to Ready");
    TEST_ASSERT(threadB.state == ke::ThreadState::Running, "Incoming thread must transition to Running");
    TEST_ASSERT(ke::g_CurrentCpu.currentThread == &threadB, "CurrentThread pointer must point to threadB");
    TEST_ASSERT(ke::g_CurrentCpu.currentCr3 == 0x2000, "Address space CR3 must switch to threadB directoryTableBase");
    TEST_ASSERT(ke::g_CurrentCpu.contextSwitches == prevSwitches + 1, "Context switch count must increment");

    // 6. Asynchronous Procedure Calls (KAPC Subsystem)
    static bool s_KernelApcRan = false;
    static bool s_NormalApcRan = false;
    static bool s_UserApcRan = false;

    ke::KAPC kernelApc{};
    ke::KeInitializeApc(
        &kernelApc,
        threadB.tid,
        [](ke::KAPC*, ke::PKNORMAL_ROUTINE*, void**, void**, void**) {
            s_KernelApcRan = true;
        },
        nullptr,
        [](void*, void*, void*) {
            s_NormalApcRan = true;
        },
        0, // KernelMode
        nullptr
    );

    ke::KeInsertQueueApc(&kernelApc);
    TEST_ASSERT(!threadB.kernelApcList.empty(), "Thread kernel APC list must contain queued APC");

    // Deliver Kernel APC
    ke::KiDeliverApc(0);
    TEST_ASSERT(s_KernelApcRan, "Kernel APC routine must execute at APC_LEVEL");
    TEST_ASSERT(s_NormalApcRan, "Normal APC routine must execute at PASSIVE_LEVEL");

    // User-Mode Alertable APC
    ke::KAPC userApc{};
    ke::KeInitializeApc(
        &userApc,
        threadB.tid,
        nullptr,
        nullptr,
        [](void*, void*, void*) {
            s_UserApcRan = true;
        },
        1, // UserMode
        nullptr
    );
    threadB.alertable = true;
    ke::KeInsertQueueApc(&userApc);
    ke::KiDeliverApc(1); // Deliver user APC
    TEST_ASSERT(s_UserApcRan, "User APC routine must execute during alertable wait");
    ke::g_CurrentCpu.currentThread = nullptr;

    // 7. 32-Queue Priority Thread Scheduler & Anti-Starvation
    auto& scheduler = ke::PriorityScheduler::get();
    ke::ScheduledThreadEntry tIdle{ .tid = 0, .basePriority = ke::PRIORITY_IDLE, .currentPriority = ke::PRIORITY_IDLE, .name = "SystemIdle" };
    ke::ScheduledThreadEntry tNormal{ .tid = 100, .basePriority = ke::PRIORITY_NORMAL, .currentPriority = ke::PRIORITY_NORMAL, .name = "NormalWorker" };
    ke::ScheduledThreadEntry tRealtime{ .tid = 200, .basePriority = 24, .currentPriority = 24, .name = "RealTimeAudio" };
    ke::ScheduledThreadEntry tLow{ .tid = 300, .basePriority = 2, .currentPriority = 2, .name = "LowPriorityBackground" };

    scheduler.readyThread(tIdle);
    scheduler.readyThread(tNormal);
    scheduler.readyThread(tRealtime);
    scheduler.readyThread(tLow);

    // Test Starvation Scanner: Low priority thread (2) gets boosted to priority 15
    size_t boostedCount = scheduler.scanForStarvation();
    TEST_ASSERT(boostedCount >= 1, "Starvation scan must boost starved low-priority threads");

    // Highest priority (Realtime 24) must still be scheduled first
    auto next1 = scheduler.selectNextThread();
    TEST_ASSERT(next1.has_value(), "Scheduler must select runnable thread");
    TEST_ASSERT(next1->tid == 200, "Real-time thread 200 must be scheduled first");
    TEST_ASSERT(next1->currentPriority == 24, "Priority must be 24");

    // Next must be the boosted starved thread (15)
    auto next2 = scheduler.selectNextThread();
    TEST_ASSERT(next2.has_value() && next2->tid == 300, "Boosted thread 300 must be scheduled ahead of normal");
    TEST_ASSERT(next2->currentPriority == 15, "Boosted priority must be 15");

    // Next must be Normal (8)
    auto next3 = scheduler.selectNextThread();
    TEST_ASSERT(next3.has_value() && next3->tid == 100, "Normal thread 100 must be scheduled next");

    // Next must be Idle (0)
    auto next4 = scheduler.selectNextThread();
    TEST_ASSERT(next4.has_value() && next4->tid == 0, "Idle thread 0 must be scheduled last");

    // Queue empty
    auto next5 = scheduler.selectNextThread();
    TEST_ASSERT(!next5.has_value(), "Run queues must now be empty");
}

// ============================================================================
// Suite 12: Executive Support (EX) Pools, ERESOURCE, FastMutex, PushLock, Lookaside & S-List
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

    // 5. Executive Resource (ERESOURCE Lite)
    ex::ERESOURCE res{};
    ex::ExInitializeResourceLite(&res);
    TEST_ASSERT(res.activeCount == 0, "Initial active count must be 0");

    // Acquire Exclusive
    BOOLEAN acqEx1 = ex::ExAcquireResourceExclusiveLite(&res, TRUE);
    TEST_ASSERT(acqEx1 == TRUE, "Exclusive acquisition should succeed");
    TEST_ASSERT(res.exclusiveOwners == 1, "Exclusive owners count should be 1");
    TEST_ASSERT(res.activeCount == 1, "Active count should be 1");

    // Recursive Exclusive Acquire on same thread
    BOOLEAN acqEx2 = ex::ExAcquireResourceExclusiveLite(&res, TRUE);
    TEST_ASSERT(acqEx2 == TRUE, "Recursive exclusive acquisition on same thread should succeed");
    TEST_ASSERT(res.exclusiveOwners == 2, "Exclusive owners count should be 2 for recursion");

    // Convert Exclusive to Shared
    ex::ExConvertExclusiveToSharedLite(&res);
    TEST_ASSERT(res.exclusiveOwners == 0, "Exclusive owners count must be 0 after conversion");
    TEST_ASSERT(res.sharedOwners == 2, "Shared owners count should equal previous exclusive owners");
    TEST_ASSERT(res.activeCount == 2, "Active count must be 2");

    // Release both shared holds
    ex::ExReleaseResourceLite(&res);
    TEST_ASSERT(res.sharedOwners == 1, "Shared owners should be 1 after first release");
    ex::ExReleaseResourceLite(&res);
    TEST_ASSERT(res.sharedOwners == 0, "Shared owners should be 0 after second release");
    TEST_ASSERT(res.activeCount == 0, "Resource activeCount should be 0");

    // Shared acquire and release
    BOOLEAN acqShared = ex::ExAcquireResourceSharedLite(&res, TRUE);
    TEST_ASSERT(acqShared == TRUE, "Shared acquisition should succeed");
    TEST_ASSERT(res.sharedOwners == 1, "Shared owners count should be 1");
    ex::ExReleaseResourceLite(&res);
    TEST_ASSERT(res.sharedOwners == 0, "Resource must be free after release");
    ex::ExDeleteResourceLite(&res);

    // 6. Fast Mutex (FAST_MUTEX) & IRQL transition to APC_LEVEL
    ex::FAST_MUTEX fastMutex{};
    ex::ExInitializeFastMutex(&fastMutex);
    TEST_ASSERT(ke::KeGetCurrentIrql() == ke::PASSIVE_LEVEL, "Initial IRQL must be PASSIVE_LEVEL");

    ex::ExAcquireFastMutex(&fastMutex);
    TEST_ASSERT(ke::KeGetCurrentIrql() == ke::APC_LEVEL, "ExAcquireFastMutex must raise IRQL to APC_LEVEL");
    TEST_ASSERT(fastMutex.owner != 0, "Fast mutex owner must be set");
    TEST_ASSERT(fastMutex.count.load() == 0, "Fast mutex count must be 0 when held");

    ex::ExReleaseFastMutex(&fastMutex);
    TEST_ASSERT(ke::KeGetCurrentIrql() == ke::PASSIVE_LEVEL, "ExReleaseFastMutex must restore IRQL to PASSIVE_LEVEL");
    TEST_ASSERT(fastMutex.owner == 0, "Fast mutex owner must be cleared");
    TEST_ASSERT(fastMutex.count.load() == 1, "Fast mutex count must be 1 when unheld");

    BOOLEAN tryAcq = ex::ExTryToAcquireFastMutex(&fastMutex);
    TEST_ASSERT(tryAcq == TRUE, "ExTryToAcquireFastMutex must succeed on free mutex");
    TEST_ASSERT(ke::KeGetCurrentIrql() == ke::APC_LEVEL, "ExTryToAcquireFastMutex must raise IRQL to APC_LEVEL");
    ex::ExReleaseFastMutex(&fastMutex);
    TEST_ASSERT(ke::KeGetCurrentIrql() == ke::PASSIVE_LEVEL, "IRQL must be PASSIVE_LEVEL after release");

    // 7. Executive Push Lock (EX_PUSH_LOCK)
    ex::EX_PUSH_LOCK pushLock{};
    ex::ExInitializePushLock(&pushLock);
    TEST_ASSERT(pushLock.value == 0, "Push lock initial value must be 0");

    ex::ExAcquirePushLockShared(&pushLock);
    TEST_ASSERT(pushLock.shared == 1, "Push lock shared count must be 1");
    ex::ExAcquirePushLockShared(&pushLock);
    TEST_ASSERT(pushLock.shared == 2, "Push lock shared count must be 2");
    ex::ExReleasePushLockShared(&pushLock);
    TEST_ASSERT(pushLock.shared == 1, "Push lock shared count must decrement to 1");
    ex::ExReleasePushLockShared(&pushLock);
    TEST_ASSERT(pushLock.shared == 0, "Push lock shared count must be 0");

    ex::ExAcquirePushLockExclusive(&pushLock);
    TEST_ASSERT(pushLock.locked == 1, "Push lock locked bit must be set for exclusive");
    ex::ExReleasePushLockExclusive(&pushLock);
    TEST_ASSERT(pushLock.locked == 0, "Push lock locked bit must be cleared after exclusive release");

    // 8. Interlocked Singly-Linked Lists (SLIST)
    ex::SLIST_HEADER slistHead{};
    ex::InitializeSListHead(&slistHead);
    TEST_ASSERT(ex::QueryDepthSList(&slistHead) == 0, "Initial SLIST depth must be 0");

    struct TestSListEntry {
        ex::SLIST_ENTRY slistEntry;
        uint32_t value;
    };
    TestSListEntry entry1{};
    entry1.value = 100;
    TestSListEntry entry2{};
    entry2.value = 200;

    ex::InterlockedPushEntrySList(&slistHead, &entry1.slistEntry);
    TEST_ASSERT(ex::QueryDepthSList(&slistHead) == 1, "SLIST depth must be 1 after pushing entry1");

    ex::InterlockedPushEntrySList(&slistHead, &entry2.slistEntry);
    TEST_ASSERT(ex::QueryDepthSList(&slistHead) == 2, "SLIST depth must be 2 after pushing entry2");

    auto* popped = ex::InterlockedPopEntrySList(&slistHead);
    TEST_ASSERT(popped == &entry2.slistEntry, "LIFO pop must return most recently pushed entry2");
    TEST_ASSERT(ex::QueryDepthSList(&slistHead) == 1, "SLIST depth must be 1 after pop");

    auto* flushed = ex::InterlockedFlushSList(&slistHead);
    TEST_ASSERT(flushed == &entry1.slistEntry, "InterlockedFlushSList must return remaining list");
    TEST_ASSERT(ex::QueryDepthSList(&slistHead) == 0, "SLIST depth must be 0 after flush");

    // 9. NonPaged and Paged Lookaside Lists (NPAGED_LOOKASIDE_LIST, PAGED_LOOKASIDE_LIST)
    ex::NPAGED_LOOKASIDE_LIST npLookaside{};
    ex::ExInitializeNPagedLookasideList(
        &npLookaside,
        nullptr,
        nullptr,
        0,
        128,
        ex::TAG_LOOKASIDE,
        4
    );
    TEST_ASSERT(npLookaside.l.depth == 4, "Lookaside list depth must be initialized to 4");

    void* pBlock1 = ex::ExAllocateFromNPagedLookasideList(&npLookaside);
    TEST_ASSERT(pBlock1 != nullptr, "Allocate from NPaged Lookaside must succeed");
    TEST_ASSERT(npLookaside.l.totalAllocates == 1, "totalAllocates must increment");
    TEST_ASSERT(npLookaside.l.allocateMisses == 1, "First allocate should be a miss (empty cache)");

    ex::ExFreeToNPagedLookasideList(&npLookaside, pBlock1);
    TEST_ASSERT(npLookaside.l.totalFrees == 1, "totalFrees must increment");
    TEST_ASSERT(ex::QueryDepthSList(&npLookaside.l.listHead) == 1, "Lookaside list depth must be 1 after returning block");

    // Allocate again: should be a cache hit!
    void* pBlock2 = ex::ExAllocateFromNPagedLookasideList(&npLookaside);
    TEST_ASSERT(pBlock2 == pBlock1, "Cache hit must reuse returned buffer");
    TEST_ASSERT(npLookaside.l.totalAllocates == 2, "totalAllocates should now be 2");
    TEST_ASSERT(npLookaside.l.allocateMisses == 1, "allocateMisses should remain 1 on hit");
    TEST_ASSERT(ex::QueryDepthSList(&npLookaside.l.listHead) == 0, "Lookaside list depth must be 0 after popping cache");

    ex::ExFreeToNPagedLookasideList(&npLookaside, pBlock2);
    ex::ExDeleteNPagedLookasideList(&npLookaside);

    // Paged Lookaside List with IRQL Guard Verification
    ex::PAGED_LOOKASIDE_LIST pagedLookaside{};
    ex::ExInitializePagedLookasideList(
        &pagedLookaside,
        nullptr,
        nullptr,
        0,
        256,
        ex::TAG_LOOKASIDE,
        4
    );

    void* pPagedBlock = ex::ExAllocateFromPagedLookasideList(&pagedLookaside);
    TEST_ASSERT(pPagedBlock != nullptr, "Paged lookaside allocate at PASSIVE_LEVEL must succeed");
    ex::ExFreeToPagedLookasideList(&pagedLookaside, pPagedBlock);

    // Attempting Paged Lookaside allocate at DISPATCH_LEVEL must fail
    ke::KIRQL irqlDpc = ke::KfRaiseIrql(ke::DISPATCH_LEVEL);
    void* pPagedIllegal = ex::ExAllocateFromPagedLookasideList(&pagedLookaside);
    TEST_ASSERT(pPagedIllegal == nullptr, "Paged lookaside allocate at DISPATCH_LEVEL must fail and return nullptr");
    ke::KeLowerIrql(irqlDpc);

    ex::ExDeletePagedLookasideList(&pagedLookaside);

    // 10. System Time Architecture (KeQuerySystemTime)
    uint64_t sysTime = ex::KeQuerySystemTime();
    TEST_ASSERT(sysTime > 116444736000000000ULL, "KeQuerySystemTime must return valid NT epoch timestamp > 1601");
}

// ============================================================================
// Suite 13: Trap & Fault Engine, SEH, BugCheck, and Kernel Debugger (KD64/DR0-DR7)
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

    // 4. KD64 Packet Framing, Protocol & Checksums
    auto& kdDbg = kd::KdDebugger::get();
    kdDbg.initialize(true);
    TEST_ASSERT(kdDbg.isDebuggerEnabled(), "Kernel debugger must be enabled");
    TEST_ASSERT(!kdDbg.isDebuggerNotPresent(), "Debugger present flag must be active");

    auto& transport = kdDbg.getTransport();
    transport.reset();

    // Checksum verification
    const uint8_t testPayload[] = { 0x10, 0x20, 0x30, 0x40, 0x50 };
    uint32_t expectedCrc = 0x10 + 0x20 + 0x30 + 0x40 + 0x50; // 0xF0
    TEST_ASSERT(kd::KdComputeChecksum(testPayload, sizeof(testPayload)) == expectedCrc, "KD checksum calculation must match byte sum");

    // Send packet over transport
    bool sendOk = transport.sendPacket(kd::PACKET_TYPE_KD_STATE_CHANGE64, testPayload, sizeof(testPayload));
    TEST_ASSERT(sendOk, "Sending KD packet over transport must succeed");
    TEST_ASSERT(transport.getOutboundQueueCount() == 1, "Transport outbound queue must contain 1 packet");

    auto rawOutPkt = transport.popOutboundPacket();
    TEST_ASSERT(rawOutPkt.size() == sizeof(kd::KD_PACKET) + sizeof(testPayload), "Raw packet size must match header + payload");
    const auto* hdr = reinterpret_cast<const kd::KD_PACKET*>(rawOutPkt.data());
    TEST_ASSERT(hdr->packetLeader == kd::PACKET_LEADER, "Packet leader must be 0x30303030 ('0000')");
    TEST_ASSERT(hdr->packetType == kd::PACKET_TYPE_KD_STATE_CHANGE64, "Packet type must be STATE_CHANGE64");
    TEST_ASSERT(hdr->byteCount == sizeof(testPayload), "Payload byte count must match");
    TEST_ASSERT(hdr->checksum == expectedCrc, "Header checksum must match payload checksum");

    // Receive packet round-trip
    transport.deliverInbound(rawOutPkt);
    kd::KD_PACKET rcvHdr{};
    std::vector<uint8_t> rcvPayload;
    bool rcvOk = transport.receivePacket(rcvHdr, rcvPayload);
    TEST_ASSERT(rcvOk, "Receiving valid packet must succeed");
    TEST_ASSERT(rcvHdr.packetType == kd::PACKET_TYPE_KD_STATE_CHANGE64, "Received packet type must match");
    TEST_ASSERT(rcvPayload.size() == sizeof(testPayload), "Received payload size must match");
    TEST_ASSERT(std::memcmp(rcvPayload.data(), testPayload, sizeof(testPayload)) == 0, "Payload contents must match exactly");

    // Acknowledge packet
    transport.sendAcknowledge(rcvHdr.packetId);
    auto ackRaw = transport.popOutboundPacket();
    const auto* ackHdr = reinterpret_cast<const kd::KD_PACKET*>(ackRaw.data());
    TEST_ASSERT(ackHdr->packetLeader == kd::CONTROL_PACKET_LEADER, "Control packet leader must be 0x69696969 ('iiii')");
    TEST_ASSERT(ackHdr->packetType == kd::PACKET_TYPE_KD_ACKNOWLEDGE, "Control packet type must be KD_ACKNOWLEDGE");
    TEST_ASSERT(ackHdr->packetId == rcvHdr.packetId, "ACK packet ID must match acknowledged packet ID");

    // 5. KD64 State Manipulate Protocol
    kd::DBGKD_MANIPULATE_STATE64 req{};
    kd::DBGKD_MANIPULATE_STATE64 reply{};
    std::vector<uint8_t> reqData;
    std::vector<uint8_t> replyData;

    // DbgKdGetVersionApi
    req.apiNumber = kd::DbgKdGetVersionApi;
    bool verOk = kdDbg.handleStateManipulate(req, reqData, reply, replyData);
    TEST_ASSERT(verOk, "DbgKdGetVersionApi must succeed");
    TEST_ASSERT(reply.returnStatus == static_cast<uint32_t>(NtStatus::Success), "Version API status must be Success");
    TEST_ASSERT(reply.getVersion.majorVersion == 0x000F, "Major version must be 0xF (Free build)");
    TEST_ASSERT(reply.getVersion.minorVersion == 26100, "Minor version must match 26100");
    TEST_ASSERT(reply.getVersion.machineType == 0x8664, "Machine type must be AMD64 (0x8664)");
    TEST_ASSERT(reply.getVersion.kernBase == 0xFFFFF80000000000ULL, "KernBase must be 0xFFFFF80000000000");

    // DbgKdReadVirtualMemoryApi64
    req.apiNumber = kd::DbgKdReadVirtualMemoryApi64;
    req.readMemory.targetBaseAddress = 0x140000000;
    req.readMemory.transferCount = 64;
    bool readOk = kdDbg.handleStateManipulate(req, reqData, reply, replyData);
    TEST_ASSERT(readOk, "DbgKdReadVirtualMemoryApi64 must succeed");
    TEST_ASSERT(reply.readMemory.actualBytesRead == 64, "Actual bytes read must match 64");
    TEST_ASSERT(replyData.size() == 64, "Reply data size must match 64 bytes");

    // DbgKdWriteBreakPointApi & ContinueApi
    req.apiNumber = kd::DbgKdWriteBreakPointApi;
    req.breakPoint.breakPointAddress = 0x140001000;
    bool bpOk = kdDbg.handleStateManipulate(req, reqData, reply, replyData);
    TEST_ASSERT(bpOk, "DbgKdWriteBreakPointApi must succeed");
    TEST_ASSERT(reply.breakPoint.breakPointHandle != 0, "Breakpoint handle must be non-zero");

    req.apiNumber = kd::DbgKdContinueApi;
    bool contOk = kdDbg.handleStateManipulate(req, reqData, reply, replyData);
    TEST_ASSERT(contOk, "DbgKdContinueApi must succeed");

    // Poll break-in signaling
    TEST_ASSERT(!kdDbg.pollBreakIn(), "Initial break-in pending should be false");
    kdDbg.signalBreakIn();
    TEST_ASSERT(kdDbg.pollBreakIn(), "Break-in request must be detected by pollBreakIn");
    TEST_ASSERT(!kdDbg.pollBreakIn(), "Break-in request must be cleared after polling");

    // 6. Hardware Breakpoint Trap Engine (DR0-DR7)
    auto& hw = kdDbg.getHwBreakpoints();

    // DR0: Execution breakpoint on 0x140005000 (1 byte)
    bool setDr0 = hw.setBreakpoint(0, 0x140005000, kd::HwBreakpointType::Execute, kd::HwBreakpointLength::Byte1);
    TEST_ASSERT(setDr0, "Setting DR0 execution breakpoint must succeed");
    TEST_ASSERT((hw.getRegisters().dr7 & 0x1) == 0x1, "DR7 L0 bit must be set");

    // DR1: Write breakpoint on 0x20001000 (4 bytes)
    bool setDr1 = hw.setBreakpoint(1, 0x20001000, kd::HwBreakpointType::Write, kd::HwBreakpointLength::Byte4);
    TEST_ASSERT(setDr1, "Setting DR1 write breakpoint must succeed");
    TEST_ASSERT((hw.getRegisters().dr7 & 0x4) == 0x4, "DR7 L1 bit must be set");

    // DR2: Read/Write breakpoint on 0x30002000 (8 bytes)
    bool setDr2 = hw.setBreakpoint(2, 0x30002000, kd::HwBreakpointType::ReadWrite, kd::HwBreakpointLength::Byte8);
    TEST_ASSERT(setDr2, "Setting DR2 read/write breakpoint must succeed");
    TEST_ASSERT((hw.getRegisters().dr7 & 0x10) == 0x10, "DR7 L2 bit must be set");

    // Evaluate instruction fetch on DR0 address
    uint64_t dr6Status = 0;
    bool hitExec = hw.evaluateAccess(0x140005000, 1, false, true, dr6Status);
    TEST_ASSERT(hitExec, "Instruction fetch on DR0 address must trigger hardware breakpoint");
    TEST_ASSERT((dr6Status & 0x1) == 0x1, "DR6 B0 bit must be set for DR0 match");

    // Evaluate read on DR1 (Write-only) -> should NOT trigger
    bool hitReadOnWriteOnly = hw.evaluateAccess(0x20001000, 4, false, false, dr6Status);
    TEST_ASSERT(!hitReadOnWriteOnly, "Read access on Write-only breakpoint must NOT trigger");

    // Evaluate write on DR1 (Write-only) -> should trigger
    bool hitWrite = hw.evaluateAccess(0x20001000, 4, true, false, dr6Status);
    TEST_ASSERT(hitWrite, "Write access on DR1 address must trigger hardware breakpoint");
    TEST_ASSERT((dr6Status & 0x2) == 0x2, "DR6 B1 bit must be set for DR1 match");

    // Evaluate read on DR2 (ReadWrite) -> should trigger
    bool hitReadWrite = hw.evaluateAccess(0x30002000, 8, false, false, dr6Status);
    TEST_ASSERT(hitReadWrite, "Read access on ReadWrite breakpoint must trigger");
    TEST_ASSERT((dr6Status & 0x4) == 0x4, "DR6 B2 bit must be set for DR2 match");

    // Evaluate unrelated address -> should NOT trigger
    bool hitMiss = hw.evaluateAccess(0x99990000, 4, true, false, dr6Status);
    TEST_ASSERT(!hitMiss, "Unrelated address access must not trigger any hardware breakpoint");

    // Clear DR0
    bool clrDr0 = hw.clearBreakpoint(0);
    TEST_ASSERT(clrDr0, "Clearing DR0 must succeed");
    TEST_ASSERT((hw.getRegisters().dr7 & 0x1) == 0, "DR7 L0 bit must be cleared");

    // 7. DbgPrint and DbgPrintEx Engine
    auto& ringBuf = kdDbg.getRingBuffer();
    ringBuf.clear();

    kd::DbgPrint("[MicaNT:Kernel] Subsystem %s initialized at IRQL %d\n", "Microkernel", 0);
    kd::DbgPrintEx(0, 0, "[MicaNT:HAL] ACPI 6.5 tables detected: 4 CPUs\n");

    auto logs = ringBuf.getLogs();
    TEST_ASSERT(logs.size() == 2, "Ring buffer must contain 2 logged entries");
    TEST_ASSERT(logs[0].find("Microkernel") != std::string::npos, "First log must contain 'Microkernel'");
    TEST_ASSERT(logs[1].find("ACPI 6.5") != std::string::npos, "Second log must contain 'ACPI 6.5'");

    // Verify DbgPrint was forwarded to KD transport as PACKET_TYPE_KD_DEBUG_IO
    TEST_ASSERT(transport.getOutboundQueueCount() >= 2, "DbgPrint calls must generate KD_DEBUG_IO packets");
}

// ============================================================================
// Suite 14: Hardware Abstraction Layer (HAL) APIC/IOAPIC, KPCR, DMA, and Timers
// ============================================================================
void Test_HardwareAbstractionLayer_KPCRAndTimers() {
    auto& hal = hal::HardwareAbstractionLayer::get();
    hal.initialize(4, hal::ProcessorArchitecture::Amd64, 3600);
    TEST_ASSERT(hal.getProcessorCount() == 4, "HAL must detect 4 processor cores");

    // 1. KPCR and KPRCB Topology
    auto* kpcr0 = hal.getKpcr(0);
    TEST_ASSERT(kpcr0 != nullptr, "KPCR 0 must not be null");
    TEST_ASSERT(kpcr0->self == kpcr0, "KPCR self pointer must point to itself");
    TEST_ASSERT(kpcr0->prcb.cpuId == 0, "CPU ID must be 0");
    TEST_ASSERT(kpcr0->prcb.architecture == hal::ProcessorArchitecture::Amd64, "Architecture must be AMD64");
    TEST_ASSERT(kpcr0->prcb.coreClockMhz == 3600, "Core clock must be 3600 MHz");

    // 2. High-Precision Performance Counter & Processor Stalling
    LargeInteger c1{};
    LargeInteger c2{};
    LargeInteger freq{};
    hal::KeQueryPerformanceCounter(c1, &freq);
    TEST_ASSERT(freq.quadPart == 1'000'000'000, "Performance frequency must be 1 GHz (nanosecond precision)");

    hal::KeStallExecutionProcessor(50); // Stall 50 microseconds
    hal::KeQueryPerformanceCounter(c2, &freq);
    TEST_ASSERT(c2.quadPart > c1.quadPart, "Performance counter must strictly advance over stall");

    // 3. Clock Tick & LAPIC EOI Dispatch
    uint64_t initialInterrupts = kpcr0->prcb.interruptsServiced;
    hal.dispatchClockTick(0);
    hal.dispatchClockTick(0);
    TEST_ASSERT(kpcr0->prcb.interruptsServiced == initialInterrupts + 2, "Dispatched clock ticks must increment interrupt count");

    // 4. Local APIC (LAPIC) Register Architecture & TPR Mapping
    auto* lapic0 = hal.getLocalApic(0);
    TEST_ASSERT(lapic0 != nullptr, "Local APIC 0 must be present");
    TEST_ASSERT(lapic0->getApicId() == 0, "LAPIC ID must match CPU 0");
    TEST_ASSERT(lapic0->isEnabled(), "LAPIC must be enabled by default");
    TEST_ASSERT((lapic0->readRegister(hal::APIC_REG_VERSION) & 0xFF) == 0x14, "LAPIC version must report 0x14 integrated APIC");

    // Task Priority Register (TPR) mapping: IRQL (0..15) -> TPR (IRQL << 4)
    lapic0->setTaskPriority(0x80); // IRQL 8 (DISPATCH_LEVEL)
    TEST_ASSERT(lapic0->getTaskPriority() == 0x80, "LAPIC TPR must reflect 0x80 (DISPATCH_LEVEL)");
    lapic0->setTaskPriority(0x00); // PASSIVE_LEVEL

    // Interrupt Request Register (IRR) and In-Service Register (ISR)
    uint8_t testVector = 0x62;
    lapic0->requestInterrupt(testVector);
    TEST_ASSERT(lapic0->isInterruptPending(testVector), "Requested interrupt must be pending in IRR");
    TEST_ASSERT(!lapic0->isInterruptInService(testVector), "Pending interrupt must not yet be in ISR");

    lapic0->markInterruptInService(testVector);
    TEST_ASSERT(!lapic0->isInterruptPending(testVector), "Servicing interrupt must clear IRR");
    TEST_ASSERT(lapic0->isInterruptInService(testVector), "Servicing interrupt must set ISR");

    lapic0->signalEoi();
    TEST_ASSERT(!lapic0->isInterruptInService(testVector), "EOI must clear highest in-service interrupt in ISR");

    // 5. I/O APIC Redirection Table & IRQ Routing
    auto& ioApic = hal.getIoApic();
    TEST_ASSERT(ioApic.getBaseAddress() == hal::DEFAULT_IOAPIC_BASE, "IO-APIC base address must match 0xFEC00000");

    // Route Keyboard IRQ 1 to Vector 0x31 on Core 0
    bool routed = ioApic.routeIrq(1, 0x31, 0, hal::InterruptMode::Latched);
    TEST_ASSERT(routed, "Routing IRQ 1 on IO-APIC must succeed");
    const auto* redir1 = ioApic.getRedirection(1);
    TEST_ASSERT(redir1 != nullptr, "Redirection entry 1 must exist");
    TEST_ASSERT(redir1->vector == 0x31, "Redirection vector must be 0x31");
    TEST_ASSERT(redir1->destinationApicId == 0, "Destination APIC ID must be 0");
    TEST_ASSERT(!redir1->masked, "Routed IRQ must be unmasked");

    // Mask IRQ 1
    ioApic.maskIrq(1, true);
    TEST_ASSERT(ioApic.getRedirection(1)->masked, "Masking IRQ 1 must update redirection table");

    // 6. System Interrupt Routing & IRQL Elevation Lifecycles
    bool enabledInt = hal::HalEnableSystemInterrupt(0x50, ke::HIGH_LEVEL, hal::InterruptMode::LevelSensitive);
    TEST_ASSERT(enabledInt, "HalEnableSystemInterrupt must succeed");

    ke::KIRQL oldIrql = ke::PASSIVE_LEVEL;
    bool beginOk = hal::HalBeginSystemInterrupt(ke::HIGH_LEVEL, 0x50, &oldIrql);
    TEST_ASSERT(beginOk, "HalBeginSystemInterrupt must succeed");
    TEST_ASSERT(ke::KeGetCurrentIrql() == ke::HIGH_LEVEL, "Current IRQL must be raised to HIGH_LEVEL");
    TEST_ASSERT(lapic0->isInterruptInService(0x50), "Interrupt 0x50 must be marked in-service in LAPIC");

    hal::HalEndSystemInterrupt(oldIrql, 0x50);
    TEST_ASSERT(ke::KeGetCurrentIrql() == ke::PASSIVE_LEVEL, "Current IRQL must be restored to PASSIVE_LEVEL");
    TEST_ASSERT(!lapic0->isInterruptInService(0x50), "LAPIC ISR must be cleared after HalEndSystemInterrupt EOI");

    // 7. Inter-Processor Interrupts (IPI) & SMP TLB Shootdown
    auto* kpcr1 = hal.getKpcr(1);
    auto* kpcr2 = hal.getKpcr(2);
    auto* kpcr3 = hal.getKpcr(3);
    TEST_ASSERT(kpcr1 && kpcr2 && kpcr3, "All 4 SMP processor KPCRs must be valid");

    // Dispatch IPI from CPU 0 to CPU 1
    bool ipiSent = hal::HalRequestIpi(1, hal::IpiType::Dispatch);
    TEST_ASSERT(ipiSent, "HalRequestIpi to CPU 1 must succeed");
    TEST_ASSERT(kpcr0->prcb.ipiSentCount >= 1, "CPU 0 must record sent IPI");
    TEST_ASSERT(kpcr1->prcb.ipiReceivedCount >= 1, "CPU 1 must record received IPI");

    // Broadcast SMP TLB Shootdown across all secondary cores
    uint64_t prevTlb1 = kpcr1->prcb.tlbFlushRequests;
    uint64_t prevTlb2 = kpcr2->prcb.tlbFlushRequests;
    uint64_t prevTlb3 = kpcr3->prcb.tlbFlushRequests;
    hal.broadcastTlbFlush(0);
    TEST_ASSERT(kpcr1->prcb.tlbFlushRequests == prevTlb1 + 1, "CPU 1 must receive TLB flush shootdown");
    TEST_ASSERT(kpcr2->prcb.tlbFlushRequests == prevTlb2 + 1, "CPU 2 must receive TLB flush shootdown");
    TEST_ASSERT(kpcr3->prcb.tlbFlushRequests == prevTlb3 + 1, "CPU 3 must receive TLB flush shootdown");

    // 8. Direct Memory Access (DMA) & Scatter-Gather Engine
    hal::DEVICE_DESCRIPTION devDesc{};
    devDesc.interfaceType = hal::InterfaceType::PCIBus;
    devDesc.master = true;
    devDesc.scatterGather = true;
    devDesc.dma64BitAddresses = true;
    devDesc.maximumLength = 64 * 1024; // 64KB max DMA burst

    uint32_t mapRegs = 0;
    auto dmaAdapter = hal::HalGetAdapter(&devDesc, &mapRegs);
    TEST_ASSERT(dmaAdapter != nullptr, "HalGetAdapter must return valid DMA adapter");
    TEST_ASSERT(mapRegs == 16, "64KB DMA transfer must require 16 4KB map registers");

    // Allocate contiguous DMA common buffer
    uint64_t logicalPhysAddr = 0;
    void* dmaBuffer = dmaAdapter->allocateCommonBuffer(8192, &logicalPhysAddr, false);
    TEST_ASSERT(dmaBuffer != nullptr, "DMA common buffer allocation must succeed");
    TEST_ASSERT(logicalPhysAddr != 0, "DMA logical physical address must be non-zero");

    // Write test pattern and verify physical memory mapping
    std::memset(dmaBuffer, 0x5A, 8192);
    TEST_ASSERT(reinterpret_cast<uint8_t*>(dmaBuffer)[0] == 0x5A, "DMA memory contents must match pattern");

    // Generate Scatter-Gather list across multi-page buffer
    auto sgList = dmaAdapter->buildScatterGatherList(reinterpret_cast<uintptr_t>(dmaBuffer), 8192);
    TEST_ASSERT(sgList.numberOfElements == 2, "8KB buffer must generate 2 4KB scatter-gather elements");
    TEST_ASSERT(sgList.elements[0].length == 4096, "First scatter-gather element length must be 4096");
    TEST_ASSERT(sgList.elements[1].length == 4096, "Second scatter-gather element length must be 4096");

    dmaAdapter->freeCommonBuffer(8192, logicalPhysAddr, dmaBuffer, false);

    // 9. Real-Time Clock (RTC) Subsystem
    hal::TIME_FIELDS rtcIn{2026, 10, 10, 15, 30, 0, 0, 6};
    hal::HalSetRealTimeClock(&rtcIn);

    hal::TIME_FIELDS rtcOut{};
    hal::HalQueryRealTimeClock(&rtcOut);
    TEST_ASSERT(rtcOut.year == 2026, "RTC year must match 2026");
    TEST_ASSERT(rtcOut.month == 10, "RTC month must match 10");
    TEST_ASSERT(rtcOut.hour == 15, "RTC hour must match 15");
    TEST_ASSERT(rtcOut.minute == 30, "RTC minute must match 30");
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
        if (!irp->systemBuffer || irp->byteOffset.highPart < static_cast<int32_t>(sizeof(uint32_t))) {
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

    // 10. Inbv (Initialization Boot Video) Subsystem & Display Ownership
    auto& inbvCtrl = inbv::InbvController::get();
    bool inbvInit = inbvCtrl.initialize(640, 480);
    TEST_ASSERT(inbvInit, "InbvController initialization must succeed");
    TEST_ASSERT(!inbv::InbvCheckDisplayOwnership(), "Display must initially be owned by firmware GOP");
    TEST_ASSERT(inbvCtrl.getOwnershipState() == inbv::DisplayOwnership::FirmwareGop, "Initial ownership state must be FirmwareGop");

    inbv::InbvEnableBootDriver(TRUE);
    TEST_ASSERT(inbvCtrl.isBootDriverEnabled(), "Boot driver must be enabled");

    // Firmware GOP to Inbv Hand-off
    inbv::InbvAcquireDisplayOwnership();
    TEST_ASSERT(inbv::InbvCheckDisplayOwnership(), "Inbv must hold display ownership after acquire");
    TEST_ASSERT(inbvCtrl.getOwnershipState() == inbv::DisplayOwnership::InbvDriver, "Ownership state must be InbvDriver");

    // Palette & Solid Color Fill
    inbv::InbvSolidColorFill(50, 50, 150, 150, inbv::INBV_COLOR_LIGHT_CYAN);
    bootvid::Color expectedCyan = inbv::InbvGetPaletteColor(inbv::INBV_COLOR_LIGHT_CYAN);
    TEST_ASSERT(inbvCtrl.getDriver().getPixel(100, 100) == expectedCyan, "InbvSolidColorFill must plot palette light cyan");

    // Text & Progress Bar
    inbv::InbvSetTextColor(inbv::INBV_COLOR_YELLOW);
    TEST_ASSERT(inbvCtrl.getTextColor() == inbv::INBV_COLOR_YELLOW, "Text color must match yellow palette index");
    inbv::InbvDisplayString("MicaNT Kernel Initializing...");
    inbv::InbvSetProgressBarCoordinates(100, 400);
    inbv::InbvUpdateProgressBar(65);
    TEST_ASSERT(inbvCtrl.getProgressPercentage() == 65, "Inbv progress bar must update to 65%");

    // 11. Inbv to Display Driver (WDDM) Hand-off Integration
    static bool s_OwnershipLostCallbackFired = false;
    inbvCtrl.registerOwnershipLostCallback([]() {
        s_OwnershipLostCallbackFired = true;
    });

    inbv::InbvNotifyDisplayOwnershipLost(TRUE);
    TEST_ASSERT(s_OwnershipLostCallbackFired, "Ownership lost callback must be invoked during display hand-off");
    TEST_ASSERT(!inbv::InbvCheckDisplayOwnership(), "Inbv must no longer own display after hand-off");
    TEST_ASSERT(inbvCtrl.getOwnershipState() == inbv::DisplayOwnership::DisplayDriver, "Ownership state must transition to DisplayDriver");

    // After hand-off, Inbv rendering is protected/blocked
    inbv::InbvSolidColorFill(50, 50, 150, 150, inbv::INBV_COLOR_LIGHT_RED);
    TEST_ASSERT(inbvCtrl.getDriver().getPixel(100, 100) == bootvid::Color::black(),
                "Inbv drawing after display driver hand-off must be a no-op");

    // 12. Framebuffer Pixel Format Conversion & Transcoding Engine
    bootvid::Color testCol(210, 140, 70); // R=210, G=140, B=70

    // BGRA32 <-> RGBA32 <-> RGB24 <-> RGB565
    uint32_t bgraVal = inbv::ColorToPixel(testCol, inbv::PixelFormat::Bgra32);
    uint32_t rgbaVal = inbv::ColorToPixel(testCol, inbv::PixelFormat::Rgba32);
    uint32_t rgb24Val = inbv::ColorToPixel(testCol, inbv::PixelFormat::Rgb24);
    uint32_t rgb565Val = inbv::ColorToPixel(testCol, inbv::PixelFormat::Rgb565);

    bootvid::Color fromBgra = inbv::PixelToColor(bgraVal, inbv::PixelFormat::Bgra32);
    bootvid::Color fromRgba = inbv::PixelToColor(rgbaVal, inbv::PixelFormat::Rgba32);
    bootvid::Color fromRgb24 = inbv::PixelToColor(rgb24Val, inbv::PixelFormat::Rgb24);
    bootvid::Color fromRgb565 = inbv::PixelToColor(rgb565Val, inbv::PixelFormat::Rgb565);

    TEST_ASSERT(fromBgra == testCol, "BGRA round-trip must be lossless");
    TEST_ASSERT(fromRgba == testCol, "RGBA round-trip must be lossless");
    TEST_ASSERT(fromRgb24 == testCol, "RGB24 round-trip must be lossless");
    TEST_ASSERT(std::abs(static_cast<int>(fromRgb565.r) - testCol.r) <= 8, "RGB565 red channel must be within 5-bit quant tolerance");
    TEST_ASSERT(std::abs(static_cast<int>(fromRgb565.g) - testCol.g) <= 4, "RGB565 green channel must be within 6-bit quant tolerance");
    TEST_ASSERT(std::abs(static_cast<int>(fromRgb565.b) - testCol.b) <= 8, "RGB565 blue channel must be within 5-bit quant tolerance");

    // BlitConvertFramebuffer test
    std::vector<uint32_t> srcFb(64 * 64, testCol.toBgra32());
    std::vector<uint32_t> dstFb(64 * 64, 0);
    bool blitOk = inbv::BlitConvertFramebuffer(
        srcFb.data(), inbv::PixelFormat::Bgra32, 64,
        dstFb.data(), inbv::PixelFormat::Rgba32, 64,
        64, 64
    );
    TEST_ASSERT(blitOk, "BlitConvertFramebuffer from BGRA32 to RGBA32 must succeed");
    bootvid::Color blittedPixel = inbv::PixelToColor(dstFb[10 * 64 + 10], inbv::PixelFormat::Rgba32);
    TEST_ASSERT(blittedPixel == testCol, "Blitted converted pixel must match source color");
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

// ============================================================================
// Suite 229: Horizon 13 - Sovereign Power Management (Po) Subsystem Refinement
// ============================================================================
namespace test_po_h13 {
    inline uint32_t g_DeviceA_IrpCount = 0;
    inline uint32_t g_DeviceB_IrpCount = 0;
    inline uint8_t  g_LastMinorA = 0;
    inline uint8_t  g_LastMinorB = 0;
    inline bool     g_VetoPowerTransition = false;
    inline uint32_t g_CallbackInvocations = 0;
    inline std::string g_LastCallbackGuid{};
    inline uint32_t g_LastCallbackValue = 0;

    inline NtStatus DeviceADispatch(io::DeviceObject* dev, io::Irp* irp) {
        if (!dev || !irp) return NtStatus::InvalidParameter;
        g_DeviceA_IrpCount++;
        g_LastMinorA = irp->minorFunction;
        if (irp->minorFunction == po::IRP_MN_QUERY_POWER && g_VetoPowerTransition) {
            return NtStatus::DeviceBusy;
        }
        return NtStatus::Success;
    }

    inline NtStatus DeviceBDispatch(io::DeviceObject* dev, io::Irp* irp) {
        if (!dev || !irp) return NtStatus::InvalidParameter;
        g_DeviceB_IrpCount++;
        g_LastMinorB = irp->minorFunction;
        return NtStatus::Success;
    }

    inline void PowerSettingTestCallback(void* context, const std::string& guid, const void* value, size_t size) {
        (void)context;
        g_CallbackInvocations++;
        g_LastCallbackGuid = guid;
        if (value && size >= sizeof(uint32_t)) {
            g_LastCallbackValue = *reinterpret_cast<const uint32_t*>(value);
        }
    }
}

inline void Test_Horizon13_SovereignPowerManagement_PoSubsystem() {
    std::cout << "[TEST] Executing Suite 229: Horizon 13 - Sovereign Power Management (Po) Subsystem Refinement...\n";
    auto& pwrMgr = po::PowerManager::get();
    pwrMgr.resetForTesting();

    // 1. Initial State & Capabilities Verification
    TEST_ASSERT(pwrMgr.getSystemPowerState() == po::SystemPowerState::PowerSystemWorking,
                "Initial system power state must be S0 (Working)");
    TEST_ASSERT(!pwrMgr.isShutdown(), "System must not be marked as shutdown initially");
    TEST_ASSERT(pwrMgr.getRegisteredDeviceCount() == 0, "Initial device count must be 0");

    auto caps = pwrMgr.getPowerCapabilities();
    TEST_ASSERT(caps.systemS1 && caps.systemS3 && caps.systemS4 && caps.systemS5,
                "ACPI S1, S3, S4, S5 states must be supported in system power capabilities");
    TEST_ASSERT(caps.thermalControl && caps.cpuThrottle, "Thermal control and CPU throttling must be enabled");

    // 2. Multi-Device Registration, Power States & Sequence Counters
    io::DriverObject driverA{};
    driverA.driverName = L"\\Driver\\PciBus";
    driverA.setDispatch(io::IRP_MJ_POWER, test_po_h13::DeviceADispatch);

    io::DeviceObject devA{};
    devA.driverObject = &driverA;
    devA.deviceName = L"\\Device\\NVMController0";

    io::DriverObject driverB{};
    driverB.driverName = L"\\Driver\\HdaudioBus";
    driverB.setDispatch(io::IRP_MJ_POWER, test_po_h13::DeviceBDispatch);

    io::DeviceObject devB{};
    devB.driverObject = &driverB;
    devB.deviceName = L"\\Device\\HDAudioCodec";

    pwrMgr.registerDevice(&devA);
    pwrMgr.registerDevice(&devB);
    TEST_ASSERT(pwrMgr.getRegisteredDeviceCount() == 2, "Registered device count must be 2");

    TEST_ASSERT(pwrMgr.getDevicePowerState(&devA) == po::DevicePowerState::PowerDeviceD0,
                "Initial device A power state must be D0");
    TEST_ASSERT(pwrMgr.getDevicePowerState(&devB) == po::DevicePowerState::PowerDeviceD0,
                "Initial device B power state must be D0");

    auto seqA = pwrMgr.getDevicePowerSequence(&devA);
    TEST_ASSERT(seqA.sequenceD0 == 1 && seqA.sequenceD1 == 0 && seqA.sequenceD2 == 0 && seqA.sequenceD3 == 0,
                "Initial sequence for D0 must be 1 and other states 0");

    // Transition device A through D1 -> D2 -> D3 -> D0
    pwrMgr.setDevicePowerState(&devA, po::DevicePowerState::PowerDeviceD1);
    pwrMgr.setDevicePowerState(&devA, po::DevicePowerState::PowerDeviceD2);
    pwrMgr.setDevicePowerState(&devA, po::DevicePowerState::PowerDeviceD3);
    pwrMgr.setDevicePowerState(&devA, po::DevicePowerState::PowerDeviceD0);

    seqA = pwrMgr.getDevicePowerSequence(&devA);
    TEST_ASSERT(seqA.sequenceD0 == 2, "D0 sequence counter must be 2 after full cycle");
    TEST_ASSERT(seqA.sequenceD1 == 1, "D1 sequence counter must be 1");
    TEST_ASSERT(seqA.sequenceD2 == 1, "D2 sequence counter must be 1");
    TEST_ASSERT(seqA.sequenceD3 == 1, "D3 sequence counter must be 1");

    // 3. Power Setting Callback Registration & Dispatch
    uintptr_t cbHandle = 0;
    test_po_h13::g_CallbackInvocations = 0;
    test_po_h13::g_LastCallbackGuid.clear();
    test_po_h13::g_LastCallbackValue = 0;

    NtStatus regCbStatus = po::PoRegisterPowerSettingCallback(
        po::GUID_SYSTEM_POWER_STATE,
        test_po_h13::PowerSettingTestCallback,
        nullptr,
        cbHandle
    );
    TEST_ASSERT(NT_SUCCESS(regCbStatus) && cbHandle != 0, "PoRegisterPowerSettingCallback must succeed");

    // 4. Two-Phase Sleep Transition with Device Veto Abort
    test_po_h13::g_VetoPowerTransition = true;
    test_po_h13::g_DeviceA_IrpCount = 0;
    test_po_h13::g_DeviceB_IrpCount = 0;

    NtStatus vetoedStatus = pwrMgr.setSystemPowerState(po::SystemPowerState::PowerSystemSleeping3);
    TEST_ASSERT(vetoedStatus == NtStatus::DeviceBusy, "Sleep transition must be rejected with STATUS_DEVICE_BUSY");
    TEST_ASSERT(pwrMgr.getSystemPowerState() == po::SystemPowerState::PowerSystemWorking,
                "System must remain in S0 after transition rejection");
    TEST_ASSERT(test_po_h13::g_DeviceA_IrpCount == 1, "Device A must have received query IRP");
    TEST_ASSERT(test_po_h13::g_LastMinorA == po::IRP_MN_QUERY_POWER, "Minor must be IRP_MN_QUERY_POWER");

    // 5. Successful Coordinated Sleep Transition (S0 -> S3 Standby)
    test_po_h13::g_VetoPowerTransition = false;
    test_po_h13::g_DeviceA_IrpCount = 0;
    test_po_h13::g_DeviceB_IrpCount = 0;

    NtStatus sleepStatus = pwrMgr.setSystemPowerState(po::SystemPowerState::PowerSystemSleeping3);
    TEST_ASSERT(NT_SUCCESS(sleepStatus), "Transition to S3 Standby must succeed when devices agree");
    TEST_ASSERT(pwrMgr.getSystemPowerState() == po::SystemPowerState::PowerSystemSleeping3,
                "System power state must be S3 (Standby)");
    TEST_ASSERT(pwrMgr.getDevicePowerState(&devA) == po::DevicePowerState::PowerDeviceD3,
                "Device A must be powered down to D3");
    TEST_ASSERT(pwrMgr.getDevicePowerState(&devB) == po::DevicePowerState::PowerDeviceD3,
                "Device B must be powered down to D3");
    TEST_ASSERT(test_po_h13::g_CallbackInvocations > 0, "Power setting callback must have been notified");
    TEST_ASSERT(test_po_h13::g_LastCallbackValue == static_cast<uint32_t>(po::SystemPowerState::PowerSystemSleeping3),
                "Notified power state value must be S3");

    // 6. Device Wait/Wake Arming & System Wake Trigger
    NtStatus armStatus = pwrMgr.armDeviceWake(&devA, po::SystemPowerState::PowerSystemSleeping3);
    TEST_ASSERT(NT_SUCCESS(armStatus), "armDeviceWake on Device A must succeed");
    TEST_ASSERT(pwrMgr.isDeviceWakeArmed(&devA), "Device A must be reported as wake armed");

    // Trigger external wake event from armed device A
    NtStatus wakeStatus = pwrMgr.triggerDeviceWake(&devA);
    TEST_ASSERT(NT_SUCCESS(wakeStatus), "triggerDeviceWake must succeed");
    TEST_ASSERT(pwrMgr.getSystemPowerState() == po::SystemPowerState::PowerSystemWorking,
                "System must wake back to S0 Working on armed device trigger");
    TEST_ASSERT(pwrMgr.getDevicePowerState(&devA) == po::DevicePowerState::PowerDeviceD0,
                "Device A must have transitioned back to D0 on resume");
    TEST_ASSERT(pwrMgr.getDevicePowerState(&devB) == po::DevicePowerState::PowerDeviceD0,
                "Device B must have transitioned back to D0 on resume");

    pwrMgr.disarmDeviceWake(&devA);
    TEST_ASSERT(!pwrMgr.isDeviceWakeArmed(&devA), "Device A must be disarmed");

    // 7. Modern Standby (Connected Standby / AoAc S0 Low Power Idle)
    TEST_ASSERT(pwrMgr.isConnectedStandbySupported(), "Connected Standby must be supported by default");
    TEST_ASSERT(!pwrMgr.isConnectedStandbyActive(), "Connected Standby must be inactive initially");

    uintptr_t monCbHandle = 0;
    pwrMgr.registerPowerSettingCallback(
        po::GUID_MONITOR_POWER_ON,
        [](void*, const std::string&, const void*, size_t) {
            // Monitor callback
        },
        nullptr,
        monCbHandle
    );

    pwrMgr.enterConnectedStandby();
    TEST_ASSERT(pwrMgr.isConnectedStandbyActive(), "Connected Standby must be active after enterConnectedStandby");
    pwrMgr.exitConnectedStandby();
    TEST_ASSERT(!pwrMgr.isConnectedStandbyActive(), "Connected Standby must be inactive after exitConnectedStandby");

    pwrMgr.unregisterPowerSettingCallback(monCbHandle);

    // 8. ACPI Thermal Zones & Multi-Trip Point Throttling
    po::ThermalZoneInfo tz{};
    bool tzFound = pwrMgr.getThermalZone(0, tz);
    TEST_ASSERT(tzFound, "Thermal Zone 0 must be discovered");
    TEST_ASSERT(tz.currentTempDeciKelvin == 3031, "Initial temperature must be 303.1 K (30.0 C)");

    // Raise temperature to 68 C (341.1 K) -> Above active fan trip point (65 C)
    pwrMgr.updateThermalZoneTemperature(0, 3411);
    TEST_ASSERT(pwrMgr.getThermalZone(0, tz), "getThermalZone at 68 C must succeed");
    TEST_ASSERT(tz.activeCoolingEngaged, "Active fan cooling must engage at 68 C");
    TEST_ASSERT(!tz.passiveThrottlingEngaged, "Passive throttling must not engage at 68 C");

    // Raise temperature to 85 C (358.1 K) -> Above passive throttle trip point (80 C)
    pwrMgr.updateThermalZoneTemperature(0, 3581);
    TEST_ASSERT(pwrMgr.getThermalZone(0, tz), "getThermalZone at 85 C must succeed");
    TEST_ASSERT(tz.passiveThrottlingEngaged, "Passive throttling must engage at 85 C");
    TEST_ASSERT(tz.throttlePercent == 50, "Throttle percent must be 50%");
    auto cpu = pwrMgr.getProcessorPowerInformation();
    TEST_ASSERT(cpu.currentMhz == cpu.maxMhz / 2, "CPU clock frequency must be clamped to 50%");

    // Cool down below trip points to 35 C (308.1 K)
    pwrMgr.updateThermalZoneTemperature(0, 3081);
    TEST_ASSERT(pwrMgr.getThermalZone(0, tz), "getThermalZone after cooling must succeed");
    TEST_ASSERT(!tz.activeCoolingEngaged, "Active cooling must disengage after cooling");
    TEST_ASSERT(!tz.passiveThrottlingEngaged, "Passive throttling must disengage after cooling");
    cpu = pwrMgr.getProcessorPowerInformation();
    TEST_ASSERT(cpu.currentMhz == cpu.maxMhz, "CPU clock frequency must be restored to 100%");

    // 9. Battery & AC Power Supply Telemetry
    pwrMgr.setAcPowerOnline(false); // Switch to DC (Battery)
    TEST_ASSERT(!pwrMgr.isAcOnline(), "System must report AC is offline (battery mode)");
    auto bState = pwrMgr.getBatteryState();
    TEST_ASSERT(bState.discharging && !bState.charging, "Battery must be discharging on DC");

    pwrMgr.setAcPowerOnline(true); // Switch to AC
    TEST_ASSERT(pwrMgr.isAcOnline(), "System must report AC is online");

    // Test battery percentage
    po::SystemBatteryState customBatt{};
    customBatt.acOnLine = false;
    customBatt.batteryPresent = true;
    customBatt.maxCapacity = 60000;
    customBatt.remainingCapacity = 45000; // 75%
    pwrMgr.updateBatteryState(customBatt);
    TEST_ASSERT(pwrMgr.getBatteryPercentage() == 75, "Battery percentage must compute to 75%");

    // 10. Power Schemes & Policy Switching
    TEST_ASSERT(pwrMgr.getActivePowerScheme() == po::GUID_POWERSCHEME_BALANCED,
                "Default power scheme must be Balanced");
    auto curPolicy = pwrMgr.getActivePowerPolicy();
    TEST_ASSERT(curPolicy.coolingPolicy == po::CoolingMode::Active, "Balanced policy cooling must be Active");

    NtStatus switchHighPerf = pwrMgr.setActivePowerScheme(po::GUID_POWERSCHEME_HIGH_PERF);
    TEST_ASSERT(NT_SUCCESS(switchHighPerf), "Switching to High Performance scheme must succeed");
    TEST_ASSERT(pwrMgr.getActivePowerScheme() == po::GUID_POWERSCHEME_HIGH_PERF,
                "Active scheme must be High Performance");
    curPolicy = pwrMgr.getActivePowerPolicy();
    TEST_ASSERT(curPolicy.cpuMinThrottlePercent == 100, "High Performance min CPU throttle must be 100%");

    NtStatus switchPowerSaver = pwrMgr.setActivePowerScheme(po::GUID_POWERSCHEME_POWER_SAVER);
    TEST_ASSERT(NT_SUCCESS(switchPowerSaver), "Switching to Power Saver scheme must succeed");
    curPolicy = pwrMgr.getActivePowerPolicy();
    TEST_ASSERT(curPolicy.coolingPolicy == po::CoolingMode::Passive, "Power Saver cooling policy must be Passive");

    // 11. DDK Helper Exports & Serialization Verification
    io::Irp dummyPwrIrp{};
    dummyPwrIrp.flags = 0;
    po::PoStartNextPowerIrp(&dummyPwrIrp);
    TEST_ASSERT((dummyPwrIrp.flags & 0x00000001) != 0, "PoStartNextPowerIrp must set IRP_POWER_PROCESSED");

    po::PowerState psD1{};
    psD1.deviceState = po::DevicePowerState::PowerDeviceD1;
    po::PoSetPowerState(&devB, po::PowerStateType::DevicePowerState, psD1);
    TEST_ASSERT(pwrMgr.getDevicePowerState(&devB) == po::DevicePowerState::PowerDeviceD1,
                "PoSetPowerState must update device B state to D1");

    // 12. Critical Thermal Trip Point Emergency Shutdown Simulation
    // 102 C (375.1 K) -> Exceeds 100 C tripPointCritical
    pwrMgr.updateThermalZoneTemperature(0, 3751);
    TEST_ASSERT(pwrMgr.isShutdown(), "Critical thermal trip point must trigger emergency shutdown");
    TEST_ASSERT(pwrMgr.getSystemPowerState() == po::SystemPowerState::PowerSystemShutdown,
                "System power state must be PowerSystemShutdown (S5)");
    TEST_ASSERT(pwrMgr.getLastShutdownAction() == po::ShutdownAction::ShutdownPowerOff,
                "Shutdown action must be ShutdownPowerOff");

    // Cleanup & Unregister
    po::PoUnregisterPowerSettingCallback(cbHandle);
    pwrMgr.unregisterDevice(&devA);
    pwrMgr.unregisterDevice(&devB);
    pwrMgr.resetForTesting();
    std::cout << "[TEST] Suite 229: Horizon 13 - Sovereign Power Management (Po) Subsystem PASSED.\n";
}

// ============================================================================
// Suite 230: Horizon 14 - Sovereign Executive Worker Queues & IoWorkItems
// ============================================================================
namespace test_ex_h14 {
    inline std::atomic<uint32_t> g_OrderCounter{0};
    inline std::vector<uint32_t> g_ExecutionOrder;
    inline std::mutex g_OrderMutex;

    struct PriorityContext {
        uint32_t priorityId;
    };

    inline void RecordPriorityCallback(void* ctx) {
        auto* pCtx = static_cast<PriorityContext*>(ctx);
        if (pCtx) {
            std::lock_guard<std::mutex> lock(g_OrderMutex);
            g_ExecutionOrder.push_back(pCtx->priorityId);
        }
    }

    struct IoTestContext {
        io::DeviceObject* expectedDevice{nullptr};
        bool routineCalled{false};
        bool routineExCalled{false};
    };

    inline void IoWorkRoutine(io::DeviceObject* dev, void* ctx) {
        auto* tCtx = static_cast<IoTestContext*>(ctx);
        if (tCtx && dev == tCtx->expectedDevice) {
            tCtx->routineCalled = true;
        }
    }

    inline void IoWorkRoutineEx(void* ioObject, void* ctx, ex::IoWorkItem* item) {
        auto* tCtx = static_cast<IoTestContext*>(ctx);
        if (tCtx && ioObject == reinterpret_cast<void*>(tCtx->expectedDevice) && item != nullptr) {
            tCtx->routineExCalled = true;
        }
    }
}

inline void Test_Horizon14_ExecutiveWorkQueues_And_IoWorkItems() {
    std::cout << "[TEST] Executing Suite 230: Horizon 14 - Sovereign Executive Worker Queues & IoWorkItems...\n";
    auto& workMgr = ex::ExecutiveWorkQueueManager::get();

    // 1. Initial State & Auto-scaling Setup
    workMgr.initialize(2, 6);
    TEST_ASSERT(workMgr.isRunning(), "Executive Work Queue Manager must be running");
    TEST_ASSERT(workMgr.getActiveWorkerCount() >= 2, "Active worker threads must be >= 2");

    // 2. Strict Priority Queue Ordering Verification
    {
        std::lock_guard<std::mutex> lock(test_ex_h14::g_OrderMutex);
        test_ex_h14::g_ExecutionOrder.clear();
    }

    test_ex_h14::PriorityContext ctxBg{1};   // Background
    test_ex_h14::PriorityContext ctxDel{2};  // Delayed
    test_ex_h14::PriorityContext ctxNorm{3}; // Normal
    test_ex_h14::PriorityContext ctxCrit{4}; // Critical
    test_ex_h14::PriorityContext ctxHyp{5};  // HyperCritical
    test_ex_h14::PriorityContext ctxRt{6};   // RealTime

    ex::WorkQueueItem itemBg{};
    ex::WorkQueueItem itemDel{};
    ex::WorkQueueItem itemNorm{};
    ex::WorkQueueItem itemCrit{};
    ex::WorkQueueItem itemHyp{};
    ex::WorkQueueItem itemRt{};

    ex::ExInitializeWorkItem(&itemBg, test_ex_h14::RecordPriorityCallback, &ctxBg);
    ex::ExInitializeWorkItem(&itemDel, test_ex_h14::RecordPriorityCallback, &ctxDel);
    ex::ExInitializeWorkItem(&itemNorm, test_ex_h14::RecordPriorityCallback, &ctxNorm);
    ex::ExInitializeWorkItem(&itemCrit, test_ex_h14::RecordPriorityCallback, &ctxCrit);
    ex::ExInitializeWorkItem(&itemHyp, test_ex_h14::RecordPriorityCallback, &ctxHyp);
    ex::ExInitializeWorkItem(&itemRt, test_ex_h14::RecordPriorityCallback, &ctxRt);

    // Queue items across all priority queues
    ex::ExQueueWorkItem(&itemBg, ex::WorkQueueType::BackgroundWorkQueue);
    ex::ExQueueWorkItem(&itemDel, ex::WorkQueueType::DelayedWorkQueue);
    ex::ExQueueWorkItem(&itemNorm, ex::WorkQueueType::NormalWorkQueue);
    ex::ExQueueWorkItem(&itemCrit, ex::WorkQueueType::CriticalWorkQueue);
    ex::ExQueueWorkItem(&itemHyp, ex::WorkQueueType::HyperCriticalWorkQueue);
    ex::ExQueueWorkItem(&itemRt, ex::WorkQueueType::RealTimeWorkQueue);

    // 3. Flush Work Queues
    bool flushOk = workMgr.flushAll(5000);
    TEST_ASSERT(flushOk, "flushAll must successfully drain all queues within timeout");
    TEST_ASSERT(itemRt.completed && itemHyp.completed && itemCrit.completed,
                "High priority work items must be marked as completed");
    TEST_ASSERT(itemNorm.completed && itemDel.completed && itemBg.completed,
                "Lower priority work items must be marked as completed");

    // 4. Synchronous Wait for Individual Item (ExWaitForWorkItem)
    std::atomic<bool> waitItemRan{false};
    ex::WorkQueueItem waitItem{};
    ex::ExInitializeWorkItem(&waitItem, [](void* p) {
        auto* ran = static_cast<std::atomic<bool>*>(p);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        ran->store(true);
    }, &waitItemRan);

    ex::ExQueueWorkItem(&waitItem, ex::WorkQueueType::CriticalWorkQueue);
    bool waitSuccess = ex::ExWaitForWorkItem(&waitItem, 5000);
    TEST_ASSERT(waitSuccess, "ExWaitForWorkItem must return true before timeout");
    TEST_ASSERT(waitItemRan.load(), "Work item payload must have completed");
    TEST_ASSERT(waitItem.completed.load(), "Work item completed flag must be true");

    // 5. Device-bound IoWorkItem Lifecycle
    io::DeviceObject dummyDev{};
    dummyDev.deviceName = L"\\Device\\TestDisk0";

    ex::IoWorkItem* pIoItem = ex::IoAllocateWorkItem(&dummyDev);
    TEST_ASSERT(pIoItem != nullptr, "IoAllocateWorkItem must allocate non-null IoWorkItem");
    TEST_ASSERT(pIoItem->deviceObject == &dummyDev, "IoWorkItem must bind to the device object");

    test_ex_h14::IoTestContext ioCtx{};
    ioCtx.expectedDevice = &dummyDev;

    // Standard IoQueueWorkItem
    ex::IoQueueWorkItem(pIoItem, test_ex_h14::IoWorkRoutine, ex::WorkQueueType::CriticalWorkQueue, &ioCtx);
    bool ioFlush = ex::ExFlushWorkQueue(ex::WorkQueueType::CriticalWorkQueue, 3000);
    TEST_ASSERT(ioFlush, "Flushing critical work queue after IoQueueWorkItem must succeed");
    TEST_ASSERT(ioCtx.routineCalled, "PIO_WORKITEM_ROUTINE must be called with bound device object");

    // Extended IoQueueWorkItemEx
    ex::IoQueueWorkItemEx(pIoItem, test_ex_h14::IoWorkRoutineEx, ex::WorkQueueType::NormalWorkQueue, &ioCtx);
    bool ioFlushEx = ex::ExFlushWorkQueue(ex::WorkQueueType::NormalWorkQueue, 3000);
    TEST_ASSERT(ioFlushEx, "Flushing normal work queue after IoQueueWorkItemEx must succeed");
    TEST_ASSERT(ioCtx.routineExCalled, "PIO_WORKITEM_ROUTINE_EX must be called with bound device and item pointer");

    ex::IoFreeWorkItem(pIoItem);

    // 6. Exception Containment & IRQL Safety
    ex::WorkQueueItem errItem{};
    ex::ExInitializeWorkItem(&errItem, [](void*) {
        throw std::runtime_error("Simulated unhandled kernel driver worker exception");
    }, nullptr);

    ex::ExQueueWorkItem(&errItem, ex::WorkQueueType::DelayedWorkQueue);
    ex::ExFlushWorkQueue(ex::WorkQueueType::DelayedWorkQueue, 3000);
    TEST_ASSERT(errItem.completed.load(), "Exception-throwing work item must still be marked completed");

    // Verify subsequent item executes normally on the same worker thread pool
    std::atomic<bool> subsequentItemRan{false};
    ex::WorkQueueItem subItem{};
    ex::ExInitializeWorkItem(&subItem, [](void* p) {
        auto* ran = static_cast<std::atomic<bool>*>(p);
        ran->store(true);
    }, &subsequentItemRan);

    ex::ExQueueWorkItem(&subItem, ex::WorkQueueType::DelayedWorkQueue);
    ex::ExFlushWorkQueue(ex::WorkQueueType::DelayedWorkQueue, 3000);
    TEST_ASSERT(subsequentItemRan.load(), "Subsequent work item must execute normally after exception");

    // 7. Telemetry & Statistics Verification
    auto statsCrit = workMgr.getQueueStatistics(ex::WorkQueueType::CriticalWorkQueue);
    TEST_ASSERT(statsCrit.itemsQueued > 0, "Critical queue itemsQueued must be > 0");
    TEST_ASSERT(statsCrit.itemsProcessed > 0, "Critical queue itemsProcessed must be > 0");
    TEST_ASSERT(workMgr.getTotalProcessed() >= 8, "Total processed items must be at least 8");
    TEST_ASSERT(workMgr.getTotalQueued() >= 8, "Total queued items must be at least 8");

    std::cout << "[TEST] Suite 230: Horizon 14 - Sovereign Executive Worker Queues & IoWorkItems PASSED.\n";
}

// ============================================================================
// Suite 231: Horizon 15 - Sovereign Kernel Timers, DPC Wheel & ExTimer Subsystem
// ============================================================================
namespace test_timer_h15 {
    struct TimerDpcContext {
        std::atomic<uint32_t> callCount{0};
        void* lastSysArg1{nullptr};
        void* lastSysArg2{nullptr};
    };

    inline void TimerDpcRoutine(ke::KDPC*, void* deferredContext, void* sysArg1, void* sysArg2) {
        auto* ctx = static_cast<TimerDpcContext*>(deferredContext);
        if (ctx) {
            ctx->callCount.fetch_add(1, std::memory_order_relaxed);
            ctx->lastSysArg1 = sysArg1;
            ctx->lastSysArg2 = sysArg2;
        }
    }

    struct ExTimerContext {
        std::atomic<uint32_t> callbackCount{0};
        void* recordedContext{nullptr};
    };

    inline void ExTimerRoutine(timer::EX_TIMER* timer, void* context) {
        (void)timer;
        auto* ctx = static_cast<ExTimerContext*>(context);
        if (ctx) {
            ctx->callbackCount.fetch_add(1, std::memory_order_relaxed);
            ctx->recordedContext = context;
        }
    }
}

inline void Test_Horizon15_KernelTimers_DpcWheel_And_ExTimer() {
    std::cout << "[TEST] Executing Suite 231: Horizon 15 - Sovereign Kernel Timers, DPC Wheel & ExTimer...\n";
    auto& tm = timer::TimerManager::get();
    tm.resetForTesting();

    // 1. Notification vs Synchronization Timer Initial States
    timer::KTIMER notifTimer;
    timer::KeInitializeTimer(&notifTimer);
    TEST_ASSERT(!timer::KeReadStateTimer(&notifTimer), "Initial state must be unsignaled");
    TEST_ASSERT(notifTimer.obj.getType() == timer::TimerType::NotificationTimer,
                "Timer type must be NotificationTimer");

    timer::KTIMER syncTimer;
    timer::KeInitializeTimerEx(&syncTimer, timer::TimerType::SynchronizationTimer);
    TEST_ASSERT(!timer::KeReadStateTimer(&syncTimer), "Initial state must be unsignaled");
    TEST_ASSERT(syncTimer.obj.getType() == timer::TimerType::SynchronizationTimer,
                "Timer type must be SynchronizationTimer");

    // 2. Timer Wheel Relative Expiration & Bucket Insertion
    LargeInteger dueRel{};
    dueRel.quadPart = -500000LL; // 50ms in 100ns units
    bool wasInserted = timer::KeSetTimer(&notifTimer, dueRel);
    TEST_ASSERT(!wasInserted, "Timer was not previously inserted");
    TEST_ASSERT(notifTimer.obj.isInserted(), "Timer must be marked inserted");
    TEST_ASSERT(tm.getActiveTimerCount() == 1, "Active timer count must be 1");

    // 3. Timer Cancellation
    bool cancelOk = timer::KeCancelTimer(&notifTimer);
    TEST_ASSERT(cancelOk, "KeCancelTimer must return true for active timer");
    TEST_ASSERT(!notifTimer.obj.isInserted(), "Timer must be marked uninserted after cancel");
    TEST_ASSERT(tm.getActiveTimerCount() == 0, "Active timer count must be 0 after cancel");
    TEST_ASSERT(!timer::KeCancelTimer(&notifTimer), "Canceling inactive timer must return false");

    // 4. Periodic Timer & Timer DPC Queueing
    test_timer_h15::TimerDpcContext dpcCtx{};
    ke::KDPC timerDpc{};
    ke::KeInitializeDpc(&timerDpc, test_timer_h15::TimerDpcRoutine, &dpcCtx);

    dueRel.quadPart = -10000LL; // 1ms
    timer::KeSetTimerEx(&notifTimer, dueRel, 100, &timerDpc); // 100ms periodic
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    size_t expiredCount = tm.processTimers();
    TEST_ASSERT(expiredCount >= 1, "processTimers must expire due timer");
    TEST_ASSERT(timer::KeReadStateTimer(&notifTimer), "Expired notification timer must be signaled");
    TEST_ASSERT(tm.getTotalDpcsQueued() >= 1, "DPC queue count must be >= 1");

    // Drain DPCs at DISPATCH_LEVEL
    ke::KiRetireDpcList();
    TEST_ASSERT(dpcCtx.callCount.load() == 1, "Timer DPC routine must have executed exactly once");
    TEST_ASSERT(dpcCtx.lastSysArg1 == &notifTimer, "systemArgument1 must be pointer to KTIMER");
    TEST_ASSERT(reinterpret_cast<uintptr_t>(dpcCtx.lastSysArg2) == 100, "systemArgument2 must be periodMs (100)");
    TEST_ASSERT(tm.getActiveTimerCount() == 1, "Periodic timer must remain active in wheel after expiration");

    timer::KeCancelTimer(&notifTimer);

    // 5. Auto-Reset Synchronization Timer Semantics
    syncTimer.obj.setSignaled();
    TEST_ASSERT(syncTimer.obj.isSignaled(), "Sync timer is signaled");
    bool satisfied = syncTimer.obj.satisfyWait();
    TEST_ASSERT(satisfied, "satisfyWait must return true for signaled synchronization timer");
    TEST_ASSERT(!syncTimer.obj.isSignaled(), "Sync timer must auto-reset to unsignaled after satisfyWait");

    // 6. High-Resolution Timer Precision
    LargeInteger dueHi{};
    dueHi.quadPart = -1000LL; // 100 microseconds (0.1ms)
    timer::KTIMER hiTimer;
    timer::KeInitializeTimer(&hiTimer);
    tm.setTimer(&hiTimer, dueHi, 0, nullptr, {}, true);
    TEST_ASSERT(hiTimer.obj.isHighResolution(), "High-resolution flag must be set");

    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    size_t hiExpired = tm.processTimers();
    TEST_ASSERT(hiExpired >= 1, "High-resolution timer must expire");
    TEST_ASSERT(timer::KeReadStateTimer(&hiTimer), "High-resolution timer must be signaled");

    // 7. Modern Executive Timer (EX_TIMER) with Coalescing
    test_timer_h15::ExTimerContext exCtx{};
    timer::EX_TIMER* pEx = timer::ExAllocateTimer(
        test_timer_h15::ExTimerRoutine,
        &exCtx,
        timer::EX_TIMER_HIGH_RESOLUTION | timer::EX_TIMER_NOTIFICATION
    );
    TEST_ASSERT(pEx != nullptr, "ExAllocateTimer must allocate EX_TIMER");

    LargeInteger exDue{};
    exDue.quadPart = -1000LL; // 0.1ms
    LargeInteger tolerableDelay{};
    tolerableDelay.quadPart = 500LL; // Coalescing delay hint

    bool wasAlreadySet = timer::ExSetTimer(pEx, exDue, 0, tolerableDelay);
    TEST_ASSERT(!wasAlreadySet, "ExSetTimer must return false when timer was not previously set");
    TEST_ASSERT(pEx->active.load(), "ExTimer must be active");

    bool rearmed = timer::ExSetTimer(pEx, exDue, 0, tolerableDelay);
    TEST_ASSERT(rearmed, "ExSetTimer must return true when timer was previously set");

    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    tm.processTimers();
    ke::KiRetireDpcList();

    TEST_ASSERT(exCtx.callbackCount.load() >= 1, "ExTimer callback must have executed");
    TEST_ASSERT(exCtx.recordedContext == &exCtx, "ExTimer context pointer must match");

    timer::ExCancelTimer(pEx);
    timer::ExDeleteTimer(pEx);

    // 8. DPC Watchdog Diagnostics
    timer::KDPC_WATCHDOG_INFORMATION wdInfo{};
    timer::KeQueryDpcWatchdogInformation(&wdInfo);
    TEST_ASSERT(wdInfo.dpcTimeLimit == 500, "Default DPC time limit must be 500ms");
    TEST_ASSERT(wdInfo.dpcWatchdogLimit == 1000, "Default cumulative limit must be 1000ms");

    tm.updateDpcWatchdog(250);
    timer::KeQueryDpcWatchdogInformation(&wdInfo);
    TEST_ASSERT(wdInfo.dpcTimeCount == 250, "Updated DPC time count must be 250ms");
    TEST_ASSERT(wdInfo.dpcWatchdogCount == 250, "Updated watchdog count must be 250ms");
    tm.resetDpcWatchdog();

    tm.resetForTesting();
    std::cout << "[TEST] Suite 231: Horizon 15 - Sovereign Kernel Timers, DPC Wheel & ExTimer PASSED.\n";
}

// ============================================================================
// Suite 232: Horizon 16 - Inter-Processor Interrupt (IPI), SMP Dispatching & Barriers
// ============================================================================
namespace test_ipi_h16 {
    inline std::atomic<uint32_t> g_WorkerInvocationCount{0};

    inline uintptr_t BroadcastWorker(uintptr_t context) {
        g_WorkerInvocationCount.fetch_add(1, std::memory_order_relaxed);
        return context * 2 + 1;
    }
}

inline void Test_Horizon16_InterProcessorInterrupts_SMP_And_Barriers() {
    std::cout << "[TEST] Executing Suite 232: Horizon 16 - Sovereign IPI, SMP Dispatching & Barriers...\n";
    auto& ipiMgr = ke::IpiManager::get();
    ipiMgr.resetForTesting();

    // 1. Processor Affinity Mask & Core Enumeration
    TEST_ASSERT(ke::KeQueryActiveProcessorCount() == 4, "Default active processor count must be 4");
    ke::KAFFINITY activeMask = 0;
    uint32_t count = ke::KeQueryActiveProcessorCount(&activeMask);
    TEST_ASSERT(count == 4, "Active processor count must be 4");
    TEST_ASSERT(activeMask == 0x0F, "Active affinity mask must be 0x0F for 4 processors");
    TEST_ASSERT(ke::KeQueryActiveProcessors() == 0x0F, "KeQueryActiveProcessors must return 0x0F");

    // Reconfigure to 8-core SMP configuration
    ipiMgr.initialize(8);
    TEST_ASSERT(ke::KeQueryActiveProcessorCount(&activeMask) == 8, "Reconfigured active processor count must be 8");
    TEST_ASSERT(activeMask == 0xFF, "Active affinity mask must be 0xFF for 8 processors");
    TEST_ASSERT(ipiMgr.isProcessorOnline(7), "Core 7 must be marked online");
    TEST_ASSERT(!ipiMgr.isProcessorOnline(8), "Core 8 must be offline");

    // 2. Targeted IPI Transmission & Processing
    // Send IPI_DPC from CPU 0 to CPU 2 (affinity bit 2 = 0x04)
    bool ipiSent = ipiMgr.sendIpi(0, 0x04, ke::IPI_DPC);
    TEST_ASSERT(ipiSent, "sendIpi to valid online processor must return true");

    const auto& cpu0State = ipiMgr.getProcessorState(0);
    const auto& cpu2State = ipiMgr.getProcessorState(2);
    TEST_ASSERT(cpu0State.ipiSentCount >= 1, "CPU 0 ipiSentCount must be >= 1");
    TEST_ASSERT(cpu2State.ipiReceivedCount >= 1, "CPU 2 ipiReceivedCount must be >= 1");
    TEST_ASSERT((cpu2State.pendingRequests.load() & ke::IPI_DPC) != 0, "CPU 2 must have pending IPI_DPC");

    // Process incoming IPI on CPU 2
    uint32_t handled = ipiMgr.processIncomingIpis(2);
    TEST_ASSERT((handled & ke::IPI_DPC) != 0, "processIncomingIpis must return handled IPI_DPC request");
    TEST_ASSERT(cpu2State.pendingRequests.load() == 0, "Pending requests on CPU 2 must be drained");

    // 3. Synchronized Multiprocessor Generic Call Broadcast
    test_ipi_h16::g_WorkerInvocationCount.store(0);
    uintptr_t genericResult = ke::KeIpiGenericCall(test_ipi_h16::BroadcastWorker, 42);
    TEST_ASSERT(genericResult == 85, "KeIpiGenericCall result must match worker routine (42 * 2 + 1 = 85)");
    TEST_ASSERT(test_ipi_h16::g_WorkerInvocationCount.load() >= 8,
                "Broadcast worker must have been executed across all 8 active SMP cores");

    auto stats = ipiMgr.getStatistics();
    TEST_ASSERT(stats.totalGenericCalls >= 1, "Total generic calls count must be >= 1");

    // 4. SMP Execution Freezing & Thawing
    TEST_ASSERT(!ipiMgr.isExecutionFrozen(), "Initial execution freeze state must be false");
    bool freezeOk = ke::KeFreezeExecution();
    TEST_ASSERT(freezeOk, "KeFreezeExecution must succeed on primary core");
    TEST_ASSERT(ipiMgr.isExecutionFrozen(), "isExecutionFrozen must report true");

    // Remote cores must be marked frozen
    TEST_ASSERT(ipiMgr.isProcessorFrozen(1), "Remote processor 1 must be frozen");
    TEST_ASSERT(ipiMgr.isProcessorFrozen(7), "Remote processor 7 must be frozen");

    // Concurrent freeze attempt should fail
    bool concurrentFreeze = ipiMgr.freezeExecution(1);
    TEST_ASSERT(!concurrentFreeze, "Concurrent freeze execution must return false");

    // Thaw cores
    ke::KeThawExecution(true);
    TEST_ASSERT(!ipiMgr.isExecutionFrozen(), "isExecutionFrozen must report false after KeThawExecution");
    TEST_ASSERT(!ipiMgr.isProcessorFrozen(1), "Remote processor 1 must be thawed");
    TEST_ASSERT(!ipiMgr.isProcessorFrozen(7), "Remote processor 7 must be thawed");

    stats = ipiMgr.getStatistics();
    TEST_ASSERT(stats.totalFreezeCycles >= 1, "totalFreezeCycles must be >= 1");

    // 5. SMP TLB Shootdown Engine
    uint64_t tlbBefore = stats.totalTlbFlushes;
    ke::KeFlushEntireTb(true);
    stats = ipiMgr.getStatistics();
    TEST_ASSERT(stats.totalTlbFlushes > tlbBefore, "KeFlushEntireTb must increment totalTlbFlushes");

    void* testVa = reinterpret_cast<void*>(0x7FFE0000);
    ke::KeFlushSingleTb(testVa, true);
    stats = ipiMgr.getStatistics();
    TEST_ASSERT(stats.totalTlbFlushes > tlbBefore + 1, "KeFlushSingleTb must increment totalTlbFlushes");

    ke::KeFlushMultipleTb(16, nullptr, true);
    stats = ipiMgr.getStatistics();
    TEST_ASSERT(stats.totalTlbFlushes > tlbBefore + 2, "KeFlushMultipleTb must increment totalTlbFlushes");

    ipiMgr.resetForTesting();
    std::cout << "[TEST] Suite 232: Horizon 16 - Sovereign IPI, SMP Dispatching & Barriers PASSED.\n";
}

void Test_Horizon17_MemoryManager_MDL_ContiguousMemory_And_WorkingSet() {
    // 1. PFN Database Initialization & Diagnostics Telemetry
    mm::PfnDatabase::get().initialize(262144);
    auto diag = mm::MmQuerySystemMemoryDiagnostics();
    TEST_ASSERT(diag.totalPhysicalPages == 262144, "Total physical pages must be 262,144 (1 GB addressable)");
    TEST_ASSERT(diag.zeroedPages > 0, "Zeroed pages must be > 0");
    TEST_ASSERT(diag.freePages > 0, "Free pages must be > 0");
    TEST_ASSERT(diag.activePages == 0, "Initial active pages must be 0");

    // 2. High-Performance Demand Paging & Copy-On-Write (COW)
    mm::ProcessAddressSpace space;
    uintptr_t baseAddr = 0;
    NtStatus st = space.allocate(baseAddr, 16384, mm::MEM_COMMIT | mm::MEM_RESERVE, mm::PAGE_READWRITE);
    TEST_ASSERT(NT_SUCCESS(st), "Allocation of 16KB committed memory must succeed");
    TEST_ASSERT(space.getResidentPages() == 0, "Initial resident pages before faults must be 0");

    // Demand-zero read fault on page 0
    st = mm::MmAccessFault(space, baseAddr, false, true, false);
    TEST_ASSERT(NT_SUCCESS(st), "Demand-zero read fault must succeed");
    TEST_ASSERT(space.getResidentPages() == 1, "Resident pages must increment to 1 after demand-zero fault");

    // Demand-zero write fault on page 1
    st = mm::MmAccessFault(space, baseAddr + 4096, true, true, false);
    TEST_ASSERT(NT_SUCCESS(st), "Demand-zero write fault must succeed");
    TEST_ASSERT(space.getResidentPages() == 2, "Resident pages must increment to 2 after write fault");

    // Copy-On-Write (PAGE_WRITECOPY) handling
    uintptr_t cowAddr = 0;
    st = space.allocate(cowAddr, 4096, mm::MEM_COMMIT | mm::MEM_RESERVE, mm::PAGE_WRITECOPY);
    TEST_ASSERT(NT_SUCCESS(st), "COW allocation must succeed");
    auto* cowVad = space.findVad(cowAddr);
    TEST_ASSERT(cowVad != nullptr && cowVad->isCopyOnWrite, "VAD must be marked Copy-On-Write");

    // Trigger COW resolution on write
    st = mm::MmAccessFault(space, cowAddr, true, true, false);
    TEST_ASSERT(NT_SUCCESS(st), "Write to COW page must resolve fault");
    TEST_ASSERT(!cowVad->isCopyOnWrite, "COW flag must be cleared after private copy");
    TEST_ASSERT(cowVad->protection == mm::PAGE_READWRITE, "Protection must transition to PAGE_READWRITE");

    // 3. Memory Descriptor List (MDL) Architecture & Lifecycle
    void* testBuffer = reinterpret_cast<void*>(0x00007FF710001800ULL);
    uint32_t bufLen = 8192; // Spans 3 pages: 0x1800..0x1FFF, 0x2000..0x2FFF, 0x3000..0x37FF
    mm::PMDL mdl = mm::IoAllocateMdl(testBuffer, bufLen, false, false, nullptr);
    TEST_ASSERT(mdl != nullptr, "IoAllocateMdl must return non-null MDL pointer");
    TEST_ASSERT(mm::MmGetMdlByteCount(mdl) == bufLen, "MmGetMdlByteCount must match 8192");
    TEST_ASSERT(mm::MmGetMdlByteOffset(mdl) == 0x800, "MmGetMdlByteOffset must match 0x800");
    TEST_ASSERT(mm::MmGetMdlVirtualAddress(mdl) == testBuffer, "MmGetMdlVirtualAddress must match original buffer");
    TEST_ASSERT(mdl->pages.size() == 3, "MDL page vector must contain exactly 3 pages");

    // Probe and lock physical pages
    st = mm::MmProbeAndLockPages(mdl, mm::KernelMode, mm::LOCK_OPERATION::IoWriteAccess);
    TEST_ASSERT(NT_SUCCESS(st), "MmProbeAndLockPages must succeed");
    TEST_ASSERT((mdl->mdlFlags & mm::MDL_PAGES_LOCKED) != 0, "MDL_PAGES_LOCKED flag must be set");
    TEST_ASSERT((mdl->mdlFlags & mm::MDL_WRITE_OPERATION) != 0, "MDL_WRITE_OPERATION flag must be set");

    uint64_t* pfnArray = mm::MmGetMdlPfnArray(mdl);
    TEST_ASSERT(pfnArray != nullptr, "MmGetMdlPfnArray must return valid pointer");
    TEST_ASSERT(pfnArray[0] != 0 && pfnArray[1] != 0 && pfnArray[2] != 0, "Locked PFNs must be valid non-zero");

    // Map locked pages into kernel system VA space
    void* sysVa = mm::MmGetSystemAddressForMdlSafe(mdl, 16);
    TEST_ASSERT(sysVa != nullptr, "MmGetSystemAddressForMdlSafe must return system VA");
    TEST_ASSERT((mdl->mdlFlags & mm::MDL_MAPPED_TO_SYSTEM_VA) != 0, "MDL_MAPPED_TO_SYSTEM_VA flag must be set");
    TEST_ASSERT(sysVa == mdl->mappedSystemVa, "Mapped VA must match mdl->mappedSystemVa");

    // Unmap and unlock pages
    mm::MmUnmapLockedPages(sysVa, mdl);
    TEST_ASSERT((mdl->mdlFlags & mm::MDL_MAPPED_TO_SYSTEM_VA) == 0, "MDL_MAPPED_TO_SYSTEM_VA must be cleared");
    mm::MmUnlockPages(mdl);
    TEST_ASSERT((mdl->mdlFlags & mm::MDL_PAGES_LOCKED) == 0, "MDL_PAGES_LOCKED must be cleared");
    mm::IoFreeMdl(mdl);

    // 4. Non-Paged Pool Fast-Path MDL
    void* poolBuf = reinterpret_cast<void*>(0xFFFF888000004000ULL);
    mm::PMDL poolMdl = mm::IoAllocateMdl(poolBuf, 4096);
    TEST_ASSERT(poolMdl != nullptr, "Nonpaged pool MDL allocation must succeed");
    mm::MmBuildMdlForNonPagedPool(poolMdl);
    TEST_ASSERT((poolMdl->mdlFlags & mm::MDL_SOURCE_IS_NONPAGED_POOL) != 0, "MDL_SOURCE_IS_NONPAGED_POOL must be set");
    TEST_ASSERT((poolMdl->mdlFlags & mm::MDL_PAGES_LOCKED) != 0, "Nonpaged pool MDL pages must be marked locked");
    TEST_ASSERT(poolMdl->mappedSystemVa == poolBuf, "Nonpaged pool mapped VA must match source VA");
    void* poolSysVa = mm::MmGetSystemAddressForMdlSafe(poolMdl, 16);
    TEST_ASSERT(poolSysVa == poolBuf, "MmGetSystemAddressForMdlSafe must return original address for nonpaged pool");
    mm::MmUnlockPages(poolMdl);
    mm::IoFreeMdl(poolMdl);

    // 5. Contiguous Physical Memory Allocator
    size_t contigBytes = 64 * 1024; // 64KB = 16 contiguous 4KB pages
    void* contigPtr = mm::MmAllocateContiguousMemory(contigBytes, 0xFFFFFFFFFFFFFFFFULL);
    TEST_ASSERT(contigPtr != nullptr, "MmAllocateContiguousMemory(64KB) must succeed");
    uintptr_t contigVa = reinterpret_cast<uintptr_t>(contigPtr);
    TEST_ASSERT((contigVa & mm::PageMask4KB) == 0, "Contiguous memory must be page-aligned");
    uint64_t contigPfn = contigVa >> mm::PageShift4KB;
    TEST_ASSERT(contigPfn >= 256, "Contiguous memory must reside outside low 1MB reserved memory");
    mm::MmFreeContiguousMemorySpecifyCache(contigPtr, contigBytes, mm::MEMORY_CACHING_TYPE::MmCached);

    // 6. Working Set Trimming & Page Aging
    mm::ProcessAddressSpace trimSpace;
    uintptr_t trimBase = 0;
    st = trimSpace.allocate(trimBase, 32 * 1024, mm::MEM_COMMIT | mm::MEM_RESERVE, mm::PAGE_READWRITE); // 8 pages
    TEST_ASSERT(NT_SUCCESS(st), "Allocating 32KB for trimming test must succeed");
    for (size_t i = 0; i < 8; ++i) {
        st = mm::MmAccessFault(trimSpace, trimBase + i * 4096, false, true, false);
        TEST_ASSERT(NT_SUCCESS(st), "Faulting in page must succeed");
    }
    TEST_ASSERT(trimSpace.getResidentPages() == 8, "All 8 pages must be resident in working set");
    size_t trimmedPages = mm::MmTrimProcessWorkingSet(trimSpace, 5);
    TEST_ASSERT(trimmedPages == 5, "MmTrimProcessWorkingSet must have trimmed exactly 5 pages");
    TEST_ASSERT(trimSpace.getResidentPages() == 3, "Resident pages must be reduced from 8 to 3");

    // 7. Modified Page Writer & Flush
    auto& pfnDb = mm::PfnDatabase::get();
    uint64_t modPfn = pfnDb.allocatePage(true);
    TEST_ASSERT(modPfn != 0, "PFN allocation must succeed");
    pfnDb.setModified(modPfn, true);
    pfnDb.freePage(modPfn);
    TEST_ASSERT(pfnDb.getModifiedCount() >= 1, "Modified page count must be >= 1 after freeing modified page");

    size_t flushed = mm::MmFlushModifiedPageList();
    TEST_ASSERT(flushed >= 1, "MmFlushModifiedPageList must flush modified pages to standby list");
    TEST_ASSERT(pfnDb.getModifiedCount() == 0, "Modified list must be empty after flush");

    // 8. Telemetry & Diagnostic Reset
    diag = mm::MmQuerySystemMemoryDiagnostics();
    TEST_ASSERT(diag.totalPhysicalPages == 262144, "Total physical pages must remain 262,144");
    mm::MmResetDiagnosticsForTesting();
    auto diagAfter = mm::MmQuerySystemMemoryDiagnostics();
    TEST_ASSERT(diagAfter.activePages == 0, "Active pages must be 0 after resetForTesting");

    std::cout << "[TEST] Suite 233: Horizon 17 - Sovereign Memory Manager, MDL Lifecycle, Contiguous Memory & Working Set Trimming PASSED.\n";
}

void Test_Horizon18_SectionObjects_MemoryMappedFiles_And_ControlAreaArchitecture() {
    auto& sm = section::SectionManager::get();
    sm.resetForTesting();
    auto& pm = ps::ProcessManager::get();

    // 1. Pagefile-Backed Section Creation & Segment/ControlArea Verification
    ob::HandleTable handleTable;
    Handle secHandle = 0;
    std::shared_ptr<section::SectionObject> secObj;
    NtStatus status = sm.createSection(
        secHandle,
        handleTable,
        64 * 1024, // 64 KB = 16 pages
        section::SEC_COMMIT,
        mm::PAGE_READWRITE,
        secObj,
        L"\\BaseNamedObjects\\SharedMemoryBlock"
    );
    TEST_ASSERT(NT_SUCCESS(status), "Section creation must succeed");
    TEST_ASSERT(secHandle != 0, "Section handle must be non-zero");
    TEST_ASSERT(secObj != nullptr, "Section object pointer must be valid");
    TEST_ASSERT(secObj->getSize() == 64 * 1024, "Section size must match requested 64KB");
    TEST_ASSERT(secObj->getAllocationAttributes() == section::SEC_COMMIT, "Allocation attributes must match SEC_COMMIT");

    auto controlArea = secObj->getControlArea();
    TEST_ASSERT(controlArea != nullptr, "Section CONTROL_AREA must not be null");
    TEST_ASSERT(controlArea->isCommit, "CONTROL_AREA must be marked commit");
    TEST_ASSERT(!controlArea->isFile, "CONTROL_AREA must not be file-backed");
    TEST_ASSERT(controlArea->sharedPfnMap.size() == 16, "Shared PFN map must contain exactly 16 pages");

    auto segment = secObj->getSegment();
    TEST_ASSERT(segment != nullptr, "Section SEGMENT must not be null");
    TEST_ASSERT(segment->totalNumberOfPtes == 16, "SEGMENT total prototype PTEs must match 16");
    TEST_ASSERT(segment->prototypePtes.size() == 16, "Prototype PTE vector size must be 16");
    TEST_ASSERT(segment->prototypePtes[0].valid, "Prototype PTE 0 must be marked valid");

    // 2. Multi-Process Shared Memory Coherency
    auto procA = pm.createProcess(L"WorkerProcessA.exe");
    auto procB = pm.createProcess(L"WorkerProcessB.exe");
    TEST_ASSERT(procA != nullptr && procB != nullptr, "Test processes must be created successfully");

    uintptr_t baseA = 0;
    size_t sizeA = 64 * 1024;
    status = sm.mapViewOfSection(secObj, *procA, baseA, sizeA, mm::MEM_COMMIT, mm::PAGE_READWRITE);
    TEST_ASSERT(NT_SUCCESS(status), "Mapping section view into Process A must succeed");
    TEST_ASSERT(baseA != 0, "Process A mapped view base must be non-zero");

    uintptr_t baseB = 0;
    size_t sizeB = 64 * 1024;
    status = sm.mapViewOfSection(secObj, *procB, baseB, sizeB, mm::MEM_COMMIT, mm::PAGE_READWRITE);
    TEST_ASSERT(NT_SUCCESS(status), "Mapping section view into Process B must succeed");
    TEST_ASSERT(baseB != 0, "Process B mapped view base must be non-zero");

    TEST_ASSERT(secObj->getMappedViewCount() == 2, "Section object must register 2 active mapped views");

    // Coherency test: Write from Process A, read from Process B
    const char testPayload[] = "Dave Cutler MICA Shared Memory IPC";
    status = sm.writeView(*procA, baseA, 128, testPayload, sizeof(testPayload));
    TEST_ASSERT(NT_SUCCESS(status), "Process A writeView must succeed");

    char readBuffer[64] = {0};
    status = sm.readView(*procB, baseB, 128, readBuffer, sizeof(readBuffer));
    TEST_ASSERT(NT_SUCCESS(status), "Process B readView must succeed");
    TEST_ASSERT(std::strcmp(readBuffer, testPayload) == 0,
                "Process B must immediately observe data written by Process A across shared views");

    // 3. Copy-on-Write (PAGE_WRITECOPY) Isolation Test
    auto procC = pm.createProcess(L"WorkerProcessC.exe");
    TEST_ASSERT(procC != nullptr, "Process C must be created");

    uintptr_t baseC = 0;
    size_t sizeC = 64 * 1024;
    status = sm.mapViewOfSection(secObj, *procC, baseC, sizeC, mm::MEM_COMMIT, mm::PAGE_WRITECOPY);
    TEST_ASSERT(NT_SUCCESS(status), "Mapping view with PAGE_WRITECOPY must succeed");

    // Process C reads initial shared data
    char readBufferC[64] = {0};
    status = sm.readView(*procC, baseC, 128, readBufferC, sizeof(readBufferC));
    TEST_ASSERT(NT_SUCCESS(status), "Process C readView before write must succeed");
    TEST_ASSERT(std::strcmp(readBufferC, testPayload) == 0, "Process C must see shared data initially");

    // Process C writes private data (triggers COW decoupling)
    const char privatePayload[] = "Process C Private Modification";
    status = sm.writeView(*procC, baseC, 128, privatePayload, sizeof(privatePayload));
    TEST_ASSERT(NT_SUCCESS(status), "Process C writeView under PAGE_WRITECOPY must succeed");

    // Verify Process C sees private copy
    std::memset(readBufferC, 0, sizeof(readBufferC));
    status = sm.readView(*procC, baseC, 128, readBufferC, sizeof(readBufferC));
    TEST_ASSERT(NT_SUCCESS(status), "Process C readView after COW must succeed");
    TEST_ASSERT(std::strcmp(readBufferC, privatePayload) == 0, "Process C must see its private modification");

    // Verify Processes A and B STILL see original shared data (unaffected by Process C's write)
    std::memset(readBuffer, 0, sizeof(readBuffer));
    status = sm.readView(*procA, baseA, 128, readBuffer, sizeof(readBuffer));
    TEST_ASSERT(NT_SUCCESS(status), "Process A readView after Process C write must succeed");
    TEST_ASSERT(std::strcmp(readBuffer, testPayload) == 0,
                "Shared backing store in Process A must be preserved and untouched by COW write");

    auto telemetry = sm.getTelemetry();
    TEST_ASSERT(telemetry.totalCowSplits >= 1, "Telemetry must record at least 1 COW split");

    // 4. File-Backed Section (SEC_FILE) Creation
    Handle fileSecHandle = 0;
    std::shared_ptr<section::SectionObject> fileSecObj;
    status = sm.createFileBackedSection(
        fileSecHandle,
        handleTable,
        nullptr,
        L"\\Device\\Harddisk0\\Partition1\\Windows\\System32\\ntoskrnl.exe",
        128 * 1024,
        section::SEC_COMMIT,
        mm::PAGE_READONLY,
        fileSecObj,
        L"\\BaseNamedObjects\\KernelImageFileSection"
    );
    TEST_ASSERT(NT_SUCCESS(status), "File-backed section creation must succeed");
    TEST_ASSERT(fileSecObj->getControlArea()->isFile, "CONTROL_AREA must be marked isFile");
    TEST_ASSERT(fileSecObj->getControlArea()->backingFilePath == L"\\Device\\Harddisk0\\Partition1\\Windows\\System32\\ntoskrnl.exe",
                "Backing file path must match specified target");

    // 5. Executable Image Section (SEC_IMAGE) & Subsections
    Handle imgSecHandle = 0;
    std::shared_ptr<section::SectionObject> imgSecObj;
    status = sm.createSection(
        imgSecHandle,
        handleTable,
        1024 * 1024, // 1 MB = 256 pages
        section::SEC_IMAGE | section::SEC_COMMIT,
        mm::PAGE_EXECUTE_READWRITE,
        imgSecObj,
        L"\\BaseNamedObjects\\TestImageSection"
    );
    TEST_ASSERT(NT_SUCCESS(status), "SEC_IMAGE section creation must succeed");
    TEST_ASSERT(imgSecObj->getControlArea()->isImage, "CONTROL_AREA must be marked isImage");

    const auto& subsections = imgSecObj->getSegment()->subsections;
    TEST_ASSERT(subsections.size() == 3, "Image section must contain 3 subsections (.text, .rdata, .data)");
    TEST_ASSERT(subsections[0].subsectionName == ".text", "First subsection must be .text");
    TEST_ASSERT(subsections[0].protection == mm::PAGE_EXECUTE_READ, ".text protection must be PAGE_EXECUTE_READ");
    TEST_ASSERT(subsections[1].subsectionName == ".rdata", "Second subsection must be .rdata");
    TEST_ASSERT(subsections[1].protection == mm::PAGE_READONLY, ".rdata protection must be PAGE_READONLY");
    TEST_ASSERT(subsections[2].subsectionName == ".data", "Third subsection must be .data");
    TEST_ASSERT(subsections[2].protection == mm::PAGE_READWRITE, ".data protection must be PAGE_READWRITE");

    const auto& imgInfo = imgSecObj->getImageInformation();
    TEST_ASSERT(imgInfo.transferAddress != nullptr, "Image transfer address (entry point) must be non-null");
    TEST_ASSERT(imgInfo.machine == 0x8664, "Target machine architecture must be AMD64");

    // 6. Virtual Memory Flush (MmFlushVirtualMemory)
    status = section::MmFlushVirtualMemory(*procA, baseA, 64 * 1024);
    TEST_ASSERT(NT_SUCCESS(status), "MmFlushVirtualMemory must succeed");
    telemetry = sm.getTelemetry();
    TEST_ASSERT(telemetry.totalFlushedPages >= 16, "Flushed pages count must be >= 16");

    // 7. View Unmapping & Section Teardown
    status = sm.unmapViewOfSection(*procA, baseA);
    TEST_ASSERT(NT_SUCCESS(status), "Unmapping Process A view must succeed");
    status = sm.unmapViewOfSection(*procB, baseB);
    TEST_ASSERT(NT_SUCCESS(status), "Unmapping Process B view must succeed");
    status = sm.unmapViewOfSection(*procC, baseC);
    TEST_ASSERT(NT_SUCCESS(status), "Unmapping Process C view must succeed");
    TEST_ASSERT(secObj->getMappedViewCount() == 0, "Active mapped view count must return to 0");

    // Terminate test processes
    procA->terminate(NtStatus::Success);
    procB->terminate(NtStatus::Success);
    procC->terminate(NtStatus::Success);

    // 8. Telemetry & Manager Reset
    sm.resetForTesting();
    auto finalTelemetry = sm.getTelemetry();
    TEST_ASSERT(finalTelemetry.totalActiveSections == 0, "Active sections must be 0 after reset");

    std::cout << "[TEST] Suite 234: Horizon 18 - Sovereign Section Objects, Memory Mapped Files & Control Area Architecture PASSED.\n";
}

// ============================================================================
// Suite 235: Horizon 19 - Sovereign Kernel Dispatcher Database, KWAIT_BLOCK,
// Multi-Wait (WaitAll/WaitAny) & Mutex Priority Inheritance
// ============================================================================
inline void Test_Horizon19_DispatcherDatabase_MultiWait_And_PriorityInheritance() {
    std::cout << "[TEST] Running Suite 235: Horizon 19 - Sovereign Kernel Dispatcher Database, Multi-Wait & Priority Inheritance...\n";

    using namespace micant::ke;
    using namespace micant::sync;

    // Reset telemetry for clean baseline
    KeResetDispatcherTelemetryForTesting();

    // 1. KEVENT Subsystem (NotificationEvent vs SynchronizationEvent)
    KEVENT notifEvent;
    KeInitializeEvent(&notifEvent, DISPATCHER_TYPE::NotificationEvent, false);
    TEST_ASSERT(notifEvent.header.type == DISPATCHER_TYPE::NotificationEvent, "NotificationEvent type must match");
    TEST_ASSERT(notifEvent.header.signalState == 0, "Initial signal state must be 0 (unsignaled)");

    int64_t zeroTimeout = 0;
    NtStatus status = KeWaitForSingleObject(&notifEvent, KWAIT_REASON::Executive, 0, false, &zeroTimeout);
    TEST_ASSERT(status == NtStatus::Timeout, "Wait on unsignaled NotificationEvent must time out");

    // Signal NotificationEvent
    int32_t prevSignaled = KeSetEvent(&notifEvent, 0, false);
    TEST_ASSERT(prevSignaled == 0, "Previous state was unsignaled (0)");
    TEST_ASSERT(notifEvent.header.signalState == 1, "Signal state must be 1 after KeSetEvent");

    // Wait on signaled NotificationEvent - should succeed AND remain signaled (manual reset)
    status = KeWaitForSingleObject(&notifEvent, KWAIT_REASON::Executive, 0, false, &zeroTimeout);
    TEST_ASSERT(status == NtStatus::Success, "Wait on signaled NotificationEvent must succeed");
    TEST_ASSERT(notifEvent.header.signalState == 1, "NotificationEvent must stay signaled after wait");

    // Reset NotificationEvent
    int32_t prevBeforeReset = KeResetEvent(&notifEvent);
    TEST_ASSERT(prevBeforeReset == 1, "Previous state before KeResetEvent was 1");
    TEST_ASSERT(notifEvent.header.signalState == 0, "State after KeResetEvent must be 0");

    // SynchronizationEvent (Auto-Reset)
    KEVENT syncEvent;
    KeInitializeEvent(&syncEvent, DISPATCHER_TYPE::SynchronizationEvent, false);
    KeSetEvent(&syncEvent, 0, false);
    TEST_ASSERT(syncEvent.header.signalState == 1, "SynchronizationEvent must be signaled");

    // Wait on SynchronizationEvent - must succeed AND auto-reset to 0
    status = KeWaitForSingleObject(&syncEvent, KWAIT_REASON::Executive, 0, false, &zeroTimeout);
    TEST_ASSERT(status == NtStatus::Success, "Wait on signaled SynchronizationEvent must succeed");
    TEST_ASSERT(syncEvent.header.signalState == 0, "SynchronizationEvent must auto-reset to 0 upon wait satisfaction");

    // PulseEvent test
    KePulseEvent(&syncEvent, 0, false);
    TEST_ASSERT(syncEvent.header.signalState == 0, "PulseEvent must leave event in unsignaled state");

    // 2. KSEMAPHORE Subsystem
    KSEMAPHORE sem;
    KeInitializeSemaphore(&sem, 2, 3);
    TEST_ASSERT(sem.header.type == DISPATCHER_TYPE::Semaphore, "Semaphore type must match");
    TEST_ASSERT(sem.header.signalState == 2, "Initial semaphore count must be 2");
    TEST_ASSERT(sem.limit == 3, "Semaphore limit must be 3");

    // Consume 1 token
    status = KeWaitForSingleObject(&sem, KWAIT_REASON::Executive, 0, false, &zeroTimeout);
    TEST_ASSERT(status == NtStatus::Success, "First wait on semaphore must consume 1 token");
    TEST_ASSERT(sem.header.signalState == 1, "Semaphore count must be 1");

    // Consume 2nd token
    status = KeWaitForSingleObject(&sem, KWAIT_REASON::Executive, 0, false, &zeroTimeout);
    TEST_ASSERT(status == NtStatus::Success, "Second wait on semaphore must consume 1 token");
    TEST_ASSERT(sem.header.signalState == 0, "Semaphore count must be 0");

    // Third wait must time out
    status = KeWaitForSingleObject(&sem, KWAIT_REASON::Executive, 0, false, &zeroTimeout);
    TEST_ASSERT(status == NtStatus::Timeout, "Third wait on semaphore with 0 count must time out");

    // Release 2 tokens
    int32_t prevCount = KeReleaseSemaphore(&sem, 0, 2, false);
    TEST_ASSERT(prevCount == 0, "Previous semaphore count was 0");
    TEST_ASSERT(sem.header.signalState == 2, "Semaphore count after release of 2 must be 2");

    // 3. KMUTANT / KMUTEX & Dave Cutler Priority Inheritance Engine
    KMUTANT mut;
    KeInitializeMutant(&mut, false);
    TEST_ASSERT(mut.header.type == DISPATCHER_TYPE::Mutant, "Mutant type must match");
    TEST_ASSERT(mut.header.signalState == 1, "Unowned mutant must have signalState = 1");
    TEST_ASSERT(mut.ownerThread == nullptr, "Unowned mutant has no owner");

    // Create simulated KThread objects
    KThread lowPrioThread{};
    lowPrioThread.tid = 1001;
    lowPrioThread.priority = 8;
    lowPrioThread.basePriority = 8;
    lowPrioThread.state = ThreadState::Running;

    KThread highPrioThread{};
    highPrioThread.tid = 1002;
    highPrioThread.priority = 14;
    highPrioThread.basePriority = 14;
    highPrioThread.state = ThreadState::Ready;

    // Low priority thread acquires mutant
    status = KeWaitForSingleObject(&mut, KWAIT_REASON::Executive, 0, false, nullptr, &lowPrioThread);
    TEST_ASSERT(status == NtStatus::Success, "lowPrioThread must acquire mutant");
    TEST_ASSERT(mut.header.signalState == 0, "Acquired mutant signalState must be 0");
    TEST_ASSERT(mut.ownerThread == &lowPrioThread, "Owner thread must be lowPrioThread");
    TEST_ASSERT(mut.ownerThreadId == 1001, "Owner TID must be 1001");
    TEST_ASSERT(mut.recursionCount == 1, "Recursion count must be 1");

    // Recursive acquisition by same thread
    status = KeWaitForSingleObject(&mut, KWAIT_REASON::Executive, 0, false, nullptr, &lowPrioThread);
    TEST_ASSERT(status == NtStatus::Success, "Recursive acquire by owner must succeed");
    TEST_ASSERT(mut.recursionCount == 2, "Recursion count must be 2");

    // Release one recursion level
    int32_t oldSig = KeReleaseMutant(&mut, 0, false, false);
    TEST_ASSERT(oldSig == 0, "Mutant still held after first recursive release");
    TEST_ASSERT(mut.recursionCount == 1, "Recursion count must decrement to 1");

    // High-priority thread attempts to acquire the mutant in a separate worker thread
    // Priority Inheritance should boost lowPrioThread's priority from 8 to 14
    std::atomic<bool> highThreadStarted{false};
    std::atomic<bool> highThreadAcquired{false};

    std::thread waiterThread([&]() {
        highThreadStarted = true;
        int64_t waitTimeout = 1000; // 1000ms
        NtStatus s = KeWaitForSingleObject(&mut, KWAIT_REASON::Executive, 0, false, &waitTimeout, &highPrioThread);
        if (s == NtStatus::Success) {
            highThreadAcquired = true;
            KeReleaseMutant(&mut, 0, false, false);
        }
    });

    while (!highThreadStarted) {
        std::this_thread::yield();
    }
    // Give waiter time to register in waitList and trigger priority inheritance
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Verify Priority Inheritance Boost: lowPrioThread must have inherited priority 14
    TEST_ASSERT(lowPrioThread.priority == 14, "lowPrioThread must be boosted to priority 14 by Priority Inheritance");
    TEST_ASSERT(mut.priorityInherited == true, "Mutant priorityInherited flag must be true");
    TEST_ASSERT(mut.originalPriority == 8, "Mutant originalPriority must be preserved as 8");

    // Low priority thread releases mutant
    KeReleaseMutant(&mut, 0, false, false);

    // Verify lowPrioThread priority restored to original base priority (8)
    TEST_ASSERT(lowPrioThread.priority == 8, "lowPrioThread priority must be restored to original 8 upon mutant release");

    waiterThread.join();
    TEST_ASSERT(highThreadAcquired, "highPrioThread must have successfully acquired mutant once released");

    // 4. KeWaitForMultipleObjects (WaitAny & WaitAll)
    KEVENT ev0, ev1, ev2;
    KeInitializeEvent(&ev0, DISPATCHER_TYPE::SynchronizationEvent, false);
    KeInitializeEvent(&ev1, DISPATCHER_TYPE::SynchronizationEvent, false);
    KeInitializeEvent(&ev2, DISPATCHER_TYPE::SynchronizationEvent, false);

    void* waitAnyArray[3] = { &ev0, &ev1, &ev2 };

    // Signal middle event (ev1)
    KeSetEvent(&ev1, 0, false);

    // WaitAny should return STATUS_WAIT_1 immediately
    status = KeWaitForMultipleObjects(3, waitAnyArray, WAIT_TYPE::WaitAny, KWAIT_REASON::Executive, 0, false, &zeroTimeout);
    TEST_ASSERT(status == static_cast<NtStatus>(STATUS_WAIT_1), "WaitAny must return STATUS_WAIT_1 for signaled ev1");
    TEST_ASSERT(ev1.header.signalState == 0, "ev1 (SynchronizationEvent) must auto-reset on satisfaction");

    // WaitAll test
    KEVENT allEv0, allEv1;
    KeInitializeEvent(&allEv0, DISPATCHER_TYPE::NotificationEvent, false);
    KeInitializeEvent(&allEv1, DISPATCHER_TYPE::NotificationEvent, false);

    void* waitAllArray[2] = { &allEv0, &allEv1 };

    // Signal only allEv0
    KeSetEvent(&allEv0, 0, false);
    int64_t multiTimeout = 20; // 20ms
    status = KeWaitForMultipleObjects(2, waitAllArray, WAIT_TYPE::WaitAll, KWAIT_REASON::Executive, 0, false, &multiTimeout);
    TEST_ASSERT(status == NtStatus::Timeout, "WaitAll must time out if only 1 of 2 events is signaled");

    // Now signal allEv1
    KeSetEvent(&allEv1, 0, false);

    // WaitAll should now succeed with STATUS_WAIT_0
    status = KeWaitForMultipleObjects(2, waitAllArray, WAIT_TYPE::WaitAll, KWAIT_REASON::Executive, 0, false, &zeroTimeout);
    TEST_ASSERT(status == static_cast<NtStatus>(STATUS_WAIT_0), "WaitAll must succeed with STATUS_WAIT_0 when all events are signaled");

    // 5. Telemetry Verification & Teardown
    auto telemetry = KeQueryDispatcherTelemetry();
    TEST_ASSERT(telemetry.totalSingleWaits >= 4, "totalSingleWaits telemetry must be >= 4");
    TEST_ASSERT(telemetry.totalMultiWaitsAny >= 1, "totalMultiWaitsAny telemetry must be >= 1");
    TEST_ASSERT(telemetry.totalMultiWaitsAll >= 2, "totalMultiWaitsAll telemetry must be >= 2");
    TEST_ASSERT(telemetry.totalPriorityInheritanceBoosts >= 1, "totalPriorityInheritanceBoosts must be >= 1");
    TEST_ASSERT(telemetry.totalMutantsAcquired >= 2, "totalMutantsAcquired must be >= 2");
    TEST_ASSERT(telemetry.totalEventsSet >= 4, "totalEventsSet must be >= 4");
    TEST_ASSERT(telemetry.totalSemaphoresReleased >= 2, "totalSemaphoresReleased must be >= 2");

    KeResetDispatcherTelemetryForTesting();
    auto resetTelem = KeQueryDispatcherTelemetry();
    TEST_ASSERT(resetTelem.totalSingleWaits == 0, "Telemetry reset must clear totalSingleWaits");
    TEST_ASSERT(resetTelem.totalPriorityInheritanceBoosts == 0, "Telemetry reset must clear totalPriorityInheritanceBoosts");

    std::cout << "[TEST] Suite 235: Horizon 19 - Sovereign Kernel Dispatcher Database, Multi-Wait & Priority Inheritance PASSED.\n";
}

// ============================================================================
// Suite 236: Horizon 20 - Sovereign Object Manager Refinement, Lifecycle,
// Symbolic Link Loop Prevention & Parse Callbacks
// ============================================================================
inline void Test_Horizon20_ObjectManager_Lifecycle_Symlinks_And_Callbacks() {
    std::cout << "[TEST] Running Suite 236: Horizon 20 - Sovereign Object Manager Refinement, Symlinks & Callbacks...\n";

    using namespace micant::ob;

    ObResetTelemetryForTesting();

    // 1. Dave Cutler Object Header Substructures & ObCreateObject
    struct DeviceBody {
        uint32_t magic;
        uint32_t status;
    };

    static std::atomic<uint32_t> s_OpenCalls{0};
    static std::atomic<uint32_t> s_CloseCalls{0};
    static std::atomic<uint32_t> s_DeleteCalls{0};
    static std::atomic<uint32_t> s_ParseCalls{0};

    s_OpenCalls = 0;
    s_CloseCalls = 0;
    s_DeleteCalls = 0;
    s_ParseCalls = 0;

    ObjectType deviceType{
        .typeName = L"SovereignDevice",
        .typeId = ObjectTypeId::Device,
        .openProcedure = [](ObjectHeader* /*obj*/, uint32_t /*mask*/) -> NtStatus {
            s_OpenCalls.fetch_add(1);
            return NtStatus::Success;
        },
        .closeProcedure = [](ObjectHeader* /*obj*/, Handle /*h*/) {
            s_CloseCalls.fetch_add(1);
        },
        .deleteProcedure = [](ObjectHeader* /*obj*/) {
            s_DeleteCalls.fetch_add(1);
        }
    };

    UnicodeString devName(L"\\Device\\SovereignDevice0");
    ObjectAttributes devAttrs{
        .length = sizeof(ObjectAttributes),
        .rootDirectory = 0,
        .objectName = &devName,
        .attributes = OBJ_CASE_INSENSITIVE
    };

    void* bodyPtr = nullptr;
    ObjectHeader* headerPtr = nullptr;
    NtStatus status = ObCreateObject(
        KernelMode,
        &deviceType,
        &devAttrs,
        sizeof(DeviceBody),
        &bodyPtr,
        &headerPtr
    );
    TEST_ASSERT(NT_SUCCESS(status), "ObCreateObject must succeed");
    TEST_ASSERT(bodyPtr != nullptr, "Object body pointer must not be null");
    TEST_ASSERT(headerPtr != nullptr, "Object header pointer must not be null");

    auto* devBody = reinterpret_cast<DeviceBody*>(bodyPtr);
    devBody->magic = 0xBEEFCAFE;
    devBody->status = 1;

    // Verify Dave Cutler Object Header substructures
    TEST_ASSERT(headerPtr->nameInfo.name == L"\\Device\\SovereignDevice0", "OBJECT_HEADER_NAME_INFO name must match");
    TEST_ASSERT(headerPtr->creatorInfo.creatorProcessId == 4, "Creator PID must be 4");
    TEST_ASSERT(headerPtr->quotaInfo.pagedPoolCharge == sizeof(DeviceBody), "PagedPool charge must equal body size");
    TEST_ASSERT(headerPtr->quotaInfo.nonPagedPoolCharge == sizeof(ObjectHeader), "NonPagedPool charge must equal header size");
    TEST_ASSERT(headerPtr->dynamicallyAllocated, "Header must be marked dynamicallyAllocated");

    // 2. Object Insertion & Handle Table Integration
    HandleTable handleTable;
    Handle hDevice = 0;
    status = ObInsertObject(headerPtr, &devAttrs, GENERIC_READ | GENERIC_WRITE, &handleTable, &hDevice);
    TEST_ASSERT(NT_SUCCESS(status), "ObInsertObject must succeed");
    TEST_ASSERT(hDevice == 4, "First allocated handle must be 4");
    TEST_ASSERT(s_OpenCalls.load() == 1, "openProcedure must be called on handle creation");
    TEST_ASSERT(headerPtr->handleCount.load() == 1, "Handle count must be 1");
    TEST_ASSERT(headerPtr->pointerCount.load() == 3, "Pointer count must be 3 (1 from alloc, 1 from directory, 1 from handle)");

    // Lookup in ObjectNamespace
    auto* foundObj = ObjectNamespace::get().lookupObject(L"\\Device\\SovereignDevice0");
    TEST_ASSERT(foundObj == headerPtr, "ObjectNamespace must find inserted object");

    // 3. Parse Procedure Interception
    static ObjectHeader s_ParsedChildHeader;
    s_ParsedChildHeader.pointerCount.store(1);
    s_ParsedChildHeader.objectName = L"ParsedChildFile";

    ObjectType volumeType{
        .typeName = L"SovereignVolume",
        .typeId = ObjectTypeId::Device,
        .parseProcedure = [](ObjectHeader* /*parseObj*/, const ObjectType* /*expectedType*/, std::wstring_view remainingPath, ObjectHeader** outFound) -> NtStatus {
            s_ParseCalls.fetch_add(1);
            if (remainingPath == L"\\Subdir\\TargetFile.txt") {
                *outFound = &s_ParsedChildHeader;
                return NtStatus::Success;
            }
            return NtStatus::ObjectNameNotFound;
        }
    };

    UnicodeString volName(L"\\Device\\SovereignVolume0");
    ObjectAttributes volAttrs{
        .length = sizeof(ObjectAttributes),
        .rootDirectory = 0,
        .objectName = &volName,
        .attributes = OBJ_PERMANENT
    };
    void* volBody = nullptr;
    ObjectHeader* volHeader = nullptr;
    status = ObCreateObject(KernelMode, &volumeType, &volAttrs, 16, &volBody, &volHeader);
    TEST_ASSERT(NT_SUCCESS(status), "Creating volume object must succeed");

    status = ObInsertObject(volHeader, &volAttrs, GENERIC_READ, nullptr, nullptr);
    TEST_ASSERT(NT_SUCCESS(status), "Inserting volume object must succeed");

    // Query path through volume parse procedure
    NtStatus parseLookupStatus = NtStatus::Success;
    auto* targetFile = ObjectNamespace::get().lookupObject(L"\\Device\\SovereignVolume0\\Subdir\\TargetFile.txt", nullptr, &parseLookupStatus);
    TEST_ASSERT(NT_SUCCESS(parseLookupStatus), "Parse procedure lookup must succeed");
    TEST_ASSERT(targetFile == &s_ParsedChildHeader, "Target file returned must match s_ParsedChildHeader");
    TEST_ASSERT(s_ParseCalls.load() == 1, "parseProcedure must have been called once");

    // 4. Symbolic Link Loop & Boundary Protection (32-hop cap & cycle detection)
    auto& obNs = ObjectNamespace::get();
    obNs.createSymbolicLink(L"\\DosDevices\\LoopA", L"\\DosDevices\\LoopB");
    obNs.createSymbolicLink(L"\\DosDevices\\LoopB", L"\\DosDevices\\LoopC");
    obNs.createSymbolicLink(L"\\DosDevices\\LoopC", L"\\DosDevices\\LoopA");

    NtStatus symlinkStatus = NtStatus::Success;
    std::wstring loopResolved = obNs.resolvePath(L"\\DosDevices\\LoopA", &symlinkStatus);
    TEST_ASSERT(symlinkStatus == NtStatus::CircularReference, "Circular symlink must return STATUS_CIRCULAR_REFERENCE");

    // 5. Security Descriptor & Access Checking
    static int dummySecDesc = 42;
    headerPtr->securityDescriptor = &dummySecDesc;

    ObSetAccessCheckProcedure([](void* secDesc, uint32_t desiredAccess, uint32_t* grantedAccess) -> NtStatus {
        if (!secDesc) return NtStatus::Success;
        if (desiredAccess & WRITE_DAC) {
            return NtStatus::AccessDenied;
        }
        if (grantedAccess) *grantedAccess = desiredAccess;
        return NtStatus::Success;
    });

    Handle hAllowed = 0;
    status = ObOpenObjectByName(&devAttrs, &deviceType, KernelMode, GENERIC_READ, handleTable, hAllowed);
    TEST_ASSERT(NT_SUCCESS(status), "Allowed access check must succeed");
    TEST_ASSERT(hAllowed != 0, "Valid handle must be returned");
    ObCloseHandle(hAllowed, handleTable);

    Handle hDenied = 0;
    status = ObOpenObjectByName(&devAttrs, &deviceType, KernelMode, WRITE_DAC, handleTable, hDenied);
    TEST_ASSERT(status == NtStatus::AccessDenied, "Denied access check must return STATUS_ACCESS_DENIED");
    TEST_ASSERT(hDenied == 0, "No handle should be returned on access denial");

    ObSetAccessCheckProcedure(nullptr);

    // 6. Temporary Object Lifecycle & Auto-Pruning
    TEST_ASSERT(!headerPtr->isPermanent(), "Object must not be permanent");
    status = ObCloseHandle(hDevice, handleTable);
    TEST_ASSERT(NT_SUCCESS(status), "Closing initial handle must succeed");
    TEST_ASSERT(s_CloseCalls.load() == 2, "closeProcedure must have been called twice (once for hAllowed, once for hDevice)");
    TEST_ASSERT(headerPtr->handleCount.load() == 0, "Handle count must now be 0");

    // Because object was temporary and all handles closed, it must be automatically pruned from directory
    auto* prunedCheck = ObjectNamespace::get().lookupObject(L"\\Device\\SovereignDevice0");
    TEST_ASSERT(prunedCheck == nullptr, "Temporary object with 0 handles must be automatically pruned from directory");

    // Release remaining reference
    ObDereferenceObject(headerPtr);
    TEST_ASSERT(s_DeleteCalls.load() == 1, "deleteProcedure must be called when pointer count reaches 0");

    // Clean up volume object
    obNs.getDirectory(L"\\Device")->removeObject(L"SovereignVolume0");
    ObDereferenceObject(volHeader);

    // 7. Telemetry Verification & Teardown
    auto telemetry = ObQueryTelemetry();
    TEST_ASSERT(telemetry.totalObjectsCreated >= 2, "totalObjectsCreated must be >= 2");
    TEST_ASSERT(telemetry.totalHandlesOpened >= 2, "totalHandlesOpened must be >= 2");
    TEST_ASSERT(telemetry.totalHandlesClosed >= 2, "totalHandlesClosed must be >= 2");
    TEST_ASSERT(telemetry.totalParseProcedureCalls >= 1, "totalParseProcedureCalls must be >= 1");
    TEST_ASSERT(telemetry.totalSymlinkRecursionsHit >= 1, "totalSymlinkRecursionsHit must be >= 1");
    TEST_ASSERT(telemetry.totalSecurityChecksPassed >= 1, "totalSecurityChecksPassed must be >= 1");
    TEST_ASSERT(telemetry.totalSecurityChecksDenied >= 1, "totalSecurityChecksDenied must be >= 1");
    TEST_ASSERT(telemetry.totalTemporaryObjectsUnlinked >= 1, "totalTemporaryObjectsUnlinked must be >= 1");

    ObResetTelemetryForTesting();
    auto resetTelem = ObQueryTelemetry();
    TEST_ASSERT(resetTelem.totalObjectsCreated == 0, "Telemetry reset must clear counts");

    std::cout << "[TEST] Suite 236: Horizon 20 - Sovereign Object Manager Refinement, Symlinks & Callbacks PASSED.\n";
}

void Test_Horizon21_ExecutiveLPC_ALPC_ThreeWayHandshake_And_Impersonation() {
    std::cout << "[TEST] Running Suite 237: Horizon 21 - Sovereign Executive LPC/ALPC Architecture, 3-Way Handshake & Impersonation Engine...\n";

    // 0. Reset and verify baseline telemetry
    lpc::LpcResetTelemetryForTesting();
    auto initialTelem = lpc::LpcQueryTelemetry();
    TEST_ASSERT(initialTelem.totalConnectionPortsCreated == 0, "Telemetry must start clean");

    auto& portMgr = lpc::PortManager::get();
    portMgr.resetForTesting();

    // 1. Connection Port Creation with Security Descriptor
    auto userSid = se::Sid::authenticatedUsers();
    auto adminSid = se::Sid::administrators();

    se::Acl dacl;
    dacl.addAllowedAce(userSid, se::GENERIC_READ | se::GENERIC_WRITE);
    dacl.addAllowedAce(adminSid, se::GENERIC_ALL);

    se::SecurityDescriptor portSd;
    portSd.setOwner(adminSid);
    portSd.setGroup(adminSid);
    portSd.setDacl(std::move(dacl));

    std::shared_ptr<lpc::PortObject> serverConnPort;
    NtStatus createStatus = lpc::LpcCreatePort(L"\\RPC Control\\Horizon21Port", serverConnPort, std::move(portSd));
    TEST_ASSERT(NT_SUCCESS(createStatus), "LpcCreatePort must succeed for \\RPC Control\\Horizon21Port");
    TEST_ASSERT(serverConnPort != nullptr, "Created connection port must be non-null");
    TEST_ASSERT(serverConnPort->getType() == lpc::PortType::ConnectionPort, "Port type must be ConnectionPort");

    // Duplicate create collision check
    std::shared_ptr<lpc::PortObject> dupPort;
    NtStatus dupStatus = lpc::LpcCreatePort(L"\\RPC Control\\Horizon21Port", dupPort);
    TEST_ASSERT(dupStatus == NtStatus::ObjectNameCollision, "Duplicate LpcCreatePort must return STATUS_OBJECT_NAME_COLLISION");

    // 2. Canonical 3-Way Handshake Connection Protocol (Client connectPortEx -> Server accept)
    std::shared_ptr<lpc::PortObject> clientCommPort;
    std::shared_ptr<lpc::PortObject> serverCommPort;
    std::atomic<bool> handshakeDone{false};
    std::atomic<NtStatus> clientConnectStatus{NtStatus::Unsuccessful};

    std::string clientHello = "ALPC_CLIENT_HANDSHAKE_V1";
    std::vector<uint8_t> connectData(clientHello.begin(), clientHello.end());

    lpc::AlpcMessageAttributes clientAttrs{};
    clientAttrs.validAttributes = lpc::ALPC_MESSAGE_SECURITY_ATTRIBUTE;
    clientAttrs.security = lpc::AlpcSecurityAttribute{
        .flags = lpc::ALPC_MESSAGE_SECURITY_ATTRIBUTE,
        .userSid = userSid,
        .integrityLevel = se::SECURITY_MANDATORY_MEDIUM_RID
    };

    auto clientToken = se::TokenObject::createUserToken(userSid);

    std::thread clientThread([&]() {
        clientConnectStatus = portMgr.connectPortEx(
            L"\\RPC Control\\Horizon21Port",
            connectData,
            clientCommPort,
            clientToken.get(),
            &clientAttrs,
            5000
        );
        handshakeDone = true;
    });

    // Server retrieves pending connection request
    lpc::PendingConnectRequest pendingReq;
    bool gotReq = serverConnPort->getNextPendingConnection(pendingReq, 3000);
    TEST_ASSERT(gotReq, "Server must dequeue pending connection request");
    TEST_ASSERT(pendingReq.connectionId != 0, "Connection ID must be assigned");
    TEST_ASSERT(pendingReq.connectMessage.u2.type == static_cast<uint16_t>(lpc::PortMessageType::LpcConnectionRequest), 
                "Pending message type must be LpcConnectionRequest");

    std::string receivedHello(reinterpret_cast<char*>(pendingReq.connectData.data()), pendingReq.connectData.size());
    TEST_ASSERT(receivedHello == clientHello, "Server received connection payload must match client hello");

    // Server accepts connection and establishes server communication port
    serverCommPort = std::make_shared<lpc::PortObject>(
        L"\\RPC Control\\Horizon21Port:Server:" + std::to_wstring(pendingReq.connectionId),
        lpc::PortType::ServerCommunicationPort
    );
    bool resolved = serverConnPort->resolvePendingConnection(pendingReq.connectionId, true, serverCommPort);
    TEST_ASSERT(resolved, "resolvePendingConnection with accept=true must succeed");

    clientThread.join();
    TEST_ASSERT(NT_SUCCESS(clientConnectStatus.load()), "Client connectPortEx must succeed on server acceptance");
    TEST_ASSERT(clientCommPort != nullptr, "clientCommPort must be valid");
    TEST_ASSERT(clientCommPort->isConnected(), "clientCommPort must be connected to peer");
    TEST_ASSERT(serverCommPort->isConnected(), "serverCommPort must be connected to peer");

    // 3. Connection Refusal (Server rejects unauthorized or refused connection)
    std::shared_ptr<lpc::PortObject> refusedClientCommPort;
    std::atomic<NtStatus> refusedConnectStatus{NtStatus::Success};

    std::thread refusedClientThread([&]() {
        std::string untrustedHello = "UNTRUSTED_CLIENT_ATTEMPT";
        std::vector<uint8_t> untrustedData(untrustedHello.begin(), untrustedHello.end());
        refusedConnectStatus = portMgr.connectPortEx(
            L"\\RPC Control\\Horizon21Port",
            untrustedData,
            refusedClientCommPort,
            clientToken.get(),
            nullptr,
            5000
        );
    });

    lpc::PendingConnectRequest refusedReq;
    bool gotRefused = serverConnPort->getNextPendingConnection(refusedReq, 3000);
    TEST_ASSERT(gotRefused, "Server must retrieve second pending connection");
    bool rejectResolved = serverConnPort->resolvePendingConnection(refusedReq.connectionId, false, nullptr);
    TEST_ASSERT(rejectResolved, "resolvePendingConnection with accept=false must succeed");

    refusedClientThread.join();
    TEST_ASSERT(refusedConnectStatus.load() == NtStatus::PortConnectionRefused, 
                "Refused client connection must return STATUS_PORT_CONNECTION_REFUSED (0xC0000041)");
    TEST_ASSERT(refusedClientCommPort == nullptr, "Refused client port must remain null");

    // 4. Access Denied on Connection Attempt (Token not in DACL)
    se::Sid guestSid(5, {32, 546}); // Builtin Guests
    auto guestToken = std::make_shared<se::TokenObject>(guestSid);
    std::shared_ptr<lpc::PortObject> deniedPort;
    NtStatus deniedStatus = portMgr.connectPortEx(
        L"\\RPC Control\\Horizon21Port",
        connectData,
        deniedPort,
        guestToken.get(),
        nullptr,
        1000
    );
    TEST_ASSERT(deniedStatus == NtStatus::AccessDenied, "Token without rights in DACL must return STATUS_ACCESS_DENIED");
    TEST_ASSERT(deniedPort == nullptr, "Denied port must remain null");

    // 5. Canonical System Call Helpers: LpcRequestWaitReplyPort & LpcReplyWaitReceivePort Rendezvous
    std::atomic<bool> serverReplied{false};
    std::thread serverWorker([&]() {
        lpc::PortMessage reqHeader{};
        std::vector<uint8_t> reqPayload;

        // Server calls LpcReplyWaitReceivePort to wait for client request (no reply to send initially)
        NtStatus rcvStatus = lpc::LpcReplyWaitReceivePort(
            serverCommPort,
            nullptr,
            {},
            reqHeader,
            reqPayload,
            3000
        );
        if (NT_SUCCESS(rcvStatus)) {
            std::string cmd(reinterpret_cast<char*>(reqPayload.data()), reqPayload.size());
            if (cmd == "ALPC_QUERY_SYSTEM_TIME") {
                std::string reply = "SYSTEM_TIME_EPOCH_64";
                std::vector<uint8_t> replyBytes(reply.begin(), reply.end());
                lpc::LpcReplyPort(serverCommPort, reqHeader, replyBytes);
                serverReplied = true;
            }
        }
    });

    lpc::PortMessage clientReqHeader{};
    clientReqHeader.messageId = 0x7701;
    std::string clientQuery = "ALPC_QUERY_SYSTEM_TIME";
    std::vector<uint8_t> queryPayload(clientQuery.begin(), clientQuery.end());
    lpc::PortMessage clientReplyHeader{};
    std::vector<uint8_t> clientReplyPayload;

    NtStatus rwrStatus = lpc::LpcRequestWaitReplyPort(
        clientCommPort,
        clientReqHeader,
        queryPayload,
        clientReplyHeader,
        clientReplyPayload,
        5000
    );
    TEST_ASSERT(NT_SUCCESS(rwrStatus), "LpcRequestWaitReplyPort must succeed");
    serverWorker.join();
    TEST_ASSERT(serverReplied.load(), "Server must have dispatched reply");
    TEST_ASSERT(clientReplyHeader.messageId == 0x7701, "Reply message ID must match request ID");
    TEST_ASSERT(clientReplyHeader.u2.type == static_cast<uint16_t>(lpc::PortMessageType::LpcReply), 
                "Reply message type must be LpcReply");
    std::string replyStr(reinterpret_cast<char*>(clientReplyPayload.data()), clientReplyPayload.size());
    TEST_ASSERT(replyStr == "SYSTEM_TIME_EPOCH_64", "Synchronous reply payload must match server reply");

    // 6. Asynchronous Datagram Transmission (LpcRequestPort / sendDatagram)
    lpc::PortMessage datagramHeader{};
    datagramHeader.messageId = 0x8802;
    std::string datagramData = "ASYNC_FIRE_AND_FORGET_EVENT";
    std::vector<uint8_t> datagramPayload(datagramData.begin(), datagramData.end());

    NtStatus dgStatus = lpc::LpcRequestPort(clientCommPort, datagramHeader, datagramPayload);
    TEST_ASSERT(NT_SUCCESS(dgStatus), "LpcRequestPort datagram must succeed");

    lpc::PortMessage rcvDgHeader{};
    std::vector<uint8_t> rcvDgPayload;
    NtStatus rcvDgStatus = serverCommPort->receiveMessage(rcvDgHeader, rcvDgPayload, 2000);
    TEST_ASSERT(NT_SUCCESS(rcvDgStatus), "Server must receive datagram message");
    TEST_ASSERT(rcvDgHeader.u2.type == static_cast<uint16_t>(lpc::PortMessageType::LpcDatagram), 
                "Datagram message type must be LpcDatagram");
    std::string receivedDg(reinterpret_cast<char*>(rcvDgPayload.data()), rcvDgPayload.size());
    TEST_ASSERT(receivedDg == datagramData, "Datagram payload must match client transmission");

    // 7. Client Impersonation Engine (setClientToken, impersonateClient, revertToSelf)
    TEST_ASSERT(serverCommPort->getClientToken() != nullptr, "Server port must hold client security token");
    TEST_ASSERT(serverCommPort->getClientToken()->getUserSid() == userSid, "Client token SID must match AuthenticatedUser");

    std::shared_ptr<se::TokenObject> savedServerToken;
    auto serverCurrentToken = se::TokenObject::createSystemToken();
    NtStatus impStatus = serverCommPort->impersonateClient(savedServerToken, serverCurrentToken.get());
    TEST_ASSERT(NT_SUCCESS(impStatus), "impersonateClient must succeed");
    TEST_ASSERT(savedServerToken != nullptr, "Previous server token must be preserved in savedServerToken");
    TEST_ASSERT(savedServerToken->getUserSid() == se::Sid::localSystem(), 
                "Saved token must be LocalSystem");

    serverCommPort->revertToSelf(savedServerToken);
    TEST_ASSERT(savedServerToken == nullptr, "revertToSelf must clear saved token handle");

    // 8. ALPC Section Management & Large Data View (Shared Memory Transfer)
    uintptr_t sectionId = clientCommPort->createSection(1024 * 1024); // 1 MB ALPC section
    TEST_ASSERT(sectionId != 0, "createSection must return non-zero section ID");

    lpc::AlpcDataView dataView{};
    bool viewCreated = clientCommPort->createDataView(sectionId, 0, 65536, dataView);
    TEST_ASSERT(viewCreated, "createDataView for 64KB must succeed");
    TEST_ASSERT(dataView.viewBase != nullptr, "viewBase must be non-null");
    TEST_ASSERT(dataView.viewSize == 65536, "viewSize must be 65536");

    const char* magicSign = "MICANT_ALPC_ZERO_COPY_SECTION_PAYLOAD";
    size_t signLen = std::strlen(magicSign);
    std::memcpy(dataView.viewBase, magicSign, signLen);

    // Verify section content directly
    auto sectionObj = clientCommPort->getSection(sectionId);
    TEST_ASSERT(sectionObj != nullptr, "Section object must be retrievable");
    std::vector<uint8_t> readBack(signLen);
    bool readOk = sectionObj->readData(0, readBack);
    TEST_ASSERT(readOk, "readData from section must succeed");
    std::string readStr(reinterpret_cast<char*>(readBack.data()), readBack.size());
    TEST_ASSERT(readStr == magicSign, "Data read from section must match written payload");

    bool deletedSec = clientCommPort->deleteSection(sectionId);
    TEST_ASSERT(deletedSec, "deleteSection must return true");

    // 9. Port Disconnection & Telemetry Validation
    clientCommPort->disconnect();
    serverCommPort->disconnect();

    auto telemetry = lpc::LpcQueryTelemetry();
    TEST_ASSERT(telemetry.totalConnectionPortsCreated >= 1, "totalConnectionPortsCreated must be >= 1");
    TEST_ASSERT(telemetry.totalConnectionRequests >= 2, "totalConnectionRequests must be >= 2");
    TEST_ASSERT(telemetry.totalConnectionsAccepted >= 1, "totalConnectionsAccepted must be >= 1");
    TEST_ASSERT(telemetry.totalConnectionsRejected >= 1, "totalConnectionsRejected must be >= 1");
    TEST_ASSERT(telemetry.totalSynchronousRequests >= 1, "totalSynchronousRequests must be >= 1");
    TEST_ASSERT(telemetry.totalRepliesSent >= 1, "totalRepliesSent must be >= 1");
    TEST_ASSERT(telemetry.totalAsynchronousDatagrams >= 1, "totalAsynchronousDatagrams must be >= 1");
    TEST_ASSERT(telemetry.totalLargeViewsCreated >= 1, "totalLargeViewsCreated must be >= 1");
    TEST_ASSERT(telemetry.totalClientImpersonations >= 1, "totalClientImpersonations must be >= 1");
    TEST_ASSERT(telemetry.totalPortsClosed >= 2, "totalPortsClosed must be >= 2");

    lpc::LpcResetTelemetryForTesting();
    auto resetTelem = lpc::LpcQueryTelemetry();
    TEST_ASSERT(resetTelem.totalConnectionPortsCreated == 0, "Telemetry reset must restore counters to 0");

    // Teardown
    portMgr.resetForTesting();

    std::cout << "[TEST] Suite 237: Horizon 21 - Sovereign Executive LPC/ALPC Architecture, 3-Way Handshake & Impersonation Engine PASSED.\n";
}

void Test_Horizon22_ConfigurationManager_HiveRefinement_Coalescing_And_Security() {
    std::cout << "[TEST] Running Suite 238: Horizon 22 - Sovereign Configuration Manager Hive Refinement, Coalescing & Security...\n";

    // 0. Reset and verify baseline telemetry
    cm::CmResetTelemetryForTesting();
    auto initialTelem = cm::CmQueryTelemetry();
    TEST_ASSERT(initialTelem.totalCellsAllocated == 0, "CM telemetry must start clean");

    // 1. Dave Cutler Free Cell Coalescing Engine (CmpCoalesceFreeSpace)
    cm::CmHive hive;
    uint32_t cellA = hive.allocateCell(64);
    uint32_t cellB = hive.allocateCell(64);
    uint32_t cellC = hive.allocateCell(64);
    TEST_ASSERT(cellA != cm::HCELL_NIL && cellB != cm::HCELL_NIL && cellC != cm::HCELL_NIL, "Cells must be allocated");

    // Free cellB in the middle, then cellA adjacent to it
    hive.freeCell(cellB);
    hive.freeCell(cellA);

    // Verify coalescing occurred
    auto postCoalesceTelem = cm::CmQueryTelemetry();
    TEST_ASSERT(postCoalesceTelem.totalCellsFreed >= 2, "totalCellsFreed must be >= 2");
    TEST_ASSERT(postCoalesceTelem.totalCellBytesCoalesced > 0, "totalCellBytesCoalesced must be > 0 (Cutler coalescing)");

    // Allocate 128 bytes - should fit into the coalesced free block
    uint32_t coalescedFit = hive.allocateCell(128);
    TEST_ASSERT(coalescedFit == cellA, "New allocation must reuse the coalesced free block at cellA offset");

    // Free remaining cells
    hive.freeCell(coalescedFit);
    hive.freeCell(cellC);

    // 2. Volatile vs Stable Hive Storage Separation (HCELL_INDEX)
    uint32_t stableCell = hive.allocateCell(128, false);
    TEST_ASSERT(!cm::HCELL_IS_VOLATILE(stableCell), "Stable cell must not have volatile bit set");

    uint32_t volatileCell = hive.allocateCell(128, true);
    TEST_ASSERT(cm::HCELL_IS_VOLATILE(volatileCell), "Volatile cell must have volatile bit 31 set");

    const char* stableData = "STABLE_PERSISTENT_PAYLOAD_DISK";
    const char* volatileData = "VOLATILE_TRANSIENT_MEMORY_ONLY";
    std::memcpy(hive.getCellData(stableCell), stableData, std::strlen(stableData) + 1);
    std::memcpy(hive.getCellData(volatileCell), volatileData, std::strlen(volatileData) + 1);

    TEST_ASSERT(std::string(reinterpret_cast<const char*>(hive.getCellData(stableCell))) == stableData, 
                "Stable cell content must match");
    TEST_ASSERT(std::string(reinterpret_cast<const char*>(hive.getCellData(volatileCell))) == volatileData, 
                "Volatile cell content must match");

    // Serializing hive must omit volatile storage
    std::vector<uint8_t> stableHiveImage = hive.serialize();
    auto loadedHive = cm::CmHive::deserialize(stableHiveImage);
    TEST_ASSERT(loadedHive != nullptr && loadedHive->isConsistent(), "Deserialized hive must be valid and consistent");
    TEST_ASSERT(loadedHive->getVolatileBinsDataSize() == 0, "Loaded hive from disk must have zero volatile bins");

    // 3. Small Value Inline Optimization (VK_DATA_INLINE)
    uint32_t smallVal = 0xDEADBEEF;
    std::vector<uint8_t> smallBytes(sizeof(smallVal));
    std::memcpy(smallBytes.data(), &smallVal, sizeof(smallVal));

    uint32_t smallVkCell = hive.writeVkRecord("SmallDword", cm::RegType::Dword, smallBytes);
    const auto* smallVkHdr = reinterpret_cast<const cm::VkRecordHeader*>(hive.getCellData(smallVkCell));
    TEST_ASSERT((smallVkHdr->dataLength & cm::VK_DATA_INLINE) != 0, 
                "Small value (<= 4 bytes) must set VK_DATA_INLINE bit 31");
    TEST_ASSERT(smallVkHdr->dataOffset == smallVal, 
                "Small value must be stored inline directly in dataOffset field");

    std::string readSmallName;
    cm::KeyValue readSmallVal;
    bool readSmallOk = hive.readVkRecord(smallVkCell, readSmallName, readSmallVal);
    TEST_ASSERT(readSmallOk && readSmallName == "SmallDword", "readVkRecord for inline value must succeed");
    TEST_ASSERT(readSmallVal.asDword() == smallVal, "Inline value content must match original dword");

    // Large value (> 4 bytes) out-of-line storage
    std::vector<uint8_t> largeBytes(64, 0xAA);
    uint32_t largeVkCell = hive.writeVkRecord("LargeBlob", cm::RegType::Binary, largeBytes);
    const auto* largeVkHdr = reinterpret_cast<const cm::VkRecordHeader*>(hive.getCellData(largeVkCell));
    TEST_ASSERT((largeVkHdr->dataLength & cm::VK_DATA_INLINE) == 0, 
                "Large value (> 4 bytes) must not set VK_DATA_INLINE");

    std::string readLargeName;
    cm::KeyValue readLargeVal;
    bool readLargeOk = hive.readVkRecord(largeVkCell, readLargeName, readLargeVal);
    TEST_ASSERT(readLargeOk && readLargeName == "LargeBlob", "readVkRecord for large value must succeed");
    TEST_ASSERT(readLargeVal.data == largeBytes, "Large value data must match original");

    // 4. Subkey Indexing with LH (Hash Leaf)
    std::vector<std::pair<std::string, uint32_t>> subkeys = {
        {"ControlSet001", 0x1000},
        {"ControlSet002", 0x2000},
        {"HardwareConfig", 0x3000}
    };
    uint32_t lhCell = hive.createLhIndex(subkeys);
    TEST_ASSERT(lhCell != cm::HCELL_NIL, "createLhIndex must succeed");

    const auto* lhHdr = reinterpret_cast<const cm::LhRecordHeader*>(hive.getCellData(lhCell));
    TEST_ASSERT(lhHdr->signature == cm::LH_SIGNATURE, "Index signature must be 'lh'");
    TEST_ASSERT(lhHdr->count == 3, "Index count must be 3");

    // 5. Transactional Registry (TxR) Journal Two-Phase Commit & Crash Recovery
    cm::TxRJournal journal;
    journal.beginTransaction(hive);
    TEST_ASSERT(!hive.isConsistent(), "Hive must be inconsistent during open transaction (sequence1 != sequence2)");

    journal.commitTransaction(hive);
    TEST_ASSERT(hive.isConsistent(), "Hive must be consistent after commit (sequence1 == sequence2)");

    // Simulate crash and recovery
    hive.setSequence(5, 4); // Mismatched sequence numbers
    TEST_ASSERT(!hive.isConsistent(), "Mismatched sequence numbers simulate interrupted crash");
    bool recovered = journal.recoverHive(hive);
    TEST_ASSERT(recovered, "recoverHive must succeed on inconsistent hive");
    TEST_ASSERT(hive.isConsistent(), "Hive must be consistent after crash recovery");
    TEST_ASSERT(hive.getHeader().sequence1 == 5 && hive.getHeader().sequence2 == 5, 
                "Sequence numbers must be synchronized to highest commit");

    // 6. Security & Access Checking on Keys
    auto& cm = cm::ConfigurationManager::get();
    cm.resetForTesting();

    auto userSid = se::Sid::authenticatedUsers();
    auto adminSid = se::Sid::administrators();

    se::Acl keyDacl;
    keyDacl.addAllowedAce(adminSid, cm::KEY_ALL_ACCESS);
    keyDacl.addAllowedAce(userSid, cm::KEY_READ);

    se::SecurityDescriptor keySd;
    keySd.setOwner(adminSid);
    keySd.setGroup(adminSid);
    keySd.setDacl(std::move(keyDacl));

    std::shared_ptr<cm::KeyObject> secureKey;
    uint32_t disposition = 0;
    NtStatus createSt = cm.createKeyEx(
        L"\\Registry\\Machine\\SecureSubsystem",
        cm::KEY_ALL_ACCESS,
        cm::REG_OPTION_NON_VOLATILE,
        secureKey,
        &disposition,
        nullptr,
        &keySd
    );
    TEST_ASSERT(NT_SUCCESS(createSt), "createKeyEx for secure key must succeed");
    TEST_ASSERT(disposition == cm::REG_CREATED_NEW_KEY, "Disposition must be REG_CREATED_NEW_KEY");

    auto adminToken = se::TokenObject::createAdminToken(adminSid);
    auto userToken = se::TokenObject::createUserToken(userSid);

    // Admin sets value -> Success
    uint32_t adminVal = 42;
    std::span<const uint8_t> adminValBytes(reinterpret_cast<const uint8_t*>(&adminVal), sizeof(adminVal));
    NtStatus setSt = cm::CmSetValueKey(secureKey, L"AdminParam", cm::RegType::Dword, adminValBytes, adminToken.get());
    TEST_ASSERT(NT_SUCCESS(setSt), "Admin token must be allowed to set value");

    // User sets value -> Access Denied (user only has KEY_READ)
    uint32_t userVal = 99;
    std::span<const uint8_t> userValBytes(reinterpret_cast<const uint8_t*>(&userVal), sizeof(userVal));
    NtStatus denySt = cm::CmSetValueKey(secureKey, L"UserHackedParam", cm::RegType::Dword, userValBytes, userToken.get());
    TEST_ASSERT(denySt == NtStatus::AccessDenied, "User token without KEY_SET_VALUE must receive STATUS_ACCESS_DENIED");

    // User queries value -> Success (user has KEY_READ)
    cm::KeyValue queryVal;
    NtStatus querySt = cm::CmQueryValueKey(secureKey, L"AdminParam", queryVal, userToken.get());
    TEST_ASSERT(NT_SUCCESS(querySt), "User token with KEY_READ must be allowed to query value");
    TEST_ASSERT(queryVal.asDword() == 42, "Queried value must match");

    // 7. Volatile Key Hierarchy
    std::shared_ptr<cm::KeyObject> volatileKey;
    createSt = cm.createKeyEx(
        L"\\Registry\\Machine\\SYSTEM\\VolatileState",
        cm::KEY_ALL_ACCESS,
        cm::REG_OPTION_VOLATILE,
        volatileKey,
        nullptr
    );
    TEST_ASSERT(NT_SUCCESS(createSt), "Creating volatile key must succeed");
    TEST_ASSERT(volatileKey->isVolatile(), "Key created with REG_OPTION_VOLATILE must report isVolatile() == true");

    auto volatileSubkey = volatileKey->createSubkey(L"SessionData");
    TEST_ASSERT(volatileSubkey->isVolatile(), "Subkey of volatile key must inherit volatile flag");

    // 8. Symbolic Link Keys & Loop Prevention
    auto linkKey = cm.createSymbolicLinkKey(
        L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet",
        L"\\Registry\\Machine\\SYSTEM\\ControlSet001"
    );
    TEST_ASSERT(linkKey != nullptr, "createSymbolicLinkKey must succeed");
    TEST_ASSERT(linkKey->isSymlink(), "Symlink key must report isSymlink() == true");

    // Create target key with value
    auto ccs001 = cm.resolvePath(L"\\Registry\\Machine\\SYSTEM\\ControlSet001");
    if (!ccs001) {
        auto sysKey = cm.resolvePath(L"\\Registry\\Machine\\SYSTEM");
        ccs001 = sysKey->createSubkey(L"ControlSet001");
    }
    ccs001->setValueString(L"ComputerName", L"MICANT-STATION-01");

    // Resolve path through symlink
    auto resolvedViaLink = cm.resolvePath(L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet");
    TEST_ASSERT(resolvedViaLink != nullptr, "Path through symlink must resolve to target key");
    const auto* compVal = resolvedViaLink->getValue(L"ComputerName");
    TEST_ASSERT(compVal != nullptr && compVal->asString() == L"MICANT-STATION-01", 
                "Value queried through symlink must match target key value");

    // Test Circular Symbolic Link Loop Detection
    cm.createSymbolicLinkKey(L"\\Registry\\Machine\\LoopA", L"\\Registry\\Machine\\LoopB");
    cm.createSymbolicLinkKey(L"\\Registry\\Machine\\LoopB", L"\\Registry\\Machine\\LoopA");

    NtStatus loopStatus = NtStatus::Success;
    auto loopKey = cm.resolvePath(L"\\Registry\\Machine\\LoopA", true, nullptr, cm::KEY_READ, &loopStatus);
    TEST_ASSERT(loopKey == nullptr, "Circular symlink must return nullptr");
    TEST_ASSERT(loopStatus == NtStatus::CircularReference, "Circular symlink must report STATUS_CIRCULAR_REFERENCE");

    // 9. Telemetry Verification & Teardown
    auto telemetry = cm::CmQueryTelemetry();
    TEST_ASSERT(telemetry.totalCellsAllocated >= 4, "totalCellsAllocated must be >= 4");
    TEST_ASSERT(telemetry.totalCellsFreed >= 2, "totalCellsFreed must be >= 2");
    TEST_ASSERT(telemetry.totalCellBytesCoalesced > 0, "totalCellBytesCoalesced must be > 0");
    TEST_ASSERT(telemetry.totalKeysCreated >= 3, "totalKeysCreated must be >= 3");
    TEST_ASSERT(telemetry.totalVolatileKeysCreated >= 1, "totalVolatileKeysCreated must be >= 1");
    TEST_ASSERT(telemetry.totalTransactionsCommitted >= 1, "totalTransactionsCommitted must be >= 1");
    TEST_ASSERT(telemetry.totalRecoveriesPerformed >= 1, "totalRecoveriesPerformed must be >= 1");
    TEST_ASSERT(telemetry.totalSecurityChecksPassed >= 2, "totalSecurityChecksPassed must be >= 2");
    TEST_ASSERT(telemetry.totalSecurityChecksDenied >= 1, "totalSecurityChecksDenied must be >= 1");
    TEST_ASSERT(telemetry.totalSymlinksResolved >= 1, "totalSymlinksResolved must be >= 1");

    cm::CmResetTelemetryForTesting();
    auto resetTelem = cm::CmQueryTelemetry();
    TEST_ASSERT(resetTelem.totalCellsAllocated == 0, "Telemetry reset must clear all counters to 0");

    cm.resetForTesting();

    std::cout << "[TEST] Suite 238: Horizon 22 - Sovereign Configuration Manager Hive Refinement, Coalescing & Security PASSED.\n";
}

// ============================================================================
// Suite 239: Horizon 23 - Sovereign I/O Subsystem FastIo, Buffering & Cancellation
// ============================================================================
void Test_Horizon23_IoSubsystem_FastIo_Cancellation_And_BufferingModes() {
    std::cout << "[TEST] Suite 239: Horizon 23 - Sovereign I/O Subsystem FastIo, Buffering & Cancellation...\n";

    // 0. Reset Telemetry for pristine baseline
    io::IoResetTelemetryForTesting();

    // 1. Buffering Modes: DO_BUFFERED_IO (Double Buffering)
    {
        io::DriverObject drvBuffered;
        io::DeviceObject devBuffered;
        devBuffered.flags = io::DO_BUFFERED_IO;
        devBuffered.stackSize = 1;
        devBuffered.driverObject = &drvBuffered;

        // Synchronous Write: Copies user buffer into kernel systemBuffer
        char writePayload[] = "MicaNT Dave Cutler DO_BUFFERED_IO Kernel Payload";
        uint32_t writeLen = static_cast<uint32_t>(sizeof(writePayload));
        LargeInteger writeOffset{.quadPart = 0x4000};

        io::Irp* writeIrp = io::IoBuildSynchronousFsdRequest(
            io::IRP_MJ_WRITE,
            &devBuffered,
            writePayload,
            writeLen,
            &writeOffset
        );
        TEST_ASSERT(writeIrp != nullptr, "IoBuildSynchronousFsdRequest WRITE must succeed");
        TEST_ASSERT(writeIrp->systemBuffer != nullptr, "DO_BUFFERED_IO WRITE must allocate intermediate systemBuffer");
        TEST_ASSERT(std::memcmp(writeIrp->systemBuffer, writePayload, writeLen) == 0, "systemBuffer must contain payload");
        TEST_ASSERT(writeIrp->userBuffer == writePayload, "userBuffer must point to caller buffer");

        writeIrp->ioStatus.status = NtStatus::Success;
        writeIrp->ioStatus.information = writeLen;
        NtStatus compSt = io::IoCompleteRequest(writeIrp);
        TEST_ASSERT(NT_SUCCESS(compSt), "IoCompleteRequest WRITE must succeed");
        TEST_ASSERT(writeIrp->systemBuffer == nullptr, "systemBuffer must be safely freed and nulled on completion");
        io::IoFreeIrp(writeIrp);

        // Synchronous Read: Allocates kernel systemBuffer, on completion copies back to userBuffer
        char readBuf[64]{0};
        uint32_t readLen = 32;
        LargeInteger readOffset{.quadPart = 0x8000};

        io::Irp* readIrp = io::IoBuildSynchronousFsdRequest(
            io::IRP_MJ_READ,
            &devBuffered,
            readBuf,
            readLen,
            &readOffset
        );
        TEST_ASSERT(readIrp != nullptr, "IoBuildSynchronousFsdRequest READ must succeed");
        TEST_ASSERT(readIrp->systemBuffer != nullptr, "DO_BUFFERED_IO READ must allocate systemBuffer");

        // Driver produces data in systemBuffer
        const char* mockDiskData = "MicaNT Fast Read Back Data 0123";
        std::memcpy(readIrp->systemBuffer, mockDiskData, 32);
        readIrp->ioStatus.status = NtStatus::Success;
        readIrp->ioStatus.information = 32;

        compSt = io::IoCompleteRequest(readIrp);
        TEST_ASSERT(NT_SUCCESS(compSt), "IoCompleteRequest READ must succeed");
        TEST_ASSERT(std::memcmp(readBuf, mockDiskData, 32) == 0, "DO_BUFFERED_IO completion must copy data into userBuffer");
        TEST_ASSERT(readIrp->systemBuffer == nullptr, "systemBuffer must be freed on completion");
        io::IoFreeIrp(readIrp);

        // Device Control (IOCTL): METHOD_BUFFERED request
        char ioctlInput[] = "MicaNT_IOCTL_QUERY";
        char ioctlOutput[32]{0};
        uint32_t ioctlCode = 0x00222004;

        io::Irp* ctlIrp = io::IoBuildDeviceIoControlRequest(
            ioctlCode,
            &devBuffered,
            ioctlInput,
            static_cast<uint32_t>(sizeof(ioctlInput)),
            ioctlOutput,
            sizeof(ioctlOutput),
            false
        );
        TEST_ASSERT(ctlIrp != nullptr, "IoBuildDeviceIoControlRequest must succeed");
        TEST_ASSERT(ctlIrp->systemBuffer != nullptr, "DO_BUFFERED_IO IOCTL must allocate systemBuffer");
        TEST_ASSERT(std::memcmp(ctlIrp->systemBuffer, ioctlInput, sizeof(ioctlInput)) == 0, "IOCTL input must be in systemBuffer");

        // Driver processes request and writes response into systemBuffer
        const char* ioctlResponse = "STATUS_QUERY_ACKNOWLEDGED";
        std::memcpy(ctlIrp->systemBuffer, ioctlResponse, 26);
        ctlIrp->ioStatus.status = NtStatus::Success;
        ctlIrp->ioStatus.information = 26;

        compSt = io::IoCompleteRequest(ctlIrp);
        TEST_ASSERT(NT_SUCCESS(compSt), "IoCompleteRequest IOCTL must succeed");
        TEST_ASSERT(std::memcmp(ioctlOutput, ioctlResponse, 26) == 0, "IOCTL response must be copied back to userBuffer");
        TEST_ASSERT(ctlIrp->systemBuffer == nullptr, "IOCTL systemBuffer must be freed on completion");
        io::IoFreeIrp(ctlIrp);
    }

    // 2. Buffering Modes: DO_DIRECT_IO (MDL Zero-Copy Direct DMA Model)
    {
        io::DriverObject drvDirect;
        io::DeviceObject devDirect;
        devDirect.flags = io::DO_DIRECT_IO;
        devDirect.stackSize = 1;
        devDirect.driverObject = &drvDirect;

        alignas(4096) char directData[4096];
        std::memset(directData, 0x5A, sizeof(directData));
        LargeInteger directOffset{.quadPart = 0x10000};

        io::Irp* directIrp = io::IoBuildSynchronousFsdRequest(
            io::IRP_MJ_READ,
            &devDirect,
            directData,
            sizeof(directData),
            &directOffset
        );
        TEST_ASSERT(directIrp != nullptr, "IoBuildSynchronousFsdRequest DIRECT_IO must succeed");
        TEST_ASSERT(directIrp->systemBuffer == nullptr, "DO_DIRECT_IO must not allocate systemBuffer");
        TEST_ASSERT(directIrp->mdlAddress != nullptr, "DO_DIRECT_IO must allocate and probe/lock MDL");

        auto* mdl = reinterpret_cast<mm::PMDL>(directIrp->mdlAddress);
        TEST_ASSERT(mdl->byteCount == sizeof(directData), "MDL byte count must match requested length");
        TEST_ASSERT((mdl->mdlFlags & mm::MDL_PAGES_LOCKED) != 0, "MDL must have MDL_PAGES_LOCKED flag set");

        directIrp->ioStatus.status = NtStatus::Success;
        directIrp->ioStatus.information = sizeof(directData);
        NtStatus compSt = io::IoCompleteRequest(directIrp);
        TEST_ASSERT(NT_SUCCESS(compSt), "IoCompleteRequest DIRECT_IO must succeed");
        TEST_ASSERT(directIrp->mdlAddress == nullptr, "directIrp mdlAddress must be unlocked and freed on completion");
        io::IoFreeIrp(directIrp);
    }

    // 3. Buffering Modes: Neither I/O (Raw User Buffer Pass-Through)
    {
        io::DriverObject drvNeither;
        io::DeviceObject devNeither;
        devNeither.flags = 0; // neither buffered nor direct
        devNeither.stackSize = 1;
        devNeither.driverObject = &drvNeither;

        char neitherBuf[256];
        std::memset(neitherBuf, 0xCC, sizeof(neitherBuf));

        io::Irp* neitherIrp = io::IoBuildSynchronousFsdRequest(
            io::IRP_MJ_WRITE,
            &devNeither,
            neitherBuf,
            sizeof(neitherBuf),
            nullptr
        );
        TEST_ASSERT(neitherIrp != nullptr, "Neither I/O IRP must be allocated");
        TEST_ASSERT(neitherIrp->systemBuffer == nullptr, "Neither I/O must not allocate systemBuffer");
        TEST_ASSERT(neitherIrp->mdlAddress == nullptr, "Neither I/O must not allocate MDL");
        TEST_ASSERT(neitherIrp->userBuffer == neitherBuf, "Neither I/O userBuffer must point directly to user buffer");

        neitherIrp->ioStatus.status = NtStatus::Success;
        neitherIrp->ioStatus.information = sizeof(neitherBuf);
        NtStatus compSt = io::IoCompleteRequest(neitherIrp);
        TEST_ASSERT(NT_SUCCESS(compSt), "IoCompleteRequest Neither I/O must succeed");
        io::IoFreeIrp(neitherIrp);
    }

    // 4. Fast I/O Dispatch Table & High-Throughput Bypass Architecture
    {
        static bool s_FastReadInvoked = false;
        static bool s_FastWriteInvoked = false;
        static bool s_FastDeviceControlInvoked = false;
        static bool s_FastQueryBasicInvoked = false;
        static bool s_FastQueryStdInvoked = false;

        io::FastIoDispatch fastTable{};
        fastTable.fastIoRead = [](void*, LargeInteger*, uint32_t length, bool, void* buffer, IoStatusBlock* ioStatus, io::DeviceObject*) -> bool {
            s_FastReadInvoked = true;
            if (buffer && length >= 8) {
                std::memcpy(buffer, "FASTDATA", 8);
            }
            ioStatus->status = NtStatus::Success;
            ioStatus->information = length;
            return true;
        };

        fastTable.fastIoWrite = [](void*, LargeInteger*, uint32_t length, bool, const void*, IoStatusBlock* ioStatus, io::DeviceObject*) -> bool {
            s_FastWriteInvoked = true;
            ioStatus->status = NtStatus::Success;
            ioStatus->information = length;
            return true;
        };

        fastTable.fastIoDeviceControl = [](void*, bool, void*, uint32_t, void* outBuf, uint32_t outLen, uint32_t, IoStatusBlock* ioStatus, io::DeviceObject*) -> bool {
            s_FastDeviceControlInvoked = true;
            if (outBuf && outLen >= 4) {
                std::memcpy(outBuf, "FCTL", 4);
            }
            ioStatus->status = NtStatus::Success;
            ioStatus->information = outLen;
            return true;
        };

        fastTable.fastIoQueryBasicInfo = [](void*, bool, void*, IoStatusBlock* ioStatus, io::DeviceObject*) -> bool {
            s_FastQueryBasicInvoked = true;
            ioStatus->status = NtStatus::Success;
            return true;
        };

        fastTable.fastIoQueryStandardInfo = [](void*, bool, void*, IoStatusBlock* ioStatus, io::DeviceObject*) -> bool {
            s_FastQueryStdInvoked = true;
            ioStatus->status = NtStatus::Success;
            return true;
        };

        io::DriverObject fastDrv;
        fastDrv.fastIoDispatch = &fastTable;
        io::DeviceObject fastDev;
        fastDev.driverObject = &fastDrv;

        char fBuf[32]{0};
        IoStatusBlock fStatus{};
        LargeInteger fOffset{.quadPart = 0};

        // Test Fast Read
        bool fr = io::IoFastRead(&fastDev, nullptr, &fOffset, 8, true, fBuf, &fStatus);
        TEST_ASSERT(fr == true, "IoFastRead must return true on fast path");
        TEST_ASSERT(s_FastReadInvoked == true, "fastIoRead handler must be invoked");
        TEST_ASSERT(std::memcmp(fBuf, "FASTDATA", 8) == 0, "Fast read buffer must match payload");
        TEST_ASSERT(fStatus.status == NtStatus::Success, "Fast read status must be Success");

        // Test Fast Write
        bool fw = io::IoFastWrite(&fastDev, nullptr, &fOffset, 16, true, "0123456789ABCDEF", &fStatus);
        TEST_ASSERT(fw == true, "IoFastWrite must return true on fast path");
        TEST_ASSERT(s_FastWriteInvoked == true, "fastIoWrite handler must be invoked");
        TEST_ASSERT(fStatus.status == NtStatus::Success, "Fast write status must be Success");

        // Test Fast Device Control
        char ctlOut[8]{0};
        bool fc = io::IoFastDeviceControl(&fastDev, nullptr, true, nullptr, 0, ctlOut, 4, 0x101, &fStatus);
        TEST_ASSERT(fc == true, "IoFastDeviceControl must return true on fast path");
        TEST_ASSERT(s_FastDeviceControlInvoked == true, "fastIoDeviceControl handler must be invoked");
        TEST_ASSERT(std::memcmp(ctlOut, "FCTL", 4) == 0, "Fast IOCTL out buffer must match");

        // Test Fast Query Info
        bool fqb = io::IoFastQueryBasicInfo(&fastDev, nullptr, true, nullptr, &fStatus);
        TEST_ASSERT(fqb == true, "IoFastQueryBasicInfo must return true");
        TEST_ASSERT(s_FastQueryBasicInvoked == true, "fastIoQueryBasicInfo handler must be invoked");

        bool fqs = io::IoFastQueryStandardInfo(&fastDev, nullptr, true, nullptr, &fStatus);
        TEST_ASSERT(fqs == true, "IoFastQueryStandardInfo must return true");
        TEST_ASSERT(s_FastQueryStdInvoked == true, "fastIoQueryStandardInfo handler must be invoked");

        // Test Fast I/O Bypass / Fallback
        fastTable.fastIoRead = [](void*, LargeInteger*, uint32_t, bool, void*, IoStatusBlock*, io::DeviceObject*) -> bool {
            return false; // Cache miss or range lock conflict - signals fallback to standard IRP
        };
        bool frBypass = io::IoFastRead(&fastDev, nullptr, &fOffset, 8, true, fBuf, &fStatus);
        TEST_ASSERT(frBypass == false, "IoFastRead returning false must signal fallback to standard IRP dispatch");

        // Test Fast I/O with device having no FastIoDispatch
        io::DriverObject noFastDrv;
        io::DeviceObject noFastDev;
        noFastDev.driverObject = &noFastDrv;
        bool frNoFast = io::IoFastRead(&noFastDev, nullptr, &fOffset, 8, true, fBuf, &fStatus);
        TEST_ASSERT(frNoFast == false, "Device without FastIoDispatch must return false");
    }

    // 5. Safe IRP Cancellation Race Condition Prevention Engine
    {
        static bool s_CancelHandlerInvoked = false;
        static io::DeviceObject* s_InvokedDev = nullptr;
        static io::Irp* s_InvokedIrp = nullptr;

        auto cancelHandler = [](io::DeviceObject* dev, io::Irp* irp) {
            s_CancelHandlerInvoked = true;
            s_InvokedDev = dev;
            s_InvokedIrp = irp;
        };

        io::DeviceObject testDev;
        testDev.stackSize = 1;

        io::Irp* irpToCancel = io::IoAllocateIrp(1);
        irpToCancel->deviceObject = &testDev;

        io::PDRIVER_CANCEL prevRoutine = io::IoSetCancelRoutine(irpToCancel, cancelHandler);
        TEST_ASSERT(prevRoutine == nullptr, "Previous cancel routine must be nullptr");
        TEST_ASSERT(irpToCancel->cancelRoutine == cancelHandler, "Cancel routine must be set");

        bool cancelRet = io::IoCancelIrp(irpToCancel);
        TEST_ASSERT(cancelRet == true, "IoCancelIrp must return true when cancel routine was executed");
        TEST_ASSERT(s_CancelHandlerInvoked == true, "Cancel routine must have executed");
        TEST_ASSERT(s_InvokedDev == &testDev, "Cancel handler device must match");
        TEST_ASSERT(s_InvokedIrp == irpToCancel, "Cancel handler IRP must match");
        TEST_ASSERT(irpToCancel->cancel == true, "irp->cancel flag must be true");
        TEST_ASSERT(irpToCancel->cancelRoutine == nullptr, "cancelRoutine must be cleared to nullptr");

        // Race Condition Simulation:
        // Scenario: Driver dequeues IRP, attempts to clear cancel routine under cancel spinlock.
        io::Irp* raceIrp = io::IoAllocateIrp(1);
        raceIrp->deviceObject = &testDev;
        io::IoSetCancelRoutine(raceIrp, cancelHandler);

        // Cancellation runs first (e.g. timeout thread or user cancel)
        io::IoCancelIrp(raceIrp);

        // Driver dequeues IRP and tries to clear cancel routine
        ke::KIRQL oldIrql = ke::PASSIVE_LEVEL;
        io::IoAcquireCancelSpinLock(&oldIrql);
        io::PDRIVER_CANCEL dpcClearedRoutine = io::IoSetCancelRoutine(raceIrp, nullptr);
        if (dpcClearedRoutine == nullptr) {
            // Driver detects cancellation race condition: cancel routine already took ownership!
            io::IoReleaseCancelSpinLock(oldIrql);
            TEST_ASSERT(raceIrp->cancel == true, "Race IRP must have cancel flag set to true");
            // Driver does NOT call IoCompleteRequest; cancel routine owns termination.
        } else {
            io::IoReleaseCancelSpinLock(oldIrql);
            TEST_ASSERT(false, "Cancel routine should already have been cleared by IoCancelIrp");
        }

        io::IoFreeIrp(irpToCancel);
        io::IoFreeIrp(raceIrp);
    }

    // 6. Device Queue Serialization (IoStartPacket & IoStartNextPacket)
    {
        static std::vector<io::Irp*> s_ExecutedPackets;
        s_ExecutedPackets.clear();

        io::DriverObject queueDrv;
        queueDrv.driverStartIo = [](io::DeviceObject*, io::Irp* irp) {
            s_ExecutedPackets.push_back(irp);
        };

        io::DeviceObject queueDev;
        queueDev.driverObject = &queueDrv;
        queueDev.stackSize = 1;

        io::Irp* p1 = io::IoAllocateIrp(1);
        io::Irp* p2 = io::IoAllocateIrp(1);
        io::Irp* p3 = io::IoAllocateIrp(1);

        // 1st Packet: Device is idle, starts immediately
        io::IoStartPacket(&queueDev, p1);
        TEST_ASSERT(queueDev.deviceQueue.busy == true, "Device queue must become busy");
        TEST_ASSERT(queueDev.currentIrp == p1, "currentIrp must be set to p1");
        TEST_ASSERT(s_ExecutedPackets.size() == 1 && s_ExecutedPackets[0] == p1, "p1 must be dispatched to driverStartIo");

        // 2nd Packet: Device is busy, should be queued
        io::IoStartPacket(&queueDev, p2);
        TEST_ASSERT(queueDev.deviceQueue.queue.size() == 1, "p2 must be placed in device queue");
        TEST_ASSERT(s_ExecutedPackets.size() == 1, "p2 must not yet be dispatched");

        // 3rd Packet: Device is busy, should be queued
        io::IoStartPacket(&queueDev, p3);
        TEST_ASSERT(queueDev.deviceQueue.queue.size() == 2, "p3 must be placed in device queue");
        TEST_ASSERT(s_ExecutedPackets.size() == 1, "p3 must not yet be dispatched");

        // Driver completes p1 and starts next packet
        io::IoStartNextPacket(&queueDev);
        TEST_ASSERT(queueDev.deviceQueue.busy == true, "Queue must remain busy with remaining packets");
        TEST_ASSERT(queueDev.currentIrp == p2, "currentIrp must advance to p2");
        TEST_ASSERT(queueDev.deviceQueue.queue.size() == 1, "Queue size must decrease to 1");
        TEST_ASSERT(s_ExecutedPackets.size() == 2 && s_ExecutedPackets[1] == p2, "p2 must be dispatched to driverStartIo");

        // Driver completes p2 and starts next packet
        io::IoStartNextPacket(&queueDev);
        TEST_ASSERT(queueDev.currentIrp == p3, "currentIrp must advance to p3");
        TEST_ASSERT(queueDev.deviceQueue.queue.empty(), "Queue must now be empty");
        TEST_ASSERT(s_ExecutedPackets.size() == 3 && s_ExecutedPackets[2] == p3, "p3 must be dispatched to driverStartIo");

        // Driver completes p3 and starts next packet on empty queue
        io::IoStartNextPacket(&queueDev);
        TEST_ASSERT(queueDev.deviceQueue.busy == false, "Device queue must be idle when empty");
        TEST_ASSERT(queueDev.currentIrp == nullptr, "currentIrp must be nullptr when idle");

        io::IoFreeIrp(p1);
        io::IoFreeIrp(p2);
        io::IoFreeIrp(p3);
    }

    // 7. Telemetry Verification & Teardown
    {
        auto telem = io::IoQueryTelemetry();
        TEST_ASSERT(telem.totalIrpsAllocated >= 6, "totalIrpsAllocated must be >= 6");
        TEST_ASSERT(telem.totalIrpsFreed >= 5, "totalIrpsFreed must be >= 5");
        TEST_ASSERT(telem.totalIrpsCompleted >= 3, "totalIrpsCompleted must be >= 3");
        TEST_ASSERT(telem.totalBufferedIoBytes > 0, "totalBufferedIoBytes must be > 0");
        TEST_ASSERT(telem.totalDirectIoBytes > 0, "totalDirectIoBytes must be > 0");
        TEST_ASSERT(telem.totalNeitherIoBytes > 0, "totalNeitherIoBytes must be > 0");
        TEST_ASSERT(telem.totalFastIoReads >= 2, "totalFastIoReads must be >= 2");
        TEST_ASSERT(telem.totalFastIoWrites >= 1, "totalFastIoWrites must be >= 1");
        TEST_ASSERT(telem.totalFastIoDeviceControls >= 1, "totalFastIoDeviceControls must be >= 1");
        TEST_ASSERT(telem.totalFastIoSuccesses >= 3, "totalFastIoSuccesses must be >= 3");
        TEST_ASSERT(telem.totalFastIoBypasses >= 1, "totalFastIoBypasses must be >= 1");
        TEST_ASSERT(telem.totalIrpsCancelled >= 2, "totalIrpsCancelled must be >= 2");
        TEST_ASSERT(telem.totalCancelRoutinesInvoked >= 2, "totalCancelRoutinesInvoked must be >= 2");
        TEST_ASSERT(telem.totalDeviceQueuePacketsStarted >= 3, "totalDeviceQueuePacketsStarted must be >= 3");
        TEST_ASSERT(telem.totalDeviceQueuePacketsQueued >= 2, "totalDeviceQueuePacketsQueued must be >= 2");

        io::IoResetTelemetryForTesting();
        auto telemReset = io::IoQueryTelemetry();
        TEST_ASSERT(telemReset.totalIrpsAllocated == 0, "IoResetTelemetryForTesting must zero out all counters");
    }

    std::cout << "[TEST] Suite 239: Horizon 23 - Sovereign I/O Subsystem FastIo, Buffering & Cancellation PASSED.\n";
}

// ============================================================================
// Suite 240: Horizon 24 - Sovereign Process Subsystem Quotas, Jobs & Callbacks
// ============================================================================
void Test_Horizon24_ProcessSubsystem_Quotas_JobObjects_Callbacks_And_Affinity() {
    std::cout << "[TEST] Suite 240: Horizon 24 - Sovereign Process Subsystem Quotas, Jobs & Callbacks...\n" << std::flush;

    auto& pm = ps::ProcessManager::get();
    pm.resetForTesting();
    ps::PsResetTelemetryForTesting();

    // 1. Process & Thread Quotas (ProcessQuotaBlock)
    {
        auto proc = pm.createProcess(L"QuotaService.exe");
        TEST_ASSERT(proc != nullptr, "Process creation must succeed");

        auto& qb = proc->getQuotaBlock();
        qb.pagedPoolLimit = 64 * 1024;      // 64 KB limit
        qb.nonPagedPoolLimit = 32 * 1024;   // 32 KB limit
        qb.pagefileLimit = 128 * 1024;      // 128 KB limit

        // Charge within quota
        NtStatus st1 = qb.chargePagedPool(16 * 1024);
        TEST_ASSERT(NT_SUCCESS(st1), "Charging 16KB paged pool under 64KB limit must succeed");
        TEST_ASSERT(qb.pagedPoolUsage == 16 * 1024, "Paged pool usage must be 16KB");

        // Exceed quota
        NtStatus st2 = qb.chargePagedPool(60 * 1024); // 16KB + 60KB = 76KB > 64KB
        TEST_ASSERT(st2 == NtStatus::QuotaExceeded, "Exceeding paged pool limit must return STATUS_QUOTA_EXCEEDED");
        TEST_ASSERT(qb.pagedPoolUsage == 16 * 1024, "Failed charge must not increment usage");

        // Return quota
        qb.returnPagedPool(16 * 1024);
        TEST_ASSERT(qb.pagedPoolUsage == 0, "Returned paged pool must reduce usage to 0");

        // Charge pagefile exceeding limit
        NtStatus st3 = qb.chargePagefile(200 * 1024); // 200KB > 128KB
        TEST_ASSERT(st3 == NtStatus::PagefileQuotaExceeded, "Exceeding pagefile limit must return STATUS_PAGEFILE_QUOTA_EXCEEDED");

        proc->terminate(NtStatus::Success);
    }

    // 2. Canonical Windows NT Job Objects (JobObject / EJOB)
    {
        auto job = std::make_shared<ps::JobObject>(L"ProductionWorkerJob");
        TEST_ASSERT(job->getName() == L"ProductionWorkerJob", "Job name must match");

        ps::JobBasicLimitInformation limits{};
        limits.limitFlags = ps::JOB_OBJECT_LIMIT_ACTIVE_PROCESS | ps::JOB_OBJECT_LIMIT_AFFINITY;
        limits.activeProcessLimit = 2; // Allow maximum 2 concurrent processes
        limits.affinity = 0x00000003;  // Cores 0 and 1
        job->setLimits(limits);

        auto worker1 = pm.createProcess(L"Worker1.exe");
        auto worker2 = pm.createProcess(L"Worker2.exe");
        auto worker3 = pm.createProcess(L"Worker3.exe");

        auto w1Thread = worker1->createThread(0x401000);
        auto w2Thread = worker2->createThread(0x402000);

        // Assign Worker 1
        NtStatus assignSt1 = job->assignProcess(worker1);
        TEST_ASSERT(NT_SUCCESS(assignSt1), "Assigning worker1 to job must succeed");
        TEST_ASSERT(worker1->getJob() == job, "worker1 must reference assigned job");
        TEST_ASSERT(worker1->getAffinityMask() == 0x03, "worker1 must inherit job affinity mask 0x03");
        TEST_ASSERT(w1Thread->getAffinityMask() == 0x03, "worker1 threads must inherit job affinity mask 0x03");
        TEST_ASSERT(job->getAccounting().activeProcesses == 1, "Job activeProcesses must be 1");

        // Assign Worker 2
        NtStatus assignSt2 = job->assignProcess(worker2);
        TEST_ASSERT(NT_SUCCESS(assignSt2), "Assigning worker2 to job must succeed");
        TEST_ASSERT(job->getAccounting().activeProcesses == 2, "Job activeProcesses must be 2");

        // Assign Worker 3 (Should fail due to activeProcessLimit = 2)
        NtStatus assignSt3 = job->assignProcess(worker3);
        TEST_ASSERT(assignSt3 == NtStatus::QuotaExceeded, "Assigning worker3 beyond activeProcessLimit must return QuotaExceeded");

        // Terminate Job
        NtStatus termSt = job->terminateJob(NtStatus::ProcessAborted);
        TEST_ASSERT(NT_SUCCESS(termSt), "Terminating job must succeed");
        TEST_ASSERT(worker1->isTerminated(), "worker1 must be terminated by job termination");
        TEST_ASSERT(worker2->isTerminated(), "worker2 must be terminated by job termination");
        TEST_ASSERT(worker1->getExitStatus() == NtStatus::ProcessAborted, "Exit status must be STATUS_PROCESS_ABORTED");
        TEST_ASSERT(job->getAccounting().activeProcesses == 0, "Job activeProcesses must drop to 0");
        TEST_ASSERT(job->getAccounting().totalTerminatedProcesses == 2, "Job totalTerminatedProcesses must be 2");

        worker3->terminate(NtStatus::Success);
    }

    // 3. Process & Thread Creation / Exit Notify Callbacks
    {
        static std::vector<std::pair<Handle, bool>> s_ProcessEvents;
        static std::vector<std::pair<Handle, bool>> s_ThreadEvents;
        s_ProcessEvents.clear();
        s_ThreadEvents.clear();

        auto procCallback = [](Handle /*parent*/, Handle pid, bool create) {
            s_ProcessEvents.push_back({pid, create});
        };

        auto threadCallback = [](Handle /*pid*/, Handle tid, bool create) {
            s_ThreadEvents.push_back({tid, create});
        };

        auto procCallbackEx = [](ps::EProcess*, Handle, ps::PS_CREATE_NOTIFY_INFO* info) {
            if (info && info->imageFileName == L"MalwareBlocked.exe") {
                info->creationStatus = NtStatus::AccessDenied; // Veto process creation
            }
        };

        ps::PsSetCreateProcessNotifyRoutine(procCallback);
        ps::PsSetCreateThreadNotifyRoutine(threadCallback);
        ps::PsSetCreateProcessNotifyRoutineEx(procCallbackEx);

        // Test normal process creation
        std::shared_ptr<ps::EProcess> safeProc;
        NtStatus safeSt = ps::PsCreateProcessEx(safeProc, L"SafeService.exe");
        TEST_ASSERT(NT_SUCCESS(safeSt), "PsCreateProcessEx SafeService.exe must succeed");
        TEST_ASSERT(safeProc != nullptr, "Created process pointer must not be null");
        TEST_ASSERT(!s_ProcessEvents.empty(), "Process callback must record create event");
        TEST_ASSERT(s_ProcessEvents.back().first == safeProc->getPid() && s_ProcessEvents.back().second == true, 
                    "Callback must receive (PID, create=true)");

        // Test thread creation inside SafeService.exe
        auto safeThread1 = safeProc->createThread(0x500000);
        auto safeThread2 = safeProc->createThread(0x510000);
        TEST_ASSERT(safeThread1 != nullptr && safeThread2 != nullptr, "Thread creations must succeed");
        TEST_ASSERT(s_ThreadEvents.size() == 2, "Thread callback must record both create events");
        TEST_ASSERT(s_ThreadEvents[0].first == safeThread1->getTid() && s_ThreadEvents[0].second == true,
                    "Callback must receive (TID1, create=true)");
        TEST_ASSERT(s_ThreadEvents[1].first == safeThread2->getTid() && s_ThreadEvents[1].second == true,
                    "Callback must receive (TID2, create=true)");

        // Test thread and process termination notifications
        safeProc->terminate(NtStatus::Success);
        TEST_ASSERT(s_ThreadEvents.size() == 4, "Thread callbacks must record exit events for both threads");
        TEST_ASSERT(s_ThreadEvents[2].second == false && s_ThreadEvents[3].second == false,
                    "Thread exit callbacks must receive create=false");
        TEST_ASSERT(s_ProcessEvents.back().second == false, "Process exit callback must receive create=false");

        // Test Veto of MalwareBlocked.exe by registered Ex callback
        std::shared_ptr<ps::EProcess> blockedProc;
        NtStatus blockedSt = ps::PsCreateProcessEx(blockedProc, L"MalwareBlocked.exe");
        TEST_ASSERT(blockedSt == NtStatus::AccessDenied, "MalwareBlocked.exe creation must be denied by Ex callback");
        TEST_ASSERT(blockedProc == nullptr, "Blocked process pointer must be null");

        // Clean up callbacks
        ps::PsSetCreateProcessNotifyRoutine(procCallback, true);
        ps::PsSetCreateThreadNotifyRoutine(threadCallback, true);
        ps::PsSetCreateProcessNotifyRoutineEx(procCallbackEx, true);
    }

    // 4. Thread Priority Boosting, Quantum Decay, CPU Affinity & Suspend/Resume
    {
        auto testProc = pm.createProcess(L"ThreadPriorityTest.exe");
        auto dynamicThread = testProc->createThread(0x600000);
        dynamicThread->setBasePriority(8);
        TEST_ASSERT(dynamicThread->getPriority() == 8, "Initial priority must match base 8");
        TEST_ASSERT(dynamicThread->getDynamicPriority() == 8, "Dynamic priority must match base 8");

        // Apply Keyboard I/O Boost (+6)
        dynamicThread->boostPriority(ps::IO_KEYBOARD_INCREMENT);
        TEST_ASSERT(dynamicThread->getDynamicPriority() == 14, "Boost (+6) on base 8 must result in priority 14");
        TEST_ASSERT(dynamicThread->getPriority() == 14, "Effective priority must reflect dynamic priority 14");

        // Apply another Boost (+4), should be clamped at 15 for non-realtime
        dynamicThread->boostPriority(4);
        TEST_ASSERT(dynamicThread->getDynamicPriority() == 15, "Dynamic boost must cap at 15 (LOW_REALTIME_PRIORITY - 1)");

        // Test quantum decay over 18 ticks
        for (int i = 0; i < 18; ++i) {
            dynamicThread->decayQuantum();
        }
        TEST_ASSERT(dynamicThread->getDynamicPriority() == 14, "Exhausting quantum tick cycle must decay dynamic priority by 1");

        // Real-Time thread behavior
        auto rtThread = testProc->createThread(0x700000);
        rtThread->setBasePriority(24); // Real-time priority
        rtThread->boostPriority(ps::IO_MOUSE_INCREMENT);
        TEST_ASSERT(rtThread->getDynamicPriority() == 24, "Real-time thread must NOT receive dynamic priority boost");

        for (int i = 0; i < 18; ++i) {
            rtThread->decayQuantum();
        }
        TEST_ASSERT(rtThread->getDynamicPriority() == 24, "Real-time thread priority must NOT decay");

        // Suspend and Resume
        uint32_t prev1 = ps::PsSuspendThread(dynamicThread.get());
        TEST_ASSERT(prev1 == 0, "First suspend must return previous count 0");
        TEST_ASSERT(dynamicThread->getState() == ps::ThreadState::Waiting, "Thread must enter Waiting state upon suspension");
        TEST_ASSERT(dynamicThread->getSuspendCount() == 1, "Suspend count must be 1");

        uint32_t prev2 = ps::PsSuspendThread(dynamicThread.get());
        TEST_ASSERT(prev2 == 1, "Second suspend must return previous count 1");
        TEST_ASSERT(dynamicThread->getSuspendCount() == 2, "Suspend count must be 2");

        uint32_t res1 = ps::PsResumeThread(dynamicThread.get());
        TEST_ASSERT(res1 == 2, "First resume must return previous count 2");
        TEST_ASSERT(dynamicThread->getState() == ps::ThreadState::Waiting, "Thread must remain Waiting while suspendCount > 0");

        uint32_t res2 = ps::PsResumeThread(dynamicThread.get());
        TEST_ASSERT(res2 == 1, "Second resume must return previous count 1");
        TEST_ASSERT(dynamicThread->getState() == ps::ThreadState::Ready, "Thread must return to Ready when suspendCount reaches 0");

        // Kernel Stack Setup
        dynamicThread->setKernelStack(0xFFFF800000100000ULL, 0xFFFF8000000F8000ULL);
        TEST_ASSERT(dynamicThread->getKernelStackBase() == 0xFFFF800000100000ULL, "Kernel stack base must match");
        TEST_ASSERT(dynamicThread->getKernelStackLimit() == 0xFFFF8000000F8000ULL, "Kernel stack limit must match");

        testProc->terminate(NtStatus::Success);
    }

    // 5. PS Subsystem Telemetry Verification & Teardown
    {
        auto telem = ps::PsQueryTelemetry();
        TEST_ASSERT(telem.totalProcessesCreated >= 4, "totalProcessesCreated must be >= 4");
        TEST_ASSERT(telem.totalProcessesTerminated >= 2, "totalProcessesTerminated must be >= 2");
        TEST_ASSERT(telem.totalThreadsCreated >= 4, "totalThreadsCreated must be >= 4");
        TEST_ASSERT(telem.totalThreadsTerminated >= 2, "totalThreadsTerminated must be >= 2");
        TEST_ASSERT(telem.totalJobsCreated >= 1, "totalJobsCreated must be >= 1");
        TEST_ASSERT(telem.totalProcessesAssignedToJobs >= 2, "totalProcessesAssignedToJobs must be >= 2");
        TEST_ASSERT(telem.totalProcessCallbacksInvoked >= 4, "totalProcessCallbacksInvoked must be >= 4");
        TEST_ASSERT(telem.totalThreadCallbacksInvoked >= 4, "totalThreadCallbacksInvoked must be >= 4");
        TEST_ASSERT(telem.totalQuotaViolations >= 2, "totalQuotaViolations must be >= 2");
        TEST_ASSERT(telem.totalThreadPriorityBoosts >= 2, "totalThreadPriorityBoosts must be >= 2");
        TEST_ASSERT(telem.totalThreadSuspensions >= 2, "totalThreadSuspensions must be >= 2");
        TEST_ASSERT(telem.totalThreadResumptions >= 2, "totalThreadResumptions must be >= 2");

        ps::PsResetTelemetryForTesting();
        auto telemReset = ps::PsQueryTelemetry();
        TEST_ASSERT(telemReset.totalProcessesCreated == 0, "PsResetTelemetryForTesting must zero out counters");
    }

    pm.resetForTesting();
    std::cout << "[TEST] Suite 240: Horizon 24 - Sovereign Process Subsystem Quotas, Jobs & Callbacks PASSED.\n";
}

// ============================================================================
// Suite 241: Horizon 25 - Sovereign Security Reference Monitor (Se) Subsystem
// ============================================================================
void Test_Horizon25_SecurityReferenceMonitor_GenericMapping_Inheritance_And_Impersonation() {
    std::cout << "[TEST] Suite 241: Horizon 25 - Sovereign Security Reference Monitor (Se) Subsystem...\n" << std::flush;

    se::SeResetTelemetryForTesting();

    // 1. Generic Rights Mapping
    {
        uint32_t genericMask = se::GENERIC_READ | se::GENERIC_WRITE;
        se::RtlMapGenericMask(genericMask, se::FILE_GENERIC_MAPPING);

        // Generic bits must be stripped
        TEST_ASSERT((genericMask & (se::GENERIC_READ | se::GENERIC_WRITE | se::GENERIC_EXECUTE | se::GENERIC_ALL)) == 0,
                    "RtlMapGenericMask must strip high generic bits");

        // Specific and standard rights must be populated
        TEST_ASSERT((genericMask & se::FILE_READ_DATA) != 0, "Mapped generic read must contain FILE_READ_DATA");
        TEST_ASSERT((genericMask & se::FILE_WRITE_DATA) != 0, "Mapped generic write must contain FILE_WRITE_DATA");
        TEST_ASSERT((genericMask & se::STANDARD_RIGHTS_READ) != 0, "Mapped generic read must contain STANDARD_RIGHTS_READ");
        TEST_ASSERT((genericMask & se::STANDARD_RIGHTS_WRITE) != 0, "Mapped generic write must contain STANDARD_RIGHTS_WRITE");
        TEST_ASSERT((genericMask & se::SYNCHRONIZE) != 0, "Mapped file rights must include SYNCHRONIZE");
    }

    // 2. Binary Self-Relative Security Descriptor Serialization & Deserialization
    {
        se::SecurityDescriptor sd;
        sd.setOwner(se::Sid::administrators());
        sd.setGroup(se::Sid::users());

        se::Acl dacl;
        dacl.addAllowedAce(se::Sid::users(), se::FILE_READ_DATA);
        dacl.addDeniedAce(se::Sid::everyone(), se::FILE_WRITE_DATA);
        sd.setDacl(std::move(dacl));

        sd.setIntegrityLevel(se::SECURITY_MANDATORY_HIGH_RID, se::SYSTEM_MANDATORY_LABEL_NO_WRITE_UP);
        TEST_ASSERT(se::RtlValidSecurityDescriptor(sd), "Constructed SecurityDescriptor must be valid");

        // Serialize to self-relative binary buffer
        auto binarySd = sd.makeSelfRelative();
        TEST_ASSERT(binarySd.size() >= 20, "Self-relative SD binary buffer must be at least header size (20 bytes)");
        TEST_ASSERT(binarySd[0] == 1, "Self-relative SD revision must be 1");
        uint16_t control = static_cast<uint16_t>(binarySd[2]) | (static_cast<uint16_t>(binarySd[3]) << 8);
        TEST_ASSERT((control & se::SE_SELF_RELATIVE) != 0, "Self-relative SD control must have SE_SELF_RELATIVE set");
        TEST_ASSERT((control & se::SE_DACL_PRESENT) != 0, "Self-relative SD control must have SE_DACL_PRESENT set");
        TEST_ASSERT((control & se::SE_SACL_PRESENT) != 0, "Self-relative SD control must have SE_SACL_PRESENT set");

        // Deserialize from binary buffer
        auto roundtripSd = se::SecurityDescriptor::fromSelfRelative(binarySd);
        TEST_ASSERT(roundtripSd.has_value(), "Deserializing self-relative SD must succeed");
        TEST_ASSERT(roundtripSd->getOwner().has_value() && *roundtripSd->getOwner() == se::Sid::administrators(),
                    "Deserialized SD owner must match administrators");
        TEST_ASSERT(roundtripSd->getGroup().has_value() && *roundtripSd->getGroup() == se::Sid::users(),
                    "Deserialized SD group must match users");
        TEST_ASSERT(roundtripSd->hasDacl() && roundtripSd->getDacl()->getAceCount() == 2,
                    "Deserialized SD DACL must have 2 ACEs");
        TEST_ASSERT(roundtripSd->hasMandatoryLabel(), "Deserialized SD must have mandatory label");
        TEST_ASSERT(roundtripSd->getIntegrityLevel() == se::SECURITY_MANDATORY_HIGH_RID,
                    "Deserialized SD integrity level must be High");
    }

    // 3. Security Descriptor Inheritance Engine (SeAssignSecurity)
    {
        se::SecurityDescriptor parentSd;
        parentSd.setOwner(se::Sid::administrators());
        parentSd.setGroup(se::Sid::administrators());

        se::Acl parentDacl;
        // ACE 1: Inheritable to containers and objects
        parentDacl.addAllowedAce(se::Sid::administrators(), se::STANDARD_RIGHTS_ALL | se::SPECIFIC_RIGHTS_ALL,
                                 se::CONTAINER_INHERIT_ACE | se::OBJECT_INHERIT_ACE);
        // ACE 2: Inheritable only to objects, no propagate
        parentDacl.addAllowedAce(se::Sid::users(), se::FILE_READ_DATA,
                                 se::OBJECT_INHERIT_ACE | se::NO_PROPAGATE_INHERIT_ACE);
        // ACE 3: Deny to Everyone
        parentDacl.addDeniedAce(se::Sid::everyone(), se::DELETE,
                                se::CONTAINER_INHERIT_ACE | se::OBJECT_INHERIT_ACE);
        parentSd.setDacl(std::move(parentDacl));

        auto userToken = se::TokenObject::createUserToken(se::Sid(5, {21, 1001}));

        // Case A: Create child leaf object (file)
        se::SecurityDescriptor childFileSd;
        se::SecurityDescriptor explicitFileSd;
        se::Acl explicitFileDacl;
        explicitFileDacl.addAllowedAce(se::Sid::localSystem(), se::FILE_READ_DATA | se::FILE_WRITE_DATA);
        explicitFileSd.setDacl(std::move(explicitFileDacl));

        NtStatus stAssign = se::SeAssignSecurity(
            &parentSd,
            &explicitFileSd,
            false, // isDirectory = false
            *userToken,
            se::FILE_GENERIC_MAPPING,
            childFileSd
        );
        TEST_ASSERT(NT_SUCCESS(stAssign), "SeAssignSecurity for child file must succeed");
        TEST_ASSERT(childFileSd.getOwner().has_value() && *childFileSd.getOwner() == userToken->getUserSid(),
                    "Child file owner must default to subject token user SID");
        TEST_ASSERT(childFileSd.hasDacl(), "Child file must have DACL assigned");

        const auto& childDacl = *childFileSd.getDacl();
        // Child DACL canonical order:
        // 1. Explicit Deny (none)
        // 2. Explicit Allow (LocalSystem)
        // 3. Inherited Deny (Everyone DELETE)
        // 4. Inherited Allow (Administrators, Users)
        TEST_ASSERT(childDacl.getAceCount() == 4, "Child file DACL must contain 4 canonical ACEs");
        TEST_ASSERT(childDacl.getAces()[0].sid == se::Sid::localSystem() && childDacl.getAces()[0].type == se::AceType::AccessAllowed,
                    "First ACE must be Explicit Allow (LocalSystem)");
        TEST_ASSERT(childDacl.getAces()[1].sid == se::Sid::everyone() && childDacl.getAces()[1].type == se::AceType::AccessDenied,
                    "Second ACE must be Inherited Deny (Everyone)");
        TEST_ASSERT((childDacl.getAces()[1].flags & se::INHERITED_ACE) != 0, "Inherited ACE must have INHERITED_ACE flag");
        TEST_ASSERT((childDacl.getAces()[2].flags & se::INHERITED_ACE) != 0, "Inherited ACE must have INHERITED_ACE flag");
        TEST_ASSERT((childDacl.getAces()[3].flags & se::INHERITED_ACE) != 0, "Inherited ACE must have INHERITED_ACE flag");

        // Case B: Create protected child with SE_DACL_PROTECTED
        se::SecurityDescriptor protectedChildSd;
        se::SecurityDescriptor explicitProtectedSd;
        explicitProtectedSd.setControl(se::SE_DACL_PROTECTED);
        se::Acl protectedDacl;
        protectedDacl.addAllowedAce(userToken->getUserSid(), se::FILE_READ_DATA);
        explicitProtectedSd.setDacl(std::move(protectedDacl));

        NtStatus stAssignProt = se::SeAssignSecurity(
            &parentSd,
            &explicitProtectedSd,
            false,
            *userToken,
            se::FILE_GENERIC_MAPPING,
            protectedChildSd
        );
        TEST_ASSERT(NT_SUCCESS(stAssignProt), "SeAssignSecurity with SE_DACL_PROTECTED must succeed");
        TEST_ASSERT(protectedChildSd.getDacl()->getAceCount() == 1,
                    "Protected child DACL must contain ONLY explicit ACEs, zero inherited");
        TEST_ASSERT((protectedChildSd.getDacl()->getAces()[0].flags & se::INHERITED_ACE) == 0,
                    "Explicit ACE must NOT have INHERITED_ACE flag");
    }

    // 4. Enhanced Access Checking (SeAccessCheck), MAXIMUM_ALLOWED & Privilege Overrides
    {
        se::SecurityDescriptor sd;
        sd.setOwner(se::Sid(5, {21, 5000})); // Special owner SID

        se::Acl dacl;
        // Allow Users read access
        dacl.addAllowedAce(se::Sid::users(), se::FILE_READ_DATA);
        // Explicitly Deny Users write access
        dacl.addDeniedAce(se::Sid::users(), se::FILE_WRITE_DATA);
        sd.setDacl(std::move(dacl));

        auto userSid = se::Sid(5, {21, 6000});
        auto userToken = se::TokenObject::createUserToken(userSid);

        // Standard user requesting MAXIMUM_ALLOWED
        uint32_t grantedMax = 0;
        NtStatus stMax = se::SecurityReferenceMonitor::SeAccessCheck(
            sd,
            *userToken,
            se::MAXIMUM_ALLOWED,
            se::FILE_GENERIC_MAPPING,
            grantedMax
        );
        TEST_ASSERT(NT_SUCCESS(stMax), "SeAccessCheck with MAXIMUM_ALLOWED must succeed");
        TEST_ASSERT((grantedMax & se::FILE_READ_DATA) != 0, "MAXIMUM_ALLOWED must grant allowed FILE_READ_DATA");
        TEST_ASSERT((grantedMax & se::FILE_WRITE_DATA) == 0, "MAXIMUM_ALLOWED must NOT grant denied FILE_WRITE_DATA");

        // Owner requesting MAXIMUM_ALLOWED: must also receive READ_CONTROL | WRITE_DAC
        auto ownerToken = se::TokenObject::createUserToken(se::Sid(5, {21, 5000}));
        uint32_t ownerGranted = 0;
        NtStatus stOwnerMax = se::SecurityReferenceMonitor::SeAccessCheck(
            sd,
            *ownerToken,
            se::MAXIMUM_ALLOWED,
            se::FILE_GENERIC_MAPPING,
            ownerGranted
        );
        TEST_ASSERT(NT_SUCCESS(stOwnerMax), "Owner MAXIMUM_ALLOWED must succeed");
        TEST_ASSERT((ownerGranted & (se::READ_CONTROL | se::WRITE_DAC)) == (se::READ_CONTROL | se::WRITE_DAC),
                    "Owner must be granted READ_CONTROL and WRITE_DAC");

        // Privilege Override: SeBackupPrivilege bypassing DACL denial
        se::SecurityDescriptor backupRestrictedSd;
        se::Acl denyAllDacl;
        denyAllDacl.addDeniedAce(se::Sid::everyone(), se::FILE_READ_DATA);
        backupRestrictedSd.setDacl(std::move(denyAllDacl));

        // Without privilege -> Denied
        uint32_t readGranted = 0;
        NtStatus stNoPriv = se::SecurityReferenceMonitor::SeAccessCheck(
            backupRestrictedSd,
            *userToken,
            se::FILE_READ_DATA,
            se::FILE_GENERIC_MAPPING,
            readGranted
        );
        TEST_ASSERT(stNoPriv == NtStatus::AccessDenied, "Without SeBackupPrivilege, denied read must return AccessDenied");

        // With enabled SeBackupPrivilege -> Overrides denial and succeeds!
        userToken->addPrivilege(se::SE_BACKUP_NAME, se::SE_PRIVILEGE_ENABLED);
        NtStatus stWithPriv = se::SecurityReferenceMonitor::SeAccessCheck(
            backupRestrictedSd,
            *userToken,
            se::FILE_READ_DATA,
            se::FILE_GENERIC_MAPPING,
            readGranted
        );
        TEST_ASSERT(NT_SUCCESS(stWithPriv), "With enabled SeBackupPrivilege, read denial must be bypassed");
        TEST_ASSERT((readGranted & se::FILE_READ_DATA) != 0, "Granted access must include FILE_READ_DATA");

        // Privilege Override: SeRestorePrivilege bypassing DACL write denial
        se::SecurityDescriptor restoreRestrictedSd;
        se::Acl denyWriteDacl;
        denyWriteDacl.addDeniedAce(se::Sid::everyone(), se::FILE_WRITE_DATA);
        restoreRestrictedSd.setDacl(std::move(denyWriteDacl));

        userToken->addPrivilege(se::SE_RESTORE_NAME, se::SE_PRIVILEGE_ENABLED);
        uint32_t writeGranted = 0;
        NtStatus stRestore = se::SecurityReferenceMonitor::SeAccessCheck(
            restoreRestrictedSd,
            *userToken,
            se::FILE_WRITE_DATA,
            se::FILE_GENERIC_MAPPING,
            writeGranted
        );
        TEST_ASSERT(NT_SUCCESS(stRestore), "With enabled SeRestorePrivilege, write denial must be bypassed");
        TEST_ASSERT((writeGranted & se::FILE_WRITE_DATA) != 0, "Granted access must include FILE_WRITE_DATA");
    }

    // 5. Security Subject Context & Token Impersonation Level Enforcement
    {
        auto primaryToken = se::TokenObject::createUserToken(se::Sid(5, {21, 7000}));
        auto clientToken = se::TokenObject::createAdminToken(se::Sid(5, {21, 8000}));

        se::SecuritySubjectContext ctx;
        se::SeCaptureSubjectContext(ctx, primaryToken, clientToken);
        TEST_ASSERT(ctx.isImpersonating(), "Context must be in impersonating state");

        se::SecurityDescriptor adminOnlySd;
        se::Acl adminDacl;
        adminDacl.addAllowedAce(se::Sid::administrators(), se::STANDARD_RIGHTS_ALL | se::SPECIFIC_RIGHTS_ALL);
        adminOnlySd.setDacl(std::move(adminDacl));

        // A. Impersonation Level: Identification (Cannot access securable objects on client's behalf)
        clientToken->setImpersonationLevel(se::SecurityImpersonationLevel::Identification);
        ctx.impersonationLevel = se::SecurityImpersonationLevel::Identification;

        uint32_t granted = 0;
        NtStatus stIdent = se::SecurityReferenceMonitor::SeAccessCheck(
            adminOnlySd,
            ctx,
            se::FILE_READ_DATA,
            se::FILE_GENERIC_MAPPING,
            granted
        );
        TEST_ASSERT(stIdent == NtStatus::BadImpersonationLevel,
                    "Access check under SecurityIdentification impersonation must return STATUS_BAD_IMPERSONATION_LEVEL");

        // B. Impersonation Level: Impersonation (Valid for local securable access)
        clientToken->setImpersonationLevel(se::SecurityImpersonationLevel::Impersonation);
        ctx.impersonationLevel = se::SecurityImpersonationLevel::Impersonation;

        NtStatus stImpersonate = se::SecurityReferenceMonitor::SeAccessCheck(
            adminOnlySd,
            ctx,
            se::FILE_READ_DATA,
            se::FILE_GENERIC_MAPPING,
            granted
        );
        TEST_ASSERT(NT_SUCCESS(stImpersonate), "Access check under SecurityImpersonation must succeed for Admin client token");
        TEST_ASSERT((granted & se::FILE_READ_DATA) != 0, "Granted access must include FILE_READ_DATA");

        se::SeReleaseSubjectContext(ctx);
        TEST_ASSERT(!ctx.isImpersonating(), "Released context must not be impersonating");
    }

    // 6. KernelMode Processor Mode Bypass
    {
        se::SecurityDescriptor closedSd;
        se::Acl emptyDacl;
        closedSd.setDacl(std::move(emptyDacl)); // Empty DACL denies all UserMode callers

        auto userToken = se::TokenObject::createUserToken(se::Sid(5, {21, 9000}));
        uint32_t grantedKm = 0;
        NtStatus stKm = se::SecurityReferenceMonitor::SeAccessCheck(
            closedSd,
            *userToken,
            se::FILE_READ_DATA | se::FILE_WRITE_DATA,
            se::FILE_GENERIC_MAPPING,
            grantedKm,
            L"",
            se::ProcessorMode::KernelMode
        );
        TEST_ASSERT(NT_SUCCESS(stKm), "ProcessorMode::KernelMode must unconditionally bypass access check");
        TEST_ASSERT((grantedKm & (se::FILE_READ_DATA | se::FILE_WRITE_DATA)) == (se::FILE_READ_DATA | se::FILE_WRITE_DATA),
                    "KernelMode must be granted all requested rights");
    }

    // 7. Security Reference Monitor Telemetry Verification
    {
        auto telem = se::SeQueryTelemetry();
        TEST_ASSERT(telem.totalAccessChecks >= 5, "totalAccessChecks must be >= 5");
        TEST_ASSERT(telem.totalAccessChecksGranted >= 3, "totalAccessChecksGranted must be >= 3");
        TEST_ASSERT(telem.totalAccessChecksDenied >= 1, "totalAccessChecksDenied must be >= 1");
        TEST_ASSERT(telem.totalMaximumAllowedEvaluations >= 2, "totalMaximumAllowedEvaluations must be >= 2");
        TEST_ASSERT(telem.totalInheritedAcesPropagated >= 2, "totalInheritedAcesPropagated must be >= 2");
        TEST_ASSERT(telem.totalSecurityDescriptorsAssigned >= 2, "totalSecurityDescriptorsAssigned must be >= 2");
        TEST_ASSERT(telem.totalBackupRestoreOverrides >= 2, "totalBackupRestoreOverrides must be >= 2");
        TEST_ASSERT(telem.totalTokenImpersonations >= 2, "totalTokenImpersonations must be >= 2");

        se::SeResetTelemetryForTesting();
        auto telemReset = se::SeQueryTelemetry();
        TEST_ASSERT(telemReset.totalAccessChecks == 0, "SeResetTelemetryForTesting must zero out counters");
    }

    std::cout << "[TEST] Suite 241: Horizon 25 - Sovereign Security Reference Monitor (Se) Subsystem PASSED.\n";
}

// ============================================================================
// Suite 242: Horizon 26 - Sovereign Event Tracing for Windows (Etw) Subsystem
// ============================================================================
void Test_Horizon26_EventTracingForWindows_Sessions_CircularBuffers_And_RealtimeDispatch() {
    std::cout << "[TEST] Suite 242: Horizon 26 - Sovereign Event Tracing for Windows (Etw) Subsystem...\n" << std::flush;

    etw::EtwResetTelemetryForTesting();

    // 1. Initial Telemetry Verification
    {
        auto telem = etw::EtwQueryTelemetry();
        TEST_ASSERT(telem.totalSessionsStarted == 0, "Initial totalSessionsStarted must be 0");
        TEST_ASSERT(telem.totalEventsWritten == 0, "Initial totalEventsWritten must be 0");
        TEST_ASSERT(telem.totalCircularBufferWraps == 0, "Initial totalCircularBufferWraps must be 0");
        TEST_ASSERT(telem.totalEventsLost == 0, "Initial totalEventsLost must be 0");
    }

    // 2. Circular Trace Session Lifecycle & Properties Configuration
    etw::TRACEHANDLE hCircSession = 0;
    {
        etw::EVENT_TRACE_PROPERTIES props{};
        props.Wnode.BufferSize = sizeof(etw::EVENT_TRACE_PROPERTIES);
        props.BufferSize = 1; // 1 KB buffers for deterministic buffer filling
        props.MinimumBuffers = 2;
        props.MaximumBuffers = 3;
        props.LogFileMode = etw::EVENT_TRACE_REAL_TIME_MODE | etw::EVENT_TRACE_FILE_MODE_CIRCULAR;
        props.FlushTimer = 1;
        props.EnableFlags = etw::EVENT_TRACE_FLAG_PROCESS;

        uint32_t status = etw::StartTraceW(&hCircSession, L"H26CircularTraceSession", &props);
        TEST_ASSERT(status == etw::ERROR_SUCCESS && hCircSession != 0, "StartTraceW for circular session must succeed");

        etw::EVENT_TRACE_PROPERTIES qProps{};
        status = etw::QueryTraceW(hCircSession, nullptr, &qProps);
        TEST_ASSERT(status == etw::ERROR_SUCCESS, "QueryTraceW must succeed");
        TEST_ASSERT(qProps.BufferSize == 1, "Configured BufferSize must be 1 KB");
        TEST_ASSERT(qProps.MaximumBuffers == 3, "Configured MaximumBuffers must be 3");
        TEST_ASSERT((qProps.LogFileMode & etw::EVENT_TRACE_FILE_MODE_CIRCULAR) != 0, "LogFileMode must include CIRCULAR");
    }

    // 3. Provider Registration, Enable Callback & Level/Keyword Filtering
    etw::REGHANDLE hProvider = 0;
    GUID h26ProviderGuid = { 0x8899AABB, 0xCCDD, 0xEEFF, { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 } };
    static bool s_h26CallbackFired = false;
    static uint32_t s_h26CallbackCode = 0;
    static uint8_t s_h26Level = 0;
    static uint64_t s_h26AnyKw = 0;
    static uint64_t s_h26AllKw = 0;

    {
        s_h26CallbackFired = false;
        auto callback = [](const GUID* srcId, uint32_t isEnabled, uint8_t level, uint64_t anyKw, uint64_t allKw, void* filter, void* ctx) {
            (void)srcId; (void)filter; (void)ctx;
            s_h26CallbackFired = true;
            s_h26CallbackCode = isEnabled;
            s_h26Level = level;
            s_h26AnyKw = anyKw;
            s_h26AllKw = allKw;
        };

        uint32_t status = etw::EventRegister(&h26ProviderGuid, callback, nullptr, &hProvider);
        TEST_ASSERT(status == etw::ERROR_SUCCESS && hProvider != 0, "EventRegister must succeed");

        // Enable provider with specific Level and Keyword mask
        status = etw::EnableTraceEx2(hCircSession, &h26ProviderGuid, etw::EVENT_CONTROL_CODE_ENABLE_PROVIDER,
                                     etw::TRACE_LEVEL_INFORMATION, 0x00FFULL, 0x0001ULL, 0, nullptr);
        TEST_ASSERT(status == etw::ERROR_SUCCESS, "EnableTraceEx2 must succeed");
        TEST_ASSERT(s_h26CallbackFired, "EnableCallback must have fired");
        TEST_ASSERT(s_h26CallbackCode == etw::EVENT_CONTROL_CODE_ENABLE_PROVIDER, "Callback code must be ENABLE");
        TEST_ASSERT(s_h26Level == etw::TRACE_LEVEL_INFORMATION, "Callback level must be INFORMATION");
        TEST_ASSERT(s_h26AnyKw == 0x00FFULL && s_h26AllKw == 0x0001ULL, "Callback keywords must match");

        // Filtering verification:
        // Level INFORMATION (4), Keyword 0x0001 -> matches
        TEST_ASSERT(etw::EventProviderEnabled(hProvider, etw::TRACE_LEVEL_INFORMATION, 0x0001) == 1,
                    "Provider must be enabled for level 4 keyword 0x0001");
        // Level VERBOSE (5) -> must be filtered out
        TEST_ASSERT(etw::EventProviderEnabled(hProvider, etw::TRACE_LEVEL_VERBOSE, 0x0001) == 0,
                    "Provider must be filtered out for level 5 (VERBOSE)");
        // Keyword without bit 0 (fails allKeyword match) -> filtered out
        TEST_ASSERT(etw::EventProviderEnabled(hProvider, etw::TRACE_LEVEL_INFORMATION, 0x0002) == 0,
                    "Provider must be filtered out when matchAllKeyword is not satisfied");
    }

    // 4. Circular Buffer Wrap Mechanics (EVENT_TRACE_FILE_MODE_CIRCULAR)
    {
        auto session = etw::TraceManager::get().getSessionByName(L"H26CircularTraceSession");
        TEST_ASSERT(session != nullptr, "Session must exist");

        // With BufferSize = 1 KB (1024 bytes), an event payload of 200 bytes (+ 80-byte header = 280 bytes)
        // allows ~3 events per buffer before full. With MaximumBuffers = 3, writing 15 events will fill
        // all 3 buffers and wrap around the circular ring, overwriting older buffers.
        std::vector<uint8_t> payload(200, 0xAA);
        etw::EVENT_DESCRIPTOR desc{};
        desc.Id = 1001;
        desc.Level = etw::TRACE_LEVEL_INFORMATION;
        desc.Keyword = 0x0001;

        etw::EVENT_DATA_DESCRIPTOR dataDesc{};
        dataDesc.Ptr = reinterpret_cast<uint64_t>(payload.data());
        dataDesc.Size = static_cast<uint32_t>(payload.size());

        for (int i = 0; i < 15; ++i) {
            uint32_t stWrite = etw::EventWrite(hProvider, &desc, 1, &dataDesc);
            TEST_ASSERT(stWrite == etw::ERROR_SUCCESS, "EventWrite in circular session must succeed");
        }

        // Circular wraps must have occurred
        TEST_ASSERT(session->getCircularWraps() >= 1, "Circular buffer wraps must be >= 1");
        TEST_ASSERT(session->getProperties().EventsLost == 0, "Circular mode must overwrite buffers without losing events");
        TEST_ASSERT(session->getCompletedBuffersCount() >= 3, "Completed buffers must be >= 3");

        auto telem = etw::EtwQueryTelemetry();
        TEST_ASSERT(telem.totalCircularBufferWraps >= 1, "Telemetry totalCircularBufferWraps must be >= 1");
        TEST_ASSERT(telem.totalBuffersFlushed >= 3, "Telemetry totalBuffersFlushed must be >= 3");
    }

    // 5. Sequential Buffer Mode & Event Loss (EVENT_TRACE_FILE_MODE_SEQUENTIAL)
    {
        etw::TRACEHANDLE hSeqSession = 0;
        etw::EVENT_TRACE_PROPERTIES seqProps{};
        seqProps.Wnode.BufferSize = sizeof(etw::EVENT_TRACE_PROPERTIES);
        seqProps.BufferSize = 1; // 1 KB
        seqProps.MinimumBuffers = 2;
        seqProps.MaximumBuffers = 2; // Strict limit: 2 buffers
        seqProps.LogFileMode = etw::EVENT_TRACE_FILE_MODE_SEQUENTIAL;

        uint32_t status = etw::StartTraceW(&hSeqSession, L"H26SequentialTraceSession", &seqProps);
        TEST_ASSERT(status == etw::ERROR_SUCCESS, "StartTraceW for sequential session must succeed");

        etw::EnableTraceEx2(hSeqSession, &h26ProviderGuid, etw::EVENT_CONTROL_CODE_ENABLE_PROVIDER,
                            etw::TRACE_LEVEL_INFORMATION, 0xFFFFFFFFULL, 0, 0, nullptr);

        auto seqSession = etw::TraceManager::get().getSessionByName(L"H26SequentialTraceSession");
        TEST_ASSERT(seqSession != nullptr, "Sequential session must exist");

        std::vector<uint8_t> payload(200, 0xBB);
        etw::EVENT_DESCRIPTOR desc{};
        desc.Id = 2001;
        desc.Level = etw::TRACE_LEVEL_INFORMATION;
        desc.Keyword = 0x0001;

        etw::EVENT_DATA_DESCRIPTOR dataDesc{};
        dataDesc.Ptr = reinterpret_cast<uint64_t>(payload.data());
        dataDesc.Size = static_cast<uint32_t>(payload.size());

        for (int i = 0; i < 15; ++i) {
            etw::EventWrite(hProvider, &desc, 1, &dataDesc);
        }

        // In sequential mode, once both buffers are full, events must be dropped and EventsLost incremented
        TEST_ASSERT(seqSession->getProperties().EventsLost > 0, "Sequential mode must track lost events when pool exhausted");
        auto telem = etw::EtwQueryTelemetry();
        TEST_ASSERT(telem.totalEventsLost > 0, "Telemetry totalEventsLost must be > 0");

        etw::StopTraceW(hSeqSession, nullptr, nullptr);
    }

    // 6. Transfer Tracing & Activity ID Propagation (EventWriteTransfer)
    GUID activityId = { 0xAAAA1111, 0x2222, 0x3333, { 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB } };
    GUID relatedActivityId = { 0xBBBB2222, 0x3333, 0x4444, { 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC } };
    {
        etw::EVENT_DESCRIPTOR transferDesc{};
        transferDesc.Id = 3001;
        transferDesc.Level = etw::TRACE_LEVEL_INFORMATION;
        transferDesc.Keyword = 0x0001;

        uint32_t transferPayload = 0x55AA55AA;
        etw::EVENT_DATA_DESCRIPTOR dataDesc{};
        dataDesc.Ptr = reinterpret_cast<uint64_t>(&transferPayload);
        dataDesc.Size = sizeof(transferPayload);

        uint32_t status = etw::EventWriteTransfer(hProvider, &transferDesc, &activityId, &relatedActivityId, 1, &dataDesc);
        TEST_ASSERT(status == etw::ERROR_SUCCESS, "EventWriteTransfer must succeed");
    }

    // 7. Real-Time Consumer Dispatch Engine (OpenTraceW, ProcessTrace, CloseTrace)
    {
        static uint32_t s_consumerEventsCount = 0;
        static uint32_t s_consumerBuffersCount = 0;
        static bool s_foundTransferEvent = false;

        s_consumerEventsCount = 0;
        s_consumerBuffersCount = 0;
        s_foundTransferEvent = false;

        etw::EVENT_TRACE_LOGFILEW logfile{};
        wchar_t sessionName[] = L"H26CircularTraceSession";
        logfile.LoggerName = sessionName;

        logfile.BufferCallback = [](void* pLog) -> uint32_t {
            if (pLog) s_consumerBuffersCount++;
            return 1;
        };

        logfile.EventRecordCallback = [](etw::EVENT_RECORD* pRec) {
            if (!pRec) return;
            s_consumerEventsCount++;

            TEST_ASSERT(pRec->BufferContext.Alignment == 8, "BufferContext alignment must be 8");
            TEST_ASSERT(pRec->BufferContext.LoggerId != 0, "BufferContext LoggerId must be valid");

            if (pRec->EventHeader.EventDescriptor.Id == 3001) {
                // Verify Activity ID and Related Activity ID extended data
                static const GUID expectedAct = { 0xAAAA1111, 0x2222, 0x3333, { 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB } };
                static const GUID expectedRel = { 0xBBBB2222, 0x3333, 0x4444, { 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC } };
                if (std::memcmp(&pRec->EventHeader.ActivityId, &expectedAct, sizeof(GUID)) == 0) {
                    if (pRec->ExtendedDataCount == 1 && pRec->ExtendedData != nullptr) {
                        auto* pExt = static_cast<etw::EVENT_HEADER_EXTENDED_DATA_ITEM*>(pRec->ExtendedData);
                        if (pExt->ExtType == etw::EVENT_HEADER_EXT_TYPE_RELATED_ACTIVITY_ID) {
                            auto* pGuid = reinterpret_cast<const GUID*>(pExt->DataPtr);
                            if (pGuid && std::memcmp(pGuid, &expectedRel, sizeof(GUID)) == 0) {
                                s_foundTransferEvent = true;
                            }
                        }
                    }
                }
            }
        };

        etw::TRACEHANDLE hConsumer = etw::OpenTraceW(&logfile);
        TEST_ASSERT(hConsumer != etw::INVALID_PROCESSTRACE_HANDLE, "OpenTraceW must succeed");

        uint32_t status = etw::ProcessTrace(&hConsumer, 1, nullptr, nullptr);
        TEST_ASSERT(status == etw::ERROR_SUCCESS, "ProcessTrace must return ERROR_SUCCESS");
        TEST_ASSERT(s_consumerEventsCount > 0, "ProcessTrace must deliver events to consumer");
        TEST_ASSERT(s_consumerBuffersCount > 0, "BufferCallback must be invoked for trace buffers");
        TEST_ASSERT(s_foundTransferEvent, "Consumer must receive EventWriteTransfer with verified ActivityId and RelatedActivityId");

        status = etw::CloseTrace(hConsumer);
        TEST_ASSERT(status == etw::ERROR_SUCCESS, "CloseTrace must succeed");
    }

    // 8. NT Kernel Logger System Event Routing & Subsystem Telemetry
    {
        // Emit kernel-mode trace events across multiple subsystems
        etw::EtwTraceProcess(etw::EVENT_TRACE_TYPE_START, 7001, 1000, 1, 0, "micant_core.exe");
        etw::EtwTraceThread(etw::EVENT_TRACE_TYPE_START, 7001, 9001, 0x7FFF00100000ULL, 0);
        etw::EtwTraceDiskIo(10 /* Read */, 0, 0x100000ULL, 16384);
        etw::EtwTracePageFault(7001, 0x7FFF00200000ULL, 0x7FFF00100040ULL, 1 /* DemandZero */);
        etw::EtwTraceAlpc(32 /* Send */, 5555, 7001, 1000, 1);
        etw::EtwTraceRegistry(1 /* Create */, 0x400ULL, 1, L"\\Registry\\Machine\\System\\CurrentControlSet", 0);

        auto telem = etw::EtwQueryTelemetry();
        TEST_ASSERT(telem.totalKernelEventsLogged >= 6, "totalKernelEventsLogged must be >= 6");

        // Verify NT Kernel Logger real-time consumption
        static uint32_t s_kernelEventsReceived = 0;
        s_kernelEventsReceived = 0;

        etw::EVENT_TRACE_LOGFILEW kmLogfile{};
        wchar_t kmName[] = L"NT Kernel Logger";
        kmLogfile.LoggerName = kmName;
        kmLogfile.EventRecordCallback = [](etw::EVENT_RECORD* pRec) {
            if (!pRec) return;
            if (pRec->EventHeader.ProviderId == etw::ProcessTraceGuid ||
                pRec->EventHeader.ProviderId == etw::ThreadTraceGuid ||
                pRec->EventHeader.ProviderId == etw::DiskIoTraceGuid ||
                pRec->EventHeader.ProviderId == etw::PageFaultTraceGuid ||
                pRec->EventHeader.ProviderId == etw::AlpcTraceGuid ||
                pRec->EventHeader.ProviderId == etw::RegistryTraceGuid) {
                s_kernelEventsReceived++;
            }
        };

        etw::TRACEHANDLE hKmConsumer = etw::OpenTraceW(&kmLogfile);
        TEST_ASSERT(hKmConsumer != etw::INVALID_PROCESSTRACE_HANDLE, "OpenTraceW on NT Kernel Logger must succeed");

        uint32_t status = etw::ProcessTrace(&hKmConsumer, 1, nullptr, nullptr);
        TEST_ASSERT(status == etw::ERROR_SUCCESS, "ProcessTrace on NT Kernel Logger must succeed");
        TEST_ASSERT(s_kernelEventsReceived >= 6, "Consumer must receive all 6 logged kernel events");

        etw::CloseTrace(hKmConsumer);
    }

    // 9. Comprehensive ETW Telemetry Audit & Reset
    {
        auto telem = etw::EtwQueryTelemetry();
        TEST_ASSERT(telem.totalSessionsStarted >= 2, "totalSessionsStarted must be >= 2");
        TEST_ASSERT(telem.totalEventsWritten >= 25, "totalEventsWritten must be >= 25");
        TEST_ASSERT(telem.totalEventsDelivered >= 10, "totalEventsDelivered must be >= 10");
        TEST_ASSERT(telem.totalBuffersFlushed >= 3, "totalBuffersFlushed must be >= 3");
        TEST_ASSERT(telem.totalProvidersRegistered >= 1, "totalProvidersRegistered must be >= 1");
        TEST_ASSERT(telem.totalCircularBufferWraps >= 1, "totalCircularBufferWraps must be >= 1");
        TEST_ASSERT(telem.totalEventsLost >= 1, "totalEventsLost must be >= 1");
        TEST_ASSERT(telem.totalKernelEventsLogged >= 6, "totalKernelEventsLogged must be >= 6");

        // Clean up circular test session & provider
        etw::StopTraceW(hCircSession, nullptr, nullptr);
        etw::EventUnregister(hProvider);

        etw::EtwResetTelemetryForTesting();
        auto resetTelem = etw::EtwQueryTelemetry();
        TEST_ASSERT(resetTelem.totalSessionsStarted == 0, "Telemetry reset must clear totalSessionsStarted");
        TEST_ASSERT(resetTelem.totalEventsWritten == 0, "Telemetry reset must clear totalEventsWritten");
        TEST_ASSERT(resetTelem.totalCircularBufferWraps == 0, "Telemetry reset must clear totalCircularBufferWraps");
        TEST_ASSERT(resetTelem.totalEventsLost == 0, "Telemetry reset must clear totalEventsLost");
        TEST_ASSERT(resetTelem.totalKernelEventsLogged == 0, "Telemetry reset must clear totalKernelEventsLogged");
    }

    std::cout << "[TEST] Suite 242: Horizon 26 - Sovereign Event Tracing for Windows (Etw) Subsystem PASSED.\n";
}

// ============================================================================
// Suite 243: Horizon 27 - Sovereign Executive Synchronization, ERESOURCE,
// PushLocks, Guarded Mutexes & Safe User-Buffer Probing Engine
// ============================================================================
void Test_Horizon27_ExecutiveSynchronization_ERESOURCE_PushLocks_And_BufferProbing() {
    std::cout << "[TEST] Suite 243: Horizon 27 - Sovereign Executive Synchronization & Buffer Probing...\n";
    ex::ExResetSyncTelemetryForTesting();

    // 1. Executive Resource (ERESOURCE) Multi-Reader Shared Reentrancy & Fairness
    {
        ex::ERESOURCE res{};
        ex::ExInitializeResourceLite(&res);
        TEST_ASSERT(res.activeCount == 0, "Initial activeCount must be 0");
        TEST_ASSERT(res.sharedOwners == 0, "Initial sharedOwners must be 0");
        TEST_ASSERT(res.exclusiveOwners == 0, "Initial exclusiveOwners must be 0");

        // Thread 1 acquires shared
        BOOLEAN acq1 = ex::ExAcquireResourceSharedLite(&res, TRUE);
        TEST_ASSERT(acq1 == TRUE, "ExAcquireResourceSharedLite must succeed");
        TEST_ASSERT(res.sharedOwners == 1, "Shared owners count must be 1");
        TEST_ASSERT(ex::ExIsResourceAcquiredSharedLite(&res) == 1, "Thread shared count must be 1");

        // Reentrant shared acquisition on same thread
        BOOLEAN acq2 = ex::ExAcquireResourceSharedLite(&res, TRUE);
        TEST_ASSERT(acq2 == TRUE, "Reentrant shared acquire must succeed");
        TEST_ASSERT(res.sharedOwners == 2, "Shared owners count must be 2");
        TEST_ASSERT(ex::ExIsResourceAcquiredSharedLite(&res) == 2, "Thread shared count must be 2");

        // Concurrent reader on another thread
        std::atomic<bool> thread2Acquired{false};
        std::thread readerThread([&]() {
            BOOLEAN ok = ex::ExAcquireResourceSharedLite(&res, TRUE);
            if (ok) {
                thread2Acquired.store(true);
                ex::ExReleaseResourceLite(&res);
            }
        });
        readerThread.join();
        TEST_ASSERT(thread2Acquired.load() == true, "Concurrent reader must acquire shared access");

        // Non-blocking exclusive attempt while shared is held must fail
        BOOLEAN tryEx = ex::ExTryToAcquireResourceExclusiveLite(&res);
        TEST_ASSERT(tryEx == FALSE, "ExTryToAcquireResourceExclusiveLite must fail when shared held");

        // Release first shared recursion
        ex::ExReleaseResourceLite(&res);
        TEST_ASSERT(res.sharedOwners == 1, "Shared owners count must decrement to 1");
        TEST_ASSERT(ex::ExIsResourceAcquiredSharedLite(&res) == 1, "Thread shared count must decrement to 1");

        // Release second shared recursion
        ex::ExReleaseResourceLite(&res);
        TEST_ASSERT(res.sharedOwners == 0, "Shared owners must be 0 after releasing all holds");
        TEST_ASSERT(ex::ExIsResourceAcquiredSharedLite(&res) == 0, "Thread shared count must be 0");

        ex::ExDeleteResourceLite(&res);
    }

    // 2. Executive Resource Exclusive Reentrancy, Lock Conversion & Sole-Owner Upgrade
    {
        ex::ERESOURCE res{};
        ex::ExInitializeResourceLite(&res);

        // Exclusive acquisition
        BOOLEAN acqEx = ex::ExAcquireResourceExclusiveLite(&res, TRUE);
        TEST_ASSERT(acqEx == TRUE, "ExAcquireResourceExclusiveLite must succeed on free resource");
        TEST_ASSERT(res.exclusiveOwners == 1, "exclusiveOwners must be 1");
        TEST_ASSERT(ex::ExIsResourceAcquiredExclusiveLite(&res) == TRUE, "Thread must be exclusive owner");

        // Reentrant exclusive acquisition
        BOOLEAN acqEx2 = ex::ExAcquireResourceExclusiveLite(&res, TRUE);
        TEST_ASSERT(acqEx2 == TRUE, "Reentrant exclusive acquire must succeed");
        TEST_ASSERT(res.exclusiveOwners == 2, "exclusiveOwners must be 2");

        // Shared attempt from another thread while exclusive held must fail
        std::atomic<bool> threadSharedSuccess{true};
        std::thread contenderThread([&]() {
            BOOLEAN ok = ex::ExTryToAcquireResourceSharedLite(&res);
            threadSharedSuccess.store(ok == TRUE);
        });
        contenderThread.join();
        TEST_ASSERT(threadSharedSuccess.load() == false, "TryAcquireShared must fail while exclusive lock held");

        // Atomic lock conversion: Exclusive -> Shared
        ex::ExConvertExclusiveToSharedLite(&res);
        TEST_ASSERT(res.exclusiveOwners == 0, "exclusiveOwners must be 0 after conversion");
        TEST_ASSERT(res.sharedOwners == 2, "sharedOwners must carry forward previous exclusive recursion count");
        TEST_ASSERT(ex::ExIsResourceAcquiredExclusiveLite(&res) == FALSE, "Resource no longer exclusive");
        TEST_ASSERT(ex::ExIsResourceAcquiredSharedLite(&res) == 2, "Shared count for thread must be 2");

        // Release 1 shared hold: now shared count is 1
        ex::ExReleaseResourceLite(&res);
        TEST_ASSERT(res.sharedOwners == 1, "sharedOwners must decrement to 1");

        // Sole-owner upgrade: Shared -> Exclusive
        BOOLEAN upgraded = ex::ExTryConvertSharedToExclusiveLite(&res);
        TEST_ASSERT(upgraded == TRUE, "ExTryConvertSharedToExclusiveLite must succeed for sole shared holder");
        TEST_ASSERT(res.exclusiveOwners == 1, "exclusiveOwners must be 1 after upgrade");
        TEST_ASSERT(res.sharedOwners == 0, "sharedOwners must be 0 after upgrade");
        TEST_ASSERT(ex::ExIsResourceAcquiredExclusiveLite(&res) == TRUE, "Thread must be exclusive owner after upgrade");

        // Release exclusive hold
        ex::ExReleaseResourceLite(&res);
        TEST_ASSERT(res.exclusiveOwners == 0, "exclusiveOwners must be 0");
        TEST_ASSERT(res.activeCount == 0, "activeCount must be 0");

        // Test RAII Resource Guards
        {
            ex::ResourceExclusiveLock exGuard(&res);
            TEST_ASSERT(exGuard.isAcquired() == TRUE, "ResourceExclusiveLock must acquire");
            TEST_ASSERT(ex::ExIsResourceAcquiredExclusiveLite(&res) == TRUE, "Resource is exclusively held");
        }
        TEST_ASSERT(ex::ExIsResourceAcquiredExclusiveLite(&res) == FALSE, "ResourceExclusiveLock dtor must release");

        {
            ex::ResourceSharedLock shGuard(&res);
            TEST_ASSERT(shGuard.isAcquired() == TRUE, "ResourceSharedLock must acquire");
            TEST_ASSERT(ex::ExIsResourceAcquiredSharedLite(&res) == 1, "Resource is held shared");
        }
        TEST_ASSERT(ex::ExIsResourceAcquiredSharedLite(&res) == 0, "ResourceSharedLock dtor must release");

        ex::ExDeleteResourceLite(&res);
    }

    // 3. Executive Push Lock (EX_PUSH_LOCK) Fast Paths, Try-Acquire & Conversion
    {
        ex::EX_PUSH_LOCK pushLock{};
        ex::ExInitializePushLock(&pushLock);
        TEST_ASSERT(pushLock.value == 0, "Push lock value must be 0");

        // Shared acquisition
        ex::ExAcquirePushLockShared(&pushLock);
        TEST_ASSERT(pushLock.shared == 1, "Push lock shared count must be 1");

        // Try acquire shared
        BOOLEAN trySh = ex::ExTryAcquirePushLockShared(&pushLock);
        TEST_ASSERT(trySh == TRUE, "ExTryAcquirePushLockShared must succeed");
        TEST_ASSERT(pushLock.shared == 2, "Push lock shared count must be 2");

        // Try acquire exclusive while shared held must fail
        BOOLEAN tryEx = ex::ExTryAcquirePushLockExclusive(&pushLock);
        TEST_ASSERT(tryEx == FALSE, "ExTryAcquirePushLockExclusive must fail while shared held");

        // Release shared holds
        ex::ExReleasePushLockShared(&pushLock);
        ex::ExReleasePushLockShared(&pushLock);
        TEST_ASSERT(pushLock.shared == 0, "Push lock shared count must be 0");

        // Acquire exclusive
        ex::ExAcquirePushLockExclusive(&pushLock);
        TEST_ASSERT(pushLock.locked == 1, "Push lock locked bit must be set");

        // Convert exclusive to shared
        ex::ExConvertPushLockExclusiveToShared(&pushLock);
        TEST_ASSERT(pushLock.locked == 0, "locked bit must be cleared after conversion");
        TEST_ASSERT(pushLock.shared == 1, "shared count must be 1 after conversion");

        // Release shared
        ex::ExReleasePushLockShared(&pushLock);
        TEST_ASSERT(pushLock.shared == 0, "Push lock must be free");

        // RAII Guards
        {
            ex::PushLockExclusiveGuard guard(&pushLock);
            TEST_ASSERT(pushLock.locked == 1, "PushLockExclusiveGuard must hold exclusive");
        }
        TEST_ASSERT(pushLock.locked == 0, "PushLockExclusiveGuard dtor must release");

        {
            ex::PushLockSharedGuard guard(&pushLock);
            TEST_ASSERT(pushLock.shared == 1, "PushLockSharedGuard must hold shared");
        }
        TEST_ASSERT(pushLock.shared == 0, "PushLockSharedGuard dtor must release");
    }

    // 4. Guarded Mutex (KGUARDED_MUTEX) & Guarded Region APC Protection
    {
        ex::KGUARDED_MUTEX guardedMutex{};
        ex::KeInitializeGuardedMutex(&guardedMutex);
        TEST_ASSERT(guardedMutex.count.load() == 1, "Initial count must be 1");
        TEST_ASSERT(guardedMutex.owner == 0, "Initial owner must be 0");

        // Acquire guarded mutex
        ex::KeAcquireGuardedMutex(&guardedMutex);
        TEST_ASSERT(guardedMutex.count.load() == 0, "count must be 0 when held");
        TEST_ASSERT(guardedMutex.owner != 0, "owner must be set");
        TEST_ASSERT(ke::KeAreApcsDisabled() == true, "Special APCs must be disabled in guarded region");

        // Release guarded mutex
        ex::KeReleaseGuardedMutex(&guardedMutex);
        TEST_ASSERT(guardedMutex.count.load() == 1, "count must be 1 after release");
        TEST_ASSERT(guardedMutex.owner == 0, "owner must be cleared");
        TEST_ASSERT(ke::KeAreApcsDisabled() == false, "Special APCs must be restored");

        // Try acquire
        BOOLEAN ok = ex::KeTryAcquireGuardedMutex(&guardedMutex);
        TEST_ASSERT(ok == TRUE, "KeTryAcquireGuardedMutex must succeed on free mutex");
        ex::KeReleaseGuardedMutex(&guardedMutex);

        // GuardedMutexGuard RAII
        {
            ex::GuardedMutexGuard guard(&guardedMutex);
            TEST_ASSERT(guardedMutex.owner != 0, "GuardedMutexGuard must acquire");
            TEST_ASSERT(ke::KeAreApcsDisabled() == true, "APCs disabled while guard in scope");
        }
        TEST_ASSERT(guardedMutex.owner == 0, "GuardedMutexGuard dtor must release");
        TEST_ASSERT(ke::KeAreApcsDisabled() == false, "APCs restored after guard dtor");
    }

    // 5. Lookaside List Dynamic Auto-Tuning
    {
        ex::NPagedLookasideList lookasideList;
        lookasideList.initialize(64, micant::ex::makePoolTag('L', 'O', 'O', 'K'), 128);

        void* b1 = lookasideList.allocate();
        void* b2 = lookasideList.allocate();
        TEST_ASSERT(b1 != nullptr && b2 != nullptr, "Lookaside allocate must succeed");
        lookasideList.free(b1);
        lookasideList.free(b2);

        // Dynamic tuning adjustment invocation
        lookasideList.adjustDepth();

        // C-style Lookaside
        ex::NPAGED_LOOKASIDE_LIST cLookaside{};
        ex::ExInitializeNPagedLookasideList(&cLookaside, nullptr, nullptr, 0, 128, micant::ex::makePoolTag('N', 'P', 'L', 'S'), 4);
        void* cb1 = ex::ExAllocateFromNPagedLookasideList(&cLookaside);
        TEST_ASSERT(cb1 != nullptr, "ExAllocateFromNPagedLookasideList must succeed");
        ex::ExFreeToNPagedLookasideList(&cLookaside, cb1);
        ex::ExAdjustLookasideDepth(&cLookaside.l);
        ex::ExDeleteNPagedLookasideList(&cLookaside);
    }

    // 6. User-Buffer Probing Engine (ProbeForRead & ProbeForWrite)
    {
        // Valid user buffer on stack
        int stackVal = 0x12345678;
        NtStatus st = mm::ProbeForRead(&stackVal, sizeof(stackVal), alignof(int), mm::UserMode);
        TEST_ASSERT(NT_SUCCESS(st), "ProbeForRead on valid user stack buffer must succeed");

        st = mm::ProbeForWrite(&stackVal, sizeof(stackVal), alignof(int), mm::UserMode);
        TEST_ASSERT(NT_SUCCESS(st), "ProbeForWrite on valid user stack buffer must succeed");

        // Null pointer probe
        st = mm::ProbeForRead(nullptr, 4, 4, mm::UserMode);
        TEST_ASSERT(st == NtStatus::AccessViolation, "Null pointer probe must return STATUS_ACCESS_VIOLATION");

        // 64KB Null trap boundary (below MM_LOWEST_USER_ADDRESS)
        void* nullTrapAddr = reinterpret_cast<void*>(0x8000ULL);
        st = mm::ProbeForRead(nullTrapAddr, 4, 4, mm::UserMode);
        TEST_ASSERT(st == NtStatus::AccessViolation, "Probe below MM_LOWEST_USER_ADDRESS must return STATUS_ACCESS_VIOLATION");

        // Misaligned pointer
        uintptr_t unaligned = reinterpret_cast<uintptr_t>(&stackVal) | 1;
        st = mm::ProbeForRead(reinterpret_cast<void*>(unaligned), 4, 4, mm::UserMode);
        TEST_ASSERT(st == NtStatus::DatatypeMisalignment, "Unaligned probe must return STATUS_DATATYPE_MISALIGNMENT");

        // Kernel-mode address in UserMode
        void* kernelAddr = reinterpret_cast<void*>(0xFFFFF80000000000ULL);
        st = mm::ProbeForRead(kernelAddr, 64, 8, mm::UserMode);
        TEST_ASSERT(st == NtStatus::AccessViolation, "Kernel space address in UserMode must return STATUS_ACCESS_VIOLATION");

        // Kernel-mode address in KernelMode bypass
        st = mm::ProbeForRead(kernelAddr, 64, 8, mm::KernelMode);
        TEST_ASSERT(NT_SUCCESS(st), "Kernel address probe in KernelMode must succeed");

        // Address overflow check
        void* overflowAddr = reinterpret_cast<void*>(0xFFFFFFFFFFFFFFF0ULL);
        st = mm::ProbeForRead(overflowAddr, 32, 1, mm::UserMode);
        TEST_ASSERT(st == NtStatus::AccessViolation, "Address wrap-around overflow must return STATUS_ACCESS_VIOLATION");

        // Synthetic page permission write check
        uintptr_t mockRoPage = 0x0000000020000000ULL;
        mm::ProbeMemoryManager::get().setPagePermission(mockRoPage, true, false); // Read-only
        st = mm::ProbeForWrite(reinterpret_cast<void*>(mockRoPage), 128, 4, mm::UserMode);
        TEST_ASSERT(st == NtStatus::AccessViolation, "ProbeForWrite to read-only page must return STATUS_ACCESS_VIOLATION");
        mm::ProbeMemoryManager::get().clearPermissions();
    }

    // 7. Structured Exception Safe Copy Engine
    {
        // Safe copy from user
        char userSource[32] = "MicaNT Sovereign Executive Sync";
        char kernelBuffer[32]{};
        NtStatus st = ex::ExSafeCopyFromUser(kernelBuffer, userSource, sizeof(userSource), mm::UserMode);
        TEST_ASSERT(NT_SUCCESS(st), "ExSafeCopyFromUser must succeed");
        TEST_ASSERT(std::memcmp(kernelBuffer, userSource, sizeof(userSource)) == 0, "Copied content must match");

        // Safe copy to user
        char kernelSource[32] = "Kernel To User Response Data";
        char userDest[32]{};
        st = ex::ExSafeCopyToUser(userDest, kernelSource, sizeof(kernelSource), mm::UserMode);
        TEST_ASSERT(NT_SUCCESS(st), "ExSafeCopyToUser must succeed");
        TEST_ASSERT(std::memcmp(userDest, kernelSource, sizeof(kernelSource)) == 0, "Dest content must match");

        // Safe copy with null pointer
        st = ex::ExSafeCopyFromUser(kernelBuffer, nullptr, 16, mm::UserMode);
        TEST_ASSERT(st == NtStatus::AccessViolation, "Safe copy with null src must return STATUS_ACCESS_VIOLATION");

        // Scalar probe & read
        uint64_t userVal64 = 0xFEEDFACECAFEBEEFULL;
        uint64_t readOut64 = 0;
        st = ex::ExSafeProbeAndRead<uint64_t>(&userVal64, readOut64, mm::UserMode);
        TEST_ASSERT(NT_SUCCESS(st), "ExSafeProbeAndRead must succeed");
        TEST_ASSERT(readOut64 == userVal64, "Read scalar value must match");

        // Scalar probe & write
        uint64_t writeTarget64 = 0;
        st = ex::ExSafeProbeAndWrite<uint64_t>(&writeTarget64, 0x1122334455667788ULL, mm::UserMode);
        TEST_ASSERT(NT_SUCCESS(st), "ExSafeProbeAndWrite must succeed");
        TEST_ASSERT(writeTarget64 == 0x1122334455667788ULL, "Written scalar value must match");

        // Safe read UNICODE_STRING
        const wchar_t testWide[] = L"\\Device\\HarddiskVolume1\\MicaNT.sys";
        UnicodeString uStr(testWide);
        std::wstring outWide;
        st = ex::ExSafeReadUnicodeString(&uStr, outWide, 1024, mm::UserMode);
        TEST_ASSERT(NT_SUCCESS(st), "ExSafeReadUnicodeString must succeed");
        TEST_ASSERT(outWide == testWide, "Read Unicode string must match input");

        // Tampered UNICODE_STRING length > maximumLength
        UnicodeString tamperedStr = uStr;
        tamperedStr.length = tamperedStr.maximumLength + 10;
        st = ex::ExSafeReadUnicodeString(&tamperedStr, outWide, 1024, mm::UserMode);
        TEST_ASSERT(st == NtStatus::InvalidParameter, "Tampered string with Length > MaximumLength must return STATUS_INVALID_PARAMETER");

        // Safe read ANSI_STRING
        const char testAnsi[] = "MicaNT Driver Verification Complete";
        AnsiString aStr(testAnsi);
        std::string outAnsi;
        st = ex::ExSafeReadAnsiString(&aStr, outAnsi, 2048, mm::UserMode);
        TEST_ASSERT(NT_SUCCESS(st), "ExSafeReadAnsiString must succeed");
        TEST_ASSERT(outAnsi == testAnsi, "Read ANSI string must match input");
    }

    // 8. Executive Synchronization & Probe Telemetry Audit
    {
        auto telem = ex::ExQuerySyncTelemetry();
        TEST_ASSERT(telem.totalSharedAcquisitions >= 3, "totalSharedAcquisitions must be >= 3");
        TEST_ASSERT(telem.totalExclusiveAcquisitions >= 2, "totalExclusiveAcquisitions must be >= 2");
        TEST_ASSERT(telem.totalResourceConversions >= 2, "totalResourceConversions must be >= 2");
        TEST_ASSERT(telem.totalResourceTryAcquireFailures >= 2, "totalResourceTryAcquireFailures must be >= 2");
        TEST_ASSERT(telem.totalPushLocksAcquiredExclusive >= 2, "totalPushLocksAcquiredExclusive must be >= 2");
        TEST_ASSERT(telem.totalPushLocksAcquiredShared >= 3, "totalPushLocksAcquiredShared must be >= 3");
        TEST_ASSERT(telem.totalPushLockConversions >= 1, "totalPushLockConversions must be >= 1");
        TEST_ASSERT(telem.totalGuardedMutexAcquisitions >= 2, "totalGuardedMutexAcquisitions must be >= 2");
        TEST_ASSERT(telem.totalProbesValidated >= 5, "totalProbesValidated must be >= 5");
        TEST_ASSERT(telem.totalProbeViolationsCaught >= 5, "totalProbeViolationsCaught must be >= 5");
        TEST_ASSERT(telem.totalSafeCopiesCompleted >= 5, "totalSafeCopiesCompleted must be >= 5");
        TEST_ASSERT(telem.totalSafeCopyFaultsCaught >= 2, "totalSafeCopyFaultsCaught must be >= 2");
        TEST_ASSERT(telem.totalLookasideAllocations >= 2, "totalLookasideAllocations must be >= 2");
        TEST_ASSERT(telem.totalLookasideDynamicAdjustments >= 2, "totalLookasideDynamicAdjustments must be >= 2");

        // Reset telemetry for testing
        ex::ExResetSyncTelemetryForTesting();
        auto resetTelem = ex::ExQuerySyncTelemetry();
        TEST_ASSERT(resetTelem.totalSharedAcquisitions == 0, "totalSharedAcquisitions reset must be 0");
        TEST_ASSERT(resetTelem.totalExclusiveAcquisitions == 0, "totalExclusiveAcquisitions reset must be 0");
        TEST_ASSERT(resetTelem.totalProbesValidated == 0, "totalProbesValidated reset must be 0");
    }

    std::cout << "[TEST] Suite 243: Horizon 27 - Sovereign Executive Synchronization & Buffer Probing PASSED.\n";
}

// ============================================================================
// Suite 244: Horizon 28 - Sovereign HAL Interrupt Architecture, APIC, HPET & RTC Clocks,
//            CR8/TPR Hardware Masking & TSC Deadline Timer Synchronization
// ============================================================================
inline void Test_Horizon28_HardwareAbstractionLayer_Interrupts_APIC_HPET_And_Timers() {
    std::cout << "[TEST] Suite 244: Running Sovereign HAL Interrupt Architecture, APIC, HPET & RTC Clocks...\n";

    // Initialize HAL
    hal::HardwareAbstractionLayer::get().initialize(4, hal::ProcessorArchitecture::Amd64, 3600);
    hal::HalResetTelemetryForTesting();

    auto* lapic0 = hal::HardwareAbstractionLayer::get().getLocalApic(0);
    TEST_ASSERT(lapic0 != nullptr, "LAPIC for CPU 0 must exist");

    // 1. APIC CR8 / TPR Hardware Masking & Priority Classes
    {
        TEST_ASSERT(lapic0->getCr8() == 0, "Initial CR8 must be 0");
        TEST_ASSERT(lapic0->getTaskPriority() == 0, "Initial TPR must be 0");

        // Priority classes: vector 0x20 is class 2, vector 0x35 is class 3
        TEST_ASSERT(!lapic0->isVectorMaskedByTpr(0x20), "Vector 0x20 must not be masked at CR8 0");
        TEST_ASSERT(!lapic0->isVectorMaskedByTpr(0x35), "Vector 0x35 must not be masked at CR8 0");

        // Set CR8 to 2 (TPR = 0x20)
        hal::HalSetCr8(2, 0);
        TEST_ASSERT(hal::HalGetCr8(0) == 2, "CR8 must be 2");
        TEST_ASSERT(lapic0->getTaskPriority() == 0x20, "TPR must be 0x20");

        // Vector 0x20 and 0x2F (class 2) must be masked because class 2 <= TPR class 2
        TEST_ASSERT(lapic0->isVectorMaskedByTpr(0x20), "Vector 0x20 must be masked when CR8 is 2");
        TEST_ASSERT(lapic0->isVectorMaskedByTpr(0x2F), "Vector 0x2F must be masked when CR8 is 2");
        // Vector 0x30 and 0x35 (class 3) must be unmasked because class 3 > TPR class 2
        TEST_ASSERT(!lapic0->isVectorMaskedByTpr(0x30), "Vector 0x30 must be unmasked when CR8 is 2");
        TEST_ASSERT(!lapic0->isVectorMaskedByTpr(0x35), "Vector 0x35 must be unmasked when CR8 is 2");

        // Request vectors in IRR
        TEST_ASSERT(lapic0->getHighestPendingVector() == 0, "Highest pending must be 0 when IRR is empty");
        lapic0->requestInterrupt(0x20);
        lapic0->requestInterrupt(0x45);
        TEST_ASSERT(lapic0->isInterruptPending(0x20), "Vector 0x20 must be pending");
        TEST_ASSERT(lapic0->isInterruptPending(0x45), "Vector 0x45 must be pending");

        // Highest pending vector is 0x45
        TEST_ASSERT(lapic0->getHighestPendingVector() == 0x45, "Highest pending vector must be 0x45");
        // Highest deliverable vector with CR8 = 2 is 0x45 (class 4 > 2)
        TEST_ASSERT(lapic0->getHighestDeliverableVector() == 0x45, "Highest deliverable vector must be 0x45");

        // Raise CR8 to 4 (TPR = 0x40): 0x45 (class 4) is now masked (4 <= 4)
        hal::HalSetCr8(4, 0);
        TEST_ASSERT(lapic0->getHighestPendingVector() == 0x45, "Highest pending vector must still be 0x45");
        TEST_ASSERT(lapic0->getHighestDeliverableVector() == 0, "Highest deliverable vector must be 0 when masked");

        // Raise CR8 to 15 (0xF = HIGH_LEVEL): masks all interrupt classes
        hal::HalSetCr8(15, 0);
        TEST_ASSERT(lapic0->getTaskPriority() == 0xF0, "TPR must be 0xF0 at CR8 15");
        TEST_ASSERT(lapic0->isVectorMaskedByTpr(0xFF), "Vector 0xFF must be masked at CR8 15");

        // Lower CR8 back to 0 (PASSIVE_LEVEL)
        hal::HalSetCr8(0, 0);
        TEST_ASSERT(lapic0->getHighestDeliverableVector() == 0x45, "Vector 0x45 must be deliverable at CR8 0");

        // Service interrupt 0x45
        lapic0->markInterruptInService(0x45);
        TEST_ASSERT(lapic0->isInterruptInService(0x45), "Vector 0x45 must be in service");
        TEST_ASSERT(!lapic0->isInterruptPending(0x45), "Vector 0x45 must no longer be in IRR");
        lapic0->signalEoi();
        TEST_ASSERT(!lapic0->isInterruptInService(0x45), "Vector 0x45 must be cleared from ISR on EOI");

        // Service interrupt 0x20
        lapic0->markInterruptInService(0x20);
        lapic0->signalEoi();
        TEST_ASSERT(!lapic0->isInterruptInService(0x20), "Vector 0x20 cleared");
    }

    // 2. TSC Deadline Timer Synchronization (IA32_TSC_DEADLINE_MSR 0x6E0)
    {
        lapic0->setTimerMode(hal::ApicTimerMode::TscDeadline);
        TEST_ASSERT(lapic0->getTimerMode() == hal::ApicTimerMode::TscDeadline, "Timer mode must be TscDeadline");

        lapic0->setTimerVector(0x55);
        TEST_ASSERT(lapic0->getTimerVector() == 0x55, "Timer vector must be 0x55");
        TEST_ASSERT(!lapic0->isTimerMasked(), "Timer must not be masked by default");

        // Arm deadline for TSC = 5,000,000
        lapic0->writeTscDeadline(5'000'000ULL);
        TEST_ASSERT(lapic0->readTscDeadline() == 5'000'000ULL, "Deadline must be 5,000,000");

        // Check before deadline: current TSC = 4,999,999
        bool expired = hal::HalCheckTscDeadline(0, 4'999'999ULL);
        TEST_ASSERT(!expired, "Deadline must not fire before target count");
        TEST_ASSERT(lapic0->readTscDeadline() == 5'000'000ULL, "Deadline must remain armed");
        TEST_ASSERT(!lapic0->isInterruptPending(0x55), "Vector 0x55 must not be pending in IRR");

        // Check at/after deadline: current TSC = 5,000,000
        expired = hal::HalCheckTscDeadline(0, 5'000'000ULL);
        TEST_ASSERT(expired, "Deadline must fire when TSC reaches target count");
        TEST_ASSERT(lapic0->readTscDeadline() == 0, "Deadline must auto-disarm to 0 on expiry");
        TEST_ASSERT(lapic0->isInterruptPending(0x55), "Vector 0x55 must be pending in IRR");

        // Service vector 0x55
        lapic0->markInterruptInService(0x55);
        lapic0->signalEoi();

        // Test masked timer deadline expiry
        lapic0->setTimerMasked(true);
        TEST_ASSERT(lapic0->isTimerMasked(), "Timer must be masked");
        lapic0->writeTscDeadline(8'000'000ULL);
        expired = hal::HalCheckTscDeadline(0, 8'000'001ULL);
        TEST_ASSERT(expired, "Masked deadline still disarms upon passage of deadline");
        TEST_ASSERT(!lapic0->isInterruptPending(0x55), "Masked timer must not set IRR");
        lapic0->setTimerMasked(false);
    }

    // 3. HPET Timer, Periodic Stepping & Comparator Routing
    {
        auto& hpet = hal::HalGetHpetTimer();
        hpet.reset();
        TEST_ASSERT(hpet.getBaseAddress() == hal::DEFAULT_HPET_BASE, "HPET base must be 0xFED00000");
        TEST_ASSERT(hpet.getPeriodFemtoseconds() == 100'000'000ULL, "HPET period must be 100,000,000 fs");
        TEST_ASSERT(!hpet.isEnabled(), "HPET must be disabled initially");

        // Comparator 0: non-periodic, match = 1000, vector = 0x60
        bool cfg0 = hpet.configureComparator(0, 1000, false, 0, 0x60, true);
        TEST_ASSERT(cfg0, "Configure comparator 0 must succeed");

        // Comparator 1: periodic, match = 400, interval = 400, vector = 0x61
        bool cfg1 = hpet.configureComparator(1, 400, true, 400, 0x61, true);
        TEST_ASSERT(cfg1, "Configure comparator 1 must succeed");

        hpet.setEnabled(true);
        TEST_ASSERT(hpet.isEnabled(), "HPET must be enabled");

        // Step by 300 ticks (counter = 300)
        uint32_t trig = hal::HalStepHpet(300);
        TEST_ASSERT(trig == 0, "Neither comparator should trigger at counter 300");
        TEST_ASSERT(hpet.readCounter() == 300, "Counter must be 300");
        TEST_ASSERT(!hpet.isComparatorInterruptTriggered(0), "Comp 0 not triggered");
        TEST_ASSERT(!hpet.isComparatorInterruptTriggered(1), "Comp 1 not triggered");

        // Step by 200 ticks (counter reaches 500)
        trig = hal::HalStepHpet(200);
        TEST_ASSERT(trig == 1, "Comparator 1 must trigger at counter 500");
        TEST_ASSERT(hpet.readCounter() == 500, "Counter must be 500");
        TEST_ASSERT(!hpet.isComparatorInterruptTriggered(0), "Comp 0 not triggered yet");
        TEST_ASSERT(hpet.isComparatorInterruptTriggered(1), "Comp 1 triggered");
        // Comparator 1 periodic accumulator advances match to 800
        auto* c1 = hpet.getComparator(1);
        TEST_ASSERT(c1 && c1->comparatorValue == 800, "Comp 1 periodic match value reloaded to 800");
        hpet.clearComparatorInterrupt(1);

        // Step by 600 ticks (counter reaches 1100): passes 800 (comp 1) and 1000 (comp 0)
        trig = hal::HalStepHpet(600);
        TEST_ASSERT(trig == 2, "Both comparators must trigger between 500 and 1100");
        TEST_ASSERT(hpet.readCounter() == 1100, "Counter must be 1100");
        TEST_ASSERT(hpet.isComparatorInterruptTriggered(0), "Comp 0 triggered");
        TEST_ASSERT(hpet.isComparatorInterruptTriggered(1), "Comp 1 triggered");

        // TSC Frequency Calibration via HPET
        // 100,000 HPET ticks @ 100 ns = 10 ms = 10^13 fs. 36,000,000 TSC ticks -> 3.6 GHz
        uint64_t calHz = hpet.calibrateTscFrequency(36'000'000ULL, 100'000ULL);
        TEST_ASSERT(calHz == 3'600'000'000ULL, "TSC calibration must yield 3.6 GHz");

        // 25,000,000 TSC ticks in 100,000 HPET ticks -> 2.5 GHz
        calHz = hpet.calibrateTscFrequency(25'000'000ULL, 100'000ULL);
        TEST_ASSERT(calHz == 2'500'000'000ULL, "TSC calibration must yield 2.5 GHz");
    }

    // 4. MSI Message Encoding & Delivery
    {
        auto msi = hal::MsiMessage::create(0, 0x72, hal::IpiDeliveryMode::Fixed, hal::InterruptMode::Latched);
        TEST_ASSERT(msi.getTargetApicId() == 0, "MSI target APIC ID must be 0");
        TEST_ASSERT(msi.getVector() == 0x72, "MSI vector must be 0x72");
        TEST_ASSERT(msi.getDeliveryMode() == hal::IpiDeliveryMode::Fixed, "MSI delivery mode must be Fixed");
        TEST_ASSERT(msi.getTriggerMode() == hal::InterruptMode::Latched, "MSI trigger mode must be Latched");
        TEST_ASSERT((msi.address & 0xFFF00000ULL) == 0xFEE00000ULL, "MSI base MMIO address must be FEE00000");

        bool delivered = hal::HalDeliverMsi(msi);
        TEST_ASSERT(delivered, "HalDeliverMsi must return true");
        TEST_ASSERT(lapic0->isInterruptPending(0x72), "Vector 0x72 must be pending in LAPIC 0 IRR");

        // Service interrupt
        lapic0->markInterruptInService(0x72);
        lapic0->signalEoi();
        TEST_ASSERT(!lapic0->isInterruptInService(0x72), "Vector 0x72 cleared");
    }

    // 5. RTC BCD & NT Epoch Bidirectional Conversions
    {
        // Leap year checks
        TEST_ASSERT(hal::HalpIsLeapYear(2024), "2024 is a leap year");
        TEST_ASSERT(!hal::HalpIsLeapYear(2025), "2025 is not a leap year");
        TEST_ASSERT(!hal::HalpIsLeapYear(2026), "2026 is not a leap year");
        TEST_ASSERT(hal::HalpIsLeapYear(2000), "2000 is a leap year (400-year rule)");
        TEST_ASSERT(!hal::HalpIsLeapYear(1900), "1900 is not a leap year (100-year rule)");
        TEST_ASSERT(hal::HalpIsLeapYear(2400), "2400 is a leap year");

        // Days in month
        TEST_ASSERT(hal::HalpDaysInMonth(2024, 2) == 29, "Feb 2024 has 29 days");
        TEST_ASSERT(hal::HalpDaysInMonth(2025, 2) == 28, "Feb 2025 has 28 days");
        TEST_ASSERT(hal::HalpDaysInMonth(2026, 4) == 30, "Apr 2026 has 30 days");
        TEST_ASSERT(hal::HalpDaysInMonth(2026, 10) == 31, "Oct 2026 has 31 days");

        // BCD conversions
        TEST_ASSERT(hal::RtcBinaryToBcd(15) == 0x15, "15 -> 0x15 BCD");
        TEST_ASSERT(hal::RtcBcdToBinary(0x15) == 15, "0x15 BCD -> 15");
        TEST_ASSERT(hal::RtcBinaryToBcd(59) == 0x59, "59 -> 0x59 BCD");
        TEST_ASSERT(hal::RtcBcdToBinary(0x59) == 59, "0x59 BCD -> 59");
        TEST_ASSERT(hal::RtcBinaryToBcd(0) == 0x00, "0 -> 0x00 BCD");
        TEST_ASSERT(hal::RtcBcdToBinary(0x00) == 0, "0x00 BCD -> 0");

        // NT Epoch Origin (1601-01-01 00:00:00.000, Monday)
        hal::TIME_FIELDS origin{1601, 1, 1, 0, 0, 0, 0, 1};
        LargeInteger stOrigin{};
        bool ok = hal::HalTimeFieldsToSystemTime(&origin, &stOrigin);
        TEST_ASSERT(ok, "HalTimeFieldsToSystemTime for 1601 origin must succeed");
        TEST_ASSERT(stOrigin.quadPart == 0, "1601 origin quadPart must be exactly 0");

        hal::TIME_FIELDS rtOrigin{};
        ok = hal::HalSystemTimeToTimeFields(&stOrigin, &rtOrigin);
        TEST_ASSERT(ok, "HalSystemTimeToTimeFields for 0 must succeed");
        TEST_ASSERT(rtOrigin.year == 1601 && rtOrigin.month == 1 && rtOrigin.day == 1, "Origin date must match");
        TEST_ASSERT(rtOrigin.hour == 0 && rtOrigin.minute == 0 && rtOrigin.second == 0 && rtOrigin.milliseconds == 0, "Origin time must be 0");
        TEST_ASSERT(rtOrigin.weekday == 1, "1601-01-01 was a Monday (weekday=1)");

        // Modern Date Round-Trip (2026-10-10 14:30:45.125, Saturday)
        hal::TIME_FIELDS modern{2026, 10, 10, 14, 30, 45, 125, 6};
        LargeInteger stModern{};
        ok = hal::HalTimeFieldsToSystemTime(&modern, &stModern);
        TEST_ASSERT(ok, "HalTimeFieldsToSystemTime for modern timestamp must succeed");
        TEST_ASSERT(stModern.quadPart > 0, "Modern timestamp must be > 0");

        hal::TIME_FIELDS rtModern{};
        ok = hal::HalSystemTimeToTimeFields(&stModern, &rtModern);
        TEST_ASSERT(ok, "HalSystemTimeToTimeFields for modern timestamp must succeed");
        TEST_ASSERT(rtModern.year == 2026 && rtModern.month == 10 && rtModern.day == 10, "Modern year, month, day must match");
        TEST_ASSERT(rtModern.hour == 14 && rtModern.minute == 30 && rtModern.second == 45, "Modern time must match");
        TEST_ASSERT(rtModern.milliseconds == 125, "Modern milliseconds must match");
        TEST_ASSERT(rtModern.weekday == 6, "2026-10-10 must be Saturday (weekday=6)");

        // Leap Day Round-Trip (2024-02-29 23:59:59.999)
        hal::TIME_FIELDS leapDate{2024, 2, 29, 23, 59, 59, 999, 4};
        LargeInteger stLeap{};
        ok = hal::HalTimeFieldsToSystemTime(&leapDate, &stLeap);
        TEST_ASSERT(ok, "HalTimeFieldsToSystemTime for leap day must succeed");

        hal::TIME_FIELDS rtLeap{};
        ok = hal::HalSystemTimeToTimeFields(&stLeap, &rtLeap);
        TEST_ASSERT(ok, "HalSystemTimeToTimeFields for leap day must succeed");
        TEST_ASSERT(rtLeap.year == 2024 && rtLeap.month == 2 && rtLeap.day == 29, "Leap day must roundtrip");
        TEST_ASSERT(rtLeap.hour == 23 && rtLeap.minute == 59 && rtLeap.second == 59 && rtLeap.milliseconds == 999, "Time must match");

        // Invalid date rejection
        hal::TIME_FIELDS invalidFeb{2025, 2, 29, 12, 0, 0, 0, 0};
        LargeInteger stInv{};
        TEST_ASSERT(!hal::HalTimeFieldsToSystemTime(&invalidFeb, &stInv), "Feb 29 in non-leap year must be rejected");

        hal::TIME_FIELDS invalidMonth{2026, 13, 1, 0, 0, 0, 0, 0};
        TEST_ASSERT(!hal::HalTimeFieldsToSystemTime(&invalidMonth, &stInv), "Month 13 must be rejected");

        // Query and set real-time clock
        hal::TIME_FIELDS rtcQuery{};
        hal::HalQueryRealTimeClock(&rtcQuery);
        TEST_ASSERT(rtcQuery.year == 2026, "Default simulated RTC year must be 2026");
        rtcQuery.year = 2027;
        hal::HalSetRealTimeClock(&rtcQuery);
        hal::TIME_FIELDS rtcUpdated{};
        hal::HalQueryRealTimeClock(&rtcUpdated);
        TEST_ASSERT(rtcUpdated.year == 2027, "Updated RTC year must be 2027");
    }

    // 6. KINTERRUPT Chaining & Priority Handling
    {
        uint32_t isrCallCountA = 0;
        uint32_t isrCallCountB = 0;

        hal::KINTERRUPT intA{};
        intA.vector = 0x85;
        intA.irql = ke::DIRQL_MIN;
        intA.synchronizeIrql = ke::DIRQL_MIN + 2;
        intA.shared = true;
        intA.isr = [&](hal::KINTERRUPT* self, void* ctx) -> bool {
            (void)self; (void)ctx;
            isrCallCountA++;
            return false; // Handled by second device
        };

        hal::KINTERRUPT intB{};
        intB.vector = 0x85;
        intB.irql = ke::DIRQL_MIN;
        intB.synchronizeIrql = ke::DIRQL_MIN + 2;
        intB.shared = true;
        intB.isr = [&](hal::KINTERRUPT* self, void* ctx) -> bool {
            (void)self; (void)ctx;
            isrCallCountB++;
            return true; // Successfully handled
        };

        TEST_ASSERT(hal::HalConnectInterrupt(&intA), "HalConnectInterrupt for intA must succeed");
        TEST_ASSERT(hal::HalConnectInterrupt(&intB), "HalConnectInterrupt for intB must succeed");
        TEST_ASSERT(!hal::HalConnectInterrupt(&intA), "Reconnecting already connected interrupt must fail");

        // Non-shared interrupt cannot join shared vector
        hal::KINTERRUPT intC{};
        intC.vector = 0x85;
        intC.shared = false;
        intC.isr = [](hal::KINTERRUPT*, void*) { return true; };
        TEST_ASSERT(!hal::HalConnectInterrupt(&intC), "Non-shared interrupt cannot join shared vector");

        // Dispatch interrupt 0x85: intA called, returns false -> intB called, returns true
        bool handled = hal::HalDispatchInterrupt(0x85);
        TEST_ASSERT(handled, "HalDispatchInterrupt must return true when handled by chained ISR");
        TEST_ASSERT(isrCallCountA == 1, "ISR A must have been called");
        TEST_ASSERT(isrCallCountB == 1, "ISR B must have been called");
        TEST_ASSERT(intA.interruptCount == 1, "intA interruptCount must be 1");
        TEST_ASSERT(intB.interruptCount == 1, "intB interruptCount must be 1");

        // Disconnect intB: now only intA is registered
        TEST_ASSERT(hal::HalDisconnectInterrupt(&intB), "HalDisconnectInterrupt for intB must succeed");
        handled = hal::HalDispatchInterrupt(0x85);
        TEST_ASSERT(!handled, "HalDispatchInterrupt must return false when ISR returns false");
        TEST_ASSERT(isrCallCountA == 2, "ISR A called a second time");
        TEST_ASSERT(isrCallCountB == 1, "ISR B must not have been called after disconnect");

        // Disconnect intA: vector is now empty
        TEST_ASSERT(hal::HalDisconnectInterrupt(&intA), "HalDisconnectInterrupt for intA must succeed");
        handled = hal::HalDispatchInterrupt(0x85);
        TEST_ASSERT(!handled, "HalDispatchInterrupt on empty vector must return false");
    }

    // 7. HAL Telemetry Audit & Reset
    {
        hal::HAL_TELEMETRY telem{};
        hal::HalQueryTelemetry(&telem);
        TEST_ASSERT(telem.totalHardwareInterrupts > 0, "totalHardwareInterrupts must be > 0");
        TEST_ASSERT(telem.totalSpuriousInterrupts > 0, "totalSpuriousInterrupts must be > 0");
        TEST_ASSERT(telem.totalTscDeadlineExpiries > 0, "totalTscDeadlineExpiries must be > 0");
        TEST_ASSERT(telem.totalHpetTicks > 0, "totalHpetTicks must be > 0");
        TEST_ASSERT(telem.totalMsiDelivered > 0, "totalMsiDelivered must be > 0");
        TEST_ASSERT(telem.totalIrqlRaises > 0, "totalIrqlRaises must be > 0");
        TEST_ASSERT(telem.totalIrqlLowers > 0, "totalIrqlLowers must be > 0");
        TEST_ASSERT(telem.totalRtcSyncs > 0, "totalRtcSyncs must be > 0");

        // Reset telemetry
        hal::HalResetTelemetryForTesting();
        hal::HAL_TELEMETRY resetTelem{};
        hal::HalQueryTelemetry(&resetTelem);
        TEST_ASSERT(resetTelem.totalHardwareInterrupts == 0, "totalHardwareInterrupts must be reset to 0");
        TEST_ASSERT(resetTelem.totalSpuriousInterrupts == 0, "totalSpuriousInterrupts must be reset to 0");
        TEST_ASSERT(resetTelem.totalTscDeadlineExpiries == 0, "totalTscDeadlineExpiries must be reset to 0");
        TEST_ASSERT(resetTelem.totalHpetTicks == 0, "totalHpetTicks must be reset to 0");
        TEST_ASSERT(resetTelem.totalMsiDelivered == 0, "totalMsiDelivered must be reset to 0");
    }

    std::cout << "[TEST] Suite 244: Horizon 28 - Sovereign HAL Interrupt Architecture, APIC, HPET & RTC Clocks PASSED.\n";
}

// ============================================================================
// Suite 245: Horizon 29 - Plug and Play (PnP) Manager Architecture,
// DevNode Hierarchy (PDO/FDO/FiDO), IRP_MN PnP State Engine & Device Interfaces
// ============================================================================
void Test_Horizon29_PlugAndPlayManager_DeviceTree_IrpStateEngine_And_Interfaces() {
    auto& pnpMgr = pnp::PnpManager::get();
    pnpMgr.resetForTesting();

    // 1. Device Tree Hierarchy & Root DevNode
    {
        auto root = pnpMgr.getRootNode();
        TEST_ASSERT(root != nullptr, "Root DevNode must exist");
        TEST_ASSERT(root->getDeviceId() == L"HTREE\\ROOT\\0", "Root DevNode ID must be HTREE\\ROOT\\0");
        TEST_ASSERT(root->getState() == pnp::PnpDeviceState::Started, "Root DevNode must be in Started state");

        // Create simulated PCI bus root PDO
        io::DriverObject pciBusDriver{};
        pciBusDriver.driverName = L"pci.sys";

        io::DeviceObject pciBusPdo{};
        pciBusPdo.driverObject = &pciBusDriver;
        pciBusPdo.deviceName = L"\\Device\\PciBus0";

        auto pciBusDevNode = pnpMgr.createDeviceNode(root.get(), &pciBusPdo, L"PCI\\ROOT\\0", L"0");
        TEST_ASSERT(pciBusDevNode != nullptr, "PCI Bus DevNode creation must succeed");
        TEST_ASSERT(pciBusDevNode->getParent() == root.get(), "PCI Bus DevNode parent must be root");
        TEST_ASSERT(pciBusDevNode->getState() == pnp::PnpDeviceState::Initialized, "Newly created DevNode must be Initialized");
        TEST_ASSERT(root->getChildren().size() == 1, "Root DevNode should have 1 child");

        // Create child device PDO on PCI bus (e.g. e1000 Gigabit NIC)
        io::DeviceObject nicPdo{};
        nicPdo.driverObject = &pciBusDriver;
        nicPdo.deviceName = L"\\Device\\PciNic0";

        auto nicDevNode = pnpMgr.createDeviceNode(pciBusDevNode.get(), &nicPdo, L"PCI\\VEN_8086&DEV_100E&SUBSYS_001E8086&REV_02", L"0000");
        TEST_ASSERT(nicDevNode != nullptr, "NIC DevNode creation must succeed");
        TEST_ASSERT(nicDevNode->getParent() == pciBusDevNode.get(), "NIC DevNode parent must be PCI Bus");
        TEST_ASSERT(pciBusDevNode->getChildren().size() == 1, "PCI Bus must have 1 child");

        nicDevNode->setDeviceDescription(L"Intel(R) PRO/1000 MT Network Connection");
        nicDevNode->addHardwareId(L"PCI\\VEN_8086&DEV_100E&SUBSYS_001E8086");
        nicDevNode->addHardwareId(L"PCI\\VEN_8086&DEV_100E");
        nicDevNode->addCompatibleId(L"PCI\\VEN_8086");
        nicDevNode->addCompatibleId(L"PCI\\CC_020000");

        TEST_ASSERT(nicDevNode->getHardwareIds().size() == 2, "NIC DevNode must have 2 hardware IDs");
        TEST_ASSERT(nicDevNode->getCompatibleIds().size() == 2, "NIC DevNode must have 2 compatible IDs");
    }

    // 2. AddDevice & Layered Driver Stacking (PDO -> FDO -> Upper Filter)
    io::DriverObject nicDriver{};
    nicDriver.driverName = L"e1000.sys";

    io::DeviceObject nicFdo{};
    nicFdo.driverObject = &nicDriver;
    nicFdo.deviceName = L"\\Device\\NetAdapter0";

    io::DriverObject filterDriver{};
    filterDriver.driverName = L"pktfilter.sys";

    io::DeviceObject filterFdo{};
    filterFdo.driverObject = &filterDriver;
    filterFdo.deviceName = L"\\Device\\PacketFilter0";

    auto root = pnpMgr.getRootNode();
    auto pciBus = root->getChildren()[0];
    auto nicNode = pciBus->getChildren()[0];

    // Configure AddDevice routine on function driver
    nicDriver.driverAddDevice = [](io::DriverObject*, io::DeviceObject* pdo) -> NtStatus {
        (void)pdo;
        return NtStatus::Success;
    };

    NtStatus stAdd = pnpMgr.addDevice(nicNode.get(), &nicDriver);
    TEST_ASSERT(NT_SUCCESS(stAdd), "addDevice must succeed");
    TEST_ASSERT(nicNode->getState() == pnp::PnpDeviceState::DriversAdded, "State must transition to DriversAdded");

    // Attach FDO to PDO stack
    io::IoAttachDeviceToDeviceStack(&nicFdo, nicNode->getPdo());
    nicNode->setFdo(&nicFdo);
    TEST_ASSERT(nicNode->getTopDeviceObject() == &nicFdo, "Top device object must be FDO");

    // Attach upper filter to stack
    io::IoAttachDeviceToDeviceStack(&filterFdo, nicNode->getPdo());
    nicNode->addUpperFilter(&filterFdo);
    TEST_ASSERT(nicNode->getTopDeviceObject() == &filterFdo, "Top device object must now be Upper Filter");

    // 3. PnP IRP State Machine: Start Device & Two-Phase Stop / Rebalancing
    {
        static uint32_t s_startCount = 0;
        static uint32_t s_queryStopCount = 0;
        static uint32_t s_cancelStopCount = 0;
        static uint32_t s_stopCount = 0;
        static bool s_vetoStop = false;

        s_startCount = 0;
        s_queryStopCount = 0;
        s_cancelStopCount = 0;
        s_stopCount = 0;
        s_vetoStop = false;

        filterDriver.setDispatch(io::IRP_MJ_PNP, [](io::DeviceObject*, io::Irp* irp) -> NtStatus {
            if (!irp) return NtStatus::InvalidParameter;
            switch (irp->minorFunction) {
                case pnp::IRP_MN_START_DEVICE:
                    s_startCount++;
                    irp->ioStatus.status = NtStatus::Success;
                    return NtStatus::Success;
                case pnp::IRP_MN_QUERY_STOP_DEVICE:
                    s_queryStopCount++;
                    if (s_vetoStop) {
                        irp->ioStatus.status = NtStatus::DeviceBusy;
                        return NtStatus::DeviceBusy;
                    }
                    irp->ioStatus.status = NtStatus::Success;
                    return NtStatus::Success;
                case pnp::IRP_MN_CANCEL_STOP_DEVICE:
                    s_cancelStopCount++;
                    irp->ioStatus.status = NtStatus::Success;
                    return NtStatus::Success;
                case pnp::IRP_MN_STOP_DEVICE:
                    s_stopCount++;
                    irp->ioStatus.status = NtStatus::Success;
                    return NtStatus::Success;
                default:
                    irp->ioStatus.status = NtStatus::Success;
                    return NtStatus::Success;
            }
        });

        // Start Device
        NtStatus stStart = pnpMgr.startDevice(nicNode.get());
        TEST_ASSERT(NT_SUCCESS(stStart), "startDevice must succeed");
        TEST_ASSERT(nicNode->getState() == pnp::PnpDeviceState::Started, "State must be Started");
        TEST_ASSERT(s_startCount == 1, "Start IRP count must be 1");

        // Two-Phase Stop: Step A: Query Stop
        NtStatus stQueryStop = pnpMgr.queryStopDevice(nicNode.get());
        TEST_ASSERT(NT_SUCCESS(stQueryStop), "queryStopDevice must succeed");
        TEST_ASSERT(nicNode->getState() == pnp::PnpDeviceState::QueryStopPending, "State must be QueryStopPending");
        TEST_ASSERT(s_queryStopCount == 1, "QueryStop IRP count must be 1");

        // Two-Phase Stop: Step B: Cancel Stop
        NtStatus stCancelStop = pnpMgr.cancelStopDevice(nicNode.get());
        TEST_ASSERT(NT_SUCCESS(stCancelStop), "cancelStopDevice must succeed");
        TEST_ASSERT(nicNode->getState() == pnp::PnpDeviceState::Started, "State must revert to Started");
        TEST_ASSERT(s_cancelStopCount == 1, "CancelStop IRP count must be 1");

        // Two-Phase Stop: Step C: Driver Veto on Query Stop
        s_vetoStop = true;
        stQueryStop = pnpMgr.queryStopDevice(nicNode.get());
        TEST_ASSERT(stQueryStop == NtStatus::DeviceBusy, "queryStopDevice must return DeviceBusy when vetoed");
        TEST_ASSERT(nicNode->getState() == pnp::PnpDeviceState::Started, "State must remain Started on veto");
        TEST_ASSERT(s_queryStopCount == 2, "QueryStop IRP count must be 2");

        // Two-Phase Stop: Step D: Successful Stop
        s_vetoStop = false;
        stQueryStop = pnpMgr.queryStopDevice(nicNode.get());
        TEST_ASSERT(NT_SUCCESS(stQueryStop), "queryStopDevice must succeed when un-vetoed");
        TEST_ASSERT(nicNode->getState() == pnp::PnpDeviceState::QueryStopPending, "State must be QueryStopPending");

        NtStatus stStop = pnpMgr.stopDevice(nicNode.get());
        TEST_ASSERT(NT_SUCCESS(stStop), "stopDevice must succeed");
        TEST_ASSERT(nicNode->getState() == pnp::PnpDeviceState::Stopped, "State must be Stopped");
        TEST_ASSERT(s_stopCount == 1, "Stop IRP count must be 1");

        // Re-start after stop rebalance
        stStart = pnpMgr.startDevice(nicNode.get());
        TEST_ASSERT(NT_SUCCESS(stStart), "startDevice after rebalance must succeed");
        TEST_ASSERT(nicNode->getState() == pnp::PnpDeviceState::Started, "State must return to Started");
        TEST_ASSERT(s_startCount == 2, "Start IRP count must be 2");
    }

    // 4. PnP IRP State Machine: Two-Phase Query-Remove, Veto & Orderly Removal
    {
        static uint32_t s_queryRemoveCount = 0;
        static uint32_t s_cancelRemoveCount = 0;
        static uint32_t s_removeCount = 0;
        static bool s_vetoRemove = false;

        s_queryRemoveCount = 0;
        s_cancelRemoveCount = 0;
        s_removeCount = 0;
        s_vetoRemove = false;

        filterDriver.setDispatch(io::IRP_MJ_PNP, [](io::DeviceObject*, io::Irp* irp) -> NtStatus {
            if (!irp) return NtStatus::InvalidParameter;
            switch (irp->minorFunction) {
                case pnp::IRP_MN_QUERY_REMOVE_DEVICE:
                    s_queryRemoveCount++;
                    if (s_vetoRemove) {
                        irp->ioStatus.status = NtStatus::Unsuccessful;
                        return NtStatus::Unsuccessful;
                    }
                    irp->ioStatus.status = NtStatus::Success;
                    return NtStatus::Success;
                case pnp::IRP_MN_CANCEL_REMOVE_DEVICE:
                    s_cancelRemoveCount++;
                    irp->ioStatus.status = NtStatus::Success;
                    return NtStatus::Success;
                case pnp::IRP_MN_REMOVE_DEVICE:
                    s_removeCount++;
                    irp->ioStatus.status = NtStatus::Success;
                    return NtStatus::Success;
                default:
                    irp->ioStatus.status = NtStatus::Success;
                    return NtStatus::Success;
            }
        });

        // Veto on Query-Remove
        s_vetoRemove = true;
        NtStatus stQueryRem = pnpMgr.queryRemoveDevice(nicNode.get());
        TEST_ASSERT(stQueryRem == NtStatus::Unsuccessful, "queryRemoveDevice must return error on veto");
        TEST_ASSERT(nicNode->getState() == pnp::PnpDeviceState::Started, "State must remain Started on veto");
        TEST_ASSERT(s_queryRemoveCount == 1, "QueryRemove IRP count must be 1");

        // Successful Query-Remove
        s_vetoRemove = false;
        stQueryRem = pnpMgr.queryRemoveDevice(nicNode.get());
        TEST_ASSERT(NT_SUCCESS(stQueryRem), "queryRemoveDevice must succeed");
        TEST_ASSERT(nicNode->getState() == pnp::PnpDeviceState::QueryRemovePending, "State must be QueryRemovePending");

        // Cancel-Remove
        NtStatus stCancelRem = pnpMgr.cancelRemoveDevice(nicNode.get());
        TEST_ASSERT(NT_SUCCESS(stCancelRem), "cancelRemoveDevice must succeed");
        TEST_ASSERT(nicNode->getState() == pnp::PnpDeviceState::Started, "State must revert to Started");
        TEST_ASSERT(s_cancelRemoveCount == 1, "CancelRemove IRP count must be 1");

        // Orderly Remove
        stQueryRem = pnpMgr.queryRemoveDevice(nicNode.get());
        TEST_ASSERT(NT_SUCCESS(stQueryRem), "queryRemoveDevice before remove must succeed");

        NtStatus stRem = pnpMgr.removeDevice(nicNode.get());
        TEST_ASSERT(NT_SUCCESS(stRem), "removeDevice must succeed");
        TEST_ASSERT(nicNode->getState() == pnp::PnpDeviceState::Deleted, "DevNode state must be Deleted");
        TEST_ASSERT(s_removeCount == 1, "Remove IRP count must be 1");
        TEST_ASSERT(pciBus->getChildren().empty(), "PCI Bus should have 0 children after remove");
    }

    // 5. Hot-Unplug / Surprise Removal State Transition
    {
        static uint32_t s_surpriseCount = 0;
        static uint32_t s_usbRemoveCount = 0;

        s_surpriseCount = 0;
        s_usbRemoveCount = 0;

        io::DriverObject usbDriver{};
        usbDriver.driverName = L"usbhub.sys";

        io::DeviceObject usbPdo{};
        usbPdo.driverObject = &usbDriver;
        usbPdo.deviceName = L"\\Device\\UsbPdo0";

        usbDriver.setDispatch(io::IRP_MJ_PNP, [](io::DeviceObject*, io::Irp* irp) -> NtStatus {
            if (!irp) return NtStatus::InvalidParameter;
            if (irp->minorFunction == pnp::IRP_MN_SURPRISE_REMOVAL) {
                s_surpriseCount++;
            } else if (irp->minorFunction == pnp::IRP_MN_REMOVE_DEVICE) {
                s_usbRemoveCount++;
            }
            irp->ioStatus.status = NtStatus::Success;
            return NtStatus::Success;
        });

        auto usbNode = pnpMgr.createDeviceNode(pciBus.get(), &usbPdo, L"USB\\VID_045E&PID_0040", L"0001");
        TEST_ASSERT(usbNode != nullptr, "USB DevNode creation must succeed");
        TEST_ASSERT(pciBus->getChildren().size() == 1, "PCI Bus should now have 1 child");

        pnpMgr.startDevice(usbNode.get());
        TEST_ASSERT(usbNode->getState() == pnp::PnpDeviceState::Started, "USB node must be Started");

        // Surprise Removal (Hot-Unplug)
        NtStatus stSurprise = pnpMgr.surpriseRemoveDevice(usbNode.get());
        TEST_ASSERT(NT_SUCCESS(stSurprise), "surpriseRemoveDevice must succeed");
        TEST_ASSERT(s_surpriseCount == 1, "SurpriseRemoval IRP count must be 1");
        TEST_ASSERT(s_usbRemoveCount == 1, "Remove IRP count must follow surprise removal");
        TEST_ASSERT(usbNode->getState() == pnp::PnpDeviceState::Deleted, "USB node state must reach Deleted");
        TEST_ASSERT(pciBus->getChildren().empty(), "PCI Bus should have 0 children after surprise removal");
    }

    // 6. Bus Enumeration (IRP_MN_QUERY_DEVICE_RELATIONS - BusRelations)
    {
        io::DriverObject busDriver{};
        busDriver.driverName = L"pcibus.sys";

        io::DeviceObject busPdo{};
        busPdo.driverObject = &busDriver;
        busPdo.deviceName = L"\\Device\\PciBusMaster";

        static io::DeviceObject s_childPdo1{};
        s_childPdo1.driverObject = &busDriver;
        s_childPdo1.deviceName = L"\\Device\\PciChild1";

        static io::DeviceObject s_childPdo2{};
        s_childPdo2.driverObject = &busDriver;
        s_childPdo2.deviceName = L"\\Device\\PciChild2";

        busDriver.setDispatch(io::IRP_MJ_PNP, [](io::DeviceObject* dev, io::Irp* irp) -> NtStatus {
            if (!irp) return NtStatus::InvalidParameter;
            if (irp->minorFunction == pnp::IRP_MN_QUERY_DEVICE_RELATIONS) {
                auto* relations = reinterpret_cast<pnp::DeviceRelationsList*>(irp->userBuffer);
                if (relations) {
                    relations->objects.push_back(&s_childPdo1);
                    relations->objects.push_back(&s_childPdo2);
                    relations->count = 2;
                }
                irp->ioStatus.status = NtStatus::Success;
                return NtStatus::Success;
            }
            if (irp->minorFunction == pnp::IRP_MN_QUERY_ID) {
                auto* outStr = reinterpret_cast<std::wstring*>(irp->userBuffer);
                if (outStr) {
                    auto idType = static_cast<pnp::BUS_QUERY_ID_TYPE>(irp->getCurrentStack()->parameters.pnp.type);
                    if (idType == pnp::BUS_QUERY_ID_TYPE::BusQueryDeviceID) {
                        *outStr = (dev == &s_childPdo1) ? L"PCI\\VEN_10DE&DEV_1B80" : L"PCI\\VEN_144D&DEV_A808";
                    } else if (idType == pnp::BUS_QUERY_ID_TYPE::BusQueryInstanceID) {
                        *outStr = (dev == &s_childPdo1) ? L"SLOT1" : L"SLOT2";
                    }
                }
                irp->ioStatus.status = NtStatus::Success;
                return NtStatus::Success;
            }
            if (irp->minorFunction == pnp::IRP_MN_QUERY_CAPABILITIES) {
                auto* caps = reinterpret_cast<pnp::DEVICE_CAPABILITIES*>(irp->userBuffer);
                if (caps) {
                    caps->removable = (dev == &s_childPdo1) ? 0 : 1;
                    caps->surpriseRemovalOK = (dev == &s_childPdo1) ? 0 : 1;
                }
                irp->ioStatus.status = NtStatus::Success;
                return NtStatus::Success;
            }
            irp->ioStatus.status = NtStatus::Success;
            return NtStatus::Success;
        });

        auto busDevNode = pnpMgr.createDeviceNode(root.get(), &busPdo, L"PCI\\ROOTBUS\\0", L"0");
        TEST_ASSERT(busDevNode != nullptr, "Bus DevNode creation must succeed");

        std::vector<std::shared_ptr<pnp::PnpDeviceNode>> discovered;
        NtStatus stEnum = pnpMgr.enumerateBus(busDevNode.get(), discovered);
        TEST_ASSERT(NT_SUCCESS(stEnum), "enumerateBus must succeed");
        TEST_ASSERT(discovered.size() == 2, "Discovered children count must be 2");
        TEST_ASSERT(busDevNode->getChildren().size() == 2, "Bus DevNode children must be 2");

        TEST_ASSERT(discovered[0]->getDeviceId() == L"PCI\\VEN_10DE&DEV_1B80", "Discovered child 0 Device ID must match");
        TEST_ASSERT(discovered[0]->getInstanceId() == L"SLOT1", "Discovered child 0 Instance ID must match");
        TEST_ASSERT(discovered[0]->getCapabilities().removable == 0, "Child 0 removable capability must be 0");

        TEST_ASSERT(discovered[1]->getDeviceId() == L"PCI\\VEN_144D&DEV_A808", "Discovered child 1 Device ID must match");
        TEST_ASSERT(discovered[1]->getInstanceId() == L"SLOT2", "Discovered child 1 Instance ID must match");
        TEST_ASSERT(discovered[1]->getCapabilities().removable == 1, "Child 1 removable capability must be 1");
    }

    // 7. Device Interface Classes, Enabling/Disabling & Symbolic Links
    {
        io::DeviceObject netPdo{};
        netPdo.deviceName = L"\\Device\\NetCard0";

        std::wstring symLinkNet;
        NtStatus stReg = pnp::IoRegisterDeviceInterface(&netPdo, &pnp::GUID_DEVINTERFACE_NET, L"eth0", &symLinkNet);
        TEST_ASSERT(NT_SUCCESS(stReg), "IoRegisterDeviceInterface must succeed");
        TEST_ASSERT(symLinkNet.starts_with(L"\\\\?\\NetCard0#"), "Symbolic link prefix must be \\\\?\\NetCard0#");
        TEST_ASSERT(symLinkNet.ends_with(L"\\eth0"), "Symbolic link suffix must be \\eth0");

        // Interface should not be in ObjectNamespace yet because it is not enabled
        auto& obNs = ob::ObjectNamespace::get();
        TEST_ASSERT(obNs.getSymbolicLink(symLinkNet) == nullptr, "Disabled interface must not exist in ObjectNamespace");

        // Enable Interface
        NtStatus stEn = pnp::IoSetDeviceInterfaceState(symLinkNet, true);
        TEST_ASSERT(NT_SUCCESS(stEn), "IoSetDeviceInterfaceState(true) must succeed");
        TEST_ASSERT(obNs.getSymbolicLink(symLinkNet) != nullptr, "Enabled interface must exist in ObjectNamespace");
        TEST_ASSERT(obNs.getSymbolicLink(symLinkNet)->getTargetPath() == L"\\Device\\NetCard0", "Target path must match PDO");

        // Disable Interface
        NtStatus stDis = pnp::IoSetDeviceInterfaceState(symLinkNet, false);
        TEST_ASSERT(NT_SUCCESS(stDis), "IoSetDeviceInterfaceState(false) must succeed");
        TEST_ASSERT(obNs.getSymbolicLink(symLinkNet) == nullptr, "Disabled interface must be removed from ObjectNamespace");
    }

    // 8. PnP Notification Callbacks & Synchronous Replay
    {
        uint32_t arrivalCount = 0;
        uint32_t removalCount = 0;
        std::wstring lastArrivedLink;

        pnp::PDRIVER_NOTIFICATION_CALLBACK_ROUTINE callback = [](void* notifStruct, void* ctx) -> NtStatus {
            auto* notif = reinterpret_cast<pnp::DEVICE_INTERFACE_CHANGE_NOTIFICATION*>(notifStruct);
            auto* counts = reinterpret_cast<std::pair<uint32_t*, uint32_t*>*>(ctx);
            if (notif && counts) {
                if (notif->event == pnp::GUID_DEVICE_INTERFACE_ARRIVAL) {
                    (*counts->first)++;
                } else if (notif->event == pnp::GUID_DEVICE_INTERFACE_REMOVAL) {
                    (*counts->second)++;
                }
            }
            return NtStatus::Success;
        };

        std::pair<uint32_t*, uint32_t*> counts{&arrivalCount, &removalCount};

        void* notifHandle = nullptr;
        GUID diskGuid = pnp::GUID_DEVINTERFACE_DISK;
        NtStatus stNotif = pnp::IoRegisterPlugPlayNotification(
            pnp::EventCategory::DeviceInterfaceChange,
            0,
            &diskGuid,
            nullptr,
            callback,
            &counts,
            &notifHandle
        );
        TEST_ASSERT(NT_SUCCESS(stNotif), "IoRegisterPlugPlayNotification must succeed");
        TEST_ASSERT(notifHandle != nullptr, "Notification handle must be non-null");

        // Register Disk PDO and Interface
        io::DeviceObject diskPdo{};
        diskPdo.deviceName = L"\\Device\\Harddisk0\\DR0";

        std::wstring diskLink;
        pnp::IoRegisterDeviceInterface(&diskPdo, &pnp::GUID_DEVINTERFACE_DISK, nullptr, &diskLink);

        // Enable disk interface -> arrival notification should fire
        pnp::IoSetDeviceInterfaceState(diskLink, true);
        TEST_ASSERT(arrivalCount == 1, "Arrival notification count must be 1");
        TEST_ASSERT(removalCount == 0, "Removal notification count must be 0");

        // Disable disk interface -> removal notification should fire
        pnp::IoSetDeviceInterfaceState(diskLink, false);
        TEST_ASSERT(arrivalCount == 1, "Arrival notification count must stay 1");
        TEST_ASSERT(removalCount == 1, "Removal notification count must be 1");

        // Re-enable disk interface
        pnp::IoSetDeviceInterfaceState(diskLink, true);
        TEST_ASSERT(arrivalCount == 2, "Arrival notification count must be 2");

        // Register second notification with PNPNOTIFY_DEVICE_INTERFACE_INCLUDE_EXISTING_INTERFACES
        uint32_t replayArrivalCount = 0;
        uint32_t replayRemovalCount = 0;
        std::pair<uint32_t*, uint32_t*> replayCounts{&replayArrivalCount, &replayRemovalCount};

        void* replayHandle = nullptr;
        NtStatus stReplay = pnp::IoRegisterPlugPlayNotification(
            pnp::EventCategory::DeviceInterfaceChange,
            pnp::PNPNOTIFY_DEVICE_INTERFACE_INCLUDE_EXISTING_INTERFACES,
            &diskGuid,
            nullptr,
            callback,
            &replayCounts,
            &replayHandle
        );
        TEST_ASSERT(NT_SUCCESS(stReplay), "IoRegisterPlugPlayNotification with existing interfaces flag must succeed");
        TEST_ASSERT(replayArrivalCount == 1, "Existing interface replay must immediately deliver 1 arrival notification");

        // Unregister notifications
        NtStatus stUnreg1 = pnp::IoUnregisterPlugPlayNotificationEx(notifHandle);
        TEST_ASSERT(NT_SUCCESS(stUnreg1), "IoUnregisterPlugPlayNotificationEx for notifHandle must succeed");

        NtStatus stUnreg2 = pnp::IoUnregisterPlugPlayNotificationEx(replayHandle);
        TEST_ASSERT(NT_SUCCESS(stUnreg2), "IoUnregisterPlugPlayNotificationEx for replayHandle must succeed");
    }

    // 9. PnP Telemetry Audit & Reset
    {
        pnp::PNP_TELEMETRY telem = pnp::PnpQueryTelemetry();
        TEST_ASSERT(telem.totalDeviceNodesCreated > 0, "totalDeviceNodesCreated must be > 0");
        TEST_ASSERT(telem.totalDeviceNodesRemoved > 0, "totalDeviceNodesRemoved must be > 0");
        TEST_ASSERT(telem.totalStartDeviceIrpsDispatched > 0, "totalStartDeviceIrpsDispatched must be > 0");
        TEST_ASSERT(telem.totalStopDeviceIrpsDispatched > 0, "totalStopDeviceIrpsDispatched must be > 0");
        TEST_ASSERT(telem.totalQueryStopIrpsDispatched > 0, "totalQueryStopIrpsDispatched must be > 0");
        TEST_ASSERT(telem.totalCancelStopIrpsDispatched > 0, "totalCancelStopIrpsDispatched must be > 0");
        TEST_ASSERT(telem.totalQueryRemoveIrpsDispatched > 0, "totalQueryRemoveIrpsDispatched must be > 0");
        TEST_ASSERT(telem.totalCancelRemoveIrpsDispatched > 0, "totalCancelRemoveIrpsDispatched must be > 0");
        TEST_ASSERT(telem.totalRemoveDeviceIrpsDispatched > 0, "totalRemoveDeviceIrpsDispatched must be > 0");
        TEST_ASSERT(telem.totalSurpriseRemovalIrpsDispatched > 0, "totalSurpriseRemovalIrpsDispatched must be > 0");
        TEST_ASSERT(telem.totalBusRelationsEnumerated > 0, "totalBusRelationsEnumerated must be > 0");
        TEST_ASSERT(telem.totalDeviceInterfacesRegistered > 0, "totalDeviceInterfacesRegistered must be > 0");
        TEST_ASSERT(telem.totalDeviceInterfacesEnabled > 0, "totalDeviceInterfacesEnabled must be > 0");
        TEST_ASSERT(telem.totalDeviceInterfacesDisabled > 0, "totalDeviceInterfacesDisabled must be > 0");
        TEST_ASSERT(telem.totalNotificationsDispatched > 0, "totalNotificationsDispatched must be > 0");

        // Reset telemetry
        pnp::PnpResetTelemetryForTesting();
        pnp::PNP_TELEMETRY resetTelem = pnp::PnpQueryTelemetry();
        TEST_ASSERT(resetTelem.totalDeviceNodesCreated == 0, "totalDeviceNodesCreated must be 0 after reset");
        TEST_ASSERT(resetTelem.totalStartDeviceIrpsDispatched == 0, "totalStartDeviceIrpsDispatched must be 0 after reset");
        TEST_ASSERT(resetTelem.totalDeviceInterfacesRegistered == 0, "totalDeviceInterfacesRegistered must be 0 after reset");
    }

    std::cout << "[TEST] Suite 245: Horizon 29 - Plug and Play (PnP) Manager Architecture, DevNode Hierarchy, IRP_MN State Engine & Device Interfaces PASSED.\n";
}

// ============================================================================
// Suite 246: Horizon 30 - Sovereign Kernel Debugger Engine (KD64), KDCom
// Protocol, Breakpoints & KdpTrap Exception Routing
// ============================================================================
void Test_Horizon30_KernelDebugger_KD64_Breakpoints_And_KdpTrapExceptionRouting() {
    kd::KdInitSystem(0);
    kd::KdResetTelemetryForTesting();

    auto& debugger = kd::KdDebugger::get();
    debugger.initialize(true);

    // 1. Packet Framing & Arithmetic Checksum Calculation and Validation
    {
        const uint8_t sampleData[] = { 0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10 };
        uint32_t expectedSum = 0;
        for (uint8_t b : sampleData) expectedSum += b;
        uint32_t computed = kd::KdComputeChecksum(sampleData, sizeof(sampleData));
        TEST_ASSERT(computed == expectedSum, "KdComputeChecksum must compute 32-bit arithmetic byte sum");

        // Transmit packet over KdTransport
        kd::KdTransport transport;
        bool okSend = transport.sendPacket(kd::PACKET_TYPE_KD_STATE_MANIPULATE, sampleData, sizeof(sampleData));
        TEST_ASSERT(okSend, "sendPacket must succeed");
        TEST_ASSERT(transport.getOutboundQueueCount() == 1, "Outbound queue count must be 1");

        auto rawPkt = transport.popOutboundPacket();
        TEST_ASSERT(rawPkt.size() == sizeof(kd::KD_PACKET) + sizeof(sampleData), "Packet size must match header + payload");

        // Deliver to inbound queue and receive
        transport.deliverInbound(rawPkt);
        kd::KD_PACKET inHdr{};
        std::vector<uint8_t> inPayload;
        bool okRecv = transport.receivePacket(inHdr, inPayload);
        TEST_ASSERT(okRecv, "receivePacket must succeed for valid packet");
        TEST_ASSERT(inHdr.packetLeader == kd::PACKET_LEADER, "Packet leader must be PACKET_LEADER (0x30303030)");
        TEST_ASSERT(inHdr.packetType == kd::PACKET_TYPE_KD_STATE_MANIPULATE, "Packet type must match");
        TEST_ASSERT(inHdr.byteCount == sizeof(sampleData), "Byte count must match payload length");
        TEST_ASSERT(inHdr.checksum == expectedSum, "Checksum must match expected arithmetic sum");
        TEST_ASSERT(inPayload.size() == sizeof(sampleData), "Payload size must match");
        TEST_ASSERT(std::memcmp(inPayload.data(), sampleData, sizeof(sampleData)) == 0, "Payload content must match");

        // Corrupt checksum test
        auto corruptPkt = rawPkt;
        auto* hdr = reinterpret_cast<kd::KD_PACKET*>(corruptPkt.data());
        hdr->checksum ^= 0xFFFFFFFF;
        transport.deliverInbound(corruptPkt);
        bool failCrc = transport.receivePacket(inHdr, inPayload);
        TEST_ASSERT(!failCrc, "receivePacket must reject packet with corrupted checksum");

        // Invalid packet leader test
        auto badLeaderPkt = rawPkt;
        auto* badHdr = reinterpret_cast<kd::KD_PACKET*>(badLeaderPkt.data());
        badHdr->packetLeader = 0x12345678;
        transport.deliverInbound(badLeaderPkt);
        bool failLeader = transport.receivePacket(inHdr, inPayload);
        TEST_ASSERT(!failLeader, "receivePacket must reject packet with invalid leader");
    }

    // 2. KDCom Transport Handshake, Packet ID Sequencing & ACK Protocol
    {
        kd::KdTransport transport;
        uint32_t startId = transport.getCurrentPacketId();
        TEST_ASSERT(startId == kd::INITIAL_PACKET_ID, "Initial packet ID must be 0x80800000");

        uint32_t dummy = 0x12345678;
        transport.sendPacket(kd::PACKET_TYPE_KD_DEBUG_IO, &dummy, sizeof(dummy));
        TEST_ASSERT(transport.getCurrentPacketId() == startId + 1, "Packet ID must increment after transmission");

        // Send ACK
        bool ackOk = transport.sendAcknowledge(startId);
        TEST_ASSERT(ackOk, "sendAcknowledge must succeed");

        auto rawData = transport.popOutboundPacket(); // pop data packet
        (void)rawData;
        auto ackPkt = transport.popOutboundPacket();  // pop ACK packet
        TEST_ASSERT(ackPkt.size() == sizeof(kd::KD_PACKET), "ACK packet size must be sizeof(KD_PACKET)");

        transport.deliverInbound(ackPkt);
        kd::KD_PACKET inAckHdr{};
        std::vector<uint8_t> inAckPayload;
        bool okRecvAck = transport.receivePacket(inAckHdr, inAckPayload);
        TEST_ASSERT(okRecvAck, "receivePacket must succeed for ACK packet");
        TEST_ASSERT(inAckHdr.packetLeader == kd::CONTROL_PACKET_LEADER, "ACK leader must be CONTROL_PACKET_LEADER (0x69696969)");
        TEST_ASSERT(inAckHdr.packetType == kd::PACKET_TYPE_KD_ACKNOWLEDGE, "Packet type must be ACK");
        TEST_ASSERT(inAckHdr.packetId == startId, "ACK packet ID must match acknowledged packet");
    }

    // 3. Resend & Reset Control Packet Protocol
    {
        kd::KdTransport transport;
        uint64_t msg = 0xCAFEBABE11223344ULL;
        transport.sendPacket(kd::PACKET_TYPE_KD_STATE_CHANGE64, &msg, sizeof(msg));
        auto firstPkt = transport.popOutboundPacket();

        // Host requests RESEND
        bool resendOk = transport.resendLastPacket();
        TEST_ASSERT(resendOk, "resendLastPacket must succeed when outbound packet was sent");
        auto resentPkt = transport.popOutboundPacket();
        TEST_ASSERT(resentPkt == firstPkt, "Resent packet bytes must be identical to original outbound packet");

        // Transport Reset
        transport.reset();
        TEST_ASSERT(transport.getCurrentPacketId() == kd::INITIAL_PACKET_ID, "Reset must restore INITIAL_PACKET_ID");
        TEST_ASSERT(transport.getOutboundQueueCount() == 0, "Outbound queue must be empty after reset");
    }

    // 4. Hardware Debug Registers & Watchpoint Engine (AMD64 DR0-DR7)
    {
        kd::HardwareBreakpointEngine hw;

        // Set DR0: Execution breakpoint at 0xFFFFF80000401000
        bool set0 = hw.setBreakpoint(0, 0xFFFFF80000401000ULL, kd::HwBreakpointType::Execute, kd::HwBreakpointLength::Byte1);
        TEST_ASSERT(set0, "setBreakpoint on DR0 must succeed");

        // Set DR1: Data write watchpoint at 0xFFFF800000200000, 4 bytes
        bool set1 = hw.setBreakpoint(1, 0xFFFF800000200000ULL, kd::HwBreakpointType::Write, kd::HwBreakpointLength::Byte4);
        TEST_ASSERT(set1, "setBreakpoint on DR1 must succeed");

        // Set DR2: Data read/write watchpoint at 0xFFFF800000300000, 8 bytes
        bool set2 = hw.setBreakpoint(2, 0xFFFF800000300000ULL, kd::HwBreakpointType::ReadWrite, kd::HwBreakpointLength::Byte8);
        TEST_ASSERT(set2, "setBreakpoint on DR2 must succeed");

        const auto& regs = hw.getRegisters();
        TEST_ASSERT(regs.dr0 == 0xFFFFF80000401000ULL, "DR0 must contain set address");
        TEST_ASSERT(regs.dr1 == 0xFFFF800000200000ULL, "DR1 must contain set address");
        TEST_ASSERT(regs.dr2 == 0xFFFF800000300000ULL, "DR2 must contain set address");
        TEST_ASSERT((regs.dr7 & 0x00000400ULL) != 0, "DR7 bit 10 must be preserved");
        TEST_ASSERT((regs.dr7 & 0x01) != 0, "DR7 L0 bit must be set");
        TEST_ASSERT((regs.dr7 & 0x04) != 0, "DR7 L1 bit must be set");
        TEST_ASSERT((regs.dr7 & 0x10) != 0, "DR7 L2 bit must be set");

        // Evaluate access: Execution match on DR0
        uint64_t dr6 = 0;
        bool hitExec = hw.evaluateAccess(0xFFFFF80000401000ULL, 1, false, true, dr6);
        TEST_ASSERT(hitExec, "evaluateAccess must detect execution breakpoint match on DR0");
        TEST_ASSERT((dr6 & 0x01) != 0, "DR6 B0 bit must be asserted on DR0 match");

        // Evaluate access: Write match on DR1
        bool hitWrite = hw.evaluateAccess(0xFFFF800000200002ULL, 2, true, false, dr6);
        TEST_ASSERT(hitWrite, "evaluateAccess must detect write watchpoint match on DR1");
        TEST_ASSERT((dr6 & 0x02) != 0, "DR6 B1 bit must be asserted on DR1 match");

        // Evaluate access: Read on write-only watchpoint should NOT hit
        bool hitReadFail = hw.evaluateAccess(0xFFFF800000200000ULL, 4, false, false, dr6);
        TEST_ASSERT(!hitReadFail, "Read access on write-only watchpoint must not trigger breakpoint");

        // Evaluate access: Read on R/W watchpoint on DR2 should hit
        bool hitRw = hw.evaluateAccess(0xFFFF800000300004ULL, 4, false, false, dr6);
        TEST_ASSERT(hitRw, "Read access on ReadWrite watchpoint must trigger breakpoint");
        TEST_ASSERT((dr6 & 0x04) != 0, "DR6 B2 bit must be asserted on DR2 match");

        // Single step control
        hw.setSingleStep(true);
        TEST_ASSERT((hw.getRegisters().dr6 & (1ULL << 14)) != 0, "DR6 bit 14 (BS) must be set on single-step enable");
        hw.setSingleStep(false);
        TEST_ASSERT((hw.getRegisters().dr6 & (1ULL << 14)) == 0, "DR6 bit 14 (BS) must be cleared on single-step disable");

        // Clear breakpoint
        bool clr0 = hw.clearBreakpoint(0);
        TEST_ASSERT(clr0, "clearBreakpoint on DR0 must succeed");
        TEST_ASSERT(hw.getRegisters().dr0 == 0, "DR0 must be 0 after clear");
        TEST_ASSERT((hw.getRegisters().dr7 & 0x01) == 0, "DR7 L0 bit must be cleared");
    }

    // 5. Software Breakpoint Table Management & Opcode Patching
    {
        uint64_t targetAddr = 0xFFFFF800DEAD0000ULL;
        uint8_t originalCode[4] = { 0x55, 0x48, 0x89, 0xE5 }; // push rbp; mov rbp, rsp
        debugger.writeSimulatedMemory(targetAddr, originalCode, sizeof(originalCode));

        uint32_t bpHandle = debugger.writeBreakpoint(targetAddr);
        TEST_ASSERT(bpHandle > 0, "writeBreakpoint must return a valid non-zero handle");
        TEST_ASSERT(debugger.getActiveBreakpointCount() == 1, "Active breakpoint count must be 1");

        // Verify simulated memory now has 0xCC (INT 3)
        uint8_t readBack[4]{};
        debugger.readSimulatedMemory(targetAddr, readBack, sizeof(readBack));
        TEST_ASSERT(readBack[0] == 0xCC, "First opcode byte must be patched with 0xCC (INT 3)");
        TEST_ASSERT(readBack[1] == 0x48, "Subsequent bytes must remain unmodified");

        // Restore breakpoint
        bool restored = debugger.restoreBreakpoint(bpHandle);
        TEST_ASSERT(restored, "restoreBreakpoint must succeed");
        TEST_ASSERT(debugger.getActiveBreakpointCount() == 0, "Active breakpoint count must be 0 after restoration");

        // Verify simulated memory restored to original 0x55
        debugger.readSimulatedMemory(targetAddr, readBack, sizeof(readBack));
        TEST_ASSERT(readBack[0] == 0x55, "First opcode byte must be restored to original 0x55");

        // Second restore on same handle must fail
        bool restoredAgain = debugger.restoreBreakpoint(bpHandle);
        TEST_ASSERT(!restoredAgain, "Restoring with an already freed handle must fail");
    }

    // 6. DbgKd State Manipulate Commands
    {
        // 6.1 GetVersion
        kd::DBGKD_MANIPULATE_STATE64 req{};
        kd::DBGKD_MANIPULATE_STATE64 reply{};
        std::vector<uint8_t> inData;
        std::vector<uint8_t> outData;

        req.apiNumber = kd::DbgKdGetVersionApi;
        bool okVer = debugger.handleStateManipulate(req, inData, reply, outData);
        TEST_ASSERT(okVer, "handleStateManipulate(DbgKdGetVersionApi) must succeed");
        TEST_ASSERT(reply.returnStatus == static_cast<uint32_t>(NtStatus::Success), "Status must be Success");
        TEST_ASSERT(reply.getVersion.majorVersion == 0x000F, "Major version must be 0x000F (Free)");
        TEST_ASSERT(reply.getVersion.machineType == 0x8664, "Machine type must be IMAGE_FILE_MACHINE_AMD64 (0x8664)");
        TEST_ASSERT(reply.getVersion.protocolVersion == 6, "Protocol version must be 6");

        // 6.2 Write & Read Virtual Memory
        const char testStr[] = "MicaNT_KD64_Kernel_Protocol_Verification";
        size_t strLen = sizeof(testStr);

        req = {};
        req.apiNumber = kd::DbgKdWriteVirtualMemoryApi64;
        req.writeMemory.targetBaseAddress = 0xFFFFF800C0000000ULL;
        req.writeMemory.transferCount = static_cast<uint32_t>(strLen);
        inData.assign(testStr, testStr + strLen);

        bool okWvm = debugger.handleStateManipulate(req, inData, reply, outData);
        TEST_ASSERT(okWvm, "DbgKdWriteVirtualMemoryApi64 must succeed");
        TEST_ASSERT(reply.writeMemory.actualBytesWritten == strLen, "Bytes written must match transfer count");

        req = {};
        req.apiNumber = kd::DbgKdReadVirtualMemoryApi64;
        req.readMemory.targetBaseAddress = 0xFFFFF800C0000000ULL;
        req.readMemory.transferCount = static_cast<uint32_t>(strLen);
        inData.clear();

        bool okRvm = debugger.handleStateManipulate(req, inData, reply, outData);
        TEST_ASSERT(okRvm, "DbgKdReadVirtualMemoryApi64 must succeed");
        TEST_ASSERT(reply.readMemory.actualBytesRead == strLen, "Bytes read must match transfer count");
        TEST_ASSERT(outData.size() == strLen, "Output data size must match transfer count");
        TEST_ASSERT(std::memcmp(outData.data(), testStr, strLen) == 0, "Read virtual memory must match written data");

        // 6.3 Write & Read Physical Memory
        const uint64_t physPattern = 0x1122334455667788ULL;
        req = {};
        req.apiNumber = kd::DbgKdWritePhysicalMemoryApi64;
        req.writePhysicalMemory.targetBaseAddress = 0x200000ULL;
        req.writePhysicalMemory.transferCount = sizeof(physPattern);
        inData.resize(sizeof(physPattern));
        std::memcpy(inData.data(), &physPattern, sizeof(physPattern));

        bool okWpm = debugger.handleStateManipulate(req, inData, reply, outData);
        TEST_ASSERT(okWpm, "DbgKdWritePhysicalMemoryApi64 must succeed");

        req = {};
        req.apiNumber = kd::DbgKdReadPhysicalMemoryApi64;
        req.readPhysicalMemory.targetBaseAddress = 0x200000ULL;
        req.readPhysicalMemory.transferCount = sizeof(physPattern);
        inData.clear();

        bool okRpm = debugger.handleStateManipulate(req, inData, reply, outData);
        TEST_ASSERT(okRpm, "DbgKdReadPhysicalMemoryApi64 must succeed");
        uint64_t readPhys = 0;
        std::memcpy(&readPhys, outData.data(), sizeof(readPhys));
        TEST_ASSERT(readPhys == physPattern, "Read physical memory must match written pattern");

        // 6.4 Read & Write Control Space (CR0, CR3, CR4, CR8)
        req = {};
        req.apiNumber = kd::DbgKdReadControlSpaceApi;
        req.readControlSpace.targetBaseAddress = 0; // CR0
        req.readControlSpace.transferCount = 8;
        bool okRcs = debugger.handleStateManipulate(req, inData, reply, outData);
        TEST_ASSERT(okRcs, "DbgKdReadControlSpaceApi (CR0) must succeed");
        uint64_t cr0Val = 0;
        std::memcpy(&cr0Val, outData.data(), sizeof(cr0Val));
        TEST_ASSERT(cr0Val == 0x80050033ULL, "Default CR0 value must match 0x80050033");

        // Write CR8
        uint64_t newCr8 = 0xF;
        req = {};
        req.apiNumber = kd::DbgKdWriteControlSpaceApi;
        req.writeControlSpace.targetBaseAddress = 8; // CR8
        req.writeControlSpace.transferCount = 8;
        inData.resize(8);
        std::memcpy(inData.data(), &newCr8, 8);
        bool okWcs = debugger.handleStateManipulate(req, inData, reply, outData);
        TEST_ASSERT(okWcs, "DbgKdWriteControlSpaceApi (CR8) must succeed");

        // Read CR8 back
        req = {};
        req.apiNumber = kd::DbgKdReadControlSpaceApi;
        req.readControlSpace.targetBaseAddress = 8; // CR8
        req.readControlSpace.transferCount = 8;
        inData.clear();
        debugger.handleStateManipulate(req, inData, reply, outData);
        uint64_t readCr8 = 0;
        std::memcpy(&readCr8, outData.data(), 8);
        TEST_ASSERT(readCr8 == 0xF, "Read CR8 must match written value 0xF");

        // 6.5 Set & Get Context Frame
        kd::DBGKD_CONTEXT64 newCtx{};
        newCtx.rip = 0xFFFFF800DEADBEEFULL;
        newCtx.rsp = 0xFFFF800012345678ULL;
        newCtx.rax = 0x1111111122222222ULL;
        newCtx.rflags = 0x00000246;

        req = {};
        req.apiNumber = kd::DbgKdSetContextApi;
        inData.resize(sizeof(newCtx));
        std::memcpy(inData.data(), &newCtx, sizeof(newCtx));
        bool okSetCtx = debugger.handleStateManipulate(req, inData, reply, outData);
        TEST_ASSERT(okSetCtx, "DbgKdSetContextApi must succeed");

        req = {};
        req.apiNumber = kd::DbgKdGetContextApi;
        inData.clear();
        bool okGetCtx = debugger.handleStateManipulate(req, inData, reply, outData);
        TEST_ASSERT(okGetCtx, "DbgKdGetContextApi must succeed");
        kd::DBGKD_CONTEXT64 retrievedCtx{};
        std::memcpy(&retrievedCtx, outData.data(), sizeof(retrievedCtx));
        TEST_ASSERT(retrievedCtx.rip == 0xFFFFF800DEADBEEFULL, "Retrieved context RIP must match");
        TEST_ASSERT(retrievedCtx.rsp == 0xFFFF800012345678ULL, "Retrieved context RSP must match");
        TEST_ASSERT(retrievedCtx.rax == 0x1111111122222222ULL, "Retrieved context RAX must match");
        TEST_ASSERT(retrievedCtx.rflags == 0x00000246, "Retrieved context RFLAGS must match");

        // 6.6 WriteBreakPoint & RestoreBreakPoint via API
        req = {};
        req.apiNumber = kd::DbgKdWriteBreakPointApi;
        req.breakPoint.breakPointAddress = 0xFFFFF800AAAA0000ULL;
        bool okWbp = debugger.handleStateManipulate(req, inData, reply, outData);
        TEST_ASSERT(okWbp, "DbgKdWriteBreakPointApi must succeed");
        uint32_t apiBpHandle = reply.breakPoint.breakPointHandle;
        TEST_ASSERT(apiBpHandle > 0, "DbgKdWriteBreakPointApi must return a valid handle");

        req = {};
        req.apiNumber = kd::DbgKdRestoreBreakPointApi;
        req.breakPoint.breakPointHandle = apiBpHandle;
        bool okRbp = debugger.handleStateManipulate(req, inData, reply, outData);
        TEST_ASSERT(okRbp, "DbgKdRestoreBreakPointApi must succeed");
    }

    // 7. KdpTrap Exception Interception & DBGKD_WAIT_STATE_CHANGE64 Framing
    {
        debugger.getTransport().reset();

        ke::ExceptionRecord record{
            .exceptionCode = ke::EXCEPTION_BREAKPOINT,
            .exceptionAddress = reinterpret_cast<void*>(0xFFFFF80000500123ULL),
            .numberParameters = 1
        };
        record.exceptionInformation[0] = 0;

        ps::ContextFrame ctx{};
        ctx.rip = 0xFFFFF80000500123ULL;
        ctx.rsp = 0xFFFF800000100000ULL;
        ctx.rax = 0x42;
        ctx.rflags = 0x202;
        ctx.cs = 0x10;
        ctx.ss = 0x18;

        bool handled = kd::KdpTrap(record, ctx, false);
        TEST_ASSERT(handled, "KdpTrap must intercept first-chance breakpoint exception");

        // Verify state change packet generated in transport outbound queue
        TEST_ASSERT(debugger.getTransport().getOutboundQueueCount() == 1, "Transport should contain 1 outbound packet");
        auto rawPkt = debugger.getTransport().popOutboundPacket();
        TEST_ASSERT(rawPkt.size() == sizeof(kd::KD_PACKET) + sizeof(kd::DBGKD_WAIT_STATE_CHANGE64), "Packet size must match header + DBGKD_WAIT_STATE_CHANGE64");

        const auto* hdr = reinterpret_cast<const kd::KD_PACKET*>(rawPkt.data());
        TEST_ASSERT(hdr->packetLeader == kd::PACKET_LEADER, "Packet leader must be PACKET_LEADER");
        TEST_ASSERT(hdr->packetType == kd::PACKET_TYPE_KD_STATE_CHANGE64, "Packet type must be PACKET_TYPE_KD_STATE_CHANGE64");

        const auto* sc = reinterpret_cast<const kd::DBGKD_WAIT_STATE_CHANGE64*>(rawPkt.data() + sizeof(kd::KD_PACKET));
        TEST_ASSERT(sc->newStatus == ke::EXCEPTION_BREAKPOINT, "State change status must match exception code");
        TEST_ASSERT(sc->programCounter == 0xFFFFF80000500123ULL, "State change PC must match RIP");
        TEST_ASSERT(sc->exception.exceptionCode == ke::EXCEPTION_BREAKPOINT, "Exception record code must match");
        TEST_ASSERT(sc->exception.exceptionAddress == 0xFFFFF80000500123ULL, "Exception record address must match");
        TEST_ASSERT(sc->controlReport.segCs == 0x10, "Control report CS must match");
        TEST_ASSERT(sc->controlReport.eflags == 0x202, "Control report EFLAGS must match");
    }

    // 8. DbgPrint Ring Buffer & Polling Break-in
    {
        debugger.getRingBuffer().clear();
        kd::DbgPrint("MicaNT Kernel Debugger KD64 Active\n");
        kd::DbgPrintEx(0, 0, "KDCOM baud rate 115200 initialized\n");

        auto logs = debugger.getRingBuffer().getLogs();
        TEST_ASSERT(logs.size() == 2, "Ring buffer must contain 2 logged messages");
        TEST_ASSERT(logs[0].find("MicaNT Kernel Debugger KD64 Active") != std::string::npos, "First log must contain message");
        TEST_ASSERT(logs[1].find("KDCOM baud rate 115200") != std::string::npos, "Second log must contain message");

        // Polling break-in
        TEST_ASSERT(!kd::KdPollBreakIn(), "Initial break-in poll must be false");
        debugger.signalBreakIn();
        TEST_ASSERT(kd::KdPollBreakIn(), "Break-in poll must return true after break-in signaled");
        TEST_ASSERT(!kd::KdPollBreakIn(), "Break-in poll must clear pending flag and return false on second poll");
    }

    // 9. Debugger Enable/Disable State Toggle
    {
        debugger.setDebuggerEnabled(false);
        TEST_ASSERT(!debugger.isDebuggerEnabled(), "Debugger must report disabled");
        TEST_ASSERT(debugger.isDebuggerNotPresent(), "isDebuggerNotPresent must return true when disabled");

        // KdpTrap should not handle traps when disabled
        ke::ExceptionRecord record{
            .exceptionCode = ke::EXCEPTION_BREAKPOINT,
            .exceptionAddress = reinterpret_cast<void*>(0x1000ULL),
            .numberParameters = 0
        };
        ps::ContextFrame ctx{};
        bool handledDisabled = kd::KdpTrap(record, ctx, false);
        TEST_ASSERT(!handledDisabled, "KdpTrap must not intercept when debugger is disabled");

        // Re-enable
        debugger.setDebuggerEnabled(true);
        TEST_ASSERT(debugger.isDebuggerEnabled(), "Debugger must report enabled");
    }

    // 10. KD Diagnostic Telemetry Audit & Testing Reset
    {
        kd::KD_TELEMETRY telem = kd::KdQueryTelemetry();
        TEST_ASSERT(telem.totalPacketsSent > 0, "totalPacketsSent must be > 0");
        TEST_ASSERT(telem.totalPacketsReceived > 0, "totalPacketsReceived must be > 0");
        TEST_ASSERT(telem.totalAcksSent > 0, "totalAcksSent must be > 0");
        TEST_ASSERT(telem.totalAcksReceived > 0, "totalAcksReceived must be > 0");
        TEST_ASSERT(telem.totalResendsRequested > 0, "totalResendsRequested must be > 0");
        TEST_ASSERT(telem.totalBreakinsReceived > 0, "totalBreakinsReceived must be > 0");
        TEST_ASSERT(telem.totalStateChangesDispatched > 0, "totalStateChangesDispatched must be > 0");
        TEST_ASSERT(telem.totalManipulateRequestsHandled > 0, "totalManipulateRequestsHandled must be > 0");
        TEST_ASSERT(telem.totalDbgPrintsLogged > 0, "totalDbgPrintsLogged must be > 0");
        TEST_ASSERT(telem.totalMemoryReads > 0, "totalMemoryReads must be > 0");
        TEST_ASSERT(telem.totalMemoryWrites > 0, "totalMemoryWrites > 0");
        TEST_ASSERT(telem.totalPhysicalMemoryReads > 0, "totalPhysicalMemoryReads must be > 0");
        TEST_ASSERT(telem.totalPhysicalMemoryWrites > 0, "totalPhysicalMemoryWrites must be > 0");
        TEST_ASSERT(telem.totalSoftwareBreakpointsSet > 0, "totalSoftwareBreakpointsSet must be > 0");
        TEST_ASSERT(telem.totalSoftwareBreakpointsCleared > 0, "totalSoftwareBreakpointsCleared must be > 0");
        TEST_ASSERT(telem.totalHardwareBreakpointsHit > 0, "totalHardwareBreakpointsHit must be > 0");
        TEST_ASSERT(telem.totalContextQueries > 0, "totalContextQueries must be > 0");
        TEST_ASSERT(telem.totalContextSets > 0, "totalContextSets must be > 0");
        TEST_ASSERT(telem.totalExceptionsHandledByKd > 0, "totalExceptionsHandledByKd must be > 0");

        // Reset telemetry
        kd::KdResetTelemetryForTesting();
        kd::KD_TELEMETRY resetTelem = kd::KdQueryTelemetry();
        TEST_ASSERT(resetTelem.totalPacketsSent == 0, "totalPacketsSent must be 0 after reset");
        TEST_ASSERT(resetTelem.totalSoftwareBreakpointsSet == 0, "totalSoftwareBreakpointsSet must be 0 after reset");
        TEST_ASSERT(resetTelem.totalExceptionsHandledByKd == 0, "totalExceptionsHandledByKd must be 0 after reset");
    }

    std::cout << "[TEST] Suite 246: Horizon 30 - Kernel Debugger Engine (KD64), KDCom Protocol, Breakpoints & KdpTrap Exception Routing PASSED.\n";
}

// ============================================================================
// Suite 247: Horizon 31 - Windows Driver Framework (KMDF / UMDF 2.0),
// Object Tree Lifetimes, PnP & Power State Machines, I/O Queues & Requests
// ============================================================================
void Test_Horizon31_WindowsDriverFramework_KMDF_ObjectTree_PnP_Queues_And_Requests() {
    wdf::TitanWdfEngine::Instance().resetForTesting();

    // 1. WDF Object Model, Parent-Child Hierarchy & Typed Context Space
    struct SampleDriverContext {
        uint32_t Magic{0x1337BEEF};
        char Tag[16]{"KMDF_DRIVER"};
    };

    uint32_t driverCleanupCalls = 0;
    uint32_t driverDestroyCalls = 0;

    WDFDRIVER driver = nullptr;
    {
        WDF_DRIVER_CONFIG drvConfig{};
        WDF_DRIVER_CONFIG_INIT(&drvConfig, [](WDFDRIVER, WDFDEVICE_INIT*) -> NTSTATUS {
            return STATUS_SUCCESS;
        });

        WDF_OBJECT_ATTRIBUTES drvAttrs{};
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&drvAttrs, SampleDriverContext);
        drvAttrs.EvtCleanupCallback = [&](WDFOBJECT) { driverCleanupCalls++; };
        drvAttrs.EvtDestroyCallback = [&](WDFOBJECT) { driverDestroyCalls++; };

        NTSTATUS stDrv = WdfDriverCreate(nullptr, L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\SampleKmdf", &drvAttrs, &drvConfig, &driver);
        TEST_ASSERT(NT_SUCCESS(stDrv), "WdfDriverCreate must succeed");
        TEST_ASSERT(driver != nullptr, "Created WDFDRIVER handle must be non-null");

        auto* ctx = WdfObjectGetTypedContext(driver, SampleDriverContext);
        TEST_ASSERT(ctx != nullptr, "WdfObjectGetTypedContext must return non-null context memory");
        ctx->Magic = 0x1337BEEF;
        std::snprintf(ctx->Tag, sizeof(ctx->Tag), "KMDF_DRIVER");
        TEST_ASSERT(ctx->Magic == 0x1337BEEF, "Typed context initialized properly");
        TEST_ASSERT(std::string(ctx->Tag) == "KMDF_DRIVER", "Typed context tag matches");
    }

    // 2. WDFDEVICE Creation, PrepareHardware & Power (D0..D3) State Machine
    WDFDEVICE device = nullptr;
    uint32_t prepareHardwareCalls = 0;
    uint32_t d0EntryCalls = 0;
    uint32_t d0ExitCalls = 0;
    uint32_t ioSuspendCalls = 0;
    uint32_t ioRestartCalls = 0;

    {
        WDFDEVICE_INIT* devInit = WdfControlDeviceInitAllocate(driver, nullptr);
        TEST_ASSERT(devInit != nullptr, "WdfControlDeviceInitAllocate must return valid init structure");
        devInit->DeviceName = "\\Device\\SampleKmdfDevice0";
        devInit->HardwareId = "PCI\\VEN_1337&DEV_0001";

        WDF_PNPPOWER_EVENT_CALLBACKS pnpCallbacks{};
        WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpCallbacks);
        pnpCallbacks.EvtDevicePrepareHardware = [&](WDFDEVICE) -> NTSTATUS {
            prepareHardwareCalls++;
            return STATUS_SUCCESS;
        };
        pnpCallbacks.EvtDeviceD0Entry = [&](WDFDEVICE, WDF_DEVICE_POWER_STATE) -> NTSTATUS {
            d0EntryCalls++;
            return STATUS_SUCCESS;
        };
        pnpCallbacks.EvtDeviceD0Exit = [&](WDFDEVICE, WDF_DEVICE_POWER_STATE) -> NTSTATUS {
            d0ExitCalls++;
            return STATUS_SUCCESS;
        };
        pnpCallbacks.EvtDeviceSelfManagedIoSuspend = [&](WDFDEVICE) -> NTSTATUS {
            ioSuspendCalls++;
            return STATUS_SUCCESS;
        };
        pnpCallbacks.EvtDeviceSelfManagedIoRestart = [&](WDFDEVICE) -> NTSTATUS {
            ioRestartCalls++;
            return STATUS_SUCCESS;
        };

        WdfDeviceInitSetPnpPowerEventCallbacks(devInit, &pnpCallbacks);

        WDF_OBJECT_ATTRIBUTES devAttrs{};
        WDF_OBJECT_ATTRIBUTES_INIT(&devAttrs);
        devAttrs.ParentObject = driver;

        NTSTATUS stDev = WdfDeviceCreate(&devInit, &devAttrs, &device);
        TEST_ASSERT(NT_SUCCESS(stDev), "WdfDeviceCreate must succeed");
        TEST_ASSERT(device != nullptr, "WDFDEVICE handle must be valid");
        TEST_ASSERT(devInit == nullptr, "WdfDeviceCreate must invalidate devInit pointer");

        TEST_ASSERT(prepareHardwareCalls == 1, "EvtDevicePrepareHardware must be called once during startup");
        TEST_ASSERT(d0EntryCalls == 1, "EvtDeviceD0Entry must be called once on initial D0 transition");

        // Power State Transition: D0 -> D3 (Sleep)
        NTSTATUS stPowerDown = WdfDeviceSetPowerState(device, WdfDevStatePowerD3);
        TEST_ASSERT(NT_SUCCESS(stPowerDown), "WdfDeviceSetPowerState(D3) must succeed");
        TEST_ASSERT(d0ExitCalls == 1, "EvtDeviceD0Exit must be invoked when transitioning to D3");
        TEST_ASSERT(ioSuspendCalls == 1, "EvtDeviceSelfManagedIoSuspend must be called during power down");

        // Power State Transition: D3 -> D0 (Wake)
        NTSTATUS stPowerUp = WdfDeviceSetPowerState(device, WdfDevStatePowerD0);
        TEST_ASSERT(NT_SUCCESS(stPowerUp), "WdfDeviceSetPowerState(D0) must succeed");
        TEST_ASSERT(d0EntryCalls == 2, "EvtDeviceD0Entry must be called again when returning to D0");
        TEST_ASSERT(ioRestartCalls == 1, "EvtDeviceSelfManagedIoRestart must be called on wake");
    }

    // 3. Sequential I/O Queue Dispatching & Automatic Serialization Pacing
    {
        WDFQUEUE seqQueue = nullptr;
        WDF_IO_QUEUE_CONFIG qConfig{};
        WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&qConfig, WdfIoQueueDispatchSequential);

        std::vector<uint32_t> dispatchedOrders;
        qConfig.EvtIoDeviceControl = [&](WDFQUEUE, WDFREQUEST, size_t, size_t, uint32_t ioctl) {
            dispatchedOrders.push_back(ioctl);
        };

        WDF_OBJECT_ATTRIBUTES qAttrs{};
        WDF_OBJECT_ATTRIBUTES_INIT(&qAttrs);
        qAttrs.ParentObject = device;

        NTSTATUS stQ = WdfIoQueueCreate(device, &qConfig, &qAttrs, &seqQueue);
        TEST_ASSERT(NT_SUCCESS(stQ), "WdfIoQueueCreate for sequential queue must succeed");
        TEST_ASSERT(seqQueue != nullptr, "Sequential queue handle must be valid");
        TEST_ASSERT(WdfDeviceGetDefaultQueue(device) == seqQueue, "Sequential queue must be registered as default queue");

        // Create 3 requests
        WDFREQUEST req1 = nullptr;
        WDFREQUEST req2 = nullptr;
        WDFREQUEST req3 = nullptr;

        WdfRequestCreate(nullptr, nullptr, &req1);
        WdfRequestCreate(nullptr, nullptr, &req2);
        WdfRequestCreate(nullptr, nullptr, &req3);

        WdfRequestSetParameters(req1, WdfRequestTypeDeviceControl, 101, nullptr, 0, nullptr, 0);
        WdfRequestSetParameters(req2, WdfRequestTypeDeviceControl, 102, nullptr, 0, nullptr, 0);
        WdfRequestSetParameters(req3, WdfRequestTypeDeviceControl, 103, nullptr, 0, nullptr, 0);

        // Dispatch all 3 requests
        WdfIoQueueDispatch(seqQueue, req1);
        WdfIoQueueDispatch(seqQueue, req2);
        WdfIoQueueDispatch(seqQueue, req3);

        // In Sequential mode, only Req1 should have been delivered!
        TEST_ASSERT(dispatchedOrders.size() == 1, "Only 1 request must be dispatched immediately in sequential queue");
        TEST_ASSERT(dispatchedOrders[0] == 101, "First dispatched request must be IOCTL 101");
        TEST_ASSERT(WdfIoQueueGetInFlightCount(seqQueue) == 1, "In-flight count must be 1");
        TEST_ASSERT(WdfIoQueueGetPendingCount(seqQueue) == 2, "Pending count must be 2");

        // Complete Req1 -> Req2 should be automatically dispatched
        WdfRequestComplete(req1, STATUS_SUCCESS);
        TEST_ASSERT(WdfRequestIsCompleted(req1), "Req1 must report completed");
        TEST_ASSERT(dispatchedOrders.size() == 2, "Second request must be dispatched automatically after Req1 completes");
        TEST_ASSERT(dispatchedOrders[1] == 102, "Second dispatched request must be IOCTL 102");

        // Complete Req2 -> Req3 should be automatically dispatched
        WdfRequestComplete(req2, STATUS_SUCCESS);
        TEST_ASSERT(dispatchedOrders.size() == 3, "Third request must be dispatched automatically after Req2 completes");
        TEST_ASSERT(dispatchedOrders[2] == 103, "Third dispatched request must be IOCTL 103");

        // Complete Req3 -> queue should be idle
        WdfRequestComplete(req3, STATUS_SUCCESS);
        TEST_ASSERT(WdfIoQueueGetInFlightCount(seqQueue) == 0, "In-flight count must return to 0");
        TEST_ASSERT(WdfIoQueueGetPendingCount(seqQueue) == 0, "Pending count must be 0");
    }

    // 4. Parallel I/O Queue Dispatching & High-Throughput Concurrency
    {
        WDFQUEUE parQueue = nullptr;
        WDF_IO_QUEUE_CONFIG qConfig{};
        WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&qConfig, WdfIoQueueDispatchParallel);

        uint32_t parallelDispatchCount = 0;
        qConfig.EvtIoWrite = [&](WDFQUEUE, WDFREQUEST, size_t) {
            parallelDispatchCount++;
        };

        NTSTATUS stQ = WdfIoQueueCreate(device, &qConfig, nullptr, &parQueue);
        TEST_ASSERT(NT_SUCCESS(stQ), "WdfIoQueueCreate for parallel queue must succeed");

        WDFREQUEST reqA = nullptr;
        WDFREQUEST reqB = nullptr;
        WDFREQUEST reqC = nullptr;

        WdfRequestCreate(nullptr, nullptr, &reqA);
        WdfRequestCreate(nullptr, nullptr, &reqB);
        WdfRequestCreate(nullptr, nullptr, &reqC);

        uint8_t buffer[64]{0};
        WdfRequestSetParameters(reqA, WdfRequestTypeWrite, 0, buffer, sizeof(buffer), nullptr, 0);
        WdfRequestSetParameters(reqB, WdfRequestTypeWrite, 0, buffer, sizeof(buffer), nullptr, 0);
        WdfRequestSetParameters(reqC, WdfRequestTypeWrite, 0, buffer, sizeof(buffer), nullptr, 0);

        WdfIoQueueDispatch(parQueue, reqA);
        WdfIoQueueDispatch(parQueue, reqB);
        WdfIoQueueDispatch(parQueue, reqC);

        // In Parallel mode, all 3 requests are delivered immediately
        TEST_ASSERT(parallelDispatchCount == 3, "All 3 requests must be dispatched immediately in parallel queue");
        TEST_ASSERT(WdfIoQueueGetInFlightCount(parQueue) == 3, "In-flight count must be 3");

        // Complete with information bytes
        WdfRequestCompleteWithInformation(reqA, STATUS_SUCCESS, 64);
        WdfRequestCompleteWithInformation(reqB, STATUS_SUCCESS, 64);
        WdfRequestCompleteWithInformation(reqC, STATUS_SUCCESS, 64);

        TEST_ASSERT(WdfRequestGetStatus(reqA) == STATUS_SUCCESS, "ReqA completion status must be SUCCESS");
        TEST_ASSERT(WdfRequestGetInformation(reqA) == 64, "ReqA transferred information must be 64 bytes");
        TEST_ASSERT(WdfIoQueueGetInFlightCount(parQueue) == 0, "In-flight count must return to 0 after all complete");
    }

    // 5. Manual I/O Queue Dispatching & Explicit Request Retrieval
    {
        WDFQUEUE manQueue = nullptr;
        WDF_IO_QUEUE_CONFIG qConfig{};
        WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&qConfig, WdfIoQueueDispatchManual);

        NTSTATUS stQ = WdfIoQueueCreate(device, &qConfig, nullptr, &manQueue);
        TEST_ASSERT(NT_SUCCESS(stQ), "WdfIoQueueCreate for manual queue must succeed");

        WDFREQUEST reqM = nullptr;
        WdfRequestCreate(nullptr, nullptr, &reqM);
        WdfRequestSetParameters(reqM, WdfRequestTypeRead, 0, nullptr, 0, nullptr, 128);

        WdfIoQueueDispatch(manQueue, reqM);
        TEST_ASSERT(WdfIoQueueGetPendingCount(manQueue) == 1, "Manual queue must hold request in pending list");

        WDFREQUEST retrievedReq = nullptr;
        NTSTATUS stRetr = WdfIoQueueRetrieveNextRequest(manQueue, &retrievedReq);
        TEST_ASSERT(NT_SUCCESS(stRetr), "WdfIoQueueRetrieveNextRequest must succeed");
        TEST_ASSERT(retrievedReq == reqM, "Retrieved request must match queued request");

        // Second retrieval on empty manual queue
        WDFREQUEST secondRetr = nullptr;
        NTSTATUS stEmpty = WdfIoQueueRetrieveNextRequest(manQueue, &secondRetr);
        TEST_ASSERT(stEmpty == STATUS_NO_MORE_ENTRIES, "Retrieval from empty queue must return STATUS_NO_MORE_ENTRIES");

        WdfRequestComplete(retrievedReq, STATUS_SUCCESS);
    }

    // 6. Queue Flow Control: Start, Stop, and Purge
    {
        WDFQUEUE ctlQueue = nullptr;
        WDF_IO_QUEUE_CONFIG qConfig{};
        WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&qConfig, WdfIoQueueDispatchManual);
        WdfIoQueueCreate(device, &qConfig, nullptr, &ctlQueue);

        WDFREQUEST pReq1 = nullptr;
        WDFREQUEST pReq2 = nullptr;
        WdfRequestCreate(nullptr, nullptr, &pReq1);
        WdfRequestCreate(nullptr, nullptr, &pReq2);

        WdfIoQueueDispatch(ctlQueue, pReq1);
        WdfIoQueueDispatch(ctlQueue, pReq2);
        TEST_ASSERT(WdfIoQueueGetPendingCount(ctlQueue) == 2, "Pending count must be 2 before purge");

        WdfIoQueuePurge(ctlQueue);
        TEST_ASSERT(WdfIoQueueGetPendingCount(ctlQueue) == 0, "Pending count must be 0 after purge");
        TEST_ASSERT(WdfRequestGetStatus(pReq1) == STATUS_CANCELLED, "Purged request 1 must be completed with STATUS_CANCELLED");
        TEST_ASSERT(WdfRequestGetStatus(pReq2) == STATUS_CANCELLED, "Purged request 2 must be completed with STATUS_CANCELLED");
    }

    // 7. WDFREQUEST Cancellation Handlers
    {
        WDFREQUEST cReq = nullptr;
        WdfRequestCreate(nullptr, nullptr, &cReq);

        bool cancelFired = false;
        WdfRequestMarkCancelable(cReq, [&](WDFREQUEST req) {
            cancelFired = true;
            WdfRequestComplete(req, STATUS_CANCELLED);
        });

        NTSTATUS stCancel = WdfRequestCancel(cReq);
        TEST_ASSERT(NT_SUCCESS(stCancel), "WdfRequestCancel must succeed");
        TEST_ASSERT(cancelFired, "Cancel routine must be invoked");
        TEST_ASSERT(WdfRequestIsCanceled(cReq), "Request must report canceled");
        TEST_ASSERT(WdfRequestGetStatus(cReq) == STATUS_CANCELLED, "Completion status must be STATUS_CANCELLED");
    }

    // 8. WDFMEMORY Allocation, Buffer Mapping & Safe Copy
    {
        WDFMEMORY mem = nullptr;
        void* rawBuf = nullptr;
        const size_t bufSize = 512;

        NTSTATUS stMem = WdfMemoryCreate(nullptr, 0, 0x4D494341 /* 'MICA' */, bufSize, &mem, &rawBuf);
        TEST_ASSERT(NT_SUCCESS(stMem), "WdfMemoryCreate must succeed");
        TEST_ASSERT(mem != nullptr, "WDFMEMORY handle must be non-null");
        TEST_ASSERT(rawBuf != nullptr, "Raw buffer pointer must be non-null");

        size_t querySize = 0;
        void* queriedBuf = WdfMemoryGetBuffer(mem, &querySize);
        TEST_ASSERT(queriedBuf == rawBuf, "WdfMemoryGetBuffer must match allocated pointer");
        TEST_ASSERT(querySize == bufSize, "WdfMemoryGetBuffer must return 512 bytes");

        const char testString[] = "Sovereign WDF Memory Buffering Engine 2026";
        size_t strLen = sizeof(testString);
        NTSTATUS stCopyIn = WdfMemoryCopyFromBuffer(mem, 0, const_cast<char*>(testString), strLen);
        TEST_ASSERT(NT_SUCCESS(stCopyIn), "WdfMemoryCopyFromBuffer must succeed");

        char readOut[64]{0};
        NTSTATUS stCopyOut = WdfMemoryCopyToBuffer(mem, 0, readOut, strLen);
        TEST_ASSERT(NT_SUCCESS(stCopyOut), "WdfMemoryCopyToBuffer must succeed");
        TEST_ASSERT(std::memcmp(readOut, testString, strLen) == 0, "Copied buffer must match original string");

        // Buffer overflow protection test
        char overflowBuf[1024];
        NTSTATUS stOverflow = WdfMemoryCopyFromBuffer(mem, 0, overflowBuf, sizeof(overflowBuf));
        TEST_ASSERT(stOverflow == STATUS_BUFFER_TOO_SMALL, "Oversized buffer copy must return STATUS_BUFFER_TOO_SMALL");
    }

    // 9. UMDF 2.0 User-Mode Driver Host Process Isolation & Reflector Recovery
    {
        uint32_t umdfPid = 0;
        NTSTATUS stHost = wdf::TitanWdfEngine::Instance().startUmdfDriver("sensors_hid_umdf.dll", &umdfPid);
        TEST_ASSERT(NT_SUCCESS(stHost), "startUmdfDriver must succeed");
        TEST_ASSERT(umdfPid >= 5000, "UMDF host process must be assigned valid PID");

        auto hosts = wdf::TitanWdfEngine::Instance().getUmdfHostsSnapshot();
        TEST_ASSERT(hosts.find(umdfPid) != hosts.end(), "UMDF host must exist in active snapshot");
        TEST_ASSERT(hosts[umdfPid].IsHealthy, "UMDF host must start in healthy state");

        // Simulate userland driver crash
        NTSTATUS stCrash = wdf::TitanWdfEngine::Instance().simulateUmdfCrash(umdfPid, true);
        TEST_ASSERT(NT_SUCCESS(stCrash), "simulateUmdfCrash with auto-recovery must succeed");

        hosts = wdf::TitanWdfEngine::Instance().getUmdfHostsSnapshot();
        TEST_ASSERT(hosts[umdfPid].CrashesRecovered == 1, "CrashesRecovered counter must increment to 1");
        TEST_ASSERT(hosts[umdfPid].IsHealthy, "UMDF host must be restored to healthy status after recovery");
    }

    // 10. Cascading Lifetime Deletion & WDF Diagnostic Telemetry Audit
    {
        // Deleting driver cascades and deletes child device, queues, and requests
        WdfObjectDelete(driver);
        TEST_ASSERT(driverCleanupCalls == 1, "EvtCleanupCallback must be invoked upon WdfObjectDelete");
        TEST_ASSERT(driverDestroyCalls == 1, "EvtDestroyCallback must be invoked upon WdfObjectDelete");

        // Telemetry Audit
        WDF_TELEMETRY telem = WdfQueryTelemetry();
        TEST_ASSERT(telem.totalDriversCreated > 0, "totalDriversCreated must be > 0");
        TEST_ASSERT(telem.totalDevicesCreated > 0, "totalDevicesCreated must be > 0");
        TEST_ASSERT(telem.totalQueuesCreated > 0, "totalQueuesCreated must be > 0");
        TEST_ASSERT(telem.totalRequestsCreated > 0, "totalRequestsCreated must be > 0");
        TEST_ASSERT(telem.totalRequestsDispatched > 0, "totalRequestsDispatched must be > 0");
        TEST_ASSERT(telem.totalRequestsCompleted > 0, "totalRequestsCompleted must be > 0");
        TEST_ASSERT(telem.totalMemoriesAllocated > 0, "totalMemoriesAllocated must be > 0");
        TEST_ASSERT(telem.totalPnpTransitions > 0, "totalPnpTransitions must be > 0");
        TEST_ASSERT(telem.totalPowerTransitions > 0, "totalPowerTransitions must be > 0");
        TEST_ASSERT(telem.totalUmdfCrashesRecovered > 0, "totalUmdfCrashesRecovered must be > 0");

        // Reset Telemetry
        WdfResetTelemetryForTesting();
        WDF_TELEMETRY resetTelem = WdfQueryTelemetry();
        TEST_ASSERT(resetTelem.totalDriversCreated == 0, "totalDriversCreated must be 0 after reset");
        TEST_ASSERT(resetTelem.totalDevicesCreated == 0, "totalDevicesCreated must be 0 after reset");
        TEST_ASSERT(resetTelem.totalRequestsDispatched == 0, "totalRequestsDispatched must be 0 after reset");
    }

    std::cout << "[TEST] Suite 247: Horizon 31 - Windows Driver Framework (KMDF / UMDF 2.0), Object Tree, PnP, Queues & Requests PASSED.\n";
}

void Test_Horizon32_StorPort_Nvme_Storage_Architecture_And_Miniport() {
    using namespace micant;
    using namespace micant::nvme;

    // Phase 1: BAR0 MMIO Register Map & Controller Initialization / Enable FSM
    {
        NvmeController ctrl(NvmeVersion::Version_2_0);
        uint32_t capLow = ctrl.readReg32(NVME_REG_CAP);
        uint32_t capHigh = ctrl.readReg32(NVME_REG_CAP + 4);
        TEST_ASSERT(capLow != 0 || capHigh != 0, "CAP register must be populated");

        uint32_t vs = ctrl.readReg32(NVME_REG_VS);
        TEST_ASSERT(vs == static_cast<uint32_t>(NvmeVersion::Version_2_0), "VS register must match NVMe 2.0 (0x00020000)");

        // Initial state: not ready
        TEST_ASSERT(!ctrl.isReady(), "Controller must not be ready before CC.EN is set");
        TEST_ASSERT((ctrl.readReg32(NVME_REG_CSTS) & NVME_CSTS_RDY) == 0, "CSTS.RDY must be 0 initially");

        // Set AQA, ASQ, ACQ
        ctrl.writeReg32(NVME_REG_AQA, 0x003F003F); // 64 entries ASQ and ACQ
        TEST_ASSERT(ctrl.readReg32(NVME_REG_AQA) == 0x003F003F, "AQA must reflect written value");

        ctrl.writeReg32(NVME_REG_ASQ, 0x1000);
        ctrl.writeReg32(NVME_REG_ASQ + 4, 0x0000);
        ctrl.writeReg32(NVME_REG_ACQ, 0x2000);
        ctrl.writeReg32(NVME_REG_ACQ + 4, 0x0000);

        // Enable controller: CC.EN = 1
        uint32_t ccVal = NVME_CC_EN | NVME_CC_IOSQES_64 | NVME_CC_IOCQES_16;
        ctrl.writeReg32(NVME_REG_CC, ccVal);
        TEST_ASSERT(ctrl.isReady(), "Controller must report isReady() == true after CC.EN");
        TEST_ASSERT((ctrl.readReg32(NVME_REG_CSTS) & NVME_CSTS_RDY) != 0, "CSTS.RDY bit must be set");

        // Normal shutdown notification
        ctrl.writeReg32(NVME_REG_CC, ccVal | NVME_CC_SHN_NORM);
        TEST_ASSERT((ctrl.readReg32(NVME_REG_CSTS) & NVME_CSTS_SHST_OK) != 0, "CSTS.SHST_OK must be set on normal shutdown");

        // Re-enable
        ctrl.writeReg32(NVME_REG_CC, ccVal);
    }

    // Phase 2: Admin Queue & Identify Controller and Namespaces
    {
        NvmeController ctrl(NvmeVersion::Version_2_0);
        ctrl.writeReg32(NVME_REG_CC, NVME_CC_EN | NVME_CC_IOSQES_64 | NVME_CC_IOCQES_16);

        // Identify Controller (CNS = 0x01)
        NvmeIdentifyController idCtrl{};
        NvmeSqe sqeCtrl{};
        sqeCtrl.opcode = NVME_ADMIN_IDENTIFY;
        sqeCtrl.commandId = 101;
        sqeCtrl.cdw10 = NVME_IDENTIFY_CNS_CTRL;

        NvmeCqe cqeCtrl = ctrl.executeAdminCommand(sqeCtrl, &idCtrl, sizeof(idCtrl));
        TEST_ASSERT(cqeCtrl.getStatusCode() == NVME_SC_SUCCESS, "Identify Controller must succeed");
        TEST_ASSERT(cqeCtrl.commandId == 101, "CQE command ID must match SQE");
        TEST_ASSERT(idCtrl.vid == 0x144D, "Identify Controller VID must match 0x144D");
        TEST_ASSERT(std::memcmp(idCtrl.mn, "TitanNVMe Sovereign Flash SSD 2TB", 34) == 0, "Identify Controller model name match");

        // Identify Namespace 1 (CNS = 0x00, NSID = 1)
        NvmeIdentifyNamespace idNs1{};
        NvmeSqe sqeNs1{};
        sqeNs1.opcode = NVME_ADMIN_IDENTIFY;
        sqeNs1.commandId = 102;
        sqeNs1.nsid = 1;
        sqeNs1.cdw10 = NVME_IDENTIFY_CNS_NS;

        NvmeCqe cqeNs1 = ctrl.executeAdminCommand(sqeNs1, &idNs1, sizeof(idNs1));
        TEST_ASSERT(cqeNs1.getStatusCode() == NVME_SC_SUCCESS, "Identify Namespace 1 must succeed");
        TEST_ASSERT(idNs1.nsze > 0, "Namespace 1 size must be > 0");
        TEST_ASSERT(idNs1.lbaf[0].lbaDataSizeShift == 9, "Namespace 1 format 0 must be 512B (shift 9)");

        // Identify Namespace 2 (CNS = 0x00, NSID = 2, 4Kn)
        NvmeIdentifyNamespace idNs2{};
        NvmeSqe sqeNs2{};
        sqeNs2.opcode = NVME_ADMIN_IDENTIFY;
        sqeNs2.commandId = 103;
        sqeNs2.nsid = 2;
        sqeNs2.cdw10 = NVME_IDENTIFY_CNS_NS;

        NvmeCqe cqeNs2 = ctrl.executeAdminCommand(sqeNs2, &idNs2, sizeof(idNs2));
        TEST_ASSERT(cqeNs2.getStatusCode() == NVME_SC_SUCCESS, "Identify Namespace 2 must succeed");
        TEST_ASSERT(idNs2.flbas == 1, "Namespace 2 must use 4096-byte LBA format (flbas=1)");
        TEST_ASSERT(idNs2.lbaf[1].lbaDataSizeShift == 12, "Namespace 2 format 1 must be 4096B (shift 12)");

        // Identify Active Namespaces (CNS = 0x02)
        uint32_t activeList[4]{};
        NvmeSqe sqeActive{};
        sqeActive.opcode = NVME_ADMIN_IDENTIFY;
        sqeActive.commandId = 104;
        sqeActive.cdw10 = NVME_IDENTIFY_CNS_ACTIVE_NS;

        NvmeCqe cqeActive = ctrl.executeAdminCommand(sqeActive, activeList, sizeof(activeList));
        TEST_ASSERT(cqeActive.getStatusCode() == NVME_SC_SUCCESS, "Identify Active Namespaces must succeed");
        TEST_ASSERT(activeList[0] == 1, "First active NSID must be 1");
        TEST_ASSERT(activeList[1] == 2, "Second active NSID must be 2");
    }

    // Phase 3: Multi-Queue dynamic allocation, doorbell writes, ring buffer dispatch, phase toggling, MSI-X
    {
        NvmeController ctrl(NvmeVersion::Version_2_0);
        ctrl.writeReg32(NVME_REG_CC, NVME_CC_EN | NVME_CC_IOSQES_64 | NVME_CC_IOCQES_16);

        // Create I/O Queue Pair (QID = 1, size = 16 entries)
        NTSTATUS stCq = ctrl.createIoQueuePair(1, 16, 16, 0x10000, 0x20000);
        TEST_ASSERT(NT_SUCCESS(stCq), "createIoQueuePair must succeed");
        TEST_ASSERT(ctrl.getQueueCount() == 2, "Queue count must be 2 (Admin + IO Q1)");

        // Post 4 commands to I/O SQ 1 to test queue dispatch and MSI-X generation
        for (uint16_t i = 0; i < 4; ++i) {
            NvmeSqe sqe{};
            sqe.opcode = NVME_NVM_FLUSH;
            sqe.commandId = 200 + i;
            sqe.nsid = 1;
            NTSTATUS postSt = ctrl.postSqe(1, sqe);
            TEST_ASSERT(NT_SUCCESS(postSt), "postSqe must succeed");
        }

        // Ring SQ Doorbell for QID 1 (tail = 4)
        ctrl.ringDoorbell(1, false, 4);

        // Verify MSI-X interrupt counter incremented for QID 1
        uint64_t msix = ctrl.getMsixInterrupts(1);
        TEST_ASSERT(msix == 4, "MSI-X interrupt count must match number of completed commands (4)");

        // Delete I/O Queue
        NTSTATUS delSt = ctrl.deleteIoQueuePair(1);
        TEST_ASSERT(NT_SUCCESS(delSt), "deleteIoQueuePair must succeed");
        TEST_ASSERT(ctrl.getQueueCount() == 1, "Queue count must return to 1");
    }

    // Phase 4: Physical Region Page (PRP) scatter-gather zero-copy DMA
    {
        PrpDmaEngine dma;
        NvmeController ctrl(NvmeVersion::Version_2_0);
        ctrl.writeReg32(NVME_REG_CC, NVME_CC_EN | NVME_CC_IOSQES_64 | NVME_CC_IOCQES_16);
        auto ns = ctrl.getNamespace(1);
        TEST_ASSERT(ns != nullptr, "Namespace 1 must exist");

        // Case A: Single Page (2048 bytes <= 4096) -> prp1 set, prp2 == 0
        std::vector<uint8_t> hostBufA(2048, 0xAA);
        uint64_t prp1A = 0, prp2A = 0;
        std::vector<uint64_t> listA;
        dma.buildPrpChain(hostBufA.data(), hostBufA.size(), prp1A, prp2A, listA);
        TEST_ASSERT(prp1A != 0, "prp1 must be non-zero");
        TEST_ASSERT(prp2A == 0, "prp2 must be 0 for <= 4096 bytes");
        TEST_ASSERT(listA.empty(), "PRP list must be empty for <= 4096 bytes");

        NvmeSqe writeSqeA{};
        writeSqeA.opcode = NVME_NVM_WRITE;
        writeSqeA.commandId = 301;
        writeSqeA.nsid = 1;
        writeSqeA.cdw10 = 100; // SLBA = 100
        writeSqeA.cdw12 = (2048 / 512) - 1; // 4 blocks
        writeSqeA.prp1 = prp1A;
        writeSqeA.prp2 = prp2A;

        NvmeCqe cqeA = ctrl.executeIoCommandWithPrp(writeSqeA, dma);
        TEST_ASSERT(cqeA.getStatusCode() == NVME_SC_SUCCESS, "PRP Write A must succeed");

        // Read back with DMA
        uint64_t rPrp1A = 0, rPrp2A = 0;
        std::vector<uint64_t> rListA;
        dma.buildPrpChain(nullptr, 2048, rPrp1A, rPrp2A, rListA);

        NvmeSqe readSqeA{};
        readSqeA.opcode = NVME_NVM_READ;
        readSqeA.commandId = 302;
        readSqeA.nsid = 1;
        readSqeA.cdw10 = 100;
        readSqeA.cdw12 = (2048 / 512) - 1;
        readSqeA.prp1 = rPrp1A;
        readSqeA.prp2 = rPrp2A;

        NvmeCqe rCqeA = ctrl.executeIoCommandWithPrp(readSqeA, dma);
        TEST_ASSERT(rCqeA.getStatusCode() == NVME_SC_SUCCESS, "PRP Read A must succeed");

        std::vector<uint8_t> readBackA(2048, 0);
        dma.readPhysicalMemory(rPrp1A, readBackA.data(), 2048);
        TEST_ASSERT(std::memcmp(hostBufA.data(), readBackA.data(), 2048) == 0, "Readback A data must match written data (0xAA)");

        // Case B: Two Pages (6144 bytes > 4096 and <= 8192) -> prp1 set, prp2 points to 2nd page
        std::vector<uint8_t> hostBufB(6144, 0xBB);
        uint64_t prp1B = 0, prp2B = 0;
        std::vector<uint64_t> listB;
        dma.buildPrpChain(hostBufB.data(), hostBufB.size(), prp1B, prp2B, listB);
        TEST_ASSERT(prp1B != 0 && prp2B != 0, "prp1 and prp2 must be non-zero for 2 pages");
        TEST_ASSERT(listB.empty(), "PRP list must be empty for exactly 2 pages");

        NvmeSqe writeSqeB{};
        writeSqeB.opcode = NVME_NVM_WRITE;
        writeSqeB.commandId = 303;
        writeSqeB.nsid = 1;
        writeSqeB.cdw10 = 200;
        writeSqeB.cdw12 = (6144 / 512) - 1;
        writeSqeB.prp1 = prp1B;
        writeSqeB.prp2 = prp2B;

        NvmeCqe cqeB = ctrl.executeIoCommandWithPrp(writeSqeB, dma);
        TEST_ASSERT(cqeB.getStatusCode() == NVME_SC_SUCCESS, "PRP Write B must succeed");

        // Case C: Multi-Page PRP List (16384 bytes = 4 pages > 8192) -> prp1 is page 0, prp2 points to PRP List
        std::vector<uint8_t> hostBufC(16384);
        for (size_t i = 0; i < hostBufC.size(); ++i) hostBufC[i] = static_cast<uint8_t>(i & 0xFF);

        uint64_t prp1C = 0, prp2C = 0;
        std::vector<uint64_t> listC;
        dma.buildPrpChain(hostBufC.data(), hostBufC.size(), prp1C, prp2C, listC);
        TEST_ASSERT(prp1C != 0 && prp2C != 0, "prp1 and prp2 must be valid");
        TEST_ASSERT(listC.size() == 3, "PRP list must contain 3 page entries for 4 pages total");

        NvmeSqe writeSqeC{};
        writeSqeC.opcode = NVME_NVM_WRITE;
        writeSqeC.commandId = 304;
        writeSqeC.nsid = 1;
        writeSqeC.cdw10 = 500;
        writeSqeC.cdw12 = (16384 / 512) - 1;
        writeSqeC.prp1 = prp1C;
        writeSqeC.prp2 = prp2C;

        NvmeCqe cqeC = ctrl.executeIoCommandWithPrp(writeSqeC, dma);
        TEST_ASSERT(cqeC.getStatusCode() == NVME_SC_SUCCESS, "PRP Write C (PRP List) must succeed");

        // Readback C
        uint64_t rPrp1C = 0, rPrp2C = 0;
        std::vector<uint64_t> rListC;
        dma.buildPrpChain(nullptr, 16384, rPrp1C, rPrp2C, rListC);

        NvmeSqe readSqeC{};
        readSqeC.opcode = NVME_NVM_READ;
        readSqeC.commandId = 305;
        readSqeC.nsid = 1;
        readSqeC.cdw10 = 500;
        readSqeC.cdw12 = (16384 / 512) - 1;
        readSqeC.prp1 = rPrp1C;
        readSqeC.prp2 = rPrp2C;

        NvmeCqe rCqeC = ctrl.executeIoCommandWithPrp(readSqeC, dma);
        TEST_ASSERT(rCqeC.getStatusCode() == NVME_SC_SUCCESS, "PRP Read C must succeed");

        std::vector<uint8_t> readBackC(16384, 0);
        auto resolvedPages = dma.resolvePrpPages(rPrp1C, rPrp2C, 16384);
        TEST_ASSERT(resolvedPages.size() == 4, "resolvePrpPages must return 4 pages");
        for (size_t p = 0; p < resolvedPages.size(); ++p) {
            dma.readPhysicalMemory(resolvedPages[p], readBackC.data() + (p * 4096), 4096);
        }
        TEST_ASSERT(std::memcmp(hostBufC.data(), readBackC.data(), 16384) == 0, "PRP List readback must match original data perfectly");
    }

    // Phase 5: High-throughput NVM I/O commands across namespaces
    {
        NvmeController ctrl(NvmeVersion::Version_2_0);
        ctrl.writeReg32(NVME_REG_CC, NVME_CC_EN | NVME_CC_IOSQES_64 | NVME_CC_IOCQES_16);

        // Write on Namespace 1 (512B)
        uint8_t writePayload[1024];
        std::memset(writePayload, 0x42, sizeof(writePayload));
        NvmeSqe sqeW1{};
        sqeW1.opcode = NVME_NVM_WRITE;
        sqeW1.commandId = 401;
        sqeW1.nsid = 1;
        sqeW1.cdw10 = 1000;
        sqeW1.cdw12 = 1; // 2 blocks = 1024 bytes
        NvmeCqe cqeW1 = ctrl.executeIoCommand(sqeW1, writePayload, sizeof(writePayload));
        TEST_ASSERT(cqeW1.getStatusCode() == NVME_SC_SUCCESS, "NVM Write NS1 must succeed");

        // Read on Namespace 1
        uint8_t readPayload[1024]{};
        NvmeSqe sqeR1{};
        sqeR1.opcode = NVME_NVM_READ;
        sqeR1.commandId = 402;
        sqeR1.nsid = 1;
        sqeR1.cdw10 = 1000;
        sqeR1.cdw12 = 1;
        NvmeCqe cqeR1 = ctrl.executeIoCommand(sqeR1, readPayload, sizeof(readPayload));
        TEST_ASSERT(cqeR1.getStatusCode() == NVME_SC_SUCCESS, "NVM Read NS1 must succeed");
        TEST_ASSERT(std::memcmp(writePayload, readPayload, 1024) == 0, "Payload read from NS1 must match written data (0x42)");

        // Write Zeroes on Namespace 1
        NvmeSqe sqeWZ{};
        sqeWZ.opcode = NVME_NVM_WRITE_ZEROES;
        sqeWZ.commandId = 403;
        sqeWZ.nsid = 1;
        sqeWZ.cdw10 = 1000;
        sqeWZ.cdw12 = 1;
        NvmeCqe cqeWZ = ctrl.executeIoCommand(sqeWZ, nullptr, 0);
        TEST_ASSERT(cqeWZ.getStatusCode() == NVME_SC_SUCCESS, "NVM Write Zeroes must succeed");

        // Verify zero readback
        std::memset(readPayload, 0xFF, sizeof(readPayload));
        ctrl.executeIoCommand(sqeR1, readPayload, sizeof(readPayload));
        bool allZero = true;
        for (size_t i = 0; i < sizeof(readPayload); ++i) {
            if (readPayload[i] != 0) { allZero = false; break; }
        }
        TEST_ASSERT(allZero, "Read after WriteZeroes must return all zeroes");

        // Flush command
        NvmeSqe sqeFlush{};
        sqeFlush.opcode = NVME_NVM_FLUSH;
        sqeFlush.commandId = 404;
        sqeFlush.nsid = 1;
        NvmeCqe cqeFlush = ctrl.executeIoCommand(sqeFlush, nullptr, 0);
        TEST_ASSERT(cqeFlush.getStatusCode() == NVME_SC_SUCCESS, "NVM Flush must succeed");
    }

    // Phase 6: Dataset Management (TRIM / Deallocate)
    {
        NvmeController ctrl(NvmeVersion::Version_2_0);
        ctrl.writeReg32(NVME_REG_CC, NVME_CC_EN | NVME_CC_IOSQES_64 | NVME_CC_IOCQES_16);

        // Pre-write data at LBA 2000
        uint8_t data[512];
        std::memset(data, 0xEE, sizeof(data));
        NvmeSqe sqeW{};
        sqeW.opcode = NVME_NVM_WRITE;
        sqeW.commandId = 501;
        sqeW.nsid = 1;
        sqeW.cdw10 = 2000;
        sqeW.cdw12 = 0;
        ctrl.executeIoCommand(sqeW, data, sizeof(data));

        // Submit Dataset Management TRIM for 1 range: LBA 2000, 1 block
        uint8_t dsmBuf[16]{};
        *reinterpret_cast<uint32_t*>(dsmBuf + 0) = 0; // context attributes
        *reinterpret_cast<uint32_t*>(dsmBuf + 4) = 1; // length in logical blocks
        *reinterpret_cast<uint64_t*>(dsmBuf + 8) = 2000; // starting LBA

        NvmeSqe sqeDsm{};
        sqeDsm.opcode = NVME_NVM_DATASET_MGMT;
        sqeDsm.commandId = 502;
        sqeDsm.nsid = 1;
        sqeDsm.cdw10 = 0; // NR = 0 (1 range)
        sqeDsm.cdw11 = 0x04; // Attribute - Deallocate
        NvmeCqe cqeDsm = ctrl.executeIoCommand(sqeDsm, dsmBuf, sizeof(dsmBuf));
        TEST_ASSERT(cqeDsm.getStatusCode() == NVME_SC_SUCCESS, "DATASET_MGMT Deallocate must succeed");

        // Verify trimmed block reads as zero (dlfeat behavior)
        uint8_t checkBuf[512]{};
        std::memset(checkBuf, 0xFF, sizeof(checkBuf));
        NvmeSqe sqeR{};
        sqeR.opcode = NVME_NVM_READ;
        sqeR.commandId = 503;
        sqeR.nsid = 1;
        sqeR.cdw10 = 2000;
        sqeR.cdw12 = 0;
        ctrl.executeIoCommand(sqeR, checkBuf, sizeof(checkBuf));
        bool isZero = true;
        for (int i = 0; i < 512; ++i) {
            if (checkBuf[i] != 0) { isZero = false; break; }
        }
        TEST_ASSERT(isZero, "Deallocated block must return zero on read");
    }

    // Phase 7: Command Abort Protocol (NVME_ADMIN_ABORT)
    {
        NvmeController ctrl(NvmeVersion::Version_2_0);
        ctrl.writeReg32(NVME_REG_CC, NVME_CC_EN | NVME_CC_IOSQES_64 | NVME_CC_IOCQES_16);
        ctrl.createIoQueuePair(1, 16, 16, 0x10000, 0x20000);

        // Submit Admin Abort command targeting SQID 1, CID 999
        NvmeSqe sqeAbort{};
        sqeAbort.opcode = NVME_ADMIN_ABORT;
        sqeAbort.commandId = 601;
        sqeAbort.cdw10 = (static_cast<uint32_t>(999) << 16) | 1; // CID 999, SQID 1

        NvmeCqe cqeAbort = ctrl.executeAdminCommand(sqeAbort, nullptr, 0);
        TEST_ASSERT(cqeAbort.getStatusCode() == NVME_SC_SUCCESS, "Admin Abort command must execute successfully");

        NVME_TELEMETRY telem = ctrl.getTelemetry();
        TEST_ASSERT(telem.abortedCommands == 1, "Aborted command telemetry count must be 1");
    }

    // Phase 8: StorPort Miniport Adapter Discovery & Configuration
    {
        auto ctrl = std::make_shared<NvmeController>(NvmeVersion::Version_2_0);
        auto dma = std::make_shared<PrpDmaEngine>();
        StorNvmeMiniport miniport(ctrl, dma);

        PORT_CONFIGURATION_INFORMATION config{};
        NTSTATUS stFind = miniport.findAdapter(&config);
        TEST_ASSERT(NT_SUCCESS(stFind), "findAdapter must succeed");
        TEST_ASSERT(config.MaximumTransferLength == 2 * 1024 * 1024, "MaximumTransferLength must be 2MB");
        TEST_ASSERT(config.NumberOfPhysicalBreaks == 512, "NumberOfPhysicalBreaks must be 512");
        TEST_ASSERT(config.Dma64BitAddresses == 1, "Dma64BitAddresses must be enabled (1)");
        TEST_ASSERT(config.NumberOfBuses == 1, "NumberOfBuses must be 1");
        TEST_ASSERT(config.MaximumNumberOfTargets == 4, "MaximumNumberOfTargets must be 4");
        TEST_ASSERT(config.MaximumNumberOfLogicalUnits == 8, "MaximumNumberOfLogicalUnits must be 8");

        NTSTATUS stInit = miniport.initialize();
        TEST_ASSERT(NT_SUCCESS(stInit), "Miniport initialize must succeed");
    }

    // Phase 9: SCSI-to-NVMe SRB Translation Pipeline
    {
        auto ctrl = std::make_shared<NvmeController>(NvmeVersion::Version_2_0);
        auto dma = std::make_shared<PrpDmaEngine>();
        StorNvmeMiniport miniport(ctrl, dma);
        miniport.initialize();

        // 1. SCSIOP_INQUIRY
        SCSI_REQUEST_BLOCK inqSrb{};
        uint8_t inqBuf[36]{};
        inqSrb.Function = SRB_FUNCTION_EXECUTE_SCSI;
        inqSrb.Cdb[0] = SCSIOP_INQUIRY;
        inqSrb.DataBuffer = inqBuf;
        inqSrb.DataTransferLength = sizeof(inqBuf);
        inqSrb.Lun = 0; // NSID 1
        miniport.startIo(&inqSrb);
        TEST_ASSERT(inqSrb.SrbStatus == SRB_STATUS_SUCCESS, "SRB INQUIRY status must be SUCCESS");
        TEST_ASSERT(inqSrb.ScsiStatus == SCSISTAT_GOOD, "SCSI status must be GOOD");
        TEST_ASSERT(std::memcmp(inqBuf + 8, "MicaNT  ", 8) == 0, "Inquiry vendor ID must match 'MicaNT  '");

        // 2. SCSIOP_READ_CAPACITY16
        SCSI_REQUEST_BLOCK capSrb{};
        uint8_t capBuf[32]{};
        capSrb.Function = SRB_FUNCTION_EXECUTE_SCSI;
        capSrb.Cdb[0] = SCSIOP_READ_CAPACITY16;
        capSrb.DataBuffer = capBuf;
        capSrb.DataTransferLength = sizeof(capBuf);
        capSrb.Lun = 0;
        miniport.startIo(&capSrb);
        TEST_ASSERT(capSrb.SrbStatus == SRB_STATUS_SUCCESS, "SRB READ_CAPACITY16 must succeed");
        uint32_t blkSize = (static_cast<uint32_t>(capBuf[8]) << 24) |
                           (static_cast<uint32_t>(capBuf[9]) << 16) |
                           (static_cast<uint32_t>(capBuf[10]) << 8) |
                           static_cast<uint32_t>(capBuf[11]);
        TEST_ASSERT(blkSize == 512, "Reported block size must be 512 for LUN 0 (NSID 1)");

        // 3. SCSIOP_WRITE10 & SCSIOP_READ10
        SCSI_REQUEST_BLOCK writeSrb{};
        uint8_t wData[1024];
        std::memset(wData, 0x77, sizeof(wData));
        writeSrb.Function = SRB_FUNCTION_EXECUTE_SCSI;
        writeSrb.Cdb[0] = SCSIOP_WRITE10;
        writeSrb.Cdb[2] = 0; writeSrb.Cdb[3] = 0; writeSrb.Cdb[4] = 0x01; writeSrb.Cdb[5] = 0x00; // LBA 256
        writeSrb.Cdb[7] = 0; writeSrb.Cdb[8] = 2; // 2 blocks
        writeSrb.DataBuffer = wData;
        writeSrb.DataTransferLength = sizeof(wData);
        writeSrb.Lun = 0;
        miniport.startIo(&writeSrb);
        TEST_ASSERT(writeSrb.SrbStatus == SRB_STATUS_SUCCESS, "SRB WRITE10 must succeed");

        SCSI_REQUEST_BLOCK readSrb{};
        uint8_t rData[1024]{};
        readSrb.Function = SRB_FUNCTION_EXECUTE_SCSI;
        readSrb.Cdb[0] = SCSIOP_READ10;
        readSrb.Cdb[2] = 0; readSrb.Cdb[3] = 0; readSrb.Cdb[4] = 0x01; readSrb.Cdb[5] = 0x00; // LBA 256
        readSrb.Cdb[7] = 0; readSrb.Cdb[8] = 2; // 2 blocks
        readSrb.DataBuffer = rData;
        readSrb.DataTransferLength = sizeof(rData);
        readSrb.Lun = 0;
        miniport.startIo(&readSrb);
        TEST_ASSERT(readSrb.SrbStatus == SRB_STATUS_SUCCESS, "SRB READ10 must succeed");
        TEST_ASSERT(std::memcmp(wData, rData, 1024) == 0, "SCSI Read10 data must match Write10 (0x77)");

        // 4. SCSIOP_SYNCHRONIZE_CACHE10
        SCSI_REQUEST_BLOCK syncSrb{};
        syncSrb.Function = SRB_FUNCTION_EXECUTE_SCSI;
        syncSrb.Cdb[0] = SCSIOP_SYNCHRONIZE_CACHE10;
        syncSrb.Lun = 0;
        miniport.startIo(&syncSrb);
        TEST_ASSERT(syncSrb.SrbStatus == SRB_STATUS_SUCCESS, "SYNCHRONIZE_CACHE must succeed");

        // 5. SCSIOP_UNMAP (TRIM)
        SCSI_REQUEST_BLOCK unmapSrb{};
        uint8_t unmapBuf[24]{};
        unmapBuf[0] = 0; unmapBuf[1] = 22; // unmap data length
        unmapBuf[2] = 0; unmapBuf[3] = 16; // block descriptor data length = 16
        // Block descriptor: LBA 256, count 2
        unmapBuf[14] = 0x01; unmapBuf[15] = 0x00; // LBA 256
        unmapBuf[19] = 2; // count 2
        unmapSrb.Function = SRB_FUNCTION_EXECUTE_SCSI;
        unmapSrb.Cdb[0] = SCSIOP_UNMAP;
        unmapSrb.DataBuffer = unmapBuf;
        unmapSrb.DataTransferLength = sizeof(unmapBuf);
        unmapSrb.Lun = 0;
        miniport.startIo(&unmapSrb);
        TEST_ASSERT(unmapSrb.SrbStatus == SRB_STATUS_SUCCESS, "SCSI UNMAP must succeed");

        // Verify block reads 0 after UNMAP
        std::memset(rData, 0xFF, sizeof(rData));
        miniport.startIo(&readSrb);
        bool unmappedZero = true;
        for (int i = 0; i < 1024; ++i) {
            if (rData[i] != 0) { unmappedZero = false; break; }
        }
        TEST_ASSERT(unmappedZero, "Data after SCSI UNMAP must be zeroed");

        // 6. SCSIOP_REPORT_LUNS
        SCSI_REQUEST_BLOCK lunsSrb{};
        uint8_t lunsBuf[24]{};
        lunsSrb.Function = SRB_FUNCTION_EXECUTE_SCSI;
        lunsSrb.Cdb[0] = SCSIOP_REPORT_LUNS;
        lunsSrb.DataBuffer = lunsBuf;
        lunsSrb.DataTransferLength = sizeof(lunsBuf);
        miniport.startIo(&lunsSrb);
        TEST_ASSERT(lunsSrb.SrbStatus == SRB_STATUS_SUCCESS, "REPORT_LUNS must succeed");
        uint32_t lunListLen = (static_cast<uint32_t>(lunsBuf[0]) << 24) |
                              (static_cast<uint32_t>(lunsBuf[1]) << 16) |
                              (static_cast<uint32_t>(lunsBuf[2]) << 8)  |
                              static_cast<uint32_t>(lunsBuf[3]);
        TEST_ASSERT(lunListLen >= 16, "REPORT_LUNS must report at least 2 LUNs (16 bytes)");
    }

    // Phase 10: Fatal Error Injection, StorPort resetBus, Controller Recovery & Telemetry Audit
    {
        auto ctrl = std::make_shared<NvmeController>(NvmeVersion::Version_2_0);
        auto dma = std::make_shared<PrpDmaEngine>();
        StorNvmeMiniport miniport(ctrl, dma);
        miniport.initialize();

        // 1. Inject fatal hardware status (CSTS.CFS = 1)
        ctrl->triggerFatalError();
        TEST_ASSERT((ctrl->readReg32(NVME_REG_CSTS) & NVME_CSTS_CFS) != 0, "CSTS.CFS must be set");

        // I/O commands must be rejected with internal device error
        NvmeSqe errSqe{};
        errSqe.opcode = NVME_NVM_READ;
        errSqe.commandId = 999;
        errSqe.nsid = 1;
        NvmeCqe errCqe = ctrl->executeIoCommand(errSqe, nullptr, 0);
        TEST_ASSERT(errCqe.getStatusCode() == NVME_SC_INTERNAL_DEVICE_ERROR, "Commands during CFS must return NVME_SC_INTERNAL_DEVICE_ERROR");

        // 2. Perform StorPort bus reset recovery (SRB_FUNCTION_RESET_BUS)
        SCSI_REQUEST_BLOCK resetSrb{};
        resetSrb.Function = SRB_FUNCTION_RESET_BUS;
        resetSrb.PathId = 0;
        miniport.startIo(&resetSrb);
        TEST_ASSERT(resetSrb.SrbStatus == SRB_STATUS_SUCCESS, "SRB RESET_BUS must succeed");

        // Controller must be recovered and ready again, CFS cleared
        TEST_ASSERT((ctrl->readReg32(NVME_REG_CSTS) & NVME_CSTS_CFS) == 0, "CSTS.CFS must be cleared after bus reset");
        TEST_ASSERT(ctrl->isReady(), "Controller must be ready after recovery");

        // 3. Telemetry Audit
        auto telem = miniport.getTelemetry();
        TEST_ASSERT(telem.srbDispatched > 0, "srbDispatched must be > 0");
        TEST_ASSERT(telem.srbCompleted > 0, "srbCompleted must be > 0");
        TEST_ASSERT(telem.busResets == 1, "busResets must be 1");

        auto ctrlTelem = ctrl->getTelemetry();
        TEST_ASSERT(ctrlTelem.fatalErrorsInjected == 1, "fatalErrorsInjected must be 1");
        TEST_ASSERT(ctrlTelem.recoveriesCompleted == 1, "recoveriesCompleted must be 1");

        // 4. Test C ABI Exports via TitanFlashSubsystem
        InitializeNvmeSubsystem();
        SCSI_REQUEST_BLOCK abiSrb{};
        uint8_t abiInqBuf[36]{};
        abiSrb.Function = SRB_FUNCTION_EXECUTE_SCSI;
        abiSrb.Cdb[0] = SCSIOP_INQUIRY;
        abiSrb.DataBuffer = abiInqBuf;
        abiSrb.DataTransferLength = sizeof(abiInqBuf);
        abiSrb.Lun = 0;
        BOOL bSubmitted = StorNvmeSubmitSrb(&abiSrb);
        TEST_ASSERT(bSubmitted != 0, "StorNvmeSubmitSrb C ABI must succeed");
        TEST_ASSERT(abiSrb.SrbStatus == SRB_STATUS_SUCCESS, "StorNvmeSubmitSrb status must be SUCCESS");

        STOR_ADDR_BTL8 addr{};
        ULONG stAddr = StorPortGetLogicalUnitAddress(nullptr, &abiSrb, &addr);
        TEST_ASSERT(stAddr == 0, "StorPortGetLogicalUnitAddress must succeed");
        TEST_ASSERT(addr.Lun == 0, "Address Lun must match SRB Lun");

        auto portTelem = StorPortQueryTelemetry();
        TEST_ASSERT(portTelem.srbDispatched > 0, "StorPortQueryTelemetry must report dispatched SRBs");

        StorPortResetTelemetry();
        auto resetPortTelem = StorPortQueryTelemetry();
        TEST_ASSERT(resetPortTelem.srbDispatched == 0, "StorPortResetTelemetry must clear counters");

        TitanFlashSubsystem::Instance().resetForTesting();
    }

    std::cout << "[TEST] Suite 248: Horizon 32 - StorPort / NVMe Storage Architecture, Scatter-Gather PRPs, Queues & Miniport PASSED.\n";
}

void Test_Horizon33_WDDM32_Display_Driver_Model_And_Graphics_Subsystem() {
    std::cout << "\n========================================================================\n";
    std::cout << "  Suite 249: Horizon 33 - WDDM 3.2 Display Driver Model & DirectX Graphics\n";
    std::cout << "========================================================================\n";

    // Initialize WDDM Subsystem defensively & reset test telemetry
    wddm::InitializeWddmSubsystem();
    auto& wddmSub = wddm::TitanWddmSubsystem::Instance();
    TEST_ASSERT(wddmSub.isInitialized(), "TitanWddmSubsystem must be initialized");
    wddmSub.resetForTesting();

    // ------------------------------------------------------------------------
    // Phase 1: Multi-Vendor Graphics Adapter Discovery & PCIe Resource Configuration
    // ------------------------------------------------------------------------
    {
        const auto& adapters = wddmSub.getAllAdapters();
        TEST_ASSERT(adapters.size() >= 3, "Subsystem must discover at least 3 multi-vendor graphics adapters");
        TEST_ASSERT(adapters.count(0x1000) > 0, "Adapter 0x1000 must exist");
        TEST_ASSERT(adapters.count(0x1001) > 0, "Adapter 0x1001 must exist");
        TEST_ASSERT(adapters.count(0x1002) > 0, "Adapter 0x1002 must exist");

        // Primary Adapter: NVIDIA / PrismX RTX 4090 Class
        auto primaryGpu = wddmSub.getPrimaryAdapter();
        TEST_ASSERT(primaryGpu != nullptr, "Primary WDDM graphics adapter must exist");
        TEST_ASSERT(primaryGpu->getPciVendorId() == 0x10DE, "Primary GPU vendor ID must be NVIDIA (0x10DE)");
        TEST_ASSERT(primaryGpu->getPciDeviceId() == 0x2684, "Primary GPU device ID must be RTX 4090 class (0x2684)");
        TEST_ASSERT(primaryGpu->getWddmVersion() == wddm::WddmVersion::WDDM_3_2, "WDDM version must report 3.2");
        TEST_ASSERT(primaryGpu->getTotalVram() == 16ULL * 1024 * 1024 * 1024, "Primary GPU must report 16 GB dedicated VRAM");
        TEST_ASSERT(primaryGpu->getTotalSystemMemory() == 16ULL * 1024 * 1024 * 1024, "Shared system memory must report 16 GB");
        TEST_ASSERT(primaryGpu->getMiniport() != nullptr, "Primary GPU miniport must be bound");
        TEST_ASSERT(primaryGpu->getMiniport()->getVendorId() == wddm::GpuVendorId::Nvidia, "Primary miniport vendor must be NVIDIA");

        // Secondary Adapter: AMD Radeon RX 7900 XTX
        auto amdGpu = adapters.at(0x1001);
        TEST_ASSERT(amdGpu->getPciVendorId() == 0x1002, "AMD GPU vendor ID must be 0x1002");
        TEST_ASSERT(amdGpu->getPciDeviceId() == 0x744C, "AMD GPU device ID must be 0x744C");
        TEST_ASSERT(amdGpu->getMiniport() != nullptr, "AMD miniport driver must be bound");
        TEST_ASSERT(amdGpu->getMiniport()->getBinaryPath().find("amdkmdag.sys") != std::string::npos, "AMD miniport binary must be amdkmdag.sys");

        // Tertiary Adapter: Intel Arc A770
        auto intelGpu = adapters.at(0x1002);
        TEST_ASSERT(intelGpu->getPciVendorId() == 0x8086, "Intel GPU vendor ID must be 0x8086");
        TEST_ASSERT(intelGpu->getPciDeviceId() == 0x56A0, "Intel GPU device ID must be 0x56A0");
        TEST_ASSERT(intelGpu->getMiniport() != nullptr, "Intel miniport driver must be bound");
        TEST_ASSERT(intelGpu->getMiniport()->getBinaryPath().find("igdkmdn64.sys") != std::string::npos, "Intel miniport binary must be igdkmdn64.sys");
    }

    // ------------------------------------------------------------------------
    // Phase 2: VidMm Segments & 48-bit GPU Virtual Addressing (GPUVA)
    // ------------------------------------------------------------------------
    auto gpu = wddmSub.getPrimaryAdapter();
    uint32_t hAllocSdr = 0;
    uint32_t hAllocHdr = 0;
    uint32_t hAllocVideo = 0;
    {
        const auto& segs = gpu->getSegments();
        TEST_ASSERT(segs.size() >= 2, "VidMm must report Aperture and Dedicated segments");
        TEST_ASSERT(segs[0].type == wddm::MemorySegmentType::ApertureSystem, "Segment 1 must be ApertureSystem");
        TEST_ASSERT(segs[1].type == wddm::MemorySegmentType::LocalDedicated, "Segment 2 must be LocalDedicated");
        TEST_ASSERT(segs[1].usedBytes == 0, "Initial usedBytes must be 0");

        // Create SDR Desktop Buffer (B8G8R8A8_UNORM, 4K, 3840x2160x4 = ~33MB)
        uint64_t sdrSize = 3840ULL * 2160 * 4;
        hAllocSdr = gpu->createAllocation(sdrSize, 2, wddm::PixelFormat::B8G8R8A8_UNORM, 3840, 2160, false);
        TEST_ASSERT(hAllocSdr != 0, "createAllocation SDR must succeed");

        auto allocSdr = gpu->getAllocation(hAllocSdr);
        TEST_ASSERT(allocSdr != nullptr, "SDR Allocation record must exist");
        TEST_ASSERT(allocSdr->gpuVirtualAddress >= 0x00007FF000000000ULL, "48-bit GPUVA must be valid");
        TEST_ASSERT(allocSdr->format == wddm::PixelFormat::B8G8R8A8_UNORM, "Format must match SDR UNORM");
        TEST_ASSERT(allocSdr->isResident, "Allocation must default to resident");

        // Create HDR10 Buffer (R10G10B10A2_UNORM, 4K, 3840x2160x4)
        uint64_t hdrSize = 3840ULL * 2160 * 4;
        hAllocHdr = gpu->createAllocation(hdrSize, 2, wddm::PixelFormat::R10G10B10A2_UNORM, 3840, 2160, true);
        TEST_ASSERT(hAllocHdr != 0, "createAllocation HDR10 must succeed");
        auto allocHdr = gpu->getAllocation(hAllocHdr);
        TEST_ASSERT(allocHdr != nullptr && allocHdr->isShared, "HDR allocation must be shared");

        // Create NV12 Video Surface (1920x1080x1.5 = ~3.1MB)
        uint64_t vidSize = (1920ULL * 1080 * 3) / 2;
        hAllocVideo = gpu->createAllocation(vidSize, 2, wddm::PixelFormat::NV12, 1920, 1080, false);
        TEST_ASSERT(hAllocVideo != 0, "createAllocation NV12 must succeed");

        // Verify segment usedBytes
        TEST_ASSERT(gpu->getSegments()[1].usedBytes == (sdrSize + hdrSize + vidSize), "Local segment usedBytes must track allocations");

        // Verify custom GPUVA mapping & unmapping
        uint64_t customVa = 0x00007FFAA0000000ULL;
        TEST_ASSERT(gpu->mapGpuVirtualAddress(hAllocSdr, customVa), "mapGpuVirtualAddress must succeed");
        TEST_ASSERT(gpu->getAllocation(hAllocSdr)->gpuVirtualAddress == customVa, "Mapped GPUVA must match custom address");
        TEST_ASSERT(gpu->unmapGpuVirtualAddress(hAllocSdr), "unmapGpuVirtualAddress must succeed");
        TEST_ASSERT(gpu->getAllocation(hAllocSdr)->gpuVirtualAddress == 0, "Unmapped GPUVA must be 0");
        TEST_ASSERT(gpu->mapGpuVirtualAddress(hAllocSdr, customVa), "Remap GPUVA must succeed");
    }

    // ------------------------------------------------------------------------
    // Phase 3: VidMm Residency & Paging Channel DMA Migration
    // ------------------------------------------------------------------------
    {
        // Eviction & Paging Out
        TEST_ASSERT(gpu->pageOut(hAllocHdr), "pageOut must succeed");
        TEST_ASSERT(!gpu->getAllocation(hAllocHdr)->isResident, "Allocation must be evicted/non-resident");

        // Multi-allocation batch eviction
        TEST_ASSERT(gpu->evict({ hAllocSdr, hAllocVideo }), "evict multi-allocation must succeed");
        TEST_ASSERT(!gpu->getAllocation(hAllocSdr)->isResident, "SDR alloc must be non-resident");
        TEST_ASSERT(!gpu->getAllocation(hAllocVideo)->isResident, "Video alloc must be non-resident");

        // Paging In & Make Resident
        TEST_ASSERT(gpu->pageIn(hAllocHdr), "pageIn must succeed");
        TEST_ASSERT(gpu->getAllocation(hAllocHdr)->isResident, "Allocation must be resident after pageIn");

        TEST_ASSERT(gpu->makeResident({ hAllocSdr, hAllocVideo }), "makeResident must succeed");
        TEST_ASSERT(gpu->getAllocation(hAllocSdr)->isResident, "SDR alloc must be resident");
        TEST_ASSERT(gpu->getAllocation(hAllocVideo)->isResident, "Video alloc must be resident");
    }

    // ------------------------------------------------------------------------
    // Phase 4: Device & Context Lifecycle Management
    // ------------------------------------------------------------------------
    uint32_t hDev1 = 0;
    uint32_t hDev2 = 0;
    uint32_t hCtx3D = 0;
    uint32_t hCtxCompute = 0;
    {
        hDev1 = gpu->createDevice(0);
        TEST_ASSERT(hDev1 != 0, "createDevice 1 must return valid handle");
        TEST_ASSERT(gpu->getDeviceCount() == 1, "Device count must be 1");
        TEST_ASSERT(!gpu->isDeviceRemoved(hDev1), "Device must not be removed");

        hDev2 = gpu->createDevice(0);
        TEST_ASSERT(hDev2 != 0, "createDevice 2 must return valid handle");
        TEST_ASSERT(gpu->getDeviceCount() == 2, "Device count must be 2");

        // Create Contexts on Dev1
        hCtx3D = gpu->createContext(hDev1, wddm::GpuEngineType::ThreeD, wddm::HwQueuePriority::Normal);
        TEST_ASSERT(hCtx3D != 0, "createContext 3D must succeed");

        hCtxCompute = gpu->createContext(hDev1, wddm::GpuEngineType::Compute, wddm::HwQueuePriority::High);
        TEST_ASSERT(hCtxCompute != 0, "createContext Compute must succeed");
        TEST_ASSERT(gpu->getContextCount() == 2, "Context count must be 2");

        const auto dev1Rec = gpu->getDevice(hDev1);
        TEST_ASSERT(dev1Rec != nullptr, "Device record must exist");
        TEST_ASSERT(dev1Rec->contextHandles.size() == 2, "Device 1 must track 2 contexts");

        const auto ctxRec = gpu->getContext(hCtxCompute);
        TEST_ASSERT(ctxRec != nullptr, "Context record must exist");
        TEST_ASSERT(ctxRec->engineType == wddm::GpuEngineType::Compute, "Engine type must match Compute");
        TEST_ASSERT(ctxRec->priority == wddm::HwQueuePriority::High, "Priority must match High");
        TEST_ASSERT(!ctxRec->isLost, "Context must not be lost");

        // Context destruction
        uint32_t tempCtx = gpu->createContext(hDev2, wddm::GpuEngineType::CopyBlt, wddm::HwQueuePriority::Idle);
        TEST_ASSERT(tempCtx != 0, "tempCtx must be created");
        TEST_ASSERT(gpu->destroyContext(tempCtx), "destroyContext must succeed");
        TEST_ASSERT(gpu->getContext(tempCtx) == nullptr, "Destroyed context must not exist");
    }

    // ------------------------------------------------------------------------
    // Phase 5: Hardware-Accelerated GPU Scheduling (HWS) & Multi-Engine Queues
    // ------------------------------------------------------------------------
    uint32_t q3d = 0;
    uint32_t qCompute = 0;
    uint32_t qCopy = 0;
    uint32_t qDecode = 0;
    {
        q3d = gpu->createHardwareQueue(hCtx3D, wddm::GpuEngineType::ThreeD, wddm::HwQueuePriority::Normal);
        TEST_ASSERT(q3d != 0, "createHardwareQueue 3D must succeed");

        qCompute = gpu->createHardwareQueue(hCtxCompute, wddm::GpuEngineType::Compute, wddm::HwQueuePriority::High);
        TEST_ASSERT(qCompute != 0, "createHardwareQueue Compute must succeed");

        qCopy = gpu->createHardwareQueue(hCtx3D, wddm::GpuEngineType::CopyBlt, wddm::HwQueuePriority::Normal);
        TEST_ASSERT(qCopy != 0, "createHardwareQueue Copy must succeed");

        qDecode = gpu->createHardwareQueue(hCtx3D, wddm::GpuEngineType::VideoDecode, wddm::HwQueuePriority::Low);
        TEST_ASSERT(qDecode != 0, "createHardwareQueue VideoDecode must succeed");
    }

    // ------------------------------------------------------------------------
    // Phase 6: 64-bit Monitored Fences & Cross-Engine Timeline Synchronization
    // ------------------------------------------------------------------------
    uint32_t fenceRender = 0;
    uint32_t fenceCompute = 0;
    {
        fenceRender = gpu->createMonitoredFence(0);
        fenceCompute = gpu->createMonitoredFence(0);
        TEST_ASSERT(fenceRender != 0 && fenceCompute != 0, "createMonitoredFence must return valid handles");
        TEST_ASSERT(gpu->getFenceValue(fenceRender) == 0, "Initial fence value must be 0");

        // Submit 3D Render command signaling fenceRender to 100
        uint64_t dmaVa = gpu->getAllocation(hAllocSdr)->gpuVirtualAddress;
        bool bSub3D = gpu->submitCommandToHwQueue(q3d, dmaVa, 4096, fenceRender, 100);
        TEST_ASSERT(bSub3D, "3D command submission must succeed");
        TEST_ASSERT(gpu->getFenceValue(fenceRender) == 100, "Fence must advance to 100 on GPU completion");

        // Compute Queue waits on 3D fence value 100 before dispatching async compute
        TEST_ASSERT(gpu->waitQueueOnFence(qCompute, fenceRender, 100), "waitQueueOnFence must succeed");

        // Submit Compute command signaling fenceCompute to 200
        bool bSubComp = gpu->submitCommandToHwQueue(qCompute, dmaVa, 2048, fenceCompute, 200);
        TEST_ASSERT(bSubComp, "Compute command submission must succeed");
        TEST_ASSERT(gpu->getFenceValue(fenceCompute) == 200, "Compute fence must advance to 200");

        // CPU synchronization: waitForFenceCpu
        TEST_ASSERT(gpu->waitForFenceCpu(fenceRender, 100), "waitForFenceCpu must return true when reached");
        TEST_ASSERT(gpu->waitForFenceCpu(fenceCompute, 200), "waitForFenceCpu must return true for compute fence");

        // CPU-side fence signaling
        TEST_ASSERT(gpu->signalFenceCpu(fenceRender, 300), "signalFenceCpu must succeed");
        TEST_ASSERT(gpu->getFenceValue(fenceRender) == 300, "Fence value must reflect CPU signal");
        TEST_ASSERT(gpu->waitForFenceCpu(fenceRender, 300), "waitForFenceCpu must return true for 300");
    }

    // ------------------------------------------------------------------------
    // Phase 7: VidPN Topology, Display Modes & HDR/VRR
    // ------------------------------------------------------------------------
    {
        const auto& sources = gpu->getSources();
        const auto& targets = gpu->getTargets();
        const auto& paths = gpu->getPaths();

        TEST_ASSERT(sources.size() >= 2, "VidPN must contain Desktop Primary and Extended sources");
        TEST_ASSERT(targets.size() >= 2, "VidPN must contain DisplayPort and HDMI targets");
        TEST_ASSERT(paths.size() >= 1, "VidPN must contain active path");

        const auto& dpTarget = targets[0];
        TEST_ASSERT(dpTarget.connectorType == wddm::VideoConnectorType::DisplayPort, "Target 0 must be DisplayPort");
        TEST_ASSERT(dpTarget.supportsHdr, "DisplayPort target must support HDR");
        TEST_ASSERT(dpTarget.supportsVrr, "DisplayPort target must support VRR");
        TEST_ASSERT(dpTarget.vrrMinHz == 48 && dpTarget.vrrMaxHz == 240, "VRR range must be 48-240 Hz");

        // Test mode change to 8K 60Hz scRGB HDR
        wddm::DisplayMode mode8k{ 7680, 4320, 60, 1, wddm::PixelFormat::R16G16B16A16_FLOAT, false };
        TEST_ASSERT(gpu->setTargetMode(dpTarget.targetId, mode8k), "setTargetMode 8K scRGB must succeed");
        TEST_ASSERT(gpu->getTargets()[0].currentMode.width == 7680, "Active width must be 7680");
        TEST_ASSERT(gpu->getTargets()[0].currentMode.format == wddm::PixelFormat::R16G16B16A16_FLOAT, "Format must be scRGB");

        // Restore to 4K 120Hz HDR10
        wddm::DisplayMode mode4k{ 3840, 2160, 120, 1, wddm::PixelFormat::R10G10B10A2_UNORM, false };
        TEST_ASSERT(gpu->setTargetMode(dpTarget.targetId, mode4k), "setTargetMode 4K HDR10 must succeed");
    }

    // ------------------------------------------------------------------------
    // Phase 8: Multi-Plane Overlay (MPO 3.0) & Presentation Pipeline
    // ------------------------------------------------------------------------
    {
        const auto& mpoPlanes = gpu->getMpoPlanes();
        TEST_ASSERT(mpoPlanes.size() == 4, "MPO 3.0 must provide 4 planes");
        TEST_ASSERT(mpoPlanes[0].enabled, "Plane 0 (Desktop) must be enabled");

        // DirectFlip presentation on Plane 0
        TEST_ASSERT(gpu->presentDirectFlip(0, hAllocHdr), "presentDirectFlip must succeed");
        TEST_ASSERT(gpu->getSources()[0].primaryAllocationHandle == hAllocHdr, "Source 0 allocation must be updated");
        TEST_ASSERT(gpu->getPaths()[0].directFlipActive, "Path directFlipActive must be true");

        // Multi-Plane Overlay presentation: Plane 1 (Video) with NV12 hardware scaling
        TEST_ASSERT(gpu->presentMpo(1, hAllocVideo, 1920, 1080, 100, 100, 1920, 1080, wddm::PixelFormat::NV12), "presentMpo Video must succeed");
        TEST_ASSERT(gpu->getMpoPlanes()[1].enabled, "Plane 1 must now be enabled");
        TEST_ASSERT(gpu->getMpoPlanes()[1].allocationHandle == hAllocVideo, "Plane 1 allocation must be hAllocVideo");
        TEST_ASSERT(gpu->getMpoPlanes()[1].format == wddm::PixelFormat::NV12, "Plane 1 format must be NV12");

        // VBlank interrupt simulation
        uint32_t scanLineBefore = gpu->getScanLine();
        gpu->simulateVBlankInterrupt();
        TEST_ASSERT(gpu->getScanLine() != scanLineBefore, "Scan line must advance on VBlank");
        TEST_ASSERT(gpu->getTotalVBlanks() >= 1, "VBlank count must increment");
    }

    // ------------------------------------------------------------------------
    // Phase 9: Timeout Detection and Recovery (TDR) Watchdog & Engine Reset
    // ------------------------------------------------------------------------
    {
        TEST_ASSERT(gpu->getTdrState() == wddm::TdrState::Normal, "Initial TDR state must be Normal");

        // Trigger TDR on 3D engine with faulting device hDev2
        bool tdrOk = gpu->triggerTdrSimulation(wddm::GpuEngineType::ThreeD, hDev2);
        TEST_ASSERT(tdrOk, "triggerTdrSimulation must succeed");
        TEST_ASSERT(gpu->getTdrState() == wddm::TdrState::Recovered, "TDR state must report Recovered without kernel crash");
        TEST_ASSERT(gpu->getTdrRecoveryCount() == 1, "Recovery count must be 1");

        // Faulting device must be marked removed
        TEST_ASSERT(gpu->isDeviceRemoved(hDev2), "Faulting device must be marked removed");

        // Non-faulting device 1 must still be active and healthy
        TEST_ASSERT(!gpu->isDeviceRemoved(hDev1), "Healthy device must NOT be marked removed");
        TEST_ASSERT(!gpu->getContext(hCtx3D)->isLost, "Healthy device context must NOT be lost");
    }

    // ------------------------------------------------------------------------
    // Phase 10: D3DKMT Syscall Thunks, Dynamic Loader Exports & WDDM Diagnostic Telemetry
    // ------------------------------------------------------------------------
    {
        // Test C ABI Dxgk* functions
        uint32_t abiDev = 0;
        int32_t stDev = wddm::DxgkCreateDevice(0x1000, &abiDev);
        TEST_ASSERT(stDev == 1 && abiDev != 0, "DxgkCreateDevice C ABI must succeed");

        uint32_t abiCtx = 0;
        int32_t stCtx = wddm::DxgkCreateContext(0x1000, abiDev, static_cast<uint32_t>(wddm::GpuEngineType::ThreeD), static_cast<uint32_t>(wddm::HwQueuePriority::Normal), &abiCtx);
        TEST_ASSERT(stCtx == 1 && abiCtx != 0, "DxgkCreateContext C ABI must succeed");

        uint32_t abiAlloc = 0;
        uint64_t abiVa = 0;
        int32_t stAlloc = wddm::DxgkCreateAllocation(0x1000, 1024 * 1024, 2, &abiAlloc, &abiVa);
        TEST_ASSERT(stAlloc == 1 && abiAlloc != 0 && abiVa != 0, "DxgkCreateAllocation C ABI must succeed");

        uint32_t abiQueue = 0;
        int32_t stQ = wddm::DxgkCreateHwQueue(0x1000, abiCtx, static_cast<uint32_t>(wddm::GpuEngineType::ThreeD), static_cast<uint32_t>(wddm::HwQueuePriority::Normal), &abiQueue);
        TEST_ASSERT(stQ == 1 && abiQueue != 0, "DxgkCreateHwQueue C ABI must succeed");

        uint32_t abiFence = 0;
        int32_t stFence = wddm::DxgkCreateMonitoredFence(0x1000, 0, &abiFence);
        TEST_ASSERT(stFence == 1 && abiFence != 0, "DxgkCreateMonitoredFence C ABI must succeed");

        int32_t stSub = wddm::DxgkSubmitCommandHwQueue(0x1000, abiQueue, abiVa, 512, abiFence, 50);
        TEST_ASSERT(stSub == 1, "DxgkSubmitCommandHwQueue C ABI must succeed");

        int32_t stWait = wddm::DxgkWaitForMonitoredFence(0x1000, abiFence, 50, 1000);
        TEST_ASSERT(stWait == 1, "DxgkWaitForMonitoredFence C ABI must succeed");

        int32_t stPres = wddm::DxgkPresentFrame(0x1000, 0, abiAlloc);
        TEST_ASSERT(stPres == 1, "DxgkPresentFrame C ABI must succeed");

        int32_t stTdr = wddm::DxgkTriggerTdr(0x1000);
        TEST_ASSERT(stTdr == 1, "DxgkTriggerTdr C ABI must succeed");

        // Clean up ABI handles
        TEST_ASSERT(wddm::DxgkDestroyAllocation(0x1000, abiAlloc) == 1, "DxgkDestroyAllocation C ABI must succeed");
        TEST_ASSERT(wddm::DxgkDestroyContext(0x1000, abiCtx) == 1, "DxgkDestroyContext C ABI must succeed");
        TEST_ASSERT(wddm::DxgkDestroyDevice(0x1000, abiDev) == 1, "DxgkDestroyDevice C ABI must succeed");

        // Test D3DKMT Syscall Thunks via dxgkrnl
        dxgkrnl::D3DKMT_OPENADAPTERFROMHDC openHdc{};
        TEST_ASSERT(dxgkrnl::NtGdiDdD3DKMTOpenAdapterFromHdc(&openHdc) == NtStatus::Success, "NtGdiDdD3DKMTOpenAdapterFromHdc must succeed");
        TEST_ASSERT(openHdc.hAdapter != 0, "hAdapter must be non-zero");

        dxgkrnl::D3DKMT_CREATEDEVICE kmtDev{};
        kmtDev.hAdapter = openHdc.hAdapter;
        TEST_ASSERT(dxgkrnl::NtGdiDdD3DKMTCreateDevice(&kmtDev) == NtStatus::Success, "NtGdiDdD3DKMTCreateDevice must succeed");

        dxgkrnl::D3DKMT_CREATEALLOCATIONINFO allocInfo{};
        dxgkrnl::D3DKMT_CREATEALLOCATION kmtAlloc{};
        kmtAlloc.hDevice = kmtDev.hDevice;
        kmtAlloc.NumAllocations = 1;
        kmtAlloc.pAllocationInfo = &allocInfo;
        TEST_ASSERT(dxgkrnl::NtGdiDdD3DKMTCreateAllocation(&kmtAlloc) == NtStatus::Success, "NtGdiDdD3DKMTCreateAllocation must succeed");

        dxgkrnl::D3DKMT_SUBMITCOMMAND kmtCmd{};
        kmtCmd.CommandLength = 1024;
        TEST_ASSERT(dxgkrnl::NtGdiDdD3DKMTSubmitCommand(&kmtCmd) == NtStatus::Success, "NtGdiDdD3DKMTSubmitCommand must succeed");

        dxgkrnl::D3DKMT_PRESENT kmtPresent{};
        kmtPresent.hDevice = kmtDev.hDevice;
        TEST_ASSERT(dxgkrnl::NtGdiDdD3DKMTPresent(&kmtPresent) == NtStatus::Success, "NtGdiDdD3DKMTPresent must succeed");

        // Verify DynamicLoader exports
        auto& ldr = ldr::DynamicLoader::get();
        TEST_ASSERT(ldr.getExport("dxgkrnl.sys", "DxgkInitialize") != nullptr, "DxgkInitialize export must exist");
        TEST_ASSERT(ldr.getExport("dxgkrnl.sys", "DxgkCreateDevice") != nullptr, "DxgkCreateDevice export must exist");
        TEST_ASSERT(ldr.getExport("dxgkrnl.sys", "DxgkDestroyDevice") != nullptr, "DxgkDestroyDevice export must exist");
        TEST_ASSERT(ldr.getExport("dxgkrnl.sys", "DxgkCreateContext") != nullptr, "DxgkCreateContext export must exist");
        TEST_ASSERT(ldr.getExport("dxgkrnl.sys", "DxgkDestroyContext") != nullptr, "DxgkDestroyContext export must exist");
        TEST_ASSERT(ldr.getExport("dxgkrnl.sys", "DxgkCreateAllocation") != nullptr, "DxgkCreateAllocation export must exist");
        TEST_ASSERT(ldr.getExport("dxgkrnl.sys", "DxgkDestroyAllocation") != nullptr, "DxgkDestroyAllocation export must exist");
        TEST_ASSERT(ldr.getExport("dxgkrnl.sys", "DxgkCreateHwQueue") != nullptr, "DxgkCreateHwQueue export must exist");
        TEST_ASSERT(ldr.getExport("dxgkrnl.sys", "DxgkSubmitCommandHwQueue") != nullptr, "DxgkSubmitCommandHwQueue export must exist");
        TEST_ASSERT(ldr.getExport("dxgkrnl.sys", "DxgkCreateMonitoredFence") != nullptr, "DxgkCreateMonitoredFence export must exist");
        TEST_ASSERT(ldr.getExport("dxgkrnl.sys", "DxgkSignalMonitoredFence") != nullptr, "DxgkSignalMonitoredFence export must exist");
        TEST_ASSERT(ldr.getExport("dxgkrnl.sys", "DxgkWaitForMonitoredFence") != nullptr, "DxgkWaitForMonitoredFence export must exist");
        TEST_ASSERT(ldr.getExport("dxgkrnl.sys", "DxgkPresentFrame") != nullptr, "DxgkPresentFrame export must exist");
        TEST_ASSERT(ldr.getExport("dxgkrnl.sys", "DxgkTriggerTdr") != nullptr, "DxgkTriggerTdr export must exist");
        TEST_ASSERT(ldr.getExport("displib.sys", "DxgkInitialize") != nullptr, "displib.sys export must exist");

        // Verify WDDM Diagnostic Telemetry
        auto telem = wddm::WddmQueryTelemetry();
        TEST_ASSERT(telem.totalAdaptersEnumerated >= 3, "totalAdaptersEnumerated must be >= 3");
        TEST_ASSERT(telem.totalDevicesCreated >= 3, "totalDevicesCreated must be >= 3");
        TEST_ASSERT(telem.totalContextsCreated >= 3, "totalContextsCreated must be >= 3");
        TEST_ASSERT(telem.totalHardwareQueuesCreated >= 4, "totalHardwareQueuesCreated must be >= 4");
        TEST_ASSERT(telem.totalAllocationsCreated >= 4, "totalAllocationsCreated must be >= 4");
        TEST_ASSERT(telem.totalPagingOperations >= 2, "totalPagingOperations must be >= 2");
        TEST_ASSERT(telem.totalCommandsSubmitted >= 3, "totalCommandsSubmitted must be >= 3");
        TEST_ASSERT(telem.totalFencesSignaled >= 3, "totalFencesSignaled must be >= 3");
        TEST_ASSERT(telem.totalPresents >= 2, "totalPresents must be >= 2");
        TEST_ASSERT(telem.totalDirectFlips >= 1, "totalDirectFlips must be >= 1");
        TEST_ASSERT(telem.totalMpoPresents >= 1, "totalMpoPresents must be >= 1");
        TEST_ASSERT(telem.totalVBlanks >= 1, "totalVBlanks must be >= 1");
        TEST_ASSERT(telem.totalTdrTriggers >= 2, "totalTdrTriggers must be >= 2");
        TEST_ASSERT(telem.totalTdrRecoveries >= 2, "totalTdrRecoveries must be >= 2");

        wddm::WddmResetTelemetryForTesting();
        auto telemReset = wddm::WddmQueryTelemetry();
        TEST_ASSERT(telemReset.totalDevicesCreated == 0, "totalDevicesCreated must be 0 after reset");
    }

    std::cout << "[TEST] Suite 249: Horizon 33 - WDDM 3.2 Display Driver Model & DirectX Graphics Subsystem PASSED.\n";
}

// ============================================================================
// SUITE 250: Horizon 34 - NDIS 6.x & NetAdapterCx Networking Architecture
// ============================================================================
inline void Test_Horizon34_NDIS6_NetAdapterCx_Networking_Architecture() {
    std::cout << "[TEST] Starting Suite 250: Horizon 34 - NDIS 6.x & NetAdapterCx Networking Architecture...\n";

    // ------------------------------------------------------------------------
    // Phase 1: Multi-NIC Hardware Discovery & PCIe Resource Configuration
    // ------------------------------------------------------------------------
    {
        micant::ndis::InitializeNdisSubsystem();
        auto& ndisSub = micant::ndis::TitanNdisSubsystem::Instance();
        TEST_ASSERT(ndisSub.isInitialized(), "TitanNdisSubsystem must be initialized");

        // Verify multi-adapter inventory
        size_t adapterCount = ndisSub.getAdapterCount();
        TEST_ASSERT(adapterCount >= 4, "Subsystem must discover at least 4 network adapters");

        // 1. RazzleNet 10GbE PCIe Adapter (BDF 00:04.0)
        auto razzle = std::dynamic_pointer_cast<micant::ndis::RazzleNetAdapter>(ndisSub.getAdapter(L"razzlenet0"));
        TEST_ASSERT(razzle != nullptr, "RazzleNet 10GbE adapter must exist");
        TEST_ASSERT(razzle->getPciAddress().bus == 0 && razzle->getPciAddress().device == 4 && razzle->getPciAddress().function == 0,
                    "RazzleNet must be on PCIe BDF 00:04.0");
        TEST_ASSERT(razzle->getSpeedBps() == 10'000'000'000ULL, "RazzleNet must negotiate 10 Gbps");
        TEST_ASSERT(razzle->getMtu() == 1500, "RazzleNet default MTU must be 1500");
        TEST_ASSERT(razzle->getLinkState() == micant::ndis::MediaConnectState::Connected, "RazzleNet link must be Connected");
        TEST_ASSERT(razzle->getMacAddress().bytes[0] == 0x52 && razzle->getMacAddress().bytes[1] == 0x5A, "RazzleNet MAC prefix must match RZ signature");

        // 2. Intel E1000e Gigabit PCIe Adapter (BDF 00:03.0)
        auto e1000 = std::dynamic_pointer_cast<micant::ndis::IntelE1000eAdapter>(ndisSub.getAdapter(L"e1000e0"));
        TEST_ASSERT(e1000 != nullptr, "Intel E1000e Gigabit adapter must exist");
        TEST_ASSERT(e1000->getPciAddress().bus == 0 && e1000->getPciAddress().device == 3 && e1000->getPciAddress().function == 0,
                    "Intel E1000e must be on PCIe BDF 00:03.0");
        TEST_ASSERT(e1000->getSpeedBps() == 1'000'000'000ULL, "Intel E1000e must negotiate 1 Gbps");
        TEST_ASSERT(e1000->getMtu() == 1500, "Intel E1000e default MTU must be 1500");
        TEST_ASSERT(e1000->getLinkState() == micant::ndis::MediaConnectState::Connected, "Intel E1000e link must be Connected");

        // 3. VirtIO-Net Paravirtualized PCIe Adapter (BDF 00:05.0)
        auto virtio = std::dynamic_pointer_cast<micant::ndis::VirtIoNetAdapter>(ndisSub.getAdapter(L"virtionet0"));
        TEST_ASSERT(virtio != nullptr, "VirtIO-Net adapter must exist");
        TEST_ASSERT(virtio->getPciAddress().bus == 0 && virtio->getPciAddress().device == 5 && virtio->getPciAddress().function == 0,
                    "VirtIO-Net must be on PCIe BDF 00:05.0");
        TEST_ASSERT(virtio->getSpeedBps() == 10'000'000'000ULL, "VirtIO-Net must negotiate 10 Gbps");
        TEST_ASSERT(virtio->getLinkState() == micant::ndis::MediaConnectState::Connected, "VirtIO-Net link must be Connected");

        // 4. Sovereign Virtual Ethernet Adapter (Loopback)
        auto vnet = std::dynamic_pointer_cast<micant::ndis::VirtualNetworkAdapter>(ndisSub.getAdapter(L"vnet0"));
        TEST_ASSERT(vnet != nullptr, "Sovereign Virtual Adapter must exist");
        TEST_ASSERT(vnet->getLinkState() == micant::ndis::MediaConnectState::Connected, "Virtual Adapter link must be Connected");

        // Verify Primary Adapter binding
        auto primary = ndisSub.getPrimaryAdapter();
        TEST_ASSERT(primary != nullptr, "Primary network adapter must be established");
        TEST_ASSERT(primary->getAdapterName() == L"\\Device\\RazzleNet0", "Primary adapter must default to RazzleNet0");
    }

    // ------------------------------------------------------------------------
    // Phase 2: NDIS 6.x NET_BUFFER & NET_BUFFER_LIST (NBL) Scatter-Gather Pools
    // ------------------------------------------------------------------------
    {
        auto& ndisSub = micant::ndis::TitanNdisSubsystem::Instance();
        auto* pool = ndisSub.getNblPool();
        TEST_ASSERT(pool != nullptr, "NBL Pool must be initialized in subsystem");
        size_t initialAllocations = pool->getTotalAllocations();

        // Allocate 5 NBLs from the pool
        auto nbl1 = pool->allocate();
        auto nbl2 = pool->allocate();
        auto nbl3 = pool->allocate();
        auto nbl4 = pool->allocate();
        auto nbl5 = pool->allocate();
        TEST_ASSERT(nbl1 && nbl2 && nbl3 && nbl4 && nbl5, "All 5 NBLs must be allocated from pool");
        TEST_ASSERT(pool->getTotalAllocations() == initialAllocations + 5, "Total allocations counter must increase by 5");
        TEST_ASSERT(pool->getActiveCount() >= 5, "Active NBL count must be >= 5");

        // Populate NBL1 with a chain of NET_BUFFERs (Scatter-Gather MDL simulation)
        auto nb1 = std::make_shared<micant::ndis::NetBuffer>();
        std::vector<uint8_t> payload1(256, 0xAA);
        auto testFrame1 = micant::ndis::buildEthernetFrame(
            micant::ndis::MacAddress(0x00, 0x11, 0x22, 0x33, 0x44, 0x55),
            micant::ndis::MacAddress::razzleNetDefault(),
            micant::ndis::ETHERTYPE_IPV4,
            payload1
        );
        nb1->data = testFrame1;
        nb1->dataLength = static_cast<uint32_t>(testFrame1.size());
        nbl1->firstNetBuffer = nb1;

        auto nb2 = std::make_shared<micant::ndis::NetBuffer>();
        std::vector<uint8_t> payload2(512, 0xBB);
        auto testFrame2 = micant::ndis::buildEthernetFrame(
            micant::ndis::MacAddress(0x00, 0x11, 0x22, 0x33, 0x44, 0x55),
            micant::ndis::MacAddress::razzleNetDefault(),
            micant::ndis::ETHERTYPE_IPV4,
            payload2
        );
        nb2->data = testFrame2;
        nb2->dataLength = static_cast<uint32_t>(testFrame2.size());
        nb1->next = nb2;

        // Chain NBLs: nbl1 -> nbl2 -> nbl3
        nbl1->next = nbl2;
        nbl2->next = nbl3;
        nbl1->status = micant::ndis::NDIS_STATUS_SUCCESS;
        nbl2->status = micant::ndis::NDIS_STATUS_SUCCESS;
        nbl3->status = micant::ndis::NDIS_STATUS_SUCCESS;

        // Verify traversal across NBL chain
        size_t chainLen = 0;
        for (auto curr = nbl1; curr != nullptr; curr = curr->next) {
            chainLen++;
        }
        TEST_ASSERT(chainLen == 3, "NBL chain length must be exactly 3");

        // Send batch through RazzleNet
        auto razzle = std::dynamic_pointer_cast<micant::ndis::RazzleNetAdapter>(ndisSub.getPrimaryAdapter());
        TEST_ASSERT(razzle != nullptr, "Primary adapter must be RazzleNet");
        auto sendStatus = razzle->sendNetBufferLists(nbl1);
        TEST_ASSERT(sendStatus == NtStatus::Success, "sendNetBufferLists must succeed for NBL batch");

        // Free NBLs back to pool
        pool->free(nbl1);
        pool->free(nbl2);
        pool->free(nbl3);
        pool->free(nbl4);
        pool->free(nbl5);
        TEST_ASSERT(pool->getTotalFreed() >= 5, "Total freed count must increment");
    }

    // ------------------------------------------------------------------------
    // Phase 3: Hardware Checksum Offload Generation & Validation
    // ------------------------------------------------------------------------
    {
        // 1. IPv4 Header Checksum Calculation (RFC 791 / RFC 1071)
        std::vector<uint8_t> ipHeader(20, 0);
        ipHeader[0] = 0x45; // Version 4, IHL 5
        ipHeader[1] = 0x00; // DSCP / ECN
        ipHeader[2] = 0x00; // Total Length (64 bytes)
        ipHeader[3] = 0x40;
        ipHeader[4] = 0x12; // Identification
        ipHeader[5] = 0x34;
        ipHeader[6] = 0x40; // Don't Fragment
        ipHeader[7] = 0x00;
        ipHeader[8] = 0x40; // TTL = 64
        ipHeader[9] = 0x06; // Protocol = TCP
        ipHeader[10] = 0x00; // Checksum init 0
        ipHeader[11] = 0x00;
        // Src IP: 192.168.1.50
        ipHeader[12] = 192; ipHeader[13] = 168; ipHeader[14] = 1; ipHeader[15] = 50;
        // Dst IP: 10.0.0.1
        ipHeader[16] = 10; ipHeader[17] = 0; ipHeader[18] = 0; ipHeader[19] = 1;

        uint16_t csum = micant::ndis::calculateIpChecksum(ipHeader.data(), ipHeader.size());
        TEST_ASSERT(csum != 0, "Calculated IPv4 checksum must be non-zero");

        // Insert checksum into header and verify that re-running calculation yields 0 (RFC 1071 property)
        ipHeader[10] = static_cast<uint8_t>(csum >> 8);
        ipHeader[11] = static_cast<uint8_t>(csum & 0xFF);
        uint16_t verifyCsum = micant::ndis::calculateIpChecksum(ipHeader.data(), ipHeader.size());
        TEST_ASSERT(verifyCsum == 0, "Valid IPv4 header with embedded checksum must verify to 0");

        // Corrupt header and verify mismatch
        ipHeader[19] = 2; // Mutate destination IP
        uint16_t corruptVerify = micant::ndis::calculateIpChecksum(ipHeader.data(), ipHeader.size());
        TEST_ASSERT(corruptVerify != 0, "Corrupted IPv4 header must produce non-zero checksum verification failure");

        // 2. Hardware offload capabilities in RazzleNet
        auto& ndisSub = micant::ndis::TitanNdisSubsystem::Instance();
        auto razzle = std::dynamic_pointer_cast<micant::ndis::RazzleNetAdapter>(ndisSub.getPrimaryAdapter());
        const auto& offloads = razzle->getOffloadCapabilities();
        TEST_ASSERT(offloads.checksum.txIpv4Header == true, "Hardware TX IPv4 header checksum must be enabled");
        TEST_ASSERT(offloads.checksum.txTcpIpv4 == true, "Hardware TX TCP IPv4 checksum must be enabled");
        TEST_ASSERT(offloads.checksum.rxIpv4Header == true, "Hardware RX IPv4 header checksum must be enabled");
        TEST_ASSERT(offloads.checksum.rxTcpIpv4 == true, "Hardware RX TCP IPv4 checksum must be enabled");
    }

    // ------------------------------------------------------------------------
    // Phase 4: Large Send Offload v2 (LSOv2 / TSO) Segmentation Engine
    // ------------------------------------------------------------------------
    {
        auto& ndisSub = micant::ndis::TitanNdisSubsystem::Instance();
        auto razzle = std::dynamic_pointer_cast<micant::ndis::RazzleNetAdapter>(ndisSub.getPrimaryAdapter());
        auto statsBefore = razzle->getStatistics();

        // Construct 16 KB Jumbo TCP Frame (> MTU 1500)
        // Headers: Ethernet (14B) + IPv4 (20B) + TCP (20B) = 54B
        const size_t tcpPayloadLen = 16384;
        std::vector<uint8_t> largeFrame(micant::ndis::ETH_HLEN + 40 + tcpPayloadLen, 0xCC);

        // Ethernet Header
        auto* eth = reinterpret_cast<micant::ndis::EthernetHeader*>(largeFrame.data());
        eth->destMac = micant::ndis::MacAddress(0x00, 0x11, 0x22, 0x33, 0x44, 0x55);
        eth->srcMac = micant::ndis::MacAddress::razzleNetDefault();
        eth->setEtherType(micant::ndis::ETHERTYPE_IPV4);

        // IPv4 Header
        uint8_t* ip = largeFrame.data() + micant::ndis::ETH_HLEN;
        ip[0] = 0x45;
        ip[9] = 0x06; // TCP
        // Src IP 192.168.1.100, Dst IP 10.0.0.2
        ip[12] = 192; ip[13] = 168; ip[14] = 1; ip[15] = 100;
        ip[16] = 10; ip[17] = 0; ip[18] = 0; ip[19] = 2;

        // TCP Header (20 bytes)
        uint8_t* tcp = ip + 20;
        // Src Port 8080 (0x1F90), Dst Port 80 (0x0050)
        tcp[0] = 0x1F; tcp[1] = 0x90;
        tcp[2] = 0x00; tcp[3] = 0x50;
        tcp[12] = 0x50; // Data offset 5 (20 bytes)
        tcp[13] = 0x18; // ACK + PSH flags

        // Send large frame triggering LSOv2 segmentation
        NtStatus lsoStatus = razzle->sendPacket(largeFrame);
        TEST_ASSERT(lsoStatus == NtStatus::Success, "sendPacket must succeed for LSOv2 payload");

        auto statsAfter = razzle->getStatistics();
        TEST_ASSERT(statsAfter.lsoPackets > statsBefore.lsoPackets, "LSO segmented packet counter must increment");
        TEST_ASSERT(statsAfter.csumOffloadTx > statsBefore.csumOffloadTx, "TX Checksum offload counter must increment during LSO");
        TEST_ASSERT(statsAfter.txPackets > statsBefore.txPackets, "Total TX packets must increment by segment count");
    }

    // ------------------------------------------------------------------------
    // Phase 5: Receive Segment Coalescing (RSC) Engine
    // ------------------------------------------------------------------------
    {
        auto& ndisSub = micant::ndis::TitanNdisSubsystem::Instance();
        auto razzle = std::dynamic_pointer_cast<micant::ndis::RazzleNetAdapter>(ndisSub.getPrimaryAdapter());
        const auto& offloads = razzle->getOffloadCapabilities();
        TEST_ASSERT(offloads.rsc.enabled == true, "RSC engine must be enabled");
        TEST_ASSERT(offloads.rsc.ipv4Supported == true, "RSC IPv4 support must be active");
        TEST_ASSERT(offloads.rsc.maxCoalescedSize >= 65536, "RSC maximum coalesced size must support up to 64KB");

        // Record simulated RSC coalesced segment in telemetry
        ndisSub.recordRscSegments(8);
        auto stats = razzle->getStatistics();
        TEST_ASSERT(stats.csumOffloadRx >= 0, "RX Checksum offload tracking must be operational");
    }

    // ------------------------------------------------------------------------
    // Phase 6: Receive Side Scaling (RSS) 40-Byte Toeplitz Hash & Core Distribution
    // ------------------------------------------------------------------------
    {
        // Test Microsoft Toeplitz Hash calculation
        std::vector<uint8_t> flowKey1 = { 192, 168, 1, 100,  10, 0, 0, 1,  0x1F, 0x90,  0x00, 0x50 };
        uint32_t hash1_a = micant::ndis::computeToeplitzHash(flowKey1, micant::ndis::DEFAULT_RSS_KEY);
        uint32_t hash1_b = micant::ndis::computeToeplitzHash(flowKey1, micant::ndis::DEFAULT_RSS_KEY);
        TEST_ASSERT(hash1_a != 0, "Toeplitz hash must be non-zero for valid 4-tuple");
        TEST_ASSERT(hash1_a == hash1_b, "Toeplitz hash must be deterministic across calls");

        // Test different flow produces different hash
        std::vector<uint8_t> flowKey2 = { 192, 168, 1, 101,  10, 0, 0, 2,  0x20, 0x00,  0x01, 0xBB };
        uint32_t hash2 = micant::ndis::computeToeplitzHash(flowKey2, micant::ndis::DEFAULT_RSS_KEY);
        TEST_ASSERT(hash1_a != hash2, "Distinct flows must yield distinct Toeplitz hashes");

        // Test RSS Indirection Table lookup across 128 entries
        micant::ndis::RssParameters rss;
        TEST_ASSERT(rss.enabled, "RSS must be enabled by default");
        TEST_ASSERT(rss.numCpuQueues == 4, "Default CPU queue count must be 4");

        uint8_t cpuFlow1 = rss.getTargetCpu(hash1_a);
        uint8_t cpuFlow2 = rss.getTargetCpu(hash2);
        TEST_ASSERT(cpuFlow1 < 4, "Target CPU for flow 1 must be within 0..3");
        TEST_ASSERT(cpuFlow2 < 4, "Target CPU for flow 2 must be within 0..3");

        // Simulate incoming packets on RazzleNet and verify RSS core accounting
        auto& ndisSub = micant::ndis::TitanNdisSubsystem::Instance();
        auto razzle = std::dynamic_pointer_cast<micant::ndis::RazzleNetAdapter>(ndisSub.getPrimaryAdapter());
        
        std::vector<uint8_t> rxFrame(micant::ndis::ETH_HLEN + 40 + 64, 0);
        auto* eth = reinterpret_cast<micant::ndis::EthernetHeader*>(rxFrame.data());
        eth->destMac = micant::ndis::MacAddress::razzleNetDefault();
        eth->srcMac = micant::ndis::MacAddress(0x00, 0x11, 0x22, 0x33, 0x44, 0x55);
        eth->setEtherType(micant::ndis::ETHERTYPE_IPV4);
        uint8_t* ip = rxFrame.data() + micant::ndis::ETH_HLEN;
        ip[0] = 0x45;
        ip[9] = 0x06; // TCP
        ip[12] = 192; ip[13] = 168; ip[14] = 1; ip[15] = 100;
        ip[16] = 10; ip[17] = 0; ip[18] = 0; ip[19] = 1;

        razzle->receivePacket(rxFrame);
        auto statsRx = razzle->getStatistics();
        TEST_ASSERT(statsRx.rxPackets >= 1, "RazzleNet rxPackets must be >= 1");
        TEST_ASSERT(statsRx.csumOffloadRx >= 1, "RazzleNet csumOffloadRx must be >= 1");
    }

    // ------------------------------------------------------------------------
    // Phase 7: NetAdapterCx WDF Class Extension, Ring Buffers & Packet/Fragment Queues
    // ------------------------------------------------------------------------
    {
        auto& ndisSub = micant::ndis::TitanNdisSubsystem::Instance();
        auto netAdp0 = ndisSub.getNetAdapter(L"netadapter0");
        TEST_ASSERT(netAdp0 != nullptr, "Default NetAdapterCx context netadapter0 must exist");
        TEST_ASSERT(netAdp0->getTxQueueCount() >= 1, "netadapter0 must have at least 1 TX queue");
        TEST_ASSERT(netAdp0->getRxQueueCount() >= 1, "netadapter0 must have at least 1 RX queue");
        TEST_ASSERT(netAdp0->getMtu() == 1500, "netadapter0 MTU must be 1500");
        TEST_ASSERT(netAdp0->getSpeedBps() == 2'500'000'000ULL, "netadapter0 speed must be 2.5 Gbps");

        // Inspect TX Queue and Ring Buffer
        auto txQueue = netAdp0->getTxQueue(0);
        TEST_ASSERT(txQueue != nullptr, "TX queue 0 must exist");
        auto* txRing = txQueue->getRingBuffer();
        TEST_ASSERT(txRing != nullptr, "TX ring buffer must exist");
        TEST_ASSERT(txRing->NumberOfElements == 256, "Ring buffer element count must be 256");
        TEST_ASSERT(txRing->ElementIndexMask == 255, "Ring buffer mask must be 255");
        TEST_ASSERT(txRing->packets.size() == 256, "Packet descriptors vector size must be 256");
        TEST_ASSERT(txRing->fragments.size() == 256, "Fragment descriptors vector size must be 256");

        // Advance TX ring
        txQueue->advanceRing(5);
        TEST_ASSERT(txQueue->getPacketsPosted() == 5, "Packets posted must be 5");
        TEST_ASSERT(txQueue->getPacketsCompleted() == 5, "Packets completed must be 5");
        TEST_ASSERT(txRing->BeginIndex == 5, "Ring BeginIndex must advance to 5");

        // Inspect RX Queue and Ring Buffer
        auto rxQueue = netAdp0->getRxQueue(0);
        TEST_ASSERT(rxQueue != nullptr, "RX queue 0 must exist");
        rxQueue->postReceivedPackets(3);
        TEST_ASSERT(rxQueue->getPacketsReceived() == 3, "Packets received must be 3");

        // Test C ABI NetAdapterCx functions
        void* abiNetAdapter = nullptr;
        int32_t stAdp = micant::ndis::NetAdapterCreate(nullptr, nullptr, &abiNetAdapter);
        TEST_ASSERT(stAdp == 0 && abiNetAdapter != nullptr, "NetAdapterCreate C ABI must succeed");

        void* abiTxQueue = nullptr;
        int32_t stTx = micant::ndis::NetTxQueueCreate(abiNetAdapter, nullptr, nullptr, &abiTxQueue);
        TEST_ASSERT(stTx == 0 && abiTxQueue != nullptr, "NetTxQueueCreate C ABI must succeed");

        void* abiRxQueue = nullptr;
        int32_t stRx = micant::ndis::NetRxQueueCreate(abiNetAdapter, nullptr, nullptr, &abiRxQueue);
        TEST_ASSERT(stRx == 0 && abiRxQueue != nullptr, "NetRxQueueCreate C ABI must succeed");

        micant::ndis::NetTxQueueNotifyMoreCompletedPacketsAvailable(abiTxQueue);
        micant::ndis::NetRxQueueNotifyMoreReceivedPacketsAvailable(abiRxQueue);
    }

    // ------------------------------------------------------------------------
    // Phase 8: Link State FSM, Speed Negotiation & Media Flapping Recovery
    // ------------------------------------------------------------------------
    {
        auto& ndisSub = micant::ndis::TitanNdisSubsystem::Instance();
        auto razzle = std::dynamic_pointer_cast<micant::ndis::RazzleNetAdapter>(ndisSub.getPrimaryAdapter());
        TEST_ASSERT(razzle->getLinkState() == micant::ndis::MediaConnectState::Connected, "Initial link state must be Connected");

        // Flap link down
        razzle->setLinkState(micant::ndis::MediaConnectState::Disconnected);
        TEST_ASSERT(razzle->getLinkState() == micant::ndis::MediaConnectState::Disconnected, "Link state must transition to Disconnected");
        TEST_ASSERT((razzle->readMmio32(micant::ndis::RazzleNetAdapter::REG_STATUS) & 0x01) == 0, "MMIO STATUS link bit must be cleared");

        // Verify packet transmission fails while link is down
        std::vector<uint8_t> dummyFrame(64, 0xEE);
        auto* eth = reinterpret_cast<micant::ndis::EthernetHeader*>(dummyFrame.data());
        eth->destMac = micant::ndis::MacAddress(0x00, 0x11, 0x22, 0x33, 0x44, 0x55);
        eth->srcMac = micant::ndis::MacAddress::razzleNetDefault();
        eth->setEtherType(micant::ndis::ETHERTYPE_IPV4);

        NtStatus txDownSt = razzle->sendPacket(dummyFrame);
        TEST_ASSERT(txDownSt == NtStatus::DeviceNotReady, "sendPacket must fail with DeviceNotReady while disconnected");

        // Flap link back up
        razzle->setLinkState(micant::ndis::MediaConnectState::Connected);
        TEST_ASSERT(razzle->getLinkState() == micant::ndis::MediaConnectState::Connected, "Link state must recover to Connected");
        TEST_ASSERT(razzle->getLinkStateChangeCount() >= 2, "Link state change count must reflect disconnect + reconnect");

        // Miniport Hardware Reset (NdisMResetMiniport)
        uint32_t resetBefore = razzle->getResetCount();
        int32_t rstSt = micant::ndis::NdisMResetMiniport(nullptr);
        TEST_ASSERT(rstSt == 0, "NdisMResetMiniport C ABI must return SUCCESS");
        TEST_ASSERT(razzle->getResetCount() == resetBefore + 1, "Adapter hardware reset count must increment");
        TEST_ASSERT(razzle->readMmio32(micant::ndis::RazzleNetAdapter::REG_STATUS) == 0x00000003, "Hardware reset must restore link state");

        // Verify transmission succeeds after reset
        NtStatus txRestoredSt = razzle->sendPacket(dummyFrame);
        TEST_ASSERT(txRestoredSt == NtStatus::Success, "sendPacket must succeed after hardware reset");
    }

    // ------------------------------------------------------------------------
    // Phase 9: Single Root I/O Virtualization (SR-IOV) Virtual Function Management
    // ------------------------------------------------------------------------
    {
        auto& ndisSub = micant::ndis::TitanNdisSubsystem::Instance();
        auto razzle = std::dynamic_pointer_cast<micant::ndis::RazzleNetAdapter>(ndisSub.getPrimaryAdapter());
        auto& sriov = razzle->getSriovManager();

        TEST_ASSERT(sriov.enableSriov(8), "SR-IOV enable for 8 VFs must succeed");
        TEST_ASSERT(sriov.isSriovEnabled(), "SR-IOV must be enabled");
        TEST_ASSERT(sriov.getActiveVfCount() == 8, "Active VF count must be 8");

        // Configure VF 3
        micant::ndis::MacAddress vf3Mac(0x00, 0x15, 0x5D, 0xAA, 0xBB, 0x03);
        bool cfgOk = sriov.configureVf(3, vf3Mac, 200, 1000, true);
        TEST_ASSERT(cfgOk, "configureVf must succeed for VF 3");

        const auto* vf3 = sriov.getVf(3);
        TEST_ASSERT(vf3 != nullptr, "VF 3 must exist");
        TEST_ASSERT(vf3->allocated == true, "VF 3 must be marked allocated");
        TEST_ASSERT(vf3->vlanId == 200, "VF 3 VLAN must be 200");
        TEST_ASSERT(vf3->maxRateMbps == 1000, "VF 3 rate cap must be 1000 Mbps");
        TEST_ASSERT(vf3->spoofCheck == true, "VF 3 spoof checking must be enabled");
        TEST_ASSERT(vf3->vfMac == vf3Mac, "VF 3 MAC address must match configured value");

        // Record traffic for VF 3
        sriov.recordVfTx(3, 2048);
        sriov.recordVfRx(3, 4096);
        TEST_ASSERT(vf3->txPackets == 1 && vf3->txBytes == 2048, "VF 3 TX telemetry must record 1 packet / 2048 bytes");
        TEST_ASSERT(vf3->rxPackets == 1 && vf3->rxBytes == 4096, "VF 3 RX telemetry must record 1 packet / 4096 bytes");

        // Release VF 3
        TEST_ASSERT(sriov.releaseVf(3), "releaseVf must succeed");
        TEST_ASSERT(vf3->allocated == false, "VF 3 allocated must be false after release");
    }

    // ------------------------------------------------------------------------
    // Phase 10: Dynamic Loader Exports, SCM Services & NDIS Diagnostic Telemetry
    // ------------------------------------------------------------------------
    {
        // 1. JanusLDR Driver Exports Verification
        auto& ldr = ldr::DynamicLoader::get();
        TEST_ASSERT(ldr.getExport("ndis.sys", "NdisMRegisterMiniportDriver") != nullptr, "ndis.sys NdisMRegisterMiniportDriver must exist");
        TEST_ASSERT(ldr.getExport("ndis.sys", "NdisMDeregisterMiniportDriver") != nullptr, "ndis.sys NdisMDeregisterMiniportDriver must exist");
        TEST_ASSERT(ldr.getExport("ndis.sys", "NdisMSetMiniportAttributes") != nullptr, "ndis.sys NdisMSetMiniportAttributes must exist");
        TEST_ASSERT(ldr.getExport("ndis.sys", "NdisAllocateNetBufferListPool") != nullptr, "ndis.sys NdisAllocateNetBufferListPool must exist");
        TEST_ASSERT(ldr.getExport("ndis.sys", "NdisFreeNetBufferListPool") != nullptr, "ndis.sys NdisFreeNetBufferListPool must exist");
        TEST_ASSERT(ldr.getExport("ndis.sys", "NdisAllocateNetBufferList") != nullptr, "ndis.sys NdisAllocateNetBufferList must exist");
        TEST_ASSERT(ldr.getExport("ndis.sys", "NdisFreeNetBufferList") != nullptr, "ndis.sys NdisFreeNetBufferList must exist");
        TEST_ASSERT(ldr.getExport("ndis.sys", "NdisMIndicateReceiveNetBufferLists") != nullptr, "ndis.sys NdisMIndicateReceiveNetBufferLists must exist");
        TEST_ASSERT(ldr.getExport("ndis.sys", "NdisMSendNetBufferListsComplete") != nullptr, "ndis.sys NdisMSendNetBufferListsComplete must exist");
        TEST_ASSERT(ldr.getExport("ndis.sys", "NdisQueryAdapterInformation") != nullptr, "ndis.sys NdisQueryAdapterInformation must exist");
        TEST_ASSERT(ldr.getExport("ndis.sys", "NdisMResetMiniport") != nullptr, "ndis.sys NdisMResetMiniport must exist");

        TEST_ASSERT(ldr.getExport("razzlenet.sys", "RazzleNetInitializeMiniport") != nullptr, "razzlenet.sys RazzleNetInitializeMiniport must exist");
        TEST_ASSERT(ldr.getExport("razzlenet.sys", "RazzleNetTransmitPacket") != nullptr, "razzlenet.sys RazzleNetTransmitPacket must exist");

        TEST_ASSERT(ldr.getExport("netadaptercx.sys", "NetAdapterCreate") != nullptr, "netadaptercx.sys NetAdapterCreate must exist");
        TEST_ASSERT(ldr.getExport("netadaptercx.sys", "NetAdapterStart") != nullptr, "netadaptercx.sys NetAdapterStart must exist");
        TEST_ASSERT(ldr.getExport("netadaptercx.sys", "NetAdapterStop") != nullptr, "netadaptercx.sys NetAdapterStop must exist");
        TEST_ASSERT(ldr.getExport("netadaptercx.sys", "NetTxQueueCreate") != nullptr, "netadaptercx.sys NetTxQueueCreate must exist");
        TEST_ASSERT(ldr.getExport("netadaptercx.sys", "NetRxQueueCreate") != nullptr, "netadaptercx.sys NetRxQueueCreate must exist");
        TEST_ASSERT(ldr.getExport("netadaptercx.sys", "NetTxQueueNotifyMoreCompletedPacketsAvailable") != nullptr, "netadaptercx.sys NetTxQueueNotifyMoreCompletedPacketsAvailable must exist");
        TEST_ASSERT(ldr.getExport("netadaptercx.sys", "NetRxQueueNotifyMoreReceivedPacketsAvailable") != nullptr, "netadaptercx.sys NetRxQueueNotifyMoreReceivedPacketsAvailable must exist");

        TEST_ASSERT(ldr.getExport("e1000e.sys", "E1000eInitializeMiniport") != nullptr, "e1000e.sys E1000eInitializeMiniport must exist");

        // 2. SCM Service Records Verification
        auto& scmMgr = scm::ServiceControlManager::get();
        auto ndisSvc = scmMgr.getServiceRecord(L"ndis");
        TEST_ASSERT(ndisSvc != nullptr, "ndis service must be registered in SCM");
        TEST_ASSERT(ndisSvc->serviceType == scm::SERVICE_KERNEL_DRIVER, "ndis must be a kernel driver");
        TEST_ASSERT(ndisSvc->startType == scm::SERVICE_BOOT_START, "ndis must be boot start");

        auto razzleSvc = scmMgr.getServiceRecord(L"razzlenet");
        TEST_ASSERT(razzleSvc != nullptr, "razzlenet service must be registered in SCM");

        auto netAdpSvc = scmMgr.getServiceRecord(L"netadaptercx");
        TEST_ASSERT(netAdpSvc != nullptr, "netadaptercx service must be registered in SCM");
        TEST_ASSERT(netAdpSvc->startType == scm::SERVICE_BOOT_START, "netadaptercx must be boot start");

        auto e1000Svc = scmMgr.getServiceRecord(L"e1000e");
        TEST_ASSERT(e1000Svc != nullptr, "e1000e service must be registered in SCM");

        // 3. Version Database Verification
        auto& verDb = version::VersionDatabase::Instance();
        TEST_ASSERT(verDb.GetModuleInfo("ndis.sys") != nullptr, "ndis.sys must be registered in Version Database");
        TEST_ASSERT(verDb.GetModuleInfo("razzlenet.sys") != nullptr, "razzlenet.sys must be registered in Version Database");
        TEST_ASSERT(verDb.GetModuleInfo("netadaptercx.sys") != nullptr, "netadaptercx.sys must be registered in Version Database");
        TEST_ASSERT(verDb.GetModuleInfo("e1000e.sys") != nullptr, "e1000e.sys must be registered in Version Database");

        // 4. NDIS Diagnostic Telemetry Verification
        auto telem = micant::ndis::NdisQueryTelemetry();
        TEST_ASSERT(telem.totalAdaptersEnumerated >= 4, "totalAdaptersEnumerated must be >= 4");
        TEST_ASSERT(telem.totalPacketsTransmitted >= 1, "totalPacketsTransmitted must be >= 1");
        TEST_ASSERT(telem.totalBytesTransmitted >= 1, "totalBytesTransmitted must be >= 1");
        TEST_ASSERT(telem.totalPacketsReceived >= 1, "totalPacketsReceived must be >= 1");
        TEST_ASSERT(telem.totalBytesReceived >= 1, "totalBytesReceived must be >= 1");
        TEST_ASSERT(telem.totalLsoSegmentsCreated >= 1, "totalLsoSegmentsCreated must be >= 1");
        TEST_ASSERT(telem.totalRscSegmentsCoalesced >= 1, "totalRscSegmentsCoalesced must be >= 1");
        TEST_ASSERT(telem.totalCsumTxOffloaded >= 1, "totalCsumTxOffloaded must be >= 1");
        TEST_ASSERT(telem.totalCsumRxValidated >= 1, "totalCsumRxValidated must be >= 1");
        TEST_ASSERT(telem.totalNblAllocations >= 5, "totalNblAllocations must be >= 5");
        TEST_ASSERT(telem.totalNblFreed >= 5, "totalNblFreed must be >= 5");
        TEST_ASSERT(telem.totalLinkStateChanges >= 2, "totalLinkStateChanges must be >= 2");
        TEST_ASSERT(telem.totalHardwareResets >= 1, "totalHardwareResets must be >= 1");
        TEST_ASSERT(telem.totalNetAdapterQueuesCreated >= 2, "totalNetAdapterQueuesCreated must be >= 2");
        TEST_ASSERT(telem.totalNetPacketsProcessed >= 8, "totalNetPacketsProcessed must be >= 8");

        micant::ndis::NdisResetTelemetryForTesting();
        auto telemReset = micant::ndis::NdisQueryTelemetry();
        TEST_ASSERT(telemReset.totalRscSegmentsCoalesced == 0, "totalRscSegmentsCoalesced must be 0 after reset");
    }

    std::cout << "[TEST] Suite 250: Horizon 34 - NDIS 6.x & NetAdapterCx Networking Architecture PASSED.\n";
}

// ============================================================================
// Suite 251: Horizon 35 - ACPI / HAL Device Tree & AML Interpreter Subsystem
// ============================================================================

void Test_Horizon35_ACPI_HAL_DeviceTree_And_AML_Interpreter() {
    std::cout << "\n========================================================================\n";
    std::cout << "  Suite 251: Horizon 35 - ACPI / HAL Device Tree & AML Interpreter      \n";
    std::cout << "========================================================================\n";

    // Initialize Subsystem
    acpi::InitializeAcpiSubsystem();
    auto& acpiSub = acpi::TitanAcpiSubsystem::Instance();
    TEST_ASSERT(acpiSub.isInitialized(), "TitanACPI subsystem must be initialized");

    // ------------------------------------------------------------------------
    // Phase 1: Physical ACPI Table Discovery & XSDT/RSDT Traversal
    // ------------------------------------------------------------------------
    {
        const auto& rsdp = acpiSub.getRsdp();
        TEST_ASSERT(std::memcmp(rsdp.signature, acpi::ACPI_SIG_RSDP, 8) == 0, "RSDP signature must be 'RSD PTR '");
        TEST_ASSERT(std::memcmp(rsdp.oemId, "MICANT", 6) == 0, "RSDP OEM ID must match MICANT");
        TEST_ASSERT(rsdp.revision == 2, "RSDP revision must be 2 (ACPI 2.0+)");
        TEST_ASSERT(acpi::VerifyAcpiChecksum(&rsdp, 20), "ACPI 1.0 RSDP 20-byte checksum must be valid");
        TEST_ASSERT(acpi::VerifyAcpiChecksum(&rsdp, sizeof(acpi::AcpiRsdp)), "ACPI 2.0+ Extended RSDP checksum must be valid");
        TEST_ASSERT(rsdp.xsdtAddress != 0, "XSDT 64-bit address must be present");
        TEST_ASSERT(rsdp.rsdtAddress != 0, "RSDT 32-bit address must be present");

        // Verify physical table map
        auto tables = acpiSub.getAllTables();
        TEST_ASSERT(tables.size() >= 10, "Subsystem must discover at least 10 physical tables");

        // Verify XSDT (64-bit pointers)
        const auto& xsdt = acpiSub.getXsdtHeader();
        TEST_ASSERT(std::memcmp(xsdt.signature, "XSDT", 4) == 0, "XSDT signature must be 'XSDT'");
        TEST_ASSERT(xsdt.revision == 1, "XSDT revision must be 1");
        const auto& xsdtEntries = acpiSub.getXsdtEntries();
        TEST_ASSERT(xsdtEntries.size() == 7, "XSDT must contain 7 standard 64-bit table pointers");

        // Verify RSDT (32-bit pointers)
        const auto& rsdt = acpiSub.getRsdtHeader();
        TEST_ASSERT(std::memcmp(rsdt.signature, "RSDT", 4) == 0, "RSDT signature must be 'RSDT'");
        TEST_ASSERT(rsdt.revision == 1, "RSDT revision must be 1");
        const auto& rsdtEntries = acpiSub.getRsdtEntries();
        TEST_ASSERT(rsdtEntries.size() == 7, "RSDT must contain 7 standard 32-bit table pointers");

        // Verify MADT (APIC)
        const auto& madtHdr = acpiSub.getMadtHeader();
        TEST_ASSERT(std::memcmp(madtHdr.header.signature, "APIC", 4) == 0, "MADT signature must be 'APIC'");
        TEST_ASSERT(madtHdr.localApicAddress == 0xFEE00000, "Local APIC default MMIO must be 0xFEE00000");
        TEST_ASSERT(acpiSub.getLocalApics().size() == 4, "MADT must enumerate 4 SMP Processor Local APICs");
        TEST_ASSERT(acpiSub.getIoApics().size() == 1, "MADT must enumerate 1 Primary I/O APIC");
        TEST_ASSERT(acpiSub.getIoApics()[0].ioApicAddress == 0xFEC00000, "I/O APIC base must be 0xFEC00000");
        TEST_ASSERT(acpiSub.getIntrOverrides().size() == 2, "MADT must declare 2 Interrupt Source Overrides (IRQ0, IRQ9)");

        // Verify MCFG (PCIe ECAM)
        const auto& mcfg = acpiSub.getMcfg();
        TEST_ASSERT(std::memcmp(mcfg.header.signature, "MCFG", 4) == 0, "MCFG signature must be 'MCFG'");
        TEST_ASSERT(mcfg.allocations[0].baseAddress == 0xE0000000ULL, "PCIe ECAM base must be 0xE0000000");
        TEST_ASSERT(mcfg.allocations[0].startBusNumber == 0 && mcfg.allocations[0].endBusNumber == 255, "PCIe buses 0-255");

        // Verify DMAR & SRAT
        const auto& dmar = acpiSub.getDmar();
        TEST_ASSERT(std::memcmp(dmar.header.signature, "DMAR", 4) == 0, "DMAR signature must be 'DMAR'");
        TEST_ASSERT(dmar.hostAddressWidth == 39, "DMAR host address width must be 39");

        const auto& srat = acpiSub.getSrat();
        TEST_ASSERT(std::memcmp(srat.header.signature, "SRAT", 4) == 0, "SRAT signature must be 'SRAT'");
        TEST_ASSERT(srat.header.revision == 3, "SRAT revision must be 3");
    }

    // ------------------------------------------------------------------------
    // Phase 2: HPET (High Precision Event Timer) & BGRT (Boot Graphics) Table Verification
    // ------------------------------------------------------------------------
    {
        uint64_t hpetAddr = 0;
        uint32_t hpetLen = 0;
        BOOL foundHpet = acpi::AcpiFindTable("HPET", &hpetAddr, &hpetLen);
        TEST_ASSERT(foundHpet == acpi::TRUE, "AcpiFindTable must locate HPET table");
        TEST_ASSERT(hpetAddr != 0, "HPET physical address must be non-zero");
        TEST_ASSERT(hpetLen == sizeof(acpi::AcpiHpetTable), "HPET length must equal sizeof(AcpiHpetTable)");

        const auto& hpet = acpiSub.getHpet();
        TEST_ASSERT(std::memcmp(hpet.header.signature, "HPET", 4) == 0, "HPET signature must match 'HPET'");
        TEST_ASSERT(hpet.header.revision == 1, "HPET revision must be 1");
        TEST_ASSERT(acpi::VerifyAcpiChecksum(&hpet, sizeof(acpi::AcpiHpetTable)), "HPET checksum must be valid");
        TEST_ASSERT(hpet.eventTimerBlockId == 0x8086A001, "HPET Event Timer Block ID must be 0x8086A001");
        TEST_ASSERT(hpet.baseAddress.addressSpaceId == 0, "HPET GAS AddressSpaceId must be 0 (System Memory)");
        TEST_ASSERT(hpet.baseAddress.address == 0xFED00000ULL, "HPET Base Address must be 0xFED00000");
        TEST_ASSERT(hpet.baseAddress.registerBitWidth == 64, "HPET RegisterBitWidth must be 64");
        TEST_ASSERT(hpet.hpetNumber == 0, "HPET sequence number must be 0");
        TEST_ASSERT(hpet.mainCounterMinClockTick == 100, "HPET min clock tick must be 100ns (10MHz)");

        uint64_t bgrtAddr = 0;
        uint32_t bgrtLen = 0;
        BOOL foundBgrt = acpi::AcpiFindTable("BGRT", &bgrtAddr, &bgrtLen);
        TEST_ASSERT(foundBgrt == acpi::TRUE, "AcpiFindTable must locate BGRT table");
        TEST_ASSERT(bgrtAddr != 0, "BGRT physical address must be non-zero");
        TEST_ASSERT(bgrtLen == sizeof(acpi::AcpiBgrtTable), "BGRT length must equal sizeof(AcpiBgrtTable)");

        const auto& bgrt = acpiSub.getBgrt();
        TEST_ASSERT(std::memcmp(bgrt.header.signature, "BGRT", 4) == 0, "BGRT signature must match 'BGRT'");
        TEST_ASSERT(bgrt.header.revision == 1, "BGRT revision must be 1");
        TEST_ASSERT(acpi::VerifyAcpiChecksum(&bgrt, sizeof(acpi::AcpiBgrtTable)), "BGRT checksum must be valid");
        TEST_ASSERT(bgrt.version == 1, "BGRT version must be 1");
        TEST_ASSERT(bgrt.status == 1, "BGRT status must be 1 (Displayed)");
        TEST_ASSERT(bgrt.imageType == 0, "BGRT imageType must be 0 (Bitmap)");
        TEST_ASSERT(bgrt.imageAddress == 0x000000007FE00000ULL, "BGRT imageAddress must be 0x7FE00000");
        TEST_ASSERT(bgrt.imageOffsetX == 384 && bgrt.imageOffsetY == 284, "BGRT offsets must be (384, 284)");
    }

    // ------------------------------------------------------------------------
    // Phase 3: AML Bytecode Stream Execution & Opcode Arithmetic/Logic Engine
    // ------------------------------------------------------------------------
    {
        acpi::AmlBytecodeParser parser;

        // 1. Arithmetic Execution: Store(10, Local0); Store(25, Local1); Add(Local0, Local1, Local2)
        std::vector<uint8_t> arithCode = {
            acpi::AML_STORE_OP, acpi::AML_BYTE_PREFIX, 10, acpi::AML_LOCAL0_OP,
            acpi::AML_STORE_OP, acpi::AML_BYTE_PREFIX, 25, acpi::AML_LOCAL1_OP,
            acpi::AML_ADD_OP, acpi::AML_LOCAL0_OP, acpi::AML_LOCAL1_OP, acpi::AML_LOCAL2_OP,
            // Subtract(Local2, 5, Local3)
            acpi::AML_SUBTRACT_OP, acpi::AML_LOCAL2_OP, acpi::AML_BYTE_PREFIX, 5, acpi::AML_LOCAL3_OP,
            // Multiply(Local3, 2, Local4)
            acpi::AML_MULTIPLY_OP, acpi::AML_LOCAL3_OP, acpi::AML_BYTE_PREFIX, 2, acpi::AML_LOCAL4_OP,
            // Divide(Local4, 7, Local5, Local6) -> Local5=Remainder (4), Local6=Quotient (8)
            acpi::AML_DIVIDE_OP, acpi::AML_LOCAL4_OP, acpi::AML_BYTE_PREFIX, 7, acpi::AML_LOCAL5_OP, acpi::AML_LOCAL6_OP,
            // ShiftLeft(Local6, 3, Local7) -> 8 << 3 = 64
            acpi::AML_SHIFT_LEFT_OP, acpi::AML_LOCAL6_OP, acpi::AML_BYTE_PREFIX, 3, acpi::AML_LOCAL7_OP,
            // ShiftRight(Local7, 2, Local0) -> 64 >> 2 = 16
            acpi::AML_SHIFT_RIGHT_OP, acpi::AML_LOCAL7_OP, acpi::AML_BYTE_PREFIX, 2, acpi::AML_LOCAL0_OP
        };
        parser.executeBytecode(arithCode);
        TEST_ASSERT(parser.getLocal(0) == 16, "Local0 must be 16 after ShiftRight");
        TEST_ASSERT(parser.getLocal(1) == 25, "Local1 must be 25");
        TEST_ASSERT(parser.getLocal(2) == 35, "Local2 must be 35 after Add(10, 25)");
        TEST_ASSERT(parser.getLocal(3) == 30, "Local3 must be 30 after Subtract(35, 5)");
        TEST_ASSERT(parser.getLocal(4) == 60, "Local4 must be 60 after Multiply(30, 2)");
        TEST_ASSERT(parser.getLocal(5) == 4,  "Local5 must be 4 (Remainder 60 % 7)");
        TEST_ASSERT(parser.getLocal(6) == 8,  "Local6 must be 8 (Quotient 60 / 7)");
        TEST_ASSERT(parser.getLocal(7) == 64, "Local7 must be 64 after ShiftLeft(8, 3)");

        // 2. Bitwise Logic: And, Or, Xor, Not
        std::vector<uint8_t> bitCode = {
            acpi::AML_AND_OP, acpi::AML_BYTE_PREFIX, 0xFF, acpi::AML_BYTE_PREFIX, 0x0F, acpi::AML_LOCAL1_OP,
            acpi::AML_OR_OP,  acpi::AML_BYTE_PREFIX, 0xF0, acpi::AML_BYTE_PREFIX, 0x0F, acpi::AML_LOCAL2_OP,
            acpi::AML_XOR_OP, acpi::AML_BYTE_PREFIX, 0xAA, acpi::AML_BYTE_PREFIX, 0x55, acpi::AML_LOCAL3_OP,
            acpi::AML_NOT_OP, acpi::AML_BYTE_PREFIX, 0x00, acpi::AML_LOCAL4_OP
        };
        parser.executeBytecode(bitCode);
        TEST_ASSERT(parser.getLocal(1) == 0x0F, "And(0xFF, 0x0F) must be 0x0F");
        TEST_ASSERT(parser.getLocal(2) == 0xFF, "Or(0xF0, 0x0F) must be 0xFF");
        TEST_ASSERT(parser.getLocal(3) == 0xFF, "Xor(0xAA, 0x55) must be 0xFF");
        TEST_ASSERT(parser.getLocal(4) == 0xFFFFFFFFFFFFFFFFULL, "Not(0) must invert all 64 bits");

        // 3. Relational & Logical Operators: LEqual, LGreater, LLess, LAnd, LOr, LNot
        std::vector<uint8_t> logicCode = {
            acpi::AML_STORE_OP, acpi::AML_LEQUAL_OP, acpi::AML_BYTE_PREFIX, 42, acpi::AML_BYTE_PREFIX, 42, acpi::AML_LOCAL0_OP,
            acpi::AML_STORE_OP, acpi::AML_LGREATER_OP, acpi::AML_BYTE_PREFIX, 50, acpi::AML_BYTE_PREFIX, 20, acpi::AML_LOCAL1_OP,
            acpi::AML_STORE_OP, acpi::AML_LLESS_OP, acpi::AML_BYTE_PREFIX, 10, acpi::AML_BYTE_PREFIX, 30, acpi::AML_LOCAL2_OP,
            acpi::AML_STORE_OP, acpi::AML_LAND_OP, acpi::AML_LOCAL0_OP, acpi::AML_LOCAL1_OP, acpi::AML_LOCAL3_OP,
            acpi::AML_STORE_OP, acpi::AML_LNOT_OP, acpi::AML_ZERO_OP, acpi::AML_LOCAL4_OP
        };
        parser.executeBytecode(logicCode);
        TEST_ASSERT(parser.getLocal(0) == 1, "LEqual(42, 42) must be 1");
        TEST_ASSERT(parser.getLocal(1) == 1, "LGreater(50, 20) must be 1");
        TEST_ASSERT(parser.getLocal(2) == 1, "LLess(10, 30) must be 1");
        TEST_ASSERT(parser.getLocal(3) == 1, "LAnd(1, 1) must be 1");
        TEST_ASSERT(parser.getLocal(4) == 1, "LNot(0) must be 1");

        // 4. Loops & Control Flow: While(Local0 < 5) { Local0 = Local0 + 1 }
        std::vector<uint8_t> whileCode = {
            acpi::AML_STORE_OP, acpi::AML_ZERO_OP, acpi::AML_LOCAL0_OP,
            acpi::AML_WHILE_OP, 9,
            acpi::AML_LLESS_OP, acpi::AML_LOCAL0_OP, acpi::AML_BYTE_PREFIX, 5,
            acpi::AML_ADD_OP, acpi::AML_LOCAL0_OP, acpi::AML_ONE_OP, acpi::AML_LOCAL0_OP
        };
        parser.executeBytecode(whileCode);
        TEST_ASSERT(parser.getLocal(0) == 5, "While loop must iterate 5 times and set Local0 = 5");

        // 5. Method with Arguments & Return: Return(Arg0 + Arg1)
        std::vector<uint8_t> methodCode = {
            acpi::AML_ADD_OP, acpi::AML_ARG0_OP, acpi::AML_ARG1_OP, acpi::AML_LOCAL0_OP,
            acpi::AML_RETURN_OP, acpi::AML_LOCAL0_OP
        };
        uint64_t retVal = parser.executeBytecode(methodCode, { acpi::AmlValue::MakeInteger(123), acpi::AmlValue::MakeInteger(456) });
        TEST_ASSERT(retVal == 579, "Method execution with Arg0=123 and Arg1=456 must return 579");
        TEST_ASSERT(parser.hasReturned() == true, "hasReturned flag must be true");
    }

    // ------------------------------------------------------------------------
    // Phase 4: ACPI Small & Large Resource Item Descriptor Encoding & Decoding
    // ------------------------------------------------------------------------
    {
        std::vector<acpi::ACPI_RESOURCE> originalList;

        // 1. FixedIoPort: 0x03F8, len 8 (COM1)
        acpi::ACPI_RESOURCE res1{};
        res1.type = acpi::AcpiResourceType::FixedIoPort;
        res1.fixedIo.baseAddress = 0x03F8;
        res1.fixedIo.length = 8;
        originalList.push_back(res1);

        // 2. IoPort: 0x01F0..0x01F7, len 8, align 1 (IDE Primary)
        acpi::ACPI_RESOURCE res2{};
        res2.type = acpi::AcpiResourceType::IoPort;
        res2.io.baseAddress = 0x01F0;
        res2.io.alignment = 1;
        res2.io.length = 8;
        originalList.push_back(res2);

        // 3. Irq: IRQ 4
        acpi::ACPI_RESOURCE res3{};
        res3.type = acpi::AcpiResourceType::Irq;
        res3.irq.irqMask = 0x0010;
        res3.irq.triggering = 0; // Edge
        res3.irq.polarity = 0;   // High
        res3.irq.sharable = 0;   // Exclusive
        originalList.push_back(res3);

        // 4. Dma: Channel 2
        acpi::ACPI_RESOURCE res4{};
        res4.type = acpi::AcpiResourceType::Dma;
        res4.dma.channelMask = 0x04;
        res4.dma.transferType = 0;
        originalList.push_back(res4);

        // 5. FixedMemory32: 0xD0000000, 1MB
        acpi::ACPI_RESOURCE res5{};
        res5.type = acpi::AcpiResourceType::FixedMemory32;
        res5.fixedMemory32.baseAddress = 0xD0000000;
        res5.fixedMemory32.length = 0x00100000;
        res5.fixedMemory32.writeProtect = 1;
        originalList.push_back(res5);

        // 6. Memory32: 0xE0000000..0xEFFFFFFF, 16MB
        acpi::ACPI_RESOURCE res6{};
        res6.type = acpi::AcpiResourceType::Memory32;
        res6.memory32.minimumAddress = 0xE0000000;
        res6.memory32.maximumAddress = 0xEFFFFFFF;
        res6.memory32.alignment = 0x1000;
        res6.memory32.length = 0x01000000;
        res6.memory32.writeProtect = 1;
        originalList.push_back(res6);

        // 7. ExtendedIrq: GSI 24, Level, Low, Shared
        acpi::ACPI_RESOURCE res7{};
        res7.type = acpi::AcpiResourceType::ExtendedIrq;
        res7.extendedIrq.gsi = 24;
        res7.extendedIrq.triggering = 1;
        res7.extendedIrq.polarity = 1;
        res7.extendedIrq.sharable = 1;
        originalList.push_back(res7);

        // Encode to byte buffer
        std::vector<uint8_t> encoded = acpi::EncodeAcpiResources(originalList);
        TEST_ASSERT(!encoded.empty(), "Encoded ACPI resource buffer must not be empty");
        TEST_ASSERT(encoded.back() == 0x00 && encoded[encoded.size() - 2] == 0x79, "Buffer must end with End Tag (0x79, 0x00)");

        // Decode from byte buffer
        std::vector<acpi::ACPI_RESOURCE> decoded = acpi::DecodeAcpiResources(encoded);
        TEST_ASSERT(decoded.size() == 7, "Decoded resource count must match original count of 7");

        TEST_ASSERT(decoded[0].type == acpi::AcpiResourceType::FixedIoPort, "Item 0 must be FixedIoPort");
        TEST_ASSERT(decoded[0].fixedIo.baseAddress == 0x03F8 && decoded[0].fixedIo.length == 8, "COM1 IO match");

        TEST_ASSERT(decoded[1].type == acpi::AcpiResourceType::IoPort, "Item 1 must be IoPort");
        TEST_ASSERT(decoded[1].io.baseAddress == 0x01F0 && decoded[1].io.length == 8, "IDE IO match");

        TEST_ASSERT(decoded[2].type == acpi::AcpiResourceType::Irq, "Item 2 must be Irq");
        TEST_ASSERT(decoded[2].irq.irqMask == 0x0010, "IRQ 4 mask match");

        TEST_ASSERT(decoded[3].type == acpi::AcpiResourceType::Dma, "Item 3 must be Dma");
        TEST_ASSERT(decoded[3].dma.channelMask == 0x04, "DMA Channel 2 match");

        TEST_ASSERT(decoded[4].type == acpi::AcpiResourceType::FixedMemory32, "Item 4 must be FixedMemory32");
        TEST_ASSERT(decoded[4].fixedMemory32.baseAddress == 0xD0000000 && decoded[4].fixedMemory32.length == 0x00100000, "FixedMemory32 match");

        TEST_ASSERT(decoded[5].type == acpi::AcpiResourceType::Memory32, "Item 5 must be Memory32");
        TEST_ASSERT(decoded[5].memory32.minimumAddress == 0xE0000000 && decoded[5].memory32.length == 0x01000000, "Memory32 match");

        TEST_ASSERT(decoded[6].type == acpi::AcpiResourceType::ExtendedIrq, "Item 6 must be ExtendedIrq");
        TEST_ASSERT(decoded[6].extendedIrq.gsi == 24 && decoded[6].extendedIrq.triggering == 1, "ExtendedIrq GSI 24 Level match");

        // C ABI AcpiGetDeviceResources test
        acpi::ACPI_RESOURCE devResources[8];
        uint32_t count = 0;
        BOOL resPci = acpi::AcpiGetDeviceResources("\\_SB.PCI0", devResources, 8, &count);
        TEST_ASSERT(resPci == acpi::TRUE && count >= 2, "AcpiGetDeviceResources for PCI0 must succeed with >= 2 resources");

        BOOL resGfx = acpi::AcpiGetDeviceResources("\\_SB.PCI0.GFX0", devResources, 8, &count);
        TEST_ASSERT(resGfx == acpi::TRUE && count >= 2, "AcpiGetDeviceResources for GFX0 must succeed with FixedMemory32 and ExtendedIrq");
        TEST_ASSERT(devResources[0].type == acpi::AcpiResourceType::FixedMemory32, "GFX0 first resource must be FixedMemory32");
        TEST_ASSERT(devResources[0].fixedMemory32.baseAddress == 0xE1000000, "GFX0 base address must be 0xE1000000");
        TEST_ASSERT(devResources[1].type == acpi::AcpiResourceType::ExtendedIrq, "GFX0 second resource must be ExtendedIrq");
        TEST_ASSERT(devResources[1].extendedIrq.gsi == 16, "GFX0 GSI must be 16");

        BOOL resNvme = acpi::AcpiGetDeviceResources("\\_SB.PCI0.NVME", devResources, 8, &count);
        TEST_ASSERT(resNvme == acpi::TRUE && devResources[1].extendedIrq.gsi == 17, "NVME GSI must be 17");

        BOOL resUsb = acpi::AcpiGetDeviceResources("\\_SB.PCI0.XUSB", devResources, 8, &count);
        TEST_ASSERT(resUsb == acpi::TRUE && devResources[1].extendedIrq.gsi == 18, "XUSB GSI must be 18");
    }

    // ------------------------------------------------------------------------
    // Phase 5: PCI Interrupt Routing Table (_PRT) Evaluation & Device GSI Translation
    // ------------------------------------------------------------------------
    {
        auto prtVal = acpiSub.getNamespace().evaluate("\\_SB.PCI0._PRT");
        TEST_ASSERT(prtVal.type == acpi::AmlObjectType::Package, "_PRT evaluation must return a Package");
        TEST_ASSERT(prtVal.packageElements.size() == 5, "_PRT package must contain 5 device routing records");

        uint32_t gsi = 0;
        // GFX0: Device 1 (ADR 0x00010000), Pin 0 (INTA) -> GSI 16
        BOOL t1 = acpi::AcpiTranslatePciRouting(0x00010000, 0, &gsi);
        TEST_ASSERT(t1 == acpi::TRUE && gsi == 16, "PCI device 1 pin 0 must translate to GSI 16");

        // NVME: Device 2 (ADR 0x00020000), Pin 0 (INTA) -> GSI 17
        BOOL t2 = acpi::AcpiTranslatePciRouting(0x00020000, 0, &gsi);
        TEST_ASSERT(t2 == acpi::TRUE && gsi == 17, "PCI device 2 pin 0 must translate to GSI 17");

        // XUSB: Device 3 (ADR 0x00030000), Pin 0 (INTA) -> GSI 18
        BOOL t3 = acpi::AcpiTranslatePciRouting(0x00030000, 0, &gsi);
        TEST_ASSERT(t3 == acpi::TRUE && gsi == 18, "PCI device 3 pin 0 must translate to GSI 18");

        // RazzleNet: Device 4 (ADR 0x00040000), Pin 0 (INTA) -> GSI 19
        BOOL t4 = acpi::AcpiTranslatePciRouting(0x00040000, 0, &gsi);
        TEST_ASSERT(t4 == acpi::TRUE && gsi == 19, "PCI device 4 pin 0 must translate to GSI 19");

        // VirtIO: Device 5 (ADR 0x00050000), Pin 0 (INTA) -> GSI 20
        BOOL t5 = acpi::AcpiTranslatePciRouting(0x00050000, 0, &gsi);
        TEST_ASSERT(t5 == acpi::TRUE && gsi == 20, "PCI device 5 pin 0 must translate to GSI 20");

        // Unknown device -> failure
        BOOL t6 = acpi::AcpiTranslatePciRouting(0x00990000, 0, &gsi);
        TEST_ASSERT(t6 == acpi::FALSE, "Unknown device must fail translation");
    }

    // ------------------------------------------------------------------------
    // Phase 6: General Purpose Events (GPE 0/1) & System Control Interrupt (SCI) Dispatch
    // ------------------------------------------------------------------------
    {
        auto& gpeCtrl = acpiSub.getGpeController();

        static uint32_t s_gpeFired12 = 0;
        acpi::AcpiRegisterGpeHandler(12, [](uint32_t gpe) {
            s_gpeFired12 = gpe;
        });

        TEST_ASSERT(gpeCtrl.isGpePending(12) == false, "GPE 12 must not be pending initially");
        gpeCtrl.setGpeStatus(12);
        TEST_ASSERT(gpeCtrl.isGpePending(12) == true, "GPE 12 must be pending after setGpeStatus");

        BOOL disp1 = acpi::AcpiDispatchGpe(12);
        TEST_ASSERT(disp1 == acpi::TRUE, "AcpiDispatchGpe(12) must succeed");
        TEST_ASSERT(s_gpeFired12 == 12, "GPE 12 handler must receive GPE pin 12");
        TEST_ASSERT(gpeCtrl.isGpePending(12) == false, "Edge-triggered GPE 12 status must be cleared after dispatch");

        // GPE1 (Pin 45) test
        static uint32_t s_gpeFired45 = 0;
        acpi::AcpiRegisterGpeHandler(45, [](uint32_t gpe) {
            s_gpeFired45 = gpe;
        });
        gpeCtrl.setGpeStatus(45);
        uint32_t dispatchedPending = gpeCtrl.dispatchPendingGpes();
        TEST_ASSERT(dispatchedPending >= 1, "dispatchPendingGpes must dispatch at least 1 pending event");
        TEST_ASSERT(s_gpeFired45 == 45, "GPE 45 handler must be invoked");

        // Invalid GPE pin
        BOOL dispBad = acpi::AcpiDispatchGpe(100);
        TEST_ASSERT(dispBad == acpi::FALSE, "Out of range GPE 100 must fail");
    }

    // ------------------------------------------------------------------------
    // Phase 7: Device Power Management FSM (D0, D1, D2, D3hot/cold) via _PS0.._PS3
    // ------------------------------------------------------------------------
    {
        uint64_t psc = 99;
        acpi::AcpiEvaluateObject("\\_SB.PCI0.GFX0._PSC", &psc);
        TEST_ASSERT(psc == 0, "GFX0 initial device power state must be D0 (0)");

        // Transition to D1
        BOOL s1 = acpi::AcpiSetDevicePowerState("\\_SB.PCI0.GFX0", 1);
        TEST_ASSERT(s1 == acpi::TRUE, "SetDevicePowerState D1 must succeed");
        acpi::AcpiEvaluateObject("\\_SB.PCI0.GFX0._PSC", &psc);
        TEST_ASSERT(psc == 1, "GFX0 _PSC must report D1");
        TEST_ASSERT(acpiSub.getDevicePowerState("\\_SB.PCI0.GFX0") == acpi::AcpiDevicePowerState::D1, "Tracking state D1");

        // Transition to D2
        BOOL s2 = acpi::AcpiSetDevicePowerState("\\_SB.PCI0.GFX0", 2);
        TEST_ASSERT(s2 == acpi::TRUE, "SetDevicePowerState D2 must succeed");
        acpi::AcpiEvaluateObject("\\_SB.PCI0.GFX0._PSC", &psc);
        TEST_ASSERT(psc == 2, "GFX0 _PSC must report D2");

        // Transition to D3hot
        BOOL s3 = acpi::AcpiSetDevicePowerState("\\_SB.PCI0.GFX0", 3);
        TEST_ASSERT(s3 == acpi::TRUE, "SetDevicePowerState D3hot must succeed");
        acpi::AcpiEvaluateObject("\\_SB.PCI0.GFX0._PSC", &psc);
        TEST_ASSERT(psc == 3, "GFX0 _PSC must report D3hot");

        // Return to D0
        BOOL s0 = acpi::AcpiSetDevicePowerState("\\_SB.PCI0.GFX0", 0);
        TEST_ASSERT(s0 == acpi::TRUE, "SetDevicePowerState D0 must succeed");
        acpi::AcpiEvaluateObject("\\_SB.PCI0.GFX0._PSC", &psc);
        TEST_ASSERT(psc == 0, "GFX0 _PSC must report D0");

        // Invalid power state
        BOOL sBad = acpi::AcpiSetDevicePowerState("\\_SB.PCI0.GFX0", 8);
        TEST_ASSERT(sBad == acpi::FALSE, "Invalid device power state must fail");
    }

    // ------------------------------------------------------------------------
    // Phase 8: Platform Sleep State FSM (_PTS, _WAK, S0..S5 transitions)
    // ------------------------------------------------------------------------
    {
        uint8_t sleepState = 99;
        BOOL getPwr = acpi::AcpiGetSystemPowerState(&sleepState);
        TEST_ASSERT(getPwr == acpi::TRUE && sleepState == 0, "Initial platform sleep state must be S0_Working");

        // Inspect \_S0..\_S5 packages
        auto valS0 = acpiSub.getNamespace().evaluate("\\_S0");
        TEST_ASSERT(valS0.type == acpi::AmlObjectType::Package && valS0.packageElements[0].integerVal == 0, "\\_S0 match");

        auto valS3 = acpiSub.getNamespace().evaluate("\\_S3");
        TEST_ASSERT(valS3.type == acpi::AmlObjectType::Package && valS3.packageElements[0].integerVal == 3, "\\_S3 match");

        auto valS5 = acpiSub.getNamespace().evaluate("\\_S5");
        TEST_ASSERT(valS5.type == acpi::AmlObjectType::Package && valS5.packageElements[0].integerVal == 5, "\\_S5 match");

        // Transition platform to S3 (simulates prepare-to-sleep and wake)
        BOOL transS3 = acpi::AcpiSetSystemPowerState(3);
        TEST_ASSERT(transS3 == acpi::TRUE, "AcpiSetSystemPowerState(3) must succeed");
        acpi::AcpiGetSystemPowerState(&sleepState);
        TEST_ASSERT(sleepState == 0, "Platform must return to S0 after wake");

        // Transition platform to S4 (Hibernate)
        BOOL transS4 = acpi::AcpiSetSystemPowerState(4);
        TEST_ASSERT(transS4 == acpi::TRUE, "AcpiSetSystemPowerState(4) must succeed");

        // Invalid sleep state
        BOOL transBad = acpi::AcpiSetSystemPowerState(7);
        TEST_ASSERT(transBad == acpi::FALSE, "Invalid sleep state 7 must fail");
    }

    // ------------------------------------------------------------------------
    // Phase 9: Secondary System Description Table (SSDT) Dynamic Loading & Hotplug Unloading
    // ------------------------------------------------------------------------
    {
        std::vector<uint8_t> ssdtBytes(sizeof(acpi::AcpiTableHeader) + 16, 0);
        auto* hdr = reinterpret_cast<acpi::AcpiTableHeader*>(ssdtBytes.data());
        std::memcpy(hdr->signature, "SSDT", 4);
        hdr->length = static_cast<uint32_t>(ssdtBytes.size());
        hdr->revision = 2;
        std::memcpy(hdr->oemId, "MICANT", 6);
        std::memcpy(hdr->oemTableId, "HOTPLUG ", 8);
        hdr->oemRevision = 1;
        std::memcpy(hdr->aslCompilerId, "MNT ", 4);
        hdr->aslCompilerRev = 1;
        hdr->checksum = 0;
        hdr->checksum = static_cast<uint8_t>(0x100 - acpi::CalculateAcpiChecksum(ssdtBytes.data(), ssdtBytes.size()));

        uint32_t handle = 0;
        BOOL loadOk = acpi::AcpiLoadTable(ssdtBytes.data(), static_cast<uint32_t>(ssdtBytes.size()), &handle);
        TEST_ASSERT(loadOk == acpi::TRUE && handle > 0, "AcpiLoadTable must succeed and return non-zero handle");

        // Verify dynamic node under \_SB
        std::string hotpDev = "\\_SB.HOTP" + std::to_string(handle);
        auto devNode = acpiSub.getNamespace().resolvePath(hotpDev);
        TEST_ASSERT(devNode != nullptr, "Dynamic hotplug device node must exist in namespace");

        auto staVal = acpiSub.getNamespace().evaluate(hotpDev + "._STA");
        TEST_ASSERT(staVal.integerVal == 0x0F, "Hotplug device _STA must return 0x0F (Present and Enabled)");

        // Unload table
        BOOL unloadOk = acpi::AcpiUnloadTable(handle);
        TEST_ASSERT(unloadOk == acpi::TRUE, "AcpiUnloadTable must succeed");

        auto devNodeAfter = acpiSub.getNamespace().resolvePath(hotpDev);
        TEST_ASSERT(devNodeAfter == nullptr, "Dynamic hotplug device node must be removed after unload");

        // Double unload must fail
        BOOL unloadAgain = acpi::AcpiUnloadTable(handle);
        TEST_ASSERT(unloadAgain == acpi::FALSE, "Unloading already unloaded table must fail");
    }

    // ------------------------------------------------------------------------
    // Phase 10: Dynamic Loader Exports, SCM Services & ACPI Diagnostic Telemetry
    // ------------------------------------------------------------------------
    {
        // 1. JanusLDR Driver Exports Verification
        auto& ldr = ldr::DynamicLoader::get();
        TEST_ASSERT(ldr.getExport("acpi.sys", "AcpiFindTable") != nullptr, "acpi.sys AcpiFindTable must exist");
        TEST_ASSERT(ldr.getExport("acpi.sys", "AcpiEvaluateObject") != nullptr, "acpi.sys AcpiEvaluateObject must exist");
        TEST_ASSERT(ldr.getExport("acpi.sys", "AcpiGetSystemPowerState") != nullptr, "acpi.sys AcpiGetSystemPowerState must exist");
        TEST_ASSERT(ldr.getExport("acpi.sys", "AcpiSetSystemPowerState") != nullptr, "acpi.sys AcpiSetSystemPowerState must exist");
        TEST_ASSERT(ldr.getExport("acpi.sys", "AcpiGetThermalZoneTemp") != nullptr, "acpi.sys AcpiGetThermalZoneTemp must exist");
        TEST_ASSERT(ldr.getExport("acpi.sys", "AcpiGetBatteryStatus") != nullptr, "acpi.sys AcpiGetBatteryStatus must exist");
        TEST_ASSERT(ldr.getExport("acpi.sys", "AcpiGetProcessorCount") != nullptr, "acpi.sys AcpiGetProcessorCount must exist");
        TEST_ASSERT(ldr.getExport("acpi.sys", "AcpiRegisterGpeHandler") != nullptr, "acpi.sys AcpiRegisterGpeHandler must exist");
        TEST_ASSERT(ldr.getExport("acpi.sys", "AcpiDispatchGpe") != nullptr, "acpi.sys AcpiDispatchGpe must exist");
        TEST_ASSERT(ldr.getExport("acpi.sys", "AcpiGetDeviceResources") != nullptr, "acpi.sys AcpiGetDeviceResources must exist");
        TEST_ASSERT(ldr.getExport("acpi.sys", "AcpiTranslatePciRouting") != nullptr, "acpi.sys AcpiTranslatePciRouting must exist");
        TEST_ASSERT(ldr.getExport("acpi.sys", "AcpiSetDevicePowerState") != nullptr, "acpi.sys AcpiSetDevicePowerState must exist");
        TEST_ASSERT(ldr.getExport("acpi.sys", "AcpiLoadTable") != nullptr, "acpi.sys AcpiLoadTable must exist");
        TEST_ASSERT(ldr.getExport("acpi.sys", "AcpiUnloadTable") != nullptr, "acpi.sys AcpiUnloadTable must exist");
        TEST_ASSERT(ldr.getExport("acpi.sys", "AcpiGetTelemetry") != nullptr, "acpi.sys AcpiGetTelemetry must exist");

        // hal.dll exports
        TEST_ASSERT(ldr.getExport("hal.dll", "HalGetBusDataByOffset") != nullptr, "hal.dll HalGetBusDataByOffset must exist");
        TEST_ASSERT(ldr.getExport("hal.dll", "HalSetBusDataByOffset") != nullptr, "hal.dll HalSetBusDataByOffset must exist");

        // Test HAL Bus Data Access
        uint32_t pciBuf = 0;
        ULONG bytesRead = acpi::HalGetBusDataByOffset(PCIConfiguration, 0, (1 << 3), &pciBuf, 0, sizeof(uint32_t));
        TEST_ASSERT(bytesRead == sizeof(uint32_t), "HalGetBusDataByOffset must read 4 bytes");

        // 2. SCM Service Record Verification
        auto& scmMgr = scm::ServiceControlManager::get();
        auto acpiSvc = scmMgr.getServiceRecord(L"acpi");
        TEST_ASSERT(acpiSvc != nullptr, "acpi service record must exist in SCM");
        TEST_ASSERT(acpiSvc->serviceType == scm::SERVICE_KERNEL_DRIVER, "acpi must be a kernel driver");
        TEST_ASSERT(acpiSvc->startType == scm::SERVICE_BOOT_START, "acpi must be boot start");
        TEST_ASSERT(acpiSvc->status.dwCurrentState == scm::SERVICE_RUNNING, "acpi service state must be SERVICE_RUNNING");

        // 3. Version Database Verification
        auto& verDb = version::VersionDatabase::Instance();
        TEST_ASSERT(verDb.GetModuleInfo("acpi.sys") != nullptr, "acpi.sys must be registered in Version Database");

        // 4. ACPI Diagnostic Telemetry Verification
        acpi::ACPI_TELEMETRY telem{};
        BOOL telemOk = acpi::AcpiGetTelemetry(&telem);
        TEST_ASSERT(telemOk == acpi::TRUE, "AcpiGetTelemetry must succeed");
        TEST_ASSERT(telem.tablesDiscovered >= 10, "tablesDiscovered must be >= 10");
        TEST_ASSERT(telem.amlBytecodeOpsExecuted >= 20, "amlBytecodeOpsExecuted must be >= 20");
        TEST_ASSERT(telem.gpeEventsDispatched >= 2, "gpeEventsDispatched must be >= 2");
        TEST_ASSERT(telem.powerTransitions >= 2, "powerTransitions must be >= 2");
        TEST_ASSERT(telem.devicePowerTransitions >= 4, "devicePowerTransitions must be >= 4");
        TEST_ASSERT(telem.dynamicTablesLoaded >= 1, "dynamicTablesLoaded must be >= 1");
        TEST_ASSERT(telem.dynamicTablesUnloaded >= 1, "dynamicTablesUnloaded must be >= 1");
        TEST_ASSERT(telem.resourceDecodes >= 4, "resourceDecodes must be >= 4");
        TEST_ASSERT(telem.pciRoutingsResolved >= 5, "pciRoutingsResolved must be >= 5");

        acpi::AcpiResetTelemetryForTesting();
        acpi::ACPI_TELEMETRY telemReset = acpi::AcpiQueryTelemetry();
        TEST_ASSERT(telemReset.powerTransitions == 0, "powerTransitions must be 0 after reset");
    }

    std::cout << "[TEST] Suite 251: Horizon 35 - ACPI / HAL Device Tree & AML Interpreter PASSED.\n";
}

// ============================================================================
// Suite 252: Horizon 36 - Kernel-to-User Subsystem Bridge, Shadow SSDT & Win32k
// ============================================================================

void Test_Horizon36_KernelToUser_ShadowSSDT_And_Win32k_Subsystem() {
    std::cout << "[TEST] Running Suite 252: Horizon 36 - Kernel-to-User Subsystem Bridge, Shadow SSDT & Win32k Architecture...\n";

    // ------------------------------------------------------------------------
    // Phase 1: Win32k & Shadow SSDT Dispatcher Initialization & Table Verification
    // ------------------------------------------------------------------------
    {
        win32k::InitializeWin32kSubsystemExports();
        auto& win32kSub = win32k::TitanWin32kSubsystem::Instance();
        win32kSub.resetForTesting();

        auto& dispatcher = sys::SyscallDispatcher::get();
        TEST_ASSERT(dispatcher.hasShadowDispatcher(), "SyscallDispatcher must have a registered shadow dispatcher");

        // Verify standard syscalls (ssn < 0x1000) are handled by standard dispatcher
        sys::SyscallFrame ntFrame{
            .ssn = sys::SSN_NtYieldExecution,
            .arg1 = 0, .arg2 = 0, .arg3 = 0, .arg4 = 0,
            .stackArgs = nullptr, .stackArgCount = 0
        };
        NtStatus ntStatus = dispatcher.dispatch(ntFrame);
        TEST_ASSERT(ntStatus == NtStatus::Success, "Standard NT syscall must dispatch successfully");

        // Verify shadow syscalls (ssn >= 0x1000) route to Shadow SSDT
        sys::SyscallFrame shadowGetDcFrame{
            .ssn = win32k::SSN_NTUSER_GETDC,
            .arg1 = 0, .arg2 = 0, .arg3 = 0, .arg4 = 0,
            .stackArgs = nullptr, .stackArgCount = 0
        };
        NtStatus shadowStatus = dispatcher.dispatch(shadowGetDcFrame);
        TEST_ASSERT(shadowStatus == NtStatus::Success, "Shadow SSDT SSN_NTUSER_GETDC must succeed");

        // Out-of-range / unhandled shadow syscall
        sys::SyscallFrame invalidShadowFrame{
            .ssn = 0x1FFF,
            .arg1 = 0, .arg2 = 0, .arg3 = 0, .arg4 = 0,
            .stackArgs = nullptr, .stackArgCount = 0
        };
        NtStatus invStatus = dispatcher.dispatch(invalidShadowFrame);
        TEST_ASSERT(invStatus == NtStatus::InvalidDeviceRequest, "Unhandled shadow syscall must return InvalidDeviceRequest");
    }

    // ------------------------------------------------------------------------
    // Phase 2: WindowStation (WinSta0) & Desktop (Default, Winlogon) Object Hierarchy
    // ------------------------------------------------------------------------
    {
        auto& win32kSub = win32k::TitanWin32kSubsystem::Instance();

        // 1. Interactive WindowStation WinSta0
        win32k::HWINSTA hWinSta0 = win32kSub.openWindowStation(L"WinSta0");
        TEST_ASSERT(hWinSta0 != nullptr, "WinSta0 interactive WindowStation must exist");
        TEST_ASSERT(win32kSub.getProcessWindowStation() == hWinSta0, "Current process WindowStation must be WinSta0");

        // 2. Default Desktops on WinSta0
        win32k::HDESK hDeskDefault = win32kSub.openDesktop(L"Default");
        win32k::HDESK hDeskWinlogon = win32kSub.openDesktop(L"Winlogon");
        win32k::HDESK hDeskScreenSaver = win32kSub.openDesktop(L"Screen-saver");
        TEST_ASSERT(hDeskDefault != nullptr, "Default desktop must exist");
        TEST_ASSERT(hDeskWinlogon != nullptr, "Winlogon desktop must exist");
        TEST_ASSERT(hDeskScreenSaver != nullptr, "Screen-saver desktop must exist");

        // 3. Desktop Switching FSM
        auto curDesk = win32kSub.getCurrentDesktop();
        TEST_ASSERT(curDesk != nullptr && curDesk->getName() == L"Default", "Initial desktop must be Default");

        BOOL swOk = win32k::NtUserSwitchDesktop(hDeskWinlogon);
        TEST_ASSERT(swOk == win32k::TRUE_VAL, "Switching to Winlogon desktop must succeed");
        TEST_ASSERT(win32kSub.getCurrentDesktop()->getName() == L"Winlogon", "Active desktop must now be Winlogon");

        // Switch back to Default
        win32k::NtUserSwitchDesktop(hDeskDefault);
        TEST_ASSERT(win32kSub.getCurrentDesktop()->getName() == L"Default", "Active desktop must return to Default");

        // 4. Custom Service WindowStation
        win32k::HWINSTA hSvcSta = win32k::NtUserCreateWindowStation(L"Service-0x0-3e7$", 0, nullptr);
        TEST_ASSERT(hSvcSta != nullptr, "Service WindowStation creation must succeed");

        BOOL setWsOk = win32k::NtUserSetProcessWindowStation(hSvcSta);
        TEST_ASSERT(setWsOk == win32k::TRUE_VAL, "Setting process WindowStation must succeed");
        TEST_ASSERT(win32k::NtUserGetProcessWindowStation() == hSvcSta, "Process WindowStation must match");

        // Restore WinSta0
        win32k::NtUserSetProcessWindowStation(hWinSta0);
        TEST_ASSERT(win32k::NtUserGetProcessWindowStation() == hWinSta0, "Process WindowStation must be WinSta0");
    }

    // ------------------------------------------------------------------------
    // Phase 3: Desktop Heap Allocation, Dual Mapping & Client Read-Only Views
    // ------------------------------------------------------------------------
    {
        auto& win32kSub = win32k::TitanWin32kSubsystem::Instance();
        auto desk = win32kSub.getCurrentDesktop();
        TEST_ASSERT(desk != nullptr, "Current desktop must exist");

        auto heap = desk->getHeap();
        TEST_ASSERT(heap != nullptr, "Desktop heap must be allocated");
        TEST_ASSERT(heap->getHeapSize() == 8 * 1024 * 1024, "Desktop heap size must default to 8MB");
        TEST_ASSERT(heap->getClientBaseAddress() != nullptr, "Desktop heap client base address must be valid");

        // Create a window to allocate CLIENT_WND_INFO in Desktop Heap
        win32k::HWND hWndTest = win32k::NtUserCreateWindowEx(
            win32k::WS_EX_APPWINDOW, L"TestWndClass", L"DesktopHeapWindow",
            win32k::WS_OVERLAPPEDWINDOW | win32k::WS_VISIBLE,
            50, 60, 640, 480, nullptr, nullptr, nullptr, nullptr);
        TEST_ASSERT(hWndTest != nullptr, "CreateWindowEx must succeed");

        uint64_t heapOffset = heap->getOffsetForHwnd(hWndTest);
        TEST_ASSERT(heapOffset > 0, "Window must have an allocated desktop heap offset");
        TEST_ASSERT((heapOffset & 0x0F) == 0, "Desktop heap allocation must be 16-byte aligned");

        // Direct read-only client view inspection (zero syscall overhead)
        const win32k::CLIENT_WND_INFO* clientInfo = heap->getClientWnd(heapOffset);
        TEST_ASSERT(clientInfo != nullptr, "Client window info pointer must be valid");
        TEST_ASSERT(clientInfo->hwnd == hWndTest, "Client info HWND must match");
        TEST_ASSERT(clientInfo->rcWindow.left == 50 && clientInfo->rcWindow.top == 60, "Client info window position match");
        TEST_ASSERT(clientInfo->rcWindow.width() == 640 && clientInfo->rcWindow.height() == 480, "Client info window dimensions match");
        TEST_ASSERT(clientInfo->isVisible == true, "Client info visibility must match WS_VISIBLE");

        // Update window via ShowWindow(SW_HIDE) and verify client view updates in-place
        win32k::NtUserShowWindow(hWndTest, win32k::SW_HIDE);
        TEST_ASSERT(clientInfo->isVisible == false, "Client view visibility must reflect SW_HIDE with zero syscall read overhead");

        // Update window position
        win32k::NtUserSetWindowPos(hWndTest, nullptr, 100, 120, 800, 600, win32k::SWP_NOZORDER | win32k::SWP_SHOWWINDOW);
        TEST_ASSERT(clientInfo->isVisible == true, "Client view must reflect SWP_SHOWWINDOW");
        TEST_ASSERT(clientInfo->rcWindow.left == 100 && clientInfo->rcWindow.top == 120, "Client view must reflect updated position");
        TEST_ASSERT(clientInfo->rcWindow.width() == 800 && clientInfo->rcWindow.height() == 600, "Client view must reflect updated dimensions");

        win32k::NtUserDestroyWindow(hWndTest);
        TEST_ASSERT(heap->getOffsetForHwnd(hWndTest) == 0, "Desktop heap block must be released upon window destruction");
    }

    // ------------------------------------------------------------------------
    // Phase 4: Window Creation (WND), Parent-Child Z-Ordering & Rectangle Geometry
    // ------------------------------------------------------------------------
    {
        auto& win32kSub = win32k::TitanWin32kSubsystem::Instance();

        // 1. Top-Level Main Window
        win32k::HWND hWndMain = win32k::NtUserCreateWindowEx(
            0, L"MainWindowClass", L"Main Application",
            win32k::WS_OVERLAPPEDWINDOW | win32k::WS_VISIBLE,
            0, 0, 1024, 768, nullptr, nullptr, nullptr, nullptr);
        TEST_ASSERT(hWndMain != nullptr, "Main window creation must succeed");

        auto wndMain = win32kSub.getWindow(hWndMain);
        TEST_ASSERT(wndMain != nullptr, "Window structure must be retrievable from kernel");
        TEST_ASSERT(wndMain->rcWindow.width() == 1024 && wndMain->rcWindow.height() == 768, "Main window dimensions");

        // 2. Child Window
        win32k::HWND hWndChild = win32k::NtUserCreateWindowEx(
            0, L"ChildWindowClass", L"Child View",
            win32k::WS_CHILD | win32k::WS_VISIBLE,
            10, 10, 300, 200, hWndMain, nullptr, nullptr, nullptr);
        TEST_ASSERT(hWndChild != nullptr, "Child window creation must succeed");

        auto wndChild = win32kSub.getWindow(hWndChild);
        TEST_ASSERT(wndChild != nullptr && wndChild->hwndParent == hWndMain, "Child window parent must be hWndMain");
        TEST_ASSERT(wndMain->children.size() == 1 && wndMain->children[0] == hWndChild, "Parent children list must contain child");

        // 3. Topmost Window & Z-Order Management
        win32k::HWND hWndTopmost = win32k::NtUserCreateWindowEx(
            win32k::WS_EX_TOPMOST, L"TopmostClass", L"Overlay HUD",
            win32k::WS_POPUP | win32k::WS_VISIBLE,
            200, 200, 400, 300, nullptr, nullptr, nullptr, nullptr);
        TEST_ASSERT(hWndTopmost != nullptr, "Topmost window creation must succeed");

        auto desk = win32kSub.getCurrentDesktop();
        auto zOrder = desk->getZOrder();
        TEST_ASSERT(!zOrder.empty() && zOrder[0] == hWndTopmost, "Topmost window must be at front of Z-order");

        // Bring main window to top
        win32k::NtUserSetWindowPos(hWndMain, win32k::HWND_TOP, 0, 0, 0, 0, win32k::SWP_NOMOVE | win32k::SWP_NOSIZE);
        auto zOrderAfter = desk->getZOrder();
        TEST_ASSERT(zOrderAfter[0] == hWndMain, "Main window must now be at front of Z-order");

        // Clean up
        win32k::NtUserDestroyWindow(hWndChild);
        win32k::NtUserDestroyWindow(hWndMain);
        win32k::NtUserDestroyWindow(hWndTopmost);
    }

    // ------------------------------------------------------------------------
    // Phase 5: Thread Message Queue, Priority Scheduling & Message Pump
    // ------------------------------------------------------------------------
    {
        auto& win32kSub = win32k::TitanWin32kSubsystem::Instance();
        uint32_t tid = 404;

        win32k::HWND hWndPump = win32kSub.createWindowEx(
            0, L"PumpClass", L"Pump Window",
            win32k::WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, nullptr, nullptr,
            tid, 4);
        TEST_ASSERT(hWndPump != nullptr, "Pump window creation must succeed");

        auto queue = win32kSub.getOrCreateThreadQueue(tid);
        TEST_ASSERT(queue != nullptr, "Thread message queue must be created for GUI thread");
        TEST_ASSERT(queue->getThreadId() == tid, "Queue TID must match");

        // 1. Post FIFO Messages via NtUserPostMessage
        win32k::NtUserPostMessage(hWndPump, win32k::WM_USER + 1, 100, 200);
        win32k::NtUserPostMessage(hWndPump, win32k::WM_USER + 2, 300, 400);
        win32k::NtUserPostMessage(hWndPump, win32k::WM_USER + 3, 500, 600);
        TEST_ASSERT(queue->getPostedCount() == 3, "Posted queue must have 3 messages");
        TEST_ASSERT((queue->getWakeMask() & win32k::QS_POSTMESSAGE) != 0, "Wake mask must include QS_POSTMESSAGE");

        // 2. PeekMessage without removal
        win32k::MSG peekMsg{};
        BOOL peekRes = queue->peekMessage(peekMsg, nullptr, 0, 0, win32k::PM_NOREMOVE);
        TEST_ASSERT(peekRes == true, "PeekMessage PM_NOREMOVE must find message");
        TEST_ASSERT(peekMsg.message == win32k::WM_USER + 1, "Peeked message must be first posted message");
        TEST_ASSERT(queue->getPostedCount() == 3, "Count must remain 3 after PM_NOREMOVE");

        // 3. GetMessage FIFO Extraction
        win32k::MSG getMsg{};
        BOOL g1 = queue->getMessage(getMsg, nullptr, 0, 0);
        TEST_ASSERT(g1 == win32k::TRUE_VAL && getMsg.message == win32k::WM_USER + 1, "First message must be WM_USER + 1");

        BOOL g2 = queue->getMessage(getMsg, nullptr, 0, 0);
        TEST_ASSERT(g2 == win32k::TRUE_VAL && getMsg.message == win32k::WM_USER + 2, "Second message must be WM_USER + 2");

        BOOL g3 = queue->getMessage(getMsg, nullptr, 0, 0);
        TEST_ASSERT(g3 == win32k::TRUE_VAL && getMsg.message == win32k::WM_USER + 3, "Third message must be WM_USER + 3");
        TEST_ASSERT(queue->getPostedCount() == 0, "Posted queue must be empty after all retrieved");

        // 4. Timers and Timer Messages
        queue->setTimer(nullptr, 77, 0, nullptr); // 0ms interval triggers immediately
        TEST_ASSERT(queue->getTimerCount() == 1, "Timer count must be 1");
        TEST_ASSERT((queue->getWakeMask() & win32k::QS_TIMER) != 0, "Wake mask must include QS_TIMER");

        BOOL gTmr = queue->getMessage(getMsg, nullptr, 0, 0);
        TEST_ASSERT(gTmr == win32k::TRUE_VAL && getMsg.message == win32k::WM_TIMER && getMsg.wParam == 77, "WM_TIMER must be generated");

        queue->killTimer(nullptr, 77);
        TEST_ASSERT(queue->getTimerCount() == 0, "Timer count must be 0 after killTimer");

        // 5. WM_QUIT Message Pump Termination
        win32k::NtUserPostMessage(hWndPump, win32k::WM_QUIT, 0, 0);
        BOOL gQuit = queue->getMessage(getMsg, nullptr, 0, 0);
        TEST_ASSERT(gQuit == win32k::FALSE_VAL && getMsg.message == win32k::WM_QUIT, "WM_QUIT must return FALSE_VAL to exit message pump");

        win32k::NtUserDestroyWindow(hWndPump);
    }

    // ------------------------------------------------------------------------
    // Phase 6: Inter-Thread Synchronous Messaging (SendMessage) & Wait Completion
    // ------------------------------------------------------------------------
    {
        auto& win32kSub = win32k::TitanWin32kSubsystem::Instance();

        // Create target window owned by TID 200
        win32k::HWND hWndTarget = win32kSub.createWindowEx(
            0, L"TargetClass", L"Target Window",
            win32k::WS_OVERLAPPEDWINDOW, 0, 0, 200, 200, nullptr, nullptr, nullptr, nullptr,
            200, 4); // TID 200
        TEST_ASSERT(hWndTarget != nullptr, "Target window creation must succeed");

        // Thread 1 sends synchronous message to Thread 2
        win32k::LRESULT result = win32kSub.sendMessage(hWndTarget, win32k::WM_COMMAND, 101, 202, 1);
        TEST_ASSERT(result == 1, "Synchronous inter-thread SendMessage must return completion result 1");

        win32k::NtUserDestroyWindow(hWndTarget);
    }

    // ------------------------------------------------------------------------
    // Phase 7: Kernel-to-User Trampoline Engine (KeUserModeCallback & NtCallbackReturn)
    // ------------------------------------------------------------------------
    {
        auto& win32kSub = win32k::TitanWin32kSubsystem::Instance();
        auto& cbEngine = win32kSub.getCallbacks();

        // 1. Hook Mock User-Mode Callback Dispatcher (simulates user32.dll!__ClientCallServer)
        static uint32_t s_lastApiIndex = 0;
        static std::vector<uint8_t> s_lastPayload;

        cbEngine.registerUserModeDispatcher([](uint32_t apiIndex, const void* args, uint32_t length) -> win32k::LRESULT {
            s_lastApiIndex = apiIndex;
            s_lastPayload.clear();
            if (args && length > 0) {
                const auto* p = reinterpret_cast<const uint8_t*>(args);
                s_lastPayload.assign(p, p + length);
            }
            // Return handled code 0x1337
            return 0x1337;
        });

        TEST_ASSERT(cbEngine.hasDispatcher(), "UserModeCallbackEngine must have registered dispatcher");

        // 2. Invoke Trampoline from Kernel
        const char testData[] = "MicaNT_Win32k_Callback_Packet";
        void* outBuf = nullptr;
        uint32_t outLen = 0;
        win32k::LRESULT userResult = 0;

        NtStatus cbStatus = cbEngine.invokeUserModeCallback(
            0x42, testData, sizeof(testData), &outBuf, &outLen, &userResult);

        TEST_ASSERT(cbStatus == NtStatus::Success, "invokeUserModeCallback must return Success");
        TEST_ASSERT(s_lastApiIndex == 0x42, "User dispatcher must receive API index 0x42");
        TEST_ASSERT(!s_lastPayload.empty() && std::memcmp(s_lastPayload.data(), testData, sizeof(testData)) == 0, "Payload data must match");
        TEST_ASSERT(userResult == 0x1337, "UserMode result must be 0x1337");

        // 3. Test KeUserModeCallback Driver Export
        NtStatus kStatus = win32k::KeUserModeCallback(0x55, testData, sizeof(testData), &outBuf, &outLen);
        TEST_ASSERT(kStatus == NtStatus::Success, "KeUserModeCallback export must return Success");
        TEST_ASSERT(s_lastApiIndex == 0x55, "User dispatcher must receive API index 0x55");
    }

    // ------------------------------------------------------------------------
    // Phase 8: GDI Engine: Memory DCs, Compatible Bitmaps & BitBlt Raster Operations
    // ------------------------------------------------------------------------
    {
        auto& win32kSub = win32k::TitanWin32kSubsystem::Instance();
        auto& gdi = win32kSub.getGdi();

        // 1. Screen DC & Dimensions
        win32k::HDC hScreenDC = win32k::NtUserGetDC(nullptr);
        TEST_ASSERT(hScreenDC != nullptr, "Screen DC must be retrievable");

        // 2. Memory DC Creation
        win32k::HDC hMemDCDest = win32k::NtGdiCreateCompatibleDC(hScreenDC);
        win32k::HDC hMemDCSrc = win32k::NtGdiCreateCompatibleDC(hScreenDC);
        TEST_ASSERT(hMemDCDest != nullptr && hMemDCSrc != nullptr, "CreateCompatibleDC must succeed for dest and src");

        // 3. Compatible Bitmaps
        win32k::HBITMAP hBmpDest = win32k::NtGdiCreateCompatibleBitmap(hScreenDC, 32, 32);
        win32k::HBITMAP hBmpSrc = win32k::NtGdiCreateCompatibleBitmap(hScreenDC, 32, 32);
        TEST_ASSERT(hBmpDest != nullptr && hBmpSrc != nullptr, "CreateCompatibleBitmap must succeed");

        win32k::NtGdiSelectBitmap(hMemDCDest, hBmpDest);
        win32k::NtGdiSelectBitmap(hMemDCSrc, hBmpSrc);

        // Populate Source Bitmap with color 0x00FF0000 (Pure Red)
        auto bmpSrcObj = gdi.getBitmap(hBmpSrc);
        TEST_ASSERT(bmpSrcObj != nullptr, "Source bitmap object must be accessible");
        bmpSrcObj->clear(0x00FF0000);

        // 4. BitBlt SRCCOPY
        BOOL bltCopy = win32k::NtGdiBitBlt(hMemDCDest, 0, 0, 32, 32, hMemDCSrc, 0, 0, win32k::SRCCOPY);
        TEST_ASSERT(bltCopy == win32k::TRUE_VAL, "BitBlt SRCCOPY must succeed");
        auto bmpDestObj = gdi.getBitmap(hBmpDest);
        TEST_ASSERT(bmpDestObj->getPixel(0, 0) == 0x00FF0000, "Dest pixel must match source pixel (0x00FF0000) after SRCCOPY");

        // 5. BitBlt BLACKNESS
        BOOL bltBlack = win32k::NtGdiBitBlt(hMemDCDest, 0, 0, 32, 32, nullptr, 0, 0, win32k::BLACKNESS);
        TEST_ASSERT(bltBlack == win32k::TRUE_VAL, "BitBlt BLACKNESS must succeed");
        TEST_ASSERT(bmpDestObj->getPixel(15, 15) == 0x00000000, "Dest pixel must be 0x00000000 after BLACKNESS");

        // 6. BitBlt WHITENESS
        BOOL bltWhite = win32k::NtGdiBitBlt(hMemDCDest, 0, 0, 32, 32, nullptr, 0, 0, win32k::WHITENESS);
        TEST_ASSERT(bltWhite == win32k::TRUE_VAL, "BitBlt WHITENESS must succeed");
        TEST_ASSERT(bmpDestObj->getPixel(15, 15) == 0x00FFFFFF, "Dest pixel must be 0x00FFFFFF after WHITENESS");

        // 7. BitBlt SRCINVERT
        bmpSrcObj->clear(0x0000FFFF); // Cyan
        BOOL bltInv = win32k::NtGdiBitBlt(hMemDCDest, 0, 0, 32, 32, hMemDCSrc, 0, 0, win32k::SRCINVERT);
        TEST_ASSERT(bltInv == win32k::TRUE_VAL, "BitBlt SRCINVERT must succeed");
        // 0x00FFFFFF ^ 0x0000FFFF = 0x00FF0000
        TEST_ASSERT((bmpDestObj->getPixel(15, 15) & 0x00FFFFFF) == 0x00FF0000, "Dest pixel must be inverted correctly");

        // 8. Stock Objects Verification
        void* pWhiteBrush = win32k::NtGdiGetStockObject(win32k::WHITE_BRUSH);
        void* pBlackBrush = win32k::NtGdiGetStockObject(win32k::BLACK_BRUSH);
        void* pSysFont = win32k::NtGdiGetStockObject(win32k::SYSTEM_FONT);
        TEST_ASSERT(pWhiteBrush != nullptr, "WHITE_BRUSH must exist");
        TEST_ASSERT(pBlackBrush != nullptr, "BLACK_BRUSH must exist");
        TEST_ASSERT(pSysFont != nullptr, "SYSTEM_FONT must exist");

        // Clean up
        win32k::NtGdiDeleteObjectApp(hBmpDest);
        win32k::NtGdiDeleteObjectApp(hBmpSrc);
        win32k::NtUserReleaseDC(nullptr, hMemDCDest);
        win32k::NtUserReleaseDC(nullptr, hMemDCSrc);
    }

    // ------------------------------------------------------------------------
    // Phase 9: WDDM Integration: Blitting to Primary Display Surface
    // ------------------------------------------------------------------------
    {
        auto& win32kSub = win32k::TitanWin32kSubsystem::Instance();
        auto& wddmSub = wddm::TitanWddmSubsystem::Instance();
        wddmSub.initialize();

        auto primaryAdapter = wddmSub.getPrimaryAdapter();
        TEST_ASSERT(primaryAdapter != nullptr, "WDDM Primary Adapter must be available");

        // Create a memory DC with 1920x1080 surface
        win32k::HDC hScreen = win32k::NtUserGetDC(nullptr);
        win32k::HDC hSrcDC = win32k::NtGdiCreateCompatibleDC(hScreen);
        win32k::HBITMAP hBmp = win32k::NtGdiCreateCompatibleBitmap(hScreen, 1920, 1080);
        win32k::NtGdiSelectBitmap(hSrcDC, hBmp);

        auto bmp = win32kSub.getGdi().getBitmap(hBmp);
        TEST_ASSERT(bmp != nullptr, "Bitmap must be retrieved");
        bmp->clear(0x002A2A2A);

        // Blit GDI surface directly into WDDM Primary
        bool bltPrimaryOk = win32kSub.blitToWddmPrimary(hSrcDC, 0, 0, 1920, 1080);
        TEST_ASSERT(bltPrimaryOk == true, "blitToWddmPrimary must succeed");

        win32k::NtGdiDeleteObjectApp(hBmp);
        win32k::NtUserReleaseDC(nullptr, hSrcDC);
    }

    // ------------------------------------------------------------------------
    // Phase 10: Dynamic Loader Exports, SCM Service Registration & Win32k Diagnostic Telemetry
    // ------------------------------------------------------------------------
    {
        // 1. JanusLDR Driver Exports Verification
        auto& ldr = ldr::DynamicLoader::get();
        TEST_ASSERT(ldr.getExport("win32k.sys", "NtUserCreateWindowEx") != nullptr, "win32k.sys NtUserCreateWindowEx must exist");
        TEST_ASSERT(ldr.getExport("win32k.sys", "NtUserDestroyWindow") != nullptr, "win32k.sys NtUserDestroyWindow must exist");
        TEST_ASSERT(ldr.getExport("win32k.sys", "NtUserShowWindow") != nullptr, "win32k.sys NtUserShowWindow must exist");
        TEST_ASSERT(ldr.getExport("win32k.sys", "NtUserSetWindowPos") != nullptr, "win32k.sys NtUserSetWindowPos must exist");
        TEST_ASSERT(ldr.getExport("win32k.sys", "NtUserPostMessage") != nullptr, "win32k.sys NtUserPostMessage must exist");
        TEST_ASSERT(ldr.getExport("win32k.sys", "NtUserSendMessage") != nullptr, "win32k.sys NtUserSendMessage must exist");
        TEST_ASSERT(ldr.getExport("win32k.sys", "NtUserGetMessage") != nullptr, "win32k.sys NtUserGetMessage must exist");
        TEST_ASSERT(ldr.getExport("win32k.sys", "NtUserPeekMessage") != nullptr, "win32k.sys NtUserPeekMessage must exist");
        TEST_ASSERT(ldr.getExport("win32k.sys", "NtUserCreateWindowStation") != nullptr, "win32k.sys NtUserCreateWindowStation must exist");
        TEST_ASSERT(ldr.getExport("win32k.sys", "NtUserCreateDesktop") != nullptr, "win32k.sys NtUserCreateDesktop must exist");
        TEST_ASSERT(ldr.getExport("win32k.sys", "NtUserGetDC") != nullptr, "win32k.sys NtUserGetDC must exist");
        TEST_ASSERT(ldr.getExport("win32k.sys", "NtGdiCreateCompatibleDC") != nullptr, "win32k.sys NtGdiCreateCompatibleDC must exist");
        TEST_ASSERT(ldr.getExport("win32k.sys", "NtGdiCreateCompatibleBitmap") != nullptr, "win32k.sys NtGdiCreateCompatibleBitmap must exist");
        TEST_ASSERT(ldr.getExport("win32k.sys", "NtGdiBitBlt") != nullptr, "win32k.sys NtGdiBitBlt must exist");
        TEST_ASSERT(ldr.getExport("win32k.sys", "KeUserModeCallback") != nullptr, "win32k.sys KeUserModeCallback must exist");
        TEST_ASSERT(ldr.getExport("win32k.sys", "NtCallbackReturn") != nullptr, "win32k.sys NtCallbackReturn must exist");

        // Split drivers: win32kbase.sys and win32kfull.sys
        TEST_ASSERT(ldr.getExport("win32kbase.sys", "NtUserCreateWindowEx") != nullptr, "win32kbase.sys NtUserCreateWindowEx must exist");
        TEST_ASSERT(ldr.getExport("win32kfull.sys", "NtGdiBitBlt") != nullptr, "win32kfull.sys NtGdiBitBlt must exist");

        // 2. SCM Service Record Verification
        auto& scmMgr = scm::ServiceControlManager::get();
        auto win32kSvc = scmMgr.getServiceRecord(L"win32k");
        TEST_ASSERT(win32kSvc != nullptr, "win32k service record must exist in SCM");
        TEST_ASSERT(win32kSvc->serviceType == scm::SERVICE_KERNEL_DRIVER, "win32k must be a kernel driver");
        TEST_ASSERT(win32kSvc->startType == scm::SERVICE_SYSTEM_START, "win32k must be system start");

        // 3. Version Database Verification
        auto& verDb = version::VersionDatabase::Instance();
        TEST_ASSERT(verDb.GetModuleInfo("win32k.sys") != nullptr, "win32k.sys must be registered in Version Database");
        TEST_ASSERT(verDb.GetModuleInfo("win32kbase.sys") != nullptr, "win32kbase.sys must be registered in Version Database");
        TEST_ASSERT(verDb.GetModuleInfo("win32kfull.sys") != nullptr, "win32kfull.sys must be registered in Version Database");

        // 4. Win32k Diagnostic Telemetry Verification
        win32k::WIN32K_TELEMETRY telem{};
        BOOL telemOk = win32k::W32kGetTelemetry(&telem);
        TEST_ASSERT(telemOk == win32k::TRUE_VAL, "W32kGetTelemetry must succeed");
        TEST_ASSERT(telem.totalSyscallsHandled >= 1, "totalSyscallsHandled must be >= 1");
        TEST_ASSERT(telem.totalWindowsCreated >= 3, "totalWindowsCreated must be >= 3");
        TEST_ASSERT(telem.totalWindowsDestroyed >= 1, "totalWindowsDestroyed must be >= 1");
        TEST_ASSERT(telem.totalMessagesPosted >= 4, "totalMessagesPosted must be >= 4");
        TEST_ASSERT(telem.totalMessagesSent >= 1, "totalMessagesSent must be >= 1");
        TEST_ASSERT(telem.totalCallbacksInvoked >= 1, "totalCallbacksInvoked must be >= 1");
        TEST_ASSERT(telem.totalBitBltsExecuted >= 4, "totalBitBltsExecuted must be >= 4");
        TEST_ASSERT(telem.activeDesktopsCount >= 3, "activeDesktopsCount must be >= 3");
        TEST_ASSERT(telem.activeWindowStationsCount >= 1, "activeWindowStationsCount must be >= 1");

        win32k::WIN32K_TELEMETRY directTelem = win32k::W32kQueryTelemetry();
        TEST_ASSERT(directTelem.totalBitBltsExecuted == telem.totalBitBltsExecuted, "Direct C++ telemetry query must match");
    }

    std::cout << "[TEST] Suite 252: Horizon 36 - Kernel-to-User Subsystem Bridge, Shadow SSDT & Win32k Architecture PASSED.\n";
}

// ============================================================================
// Suite 253: Horizon 37 - Ring 1 Privileged Service Domain, Fault-Isolated
//                         Driver Shims & Hardware Callgate Architecture
// ============================================================================

void Test_Horizon37_Ring1_PrivilegedServiceDomain_And_FaultIsolation() {
    std::cout << "[TEST] Running Suite 253: Horizon 37 - Ring 1 Privileged Service Domain & Fault Isolation...\n";

    // ------------------------------------------------------------------------
    // Phase 1: GDT Ring 1 Segment Descriptors, DPL/RPL Calculation & TSS RSP1 Stack Verification
    // ------------------------------------------------------------------------
    {
        ring1::InitializeRing1SubsystemExports();
        auto& r1Sub = ring1::TitanRing1Subsystem::Instance();
        r1Sub.resetForTesting();

        // 1. Selector RPL verification: lower 2 bits must be 0b01 (RPL=1)
        TEST_ASSERT((ring1::GDT_SELECTOR_RING1_CODE64 & 0x03) == 1, "GDT_SELECTOR_RING1_CODE64 RPL must be 1");
        TEST_ASSERT((ring1::GDT_SELECTOR_RING1_DATA64 & 0x03) == 1, "GDT_SELECTOR_RING1_DATA64 RPL must be 1");

        // Ring 0 and Ring 3 for comparison
        TEST_ASSERT((ring1::GDT_SELECTOR_RING0_CODE64 & 0x03) == 0, "GDT_SELECTOR_RING0_CODE64 RPL must be 0");
        TEST_ASSERT((ring1::GDT_SELECTOR_RING3_CODE64 & 0x03) == 3, "GDT_SELECTOR_RING3_CODE64 RPL must be 3");

        // 2. Task State Segment (TSS64) Stack Configuration
        const auto& tss = r1Sub.getTss();
        TEST_ASSERT(tss.rsp0 == 0xFFFF800000100000ULL, "TSS RSP0 must point to Ring 0 stack");
        TEST_ASSERT(tss.rsp1 == 0xFFFF800000200000ULL, "TSS RSP1 must point to isolated Ring 1 stack");
        TEST_ASSERT(tss.rsp1 != tss.rsp0, "Ring 1 stack must be physically isolated from Ring 0 stack");
        TEST_ASSERT(tss.ioMapBase == 104, "TSS I/O Map Base must be 104 bytes");
    }

    // ------------------------------------------------------------------------
    // Phase 2: 64-bit Hardware Call Gate Configuration & Privilege Boundary Validation
    // ------------------------------------------------------------------------
    {
        auto& r1Sub = ring1::TitanRing1Subsystem::Instance();

        // Gate 1: Ring 3 User to Ring 1 Driver Shim (DPL = 3)
        const auto* gate1 = r1Sub.getCallGate(1);
        TEST_ASSERT(gate1 != nullptr, "Call Gate 1 must exist");
        TEST_ASSERT(gate1->isPresent(), "Call Gate 1 must be Present");
        TEST_ASSERT(gate1->getDpl() == 3, "Call Gate 1 DPL must be 3 (accessible from Ring 3)");
        TEST_ASSERT(gate1->targetSelector == ring1::GDT_SELECTOR_RING1_CODE64, "Call Gate 1 target selector must be Ring 1 Code");

        // Gate 0: Ring 1 to Ring 0 Kernel (DPL = 1)
        const auto* gate0 = r1Sub.getCallGate(0);
        TEST_ASSERT(gate0 != nullptr, "Call Gate 0 must exist");
        TEST_ASSERT(gate0->isPresent(), "Call Gate 0 must be Present");
        TEST_ASSERT(gate0->getDpl() == 1, "Call Gate 0 DPL must be 1 (accessible from Ring 1 only)");
        TEST_ASSERT(gate0->targetSelector == ring1::GDT_SELECTOR_RING0_CODE64, "Call Gate 0 target selector must be Ring 0 Code");

        // 1. Invoke Gate 1 from Ring 3 (User) -> Should SUCCEED
        uint64_t targetRip = 0, newRsp = 0;
        uint16_t targetCs = 0;
        NtStatus st1 = r1Sub.invokeCallGate(1, ring1::CpuPrivilegeRing::Ring3_User, &targetRip, &targetCs, &newRsp);
        TEST_ASSERT(st1 == NtStatus::Success, "Ring 3 caller must successfully traverse Gate 1 into Ring 1");
        TEST_ASSERT((targetCs & 0x03) == 1, "Target CS must have RPL=1");
        TEST_ASSERT(newRsp == r1Sub.getTss().rsp1, "Transition into Ring 1 must load TSS RSP1 stack");

        // 2. Invoke Gate 0 from Ring 3 (User) -> Should FAIL with AccessDenied (#GP)
        NtStatus stFail = r1Sub.invokeCallGate(0, ring1::CpuPrivilegeRing::Ring3_User, &targetRip, &targetCs, &newRsp);
        TEST_ASSERT(stFail == NtStatus::AccessDenied, "Ring 3 caller must be rejected by Gate 0 (DPL=1 violation)");

        // 3. Invoke Gate 0 from Ring 1 (Driver Shim) -> Should SUCCEED
        NtStatus st0 = r1Sub.invokeCallGate(0, ring1::CpuPrivilegeRing::Ring1_DriverShim, &targetRip, &targetCs, &newRsp);
        TEST_ASSERT(st0 == NtStatus::Success, "Ring 1 caller must successfully traverse Gate 0 into Ring 0");
        TEST_ASSERT((targetCs & 0x03) == 0, "Target CS must have RPL=0");
        TEST_ASSERT(newRsp == r1Sub.getTss().rsp0, "Transition into Ring 0 must load TSS RSP0 stack");
    }

    // ------------------------------------------------------------------------
    // Phase 3: Ring 1 Driver Hosting Lifecycle (Loading, Dispatching, Unloading)
    // ------------------------------------------------------------------------
    {
        auto& r1Sub = ring1::TitanRing1Subsystem::Instance();

        // 1. Create Driver Host in Ring 1
        uint32_t driverId = ring1::Ring1CreateDriverHost(L"SovereignAudioShim", 0xFFFF800010000000ULL, 0x40000);
        TEST_ASSERT(driverId > 0, "Ring1CreateDriverHost must return valid driver ID");

        auto host = r1Sub.getDriverHost(driverId);
        TEST_ASSERT(host != nullptr, "Driver host structure must exist");
        TEST_ASSERT(host->getStatus() == ring1::Ring1DriverStatus::Loaded, "Initial driver status must be Loaded");

        // 2. Set Entry Point and Dispatch Call
        static uint32_t s_lastIoctl = 0;
        host->setEntryPoint([](uint32_t ioControlCode, const void* inBuf, size_t inLen,
                               void* outBuf, size_t outLen, size_t* outWritten) -> NtStatus {
            (void)inBuf; (void)inLen;
            s_lastIoctl = ioControlCode;
            if (outBuf && outLen >= sizeof(uint32_t)) {
                *reinterpret_cast<uint32_t*>(outBuf) = 0xAA55AA55;
                if (outWritten) *outWritten = sizeof(uint32_t);
            }
            return NtStatus::Success;
        });
        TEST_ASSERT(host->getStatus() == ring1::Ring1DriverStatus::Running, "Driver status must transition to Running");

        uint32_t resp = 0;
        size_t written = 0;
        NtStatus callSt = ring1::Ring1InvokeDriverCall(driverId, 0x80002000, nullptr, 0, &resp, sizeof(resp), &written);
        TEST_ASSERT(callSt == NtStatus::Success, "Ring1InvokeDriverCall must succeed");
        TEST_ASSERT(s_lastIoctl == 0x80002000, "Driver must receive IOCTL 0x80002000");
        TEST_ASSERT(resp == 0xAA55AA55, "Driver output response must match expected payload");
        TEST_ASSERT(written == sizeof(uint32_t), "Output bytes written must match");

        // Clean up
        BOOL destOk = ring1::Ring1DestroyDriverHost(driverId);
        TEST_ASSERT(destOk == ring1::TRUE_VAL, "Ring1DestroyDriverHost must succeed");
    }

    // ------------------------------------------------------------------------
    // Phase 4: Zero-BSOD Fault Containment & Automatic Driver Crash Recovery
    // ------------------------------------------------------------------------
    {
        auto& r1Sub = ring1::TitanRing1Subsystem::Instance();

        uint32_t driverId = ring1::Ring1CreateDriverHost(L"VolatileStorageFilter", 0xFFFF800020000000ULL, 0x20000);
        TEST_ASSERT(driverId > 0, "Driver creation must succeed");

        auto host = r1Sub.getDriverHost(driverId);
        TEST_ASSERT(host->getFaultCount() == 0, "Fault count must be 0 initially");

        // Simulate a driver crash in Ring 1 (e.g. NULL pointer dereference / #GP at 0xFFFF800020001234)
        BOOL recOk = ring1::Ring1SimulateFaultRecovery(
            driverId, NtStatus::AccessViolation, 0xFFFF800020001234ULL, L"Null pointer read in driver dispatch");
        TEST_ASSERT(recOk == ring1::TRUE_VAL, "Fault recovery must succeed without BSOD");

        TEST_ASSERT(host->getFaultCount() == 1, "Fault count must be incremented to 1");
        TEST_ASSERT(host->getRecoveriesCount() == 1, "Recoveries count must be 1");
        TEST_ASSERT(host->getStatus() == ring1::Ring1DriverStatus::Recovered, "Driver status must transition to Recovered");

        const auto& crashHistory = host->getCrashHistory();
        TEST_ASSERT(crashHistory.size() == 1, "Crash history must record 1 dump");
        TEST_ASSERT(crashHistory[0].faultRip == 0xFFFF800020001234ULL, "Fault RIP must be accurately recorded");
        TEST_ASSERT(crashHistory[0].faultCode == NtStatus::AccessViolation, "Fault code must be recorded");

        // Simulate second crash
        ring1::Ring1SimulateFaultRecovery(
            driverId, NtStatus::IllegalInstruction, 0xFFFF800020005678ULL, L"UD2 opcode encountered in driver body");
        TEST_ASSERT(host->getFaultCount() == 2, "Fault count must be 2 after second incident");
        TEST_ASSERT(host->getRecoveriesCount() == 2, "Recoveries count must be 2");

        ring1::Ring1DestroyDriverHost(driverId);
    }

    // ------------------------------------------------------------------------
    // Phase 5: Hardware I/O Privilege Level (IOPL=1) & Port Bitmap Access Control
    // ------------------------------------------------------------------------
    {
        // 1. Port 0x03F8 (COM1) is allowed by default for Ring 1
        BOOL com1Allowed = ring1::Ring1CheckIoPortAccess(0x03F8, 1); // Ring 1
        TEST_ASSERT(com1Allowed == ring1::TRUE_VAL, "COM1 port 0x03F8 must be allowed for Ring 1");

        // 2. Port 0x0064 (PS/2 Keyboard) is denied by default for Ring 1
        BOOL ps2Allowed = ring1::Ring1CheckIoPortAccess(0x0064, 1); // Ring 1
        TEST_ASSERT(ps2Allowed == ring1::FALSE_VAL, "Port 0x0064 must be denied by default for Ring 1");

        // 3. Grant Port 0x0064 and re-verify
        ring1::Ring1SetIoPortAccess(0x0064, ring1::TRUE_VAL);
        BOOL ps2Granted = ring1::Ring1CheckIoPortAccess(0x0064, 1);
        TEST_ASSERT(ps2Granted == ring1::TRUE_VAL, "Port 0x0064 must be allowed after Ring1SetIoPortAccess");

        // 4. Revoke Port 0x0064 and re-verify
        ring1::Ring1SetIoPortAccess(0x0064, ring1::FALSE_VAL);
        BOOL ps2Revoked = ring1::Ring1CheckIoPortAccess(0x0064, 1);
        TEST_ASSERT(ps2Revoked == ring1::FALSE_VAL, "Port 0x0064 must be denied after revocation");

        // 5. Ring 0 always has full access regardless of bitmap
        BOOL r0Access = ring1::Ring1CheckIoPortAccess(0x0064, 0); // Ring 0
        TEST_ASSERT(r0Access == ring1::TRUE_VAL, "Ring 0 must unconditionally have access to all I/O ports");
    }

    // ------------------------------------------------------------------------
    // Phase 6: VTL 1 Secure Partitioning & Secret Vault Isolation
    // ------------------------------------------------------------------------
    {
        auto& r1Sub = ring1::TitanRing1Subsystem::Instance();
        auto& vtl1 = r1Sub.getVtl1();

        std::vector<uint8_t> masterKey = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};

        // 1. Write from VTL 1 -> Must SUCCEED
        bool setOk = vtl1.setSecret("BitLockerVMK", masterKey, 1); // VTL 1
        TEST_ASSERT(setOk == true, "VTL 1 caller must successfully write to secret vault");

        // 2. Read from VTL 1 -> Must SUCCEED
        std::vector<uint8_t> readKey;
        bool getOk = vtl1.getSecret("BitLockerVMK", readKey, 1); // VTL 1
        TEST_ASSERT(getOk == true, "VTL 1 caller must successfully read from secret vault");
        TEST_ASSERT(readKey == masterKey, "Read key payload must match original master key");

        // 3. Write from VTL 0 (Non-Secure Kernel) -> Must FAIL
        std::vector<uint8_t> rogueKey = {0xFF, 0xFF, 0xFF, 0xFF};
        bool rogueSet = vtl1.setSecret("RogueSecret", rogueKey, 0); // VTL 0
        TEST_ASSERT(rogueSet == false, "VTL 0 caller must be rejected when writing to VTL 1 vault");

        // 4. Read from VTL 0 -> Must FAIL
        std::vector<uint8_t> leakedKey;
        bool rogueGet = vtl1.getSecret("BitLockerVMK", leakedKey, 0); // VTL 0
        TEST_ASSERT(rogueGet == false, "VTL 0 caller must be locked out from reading VTL 1 secrets");
    }

    // ------------------------------------------------------------------------
    // Phase 7: Secure Hypercall Dispatching (VtlCall) & Crypto Isolation
    // ------------------------------------------------------------------------
    {
        // 1. Query VTL Version
        uint32_t vtlVer = 0;
        size_t written = 0;
        NtStatus stVer = ring1::Ring1VtlCall(ring1::VTL_HYPERCALL_QUERY_VERSION, nullptr, 0, &vtlVer, sizeof(vtlVer), &written);
        TEST_ASSERT(stVer == NtStatus::Success, "VTL_HYPERCALL_QUERY_VERSION must succeed");
        TEST_ASSERT(vtlVer == 0x00010000, "VTL version must report 1.0 (0x00010000)");

        // 2. Seal Secret Payload via Hypercall
        const char secretPayload[] = "SovereignSecureKernelKey2026";
        NtStatus stSeal = ring1::Ring1VtlCall(
            ring1::VTL_HYPERCALL_SEAL_SECRET, secretPayload, sizeof(secretPayload), nullptr, 0, &written);
        TEST_ASSERT(stSeal == NtStatus::Success, "VTL_HYPERCALL_SEAL_SECRET must succeed");

        // 3. Unseal Secret Payload via Hypercall
        char unsealed[64]{};
        NtStatus stUnseal = ring1::Ring1VtlCall(
            ring1::VTL_HYPERCALL_UNSEAL_SECRET, nullptr, 0, unsealed, sizeof(unsealed), &written);
        TEST_ASSERT(stUnseal == NtStatus::Success, "VTL_HYPERCALL_UNSEAL_SECRET must succeed");
        TEST_ASSERT(std::strcmp(unsealed, secretPayload) == 0, "Unsealed secret must match original sealed string");

        // 4. Code Integrity Verification Hypercall (skci.dll)
        uint32_t ciResult = 0;
        NtStatus stCi = ring1::Ring1VtlCall(
            ring1::VTL_HYPERCALL_VERIFY_PAGE_CI, nullptr, 0, &ciResult, sizeof(ciResult), &written);
        TEST_ASSERT(stCi == NtStatus::Success, "VTL_HYPERCALL_VERIFY_PAGE_CI must succeed");
        TEST_ASSERT(ciResult == 1, "CI verification must return 1 (Valid)");
    }

    // ------------------------------------------------------------------------
    // Phase 8: Inter-Ring Communication (Dynamic Call Gate Registration)
    // ------------------------------------------------------------------------
    {
        auto& r1Sub = ring1::TitanRing1Subsystem::Instance();

        // Register Dynamic Gate 2: User Mode to Custom Service Handler
        BOOL regOk = ring1::Ring1RegisterCallGate(2, ring1::GDT_SELECTOR_RING1_CODE64, 0xFFFF800000204000ULL, 3);
        TEST_ASSERT(regOk == ring1::TRUE_VAL, "Ring1RegisterCallGate must succeed");

        uint64_t targetRip = 0, newRsp = 0;
        uint16_t targetCs = 0;
        NtStatus stGate = ring1::Ring1InvokeCallGate(2, 3, &targetRip, &targetCs, &newRsp); // Caller Ring 3
        TEST_ASSERT(stGate == NtStatus::Success, "Ring1InvokeCallGate for dynamic gate must succeed");
        TEST_ASSERT(targetRip == 0xFFFF800000204000ULL, "Dynamic gate target RIP must match");
        TEST_ASSERT(newRsp == r1Sub.getTss().rsp1, "Dynamic gate must switch to Ring 1 stack");
    }

    // ------------------------------------------------------------------------
    // Phase 9: Dynamic Loader Exports, SCM Service Record & Version Database
    // ------------------------------------------------------------------------
    {
        // 1. JanusLDR Driver Exports Verification
        auto& ldr = ldr::DynamicLoader::get();
        TEST_ASSERT(ldr.getExport("ring1shim.sys", "Ring1CreateDriverHost") != nullptr, "ring1shim.sys Ring1CreateDriverHost must exist");
        TEST_ASSERT(ldr.getExport("ring1shim.sys", "Ring1DestroyDriverHost") != nullptr, "ring1shim.sys Ring1DestroyDriverHost must exist");
        TEST_ASSERT(ldr.getExport("ring1shim.sys", "Ring1InvokeDriverCall") != nullptr, "ring1shim.sys Ring1InvokeDriverCall must exist");
        TEST_ASSERT(ldr.getExport("ring1shim.sys", "Ring1RegisterCallGate") != nullptr, "ring1shim.sys Ring1RegisterCallGate must exist");
        TEST_ASSERT(ldr.getExport("ring1shim.sys", "Ring1InvokeCallGate") != nullptr, "ring1shim.sys Ring1InvokeCallGate must exist");
        TEST_ASSERT(ldr.getExport("ring1shim.sys", "Ring1VtlCall") != nullptr, "ring1shim.sys Ring1VtlCall must exist");
        TEST_ASSERT(ldr.getExport("ring1shim.sys", "Ring1SetIoPortAccess") != nullptr, "ring1shim.sys Ring1SetIoPortAccess must exist");
        TEST_ASSERT(ldr.getExport("ring1shim.sys", "Ring1CheckIoPortAccess") != nullptr, "ring1shim.sys Ring1CheckIoPortAccess must exist");
        TEST_ASSERT(ldr.getExport("ring1shim.sys", "Ring1SimulateFaultRecovery") != nullptr, "ring1shim.sys Ring1SimulateFaultRecovery must exist");
        TEST_ASSERT(ldr.getExport("ring1shim.sys", "Ring1GetTelemetry") != nullptr, "ring1shim.sys Ring1GetTelemetry must exist");

        // 2. SCM Service Record Verification
        auto& scmMgr = scm::ServiceControlManager::get();
        auto r1Svc = scmMgr.getServiceRecord(L"ring1shim");
        TEST_ASSERT(r1Svc != nullptr, "ring1shim service record must exist in SCM");
        TEST_ASSERT(r1Svc->serviceType == scm::SERVICE_KERNEL_DRIVER, "ring1shim must be a kernel driver");
        TEST_ASSERT(r1Svc->startType == scm::SERVICE_BOOT_START, "ring1shim must be boot start");
        TEST_ASSERT(r1Svc->status.dwCurrentState == scm::SERVICE_RUNNING, "ring1shim must be running");

        // 3. Version Database Verification
        auto& verDb = version::VersionDatabase::Instance();
        TEST_ASSERT(verDb.GetModuleInfo("ring1shim.sys") != nullptr, "ring1shim.sys must be registered in Version Database");
    }

    // ------------------------------------------------------------------------
    // Phase 10: Subsystem Diagnostic Telemetry & Metric Verification
    // ------------------------------------------------------------------------
    {
        ring1::RING1_TELEMETRY telem{};
        BOOL telemOk = ring1::Ring1GetTelemetry(&telem);
        TEST_ASSERT(telemOk == ring1::TRUE_VAL, "Ring1GetTelemetry must succeed");
        TEST_ASSERT(telem.totalCallGateTransitions >= 3, "totalCallGateTransitions must be >= 3");
        TEST_ASSERT(telem.totalDriverInvocations >= 1, "totalDriverInvocations must be >= 1");
        TEST_ASSERT(telem.totalFaultsContained >= 2, "totalFaultsContained must be >= 2");
        TEST_ASSERT(telem.totalRecoveriesExecuted >= 2, "totalRecoveriesExecuted must be >= 2");
        TEST_ASSERT(telem.totalVtlCallsExecuted >= 4, "totalVtlCallsExecuted must be >= 4");
        TEST_ASSERT(telem.totalIoPortAccesses >= 5, "totalIoPortAccesses must be >= 5");
        TEST_ASSERT(telem.activeCallGatesCount >= 2, "activeCallGatesCount must be >= 2");
        TEST_ASSERT(telem.vtl1SecretsCount >= 1, "vtl1SecretsCount must be >= 1");

        ring1::RING1_TELEMETRY directTelem = ring1::Ring1QueryTelemetry();
        TEST_ASSERT(directTelem.totalFaultsContained == telem.totalFaultsContained, "Direct C++ telemetry query must match");
    }

    std::cout << "[TEST] Suite 253: Horizon 37 - Ring 1 Privileged Service Domain & Fault Isolation PASSED.\n";
}

// ============================================================================
// Suite 254: Horizon 38 - I/O Filter Manager & Minifilter Driver Architecture
// ============================================================================

void Test_Horizon38_FilterManager_Minifilter_Architecture() {
    std::cout << "[TEST] Running Suite 254: Horizon 38 - I/O Filter Manager & Minifilter Driver Architecture...\n" << std::flush;

    // ------------------------------------------------------------------------
    // Phase 1: Subsystem Initialization, JanusLDR Exports & SCM/Version DB
    // ------------------------------------------------------------------------
    {
        fltmgr::InitializeFilterManagerSubsystemExports();
        auto& fltMgr = fltmgr::TitanFilterManagerSubsystem::Instance();
        fltMgr.resetForTesting();

        // 1. JanusLDR Driver Exports Verification
        auto& ldr = ldr::DynamicLoader::get();
        TEST_ASSERT(ldr.getExport("fltmgr.sys", "FltRegisterFilter") != nullptr, "fltmgr.sys FltRegisterFilter must exist");
        TEST_ASSERT(ldr.getExport("fltmgr.sys", "FltStartFiltering") != nullptr, "fltmgr.sys FltStartFiltering must exist");
        TEST_ASSERT(ldr.getExport("fltmgr.sys", "FltUnregisterFilter") != nullptr, "fltmgr.sys FltUnregisterFilter must exist");
        TEST_ASSERT(ldr.getExport("fltmgr.sys", "FltDispatchOperation") != nullptr, "fltmgr.sys FltDispatchOperation must exist");
        TEST_ASSERT(ldr.getExport("fltmgr.sys", "FltAllocateContext") != nullptr, "fltmgr.sys FltAllocateContext must exist");
        TEST_ASSERT(ldr.getExport("fltmgr.sys", "FltReleaseContext") != nullptr, "fltmgr.sys FltReleaseContext must exist");
        TEST_ASSERT(ldr.getExport("fltmgr.sys", "FltCreateCommunicationPort") != nullptr, "fltmgr.sys FltCreateCommunicationPort must exist");
        TEST_ASSERT(ldr.getExport("fltmgr.sys", "FltCloseCommunicationPort") != nullptr, "fltmgr.sys FltCloseCommunicationPort must exist");
        TEST_ASSERT(ldr.getExport("fltmgr.sys", "FilterConnectCommunicationPort") != nullptr, "fltmgr.sys FilterConnectCommunicationPort must exist");
        TEST_ASSERT(ldr.getExport("fltmgr.sys", "FilterSendMessage") != nullptr, "fltmgr.sys FilterSendMessage must exist");
        TEST_ASSERT(ldr.getExport("fltmgr.sys", "FilterClose") != nullptr, "fltmgr.sys FilterClose must exist");
        TEST_ASSERT(ldr.getExport("fltmgr.sys", "FltGetTelemetry") != nullptr, "fltmgr.sys FltGetTelemetry must exist");

        // 2. SCM Service Record Verification
        auto& scmMgr = scm::ServiceControlManager::get();
        auto fltSvc = scmMgr.getServiceRecord(L"fltmgr");
        TEST_ASSERT(fltSvc != nullptr, "fltmgr service record must exist in SCM");
        TEST_ASSERT(fltSvc->serviceType == scm::SERVICE_KERNEL_DRIVER, "fltmgr must be a kernel driver");
        TEST_ASSERT(fltSvc->startType == scm::SERVICE_BOOT_START, "fltmgr must be boot start");
        TEST_ASSERT(fltSvc->status.dwCurrentState == scm::SERVICE_RUNNING, "fltmgr must be running");

        // 3. Version Database Verification
        auto& verDb = version::VersionDatabase::Instance();
        TEST_ASSERT(verDb.GetModuleInfo("fltmgr.sys") != nullptr, "fltmgr.sys must be registered in Version Database");
    }

    // ------------------------------------------------------------------------
    // Phase 2: Dynamic Minifilter Registration & Altitude Stacking
    // ------------------------------------------------------------------------
    uint32_t dynamicFilterHandle = 0;
    bool unloadCalled = false;
    {
        fltmgr::FLT_REGISTRATION dynReg{};
        dynReg.FilterName = L"SovereignReplicationFlt";
        dynReg.Altitude = fltmgr::ALTITUDE_REPLICATION; // 265000
        dynReg.FilterUnloadCallback = [&unloadCalled](uint32_t) -> NtStatus {
            unloadCalled = true;
            return NtStatus::Success;
        };

        // Add dummy operation
        fltmgr::FLT_OPERATION_REGISTRATION op{};
        op.MajorFunction = fltmgr::IRP_MJ_READ;
        op.PreOperation = [](fltmgr::FLT_CALLBACK_DATA*, void**) -> fltmgr::FltPreopCallbackStatus {
            return fltmgr::FltPreopCallbackStatus::SuccessNoCallback;
        };
        dynReg.OperationRegistration.push_back(op);

        NtStatus st = fltmgr::FltRegisterFilter(&dynReg, &dynamicFilterHandle);
        TEST_ASSERT(st == NtStatus::Success, "FltRegisterFilter must succeed for dynamic filter");
        TEST_ASSERT(dynamicFilterHandle > 0, "dynamicFilterHandle must be non-zero");

        NtStatus stStart = fltmgr::FltStartFiltering(dynamicFilterHandle);
        TEST_ASSERT(stStart == NtStatus::Success, "FltStartFiltering must succeed");

        auto telem = fltmgr::FltQueryTelemetry();
        TEST_ASSERT(telem.activeFiltersCount >= 4, "Active filters count must include 3 default + 1 dynamic");
    }

    // ------------------------------------------------------------------------
    // Phase 3: Top-Down Pre-Operation Traversal (Altitude Order)
    // ------------------------------------------------------------------------
    {
        std::vector<std::string> traversalOrder;

        fltmgr::FLT_REGISTRATION highReg{};
        highReg.FilterName = L"HighFilter";
        highReg.Altitude = "390000";
        fltmgr::FLT_OPERATION_REGISTRATION highOp{};
        highOp.MajorFunction = fltmgr::IRP_MJ_QUERY_INFORMATION;
        highOp.PreOperation = [&traversalOrder](fltmgr::FLT_CALLBACK_DATA*, void**) -> fltmgr::FltPreopCallbackStatus {
            traversalOrder.push_back("HighFilter");
            return fltmgr::FltPreopCallbackStatus::SuccessWithCallback;
        };
        highOp.PostOperation = [&traversalOrder](fltmgr::FLT_CALLBACK_DATA*, void*, uint32_t) -> fltmgr::FltPostopCallbackStatus {
            traversalOrder.push_back("Post_HighFilter");
            return fltmgr::FltPostopCallbackStatus::FinishedProcessing;
        };
        highReg.OperationRegistration.push_back(highOp);

        fltmgr::FLT_REGISTRATION lowReg{};
        lowReg.FilterName = L"LowFilter";
        lowReg.Altitude = "120000";
        fltmgr::FLT_OPERATION_REGISTRATION lowOp{};
        lowOp.MajorFunction = fltmgr::IRP_MJ_QUERY_INFORMATION;
        lowOp.PreOperation = [&traversalOrder](fltmgr::FLT_CALLBACK_DATA*, void**) -> fltmgr::FltPreopCallbackStatus {
            traversalOrder.push_back("LowFilter");
            return fltmgr::FltPreopCallbackStatus::SuccessWithCallback;
        };
        lowOp.PostOperation = [&traversalOrder](fltmgr::FLT_CALLBACK_DATA*, void*, uint32_t) -> fltmgr::FltPostopCallbackStatus {
            traversalOrder.push_back("Post_LowFilter");
            return fltmgr::FltPostopCallbackStatus::FinishedProcessing;
        };
        lowReg.OperationRegistration.push_back(lowOp);

        uint32_t hHigh = 0, hLow = 0;
        fltmgr::FltRegisterFilter(&highReg, &hHigh);
        fltmgr::FltStartFiltering(hHigh);
        fltmgr::FltRegisterFilter(&lowReg, &hLow);
        fltmgr::FltStartFiltering(hLow);

        fltmgr::FLT_CALLBACK_DATA cbData{};
        cbData.Iopb.MajorFunction = fltmgr::IRP_MJ_QUERY_INFORMATION;
        cbData.Iopb.TargetFilePath = L"\\Device\\HarddiskVolume1\\test.dat";

        NtStatus stDisp = fltmgr::FltDispatchOperation(&cbData);
        TEST_ASSERT(stDisp == NtStatus::Success, "FltDispatchOperation must succeed");

        // Verify Pre-Op ordering: HighFilter (390000) must run BEFORE LowFilter (120000)
        TEST_ASSERT(traversalOrder.size() == 4, "traversalOrder must contain 4 callback entries");
        TEST_ASSERT(traversalOrder[0] == "HighFilter", "HighFilter pre-op must execute first");
        TEST_ASSERT(traversalOrder[1] == "LowFilter", "LowFilter pre-op must execute second");

        // --------------------------------------------------------------------
        // Phase 4: Bottom-Up Post-Operation Callback Unwinding Order
        // --------------------------------------------------------------------
        // Post-op executes in reverse (ascending altitude) order: LowFilter post-op before HighFilter post-op!
        TEST_ASSERT(traversalOrder[2] == "Post_LowFilter", "LowFilter post-op must execute before HighFilter post-op");
        TEST_ASSERT(traversalOrder[3] == "Post_HighFilter", "HighFilter post-op must execute last");

        fltmgr::FltUnregisterFilter(hHigh);
        fltmgr::FltUnregisterFilter(hLow);
    }

    // ------------------------------------------------------------------------
    // Phase 5: Short-Circuiting I/O with FLT_PREOP_COMPLETE
    // ------------------------------------------------------------------------
    {
        fltmgr::FLT_REGISTRATION blockReg{};
        blockReg.FilterName = L"BlockingFilter";
        blockReg.Altitude = "350000";
        fltmgr::FLT_OPERATION_REGISTRATION blockOp{};
        blockOp.MajorFunction = fltmgr::IRP_MJ_DEVICE_CONTROL;
        blockOp.PreOperation = [](fltmgr::FLT_CALLBACK_DATA* Data, void**) -> fltmgr::FltPreopCallbackStatus {
            Data->IoStatus.Status = NtStatus::AccessDenied;
            return fltmgr::FltPreopCallbackStatus::Complete; // Short-circuit!
        };
        blockReg.OperationRegistration.push_back(blockOp);

        uint32_t hBlock = 0;
        fltmgr::FltRegisterFilter(&blockReg, &hBlock);
        fltmgr::FltStartFiltering(hBlock);

        fltmgr::FLT_CALLBACK_DATA cbData{};
        cbData.Iopb.MajorFunction = fltmgr::IRP_MJ_DEVICE_CONTROL;
        cbData.IoStatus.Status = NtStatus::Success;

        NtStatus stDisp = fltmgr::FltDispatchOperation(&cbData);
        TEST_ASSERT(stDisp == NtStatus::AccessDenied, "Operation must be short-circuited with AccessDenied");

        fltmgr::FltUnregisterFilter(hBlock);
    }

    // ------------------------------------------------------------------------
    // Phase 6: Minifilter Context Management (Stream & File Contexts)
    // ------------------------------------------------------------------------
    {
        auto& fltMgr = fltmgr::TitanFilterManagerSubsystem::Instance();
        uint32_t filterHandle = fltMgr.getSentinelScanFilterHandle();

        void* rawCtx = nullptr;
        NtStatus stAlloc = fltmgr::FltAllocateContext(
            filterHandle,
            fltmgr::FltContextType::StreamContext,
            128,
            0,
            &rawCtx
        );
        TEST_ASSERT(stAlloc == NtStatus::Success, "FltAllocateContext must succeed");
        TEST_ASSERT(rawCtx != nullptr, "Context pointer must not be null");

        auto* ctx = reinterpret_cast<fltmgr::FltContext*>(rawCtx);
        TEST_ASSERT(ctx->getType() == fltmgr::FltContextType::StreamContext, "Context type must match");
        TEST_ASSERT(ctx->getSize() == 128, "Context size must be 128 bytes");
        TEST_ASSERT(ctx->getRefCount() == 2, "Ref count must be 2 after alloc+caller ref");

        // Write marker into context
        uint8_t* ctxBuf = reinterpret_cast<uint8_t*>(ctx->getData());
        ctxBuf[0] = 0xAA;
        ctxBuf[1] = 0xBB;

        // Associate context with stream
        std::shared_ptr<fltmgr::FltContext> sharedCtx;
        fltMgr.allocateContext(filterHandle, fltmgr::FltContextType::FileContext, 64, sharedCtx);
        TEST_ASSERT(sharedCtx != nullptr, "allocateContext must return valid shared_ptr");

        fltMgr.setFileContext(0xDEADBEEF, sharedCtx);

        std::shared_ptr<fltmgr::FltContext> retrievedCtx;
        NtStatus stGet = fltMgr.getFileContext(0xDEADBEEF, retrievedCtx);
        TEST_ASSERT(stGet == NtStatus::Success, "getFileContext must find the attached context");
        TEST_ASSERT(retrievedCtx == sharedCtx, "Retrieved context must match attached context");

        fltmgr::FltReleaseContext(rawCtx);
    }

    // ------------------------------------------------------------------------
    // Phase 7: Real-Time Anti-Virus Minifilter (SentinelScanFlt @ 328000)
    // ------------------------------------------------------------------------
    {
        // 1. Intercept malicious file path on CREATE
        fltmgr::FLT_CALLBACK_DATA badCreateData{};
        badCreateData.Iopb.MajorFunction = fltmgr::IRP_MJ_CREATE;
        badCreateData.Iopb.TargetFilePath = L"\\Device\\HarddiskVolume1\\malware.exe";

        NtStatus stMalware = fltmgr::FltDispatchOperation(&badCreateData);
        TEST_ASSERT(stMalware == NtStatus::AccessDenied, "SentinelScanFlt must block malware.exe with AccessDenied");

        // 2. Allow benign file on CREATE
        fltmgr::FLT_CALLBACK_DATA goodCreateData{};
        goodCreateData.Iopb.MajorFunction = fltmgr::IRP_MJ_CREATE;
        goodCreateData.Iopb.TargetFilePath = L"\\Device\\HarddiskVolume1\\calc.exe";

        NtStatus stGood = fltmgr::FltDispatchOperation(&goodCreateData);
        TEST_ASSERT(stGood == NtStatus::Success, "SentinelScanFlt must permit benign calc.exe creation");

        // 3. Intercept EICAR threat string on WRITE
        char threatBuffer[] = "X5O!P%@AP[4\\PZX54(P^)7CC)7}$EICAR-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*";
        fltmgr::FLT_CALLBACK_DATA badWriteData{};
        badWriteData.Iopb.MajorFunction = fltmgr::IRP_MJ_WRITE;
        badWriteData.Iopb.TargetFilePath = L"\\Device\\HarddiskVolume1\\test.tmp";
        badWriteData.Iopb.ReadWrite.Buffer = threatBuffer;
        badWriteData.Iopb.ReadWrite.Length = sizeof(threatBuffer);

        NtStatus stThreatWrite = fltmgr::FltDispatchOperation(&badWriteData);
        TEST_ASSERT(stThreatWrite == NtStatus::AccessDenied, "SentinelScanFlt must block EICAR signature write");
    }

    // ------------------------------------------------------------------------
    // Phase 8: Transparent Cryptographic Minifilter (EmeraldFlt @ 141000)
    // ------------------------------------------------------------------------
    {
        char secretData[] = "Dave Cutler Sovereign MicaNT Clean-Room Kernel 2026!";
        char originalCopy[sizeof(secretData)];
        std::memcpy(originalCopy, secretData, sizeof(secretData));

        // 1. WRITE to .efs encrypted file
        fltmgr::FLT_CALLBACK_DATA writeData{};
        writeData.Iopb.MajorFunction = fltmgr::IRP_MJ_WRITE;
        writeData.Iopb.TargetFilePath = L"\\Device\\HarddiskVolume1\\finance.efs";
        writeData.Iopb.ReadWrite.Buffer = secretData;
        writeData.Iopb.ReadWrite.Length = sizeof(secretData);

        NtStatus stEncWrite = fltmgr::FltDispatchOperation(&writeData);
        TEST_ASSERT(stEncWrite == NtStatus::Success, "EmeraldFlt write dispatch must succeed");
        // Verify that in-transit data was encrypted (XOR 0x5A)
        TEST_ASSERT(std::memcmp(secretData, originalCopy, sizeof(secretData)) != 0, "Buffer must be encrypted on write");

        // 2. READ from .efs encrypted file -> Post-Op automatically decrypts it back!
        fltmgr::FLT_CALLBACK_DATA readData{};
        readData.Iopb.MajorFunction = fltmgr::IRP_MJ_READ;
        readData.Iopb.TargetFilePath = L"\\Device\\HarddiskVolume1\\finance.efs";
        readData.Iopb.ReadWrite.Buffer = secretData;
        readData.Iopb.ReadWrite.Length = sizeof(secretData);

        NtStatus stEncRead = fltmgr::FltDispatchOperation(&readData);
        TEST_ASSERT(stEncRead == NtStatus::Success, "EmeraldFlt read dispatch must succeed");
        // Verify that Post-Op successfully decrypted the buffer back to original plaintext
        TEST_ASSERT(std::memcmp(secretData, originalCopy, sizeof(secretData)) == 0, "Buffer must be restored to original plaintext on read");
    }

    // ------------------------------------------------------------------------
    // Phase 9: Kernel-to-User Communication Ports (FilterSendMessage)
    // ------------------------------------------------------------------------
    {
        uint32_t clientHandle = 0;
        NtStatus stConn = fltmgr::FilterConnectCommunicationPort(
            L"\\SentinelScanPort",
            0,
            nullptr,
            0,
            &clientHandle
        );
        TEST_ASSERT(stConn == NtStatus::Success, "FilterConnectCommunicationPort must connect to \\SentinelScanPort");
        TEST_ASSERT(clientHandle > 0, "clientHandle must be valid non-zero handle");

        // 1. Send threat payload for evaluation
        const char msgThreat[] = "SCAN: VIRUS_DROPPER_SAMPLE";
        uint32_t verdict = 0;
        uint32_t retLen = 0;
        NtStatus stMsgThreat = fltmgr::FilterSendMessage(
            clientHandle,
            msgThreat,
            sizeof(msgThreat),
            &verdict,
            sizeof(verdict),
            &retLen
        );
        TEST_ASSERT(stMsgThreat == NtStatus::Success, "FilterSendMessage threat scan must succeed");
        TEST_ASSERT(verdict == 1, "Verdict must indicate 1 (VIRUS detected)");
        TEST_ASSERT(retLen == sizeof(uint32_t), "Return length must be 4 bytes");

        // 2. Send clean payload
        const char msgClean[] = "SCAN: SYSTEM_CALCULATOR";
        uint32_t cleanVerdict = 99;
        NtStatus stMsgClean = fltmgr::FilterSendMessage(
            clientHandle,
            msgClean,
            sizeof(msgClean),
            &cleanVerdict,
            sizeof(cleanVerdict),
            &retLen
        );
        TEST_ASSERT(stMsgClean == NtStatus::Success, "FilterSendMessage clean scan must succeed");
        TEST_ASSERT(cleanVerdict == 0, "Verdict must indicate 0 (Clean)");

        fltmgr::FilterClose(clientHandle);
    }

    // ------------------------------------------------------------------------
    // Phase 10: Dynamic Filter Unloading & Subsystem Diagnostic Telemetry
    // ------------------------------------------------------------------------
    {
        // Unregister dynamic filter from Phase 2
        NtStatus stUnload = fltmgr::FltUnregisterFilter(dynamicFilterHandle);
        TEST_ASSERT(stUnload == NtStatus::Success, "FltUnregisterFilter must succeed");
        TEST_ASSERT(unloadCalled == true, "FilterUnloadCallback must have been invoked");

        // Verify Telemetry
        fltmgr::FLT_TELEMETRY telem{};
        BOOL telemOk = fltmgr::FltGetTelemetry(&telem);
        TEST_ASSERT(telemOk == fltmgr::TRUE_VAL, "FltGetTelemetry must succeed");
        TEST_ASSERT(telem.activeFiltersCount >= 3, "activeFiltersCount must reflect remaining default filters");
        TEST_ASSERT(telem.totalPreOpsDispatched >= 6, "totalPreOpsDispatched must be >= 6");
        TEST_ASSERT(telem.totalPostOpsDispatched >= 2, "totalPostOpsDispatched must be >= 2");
        TEST_ASSERT(telem.totalOpsShortCircuited >= 3, "totalOpsShortCircuited must be >= 3");
        TEST_ASSERT(telem.totalMalwareBlocked >= 3, "totalMalwareBlocked must be >= 3");
        TEST_ASSERT(telem.totalCryptoOpsProcessed >= 2, "totalCryptoOpsProcessed must be >= 2");
        TEST_ASSERT(telem.totalPortMessagesSent >= 2, "totalPortMessagesSent must be >= 2");

        fltmgr::FLT_TELEMETRY directTelem = fltmgr::FltQueryTelemetry();
        TEST_ASSERT(directTelem.totalMalwareBlocked == telem.totalMalwareBlocked, "Direct telemetry query must match");
    }

    std::cout << "[TEST] Suite 254: Horizon 38 - I/O Filter Manager & Minifilter Driver Architecture PASSED.\n";
}

// ============================================================================
// Suite 255: Horizon 39 - Sovereign Kernel Crash Dump Engine, BugCheck & BSOD
// ============================================================================

void Test_Horizon39_KernelCrashDumpEngine_BugCheck_And_BSOD() {
    std::cout << "[TEST] Running Suite 255: Horizon 39 - Sovereign Kernel Crash Dump Engine, BugCheck & BSOD...\n" << std::flush;

    // ------------------------------------------------------------------------
    // Phase 1: Subsystem Initialization, JanusLDR Driver Exports & SCM/Version DB
    // ------------------------------------------------------------------------
    {
        crashdump::InitializeCrashDumpSubsystemExports();
        auto& dumpEngine = crashdump::TitanCrashDumpEngine::Instance();
        dumpEngine.resetForTesting();

        // 1. JanusLDR Exports Verification (ntoskrnl.exe & crashdump.sys)
        auto& ldr = ldr::DynamicLoader::get();
        TEST_ASSERT(ldr.getExport("ntoskrnl.exe", "KeBugCheck") != nullptr, "ntoskrnl.exe KeBugCheck must exist");
        TEST_ASSERT(ldr.getExport("ntoskrnl.exe", "KeBugCheckEx") != nullptr, "ntoskrnl.exe KeBugCheckEx must exist");
        TEST_ASSERT(ldr.getExport("ntoskrnl.exe", "KeRegisterBugCheckCallback") != nullptr, "ntoskrnl.exe KeRegisterBugCheckCallback must exist");
        TEST_ASSERT(ldr.getExport("ntoskrnl.exe", "KeDeregisterBugCheckCallback") != nullptr, "ntoskrnl.exe KeDeregisterBugCheckCallback must exist");
        TEST_ASSERT(ldr.getExport("ntoskrnl.exe", "KeRegisterBugCheckReasonCallback") != nullptr, "ntoskrnl.exe KeRegisterBugCheckReasonCallback must exist");
        TEST_ASSERT(ldr.getExport("ntoskrnl.exe", "KeDeregisterBugCheckReasonCallback") != nullptr, "ntoskrnl.exe KeDeregisterBugCheckReasonCallback must exist");
        TEST_ASSERT(ldr.getExport("ntoskrnl.exe", "IoWriteCrashDump") != nullptr, "ntoskrnl.exe IoWriteCrashDump must exist");
        TEST_ASSERT(ldr.getExport("ntoskrnl.exe", "KeGetBugCheckTelemetry") != nullptr, "ntoskrnl.exe KeGetBugCheckTelemetry must exist");

        TEST_ASSERT(ldr.getExport("crashdump.sys", "IoWriteCrashDump") != nullptr, "crashdump.sys IoWriteCrashDump must exist");
        TEST_ASSERT(ldr.getExport("crashdump.sys", "KeGetBugCheckTelemetry") != nullptr, "crashdump.sys KeGetBugCheckTelemetry must exist");

        // 2. SCM Service Record Verification
        auto& scmMgr = scm::ServiceControlManager::get();
        auto crashSvc = scmMgr.getServiceRecord(L"crashdump");
        TEST_ASSERT(crashSvc != nullptr, "crashdump service record must exist in SCM");
        TEST_ASSERT(crashSvc->serviceType == scm::SERVICE_KERNEL_DRIVER, "crashdump must be a kernel driver");
        TEST_ASSERT(crashSvc->startType == scm::SERVICE_BOOT_START, "crashdump must be boot start");
        TEST_ASSERT(crashSvc->status.dwCurrentState == scm::SERVICE_RUNNING, "crashdump must be running");

        // 3. Version Database Verification
        auto& verDb = version::VersionDatabase::Instance();
        TEST_ASSERT(verDb.GetModuleInfo("crashdump.sys") != nullptr, "crashdump.sys must be registered in Version Database");
    }

    // ------------------------------------------------------------------------
    // Phase 2: BugCheck Stop Code Catalog & Name String Resolution
    // ------------------------------------------------------------------------
    {
        TEST_ASSERT(std::string(crashdump::BugCheckCodeToString(crashdump::IRQL_NOT_LESS_OR_EQUAL)) == "IRQL_NOT_LESS_OR_EQUAL", "0x0A must map to IRQL_NOT_LESS_OR_EQUAL");
        TEST_ASSERT(std::string(crashdump::BugCheckCodeToString(crashdump::PAGE_FAULT_IN_NONPAGED_AREA)) == "PAGE_FAULT_IN_NONPAGED_AREA", "0x50 must map to PAGE_FAULT_IN_NONPAGED_AREA");
        TEST_ASSERT(std::string(crashdump::BugCheckCodeToString(crashdump::SYSTEM_SERVICE_EXCEPTION)) == "SYSTEM_SERVICE_EXCEPTION", "0x3B must map to SYSTEM_SERVICE_EXCEPTION");
        TEST_ASSERT(std::string(crashdump::BugCheckCodeToString(crashdump::KMODE_EXCEPTION_NOT_HANDLED)) == "KMODE_EXCEPTION_NOT_HANDLED", "0x1E must map to KMODE_EXCEPTION_NOT_HANDLED");
        TEST_ASSERT(std::string(crashdump::BugCheckCodeToString(crashdump::CRITICAL_PROCESS_DIED)) == "CRITICAL_PROCESS_DIED", "0xEF must map to CRITICAL_PROCESS_DIED");
        TEST_ASSERT(std::string(crashdump::BugCheckCodeToString(crashdump::DPC_WATCHDOG_VIOLATION)) == "DPC_WATCHDOG_VIOLATION", "0x133 must map to DPC_WATCHDOG_VIOLATION");
        TEST_ASSERT(std::string(crashdump::BugCheckCodeToString(crashdump::KERNEL_SECURITY_CHECK_FAILURE)) == "KERNEL_SECURITY_CHECK_FAILURE", "0x139 must map to KERNEL_SECURITY_CHECK_FAILURE");
        TEST_ASSERT(std::string(crashdump::BugCheckCodeToString(0x12345678)) == "UNKNOWN_BUGCHECK_CODE", "Invalid code must map to UNKNOWN_BUGCHECK_CODE");
    }

    // ------------------------------------------------------------------------
    // Phase 3: BugCheck Standard Callback Registration & Invocations
    // ------------------------------------------------------------------------
    static bool s_cbInvoked = false;
    static uint32_t s_cbBufferVal = 0;
    crashdump::KBUGCHECK_CALLBACK_RECORD cbRecord{};
    uint32_t testBuffer = 0x1337C0DE;
    {
        s_cbInvoked = false;
        s_cbBufferVal = 0;
        crashdump::PKBUGCHECK_CALLBACK_ROUTINE cbFunc = [](void* buf, uint32_t len) {
            s_cbInvoked = true;
            if (buf && len >= sizeof(uint32_t)) {
                s_cbBufferVal = *reinterpret_cast<uint32_t*>(buf);
            }
        };

        BOOL regOk = crashdump::KeRegisterBugCheckCallback(
            &cbRecord,
            cbFunc,
            &testBuffer,
            sizeof(testBuffer),
            "StorageMiniportVerifier"
        );
        TEST_ASSERT(regOk == crashdump::TRUE_VAL, "KeRegisterBugCheckCallback must succeed");
        TEST_ASSERT(cbRecord.IsRegistered == true, "cbRecord must be marked registered");

        // Trigger BugCheck
        crashdump::KeBugCheckEx(crashdump::IRQL_NOT_LESS_OR_EQUAL, 0xDEADBEEF, 2, 1, 0xFFFFF80004010000ULL);
        TEST_ASSERT(s_cbInvoked == true, "BugCheck callback must be invoked during KeBugCheckEx");
        TEST_ASSERT(s_cbBufferVal == 0x1337C0DE, "Callback buffer data must be correctly passed");
        TEST_ASSERT(cbRecord.InvocationCount == 1, "InvocationCount must be 1");

        // Deregister
        BOOL deregOk = crashdump::KeDeregisterBugCheckCallback(&cbRecord);
        TEST_ASSERT(deregOk == crashdump::TRUE_VAL, "KeDeregisterBugCheckCallback must succeed");
        TEST_ASSERT(cbRecord.IsRegistered == false, "cbRecord must be marked unregistered");
    }

    // ------------------------------------------------------------------------
    // Phase 4: BugCheck Reason Callback Registration & Dispatch
    // ------------------------------------------------------------------------
    static bool s_reasonInvoked = false;
    crashdump::KBUGCHECK_REASON_CALLBACK_RECORD reasonRecord{};
    {
        s_reasonInvoked = false;
        crashdump::PKBUGCHECK_REASON_CALLBACK_ROUTINE reasonFunc = [](
            crashdump::KBUGCHECK_CALLBACK_REASON reason,
            crashdump::KBUGCHECK_REASON_CALLBACK_RECORD*,
            void* reasonBuffer,
            uint32_t reasonBufferLength
        ) {
            s_reasonInvoked = true;
            if (reason == crashdump::KBUGCHECK_CALLBACK_REASON::KbCallbackSecondaryDumpData && reasonBuffer) {
                auto* sdd = reinterpret_cast<crashdump::KBUGCHECK_SECONDARY_DUMP_DATA*>(reasonBuffer);
                sdd->OutBufferLength = 32;
            }
            (void)reasonBufferLength;
        };

        BOOL regReasonOk = crashdump::KeRegisterBugCheckReasonCallback(
            &reasonRecord,
            reasonFunc,
            crashdump::KBUGCHECK_CALLBACK_REASON::KbCallbackSecondaryDumpData,
            "NetworkFilterDriver"
        );
        TEST_ASSERT(regReasonOk == crashdump::TRUE_VAL, "KeRegisterBugCheckReasonCallback must succeed");
        TEST_ASSERT(reasonRecord.IsRegistered == true, "reasonRecord must be marked registered");

        crashdump::KeBugCheckEx(crashdump::SYSTEM_SERVICE_EXCEPTION, 0xC0000005, 0xFFFFF80004B81000ULL, 0, 0);
        TEST_ASSERT(s_reasonInvoked == true, "Reason callback must be invoked during KeBugCheckEx");
        TEST_ASSERT(reasonRecord.InvocationCount == 1, "Reason callback InvocationCount must be 1");

        BOOL deregReasonOk = crashdump::KeDeregisterBugCheckReasonCallback(&reasonRecord);
        TEST_ASSERT(deregReasonOk == crashdump::TRUE_VAL, "KeDeregisterBugCheckReasonCallback must succeed");
    }

    // ------------------------------------------------------------------------
    // Phase 5: 64-bit Memory Dump Header Integrity & Checksum ('PAGE' / 'DU64')
    // ------------------------------------------------------------------------
    {
        auto& dumpEngine = crashdump::TitanCrashDumpEngine::Instance();
        const auto& hdr = dumpEngine.getLastDumpHeader();

        TEST_ASSERT(hdr.Signature == crashdump::DUMP_SIGNATURE_PAGE, "Header signature must be 'PAGE'");
        TEST_ASSERT(hdr.ValidDump == crashdump::DUMP_VALID_DUMP64, "Header ValidDump must be 'DU64'");
        TEST_ASSERT(hdr.MajorVersion == 0x000F, "MajorVersion must be 0x000F (NT 10.0 / 11)");
        TEST_ASSERT(hdr.MinorVersion == 26100, "MinorVersion must be 26100 (Windows 11 24H2)");
        TEST_ASSERT(hdr.MachineImageType == crashdump::DUMP_MACHINE_AMD64, "MachineImageType must be AMD64");
        TEST_ASSERT(hdr.NumberProcessors >= 1, "NumberProcessors must be >= 1");
        TEST_ASSERT(hdr.BugCheckCode == crashdump::SYSTEM_SERVICE_EXCEPTION, "BugCheckCode must match last crash");
        TEST_ASSERT(hdr.BugCheckParameter1 == 0xC0000005, "Param 1 must match 0xC0000005");
        TEST_ASSERT(hdr.CheckSum != 0, "Dump header CheckSum must be non-zero");
    }

    // ------------------------------------------------------------------------
    // Phase 6: Machine State Context Capture & AMD64 Register Preservation
    // ------------------------------------------------------------------------
    {
        auto& dumpEngine = crashdump::TitanCrashDumpEngine::Instance();
        crashdump::CONTEXT_AMD64 customCtx{};
        customCtx.Rip = 0xFFFFF80004B81234ULL;
        customCtx.Rsp = 0xFFFFD00010002000ULL;
        customCtx.Rbp = 0xFFFFD00010002080ULL;
        customCtx.Rax = 0xCAFEBABE11223344ULL;
        customCtx.Rbx = 0x5566778899AABBCCULL;
        customCtx.Rcx = 0x0000000000000001ULL;
        customCtx.Cr2 = 0x00007FF812345000ULL;
        customCtx.Cr3 = 0x000000001000ULL;
        customCtx.Rflags = 0x00000286;

        dumpEngine.executeBugCheck(
            crashdump::PAGE_FAULT_IN_NONPAGED_AREA,
            customCtx.Cr2,
            0,
            customCtx.Rip,
            0,
            &customCtx
        );

        const auto& hdr = dumpEngine.getLastDumpHeader();
        TEST_ASSERT(hdr.ContextRecord.Rip == customCtx.Rip, "Dump context RIP must match");
        TEST_ASSERT(hdr.ContextRecord.Rsp == customCtx.Rsp, "Dump context RSP must match");
        TEST_ASSERT(hdr.ContextRecord.Rax == customCtx.Rax, "Dump context RAX must match");
        TEST_ASSERT(hdr.ContextRecord.Rbx == customCtx.Rbx, "Dump context RBX must match");
        TEST_ASSERT(hdr.ContextRecord.Cr2 == customCtx.Cr2, "Dump context CR2 must match");
    }

    // ------------------------------------------------------------------------
    // Phase 7: Culprit Module Attribution & Callstack Unwinding
    // ------------------------------------------------------------------------
    {
        auto& dumpEngine = crashdump::TitanCrashDumpEngine::Instance();
        // Fault in culprit_faulty.sys (base 0xFFFFF80005000000)
        uint64_t faultyRip = 0xFFFFF80005002400ULL;
        crashdump::CONTEXT_AMD64 culpritCtx{};
        culpritCtx.Rip = faultyRip;
        culpritCtx.Rsp = 0xFFFFD00030001000ULL;
        culpritCtx.Rbp = 0xFFFFD00030001080ULL;

        dumpEngine.executeBugCheck(
            crashdump::KMODE_EXCEPTION_NOT_HANDLED,
            0xC0000005,
            faultyRip,
            0,
            0,
            &culpritCtx
        );

        auto telem = dumpEngine.getTelemetry();
        TEST_ASSERT(telem.lastCulpritModule == "culprit_faulty.sys", "Culprit module must be resolved to culprit_faulty.sys");

        const std::string& bsodText = dumpEngine.getLastBsodText();
        TEST_ASSERT(bsodText.find("WHAT FAILED:   culprit_faulty.sys") != std::string::npos, "BSOD text must display culprit driver name");
        TEST_ASSERT(bsodText.find("KMODE_EXCEPTION_NOT_HANDLED") != std::string::npos, "BSOD text must display stop code name");
    }

    // ------------------------------------------------------------------------
    // Phase 8: Blue Screen of Death (BSOD) Text & Sovereign Multi-Theme Rendering
    // ------------------------------------------------------------------------
    {
        auto& dumpEngine = crashdump::TitanCrashDumpEngine::Instance();
        const std::string& bsod = dumpEngine.getLastBsodText();

        TEST_ASSERT(bsod.find("[ ◆ ]") != std::string::npos || bsod.find("A kernel exception occurred") != std::string::npos,
                    "BSOD text must contain sovereign kernel exception notice");
        TEST_ASSERT(bsod.find("QR DIAGNOSTIC CODE") != std::string::npos, "BSOD text must contain QR diagnostic code");
        TEST_ASSERT(bsod.find("https://micant.org/kernel/stopcode") != std::string::npos, "BSOD text must contain diagnostic URL");
        TEST_ASSERT(bsod.find("PROCESSOR CONTEXT (AMD64)") != std::string::npos, "BSOD text must render register context");
        TEST_ASSERT(bsod.find("CALLSTACK UNWINDING TRACE") != std::string::npos, "BSOD text must render callstack trace");

        // Verify Default Theme (Midnight Obsidian #0B101B)
        TEST_ASSERT(dumpEngine.getCrashScreenTheme() == crashdump::CrashScreenTheme::MidnightObsidian,
                    "Default crash theme must be Midnight Obsidian");
        auto midnightRaster = crashdump::BsodRenderer::RenderFramebufferRaster(640, 480, crashdump::CrashScreenTheme::MidnightObsidian);
        TEST_ASSERT(midnightRaster.size() == 640 * 480, "Framebuffer raster size must be 640x480 pixels");
        TEST_ASSERT(midnightRaster[0] == 0xFF0B101B, "Midnight Obsidian raster background must be #0B101B");

        // Verify Dynamic Theme Switch to Emerald Prism (#03251E)
        crashdump::KeSetCrashScreenTheme(crashdump::CrashScreenTheme::EmeraldPrism);
        TEST_ASSERT(crashdump::KeGetCrashScreenTheme() == crashdump::CrashScreenTheme::EmeraldPrism,
                    "Crash screen theme must switch to Emerald Prism");
        auto emeraldRaster = crashdump::BsodRenderer::RenderFramebufferRaster(640, 480, crashdump::CrashScreenTheme::EmeraldPrism);
        TEST_ASSERT(emeraldRaster[0] == 0xFF03251E, "Emerald Prism raster background must be #03251E");

        // Trigger a crash under Emerald Prism
        dumpEngine.executeBugCheck(crashdump::CRITICAL_PROCESS_DIED, 0x2000, 0, 0, 0);
        const std::string& emeraldBsod = dumpEngine.getLastBsodText();
        TEST_ASSERT(emeraldBsod.find("EMERALD PRISM") != std::string::npos, "Emerald BSOD text must indicate Emerald Prism theme");

        // Verify Royal Navy (#0A192F)
        auto navyRaster = crashdump::BsodRenderer::RenderFramebufferRaster(640, 480, crashdump::CrashScreenTheme::RoyalNavy);
        TEST_ASSERT(navyRaster[0] == 0xFF0A192F, "Royal Navy raster background must be #0A192F");

        // Reset theme back to Midnight Obsidian
        crashdump::KeSetCrashScreenTheme(crashdump::CrashScreenTheme::MidnightObsidian);
        TEST_ASSERT(crashdump::KeGetCrashScreenTheme() == crashdump::CrashScreenTheme::MidnightObsidian,
                    "Crash screen theme must reset to Midnight Obsidian");
    }

    // ------------------------------------------------------------------------
    // Phase 9: Direct Sector Disk Sink & Minidump Image Serialization
    // ------------------------------------------------------------------------
    {
        auto& dumpEngine = crashdump::TitanCrashDumpEngine::Instance();
        size_t diskBytes = dumpEngine.getSimulatedDiskBytesWritten();
        TEST_ASSERT(diskBytes > sizeof(crashdump::DUMP_HEADER64), "Direct disk sink must have written minidump sectors to storage");

        // Low-level IoWriteCrashDump API verification
        uint8_t triageBlock[512];
        std::memset(triageBlock, 0xEE, sizeof(triageBlock));
        size_t written = 0;
        NtStatus stDump = crashdump::IoWriteCrashDump(crashdump::DUMP_TYPE_TRIAGE, triageBlock, sizeof(triageBlock), &written);
        TEST_ASSERT(stDump == NtStatus::Success, "IoWriteCrashDump direct block write must succeed");
        TEST_ASSERT(written == sizeof(triageBlock), "IoWriteCrashDump written size must match 512 bytes");
    }

    // ------------------------------------------------------------------------
    // Phase 10: TrapEngine Hook Integration & Subsystem Diagnostic Telemetry
    // ------------------------------------------------------------------------
    {
        auto& trap = ke::TrapEngine::get();
        // Trigger BugCheck through TrapEngine API
        trap.keBugCheckEx(crashdump::CRITICAL_PROCESS_DIED, 0x1000, 0, 0, 0);

        crashdump::BUGCHECK_TELEMETRY telem{};
        BOOL telemOk = crashdump::KeGetBugCheckTelemetry(&telem);
        TEST_ASSERT(telemOk == crashdump::TRUE_VAL, "KeGetBugCheckTelemetry must succeed");
        TEST_ASSERT(telem.totalBugChecks >= 5, "totalBugChecks must be >= 5 across test suite");
        TEST_ASSERT(telem.totalMiniDumps >= 5, "totalMiniDumps must be >= 5");
        TEST_ASSERT(telem.totalCallbacksInvoked >= 1, "totalCallbacksInvoked must be >= 1");
        TEST_ASSERT(telem.totalReasonCallbacksInvoked >= 1, "totalReasonCallbacksInvoked must be >= 1");
        TEST_ASSERT(telem.totalBytesDumped > 0, "totalBytesDumped must be > 0");
        TEST_ASSERT(telem.lastDumpValid == true, "lastDumpValid must be true");

        crashdump::BUGCHECK_TELEMETRY directTelem = crashdump::KeQueryBugCheckTelemetry();
        TEST_ASSERT(directTelem.totalBugChecks == telem.totalBugChecks, "Direct telemetry query must match");
    }

    std::cout << "[TEST] Suite 255: Horizon 39 - Sovereign Kernel Crash Dump Engine, BugCheck & BSOD PASSED.\n";
}

// ============================================================================
// Suite 256: Horizon 40 - USB 3.2 xHCI Host Controller Driver, Root Hub & URB Pipeline
// ============================================================================

void Test_Horizon40_USB32_xHCI_HostController_And_HubArchitecture() {
    using namespace micant::usb;
    std::cout << "[TEST] Running Suite 256: Horizon 40 - USB 3.2 xHCI Host Controller Driver, Root Hub & URB Pipeline...\n" << std::flush;

    // ------------------------------------------------------------------------
    // Phase 1: Subsystem Initialization, JanusLDR Driver Exports & SCM/Version DB
    // ------------------------------------------------------------------------
    {
        km::xhci::InitializeXhciSubsystemExports();
        auto& xhciDriver = km::xhci::TitanXhciControllerDriver::Instance();
        xhciDriver.resetForTesting();

        // 1. JanusLDR Exports Verification (usbxhci.sys, usbd.sys, usbhub3.sys, winusb.dll)
        auto& ldr = ldr::DynamicLoader::get();
        TEST_ASSERT(ldr.getExport("usbxhci.sys", "XhciSubmitUrb") != nullptr, "usbxhci.sys XhciSubmitUrb must exist");
        TEST_ASSERT(ldr.getExport("usbxhci.sys", "XhciGetTelemetry") != nullptr, "usbxhci.sys XhciGetTelemetry must exist");

        TEST_ASSERT(ldr.getExport("usbd.sys", "USBD_UrbAllocate") != nullptr, "usbd.sys USBD_UrbAllocate must exist");
        TEST_ASSERT(ldr.getExport("usbd.sys", "USBD_UrbFree") != nullptr, "usbd.sys USBD_UrbFree must exist");
        TEST_ASSERT(ldr.getExport("usbd.sys", "USBD_CreateConfigurationRequestEx") != nullptr, "usbd.sys USBD_CreateConfigurationRequestEx must exist");
        TEST_ASSERT(ldr.getExport("usbd.sys", "USBD_ParseConfigurationDescriptorEx") != nullptr, "usbd.sys USBD_ParseConfigurationDescriptorEx must exist");
        TEST_ASSERT(ldr.getExport("usbd.sys", "USBD_CalculateUsbBandwidth") != nullptr, "usbd.sys USBD_CalculateUsbBandwidth must exist");
        TEST_ASSERT(ldr.getExport("usbd.sys", "USBD_QueryBusTime") != nullptr, "usbd.sys USBD_QueryBusTime must exist");

        TEST_ASSERT(ldr.getExport("usbhub3.sys", "XhciSubmitUrb") != nullptr, "usbhub3.sys XhciSubmitUrb must exist");
        TEST_ASSERT(ldr.getExport("usbhub3.sys", "XhciGetTelemetry") != nullptr, "usbhub3.sys XhciGetTelemetry must exist");

        TEST_ASSERT(ldr.getExport("winusb.dll", "WinUsb_Initialize") != nullptr, "winusb.dll WinUsb_Initialize must exist");
        TEST_ASSERT(ldr.getExport("winusb.dll", "WinUsb_Free") != nullptr, "winusb.dll WinUsb_Free must exist");

        // 2. SCM Service Records Verification
        auto& scm = scm::ServiceControlManager::get();
        auto xhciSvc = scm.getServiceRecord(L"usbxhci");
        TEST_ASSERT(xhciSvc != nullptr, "usbxhci service record must exist in SCM");
        TEST_ASSERT(xhciSvc->serviceType == scm::SERVICE_KERNEL_DRIVER, "usbxhci must be a kernel driver");
        TEST_ASSERT(xhciSvc->startType == scm::SERVICE_BOOT_START, "usbxhci must be boot start");
        TEST_ASSERT(xhciSvc->status.dwCurrentState == scm::SERVICE_RUNNING, "usbxhci must be running");

        auto hubSvc = scm.getServiceRecord(L"usbhub3");
        TEST_ASSERT(hubSvc != nullptr, "usbhub3 service record must exist in SCM");
        TEST_ASSERT(hubSvc->serviceType == scm::SERVICE_KERNEL_DRIVER, "usbhub3 must be a kernel driver");

        // 3. Version Database Verification
        auto& verDb = version::VersionDatabase::Instance();
        TEST_ASSERT(verDb.GetModuleInfo("usbxhci.sys") != nullptr, "usbxhci.sys must be in Version Database");
        TEST_ASSERT(verDb.GetModuleInfo("usbhub3.sys") != nullptr, "usbhub3.sys must be in Version Database");
        TEST_ASSERT(verDb.GetModuleInfo("usbd.sys") != nullptr, "usbd.sys must be in Version Database");
    }

    // ------------------------------------------------------------------------
    // Phase 2: xHCI MMIO Capability, Operational & Runtime Registers
    // ------------------------------------------------------------------------
    {
        auto& xhciDriver = km::xhci::TitanXhciControllerDriver::Instance();
        const auto& caps = xhciDriver.getCapabilityRegisters();
        TEST_ASSERT(caps.CapLength == 0x20, "Capability length must be 32 bytes (0x20)");
        TEST_ASSERT(caps.HciVersion == 0x0120, "xHCI version must be 1.2 (0x0120)");
        TEST_ASSERT((caps.HcsParams1 & 0xFF) == 32, "MaxSlots must be 32");
        TEST_ASSERT(((caps.HcsParams1 >> 24) & 0xFF) == 8, "MaxPorts must be 8");
        TEST_ASSERT(caps.Dboff == 0x1000, "Doorbell offset must be 0x1000");
        TEST_ASSERT(caps.Rtsoff == 0x2000, "Runtime registers offset must be 0x2000");

        const auto& op = xhciDriver.getOperationalRegisters();
        TEST_ASSERT((op.UsbCmd & 0x01) != 0, "USBCMD Run/Stop bit must be 1 (Controller Running)");
        TEST_ASSERT((op.UsbSts & 0x01) == 0, "USBSTS HCHalted bit must be 0");
        TEST_ASSERT(op.PageSize == 1, "PageSize bit 0 must be 1 (4KB pages)");
        TEST_ASSERT(op.Config == 32, "CONFIG MaxSlotsEn must be 32");

        const auto& intr = xhciDriver.getRuntimeInterrupter();
        TEST_ASSERT((intr.Iman & 0x01) != 0, "Interrupter Pending (IP) must be 1");
        TEST_ASSERT((intr.Iman & 0x02) != 0, "Interrupter Enable (IE) must be 1");
        TEST_ASSERT(intr.ErstSz == 16, "ERST Size must be 16");
    }

    // ------------------------------------------------------------------------
    // Phase 3: Command Ring, Event Ring & Interrupter Management
    // ------------------------------------------------------------------------
    {
        auto primaryHc = usb::TitanUsbSubsystem::Instance().getPrimaryController();
        TEST_ASSERT(primaryHc != nullptr, "Primary xHCI Host Controller must exist");

        // Submit EnableSlotCmd to allocate a slot
        usb::xhci::TRB enableCmd{};
        enableCmd.setType(usb::xhci::TrbType::EnableSlotCmd);
        usb::xhci::TRB compEvent{};
        bool cmdOk = primaryHc->submitCommand(enableCmd, compEvent);
        TEST_ASSERT(cmdOk, "Submitting EnableSlotCmd must succeed");
        TEST_ASSERT(compEvent.getType() == usb::xhci::TrbType::CommandCompletionEvent, "Must emit CommandCompletionEvent");
        TEST_ASSERT(compEvent.getCompletionCode() == usb::xhci::CompCode::Success, "CompletionCode must be Success (1)");
        uint8_t slotId = compEvent.getSlotId();
        TEST_ASSERT(slotId > 0 && slotId <= 32, "Allocated slot ID must be in range 1..32");

        // Submit AddressDeviceCmd on attached device (Slot 1)
        usb::xhci::TRB addrCmd{};
        addrCmd.setType(usb::xhci::TrbType::AddressDeviceCmd);
        addrCmd.setSlotId(1);
        cmdOk = primaryHc->submitCommand(addrCmd, compEvent);
        TEST_ASSERT(cmdOk && compEvent.getCompletionCode() == usb::xhci::CompCode::Success, "AddressDeviceCmd must complete successfully");

        // Submit ConfigureEndpointCmd on attached device (Slot 1)
        usb::xhci::TRB cfgCmd{};
        cfgCmd.setType(usb::xhci::TrbType::ConfigureEndpointCmd);
        cfgCmd.setSlotId(1);
        cmdOk = primaryHc->submitCommand(cfgCmd, compEvent);
        TEST_ASSERT(cmdOk && compEvent.getCompletionCode() == usb::xhci::CompCode::Success, "ConfigureEndpointCmd must complete successfully");
    }

    // ------------------------------------------------------------------------
    // Phase 4: Device Context Base Address Array (DCBAA), Slot & Endpoint Contexts
    // ------------------------------------------------------------------------
    {
        usb::xhci::SlotContext slotCtx{};
        slotCtx.setSpeed(4); // SuperSpeed (5 Gbps)
        slotCtx.setRootHubPort(1);
        slotCtx.setDeviceAddress(1);
        slotCtx.setSlotState(3); // Configured state

        TEST_ASSERT(slotCtx.getSpeed() == 4, "SlotContext speed must be SuperSpeed");
        TEST_ASSERT(slotCtx.getRootHubPort() == 1, "SlotContext RootHubPort must be 1");
        TEST_ASSERT(slotCtx.getDeviceAddress() == 1, "SlotContext device address must be 1");
        TEST_ASSERT(slotCtx.getSlotState() == 3, "SlotContext slot state must be Configured");

        usb::xhci::EndpointContext ep0Ctx{};
        ep0Ctx.setEpState(1); // Running
        ep0Ctx.setEpType(4);  // Control Bi-directional
        ep0Ctx.setMaxPacketSize(512);

        TEST_ASSERT(ep0Ctx.getEpState() == 1, "EP0 state must be Running");
        TEST_ASSERT(ep0Ctx.getEpType() == 4, "EP0 type must be Control Bi-directional");
        TEST_ASSERT(ep0Ctx.getMaxPacketSize() == 512, "EP0 max packet size must be 512");

        usb::xhci::DeviceContext devCtx{};
        devCtx.slot = slotCtx;
        devCtx.endpoints[0] = ep0Ctx;
        TEST_ASSERT(sizeof(usb::xhci::DeviceContext) == 32 * 32, "DeviceContext must be exactly 1024 bytes (32 * 32-byte contexts)");
    }

    // ------------------------------------------------------------------------
    // Phase 5: Root Hub Topology, Port Status FSM & Speed Negotiation
    // ------------------------------------------------------------------------
    {
        auto& xhciDriver = km::xhci::TitanXhciControllerDriver::Instance();
        uint32_t port0 = xhciDriver.readPortSC(0);
        TEST_ASSERT((port0 & 0x01) != 0, "Port 0 Current Connect Status (CCS) must be 1 (Connected)");
        TEST_ASSERT((port0 & 0x02) != 0, "Port 0 Port Enabled/Disabled (PED) must be 1 (Enabled)");
        TEST_ASSERT((port0 & 0x200) != 0, "Port 0 Port Power (PP) must be 1 (Powered)");

        // Trigger Root Hub Port Reset
        bool resetOk = xhciDriver.resetPort(0);
        TEST_ASSERT(resetOk, "Resetting Root Hub Port 0 must succeed");

        auto telem = xhciDriver.getTelemetry();
        TEST_ASSERT(telem.totalPortResets >= 1, "totalPortResets must be >= 1");
    }

    // ------------------------------------------------------------------------
    // Phase 6: USB Request Block (URB) Architecture & IOCTL Dispatch Pipeline
    // ------------------------------------------------------------------------
    {
        auto& xhciDriver = km::xhci::TitanXhciControllerDriver::Instance();

        // 1. Control Transfer URB: GET_STATUS
        km::xhci::URB* urbControl = km::xhci::USBD_UrbAllocate();
        TEST_ASSERT(urbControl != nullptr, "USBD_UrbAllocate must return valid URB pointer");

        urbControl->UrbHeader.Function = km::xhci::URB_FUNCTION_CONTROL_TRANSFER_EX;
        uint8_t statusBuf[2]{0};
        auto* ct = &urbControl->UrbControlTransferEx;
        ct->TransferBuffer = statusBuf;
        ct->TransferBufferLength = sizeof(statusBuf);

        usb::USB_DEFAULT_PIPE_SETUP_PACKET setup{};
        setup.bmRequestType = usb::RequestType::Standard | usb::RequestType::RecipientDevice | usb::RequestType::DirectionIn;
        setup.bRequest = usb::StandardRequest::GET_STATUS;
        setup.wLength = 2;
        std::memcpy(ct->SetupPacket, &setup, sizeof(setup));

        NtStatus stUrb = km::xhci::XhciSubmitUrb(urbControl, 1);
        TEST_ASSERT(stUrb == NtStatus::Success, "Submitting Control Transfer URB must succeed");
        TEST_ASSERT(urbControl->UrbHeader.Status == km::xhci::USBD_STATUS_SUCCESS, "URB status must be USBD_STATUS_SUCCESS");
        TEST_ASSERT(ct->TransferBufferLength == 2, "Transferred length must be 2 bytes");
        TEST_ASSERT((statusBuf[0] & 0x01) != 0, "Device must report self-powered status");

        km::xhci::USBD_UrbFree(urbControl);

        // 2. Bulk Transfer URB: SCSI INQUIRY on Mass Storage (Slot 1)
        km::xhci::URB* urbBulkInq = km::xhci::USBD_UrbAllocate();
        TEST_ASSERT(urbBulkInq != nullptr, "USBD_UrbAllocate must succeed");

        urbBulkInq->UrbHeader.Function = km::xhci::URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER;
        uint8_t inqCbw[31]{};
        *reinterpret_cast<uint32_t*>(inqCbw) = 0x43425355; // 'USBC'
        *reinterpret_cast<uint32_t*>(inqCbw + 4) = 0x11223344;
        *reinterpret_cast<uint32_t*>(inqCbw + 8) = 36;
        inqCbw[12] = 0x80; // Data IN
        inqCbw[14] = 6;
        inqCbw[15] = 0x12; // INQUIRY
        inqCbw[19] = 36;

        auto* bi = &urbBulkInq->UrbBulkOrInterruptTransfer;
        bi->TransferBuffer = inqCbw;
        bi->TransferBufferLength = sizeof(inqCbw);
        bi->TransferFlags = km::xhci::USBD_TRANSFER_DIRECTION_OUT;

        stUrb = km::xhci::XhciSubmitUrb(urbBulkInq, 1);
        TEST_ASSERT(stUrb == NtStatus::Success, "Submitting SCSI INQUIRY CBW URB must succeed");
        TEST_ASSERT(bi->TransferBufferLength == 31, "CBW transferred length must be 31 bytes");

        // Read Inquiry Response
        uint8_t inqData[36]{};
        bi->TransferBuffer = inqData;
        bi->TransferBufferLength = sizeof(inqData);
        bi->TransferFlags = km::xhci::USBD_TRANSFER_DIRECTION_IN;

        stUrb = km::xhci::XhciSubmitUrb(urbBulkInq, 1);
        TEST_ASSERT(stUrb == NtStatus::Success, "Reading SCSI INQUIRY Data URB must succeed");
        TEST_ASSERT(bi->TransferBufferLength == 36, "Inquiry data length must be 36 bytes");
        TEST_ASSERT(std::memcmp(&inqData[8], "SanDisk ", 8) == 0, "SCSI Vendor identification must match SanDisk");

        km::xhci::USBD_UrbFree(urbBulkInq);
    }

    // ------------------------------------------------------------------------
    // Phase 7: Scatter-Gather Chained TRBs & Multi-Page DMA Buffer Fragments
    // ------------------------------------------------------------------------
    {
        std::vector<km::xhci::SCATTER_GATHER_ELEMENT> sgList = {
            {0x10001000ULL, 4096}, // Page 0
            {0x10002000ULL, 4096}, // Page 1
            {0x10003000ULL, 2048}  // Partial Page 2
        };

        auto trbs = km::xhci::XhciScatterGatherBuilder::BuildChainedTransferTrbs(sgList, false, true);
        TEST_ASSERT(trbs.size() == 3, "Chained TRB count must match scatter-gather entries (3)");

        // First two TRBs must have Chain bit (CH=1) set
        TEST_ASSERT((trbs[0].control & 0x10) != 0, "First TRB must have Chain bit (CH=1)");
        TEST_ASSERT(trbs[0].getTransferLength() == 4096, "First TRB length must be 4096");
        TEST_ASSERT(trbs[0].getIoc() == false, "Intermediate TRB must not have IOC set");

        TEST_ASSERT((trbs[1].control & 0x10) != 0, "Second TRB must have Chain bit (CH=1)");
        TEST_ASSERT(trbs[1].getTransferLength() == 4096, "Second TRB length must be 4096");

        // Terminal TRB must NOT have Chain bit and MUST have IOC bit
        TEST_ASSERT((trbs[2].control & 0x10) == 0, "Terminal TRB must not have Chain bit set");
        TEST_ASSERT(trbs[2].getTransferLength() == 2048, "Terminal TRB length must be 2048");
        TEST_ASSERT(trbs[2].getIoc() == true, "Terminal TRB must have IOC=1 for interrupt dispatch");
    }

    // ------------------------------------------------------------------------
    // Phase 8: PnP DevNode Hierarchy, Enumeration & Hardware IDs
    // ------------------------------------------------------------------------
    {
        auto& xhciDriver = km::xhci::TitanXhciControllerDriver::Instance();
        const auto& nodes = xhciDriver.getDevNodes();
        TEST_ASSERT(nodes.size() >= 5, "DevNode tree must contain Controller, RootHub, and at least 3 child devices");

        // Verify PCI Controller Node
        TEST_ASSERT(nodes[0].DeviceName == "\\Device\\USBPDO-0", "Node 0 must be PCI Host Controller");
        TEST_ASSERT(nodes[0].HardwareId.find(L"PCI\\VEN_8086&DEV_02ED") != std::wstring::npos, "PCI HWID must match Intel xHCI");

        // Verify Root Hub Node
        TEST_ASSERT(nodes[1].DeviceName == "\\Device\\USBRH0", "Node 1 must be Root Hub FDO");
        TEST_ASSERT(nodes[1].IsRootHub == true, "Node 1 IsRootHub must be true");
        TEST_ASSERT(nodes[1].HardwareId == L"USB\\ROOT_HUB30", "Root Hub HWID must be USB\\ROOT_HUB30");

        // Verify Child Downstream Devices
        auto flashNode = xhciDriver.findDevNodeBySlot(1);
        TEST_ASSERT(flashNode.has_value(), "Slot 1 DevNode must exist");
        TEST_ASSERT(flashNode->HardwareId.find(L"USB\\VID_0781&PID_5583") != std::wstring::npos, "Slot 1 must be SanDisk Flash Drive");

        auto mouseNode = xhciDriver.findDevNodeBySlot(2);
        TEST_ASSERT(mouseNode.has_value(), "Slot 2 DevNode must exist");
        TEST_ASSERT(mouseNode->HardwareId.find(L"USB\\VID_046D&PID_C077") != std::wstring::npos, "Slot 2 must be Logitech Mouse");

        auto serialNode = xhciDriver.findDevNodeBySlot(3);
        TEST_ASSERT(serialNode.has_value(), "Slot 3 DevNode must exist");
        TEST_ASSERT(serialNode->HardwareId.find(L"USB\\VID_10C4&PID_EA60") != std::wstring::npos, "Slot 3 must be Silicon Labs Serial");
    }

    // ------------------------------------------------------------------------
    // Phase 9: Selective Suspend, Device Power FSM (D0 <-> D2 <-> D3) & Remote Wakeup
    // ------------------------------------------------------------------------
    {
        auto& xhciDriver = km::xhci::TitanXhciControllerDriver::Instance();

        // 1. Transition Slot 1 to D2 (Selective Suspend / U3)
        bool pwrOk = xhciDriver.setDevicePowerState(1, km::xhci::XhciDevicePowerState::PowerDeviceD2);
        TEST_ASSERT(pwrOk, "Setting Slot 1 to D2 Selective Suspend must succeed");

        auto nodeOpt = xhciDriver.findDevNodeBySlot(1);
        TEST_ASSERT(nodeOpt.has_value() && nodeOpt->PowerState == km::xhci::XhciDevicePowerState::PowerDeviceD2, "Slot 1 power state must be D2");

        uint32_t port0Sc = xhciDriver.readPortSC(0);
        uint8_t pls = static_cast<uint8_t>((port0Sc >> 5) & 0x0F);
        TEST_ASSERT(pls == 3, "Port Link State (PLS) must be 3 (U3 Suspended)");

        // 2. Trigger Remote Wakeup on Slot 1
        bool wakeOk = xhciDriver.triggerRemoteWakeup(1);
        TEST_ASSERT(wakeOk, "Remote Wakeup from D2 must succeed");

        nodeOpt = xhciDriver.findDevNodeBySlot(1);
        TEST_ASSERT(nodeOpt.has_value() && nodeOpt->PowerState == km::xhci::XhciDevicePowerState::PowerDeviceD0, "Slot 1 power state must return to D0");

        auto telem = xhciDriver.getTelemetry();
        TEST_ASSERT(telem.totalSelectiveSuspends >= 1, "totalSelectiveSuspends must be >= 1");
        TEST_ASSERT(telem.totalRemoteWakeups >= 1, "totalRemoteWakeups must be >= 1");
    }

    // ------------------------------------------------------------------------
    // Phase 10: USBD Helper APIs, WinUSB Client & Subsystem Diagnostic Telemetry
    // ------------------------------------------------------------------------
    {
        // 1. USBD Bandwidth Calculation
        uint32_t bwIsoch = km::xhci::USBD_CalculateUsbBandwidth(1024, 1, km::xhci::FALSE_VAL);
        TEST_ASSERT(bwIsoch > 1024 * 8, "Isochronous bandwidth must include protocol overhead");

        // 2. USBD Query Bus Time
        uint32_t frameNum = 0;
        uint32_t qRes = km::xhci::USBD_QueryBusTime(nullptr, &frameNum);
        TEST_ASSERT(qRes == 0, "USBD_QueryBusTime must return 0 (Success)");
        TEST_ASSERT(frameNum > 0, "Current frame number must be non-zero");

        // 3. WinUSB Userland Client API Integration
        WINUSB_INTERFACE_HANDLE winusbHandle = nullptr;
        BOOL initOk = WinUsb_Initialize(reinterpret_cast<win32::HANDLE>(1), &winusbHandle);
        TEST_ASSERT(initOk == TRUE, "WinUsb_Initialize on Slot 1 device must succeed");
        TEST_ASSERT(winusbHandle != nullptr, "Valid WinUSB interface handle must be returned");

        // Query Pipe 0
        WINUSB_PIPE_INFORMATION pipeInfo{};
        BOOL pipeOk = WinUsb_QueryPipe(winusbHandle, 0, 0, &pipeInfo);
        TEST_ASSERT(pipeOk == TRUE, "WinUsb_QueryPipe must succeed");
        TEST_ASSERT(pipeInfo.MaximumPacketSize == 512, "Pipe max packet size must be 512 for SuperSpeed");

        // Set & Get Pipe Policy
        uint32_t timeout = 5000;
        BOOL setPolOk = WinUsb_SetPipePolicy(winusbHandle, pipeInfo.PipeId, WinUsbPolicy::PIPE_TRANSFER_TIMEOUT, sizeof(timeout), &timeout);
        TEST_ASSERT(setPolOk == TRUE, "WinUsb_SetPipePolicy must succeed");

        uint32_t readTimeout = 0;
        ULONG lenOut = sizeof(readTimeout);
        BOOL getPolOk = WinUsb_GetPipePolicy(winusbHandle, pipeInfo.PipeId, WinUsbPolicy::PIPE_TRANSFER_TIMEOUT, &lenOut, &readTimeout);
        TEST_ASSERT(getPolOk == TRUE && readTimeout == 5000, "WinUsb_GetPipePolicy must retrieve set timeout 5000");

        WinUsb_Free(winusbHandle);

        // 4. Overall Subsystem Telemetry Validation
        km::xhci::XHCI_TELEMETRY telem{};
        BOOL telemOk = km::xhci::XhciGetTelemetry(&telem);
        TEST_ASSERT(telemOk == km::xhci::TRUE_VAL, "XhciGetTelemetry must succeed");
        TEST_ASSERT(telem.totalUrbsSubmitted >= 3, "totalUrbsSubmitted must be >= 3");
        TEST_ASSERT(telem.totalUrbsCompleted >= 3, "totalUrbsCompleted must be >= 3");
        TEST_ASSERT(telem.totalControlTransfers >= 1, "totalControlTransfers must be >= 1");
        TEST_ASSERT(telem.totalBulkTransfers >= 2, "totalBulkTransfers must be >= 2");
        TEST_ASSERT(telem.totalBytesTransferred > 0, "totalBytesTransferred must be > 0");
        TEST_ASSERT(telem.hostControllerRunning == true, "hostControllerRunning must be true");

        km::xhci::XHCI_TELEMETRY directTelem = km::xhci::XhciQueryTelemetry();
        TEST_ASSERT(directTelem.totalUrbsSubmitted == telem.totalUrbsSubmitted, "Direct telemetry query must match");
    }

    std::cout << "[TEST] Suite 256: Horizon 40 - USB 3.2 xHCI Host Controller Driver, Root Hub & URB Pipeline PASSED.\n";
}

// ============================================================================
// Suite 257: Horizon 41 - Human Interface Device (HID) Class Driver, Report Parser & Input Stack
// ============================================================================

void Test_Horizon41_HIDClass_ReportParser_And_KernelInputStack() {
    using namespace micant::km::hid;
    std::cout << "[TEST] Running Suite 257: Horizon 41 - HID Class Driver, Report Descriptor Parser & Kernel Input Stack...\n" << std::flush;

    // ------------------------------------------------------------------------
    // Phase 1: Subsystem Initialization, JanusLDR Exports & SCM/Version DB
    // ------------------------------------------------------------------------
    {
        InitializeHidSubsystemExports();
        auto& hidSys = TitanHidInputSubsystem::Instance();
        hidSys.resetForTesting();

        // 1. JanusLDR Exports Verification (hid.dll, hidparse.sys, hidclass.sys, hidusb.sys, kbdclass.sys, mouclass.sys, i8042prt.sys)
        auto& ldr = ldr::DynamicLoader::get();
        TEST_ASSERT(ldr.getExport("hid.dll", "HidD_GetHidGuid") != nullptr, "hid.dll HidD_GetHidGuid must exist");
        TEST_ASSERT(ldr.getExport("hid.dll", "HidD_GetAttributes") != nullptr, "hid.dll HidD_GetAttributes must exist");
        TEST_ASSERT(ldr.getExport("hid.dll", "HidD_GetPreparsedData") != nullptr, "hid.dll HidD_GetPreparsedData must exist");
        TEST_ASSERT(ldr.getExport("hid.dll", "HidD_FreePreparsedData") != nullptr, "hid.dll HidD_FreePreparsedData must exist");
        TEST_ASSERT(ldr.getExport("hid.dll", "HidD_GetFeature") != nullptr, "hid.dll HidD_GetFeature must exist");
        TEST_ASSERT(ldr.getExport("hid.dll", "HidD_SetFeature") != nullptr, "hid.dll HidD_SetFeature must exist");
        TEST_ASSERT(ldr.getExport("hid.dll", "HidP_GetCaps") != nullptr, "hid.dll HidP_GetCaps must exist");
        TEST_ASSERT(ldr.getExport("hid.dll", "HidP_GetUsages") != nullptr, "hid.dll HidP_GetUsages must exist");
        TEST_ASSERT(ldr.getExport("hid.dll", "HidP_GetUsageValue") != nullptr, "hid.dll HidP_GetUsageValue must exist");

        TEST_ASSERT(ldr.getExport("hidparse.sys", "HidP_GetCaps") != nullptr, "hidparse.sys HidP_GetCaps must exist");
        TEST_ASSERT(ldr.getExport("hidparse.sys", "HidP_GetButtonCaps") != nullptr, "hidparse.sys HidP_GetButtonCaps must exist");
        TEST_ASSERT(ldr.getExport("hidparse.sys", "HidP_GetValueCaps") != nullptr, "hidparse.sys HidP_GetValueCaps must exist");

        TEST_ASSERT(ldr.getExport("hidclass.sys", "HidGetTelemetry") != nullptr, "hidclass.sys HidGetTelemetry must exist");
        TEST_ASSERT(ldr.getExport("hidusb.sys", "HidGetTelemetry") != nullptr, "hidusb.sys HidGetTelemetry must exist");
        TEST_ASSERT(ldr.getExport("kbdclass.sys", "KeyboardClassServiceCallback") != nullptr, "kbdclass.sys KeyboardClassServiceCallback must exist");
        TEST_ASSERT(ldr.getExport("mouclass.sys", "MouseClassServiceCallback") != nullptr, "mouclass.sys MouseClassServiceCallback must exist");
        TEST_ASSERT(ldr.getExport("i8042prt.sys", "I8042ReadPortUchar") != nullptr, "i8042prt.sys I8042ReadPortUchar must exist");
        TEST_ASSERT(ldr.getExport("i8042prt.sys", "I8042WritePortUchar") != nullptr, "i8042prt.sys I8042WritePortUchar must exist");

        // 2. SCM Service Records Verification
        auto& scm = scm::ServiceControlManager::get();
        auto hidClassSvc = scm.getServiceRecord(L"hidclass");
        TEST_ASSERT(hidClassSvc != nullptr, "hidclass service record must exist in SCM");
        TEST_ASSERT(hidClassSvc->serviceType == scm::SERVICE_KERNEL_DRIVER, "hidclass must be a kernel driver");
        TEST_ASSERT(hidClassSvc->startType == scm::SERVICE_BOOT_START, "hidclass must be boot start");
        TEST_ASSERT(hidClassSvc->status.dwCurrentState == scm::SERVICE_RUNNING, "hidclass must be running");

        TEST_ASSERT(scm.getServiceRecord(L"hidparse") != nullptr, "hidparse service record must exist");
        TEST_ASSERT(scm.getServiceRecord(L"hidusb") != nullptr, "hidusb service record must exist");
        TEST_ASSERT(scm.getServiceRecord(L"kbdclass") != nullptr, "kbdclass service record must exist");
        TEST_ASSERT(scm.getServiceRecord(L"mouclass") != nullptr, "mouclass service record must exist");
        TEST_ASSERT(scm.getServiceRecord(L"i8042prt") != nullptr, "i8042prt service record must exist");

        // 3. Version Database Verification
        auto& verDb = version::VersionDatabase::Instance();
        TEST_ASSERT(verDb.GetModuleInfo("hidclass.sys") != nullptr, "hidclass.sys must be in Version Database");
        TEST_ASSERT(verDb.GetModuleInfo("hidparse.sys") != nullptr, "hidparse.sys must be in Version Database");
        TEST_ASSERT(verDb.GetModuleInfo("hidusb.sys") != nullptr, "hidusb.sys must be in Version Database");
        TEST_ASSERT(verDb.GetModuleInfo("kbdclass.sys") != nullptr, "kbdclass.sys must be in Version Database");
        TEST_ASSERT(verDb.GetModuleInfo("mouclass.sys") != nullptr, "mouclass.sys must be in Version Database");
        TEST_ASSERT(verDb.GetModuleInfo("i8042prt.sys") != nullptr, "i8042prt.sys must be in Version Database");
        TEST_ASSERT(verDb.GetModuleInfo("hid.dll") != nullptr, "hid.dll must be in Version Database");
    }

    // ------------------------------------------------------------------------
    // Phase 2: HID Report Descriptor Bytecode Parser (hidparse.sys)
    // ------------------------------------------------------------------------
    {
        // 1. Parse Standard 6KRO Keyboard Report Descriptor
        win32::HANDLE hKbd = reinterpret_cast<win32::HANDLE>(0x4101);
        PHIDP_PREPARSED_DATA kbdPreparsed = nullptr;
        TEST_ASSERT(HidD_GetPreparsedData(hKbd, &kbdPreparsed) == TRUE_B, "HidD_GetPreparsedData on Keyboard must succeed");
        TEST_ASSERT(kbdPreparsed != nullptr, "Keyboard preparsed data must not be null");

        HIDP_CAPS kbdCaps{};
        TEST_ASSERT(HidP_GetCaps(kbdPreparsed, &kbdCaps) == HIDP_STATUS_SUCCESS, "HidP_GetCaps on Keyboard must succeed");
        TEST_ASSERT(kbdCaps.UsagePage == HID_USAGE_PAGE_GENERIC, "Keyboard UsagePage must be Generic Desktop (0x01)");
        TEST_ASSERT(kbdCaps.Usage == HID_USAGE_GENERIC_KEYBOARD, "Keyboard Usage must be Keyboard (0x06)");
        TEST_ASSERT(kbdCaps.InputReportByteLength == 9, "Keyboard InputReportByteLength must be 9 (1 ReportID + 8 payload bytes)");
        TEST_ASSERT(kbdCaps.OutputReportByteLength == 2, "Keyboard OutputReportByteLength must be 2 (1 ReportID + 1 LED byte)");
        TEST_ASSERT(kbdCaps.NumberInputButtonCaps == 2, "Keyboard must have 2 InputButtonCaps (Modifiers + 6KRO Array)");
        TEST_ASSERT(kbdCaps.NumberOutputButtonCaps == 1, "Keyboard must have 1 OutputButtonCap (5 LEDs)");

        TEST_ASSERT(HidD_FreePreparsedData(kbdPreparsed) == TRUE_B, "HidD_FreePreparsedData must succeed");

        // 2. Parse High-Precision 16-bit Gaming Mouse Report Descriptor
        win32::HANDLE hMou = reinterpret_cast<win32::HANDLE>(0x4102);
        PHIDP_PREPARSED_DATA mouPreparsed = nullptr;
        TEST_ASSERT(HidD_GetPreparsedData(hMou, &mouPreparsed) == TRUE_B, "HidD_GetPreparsedData on Mouse must succeed");

        HIDP_CAPS mouCaps{};
        TEST_ASSERT(HidP_GetCaps(mouPreparsed, &mouCaps) == HIDP_STATUS_SUCCESS, "HidP_GetCaps on Mouse must succeed");
        TEST_ASSERT(mouCaps.UsagePage == HID_USAGE_PAGE_GENERIC, "Mouse UsagePage must be Generic Desktop (0x01)");
        TEST_ASSERT(mouCaps.Usage == HID_USAGE_GENERIC_MOUSE, "Mouse Usage must be Mouse (0x02)");
        TEST_ASSERT(mouCaps.InputReportByteLength == 7, "Mouse InputReportByteLength must be 7 (1 ReportID + 6 payload bytes)");
        TEST_ASSERT(mouCaps.NumberInputButtonCaps == 1, "Mouse must have 1 InputButtonCap (Buttons 1..5)");
        TEST_ASSERT(mouCaps.NumberInputValueCaps == 3, "Mouse must have 3 InputValueCaps (16-bit X, 16-bit Y, 8-bit Wheel)");

        USHORT vcapLen = mouCaps.NumberInputValueCaps;
        std::vector<HIDP_VALUE_CAPS> vcaps(vcapLen);
        TEST_ASSERT(HidP_GetValueCaps(HidP_Input, vcaps.data(), &vcapLen, mouPreparsed) == HIDP_STATUS_SUCCESS,
                    "HidP_GetValueCaps on Mouse must succeed");
        TEST_ASSERT(vcaps[0].NotRange.Usage == HID_USAGE_GENERIC_X && vcaps[0].BitSize == 16, "ValueCap 0 must be 16-bit X");
        TEST_ASSERT(vcaps[0].LogicalMin == -32767 && vcaps[0].LogicalMax == 32767, "X axis must have signed 16-bit logical bounds");
        TEST_ASSERT(vcaps[1].NotRange.Usage == HID_USAGE_GENERIC_Y && vcaps[1].BitSize == 16, "ValueCap 1 must be 16-bit Y");
        TEST_ASSERT(vcaps[2].NotRange.Usage == HID_USAGE_GENERIC_WHEEL && vcaps[2].BitSize == 8, "ValueCap 2 must be 8-bit Wheel");

        HidD_FreePreparsedData(mouPreparsed);
    }

    // ------------------------------------------------------------------------
    // Phase 3: HidP_* Report Extraction & Bitfield Packing Engine
    // ------------------------------------------------------------------------
    {
        // 1. Keyboard Input Report Unpacking & Output LED Packing
        win32::HANDLE hKbd = reinterpret_cast<win32::HANDLE>(0x4101);
        PHIDP_PREPARSED_DATA kbdData = nullptr;
        HidD_GetPreparsedData(hKbd, &kbdData);

        // Craft a 9-byte Keyboard Input Report:
        // Byte 0: ReportID = 0
        // Byte 1: Modifiers = 0x05 (Bit 0 LeftCtrl 0xE0 + Bit 2 LeftAlt 0xE2)
        // Byte 2: Reserved = 0x00
        // Bytes 3..8: Keycodes = [0x04 ('A'), 0x16 ('S'), 0x07 ('D'), 0, 0, 0]
        uint8_t kbdReport[9] = {0x00, 0x05, 0x00, 0x04, 0x16, 0x07, 0x00, 0x00, 0x00};
        USAGE pressedKeys[16]{0};
        ULONG keyCount = 16;
        HIDP_STATUS stU = HidP_GetUsages(
            HidP_Input, HID_USAGE_PAGE_KEYBOARD, 0, pressedKeys, &keyCount,
            kbdData, reinterpret_cast<PCHAR>(kbdReport), sizeof(kbdReport));
        TEST_ASSERT(stU == HIDP_STATUS_SUCCESS, "HidP_GetUsages on Keyboard report must succeed");
        TEST_ASSERT(keyCount == 5, "Must extract 2 modifier keys + 3 array keys = 5 usages");
        TEST_ASSERT(pressedKeys[0] == 0xE0 && pressedKeys[1] == 0xE2, "Modifiers must be LeftCtrl (0xE0) and LeftAlt (0xE2)");
        TEST_ASSERT(pressedKeys[2] == 0x04 && pressedKeys[3] == 0x16 && pressedKeys[4] == 0x07, "Key array must match A, S, D");

        // Pack Output LED report (NumLock = 0x01, CapsLock = 0x02)
        uint8_t ledReport[2] = {0x00, 0x00};
        USAGE ledsToSet[2] = {0x01, 0x02};
        ULONG ledCount = 2;
        HIDP_STATUS stLed = HidP_SetUsages(
            HidP_Output, HID_USAGE_PAGE_LED, 0, ledsToSet, &ledCount,
            kbdData, reinterpret_cast<PCHAR>(ledReport), sizeof(ledReport));
        TEST_ASSERT(stLed == HIDP_STATUS_SUCCESS, "HidP_SetUsages for Keyboard LEDs must succeed");
        TEST_ASSERT(ledReport[1] == 0x03, "LED Output Report payload byte must have bits 0 and 1 set (0x03)");

        HidD_FreePreparsedData(kbdData);

        // 2. Gaming Mouse 16-bit Signed X/Y & Wheel Extraction + Packing
        win32::HANDLE hMou = reinterpret_cast<win32::HANDLE>(0x4102);
        PHIDP_PREPARSED_DATA mouData = nullptr;
        HidD_GetPreparsedData(hMou, &mouData);

        uint8_t mouReport[7] = {0};
        USAGE mouseBtns[2] = {1, 3}; // Left + Middle button
        ULONG btnCount = 2;
        TEST_ASSERT(HidP_SetUsages(HidP_Input, HID_USAGE_PAGE_BUTTON, 0, mouseBtns, &btnCount,
                                   mouData, reinterpret_cast<PCHAR>(mouReport), sizeof(mouReport)) == HIDP_STATUS_SUCCESS,
                    "HidP_SetUsages on Mouse buttons must succeed");
        TEST_ASSERT(HidP_SetUsageValue(HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_X, static_cast<ULONG>(450),
                                       mouData, reinterpret_cast<PCHAR>(mouReport), sizeof(mouReport)) == HIDP_STATUS_SUCCESS,
                    "HidP_SetUsageValue on Mouse X (+450) must succeed");
        TEST_ASSERT(HidP_SetUsageValue(HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_Y, static_cast<ULONG>(static_cast<int32_t>(-125)),
                                       mouData, reinterpret_cast<PCHAR>(mouReport), sizeof(mouReport)) == HIDP_STATUS_SUCCESS,
                    "HidP_SetUsageValue on Mouse Y (-125) must succeed");
        TEST_ASSERT(HidP_SetUsageValue(HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_WHEEL, static_cast<ULONG>(static_cast<int32_t>(-3)),
                                       mouData, reinterpret_cast<PCHAR>(mouReport), sizeof(mouReport)) == HIDP_STATUS_SUCCESS,
                    "HidP_SetUsageValue on Mouse Wheel (-3) must succeed");

        // Now unpack and verify sign extension!
        ULONG rawX = 0, rawY = 0, rawWheel = 0;
        TEST_ASSERT(HidP_GetUsageValue(HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_X, &rawX,
                                       mouData, reinterpret_cast<PCHAR>(mouReport), sizeof(mouReport)) == HIDP_STATUS_SUCCESS,
                    "HidP_GetUsageValue for Mouse X must succeed");
        TEST_ASSERT(HidP_GetUsageValue(HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_Y, &rawY,
                                       mouData, reinterpret_cast<PCHAR>(mouReport), sizeof(mouReport)) == HIDP_STATUS_SUCCESS,
                    "HidP_GetUsageValue for Mouse Y must succeed");
        TEST_ASSERT(HidP_GetUsageValue(HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_WHEEL, &rawWheel,
                                       mouData, reinterpret_cast<PCHAR>(mouReport), sizeof(mouReport)) == HIDP_STATUS_SUCCESS,
                    "HidP_GetUsageValue for Mouse Wheel must succeed");

        TEST_ASSERT(static_cast<int32_t>(rawX) == 450, "Unpacked Mouse X must equal +450");
        TEST_ASSERT(static_cast<int32_t>(rawY) == -125, "Unpacked Mouse Y must sign-extend to -125");
        TEST_ASSERT(static_cast<int32_t>(rawWheel) == -3, "Unpacked Mouse Wheel must sign-extend to -3");

        HidD_FreePreparsedData(mouData);
    }

    // ------------------------------------------------------------------------
    // Phase 4: USB HID Miniport (hidusb.sys) & Ping-Pong URB Ring over xHCI
    // ------------------------------------------------------------------------
    {
        auto& hidSys = TitanHidInputSubsystem::Instance();
        win32::HANDLE hMou = reinterpret_cast<win32::HANDLE>(0x4102);

        // Queue an Interrupt IN report via Ping-Pong URB over usbxhci.sys
        uint8_t sampleMouseInput[7] = {0x00, 0x01, 0x20, 0x00, 0xF0, 0xFF, 0x01};
        NtStatus stPing = hidSys.submitPingPongInterruptReadUrb(hMou, sampleMouseInput, sizeof(sampleMouseInput));
        TEST_ASSERT(stPing == NtStatus::Success, "Ping-Pong Interrupt Read URB over xHCI must succeed");

        // Read the queued report via HidD_GetInputReport (GET_REPORT control transfer URB)
        uint8_t readBuf[7] = {0};
        BOOLEAN getInOk = HidD_GetInputReport(hMou, readBuf, sizeof(readBuf));
        TEST_ASSERT(getInOk == TRUE_B, "HidD_GetInputReport must succeed");
        TEST_ASSERT(readBuf[1] == 0x01 && readBuf[2] == 0x20, "Retrieved input report bytes must match Ping-Pong frame");
    }

    // ------------------------------------------------------------------------
    // Phase 5: Top-Level Collection (TLC) PDO Enumeration & Device Interfaces
    // ------------------------------------------------------------------------
    {
        auto& hidSys = TitanHidInputSubsystem::Instance();
        auto collections = hidSys.getAllCollections();
        TEST_ASSERT(collections.size() == 3, "Must enumerate 3 Top-Level Collections (Keyboard, Mouse, Gamepad)");

        HID_GUID hidGuid{};
        HidD_GetHidGuid(&hidGuid);
        TEST_ASSERT(hidGuid == GUID_DEVINTERFACE_HID, "HidD_GetHidGuid must return canonical GUID_DEVINTERFACE_HID");

        // Verify Gamepad Collection (0x4103)
        win32::HANDLE hPad = reinterpret_cast<win32::HANDLE>(0x4103);
        HIDD_ATTRIBUTES padAttr{};
        TEST_ASSERT(HidD_GetAttributes(hPad, &padAttr) == TRUE_B, "HidD_GetAttributes on Gamepad must succeed");
        TEST_ASSERT(padAttr.VendorID == 0x045E && padAttr.ProductID == 0x0B12, "Gamepad VID/PID must match 045E:0B12");

        HID_COLLECTION_INFORMATION colInfo{};
        TEST_ASSERT(HidD_GetCollectionInformation(hPad, &colInfo, sizeof(colInfo)) == TRUE_B,
                    "HidD_GetCollectionInformation must succeed");
        TEST_ASSERT(colInfo.VendorID == 0x045E && colInfo.ProductID == 0x0B12, "Collection info VID/PID must match");
    }

    // ------------------------------------------------------------------------
    // Phase 6: Keyboard Class Driver (kbdclass.sys) & KEYBOARD_INPUT_DATA Ring
    // ------------------------------------------------------------------------
    {
        auto& hidSys = TitanHidInputSubsystem::Instance();

        KEYBOARD_ATTRIBUTES kbdAttr{};
        NtStatus stAttr = hidSys.dispatchKeyboardIoctl(IOCTL_KEYBOARD_QUERY_ATTRIBUTES, nullptr, 0, &kbdAttr, sizeof(kbdAttr));
        TEST_ASSERT(stAttr == NtStatus::Success, "IOCTL_KEYBOARD_QUERY_ATTRIBUTES must succeed");
        TEST_ASSERT(kbdAttr.NumberOfKeysTotal == 104, "Keyboard must report 104 keys");
        TEST_ASSERT(kbdAttr.NumberOfFunctionKeys == 12, "Keyboard must report 12 function keys");

        // Set & Query LED Indicators (CapsLock + NumLock)
        KEYBOARD_INDICATOR_PARAMETERS setInd{0, KEYBOARD_CAPS_LOCK_ON | KEYBOARD_NUM_LOCK_ON};
        TEST_ASSERT(hidSys.dispatchKeyboardIoctl(IOCTL_KEYBOARD_SET_INDICATORS, &setInd, sizeof(setInd), nullptr, 0) == NtStatus::Success,
                    "IOCTL_KEYBOARD_SET_INDICATORS must succeed");

        KEYBOARD_INDICATOR_PARAMETERS getInd{};
        TEST_ASSERT(hidSys.dispatchKeyboardIoctl(IOCTL_KEYBOARD_QUERY_INDICATORS, nullptr, 0, &getInd, sizeof(getInd)) == NtStatus::Success,
                    "IOCTL_KEYBOARD_QUERY_INDICATORS must succeed");
        TEST_ASSERT(getInd.LedFlags == (KEYBOARD_CAPS_LOCK_ON | KEYBOARD_NUM_LOCK_ON), "Queried LED indicators must match CapsLock + NumLock");

        // Inject KEYBOARD_INPUT_DATA packets via KeyboardClassServiceCallback
        KEYBOARD_INPUT_DATA kbdBatch[2]{};
        kbdBatch[0] = {0, 0x1E, KEY_MAKE, 0, 0};  // 'A' Make
        kbdBatch[1] = {0, 0x1E, KEY_BREAK, 0, 0}; // 'A' Break
        ULONG consumed = 0;
        KeyboardClassServiceCallback(kbdBatch, kbdBatch + 2, &consumed);
        TEST_ASSERT(consumed == 2, "KeyboardClassServiceCallback must consume 2 packets");
        TEST_ASSERT(hidSys.getKeyboardQueueDepth() == 2, "Keyboard ring buffer depth must be 2");
    }

    // ------------------------------------------------------------------------
    // Phase 7: Mouse Class Driver (mouclass.sys) & MOUSE_INPUT_DATA Ring
    // ------------------------------------------------------------------------
    {
        auto& hidSys = TitanHidInputSubsystem::Instance();

        MOUSE_ATTRIBUTES mouAttr{};
        TEST_ASSERT(hidSys.dispatchMouseIoctl(IOCTL_MOUSE_QUERY_ATTRIBUTES, &mouAttr, sizeof(mouAttr)) == NtStatus::Success,
                    "IOCTL_MOUSE_QUERY_ATTRIBUTES must succeed");
        TEST_ASSERT(mouAttr.NumberOfButtons == 5, "Mouse must report 5 buttons");
        TEST_ASSERT(mouAttr.SampleRate == 1000, "Mouse must report 1000 Hz sample rate");

        MOUSE_INPUT_DATA mouPkt{};
        mouPkt.UnitId      = 0;
        mouPkt.Flags       = MOUSE_MOVE_RELATIVE;
        mouPkt.ButtonFlags = MOUSE_LEFT_BUTTON_DOWN | MOUSE_WHEEL;
        mouPkt.ButtonData  = 120;
        mouPkt.LastX       = 64;
        mouPkt.LastY       = 32;

        ULONG consumed = 0;
        MouseClassServiceCallback(&mouPkt, &mouPkt + 1, &consumed);
        TEST_ASSERT(consumed == 1, "MouseClassServiceCallback must consume 1 packet");
        TEST_ASSERT(hidSys.getMouseQueueDepth() == 1, "Mouse ring buffer depth must be 1");
    }

    // ------------------------------------------------------------------------
    // Phase 8: PS/2 i8042 Controller Port Driver (i8042prt.sys)
    // ------------------------------------------------------------------------
    {
        auto& hidSys = TitanHidInputSubsystem::Instance();

        // 1. Execute i8042 Controller Self-Test (Command 0xAA on Port 0x64 -> 0x55 on Port 0x60)
        I8042WritePortUchar(I8042_COMMAND_PORT, I8042_CMD_SELF_TEST);
        uint8_t status = I8042ReadPortUchar(I8042_STATUS_PORT);
        TEST_ASSERT((status & I8042_STS_OBF) != 0, "i8042 Status Register OBF bit must be 1 after self-test");
        uint8_t selfTestResult = I8042ReadPortUchar(I8042_DATA_PORT);
        TEST_ASSERT(selfTestResult == 0x55, "i8042 Self-Test must return 0x55");

        // 2. Inject Extended E0 Scancode via IRQ1 (Right Arrow: 0xE0, 0x4D)
        hidSys.i8042InjectKeyboardIrq1(0xE0);
        hidSys.i8042InjectKeyboardIrq1(0x4D);
        TEST_ASSERT(hidSys.getKeyboardQueueDepth() == 3, "Keyboard queue must now contain 3 packets (2 from Phase 6 + 1 from IRQ1)");

        // 3. Inject PS/2 Mouse IRQ12 4-Byte IntelliMouse Packet
        hidSys.i8042InjectMouseIrq12Packet(0x09 /* Left button + bit 3 */, 15, -10, 1);
        TEST_ASSERT(hidSys.getMouseQueueDepth() == 2, "Mouse queue must now contain 2 packets");
    }

    // ------------------------------------------------------------------------
    // Phase 9: Kernel Raw Input Thread (RIT) -> Win32k Window Message Bridge
    // ------------------------------------------------------------------------
    {
        auto& w32k = win32k::TitanWin32kSubsystem::Instance();
        w32k.initialize();

        win32k::HWND hwnd = w32k.createWindowEx(
            0, L"MicaHidTestClass", L"HID Input Test Window",
            win32k::WS_OVERLAPPEDWINDOW | win32k::WS_VISIBLE,
            100, 100, 800, 600, nullptr, nullptr, nullptr, nullptr);
        TEST_ASSERT(hwnd != nullptr, "Creating test window in Win32k must succeed");

        auto& hidSys = TitanHidInputSubsystem::Instance();
        uint32_t injected = hidSys.pumpRawInputToWin32k(hwnd);
        TEST_ASSERT(injected >= 5, "Raw Input Thread must inject queued keyboard and mouse messages into Win32k");
        TEST_ASSERT(hidSys.getKeyboardQueueDepth() == 0, "Keyboard ring buffer must be completely drained");
        TEST_ASSERT(hidSys.getMouseQueueDepth() == 0, "Mouse ring buffer must be completely drained");

        // Verify message arrival in Win32k thread queue
        win32k::MSG msg{};
        BOOL gotMsg = w32k.peekMessage(msg, hwnd, 0, 0, win32k::PM_REMOVE, 1);
        TEST_ASSERT(gotMsg == 1, "Win32k queue must have received injected input message");

        w32k.destroyWindow(hwnd);
    }

    // ------------------------------------------------------------------------
    // Phase 10: User-Mode hid.dll Client API & Subsystem Diagnostic Telemetry
    // ------------------------------------------------------------------------
    {
        win32::HANDLE hKbd = reinterpret_cast<win32::HANDLE>(0x4101);
        win32::HANDLE hPad = reinterpret_cast<win32::HANDLE>(0x4103);

        wchar_t mfg[64]{0}, prod[64]{0}, serial[64]{0};
        TEST_ASSERT(HidD_GetManufacturerString(hKbd, mfg, sizeof(mfg)) == TRUE_B, "HidD_GetManufacturerString must succeed");
        TEST_ASSERT(HidD_GetProductString(hKbd, prod, sizeof(prod)) == TRUE_B, "HidD_GetProductString must succeed");
        TEST_ASSERT(HidD_GetSerialNumberString(hKbd, serial, sizeof(serial)) == TRUE_B, "HidD_GetSerialNumberString must succeed");
        TEST_ASSERT(std::wstring(prod) == L"MicaNT Prism Mechanical Keyboard", "Product string must match");
        TEST_ASSERT(std::wstring(serial) == L"MICA-KBD-0001", "Serial string must match");

        // Set & Get Input Buffer Queue Depth
        TEST_ASSERT(HidD_SetNumInputBuffers(hKbd, 64) == TRUE_B, "HidD_SetNumInputBuffers(64) must succeed");
        ULONG numBufs = 0;
        TEST_ASSERT(HidD_GetNumInputBuffers(hKbd, &numBufs) == TRUE_B && numBufs == 64, "HidD_GetNumInputBuffers must return 64");

        // Set & Get Feature Report on Gamepad
        uint8_t featSet[5] = {0x00, 0xAA, 0xBB, 0xCC, 0xDD};
        TEST_ASSERT(HidD_SetFeature(hPad, featSet, sizeof(featSet)) == TRUE_B, "HidD_SetFeature on Gamepad must succeed");

        uint8_t featGet[5] = {0x00, 0, 0, 0, 0};
        TEST_ASSERT(HidD_GetFeature(hPad, featGet, sizeof(featGet)) == TRUE_B, "HidD_GetFeature on Gamepad must succeed");
        TEST_ASSERT(featGet[1] == 0xAA && featGet[4] == 0xDD, "Retrieved Feature report payload must match set calibration bytes");

        // Set Output Report on Keyboard
        uint8_t outRep[2] = {0x00, 0x07};
        TEST_ASSERT(HidD_SetOutputReport(hKbd, outRep, sizeof(outRep)) == TRUE_B, "HidD_SetOutputReport must succeed");

        // Verify Overall Subsystem Telemetry
        HID_SUBSYSTEM_TELEMETRY telem{};
        TEST_ASSERT(HidGetTelemetry(&telem) == TRUE_VAL, "HidGetTelemetry must succeed");
        TEST_ASSERT(telem.totalDescriptorsParsed >= 3, "totalDescriptorsParsed must be >= 3");
        TEST_ASSERT(telem.totalHidInputReportsRead >= 2, "totalHidInputReportsRead must be >= 2");
        TEST_ASSERT(telem.totalHidOutputReportsWritten >= 2, "totalHidOutputReportsWritten must be >= 2");
        TEST_ASSERT(telem.totalHidFeatureTransfers >= 2, "totalHidFeatureTransfers must be >= 2");
        TEST_ASSERT(telem.totalPingPongUrbsDispatched >= 4, "totalPingPongUrbsDispatched must be >= 4");
        TEST_ASSERT(telem.totalKeyboardPacketsQueued >= 3, "totalKeyboardPacketsQueued must be >= 3");
        TEST_ASSERT(telem.totalMousePacketsQueued >= 2, "totalMousePacketsQueued must be >= 2");
        TEST_ASSERT(telem.totalI8042InterruptsServiced >= 3, "totalI8042InterruptsServiced must be >= 3");
        TEST_ASSERT(telem.totalWin32kMessagesInjected >= 5, "totalWin32kMessagesInjected must be >= 5");
        TEST_ASSERT(telem.activeTopLevelCollections == 3, "activeTopLevelCollections must be 3");
        TEST_ASSERT(telem.i8042ControllerHealthy == true, "i8042ControllerHealthy must be true");

        HID_SUBSYSTEM_TELEMETRY directTelem = HidQueryTelemetry();
        TEST_ASSERT(directTelem.totalPingPongUrbsDispatched == telem.totalPingPongUrbsDispatched, "Direct telemetry query must match");
    }

    std::cout << "[TEST] Suite 257: Horizon 41 - HID Class Driver, Report Descriptor Parser & Kernel Input Stack PASSED.\n";
}

// ============================================================================
// Suite 258: Horizon 42 - Sovereign NT Executive Cache Manager (Cc*), 256KB VACBs, BCB Pinning & Lazy Writer
// ============================================================================

namespace Horizon42Callbacks {
    inline uint32_t g_lazyWriteAcquires  = 0;
    inline uint32_t g_lazyWriteReleases  = 0;
    inline uint32_t g_readAheadAcquires  = 0;
    inline uint32_t g_readAheadReleases  = 0;
    inline int64_t  g_lastFlushedWalLsn  = 0;
    inline uint32_t g_deferredPostCalls  = 0;

    inline void reset() {
        g_lazyWriteAcquires = 0;
        g_lazyWriteReleases = 0;
        g_readAheadAcquires = 0;
        g_readAheadReleases = 0;
        g_lastFlushedWalLsn = 0;
        g_deferredPostCalls = 0;
    }

    inline uint8_t AcquireForLazyWrite(void* /*Context*/, uint8_t /*Wait*/) {
        g_lazyWriteAcquires++;
        return 1;
    }
    inline void ReleaseFromLazyWrite(void* /*Context*/) {
        g_lazyWriteReleases++;
    }
    inline uint8_t AcquireForReadAhead(void* /*Context*/, uint8_t /*Wait*/) {
        g_readAheadAcquires++;
        return 1;
    }
    inline void ReleaseFromReadAhead(void* /*Context*/) {
        g_readAheadReleases++;
    }
    inline void FlushWalToLsn(void* /*LogHandle*/, micant::LargeInteger lsn) {
        if (lsn.QuadPart > g_lastFlushedWalLsn) {
            g_lastFlushedWalLsn = lsn.QuadPart;
        }
    }
    inline void DeferredWritePostCallback(void* ctx1, void* /*ctx2*/) {
        g_deferredPostCalls++;
        if (ctx1) {
            *static_cast<bool*>(ctx1) = true;
        }
    }
} // namespace Horizon42Callbacks

void Test_Horizon42_ExecutiveCacheManager_VACB_LazyWriter_And_Pinning() {
    using namespace micant::km::cc;
    std::cout << "[TEST] Running Suite 258: Horizon 42 - Sovereign NT Executive Cache Manager (Cc*), 256KB VACBs & Lazy Writer...\n" << std::flush;

    Horizon42Callbacks::reset();

    // ------------------------------------------------------------------------
    // Phase 1: Subsystem Initialization, JanusLDR Exports & SCM/Version DB
    // ------------------------------------------------------------------------
    {
        InitializeCcCacheSubsystemExports();
        auto& ccMgr = TitanCacheManager::Instance();
        ccMgr.resetForTesting();

        auto& ldr = ldr::DynamicLoader::get();
        TEST_ASSERT(ldr.getExport("ntoskrnl.exe", "CcInitializeCacheMap") != nullptr, "ntoskrnl.exe CcInitializeCacheMap must exist");
        TEST_ASSERT(ldr.getExport("ntoskrnl.exe", "CcUninitializeCacheMap") != nullptr, "ntoskrnl.exe CcUninitializeCacheMap must exist");
        TEST_ASSERT(ldr.getExport("ntoskrnl.exe", "CcSetFileSizes") != nullptr, "ntoskrnl.exe CcSetFileSizes must exist");
        TEST_ASSERT(ldr.getExport("ntoskrnl.exe", "CcGetFileSizePointer") != nullptr, "ntoskrnl.exe CcGetFileSizePointer must exist");
        TEST_ASSERT(ldr.getExport("ntoskrnl.exe", "CcCopyRead") != nullptr, "ntoskrnl.exe CcCopyRead must exist");
        TEST_ASSERT(ldr.getExport("ntoskrnl.exe", "CcCopyWrite") != nullptr, "ntoskrnl.exe CcCopyWrite must exist");
        TEST_ASSERT(ldr.getExport("ntoskrnl.exe", "CcFastCopyRead") != nullptr, "ntoskrnl.exe CcFastCopyRead must exist");
        TEST_ASSERT(ldr.getExport("ntoskrnl.exe", "CcFastCopyWrite") != nullptr, "ntoskrnl.exe CcFastCopyWrite must exist");
        TEST_ASSERT(ldr.getExport("ntoskrnl.exe", "CcZeroData") != nullptr, "ntoskrnl.exe CcZeroData must exist");
        TEST_ASSERT(ldr.getExport("ntoskrnl.exe", "CcMdlRead") != nullptr, "ntoskrnl.exe CcMdlRead must exist");
        TEST_ASSERT(ldr.getExport("ntoskrnl.exe", "CcMdlReadComplete") != nullptr, "ntoskrnl.exe CcMdlReadComplete must exist");
        TEST_ASSERT(ldr.getExport("ntoskrnl.exe", "CcPrepareMdlWrite") != nullptr, "ntoskrnl.exe CcPrepareMdlWrite must exist");
        TEST_ASSERT(ldr.getExport("ntoskrnl.exe", "CcMdlWriteComplete") != nullptr, "ntoskrnl.exe CcMdlWriteComplete must exist");
        TEST_ASSERT(ldr.getExport("ntoskrnl.exe", "CcMapData") != nullptr, "ntoskrnl.exe CcMapData must exist");
        TEST_ASSERT(ldr.getExport("ntoskrnl.exe", "CcPinMappedData") != nullptr, "ntoskrnl.exe CcPinMappedData must exist");
        TEST_ASSERT(ldr.getExport("ntoskrnl.exe", "CcPinRead") != nullptr, "ntoskrnl.exe CcPinRead must exist");
        TEST_ASSERT(ldr.getExport("ntoskrnl.exe", "CcPreparePinWrite") != nullptr, "ntoskrnl.exe CcPreparePinWrite must exist");
        TEST_ASSERT(ldr.getExport("ntoskrnl.exe", "CcSetDirtyPinnedData") != nullptr, "ntoskrnl.exe CcSetDirtyPinnedData must exist");
        TEST_ASSERT(ldr.getExport("ntoskrnl.exe", "CcUnpinData") != nullptr, "ntoskrnl.exe CcUnpinData must exist");
        TEST_ASSERT(ldr.getExport("ntoskrnl.exe", "CcFlushCache") != nullptr, "ntoskrnl.exe CcFlushCache must exist");
        TEST_ASSERT(ldr.getExport("ntoskrnl.exe", "CcPurgeCacheSection") != nullptr, "ntoskrnl.exe CcPurgeCacheSection must exist");
        TEST_ASSERT(ldr.getExport("ntoskrnl.exe", "CcCanIWrite") != nullptr, "ntoskrnl.exe CcCanIWrite must exist");
        TEST_ASSERT(ldr.getExport("ntoskrnl.exe", "CcDeferWrite") != nullptr, "ntoskrnl.exe CcDeferWrite must exist");

        auto& scm = scm::ServiceControlManager::get();
        auto ccSvc = scm.getServiceRecord(L"CcCacheMgr");
        TEST_ASSERT(ccSvc != nullptr, "CcCacheMgr service record must exist in SCM");
        TEST_ASSERT(ccSvc->startType == scm::SERVICE_BOOT_START, "CcCacheMgr must be boot start");
        TEST_ASSERT(ccSvc->status.dwCurrentState == scm::SERVICE_RUNNING, "CcCacheMgr must be running");

        auto& verDb = version::VersionDatabase::Instance();
        TEST_ASSERT(verDb.GetModuleInfo("cc_executive") != nullptr, "cc_executive must be in Version Database");
    }

    CC_FILE_OBJECT fileObj{};
    fileObj.FileName = L"\\Device\\HarddiskVolume1\\MicaDatabase.db";

    CACHE_MANAGER_CALLBACKS callbacks{};
    callbacks.AcquireForLazyWrite  = Horizon42Callbacks::AcquireForLazyWrite;
    callbacks.ReleaseFromLazyWrite = Horizon42Callbacks::ReleaseFromLazyWrite;
    callbacks.AcquireForReadAhead  = Horizon42Callbacks::AcquireForReadAhead;
    callbacks.ReleaseFromReadAhead = Horizon42Callbacks::ReleaseFromReadAhead;

    // ------------------------------------------------------------------------
    // Phase 2 & 3: SHARED_CACHE_MAP, PRIVATE_CACHE_MAP & 256 KB VACB Lifecycle
    // ------------------------------------------------------------------------
    {
        TEST_ASSERT(!CcIsFileCached(&fileObj), "FileObject must not be cached before CcInitializeCacheMap");

        CC_FILE_SIZES initialSizes{};
        initialSizes.AllocationSize.QuadPart  = 512 * 1024; // 512 KB (2 VACBs)
        initialSizes.FileSize.QuadPart        = 512 * 1024;
        initialSizes.ValidDataLength.QuadPart = 512 * 1024;

        CcInitializeCacheMap(&fileObj, &initialSizes, TRUE_B, &callbacks, reinterpret_cast<PVOID>(0x1234));
        TEST_ASSERT(CcIsFileCached(&fileObj), "FileObject must be cached after CcInitializeCacheMap");
        TEST_ASSERT(fileObj.PrivateCacheMap != nullptr, "PrivateCacheMap must be allocated on FileObject");

        // Bind Write-Ahead Log flush hook
        CcSetLogHandleForFile(&fileObj, reinterpret_cast<PVOID>(0x9999), Horizon42Callbacks::FlushWalToLsn);

        // Extend file to 1 MB (4 VACBs of 256 KB each) via CcSetFileSizes
        CC_FILE_SIZES extendedSizes{};
        extendedSizes.AllocationSize.QuadPart  = 1024 * 1024;
        extendedSizes.FileSize.QuadPart        = 1024 * 1024;
        extendedSizes.ValidDataLength.QuadPart = 1024 * 1024;
        CcSetFileSizes(&fileObj, &extendedSizes);

        PLARGE_INTEGER fsizePtr = CcGetFileSizePointer(&fileObj);
        TEST_ASSERT(fsizePtr != nullptr && fsizePtr->QuadPart == 1024 * 1024, "CcGetFileSizePointer must reflect 1 MB FileSize");
    }

    // ------------------------------------------------------------------------
    // Phase 4: Fast Copy I/O Across 256 KB VACB Boundaries & CcZeroData
    // ------------------------------------------------------------------------
    {
        // Write a 1024-byte pattern straddling VACB[0] and VACB[1] at offset 261,632 (256KB - 512)
        std::vector<uint8_t> writePattern(1024);
        for (size_t i = 0; i < writePattern.size(); ++i) {
            writePattern[i] = static_cast<uint8_t>((i * 7 + 0x3C) & 0xFF);
        }

        LARGE_INTEGER boundaryOffset{};
        boundaryOffset.QuadPart = VACB_MAPPING_GRANULARITY - 512; // 261,632
        BOOLEAN wrOk = CcCopyWrite(&fileObj, &boundaryOffset, static_cast<ULONG>(writePattern.size()), TRUE_B, writePattern.data());
        TEST_ASSERT(wrOk == TRUE_B, "CcCopyWrite across 256 KB VACB boundary must succeed");

        // Read back the 1024-byte span via CcCopyRead (verifying VACB cache hits)
        std::vector<uint8_t> readBack(1024, 0);
        IoStatusBlock iosb{};
        BOOLEAN rdOk = CcCopyRead(&fileObj, &boundaryOffset, static_cast<ULONG>(readBack.size()), TRUE_B, readBack.data(), &iosb);
        TEST_ASSERT(rdOk == TRUE_B && iosb.Status == NtStatus::Success, "CcCopyRead across 256 KB VACB boundary must succeed");
        TEST_ASSERT(iosb.Information == 1024, "CcCopyRead must report 1024 bytes read");
        TEST_ASSERT(std::memcmp(writePattern.data(), readBack.data(), 1024) == 0, "Cross-VACB read data must match written pattern");

        // Exercise CcFastCopyWrite & CcFastCopyRead
        uint8_t fastPayload[64];
        std::memset(fastPayload, 0xA5, sizeof(fastPayload));
        CcFastCopyWrite(&fileObj, 4096, sizeof(fastPayload), fastPayload);

        uint8_t fastVerify[64]{0};
        CcFastCopyRead(&fileObj, 4096, sizeof(fastVerify), 1, fastVerify, &iosb);
        TEST_ASSERT(fastVerify[0] == 0xA5 && fastVerify[63] == 0xA5, "CcFastCopyRead must retrieve 0xA5 pattern");

        // Exercise CcZeroData on middle 16 bytes [4112 .. 4128)
        LARGE_INTEGER zeroStart{}, zeroEnd{};
        zeroStart.QuadPart = 4112;
        zeroEnd.QuadPart   = 4128;
        TEST_ASSERT(CcZeroData(&fileObj, &zeroStart, &zeroEnd, TRUE_B) == TRUE_B, "CcZeroData must succeed");

        CcFastCopyRead(&fileObj, 4096, sizeof(fastVerify), 1, fastVerify, &iosb);
        TEST_ASSERT(fastVerify[0] == 0xA5 && fastVerify[16] == 0x00 && fastVerify[31] == 0x00 && fastVerify[32] == 0xA5,
                    "CcZeroData must zero exact sub-range [16..31] while preserving surrounding bytes");
    }

    // ------------------------------------------------------------------------
    // Phase 5: Zero-Copy MDL Cache Interface (CcMdlRead / CcPrepareMdlWrite)
    // ------------------------------------------------------------------------
    {
        LARGE_INTEGER mdlOffset{};
        mdlOffset.QuadPart = 65536; // 64 KB offset inside VACB[0]
        PCC_MDL writeMdl   = nullptr;
        IoStatusBlock iosb{};

        CcPrepareMdlWrite(&fileObj, &mdlOffset, 256, &writeMdl, &iosb);
        TEST_ASSERT(writeMdl != nullptr && iosb.Status == NtStatus::Success, "CcPrepareMdlWrite must return valid MDL chain");
        TEST_ASSERT(writeMdl->MappedSystemVa != nullptr && writeMdl->ByteCount == 256, "Write MDL MappedSystemVa and ByteCount must be valid");

        // Directly DMA-write into the VACB's physical/system memory via MappedSystemVa
        const char* dmaSignature = "MicaNT Zero-Copy SMB Direct / FastIo MDL Cache Frame";
        std::memcpy(writeMdl->MappedSystemVa, dmaSignature, std::strlen(dmaSignature) + 1);
        CcMdlWriteComplete(&fileObj, &mdlOffset, writeMdl);

        // Read back zero-copy via CcMdlRead
        PCC_MDL readMdl = nullptr;
        CcMdlRead(&fileObj, &mdlOffset, 256, &readMdl, &iosb);
        TEST_ASSERT(readMdl != nullptr && iosb.Status == NtStatus::Success, "CcMdlRead must return valid MDL chain");
        TEST_ASSERT(std::strcmp(static_cast<const char*>(readMdl->MappedSystemVa), dmaSignature) == 0,
                    "CcMdlRead zero-copy pointer must expose written DMA signature");
        CcMdlReadComplete(&fileObj, readMdl);
    }

    // ------------------------------------------------------------------------
    // Phase 6: Metadata Pinning & BCBs (CcMapData, CcPinRead, CcSetDirtyPinnedData)
    // ------------------------------------------------------------------------
    {
        LARGE_INTEGER mftOffset{};
        mftOffset.QuadPart = 131072; // 128 KB metadata record offset
        PVOID bcb    = nullptr;
        PVOID buffer = nullptr;

        // 1. Map metadata via CcMapData, then upgrade to pinned via CcPinMappedData
        TEST_ASSERT(CcMapData(&fileObj, &mftOffset, 1024, MAP_WAIT, &bcb, &buffer) == TRUE_B, "CcMapData must succeed");
        TEST_ASSERT(bcb != nullptr && buffer != nullptr, "CcMapData must return valid BCB and buffer pointer");

        auto* pubBcb = static_cast<PUBLIC_BCB*>(bcb);
        TEST_ASSERT(pubBcb->NodeTypeCode == CC_NTC_BCB, "Public BCB NodeTypeCode must be CC_NTC_BCB (0x02FE)");
        TEST_ASSERT(pubBcb->MappedLength == 1024, "Public BCB MappedLength must be 1024");

        TEST_ASSERT(CcPinMappedData(&fileObj, &mftOffset, 1024, PIN_WAIT | PIN_EXCLUSIVE, &bcb) == TRUE_B,
                    "CcPinMappedData upgrade must succeed");

        // Modify metadata in place ("FILE0" MFT signature) and stamp WAL LSN
        std::memcpy(buffer, "FILE0_MFT_RECORD", 16);
        LARGE_INTEGER walLsn{};
        walLsn.QuadPart = 0x0000000100004200LL;
        CcSetDirtyPinnedData(bcb, &walLsn);

        // Repin and Unpin twice
        CcRepinBcb(bcb);
        CcUnpinData(bcb);
        CcUnpinData(bcb);

        // 2. Verify CcPreparePinWrite with Zero=TRUE
        LARGE_INTEGER journalOffset{};
        journalOffset.QuadPart = 140000;
        PVOID jBcb = nullptr, jBuf = nullptr;
        TEST_ASSERT(CcPreparePinWrite(&fileObj, &journalOffset, 512, TRUE_B, PIN_WAIT, &jBcb, &jBuf) == TRUE_B,
                    "CcPreparePinWrite must succeed");
        TEST_ASSERT(static_cast<uint8_t*>(jBuf)[0] == 0, "CcPreparePinWrite with Zero=TRUE must zero buffer");
        CcUnpinData(jBcb);
    }

    // ------------------------------------------------------------------------
    // Phase 7: Intelligent Sequential Read-Ahead Engine (CcScheduleReadAhead)
    // ------------------------------------------------------------------------
    {
        CcSetReadAheadGranularity(&fileObj, 128 * 1024); // 128 KB read-ahead

        uint8_t seqBuf[4096]{0};
        IoStatusBlock iosb{};
        LARGE_INTEGER off1{}, off2{};
        off1.QuadPart = 512 * 1024;        // Start of VACB[2]
        off2.QuadPart = 512 * 1024 + 4096; // Immediately sequential read

        CcCopyRead(&fileObj, &off1, sizeof(seqBuf), TRUE_B, seqBuf, &iosb);
        CcCopyRead(&fileObj, &off2, sizeof(seqBuf), TRUE_B, seqBuf, &iosb);

        TEST_ASSERT(Horizon42Callbacks::g_readAheadAcquires >= 1, "Sequential reads must trigger AcquireForReadAhead callback");
        TEST_ASSERT(Horizon42Callbacks::g_readAheadReleases >= 1, "Sequential reads must trigger ReleaseFromReadAhead callback");
    }

    // ------------------------------------------------------------------------
    // Phase 8 & 9: Write Throttling (CcCanIWrite / CcDeferWrite) & Lazy Writer (CcLazyWriteScan)
    // ------------------------------------------------------------------------
    {
        // Set a strict dirty page threshold of 2 pages on fileObj
        CcSetDirtyPageThreshold(&fileObj, 2);

        // Since fileObj already has dirty pages from Phases 4-6, writing another 16 KB (4 pages) must be throttled!
        BOOLEAN canWriteNow = CcCanIWrite(&fileObj, 16384, FALSE_B, FALSE_B);
        TEST_ASSERT(canWriteNow == FALSE_B, "CcCanIWrite must return FALSE when dirty pages exceed DirtyPageThreshold");

        // Queue a deferred write that waits for Lazy Writer to drain dirty pages
        bool deferredCompleted = false;
        CcDeferWrite(&fileObj, Horizon42Callbacks::DeferredWritePostCallback, &deferredCompleted, nullptr, 4096, FALSE_B);
        TEST_ASSERT(deferredCompleted == false, "Deferred write must remain queued while above dirty page threshold");

        // Now run the Lazy Writer background scan (CcLazyWriteScan)!
        // This must:
        //  1) AcquireForLazyWrite -> Flush WAL to LSN (0x0000000100004200) -> Flush dirty VACBs to disk -> ReleaseFromLazyWrite
        //  2) Drain the Deferred Write queue, invoking DeferredWritePostCallback!
        uint32_t flushedPages = CcLazyWriteScan();
        TEST_ASSERT(flushedPages > 0, "CcLazyWriteScan must flush dirty pages to backing disk");
        TEST_ASSERT(Horizon42Callbacks::g_lazyWriteAcquires >= 1, "Lazy Writer must invoke AcquireForLazyWrite");
        TEST_ASSERT(Horizon42Callbacks::g_lazyWriteReleases >= 1, "Lazy Writer must invoke ReleaseFromLazyWrite");
        TEST_ASSERT(Horizon42Callbacks::g_lastFlushedWalLsn == 0x0000000100004200LL,
                    "Write-Ahead Log must be flushed up to newest pinned BCB LSN before dirty data pages");
        TEST_ASSERT(deferredCompleted == true && Horizon42Callbacks::g_deferredPostCalls == 1,
                    "Deferred write post routine must automatically fire once Lazy Writer drains dirty pages");

        // Verify CcCanIWrite now permits writing 4 KB (1 page <= threshold 2)
        TEST_ASSERT(CcCanIWrite(&fileObj, 4096, FALSE_B, FALSE_B) == TRUE_B,
                    "CcCanIWrite must return TRUE after Lazy Writer flushes dirty pages");
    }

    // ------------------------------------------------------------------------
    // Phase 10: Cache Purge, Uninitialize & Subsystem Diagnostic Telemetry
    // ------------------------------------------------------------------------
    {
        // Purge cached views and verify data persists on committed disk backing when re-faulted in!
        TEST_ASSERT(CcPurgeCacheSection(fileObj.SectionObjectPointer, nullptr, 0, FALSE_B) == TRUE_B,
                    "CcPurgeCacheSection must succeed when no BCBs are pinned");

        // Read back the MFT metadata written in Phase 6 at offset 131,072 (this faults in a fresh VACB from disk backing)
        LARGE_INTEGER mftOffset{};
        mftOffset.QuadPart = 131072;
        char mftVerify[17]{0};
        IoStatusBlock iosb{};
        TEST_ASSERT(CcCopyRead(&fileObj, &mftOffset, 16, TRUE_B, mftVerify, &iosb) == TRUE_B,
                    "Reading from re-faulted VACB after purge must succeed");
        TEST_ASSERT(std::memcmp(mftVerify, "FILE0_MFT_RECORD", 16) == 0,
                    "Lazy-written data must persist on backing storage across CcPurgeCacheSection");

        // Uninitialize Cache Map
        CACHE_UNINITIALIZE_EVENT uninitEvt{};
        TEST_ASSERT(CcUninitializeCacheMap(&fileObj, nullptr, &uninitEvt) == TRUE_B, "CcUninitializeCacheMap must succeed");
        TEST_ASSERT(uninitEvt.Signaled == TRUE_B && !CcIsFileCached(&fileObj), "FileObject must be uncached after teardown");

        // Verify Telemetry
        CC_CACHE_TELEMETRY telem{};
        TEST_ASSERT(CcGetCacheTelemetry(&telem) == TRUE_VAL, "CcGetCacheTelemetry must succeed");
        TEST_ASSERT(telem.totalCacheMapsInitialized >= 1, "totalCacheMapsInitialized must be >= 1");
        TEST_ASSERT(telem.totalCacheMapsUninitialized >= 1, "totalCacheMapsUninitialized must be >= 1");
        TEST_ASSERT(telem.totalVacbsAllocated >= 4, "totalVacbsAllocated must be >= 4");
        TEST_ASSERT(telem.totalVacbHits >= 5, "totalVacbHits must be >= 5");
        TEST_ASSERT(telem.totalVacbMisses >= 4, "totalVacbMisses must be >= 4");
        TEST_ASSERT(telem.totalCopyReads >= 4, "totalCopyReads must be >= 4");
        TEST_ASSERT(telem.totalCopyWrites >= 3, "totalCopyWrites must be >= 3");
        TEST_ASSERT(telem.totalMdlReads >= 1 && telem.totalMdlWrites >= 1, "MDL reads and writes must be >= 1");
        TEST_ASSERT(telem.totalBcbsMapped >= 2 && telem.totalBcbsPinned >= 2, "BCB map and pin counts must be >= 2");
        TEST_ASSERT(telem.totalBcbsDirtyLsn >= 1, "totalBcbsDirtyLsn must be >= 1");
        TEST_ASSERT(telem.totalReadAheadsScheduled >= 1, "totalReadAheadsScheduled must be >= 1");
        TEST_ASSERT(telem.totalLazyWriteScans >= 1, "totalLazyWriteScans must be >= 1");
        TEST_ASSERT(telem.totalPagesFlushedToDisk >= 1, "totalPagesFlushedToDisk must be >= 1");
        TEST_ASSERT(telem.totalDeferredWritesExecuted >= 1, "totalDeferredWritesExecuted must be >= 1");
        TEST_ASSERT(telem.currentDirtyPages == 0, "currentDirtyPages must be 0 after uninitialize");

        CC_CACHE_TELEMETRY directTelem = CcQueryCacheTelemetry();
        TEST_ASSERT(directTelem.totalVacbsAllocated == telem.totalVacbsAllocated, "Direct telemetry query must match");
    }

    std::cout << "[TEST] Suite 258: Horizon 42 - Sovereign NT Executive Cache Manager (Cc*), 256KB VACBs & Lazy Writer PASSED.\n";
}

