/**
 * @file ntdll.cpp
 * @brief Sovereign NT Layer DLL (ntdll.dll) for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 * Sole Ring 3 -> Ring 0 native syscall gateway for all Win32 subsystem DLLs
 * (kernel32.dll -> ntdll.dll -> KiSystemCall64Entry):
 *   0x01 = NtTerminateProcess
 *   0x08 = NtShutdownSystem
 *   0x0F = NtClose
 *   0x11 = NtQuerySystemInformation
 *   0x18 = NtAllocateVirtualMemory
 *   0x1B = NtFreeVirtualMemory
 *   0x23 = NtQueryVirtualMemory
 *   0x24 = NtReadFile  (live NTFS 3.1 AHCI DMA)
 *   0x25 = NtWriteFile (console / file write)
 *   0x26 = NtIcmpSendEcho
 *   0x46 = NtYieldExecution
 *   0x52 = NtCreateFile (live NTFS 3.1 file open)
 *   0x80 = NtGdiGetVideoInfo
 *   0x81 = NtGdiPresentSurface
 */

#include <cstdint>
#include <cstddef>

extern "C" {

// ============================================================================
// 0. Canonical NT Native Types & Freestanding Memory Primitives
// ============================================================================

struct UnicodeString {
    uint16_t length{0};
    uint16_t maximumLength{0};
    wchar_t* buffer{nullptr};
};

struct ObjectAttributes {
    uint32_t       length{0};
    void*          rootDirectory{nullptr};
    UnicodeString* objectName{nullptr};
    uint32_t       attributes{0};
    void*          securityDescriptor{nullptr};
    void*          securityQualityOfService{nullptr};
};

struct IoStatusBlock {
    uint64_t status{0};
    uint64_t information{0};
};

static constexpr uint64_t NT_STATUS_SUCCESS                 = 0x00000000ULL;
static constexpr uint64_t NT_STATUS_UNSUCCESSFUL            = 0xC0000001ULL;
static constexpr uint64_t NT_STATUS_INVALID_PARAMETER       = 0xC000000DULL;
static constexpr uint64_t NT_STATUS_NO_MEMORY               = 0xC0000017ULL;
static constexpr uint64_t NT_STATUS_OBJECT_NAME_NOT_FOUND   = 0xC0000034ULL;

// Low-level x86_64 fast syscall dispatcher (RAX=SSN, RDX=arg1, R8=arg2, R9=arg3)
static inline uint64_t NtSyscall3(
    uint64_t ssn,
    uint64_t arg1 = 0,
    uint64_t arg2 = 0,
    uint64_t arg3 = 0
) noexcept {
#if defined(__x86_64__)
    register uint64_t r_rax __asm__("rax") = ssn;
    register uint64_t r_rdx __asm__("rdx") = arg1;
    register uint64_t r_r8  __asm__("r8")  = arg2;
    register uint64_t r_r9  __asm__("r9")  = arg3;
    __asm__ __volatile__(
        "syscall"
        : "+r"(r_rax)
        : "r"(r_rdx), "r"(r_r8), "r"(r_r9)
        : "rcx", "r11", "memory"
    );
    return r_rax;
#else
    (void)ssn; (void)arg1; (void)arg2; (void)arg3;
    return 0;
#endif
}

// Freestanding memory primitives exported by ntdll
__declspec(dllexport) void* memset(void* dest, int c, size_t n) noexcept {
    auto d = static_cast<char*>(dest);
    while (n--) *d++ = static_cast<char>(c);
    return dest;
}

__declspec(dllexport) void* memcpy(void* dest, const void* src, size_t n) noexcept {
    auto d = static_cast<char*>(dest);
    auto s = static_cast<const char*>(src);
    while (n--) *d++ = *s++;
    return dest;
}

__declspec(dllexport) void* memmove(void* dest, const void* src, size_t n) noexcept {
    auto d = static_cast<char*>(dest);
    const char* s = static_cast<const char*>(src);
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n;
        auto s_end = s + n;
        while (n--) *--d = *--s_end;
    }
    return dest;
}

bool DllMain(void*, unsigned long, void*) noexcept {
    return true;
}

// ============================================================================
// 1. Native NT Fast Syscall Gateways (Ring 3 -> Ring 0 KiSystemCall64)
// ============================================================================

__declspec(dllexport) uint64_t NtTerminateProcess(
    void* processHandle,
    uint64_t exitStatus
) noexcept {
    return NtSyscall3(0x01, reinterpret_cast<uint64_t>(processHandle), exitStatus, 0);
}

__declspec(dllexport) uint64_t NtShutdownSystem(uint32_t action) noexcept {
    return NtSyscall3(0x08, action, 0, 0);
}

__declspec(dllexport) uint64_t NtClose(void* handle) noexcept {
    return NtSyscall3(0x0F, reinterpret_cast<uint64_t>(handle), 0, 0);
}

__declspec(dllexport) uint64_t NtQuerySystemInformation(
    uint32_t infoClass,
    void* infoBuffer,
    uint32_t length,
    uint32_t* /*returnLength*/
) noexcept {
    return NtSyscall3(0x11, infoClass, reinterpret_cast<uint64_t>(infoBuffer), length);
}

__declspec(dllexport) uint64_t NtAllocateVirtualMemory(
    void* /*processHandle*/,
    void** baseAddress,
    uint64_t /*zeroBits*/,
    size_t* regionSize,
    uint32_t allocationType,
    uint32_t /*protect*/
) noexcept {
    if (!baseAddress || !regionSize) return NT_STATUS_INVALID_PARAMETER;
    uint64_t reqBase = reinterpret_cast<uint64_t>(*baseAddress);
    size_t reqSize = (*regionSize != 0) ? *regionSize : 4096;
    uint64_t allocVa = NtSyscall3(0x18, reqBase, reqSize, allocationType);
    if (allocVa == 0) {
        return NT_STATUS_NO_MEMORY;
    }
    *baseAddress = reinterpret_cast<void*>(allocVa);
    *regionSize = (reqSize + 4095ULL) & ~4095ULL;
    return NT_STATUS_SUCCESS;
}

__declspec(dllexport) uint64_t NtFreeVirtualMemory(
    void* /*processHandle*/,
    void** baseAddress,
    size_t* regionSize,
    uint32_t freeType
) noexcept {
    if (!baseAddress) return NT_STATUS_INVALID_PARAMETER;
    uint64_t base = reinterpret_cast<uint64_t>(*baseAddress);
    size_t sz = regionSize ? *regionSize : 0;
    return NtSyscall3(0x1B, base, sz, freeType);
}

__declspec(dllexport) uint64_t NtQueryVirtualMemory(
    void* /*processHandle*/,
    void* baseAddress,
    uint32_t infoClass,
    void* /*memoryInformation*/,
    size_t /*memoryInformationLength*/,
    size_t* /*returnLength*/
) noexcept {
    return NtSyscall3(0x23, reinterpret_cast<uint64_t>(baseAddress), infoClass, 0);
}

__declspec(dllexport) uint64_t NtCreateFile(
    void** fileHandle,
    uint32_t desiredAccess,
    ObjectAttributes* objectAttributes,
    IoStatusBlock* ioStatusBlock,
    void* /*allocationSize*/,
    uint32_t /*fileAttributes*/,
    uint32_t /*shareAccess*/,
    uint32_t createDisposition,
    uint32_t /*createOptions*/,
    void* /*eaBuffer*/,
    uint32_t /*eaLength*/
) noexcept {
    if (!fileHandle || !objectAttributes || !objectAttributes->objectName || !objectAttributes->objectName->buffer) {
        if (ioStatusBlock) {
            ioStatusBlock->status = NT_STATUS_INVALID_PARAMETER;
            ioStatusBlock->information = 0;
        }
        return NT_STATUS_INVALID_PARAMETER;
    }
    const wchar_t* wpath = objectAttributes->objectName->buffer;
    uint64_t h = NtSyscall3(
        0x52,
        reinterpret_cast<uint64_t>(wpath),
        desiredAccess,
        createDisposition
    );
    if (h == 0 || h == ~0ULL) {
        *fileHandle = nullptr;
        if (ioStatusBlock) {
            ioStatusBlock->status = NT_STATUS_OBJECT_NAME_NOT_FOUND;
            ioStatusBlock->information = 0;
        }
        return NT_STATUS_OBJECT_NAME_NOT_FOUND;
    }
    *fileHandle = reinterpret_cast<void*>(h);
    if (ioStatusBlock) {
        ioStatusBlock->status = NT_STATUS_SUCCESS;
        ioStatusBlock->information = 1; // FILE_OPENED
    }
    return NT_STATUS_SUCCESS;
}

__declspec(dllexport) uint64_t NtReadFile(
    void* fileHandle,
    void* /*event*/,
    void* /*apcRoutine*/,
    void* /*apcContext*/,
    IoStatusBlock* ioStatusBlock,
    void* buffer,
    uint32_t length,
    void* /*byteOffset*/,
    uint32_t* /*key*/
) noexcept {
    if (!buffer) {
        if (ioStatusBlock) {
            ioStatusBlock->status = NT_STATUS_INVALID_PARAMETER;
            ioStatusBlock->information = 0;
        }
        return NT_STATUS_INVALID_PARAMETER;
    }
    int64_t res = static_cast<int64_t>(NtSyscall3(
        0x24,
        reinterpret_cast<uint64_t>(fileHandle),
        reinterpret_cast<uint64_t>(buffer),
        length
    ));
    if (res < 0) {
        if (ioStatusBlock) {
            ioStatusBlock->status = NT_STATUS_UNSUCCESSFUL;
            ioStatusBlock->information = 0;
        }
        return NT_STATUS_UNSUCCESSFUL;
    }
    if (ioStatusBlock) {
        ioStatusBlock->status = NT_STATUS_SUCCESS;
        ioStatusBlock->information = static_cast<uint64_t>(res);
    }
    return NT_STATUS_SUCCESS;
}

__declspec(dllexport) uint64_t NtWriteFile(
    void* fileHandle,
    void* /*event*/,
    void* /*apcRoutine*/,
    void* /*apcContext*/,
    IoStatusBlock* ioStatusBlock,
    const void* buffer,
    uint32_t length,
    void* /*byteOffset*/,
    uint32_t* /*key*/
) noexcept {
    if (!buffer) {
        if (ioStatusBlock) {
            ioStatusBlock->status = NT_STATUS_INVALID_PARAMETER;
            ioStatusBlock->information = 0;
        }
        return NT_STATUS_INVALID_PARAMETER;
    }
    int64_t res = static_cast<int64_t>(NtSyscall3(
        0x25,
        reinterpret_cast<uint64_t>(buffer),
        length,
        reinterpret_cast<uint64_t>(fileHandle)
    ));
    if (res < 0) {
        if (ioStatusBlock) {
            ioStatusBlock->status = NT_STATUS_UNSUCCESSFUL;
            ioStatusBlock->information = 0;
        }
        return NT_STATUS_UNSUCCESSFUL;
    }
    if (ioStatusBlock) {
        ioStatusBlock->status = NT_STATUS_SUCCESS;
        ioStatusBlock->information = static_cast<uint64_t>(res);
    }
    return NT_STATUS_SUCCESS;
}

__declspec(dllexport) uint64_t NtIcmpSendEcho(
    uint32_t destIp,
    const void* buffer,
    uint32_t size,
    uint32_t /*timeoutMs*/
) noexcept {
    return NtSyscall3(0x26, destIp, reinterpret_cast<uint64_t>(buffer), size);
}

__declspec(dllexport) uint64_t NtYieldExecution() noexcept {
    return NtSyscall3(0x46, 0, 0, 0);
}

__declspec(dllexport) uint64_t NtCreateUserProcess(
    void** processHandle,
    void** threadHandle,
    ObjectAttributes* imagePathAttr,
    uint64_t* outPid
) noexcept {
    if (!imagePathAttr || !imagePathAttr->objectName || !imagePathAttr->objectName->buffer) {
        return NT_STATUS_INVALID_PARAMETER;
    }
    const wchar_t* wpath = imagePathAttr->objectName->buffer;
    uint64_t childPid = NtSyscall3(0x58, reinterpret_cast<uint64_t>(wpath), 0, 0);
    if (childPid == 0 || childPid == ~0ULL) {
        if (processHandle) *processHandle = nullptr;
        if (threadHandle)  *threadHandle  = nullptr;
        if (outPid)        *outPid        = 0;
        return NT_STATUS_OBJECT_NAME_NOT_FOUND;
    }
    if (processHandle) *processHandle = reinterpret_cast<void*>(childPid);
    if (threadHandle)  *threadHandle  = reinterpret_cast<void*>(childPid + 100);
    if (outPid)        *outPid        = childPid;
    return NT_STATUS_SUCCESS;
}

__declspec(dllexport) uint64_t NtGdiGetVideoInfo(void* videoInfo) noexcept {
    return NtSyscall3(0x80, reinterpret_cast<uint64_t>(videoInfo), 0, 0);
}

__declspec(dllexport) uint64_t NtGdiPresentSurface(const void* params) noexcept {
    return NtSyscall3(0x81, reinterpret_cast<uint64_t>(params), 0, 0);
}

// ============================================================================
// 2. RTL Runtime Library Functions
// ============================================================================

__declspec(dllexport) void RtlInitUnicodeString(UnicodeString* dest, const wchar_t* source) noexcept {
    if (!dest) return;
    dest->buffer = const_cast<wchar_t*>(source);
    if (!source) {
        dest->length = 0;
        dest->maximumLength = 0;
        return;
    }
    uint16_t len = 0;
    while (source[len] != L'\0') len++;
    dest->length = static_cast<uint16_t>(len * sizeof(wchar_t));
    dest->maximumLength = static_cast<uint16_t>((len + 1) * sizeof(wchar_t));
}

__declspec(dllexport) void RtlZeroMemory(void* dest, size_t length) noexcept {
    memset(dest, 0, length);
}

__declspec(dllexport) void RtlCopyMemory(void* dest, const void* src, size_t length) noexcept {
    memcpy(dest, src, length);
}

__declspec(dllexport) void RtlMoveMemory(void* dest, const void* src, size_t length) noexcept {
    memmove(dest, src, length);
}

__declspec(dllexport) bool RtlEqualUnicodeString(const UnicodeString* s1, const UnicodeString* s2, bool caseInSensitive) noexcept {
    if (!s1 || !s2) return s1 == s2;
    if (s1->length != s2->length) return false;
    size_t chars = s1->length / sizeof(wchar_t);
    for (size_t i = 0; i < chars; ++i) {
        wchar_t c1 = s1->buffer[i];
        wchar_t c2 = s2->buffer[i];
        if (caseInSensitive) {
            if (c1 >= L'A' && c1 <= L'Z') c1 += (L'a' - L'A');
            if (c2 >= L'A' && c2 <= L'Z') c2 += (L'a' - L'A');
        }
        if (c1 != c2) return false;
    }
    return true;
}

// Sovereign Static Heap Pool (64 KB)
static uint8_t s_heapStorage[65536];
static size_t s_heapOffset = 0;

__declspec(dllexport) void* RtlAllocateHeap(void* /*heapHandle*/, uint32_t /*flags*/, size_t size) noexcept {
    size_t aligned = (size + 15) & ~15ULL;
    if (s_heapOffset + aligned <= sizeof(s_heapStorage)) {
        void* ptr = &s_heapStorage[s_heapOffset];
        s_heapOffset += aligned;
        return ptr;
    }
    return nullptr;
}

__declspec(dllexport) bool RtlFreeHeap(void* /*heapHandle*/, uint32_t /*flags*/, void* /*baseAddress*/) noexcept {
    return true;
}

// ============================================================================
// 3. Loader Thunks
// ============================================================================

__declspec(dllexport) void LdrInitializeThunk() noexcept {}
__declspec(dllexport) void LdrLoadDll() noexcept {}
__declspec(dllexport) void LdrGetProcedureAddress() noexcept {}

} // extern "C"
