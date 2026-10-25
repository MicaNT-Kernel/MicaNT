/**
 * @file ntdll.cpp
 * @brief Sovereign NT Layer DLL (ntdll.dll) for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 * Native x64 system call trampolines and runtime library exports.
 */

#include <cstdint>
#include <cstddef>

extern "C" {

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
// 1. Native NT Fast Syscall Trampolines (Ring 3 -> Ring 0)
// ============================================================================

__attribute__((naked)) __declspec(dllexport) uint64_t NtTerminateProcess(void* /*processHandle*/, uint64_t /*exitStatus*/) noexcept {
#if defined(__x86_64__)
    __asm__ __volatile__(
        "movq %rcx, %r10\n\t"
        "movq $0x01, %rax\n\t"
        "movq %rdx, %r8\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
#else
    return 0;
#endif
}

__attribute__((naked)) __declspec(dllexport) uint64_t NtShutdownSystem(uint32_t /*action*/) noexcept {
#if defined(__x86_64__)
    __asm__ __volatile__(
        "movq %rcx, %r10\n\t"
        "movq $0x08, %rax\n\t"
        "movq %rdx, %r8\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
#else
    return 0;
#endif
}

__attribute__((naked)) __declspec(dllexport) uint64_t NtQuerySystemInformation(
    uint32_t /*infoClass*/,
    void* /*infoBuffer*/,
    uint32_t /*length*/,
    uint32_t* /*returnLength*/
) noexcept {
#if defined(__x86_64__)
    __asm__ __volatile__(
        "movq %rcx, %r10\n\t"
        "movq $0x11, %rax\n\t"
        "movq %rdx, %r8\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
#else
    return 0;
#endif
}

__attribute__((naked)) __declspec(dllexport) uint64_t NtAllocateVirtualMemory(
    void* /*processHandle*/,
    void** /*baseAddress*/,
    uint64_t /*zeroBits*/,
    size_t* /*regionSize*/,
    uint32_t /*allocationType*/,
    uint32_t /*protect*/
) noexcept {
#if defined(__x86_64__)
    __asm__ __volatile__(
        "movq %rcx, %r10\n\t"
        "movq $0x18, %rax\n\t"
        "movq %rdx, %r8\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
#else
    return 0;
#endif
}

__attribute__((naked)) __declspec(dllexport) uint64_t NtFreeVirtualMemory(
    void* /*processHandle*/,
    void** /*baseAddress*/,
    size_t* /*regionSize*/,
    uint32_t /*freeType*/
) noexcept {
#if defined(__x86_64__)
    __asm__ __volatile__(
        "movq %rcx, %r10\n\t"
        "movq $0x1B, %rax\n\t"
        "movq %rdx, %r8\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
#else
    return 0;
#endif
}

__attribute__((naked)) __declspec(dllexport) uint64_t NtClose(void* /*handle*/) noexcept {
#if defined(__x86_64__)
    __asm__ __volatile__(
        "movq %rcx, %r10\n\t"
        "movq $0x1E, %rax\n\t"
        "movq %rdx, %r8\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
#else
    return 0;
#endif
}

__attribute__((naked)) __declspec(dllexport) uint64_t NtReadFile(
    void* /*fileHandle*/,
    void* /*event*/,
    void* /*apcRoutine*/,
    void* /*apcContext*/,
    void* /*ioStatusBlock*/,
    void* /*buffer*/,
    uint32_t /*length*/,
    void* /*byteOffset*/,
    uint32_t* /*key*/
) noexcept {
#if defined(__x86_64__)
    __asm__ __volatile__(
        "movq %rcx, %r10\n\t"
        "movq $0x24, %rax\n\t"
        "movq %rdx, %r8\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
#else
    return 0;
#endif
}

__attribute__((naked)) __declspec(dllexport) uint64_t NtWriteFile(
    void* /*fileHandle*/,
    void* /*event*/,
    void* /*apcRoutine*/,
    void* /*apcContext*/,
    void* /*ioStatusBlock*/,
    const void* /*buffer*/,
    uint32_t /*length*/,
    void* /*byteOffset*/,
    uint32_t* /*key*/
) noexcept {
#if defined(__x86_64__)
    __asm__ __volatile__(
        "movq %rcx, %r10\n\t"
        "movq $0x25, %rax\n\t"
        "movq %rdx, %r8\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
#else
    return 0;
#endif
}

__attribute__((naked)) __declspec(dllexport) uint64_t NtIcmpSendEcho(
    uint32_t /*destIp*/,
    const void* /*buffer*/,
    uint32_t /*size*/,
    uint32_t /*timeoutMs*/
) noexcept {
#if defined(__x86_64__)
    __asm__ __volatile__(
        "movq %rcx, %r10\n\t"
        "movq $0x26, %rax\n\t"
        "movq %rdx, %r8\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
#else
    return 0;
#endif
}

__attribute__((naked)) __declspec(dllexport) uint64_t NtYieldExecution() noexcept {
#if defined(__x86_64__)
    __asm__ __volatile__(
        "movq $0x46, %rax\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
#else
    return 0;
#endif
}

__attribute__((naked)) __declspec(dllexport) uint64_t NtGdiGetVideoInfo(void* /*videoInfo*/) noexcept {
#if defined(__x86_64__)
    __asm__ __volatile__(
        "movq %rcx, %r10\n\t"
        "movq $0x80, %rax\n\t"
        "movq %rdx, %r8\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
#else
    return 0;
#endif
}

__attribute__((naked)) __declspec(dllexport) uint64_t NtGdiPresentSurface(const void* /*params*/) noexcept {
#if defined(__x86_64__)
    __asm__ __volatile__(
        "movq %rcx, %r10\n\t"
        "movq $0x81, %rax\n\t"
        "movq %rdx, %r8\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
#else
    return 0;
#endif
}

__declspec(dllexport) uint64_t NtQueryVirtualMemory() noexcept { return 0; }

// ============================================================================
// 2. RTL Runtime Library Functions
// ============================================================================

struct UnicodeString {
    uint16_t length{0};
    uint16_t maximumLength{0};
    wchar_t* buffer{nullptr};
};

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
