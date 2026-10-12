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






