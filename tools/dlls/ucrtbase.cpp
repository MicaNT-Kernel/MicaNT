/**
 * @file ucrtbase.cpp
 * @brief Sovereign Universal C Runtime DLL (ucrtbase.dll) for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 * ZERO direct `syscall` instructions: chains strictly through ntdll.dll IAT!
 * Real header-tracked heap allocator (backed by NtAllocateVirtualMemory + NtFreeVirtualMemory),
 * overflow-checked calloc, safe bounds-preserving realloc, string primitives, and NtTerminateProcess.
 */

#include <cstdint>
#include <cstddef>

extern "C" {

__declspec(dllimport) uint64_t NtTerminateProcess(void* processHandle, uint64_t exitStatus) noexcept;
__declspec(dllimport) uint64_t NtAllocateVirtualMemory(
    void* processHandle, void** baseAddress, uint64_t zeroBits, size_t* regionSize,
    uint32_t allocationType, uint32_t protect
) noexcept;
__declspec(dllimport) uint64_t NtFreeVirtualMemory(
    void* processHandle, void** baseAddress, size_t* regionSize, uint32_t freeType
) noexcept;

bool DllMain(void*, unsigned long, void*) noexcept {
    return true;
}

namespace {

static constexpr uint64_t UCRT_ALLOC_MAGIC = 0x554352544D414C4CULL; // "UCRTMALL"
static constexpr uint32_t MEM_COMMIT_RESERVE = 0x3000u;
static constexpr uint32_t MEM_RELEASE_FLAG   = 0x8000u;
static constexpr uint32_t PAGE_RW_PROTECT    = 0x04u;

struct alignas(16) AllocHeader {
    uint64_t magic;
    size_t   requestedSize;
    size_t   regionSize;
    uint32_t fromVm;
    uint32_t inUse;
};

// Fast 64 KB lookaside arena for tiny allocations + dynamic NtAllocateVirtualMemory fallback
alignas(16) static uint8_t s_ucrtHeap[65536];
static size_t s_ucrtOffset = 0;

} // namespace

__declspec(dllexport) void* malloc(size_t size) noexcept {
    if (size == 0) size = 1;
    if (size > (SIZE_MAX - sizeof(AllocHeader) - 4096u)) return nullptr;

    size_t total = (sizeof(AllocHeader) + size + 15u) & ~15ULL;
    if (total <= 4096u && s_ucrtOffset + total <= sizeof(s_ucrtHeap)) {
        auto* hdr = reinterpret_cast<AllocHeader*>(&s_ucrtHeap[s_ucrtOffset]);
        s_ucrtOffset += total;
        hdr->magic = UCRT_ALLOC_MAGIC;
        hdr->requestedSize = size;
        hdr->regionSize = total;
        hdr->fromVm = 0;
        hdr->inUse = 1;
        return hdr + 1;
    }

    void* base = nullptr;
    size_t regionSize = (sizeof(AllocHeader) + size + 4095u) & ~4095ULL;
    uint64_t st = NtAllocateVirtualMemory(
        reinterpret_cast<void*>(-1), &base, 0, &regionSize, MEM_COMMIT_RESERVE, PAGE_RW_PROTECT
    );
    if (st != 0 || !base) return nullptr;
    auto* hdr = reinterpret_cast<AllocHeader*>(base);
    hdr->magic = UCRT_ALLOC_MAGIC;
    hdr->requestedSize = size;
    hdr->regionSize = regionSize;
    hdr->fromVm = 1;
    hdr->inUse = 1;
    return hdr + 1;
}

__declspec(dllexport) void free(void* ptr) noexcept {
    if (!ptr) return;
    auto* hdr = reinterpret_cast<AllocHeader*>(ptr) - 1;
    if (hdr->magic != UCRT_ALLOC_MAGIC || hdr->inUse == 0) return;
    hdr->inUse = 0;
    if (hdr->fromVm) {
        void* base = hdr;
        size_t sz = hdr->regionSize;
        NtFreeVirtualMemory(reinterpret_cast<void*>(-1), &base, &sz, MEM_RELEASE_FLAG);
    }
}

__declspec(dllexport) void* calloc(size_t num, size_t size) noexcept {
    if (num != 0 && size > SIZE_MAX / num) return nullptr; // Prevent integer overflow!
    size_t total = num * size;
    void* p = malloc(total);
    if (p) {
        auto* b = static_cast<unsigned char*>(p);
        for (size_t i = 0; i < total; ++i) b[i] = 0;
    }
    return p;
}

__declspec(dllexport) void* realloc(void* ptr, size_t size) noexcept {
    if (!ptr) return malloc(size);
    if (size == 0) {
        free(ptr);
        return nullptr;
    }
    auto* oldHdr = reinterpret_cast<AllocHeader*>(ptr) - 1;
    if (oldHdr->magic != UCRT_ALLOC_MAGIC || oldHdr->inUse == 0) return nullptr;
    if (size <= oldHdr->requestedSize) {
        oldHdr->requestedSize = size;
        return ptr;
    }
    void* newPtr = malloc(size);
    if (!newPtr) return nullptr;
    size_t copyLen = (oldHdr->requestedSize < size) ? oldHdr->requestedSize : size;
    auto* d = static_cast<unsigned char*>(newPtr);
    const auto* s = static_cast<const unsigned char*>(ptr);
    for (size_t i = 0; i < copyLen; ++i) d[i] = s[i];
    free(ptr);
    return newPtr;
}

__declspec(dllexport) void* memcpy(void* dest, const void* src, size_t n) noexcept {
    auto* d = static_cast<unsigned char*>(dest);
    const auto* s = static_cast<const unsigned char*>(src);
    while (n--) *d++ = *s++;
    return dest;
}

__declspec(dllexport) void* memset(void* dest, int c, size_t n) noexcept {
    auto* d = static_cast<unsigned char*>(dest);
    while (n--) *d++ = static_cast<unsigned char>(c);
    return dest;
}

__declspec(dllexport) int memcmp(const void* s1, const void* s2, size_t n) noexcept {
    const auto* p1 = static_cast<const unsigned char*>(s1);
    const auto* p2 = static_cast<const unsigned char*>(s2);
    while (n--) {
        if (*p1 != *p2) return static_cast<int>(*p1) - static_cast<int>(*p2);
        p1++; p2++;
    }
    return 0;
}

__declspec(dllexport) size_t strlen(const char* s) noexcept {
    if (!s) return 0;
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

__declspec(dllexport) int strcmp(const char* s1, const char* s2) noexcept {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return static_cast<int>(*reinterpret_cast<const unsigned char*>(s1)) -
           static_cast<int>(*reinterpret_cast<const unsigned char*>(s2));
}

__declspec(dllexport) void abort() noexcept {
    NtTerminateProcess(reinterpret_cast<void*>(-1), 3u);
    while (true) {}
}

__declspec(dllexport) void exit(int code) noexcept {
    NtTerminateProcess(reinterpret_cast<void*>(-1), static_cast<uint64_t>(static_cast<uint32_t>(code)));
    while (true) {}
}

} // extern "C"
