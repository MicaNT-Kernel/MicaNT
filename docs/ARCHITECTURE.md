# MicaNT Subsystem Architecture Specification

## 1. Process & Thread Management (`ps/`)

The process and thread management subsystem models Dave Cutler's clean executive architecture:

### A. Executive Process (`EProcess`)
- **Address Space Isolation**: Each process encapsulates an independent `mm::ProcessAddressSpace`, managing virtual memory ranges through Virtual Address Descriptor (VAD) trees.
- **Handle Table**: Isolated `ob::HandleTable` providing process-private handle translations (`0x4`, `0x8`, `0xC`...) mapped to referenced `ObjectHeader` pointers.
- **Process Environment Block (PEB)**: Initialized in userland at fixed virtual base `0x00007FFDF0000000ULL`.
- **Termination Lifecycle**: Calling `proc->terminate(status)` sets the process termination flag and cascades state transitions to all active threads.

### B. Executive Thread (`EThread`)
- **Machine Architecture Context**: Fully models the 64-bit AMD64 context frame:
  - `RIP`, `RSP`, `RFLAGS` (`0x202` Interrupt Flag enabled)
  - General purpose registers (`RAX`, `RBX`, `RCX`, `RDX`, `RSI`, `RDI`, `R8`–`R15`)
  - Segment selectors: `CS=0x33` (Ring 3 64-bit code), `SS=0x2B` (Ring 3 data).
- **Thread Environment Block (TEB)**: Userland thread descriptor addressed via `GS:[0x30]`. Contains `NtTib` (`StackBase`, `StackLimit`, `Self`) and `ClientId`.
- **State Machine**: Threads cycle through `Initialized` -> `Ready` -> `Running` -> `Waiting` -> `Terminated`.

---

## 2. Section Objects & Memory Mapping (`section/`)

In Windows NT, executable files and shared memory are projected into virtual address spaces using **Section Objects**:

1. **`SectionObject`**: Encapsulates a contiguous block of backing memory or file data with allocation attributes:
   - `SEC_IMAGE`: Executable binary image mapping.
   - `SEC_COMMIT`: Immediately committed virtual memory.
2. **`SectionManager::mapViewOfSection`**:
   - Maps views into target process address spaces at requested base addresses.
   - Configures memory page protections (`PAGE_EXECUTE_READWRITE`, `PAGE_READWRITE`, `PAGE_READONLY`).
3. **`SectionManager::unmapViewOfSection`**:
   - Reclaims mapped view regions, freeing VAD tracking nodes.

---

## 3. The `KiSystemCall64` Central Dispatcher (`sys/`)

The system call trap interfaces userland `syscall` instructions with kernel executive routines:
- **System Service Numbers (SSNs)**: Windows 10/11 x64 indices:
  - `0x0018`: `NtAllocateVirtualMemory`
  - `0x001E`: `NtFreeVirtualMemory`
  - `0x000F`: `NtClose`
  - `0x002C`: `NtTerminateProcess`
  - `0x0004`: `NtWaitForSingleObject`
  - `0x0036`: `NtQuerySystemInformation`
- **Register Unpacking**:
  - `R10`: Argument 1 (passed from `RCX`)
  - `RDX`: Argument 2
  - `R8`: Argument 3
  - `R9`: Argument 4
  - User stack: Arguments 5+

---

## 4. Test Verification Suite (`test/test_runner.cpp`)

MicaNT includes an automated unit test runner verifying all 5 core subsystems:
1. **Object Manager**: Directory insertion, collision handling, handle table allocation, lookup, closure, and invalid handle rejection.
2. **Memory Manager**: VAD allocation, 4KB page alignment, commit/reserve semantics, non-contiguous ranges, and deallocation.
3. **PE32+ Loader**: Valid header parsing, machine AMD64 validation, section header extraction, and corrupt buffer rejection.
4. **Syscall Dispatcher**: SSN registration, register unpacking, stack argument decoding, and unregistered SSN rejection.
5. **Process & Section Manager**: Process/thread lifecycle, PEB/TEB address mapping, section creation, view mapping, and unmapping.
